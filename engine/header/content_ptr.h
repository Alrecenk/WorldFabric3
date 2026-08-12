#ifndef _CONTENT_PTR_H_
#define _CONTENT_PTR_H_ 1

#include "Registry.h"


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
		int64_t hash = hashBytes(serialize(value)) ;
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

		this->hash = other.hash;
		ContentAddressedStorage::addReference(this->hash);
		this->clean = true;
		this->local = false;
	}

	//Equal semantics
	content_ptr& operator=(const content_ptr& other) {
		if (this == &other){ // set equal to self
			return *this; // don't break anything
		}
		reset(); // we're being overwritten, so clean up anything we have
		*this = content_ptr(other); // Reuse copy constructor logic to build new value for self
		return *this;
	}

	content_ptr& operator=(const T& initial_data) {
		reset(); // we're being overwritten, so clean up anything we have
		*this = content_ptr(initial_data);
		return *this;
	}

	//Move semantics, steals local buffer but other remains a clean nonlocal content_ptr
	content_ptr(content_ptr&& other) noexcept {
		if (!other.clean) {
			// Clean the other object by copying it to storage
			other.hash = ContentAddressedStorage::insert(*other.local_value);
			other.clean = true;
		}

		this->hash = other.hash;
		ContentAddressedStorage::addReference(this->hash);
		this->clean = true;
		this->local = false;

		if(other.local){
			this->local_value = std::move(other.local_value) ;
			this->local = true ;
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
			ContentAddressedStorage::removeReference(this->hash); // not clean means invalid refrence
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
			this->hash = ContentAddressedStorage::insert(local_value);
			this->clean = true ;
		}
		if(local){
			local_value.reset();
			local = false;
		}
	}
	
};

static inline void testContentPtr() {
	std::vector<int> v_data = { 1,2,3,4,5,6 } ;
	content_ptr<std::vector<int>> data = v_data;
	//content_ptr<std::vector<int>> data(std::make_unique<std::vector<int>>(v_data));
	for (int k = 0; k < data->size(); k++) {
		data->at(k) = 2 * k;
	}

	content_ptr<std::vector<int>> data2 = data;

	for (int k = 0; k < data->size(); k++) {
		data->at(k) = 3 * k;
	}


	for (int k = 0; k < data->size(); k++) {
		printf("%d : %d\n", data->at(k), data2->at(k)) ;
	}
}

#endif // #ifndef _CONTENT_PTR_H_