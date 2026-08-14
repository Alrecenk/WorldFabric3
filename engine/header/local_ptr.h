#ifndef _local_ptr_H_
#define _local_ptr_H_ 1

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
		if (!initial_data) {
			return *this;
		}
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
		if (!initial_data) {
			return *this;
		}
		local_value = std::move(initial_data) ;
		clean = false;
		local = true;
		return *this;
	}

	//Move semantics, steals local buffer but other remains a clean nonlocal local_ptr
	local_ptr(local_ptr&& other) noexcept {
		if (!other.clean && !other.local) {
			return;
		}
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
		if (!other.clean && !other.local) {
			return *this;
		}
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
		if(!other.clean && !other.local){
			return ;
		}
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
		if (!other.clean && !other.local) {
			return *this;
		}
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
		if (!other.clean && !other.local) {
			return *this;
		}
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
	
};

//getStructure implementation is used by Registry to allow serialization of this object type
template <typename T>
auto static getStructure(local_ptr<T>& obj) {
	// we never want to serialize the uncommited data or walk the local pointer, we should only be copying clean shallow hashes
	return std::tie(obj.clean, obj.hash); 
}


// Trait to detect any local_ptr regardless of template argument T
template <typename T> struct is_local_ptr : std::false_type {};
template <typename T> struct is_local_ptr<local_ptr<T>> : std::true_type {};
template <typename T> inline constexpr bool is_local_ptr_v = is_local_ptr<T>::value;

// Forward declare tuple helper function because it's mutally recursive with nontuple collectHashesArg
template <typename Tuple, size_t Index = 0>
void collectHashesTupleArg(std::unordered_set<int64_t>& hashes, const Tuple& t);

// Collect all hashes for data that could be reached through an argument
template<typename T>
inline void collectHashesArg(std::unordered_set<int64_t>& hashes, const T& arg) {
	using RawType = std::remove_cvref_t<T>;
	if constexpr (is_local_ptr_v<RawType>) {
		arg.commit(); // push to CAS if local
		if (arg.clean && hashes.find(arg.hash) == hashes.end()) { // not null, and not already walked
			hashes.insert(arg.hash);
			// recurse into the held object
			collectHashesArg(hashes, *ContentAddressedStorage::get<typename RawType::value_type>(arg.hash));
		}
	}
	else if constexpr (is_vector_v<RawType>) {
		if constexpr (!std::is_trivially_copyable_v<typename RawType::value_type>) { // contained objects could have local_ptr
			for (const auto& item : arg) {
				collectHashesArg(hashes, item);  // recursively search each item
			}
		}
	}
	else if constexpr (is_pair_v<RawType>) {
		collectHashesArg(hashes, arg.first);
		collectHashesArg(hashes, arg.second);
	}
	else if constexpr (is_any_set_v<RawType>) {
		if constexpr (!std::is_trivially_copyable_v<typename RawType::value_type>) { // contained objects could have local_ptr
			for (const auto& item : arg) {
				collectHashesArg(hashes, item);  // recursively search each item
			}
		}
	}
	else if constexpr (is_any_map_v<RawType>) {
		if constexpr (!std::is_trivially_copyable_v<typename RawType::key_type>) { // contained keys could have local_ptr
			for (const auto& kv : arg) {
				collectHashesArg(hashes, kv.first);
			}
		}
		if constexpr (!std::is_trivially_copyable_v<typename RawType::mapped_type>) { // contained values could have local_ptr
			for (const auto& kv : arg) {
				collectHashesArg(hashes, kv.second);
			}
		}
	}
	else if constexpr (has_getStructure_v<RawType>) {
		collectHashesTupleArg(hashes, getStructure(const_cast<RawType&>(arg)));
	}
	else if constexpr (is_tuple_like_v<RawType>) {
		collectHashesTupleArg(hashes, arg);
	}
	//Trivially copyable types and everything else falls through and is not searched for hashes
}

// Collect all hashes for data that could be reached through an element of a tuple
template <typename Tuple, size_t Index>
inline void collectHashesTupleArg(std::unordered_set<int64_t>& hashes, const Tuple& t) {
	if constexpr (Index < std::tuple_size_v<Tuple>) {
		collectHashesArg(hashes, std::get<Index>(t));
		collectHashesTupleArg<Tuple, Index + 1>(hashes, t);
	}
}

//Walks a set of arguments and collects a list of all hashes that could be reached through them
template<typename... Args>
inline void collectHashes(std::unordered_set<int64_t>& hashes, const Args&... args) {
	(collectHashesArg(hashes, args), ...); // Fold expression runs collectHashes on every arg in order, return values ignored (result is pushed into set)
}

//Walks a set of arguments and collects a list of all hashes that could be reached through them
template<typename... Args>
inline std::unordered_set<int64_t> collectHashes(const Args&... args) {
	std::unordered_set<int64_t> hashes ;
	(collectHashesArg(hashes, args), ...); // Fold expression runs collectHashes on every arg in order, return values ignored (result is pushed into set)
	return hashes ;
}


#endif // #ifndef _local_ptr_H_