#ifndef _DETERMINISTIC_UNORDERED_MAP_H_
#define _DETERMINISTIC_UNORDERED_MAP_H_ 1

// including the regular unordered_map gets all its dependencies,which are the only dependencies we want
#include <unordered_map> 

template <typename Key, typename Value>
class deterministic_unordered_map {
private:

	//Maps are created with empty tables and will be initialized to this size on first insertion if not explicitly initialized
	static inline int initial_size = 10;

	//Table will be rehashed at the fill rate and increased in size by the multiplier
	static inline float rehash_fill_rate = 0.7f;
	static inline float rehash_size_multiplier = 1.3f;


	static const inline Value default_value = Value();

	struct Slot {
		int element_index = -1; // index in element list, < 0 means empty slot
		bool clean = true ; //whether this slot has ever been written to
	};

	struct Element {
		int slot_index = -1;
		std::pair<Key, Value> key_value;
	};

	std::vector<Slot> table ; // has table of contents
	std::vector<Element> elements ; // indices in table of all filled elements in a deterministic order
	int dirty_slots = 0 ;


	// returns a pair of the first available dirty write slot followed by the first clean slot
	//If the key is in the table, clean_slot = -1, and write_slot contains the found Key's location
	//if write slot < -1 then there were no empty dirty slots before the clean slot was hit
	static inline std::pair<int, int> findSlot(const Key& key,const std::vector<Slot>& table, const std::vector<Element>& elements){
			
		size_t hash = std::hash<Key>{}(key);
		int slot_index = hash % table.size();
		int clean_slot = -1;
		int write_slot = -1;
		while (true) {
			const Slot& slot = table.at(slot_index) ;
			if (slot.clean) {
				//If we hit a clean slot, then the item is not in the list
				clean_slot = slot_index;
				break;
			} else if (slot.element_index < 0 && write_slot < 0) {
				// If we hit an empty slot which is not clean, we could insert here, but item may still be in the list after this
				write_slot = slot_index;
			} else if (slot.element_index >= 0 && elements[slot.element_index].key_value.first == key) {
				//if we hit an identical key
				write_slot = slot_index;
				break;
			}
			slot_index = (slot_index + 1) % table.size();
		}
		return {write_slot, clean_slot} ;
	}

	//Rehashes the table to the new size (assumed to be large enough)
	void rehash(int new_size){
		table = std::vector<Slot>(new_size) ;
		for(int element_index = 0; element_index < elements.size(); element_index++){
			Element& element = elements[element_index] ;
			auto [write_slot, clean_slot] = findSlot(element.key_value.first, table,elements);
			//findSlot during rehash should always return a clean empty slot
			Slot& slot = table[clean_slot];
			slot.clean = false;
			slot.element_index = element_index;
			element.slot_index = clean_slot ;
		}
		dirty_slots = elements.size();
	}

public:
	using key_type = typename Key;
	using mapped_type = typename Value;
	struct Iterator {
		deterministic_unordered_map* map;
		int index = 0;

		bool operator!=(const Iterator& other) const {
			return map != other.map || index != other.index; 
		}

		bool operator==(const Iterator& other) const {
			return map == other.map && index == other.index;
		}

		void operator++() { 
			index++; 
		}
		void operator--() {
			index--;
		}

		std::pair<Key,Value>& operator*() const{
			return map->elements[index].key_value ;
		}


		std::pair<Key,Value>* operator->() const{
			return &map->elements[index].key_value;
		}
	};

	struct ConstIterator {
		const deterministic_unordered_map* map;
		int index = 0;

		bool operator!=(const ConstIterator& other) const {
			return map != other.map || index != other.index;
		}

		bool operator==(const ConstIterator& other) const {
			return map == other.map && index == other.index;
		}

		void operator++() {
			index++;
		}
		void operator--() {
			index--;
		}

		const std::pair<Key, Value>& operator*() const {
			return map->elements.at(index).key_value;
		}

		const std::pair<Key, Value>* operator->() const{
			return &map->elements.at(index).key_value;
		}
	};

	//Reserve allocates a size such that inserting the given number of elements will not result in a rehash
	void reserve(int num_elements){
		rehash((int)(num_elements / rehash_fill_rate) + 1)  ;
	}

	//This insert takes a table so it can be used with rehash
	Iterator insert(const Key& key, const Value& value, std::vector<Slot>& table){
		if(table.empty()){
			rehash(initial_size);
		}else if (dirty_slots >= table.size() * rehash_fill_rate) {
			rehash((int)(table.size() * rehash_size_multiplier));
		}

		auto [write_slot, clean_slot] = findSlot(key, table, elements);
		
		if(clean_slot < 0){ //exited without hitting a clean slot then means we found it
			elements[table[write_slot].element_index].key_value.second = value ; // update value
			// No need to adjust element indices as it shouold already be there
		}else if(write_slot < 0){//a clean slot exit without a dirty write slot
			//write to the clean slot
			Slot& slot = table[clean_slot] ;
			slot.clean = false ;
			slot.element_index = elements.size() ;
			elements.push_back({clean_slot,{ key, value }}) ;
			write_slot = clean_slot ; // used for return iterator
			dirty_slots++ ; //clean slot is now dirty until rehash
		}else{//clean slot and dirty write slot are both set
			//write to the earlier dirty write slot
			Slot& slot = table[write_slot];
			slot.element_index = elements.size();
			elements.push_back({write_slot,{ key, value }});
		}

		return {this, write_slot} ;
	}

	//Calls the generic insert with the active table
	Iterator insert(const Key& key, const Value& value) {
		return insert(key,value, table) ;
	}

	//Removes the element with given key fro mthe map if found
	//return whether the element was found
	//Note: unlike std::unordered_map, erasing DOES invalidate iterators here
	bool erase(const Key& key){
		if(table.empty()){
			return false;
		}
		auto [write_slot, clean_slot] = findSlot(key, table, elements);
		if (clean_slot < 0) { //exited without hitting a clean slot this means we found it
			Slot& erase_slot = table[write_slot] ;
			//swap the elements at the end of the iterator list into the erased position
			elements[erase_slot.element_index] = elements[elements.size()-1] ;
			//update the moved element's table slot to point to its new index in element
			table[elements[erase_slot.element_index].slot_index].element_index = erase_slot.element_index;
			elements.pop_back(); // pop the now duplicate index off the list (do this last in case we swapped the last item with itself)
			//Erase the now unreferenced slot
			erase_slot.element_index = -1 ;
			return true ;
		}else{
			return false; // we did not find it
		}
	}

	Iterator begin() { 
		return { this, 0 }; 
	}
	Iterator end() { 
		return { this, (int)elements.size() }; 
	}

	ConstIterator begin() const{
		return { this, 0 };
	}
	ConstIterator end() const {
		return { this, (int)elements.size() };
	}

	Value& operator[](const Key& key){
		if (table.empty()) {
			rehash(initial_size);
		}
		else if (dirty_slots >= table.size() * rehash_fill_rate) {
			rehash((int)(table.size() * rehash_size_multiplier));
		}

		auto [write_slot, clean_slot] = findSlot(key, table, elements);

		if (clean_slot < 0) { //exited without hitting a clean slot then means we found it
			return elements[table[write_slot].element_index].key_value.second;
		} else if (write_slot < 0) {//a clean slot exit without a dirty write slot
			//create a defaulrt initialized elment in the clean clost
			Slot& slot = table[clean_slot];
			slot.clean = false;
			slot.element_index = elements.size();
			elements.push_back({clean_slot,{key, Value() }});
			dirty_slots++; //clean slot is now dirty until rehash
			return elements[slot.element_index].key_value.second;
		} else {//clean slot and dirty write slot are both set
			//write a default initialized element to the earlier dirty write slot
			Slot& slot = table[write_slot];
			slot.element_index = elements.size();
			elements.push_back({write_slot,{ key, Value() } });
			return elements[slot.element_index].key_value.second;
		}
	}

	const Value& at(const Key& key) const{
		auto [write_slot, clean_slot] = findSlot(key, table, elements);

		if (clean_slot < 0) { //exited without hitting a clean slot then means we found it
			return elements[table[write_slot].element_index].key_value.second;
		}else{
			return default_value;
		}

	}

	Iterator find(const Key& key) {
		if (table.empty()) {
			return end();
		}
		auto [write_slot, clean_slot] = findSlot(key, table, elements);
		if (clean_slot < 0) { //exited without hitting a clean slot then means we found it
			return { this, table[write_slot].element_index};
		}
		else {
			return end() ;
		}
	}

	ConstIterator find(const Key& key) const {
		if (table.empty()) {
			return end();
		}
		auto [write_slot, clean_slot] = findSlot(key, table, elements);
		if (clean_slot < 0) { //exited without hitting a clean slot then means we found it
			return { this, table[write_slot].element_index };
		}
		else {
			return end();
		}
	}

	size_t size() const {
		return elements.size();
	}

	size_t capacity() const {
		return table.size();
	}

	template <typename KeyArgs, typename ValueArgs>
	Iterator emplace(KeyArgs&& key_args, ValueArgs&& value_args){
		if (table.empty()) {
			rehash(initial_size);
		}
		else if (dirty_slots >= table.size() * rehash_fill_rate) {
			rehash((int)(table.size() * rehash_size_multiplier));
		}
		Key key(key_args);
		auto [write_slot, clean_slot] = findSlot(key, table, elements);

		if (clean_slot < 0) { //exited without hitting a clean slot means we found it
			elements[table[write_slot].element_index].key_value.second = Value(std::forward<ValueArgs>(value_args)); // update value
			// No need to adjust element indices as it shouold already be there
		}
		else if (write_slot < 0) {//a clean slot exit without a dirty write slot
			//write to the clean slot
			Slot& slot = table[clean_slot];
			slot.clean = false;
			slot.element_index = elements.size();
			elements.push_back({clean_slot,{ key, Value(std::forward<ValueArgs>(value_args)) }});
			write_slot = clean_slot; // used for return iterator
			dirty_slots++; //clean slot is now dirty until rehash
		}
		else {//clean slot and dirty write slot are both set
			//write to the earlier dirty write slot
			Slot& slot = table[write_slot];
			slot.element_index = elements.size();
			elements.push_back({write_slot,{ key, Value(std::forward<ValueArgs>(value_args))}});
		}

		return { this, write_slot };

	}


	void clear(){
		for(auto& slot : table){
			slot.element_index = -1 ;
			slot.clean = true ;
		}
		elements.clear();
	}
			
};

//Short name for deterministic_unordered_map
template<typename Key, typename Value>
using det_map = deterministic_unordered_map<Key, Value> ;


//Test helper for checking two maps iterate identically
template <typename Key, typename Value>
bool matching(det_map<Key, Value>& A, det_map<Key, Value>& B){
	if(A.size() != B.size()){
		printf("size doesn't match!\n");
		return false;
	}
	auto itA = A.begin();
	auto itB = B.begin();
	while(itA!=A.end() && itB!=B.end()){
		auto pairA = *itA;
		auto pairB = *itB ;
		if (pairA.first != pairB.first || pairA.second != pairB.second) {
			printf("iteraton mismatch!\n");
			return false;
		}
		++itA;
		++itB;
	}
	return itA == A.end() && itB == B.end() ;
}

inline void testDeterministicMap() {
	det_map<int, int> A;
	det_map<int, int> B;


	for (int k = 0; k < 1000; k++) {
		A.insert(k, 1000 - k);
	}
	bool correct = true ;
	int k = 0 ;
	for(const auto& [ key,value] : A){
		correct = key == k && value == 1000-k ;
		k++;
	}
	correct &= k == 1000 ;
	if(correct){
		printf("initial add test succeeded!\n");
	}else{
		printf("initial add test failed!\n");
		return ;
	}

	for(int k=0; k < 1000; k+=2){
		A.erase(k);
	}
	correct & A.size() == 500 ;
	for (const auto& [key, value] : A) {
		correct &= key%2 == 1 ;
	}
	if (correct) {
		printf("Delete evens test succeeded!\n");
	} else {
		printf("Delete evens test failed!\n");
		return;
	}

	for (const auto& [key, value] : A) {
		B.insert(key, value) ;
	}
	correct = matching(A,B) ;
	if(correct){
		printf("Matching after copy.\n");
	} else {
		printf("Not matching after copy!\n");
		return;
	}

	for(int k=0; k < 50000; k++){
		int a = (k ^ (k << 2) * (k) - (1546749*k)) & 0xffff ;
		bool add = (a&1) == 0 ;
		a >>= 1 ;
		int b = 0xffff - a ;
		if(add){
			A.insert(a,b) ;
			B.insert(a,b);
		}else{
			A.erase(a);
			B.erase(a);
		}
	}

	//B.insert(5,7) ; // use to check matching does detect a mismatch
	correct = matching(A, B);
	if (correct) {
		printf("Matching after hammer. Size %d Capacity: %d.\n",(int)A.size(), (int)A.capacity());
	}
	else {
		printf("Not matching after hammer!\n");
		return;
	}


	const auto& C = A ;
	correct = true ;
	for (const auto& [key, value] : C) {
		correct = C.at(key) == B[key ] ;
	}
	

	if (correct) {
		printf("Const iterator and bracket check passed.\n");
	}
	else {
		printf("Const iterator and bracket check failed!\n");
		return;
	}
}
#endif // #ifndef _DETERMINISTIC_UNORDERED_MAP_H_