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

	std::unordered_set<local_ptr<int>> hash_set ;
	for (int i = 0; i < NUM_SLOTS; ++i) {
		hash_set.insert(i);
	}
	hashes = collectHashes(hash_set);
	passing &= hashes.size() == NUM_SLOTS;
	printf("Set hashes == %d\n", (int)hashes.size());
	hash_set.clear();


	std::unordered_map<local_ptr<int>, local_ptr<TrackedObj>> map;
	for (int i = 0; i < NUM_SLOTS; ++i) {
		// because of the fancy constructor both objects types should be buildable inline from raw ints
		map[i] = i+5 ; 
	}
	hashes = collectHashes(map);
	passing &= hashes.size() == NUM_SLOTS * 2 - 5;
	printf("Map hashes == %d\n", (int)hashes.size());
	map.clear();


	printf("Attempting to create a cycle...\n") ;
	local_ptr<LocalNode> a = 7 ;
	local_ptr<LocalNode> b = 8 ;
	a.edit()->next = b ;
	b.edit()->prev = a ; 
	a.edit()->next = b ; // these can't cycle as each change causes a clear duplication

	a.edit()->next.edit()->prev.edit()->next.edit()->prev = a; // this alos shouldn't be able to actually create a cycle
	a.reset();
	b.reset();
	printf("Elements after attempted cycle clear:%d\n", (int)ContentAddressedStorage::content.size());
	passing &= ContentAddressedStorage::content.size() == 0 ;

	if (passing) {
		std::cout << "All tests passed!" << std::endl;
	}else {
		std::cout << "Tests failed!" << std::endl;
	}

}


void testDuplication(){

	local_ptr<int> X = 5 ;
	X.commit(); // must commit before serializing local_ptr
	std::vector<char> ptr_serial = serialize(X);
	local_ptr<int> Y = deserializeValue<local_ptr<int>>(ptr_serial);
	std::cout << *(X.edit()) << " = " << *(Y.edit()) << std::endl;
	X.reset();
	Y.reset();

	std::unordered_map<std::string, local_ptr<TrackedObj>> a_structure ;
	a_structure["A"] = 1 ;
	a_structure["B"] = 2;
	a_structure["C"] = 3;

	std::unordered_set<int64_t> hashes = collectHashes(a_structure); // will commit making serialize safe, do it first!
	std::cout << "Content after collect hashes: " << ContentAddressedStorage::content.size() << std::endl;

	std::vector<char> obj_serial = serialize(a_structure) ;
	
	std::vector<char> packet = ContentAddressedStorage::createPacket(hashes) ;

	std::cout << "Obj serial size: " << obj_serial.size() << " Packet size: " << packet.size() << std::endl;

	a_structure.clear();
	std::cout << "Content after clear: " << ContentAddressedStorage::content.size() << std::endl;

	ContentAddressedStorage::addPacket(packet);
	std::unordered_map<std::string, local_ptr<TrackedObj>> replicated_structure = deserializeValue<std::unordered_map<std::string, local_ptr<TrackedObj>>>(obj_serial) ;

	std::cout << "Content after reconstruction: " << ContentAddressedStorage::content.size() << std::endl;
	for(auto& [name, number] : replicated_structure){
		std::cout << name << " : " << number->value << std::endl;
	}
	replicated_structure.clear();
	std::cout << "Content after final clear: " << ContentAddressedStorage::content.size() << std::endl;
}

#endif // #ifndef _local_ptr_test_H_