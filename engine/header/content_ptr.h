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
	content_ptr(std::unique_ptr<T> initial_data)
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


//TODO check AI generated tests

// Helper to print current status
void print_stats(const std::string& test_name) {
	std::cout << "[" << test_name << "] Allocs: " << TrackedObj::allocs
		<< " | Frees: " << TrackedObj::frees
		<< " | CAS Size: " << ContentAddressedStorage::content.size()
		<< std::endl;
}

// ============================================================================
// TEST CASES
// ============================================================================

void test_basic_cow_sharing() {
	TrackedObj::reset_counters();
	std::cout << "Running: Basic CoW & Sharing..." << std::endl;

	// 1. Initial creation (1 alloc)
	content_ptr<TrackedObj> p1(10);

	// 2. Modify p1 -> transitions to dirty local (Still 1 alloc, because it started local)
	p1->value = 20;

	// 3. Copy to p2 -> triggers hash and CAS insert (1 more alloc for the TypedContentValue wrapper)
	content_ptr<TrackedObj> p2 = p1;

	// Verification: p2 should see 20, p1 should still be local
	assert(p2->value == 20);

	// 4. Modify p1 again -> should be ZERO new allocations because it's already Local
	int allocs_before = TrackedObj::allocs;
	p1->value = 30;
	assert(TrackedObj::allocs == allocs_before && "Editing a local object must not allocate!");

	// 5. Modify p2 -> should trigger CoW allocation (1 new alloc)
	p2->value = 40;
	assert(p2->value == 40);
	assert(p1->value == 30);

	print_stats("Basic CoW");
}

void test_cas_cleanup() {
	TrackedObj::reset_counters();
	std::cout << "Running: CAS Cleanup..." << std::endl;

	{
		content_ptr<TrackedObj> p1(100);
		p1.commit(); // Push to CAS and drop local (now Pure Hash)

		content_ptr<TrackedObj> p2 = p1; // Both point to same hash
		content_ptr<TrackedObj> p3 = p1; // All three share one hash
	}
	// Everything out of scope. All refs should be gone, CAS should be empty.

	assert(ContentAddressedStorage::content.size() == 0 && "CAS failed to prune unreferenced objects!");
	print_stats("CAS Cleanup");
}

void test_move_semantics_frame_pattern() {
	TrackedObj::reset_counters();
	std::cout << "Running: Move-Steal Frame Pattern..." << std::endl;

	// Simulation of frame-to-frame transfer
	content_ptr<TrackedObj> current_frame(50);
	current_frame->value = 60; // Now Dirty Local

	for (int i = 1; i <= 3; ++i) {
		// Transfer to next frame using move semantics
		// This should hash the object for history, but STEAL the buffer for the new pointer
		content_ptr<TrackedObj> next_frame = std::move(current_frame);

		// Modify in the new frame
		next_frame->value += 10; // Should be zero allocation because it stole the buffer

		int allocs_during_edit = TrackedObj::allocs;
		next_frame->value += 1;
		assert(TrackedObj::allocs == allocs_during_edit && "Move-stealing must prevent re-allocation on next frame edit!");

		current_frame = std::move(next_frame);
	}

	print_stats("Move Steal");
}

void test_complex_interleaving() {
	TrackedObj::reset_counters();
	std::cout << "Running: Complex Interleaving..." << std::endl;

	content_ptr<TrackedObj> a(1);
	content_ptr<TrackedObj> b = a; // Hash created

	a->value = 2; // a is dirty local
	content_ptr<TrackedObj> c = a; // a hashed again, c is clean nonlocal

	b->value = 3; // b was clean nonlocal -> allocates new local (CoW)

	a.commit(); // a becomes pure hash
	c.reset();   // c released

	// Final check: Only 'a' and 'b' exist. 'a' is clean, 'b' is dirty.
	assert(a->value == 2);
	assert(b->value == 3);

	print_stats("Complex Interleaving");
}


static void testContentPtr() {
		test_basic_cow_sharing();
		test_cas_cleanup();
		test_move_semantics_frame_pattern();
		test_complex_interleaving();
		std::cout << "\nALL TESTS PASSED SUCCESSFULLY!" << std::endl;
}

#endif // #ifndef _CONTENT_PTR_H_