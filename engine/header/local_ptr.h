#ifndef _local_ptr_H_
#define _local_ptr_H_ 1

#include "Registry.h" // Used for generic hash and serializer



// Trait to detect any local_ptr regardless of template argument T
template <typename T> struct is_local_ptr : std::false_type {};
template <typename T> struct is_local_ptr<local_ptr<T>> : std::true_type {};
template <typename T> inline constexpr bool is_local_ptr_v = is_local_ptr<T>::value;

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
class local_ptr {
public:
	mutable bool clean = false ; // wehtehr our current data matches the content storage and our hash is valid
	mutable bool local = false ; // whether we have a local copy ready to edit on
 
	mutable int64_t hash = 0 ;
	mutable std::unique_ptr<T> local_value  = nullptr;

	using value_type = T; // This allows us to extract T from an instance
	
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
	local_ptr(){};

	//Make an element from raw data
	local_ptr(const T& initial_data)
		: local_value(std::make_unique<T>(initial_data)), clean(false), local(true) {
	}

	local_ptr& operator=(const T& initial_data) {
		reset();
		local_value = std::make_unique<T>(initial_data);
		clean = false;
		local = true;
		return *this;
	}

	//Make an element from an initializer allocated on the the stack without a copy
	template <typename U,
		typename std::enable_if<!std::is_same<typename std::decay<U>::type, local_ptr>::value, int>::type = 0>// don't use this for local_ptr
	local_ptr(U&& initial_data)
		: local_value(std::make_unique<T>(std::forward<U>(initial_data))), clean(false), local(true) {
	}


	template <typename U ,
	typename std::enable_if<!std::is_same<typename std::decay<U>::type, local_ptr>::value, int>::type = 0> // don't use this for local_ptr
	local_ptr& operator=(U&& initial_data) {
		reset();
		local_value = std::make_unique<T>(std::forward<U>(initial_data)) ;
		clean = false;
		local = true;
		return *this;
	}

	//Make an element by taking a unique_ptr
	local_ptr(std::unique_ptr<T>& initial_data)
		: local_value(std::move(initial_data)), clean(false), local(true) {
	}

	local_ptr& operator=(std::unique_ptr<T>& initial_data) {
		reset();
		local_value = std::move(initial_data) ;
		clean = false;
		local = true;
		return *this;
	}

	//Move semantics, steals local buffer but other remains a clean nonlocal local_ptr
	local_ptr(local_ptr&& other) noexcept {
		if (!other.clean) {
			// Clean the other object by copying it to storage
			other.hash = ContentAddressedStorage::insert(*other.local_value);
			other.clean = true;
		}

		hash = other.hash;
		ContentAddressedStorage::addReference(hash);
		clean = true;
		local = false;

		if (other.local) {
			local_value = std::move(other.local_value);
			local = true;
			other.local = false;
		}
	}

	local_ptr& operator=(local_ptr&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		reset();

		if (!other.clean) {
			// Clean the other object by copying it to storage
			other.hash = ContentAddressedStorage::insert(*other.local_value);
			other.clean = true;
		}

		hash = other.hash;
		ContentAddressedStorage::addReference(hash);
		clean = true;
		local = false;

		if (other.local) {
			local_value = std::move(other.local_value);
			local = true;
			other.local = false;
		}
		return *this;
	}

	// Copy semantics
	local_ptr(const local_ptr& other) {
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

	local_ptr& operator=(const local_ptr& other) {
		if (this == &other) { // set equal to self
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
	// yes this does seem to be required for some cases where the const one fails
	local_ptr& operator=(local_ptr& other) { 
		if (this == &other) { // set equal to self
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

	~local_ptr(){
		reset();
	}

	//Const accessor is read only, no dirty required
	const T* operator->() const {
		if (local){
			return local_value.get();
		}else{
			return ContentAddressedStorage::get<T>(hash);
		}
	}

	//Non-const access requires a local copy
	//Explicitly delete non_const -> so user have to do .edit()-> to explicity get a writeable
	T* edit() {
		if (local) {
			if(clean){
				clean = false ;
				ContentAddressedStorage::removeReference(hash);
			}
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
	void commit() const{
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



	// Forward declare tuple helper function because it's mutally recursive with nontuple collectHashesArg
	template <typename Tuple, size_t Index = 0>
	static void collectHashesTupleArg(std::unordered_set<int64_t>& hashes, const Tuple& t);

	// Collect all hashes for data that could be reached through an argument
	template<typename T>
	static inline void collectHashesArg(std::unordered_set<int64_t>& hashes, const T& arg) {
		using RawType = std::remove_cvref_t<T>;
		if(constexpr (is_local_ptr_v<RawType>){
			arg.commit(); // push to CAS if local
			if(arg.clean && hashes.find(arg.hash) == hashes.end()){ // not null, and not already walked
				hashes.insert(arg.hash);
				// recurse into the held object
				collectHashesArg(hashes, *ContentAddressedStorage::get<RawType::value_type>(hash)) ;
			}
		}else if constexpr (is_vector_v<RawType>) {
			if constexpr (!std::is_trivially_copyable_v<RawType::value_type>) { // contained objects could have local_ptr
				for (const auto& item : arg) {
					collectHashesArg(hashes, item);  // recursively search each item
				}
			}
		}else if constexpr (is_pair_v<RawType>) {
			collectHashesArg(hashes, arg.first);
			collectHashesArg(hashes, arg.second);
		}
		else if constexpr (is_any_set_v<RawType>) {
			if constexpr (!std::is_trivially_copyable_v<RawType::value_type>) { // contained objects could have local_ptr
				for (const auto& item : arg) {
					collectHashesArg(hashes, item);  // recursively search each item
				}
			}
		}else if constexpr (is_any_map_v<RawType>) {
			if constexpr (!std::is_trivially_copyable_v<RawType::key_type>) { // contained keys could have local_ptr
				for (const auto& kv : arg) {
					collectHashesArg(hashes, kv.first);
				}
			}
			if constexpr (!std::is_trivially_copyable_v<RawType::mapped_type>) { // contained values could have local_ptr
				for (const auto& kv : arg) {
					collectHashesArg(hashes, kv.second;
				}
			}
		}else if constexpr (has_getStructure_v<RawType>) {
			collectHashesTupleArg(buffer, getStructure(const_cast<RawType&>(arg)));
		}
		else if constexpr (is_tuple_like_v<RawType>) {
			collectHashesTupleArg(buffer, arg);
		}
		//Trivially copyable types and everything else falls through and is not searched for hashes
	}

	// Collect all hashes for data that could be reached through an element of a tuple
	template <typename Tuple, size_t Index>
	static inline void collectHashesTupleArg(std::unordered_set<int64_t>& hashes, const Tuple& t) {
		if constexpr (Index < std::tuple_size_v<Tuple>) {
			collectHashesArg(hahes, std::get<Index>(t));
			collectHashesTupleArg<Tuple, Index + 1>(hashes, t);
		}
	}

	//Walks a set of arguments and collects a list of all hashes that could be reached through them
	template<typename... Args>
	static inline void collectHashes(std::unordered_set<int64_t>& hashes, const Args&... args) {
		(collectHashesArg(hashes, args), ...); // Fold expression runs collectHashes on every arg in order, return values ignored (result is pushed into set)
	}

	
	
};


// An object which tracks its allocations and deallocations, used only for tests
class TrackedObj {
public:
	static inline int allocs = 0;
	static inline int frees = 0;
	int value = 0;

	TrackedObj() : value(0) { allocs++; }
	TrackedObj(int v) : value(v) { allocs++; }
	TrackedObj(const TrackedObj& other) : value(other.value) { allocs++; }
	~TrackedObj() { frees++; }

	static void resetCounters() {
		allocs = 0;
		frees = 0;
	}
};

//getStructure implementation is used by Registry to allow serialization of this object type
auto static getStructure(TrackedObj& obj) {
	return std::tie(obj.value);
}

void testLocalPtr() {
	TrackedObj::resetCounters();
	std::cout << "Starting Test..." << std::endl;
	bool passing = true ;

	local_ptr<TrackedObj> first = 10 ; // one alloc
	local_ptr<TrackedObj> second = first ; // one alloc because push to CAS
	local_ptr<TrackedObj> third = first; // no allocs
	first.edit()->value = 20 ; // no allocs, local retained

	passing &= first->value == 20 ;
	passing &= second->value == 10;
	passing &= third->value == 10;
	passing &= TrackedObj::allocs == 2 ;
	if(passing){
		std::cout << "Value retained after source edit check passed\n" ;
	}
	first.reset();
	second.reset();
	third.reset();
	passing &= TrackedObj::frees == 2;
	passing &= ContentAddressedStorage::content.size() == 0 ;
	
	if (passing) {
		std::cout << "Clean up after source edit check passed\n";
	}
	TrackedObj::resetCounters() ;
		
	const int NUM_SLOTS = 10;
	std::vector<local_ptr<TrackedObj>> slots;
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
		slots[target].edit()->value = i;
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
	slots[0].edit()->value = 999;
	passing &= TrackedObj::allocs == NUM_SLOTS + 2;
	std::cout << "(Dirty 0) Allocs " << NUM_SLOTS + 2 << " = " << TrackedObj::allocs << std::endl;

	// move the local copy of slot 0 across the list one at time
	for (int i = 1; i < NUM_SLOTS; ++i) {
		slots[i] = std::move(slots[i-1]);
	}
	passing &= TrackedObj::allocs == NUM_SLOTS + 3;
	
	//Edit the local copy at the end of the list, should be free if buffer was properly moved
	slots[NUM_SLOTS-1].edit()->value = 345 ;
	passing &= TrackedObj::allocs == NUM_SLOTS + 3;

	for (int i = 0; i < NUM_SLOTS - 1; ++i) {
		const auto& ptr = slots[i];
		passing &= ptr->value == 999;
		if(ptr->value != 999){
			std::cout << "Incorrect value after sweep!" << std::endl;
		}
		
	}
	const auto& ptr = slots[NUM_SLOTS - 1] ;
	passing &= ptr->value == 345 ;
	if (ptr->value != 345) {
		std::cout << "Incorrect value at end of sweep!" << std::endl;
	}
	passing &= TrackedObj::allocs == NUM_SLOTS + 3;
	std::cout << "(Move Sweep) Allocs " << NUM_SLOTS + 3 << " = " << TrackedObj::allocs << std::endl;

	//Commit everything to content storage and out of local
	for (int i = 1; i < NUM_SLOTS; ++i) {
		slots[i].commit();
	}
	passing &= TrackedObj::allocs == NUM_SLOTS + 4;
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


#endif // #ifndef _local_ptr_H_