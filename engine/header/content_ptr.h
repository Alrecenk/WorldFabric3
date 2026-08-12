#ifndef _CONTENT_PTR_H_
#define _CONTENT_PTR_H_ 1

#include "Registry.h" // Used for generic hash and serializer


struct ContentValue {
	int references = 1;
	virtual ~ContentValue() = default;

};

template <typename T>
struct TypedContentValue : public ContentValue {
	const T data;
	// Copy data when putting it into the content storage
	TypedContentValue(const T& value) : data(value) {}
};

class ContentAddressedStorage {
public:

	static inline std::unordered_map<int64_t,std::unique_ptr<ContentValue>> content ;

	template <typename T>
	static int64_t insert(const T& value){
		int64_t hash = hashBytes(serialize(value)) ; // could swap in your own hash function here if not using Registry
		auto iter = content.find(hash) ;
		if(iter == content.end()){
			auto wrapper = std::make_unique<TypedContentValue<T>>(value);
			content.emplace(hash, std::move(wrapper));
		}else{
			iter->second->references++;
		}
		return hash ;
	}

	template <typename T>
	static const T* get(const int64_t& hash){
		auto iter = content.find(hash);
		if (iter != content.end()) {
			auto* typed = static_cast<TypedContentValue<T>*>(iter->second.get());
			return &(typed->data) ;
		}else{
			return nullptr ;
		}
	}

	static void addReference(const int64_t& hash){
		auto iter = content.find(hash);
		if (iter != content.end()) {
			iter->second->references++;
		}else{
			throw std::runtime_error("Adding a reference to content adressed storage element not found!");
		}
	}

	static void removeReference(const int64_t& hash){
		auto iter = content.find(hash);
		if (iter != content.end()) {
			iter->second->references--;
			if(iter->second->references <=0){
				content.erase(iter->first) ;
			}
		}
	}
};

template <typename T>
class content_ptr {
public:
	mutable bool clean = false ; // wehtehr our current data matches the content storage and our hash is valid
	mutable bool local = false ; // whether we have a local copy ready to edit on
 
	mutable int64_t hash = 0 ;
	mutable std::unique_ptr<T> local_value  = nullptr;
	
	// Reset to nullptr state
	void reset(){
		if(clean){
			ContentAddressedStorage::removeReference(this->hash);
			clean = false;
		}
		if(local){
			local_value.reset();
			local = false;
		}
	}
		
	//empty pointer, needed for use in some data structures
	content_ptr(){};

	//Make an element by taking a unique_ptr
	content_ptr(std::unique_ptr<T>& initial_data)
		: local_value(std::move(initial_data)), clean(false), local(true) {
	}

	//Make an element from raw data
	content_ptr(const T& initial_data)
		: local_value(std::make_unique<T>(initial_data)), clean(false), local(true) {
	}

	// Copy semantics
	content_ptr(const content_ptr& other) {
		if (!other.clean) {
			// Clean the other object by copying it to storage
			other.hash = ContentAddressedStorage::insert(*other.local_value);
			other.clean = true ;
		}

		hash = other.hash;
		ContentAddressedStorage::addReference(hash);
		clean = true;
		local = false;
	}

	//Equal semantics
	content_ptr& operator=(const content_ptr& other) {
		if (this == &other){ // set equal to self
			return *this; // don't break anything
		}
		reset(); // we're being overwritten, so clean up anything we have
		if (!other.clean) {
			// Clean the other object by copying it to storage
			other.hash = ContentAddressedStorage::insert(*other.local_value);
			other.clean = true;
		}

		hash = other.hash;
		ContentAddressedStorage::addReference(hash);
		clean = true;
		local = false;
		return *this;
	}

	content_ptr& operator=(const T& initial_data) {
		reset(); // we're being overwritten, so clean up anything we have
		local_value = std::make_unique<T>(initial_data) ;
		clean = false;
		local = true ;
		return *this;
	}

	//Move semantics, steals local buffer but other remains a clean nonlocal content_ptr
	content_ptr(content_ptr&& other) noexcept {
		if (!other.clean) {
			// Clean the other object by copying it to storage
			other.hash = ContentAddressedStorage::insert(*other.local_value);
			other.clean = true;
		}

		hash = other.hash;
		ContentAddressedStorage::addReference(hash);
		clean = true;
		local = false;

		if(other.local){
			local_value = std::move(other.local_value) ;
			local = true ;
			other.local = false ;
		}
	}

	~content_ptr(){
		reset();
	}

	//Constaccessor is read only, no dirty required
	const T* operator->() const {
		if (local){
			return local_value.get();
		}else{
			return ContentAddressedStorage::get<T>(hash);
		}
	}

	//Non const access requires a local copy
	T* operator->() {
		if (local) {
			return local_value.get(); // already local, good
		}else if(clean){ // not local but valid CAS data
			local_value = std::make_unique<T> (* ContentAddressedStorage::get<T>(hash)); // copy CAS into local
			local = true ;
			clean = false ; // this non-const -> immediately precedes a modifcation so its no longer clean
			ContentAddressedStorage::removeReference(hash); // not clean means invalid refrence
			return local_value.get();
		}else{ // not local or clean is defined as a nullptr
			return nullptr ;
		}
	}

	//Explicity push local data to the CAS
	void commit(){
		if(!clean && ! local){
			return ;
		}
		if(!clean){
			hash = ContentAddressedStorage::insert(*local_value);
			clean = true ;
		}
		if(local){
			local_value.reset();
			local = false;
		}
	}
	
};


// An object whichtracks its allocations and deallocations, used only for tests
class TrackedObj {
public:
	static inline int allocs = 0;
	static inline int frees = 0;
	int value = 0;

	TrackedObj() : value(0) { allocs++; }
	TrackedObj(int v) : value(v) { allocs++; }
	TrackedObj(const TrackedObj& other) : value(other.value) { allocs++; }
	~TrackedObj() { frees++; }

	static void reset_counters() {
		allocs = 0;
		frees = 0;
		ContentAddressedStorage::content.clear();
	}
};

//getStructure implementation is used by Registry to allow serialization of this object type
auto static getStructure(TrackedObj& obj) {
	return std::tie(obj.value);
}

void testContentPtr() {
	TrackedObj::reset_counters();
	std::cout << "Starting Test..." << std::endl;
	bool passing = true ;
	const int NUM_SLOTS = 100;
	std::vector<content_ptr<TrackedObj>> slots;
	slots.reserve(NUM_SLOTS);
	std::cout << "(Init Empty) Allocs =  0 " << TrackedObj::allocs << std::endl;
	passing &= TrackedObj::allocs == 0 ;
	// Create 10 unique objects. 
	for (int i = 0; i < NUM_SLOTS; ++i) {
		slots.emplace_back(i);
	}
	passing &= TrackedObj::allocs == NUM_SLOTS;
	std::cout << "(Init Full) Allocs " << NUM_SLOTS << " = " << TrackedObj::allocs << std::endl;

	// Hammer the slots at random
	for (int i = 0; i < 100; ++i) {
		int target = (int)(randomFloat()*NUM_SLOTS) ;
		slots[target]->value = i;
	}
	passing &= TrackedObj::allocs == NUM_SLOTS;
	std::cout << "(Mutation Burst) Allocs still " << TrackedObj::allocs << std::endl;

	// Copy one slot to every slot
	int target = 5 ;
	for (int i = 0; i < NUM_SLOTS; ++i) {
		slots[i] = slots[target]; // even self copy is safe
	}
	passing &= TrackedObj::allocs == NUM_SLOTS + 1;
	std::cout << "(Sharing Wave) Allocs " << NUM_SLOTS + 1 << " = " << TrackedObj::allocs << std::endl;

	// Make slot [0] unique again by editing it.
	slots[0]->value = 999;
	passing &= TrackedObj::allocs == NUM_SLOTS + 2;
	std::cout << "(Dirty 0) Allocs " << NUM_SLOTS + 2 << " = " << TrackedObj::allocs << std::endl;

	// move the local copy of slot 0 across the list one at time
	for (int i = 1; i < NUM_SLOTS; ++i) {
		slots[i] = std::move(slots[i-1]);
	}
	passing &= TrackedObj::allocs == NUM_SLOTS + 3;
	
	//Edit the local copy at the end of the list, should be free if was properly moved
	slots[NUM_SLOTS-1] = 345 ;
	passing &= TrackedObj::allocs == NUM_SLOTS + 4;

	for (int i = 0; i < NUM_SLOTS - 1; ++i) {
		passing &= slots[i]->value == 999;
		if(slots[i]->value != 999){
			printf("Incorrect value after sweep!\n");
		}
	}
	passing &= slots[NUM_SLOTS - 1]->value == 345 ;
	if (slots[NUM_SLOTS - 1]->value != 345) {
		printf("Incorrect value at endof sweep!\n");
	}
	passing &= TrackedObj::allocs == NUM_SLOTS + 3;
	std::cout << "(Move Sweep) Allocs " << NUM_SLOTS + 3 << " = " << TrackedObj::allocs << std::endl;

	//Commit everything to content storage and out of local
	for (int i = 1; i < NUM_SLOTS; ++i) {
		slots[i].commit();
	}
	passing &= TrackedObj::allocs == NUM_SLOTS + 3;
	std::cout << "(Commit) Allocs "<< NUM_SLOTS + 4 << "= " << TrackedObj::allocs << std::endl;
	//Clear all the data
	slots.clear();
	passing &= TrackedObj::allocs == TrackedObj::frees && ContentAddressedStorage::content.size() == 0 ;
	
	std::cout << "Final Tally -> Allocs: " << TrackedObj::allocs << " Frees: " << TrackedObj::frees << std::endl;
	if(passing){
		std::cout << "All tests passed!" << std::endl;
	}else{
		std::cout << "Tests failed!" << std::endl;
	}
}


#endif // #ifndef _CONTENT_PTR_H_