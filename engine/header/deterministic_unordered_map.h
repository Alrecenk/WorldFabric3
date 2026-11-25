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
	static inline float rehash_size_multiplier = 2.0f;

	struct Slot {
		std::pair<Key, Value> key_value ;
		int element_index = -1; // index in element list, < 0 means empty slot
		bool clean = true ; //whether this slot has ever been written to
	};

	std::vector<Slot> table ; // has table of contents
	std::vector<int> elements ; // indices in table of all filled elements in a deterministic order
	int dirty_slots = 0 ;


	// returns a pair of the first available dirty write slot followed by the first clean slot
	//If the key is in the table, clean_slot = -1, and write_slot contains the found Key's location
	//if write slot < -1 then there were no empty dirty slots before the clean slot was hit
	std::pair<int, int> findSlot(const Key& key,const std::vector<Slot>& table){
		size_t hash = std::hash(key);
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
			} else if (slot.key_value.first == key) {
				//if we hit an identical key
				write_slot = slot_index;
				break;
			}
			slot_index = (slot_index + 1) % table->size();
		}
		return {write_slot, clean_slot} ;
	}

	//Rehashes the table to the new size (assumed to be large enough)
	void rehash(int new_size){
		std::vector<Slot> old_table = std::move(table) ;
		table = std::vector<Slot>(new_size) ;
		for(auto& old_index : elements){
			Slot& old_slot = old_table[old_index] ;
			auto [write_slot, clean_slot] = findSlot(old_slot.key_value.first, table);
			//findSlot during rehash should always return a clean empty slot
			Slot& slot = table[clean_slot];
			slot.key_value = old_slot.key_value ;
			slot.dirty = true;
			slot.element_index = old_index;
			elements[old_index] = clean_slot ;
		}
		dirty_slots = elements.size();
	}


public:
	
	struct Iterator {
		deterministic_unordered_map* map;
		int index = 0;

		bool operator!=(const Iterator& other) const {
			return map != other.map || index != other.index; 
		}
		void operator++() { 
			index++; 
		}
		void operator--() {
			index--;
		}

		std::pair<Key,Value>& operator*() {
			Slot& slot = map->table[elements[index]] ;
			return slot.key_value ;
		}

		const std::pair<Key, Value>& operator*() {
			Slot& slot = map->table[elements[index]];
			return slot.key_value;
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
		}
		auto [write_slot, clean_slot] = findSlot(key, table);
		
		if(clean_slot < 0){ //exited without hitting a clean slot then means we found it
			table[write_slot].key_value.value = value ; // update value
			// No need to adjust element indices as it shouold already be there
		}else if(write_slot < 0){//a clean slot exit without a dirty write slot
			//write to the clean slot
			Slot& slot = table[clean_slot] ;
			slot.key_value = {key, value} ;
			slot.dirty = true ;
			slot.element_index = elements.size() ;
			elements.push_back(clean_slot) ;
			write_slot = clean_slot ; // used for return iterator
			dirty_slots++ ; //clean slot is now dirty until rehash
		}else{//clean slot and dirty write slot are both set
			//write to the earlier dirty write slot
			Slot& slot = table[write_slot];
			slot.key_value = { key, value };
			slot.element_index = elements.size();
			elements.push_back(write_slot);
		}

		return {this, write_slot} ;
	}

	//Calls the generic insert with the active table
	Iterator insert(const Key& key, const Value& value) {
		return insert(key,value, table) ;
	}

	//Removes the element with given key fro mthe map if found
	//return whether the lement was found
	//Note: unlike std::unordered_map, erasing DOES invalidate iterators
	bool erase(const Key& key, const Value& value, std::vector<Slot>& table){
		auto [write_slot, clean_slot] = findSlot(key, table);
		if (clean_slot < 0) { //exited without hitting a clean slot this means we found it
			Slot& erase_slot = table[write_slot] ;
			//swap the elements at the end of the iterator list into the erased position
			elements[erase_slot.element_index] = elements[elements.size()-1] ;
			elements.pop_back(); // pop the now duplicate index off the list
			//update the moved element's table slot to point ot its new index  in element
			table[elements[erase_slot.element_index]].element_index = erase_slot.element_index ;
			
			//Erase the now unreferenced slot
			erase_slot.element_index = -1 ;
			erase_slot.key_value = {} ; //default allocate to make sure data is released
			return true ;
		}else{
			return false; // we did not find it
		}

	}


};
#endif // #ifndef _DETERMINISTIC_UNORDERED_MAP_H_