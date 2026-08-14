#ifndef _local_ptr_test_H_
#define _local_ptr_test_H_ 1

#include "local_ptr.h"



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

class LocalNode{
public:
	int value  = 0 ;
	local_ptr<LocalNode> prev ;
	local_ptr<LocalNode> next ;

	LocalNode(int v = 0 ) : value(v) {} ;
};

auto static getStructure(LocalNode& obj) {
	return std::tie(obj.value, obj.prev, obj.next);
}

void testLocalPtr() {
	TrackedObj::resetCounters();
	std::cout << "Starting local_ptr Test..." << std::endl;
	bool passing = true;

	local_ptr<TrackedObj> first = 10; // one alloc
	local_ptr<TrackedObj> second = first; // one alloc because push to CAS
	local_ptr<TrackedObj> third = first; // no allocs
	first.edit()->value = 20; // no allocs, local retained

	passing &= first->value == 20;
	passing &= second->value == 10;
	passing &= third->value == 10;
	passing &= TrackedObj::allocs == 2;
	if (passing) {
		std::cout << "Value retained after source edit check passed\n";
	}
	first.reset();
	second.reset();
	third.reset();
	passing &= TrackedObj::frees == 2;
	passing &= ContentAddressedStorage::content.size() == 0;

	if (passing) {
		std::cout << "Clean up after source edit check passed\n";
	}
	TrackedObj::resetCounters();

	const int NUM_SLOTS = 10;
	std::vector<local_ptr<TrackedObj>> slots;
	slots.reserve(NUM_SLOTS);
	std::cout << "(Init Empty) Allocs =  0 " << TrackedObj::allocs << std::endl;
	passing &= TrackedObj::allocs == 0;
	// Create 10 unique objects. 
	for (int i = 0; i < NUM_SLOTS; ++i) {
		slots.emplace_back(i);
	}
	passing &= TrackedObj::allocs == NUM_SLOTS;
	std::cout << "(Init Full) Allocs " << NUM_SLOTS << " = " << TrackedObj::allocs << std::endl;

	// Hammer the slots at random
	for (int i = 0; i < 100; ++i) {
		int target = (int)(randomFloat() * NUM_SLOTS);
		slots[target].edit()->value = i;
	}
	passing &= TrackedObj::allocs == NUM_SLOTS;
	std::cout << "(Mutation Burst) Allocs still " << TrackedObj::allocs << std::endl;

	// Copy one slot to every slot
	int target = 5;
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
		slots[i] = std::move(slots[i - 1]);
	}
	passing &= TrackedObj::allocs == NUM_SLOTS + 3;

	//Edit the local copy at the end of the list, should be free if buffer was properly moved
	slots[NUM_SLOTS - 1].edit()->value = 345;
	passing &= TrackedObj::allocs == NUM_SLOTS + 3;

	for (int i = 0; i < NUM_SLOTS - 1; ++i) {
		const auto& ptr = slots[i];
		passing &= ptr->value == 999;
		if (ptr->value != 999) {
			std::cout << "Incorrect value after sweep!" << std::endl;
		}

	}
	const auto& ptr = slots[NUM_SLOTS - 1];
	passing &= ptr->value == 345;
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
	std::cout << "(Commit) Allocs " << NUM_SLOTS + 4 << "= " << TrackedObj::allocs << std::endl;
	//Clear all the data
	slots.clear();
	passing &= TrackedObj::allocs == TrackedObj::frees && ContentAddressedStorage::content.size() == 0;

	std::cout << "Final Tally -> Allocs: " << TrackedObj::allocs << " Frees: " << TrackedObj::frees << std::endl;
	if (passing) {
		std::cout << "All tests passed!" << std::endl;
	}
	else {
		std::cout << "Tests failed!" << std::endl;
	}
}


void testCollectHashes(){
	TrackedObj::resetCounters();
	printf("Starting collect hashes test...\n") ;
	bool passing = true;

	//Check a signle local_ptr
	local_ptr<int> first = 12;
	std::unordered_set<int64_t> hashes = collectHashes(first) ;
	passing &= hashes.size() == 1 ;
	printf("One object hashes == %d\n", (int)hashes.size()) ;
	first.reset();

	const int NUM_SLOTS = 10;

	//Check a vector of local_ptr
	std::vector<local_ptr<TrackedObj>> slots;
	slots.reserve(NUM_SLOTS);
	passing &= TrackedObj::allocs == 0;
	for (int i = 0; i < NUM_SLOTS; ++i) {
		slots.emplace_back(i);
	}
	hashes = collectHashes(slots);
	passing &= hashes.size() == NUM_SLOTS;
	printf("Vector hashes == %d\n", (int)hashes.size());
	slots.clear();

	printf("Starting elements:%d\n",(int) ContentAddressedStorage::content.size()) ;
	local_ptr<LocalNode> a = 7 ;
	local_ptr<LocalNode> b = 8 ;
	a.edit()->next = b ;
	b.edit()->prev = a ; // makesome links to compare change to
	a.edit()->next = b ; // these can't cycle as each change causes a clear duplication
	a.commit();
	b.commit(); // push to cas so we can see hashes
	printf("start A: %lld, B: %lld\n", a.hash, b.hash) ;
	printf("start A.next: %lld, B.prev: %lld  A.next.prev: %lld\n", a->next.hash, b->prev.hash, a->next->prev.hash);
	//a.edit()->next.edit()->prev = a ; //WTF does this do?
	a.edit()->next.edit()->prev.edit()->next.edit()->prev = a; //WTF does this do?
	printf("end A: %lld, B: %lld\n", a.hash, b.hash);
	printf("end A.next: %lld, B.prev: %lld  A.next.prev: %lld\n", a->next.hash, b->prev.hash, a->next->prev.hash);
	hashes = collectHashes(a);
	printf("collect A: %lld, B: %lld\n", a.hash, b.hash);
	printf("collect A.next: %lld, B.prev: %lld  A.next.prev: %lld\n", a->next.hash, b->prev.hash, a->next->prev.hash);
	printf("Ending elements:%d\n", (int)ContentAddressedStorage::content.size());
	printf("hashes s:%d\n", (int)hashes.size());
	a.reset();
	b.reset();
	printf("Elements after cycle clear:%d\n", (int)ContentAddressedStorage::content.size());

} ;

#endif // #ifndef _local_ptr_test_H_