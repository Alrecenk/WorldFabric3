#ifndef _REGISTRY_H_
#define _REGISTRY_H_ 1

#include <iostream>
#include <vector>
#include <tuple>
#include <string>
#include <cstring>
#include <type_traits>
#include <memory>
#include <unordered_map>
#include <functional>
#include <typeindex>
#include <set>
#include <unordered_set>
#include <map>
#include "Variant.h"

#include "glm/glm.hpp"

#define GLM_ENABLE_EXPERIMENTAL // required to use glm::decompose

#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/matrix_decompose.hpp"
#include "glm/gtx/quaternion.hpp" 

//TODO maybe get these functions out of the gobal name space?

//Prints anything that properly handles the << operator with a comma and space after
template<typename T>
inline void printArg(const T& arg) {
    if constexpr (std::is_same_v<T, glm::vec3>) {
        std::cout << "(" << arg.x << "," << arg.y << "," << arg.z << "), ";
    }else if constexpr (std::is_trivially_copyable_v<T>) {
        std::cout << arg << ", ";
    }else if constexpr (std::is_same_v<T, std::vector<typename T::value_type>>) {
        for (auto t : arg) {
            printArg(t);
        }
    }
    else {
        std::cout << arg << ", ";
    }
}

//Prints an arbitrary set of multiple args
template<typename... Args>
inline void printArgs(const Args&... args) {
    (printArg(args), ...);
    std::cout << '\n';
}
// Prints a tuple of arbitrary structure
template<typename... Args>
inline void printTuple(const std::tuple<Args...> tuple) {
    std::apply([&](const auto&... args) {
        (printArg(args), ...);
        }, tuple);
    printf("\n");
}


// Helper for assignTuple that uses an index_sequence to create the proper fold expression for the copy
template<typename... ArgsSrc, typename... ArgsDst, std::size_t... I>
inline void assignTupleImpl(const std::tuple<ArgsDst&...>& dst, const std::tuple<ArgsSrc...>& src, std::index_sequence<I...>) {
    ((std::get<I>(dst) = std::get<I>(src)), ...);
}
// Assigns from tuple `src` into tuple `dst` (which holds references).
template<typename... ArgsSrc, typename... ArgsDst>
inline void assignTuple(const std::tuple<ArgsDst&...>& dst, const std::tuple<ArgsSrc...>& src) {
    static_assert(sizeof...(ArgsSrc) == sizeof...(ArgsDst), "Tuples must be same size");
    assignTupleImpl(dst, src, std::index_sequence_for<ArgsSrc...>{});
}

// Given tuple type full of reference types, these three type templates work to get a tuple type of the matching value types
// Usage: removeTupleRefs<decltype(ref_tuple)>  and then put that in < > to a templated function that needs it
template<typename Tuple>
struct remove_refs_from_tuple;
// Remove references in a tuple
template<typename... Args>
struct remove_refs_from_tuple<std::tuple<Args...>> {
    using type = std::tuple<std::remove_cvref_t<Args>...>;
};
//Creates a more convenient alias of above that is templated on the Tuple instead of its args
template<typename Tuple>
using removeTupleRefs = typename remove_refs_from_tuple<Tuple>::type;

// Runs a class method on a shared_ptr to an object with the given arguments
template <typename T, typename Ret, typename... Args>
inline Ret execute(std::shared_ptr<T> obj, Ret(T::* method)(Args...), Args&&... args) {
    return ((*(obj.get())).*method)(std::forward<Args>(args)...);
}
// Runs a class method on a shared_ptr to an object with the arguments given in the tuple
template <typename T, typename Ret, typename... Args>
inline Ret executeTuple(std::shared_ptr<T> obj, Ret(T::* method)(Args...), const std::tuple<Args...>& args_tuple) {
    return std::apply([&](const auto&... args) {
        return ((*(obj.get())).*method)(std::forward<decltype(args)>(args)...);
        }, args_tuple);
}

// Templated constexpr elements to detect container types
// --- vector detection ---
template<typename T> struct is_vector : std::false_type {};
template<typename T, typename A>
struct is_vector<std::vector<T, A>> : std::true_type {};
template<typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

// --- pair detection ---
template<typename T> struct is_pair : std::false_type {};
template<typename F, typename S>
struct is_pair<std::pair<F, S>> : std::true_type {};
template<typename T>
inline constexpr bool is_pair_v = is_pair<T>::value;

// map detection (std::map or std::unordered_map)
template<typename T> struct is_any_map : std::false_type {};
template<typename K, typename V, typename C, typename A>
struct is_any_map<std::map<K, V, C, A>> : std::true_type {};
template<typename K, typename V, typename H, typename Eq, typename A>
struct is_any_map<std::unordered_map<K, V, H, Eq, A>> : std::true_type {};
template<typename T>
inline constexpr bool is_any_map_v = is_any_map<T>::value;

// set detection (std::set or std::unordered_set)
template<typename T> struct is_any_set : std::false_type {};
template<typename K, typename C, typename A>
struct is_any_set<std::set<K, C, A>> : std::true_type {};
template<typename K, typename H, typename Eq, typename A>
struct is_any_set<std::unordered_set<K, H, Eq, A>> : std::true_type {};
template<typename T>
inline constexpr bool is_any_set_v = is_any_set<T>::value;


// detect if getStructure exists
template<class T, class = void> struct has_getStructure : std::false_type {};
template<class T> // getStructure should always have a non const reference param, but this should work with const or not reference
struct has_getStructure<T, std::void_t<decltype(getStructure(std::declval< std::add_lvalue_reference_t<std::remove_cv_t<T>> >()))>>
	: std::true_type {};
template<typename T> inline constexpr bool has_getStructure_v = has_getStructure<T>::value;

// detect if onDeserialize exists
template<class T, class = void> struct has_onDeserialize : std::false_type {};
template<class T>
struct has_onDeserialize<T, std::void_t<decltype(std::declval<T>().onDeserialize())>>
	: std::true_type {
};
template<typename T> inline constexpr bool has_onDeserialize_v = has_onDeserialize<T>::value;



// detect if can treat like a tuple
template<class, class = void> struct is_tuple_like : std::false_type {};
template<class T> struct is_tuple_like<T, std::void_t<decltype(std::tuple_size<std::remove_cvref_t<T>>::value)>> : std::true_type {};
template<class T> inline constexpr bool is_tuple_like_v = is_tuple_like<T>::value;

// Forward declare tuple helper functions because they're mutally recursive with serializeArg and deserializeArg
template <typename Tuple, size_t Index = 0>
void serializeTupleArg(std::vector<char>& buffer, const Tuple& t);

template <typename Tuple, size_t Index = 0>
void deserializeTupleArg(const char*& data, Tuple& t);



// Appends the given argument to the end of the byte buffer
// Function arguments and class member types must be in this list for them to be serializable
template<typename T>
inline void serializeArg(std::vector<char>& buffer, const T& arg) {
    using RawType = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<RawType, std::string>) {
        //Serialize string
        const std::string& str = static_cast<std::string> (arg);
        size_t len = str.size();
        buffer.insert(buffer.end(), reinterpret_cast<const char*>(&len), reinterpret_cast<const char*>(&len) + sizeof(len));
        buffer.insert(buffer.end(), str.begin(), str.end());
    }
    else if constexpr (std::is_trivially_copyable_v<RawType>) {
        //Serialize plain old data types
        const char* data = reinterpret_cast<const char*>(&arg);
        buffer.insert(buffer.end(), data, data + sizeof(RawType));
    }
    else if constexpr (is_vector_v<RawType>) {
		//Serialize vector
        size_t size = arg.size();
        buffer.insert(buffer.end(), reinterpret_cast<const char*>(&size), reinterpret_cast<const char*>(&size) + sizeof(size));
        for (const auto& item : arg) {
            serializeArg(buffer, item);  // recursively serialize each item
        }
    }else if constexpr (is_pair_v<RawType>) {
		// Serialize pair
		serializeArg(buffer, arg.first);
		serializeArg(buffer, arg.second);
	}else if constexpr (is_any_set_v<RawType>) {
		//Serialize set or unordered_set
		size_t size = arg.size();
		buffer.insert(buffer.end(), reinterpret_cast<const char*>(&size), reinterpret_cast<const char*>(&size) + sizeof(size));
		for (const auto& item : arg) {
			serializeArg(buffer, item);  // recursively serialize each item
		}
	}else if constexpr (is_any_map_v<RawType>) {
		// Serialize map or unordered_map
		size_t size = arg.size();
		buffer.insert(buffer.end(),
			reinterpret_cast<const char*>(&size),
			reinterpret_cast<const char*>(&size) + sizeof(size));
		for (const auto& kv : arg) {
			serializeArg(buffer, kv.first);
			serializeArg(buffer, kv.second);
		}
	}else if constexpr (has_getStructure_v<RawType>){
		serializeTupleArg(buffer, getStructure(const_cast<RawType&>(arg)));
	}else if constexpr (is_tuple_like_v<RawType>) {
		// Serialize tuple (any size)
		serializeTupleArg(buffer, arg);
	}else {
		static_assert(false, "Unsupported type for serialization!");
    }
}

// Deserializes an argument serialized with SerializeArg
// Uses a pointer into the the vector<char> data and increments after each read, 
// so the current arg always starts at the pointer
// Function arguments and class member types must be in this list for them to be deserializable
template<typename T>
inline std::remove_cvref_t<T> deserializeArg(const char*& data) {
    using RawType = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<RawType, std::string>) {
        //Deserialize string
        size_t len;
        std::memcpy(&len, data, sizeof(len));
        data += sizeof(len);
        std::string result(data, len);
        data += len;
        return result;
    }
    else if constexpr (std::is_trivially_copyable_v<RawType>) {
        // Deserialize plain old data types
        RawType value;
        std::memcpy(&value, data, sizeof(RawType));
        data += sizeof(RawType);
        return value;
    }
    else if constexpr (is_vector_v<RawType>) {
		// Deserialize vector
        size_t size;
        std::memcpy(&size, data, sizeof(size));
        data += sizeof(size);
        RawType result;
        result.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            result.push_back(deserializeArg<typename RawType::value_type>(data));  // recursively deserialize
        }
        return result;
    }else if constexpr (is_any_set_v<RawType>) {
		// Deserialize set or unordered_set
		size_t size;
		std::memcpy(&size, data, sizeof(size));
		data += sizeof(size);
		RawType result;
		for (size_t i = 0; i < size; ++i) {
			result.insert(deserializeArg<typename RawType::value_type>(data));  // recursively deserialize
		}
		return result;
	}else if constexpr (is_pair_v<RawType>) {
		//Deserialize pair
		RawType result;
		result.first = deserializeArg<typename RawType::first_type>(data);
		result.second = deserializeArg<typename RawType::second_type>(data);
		return result;
	}else if constexpr (is_any_map_v<RawType>) {
		// Deserialize map or unordered_map
		size_t size;
		std::memcpy(&size, data, sizeof(size));
		data += sizeof(size);
		RawType result;
		for (size_t i = 0; i < size; ++i) {
			auto key = deserializeArg<typename RawType::key_type>(data);
			auto value = deserializeArg<typename RawType::mapped_type>(data);
			result.emplace(std::move(key), std::move(value));
		}
		return result;
	}else if constexpr (has_getStructure_v<RawType>) { // Custom objects typically use this
		RawType result ;
		auto ref_tuple = getStructure(result);
		deserializeTupleArg(data, ref_tuple);
		if constexpr (has_onDeserialize_v<RawType>) {
			result.onDeserialize(); // Call any post-deserialization logic if it exists
		}
		return result ;
	} else if constexpr (is_tuple_like_v<RawType>) {
		//DeserializeTuple
		RawType result{};
		deserializeTupleArg(data, result);
		return result;
	}else {
        static_assert(false, "Unsupported type for deserialization");
    }

}


template <typename Tuple, size_t Index>
inline void serializeTupleArg(std::vector<char>& buffer, const Tuple& t) {
	if constexpr (Index < std::tuple_size_v<Tuple>) {
		serializeArg(buffer, std::get<Index>(t));
		serializeTupleArg<Tuple, Index + 1>(buffer, t);
	}
}

template <typename Tuple, size_t Index>
inline void deserializeTupleArg(const char*& data, Tuple& t) {
	if constexpr (Index < std::tuple_size_v<Tuple>) {
		using ElementType = std::tuple_element_t<Index, Tuple>;
		std::get<Index>(t) = deserializeArg<ElementType>(data);
		deserializeTupleArg<Tuple, Index + 1>(data, t);
	}
}

//serializes an arbitrary list of arguments into a byte array
// Types must be supported in serializeArg
template<typename... Args>
inline std::vector<char> serialize(const Args&... args) {
    //std::vector<char> serial;
    static std::vector<char> preallocated_serial; // TODO this is not thread safe!
    (serializeArg(preallocated_serial, args), ...); // Fold expression runs serialize on every arg in order, return values ignored (data is appended to serial)
    std::vector<char> serial = std::vector<char>(preallocated_serial.begin(), preallocated_serial.end());
    preallocated_serial.clear();
    return serial;
}

//Same as serialize but the args are passed in wrapped in a Tuple
template<typename... Args>
inline std::vector<char> serializeTuple(const std::tuple<Args...>& tuple_args) {
    //std::vector<char> serial;
    static std::vector<char> preallocated_serial; // TODO this is not thread safe!
    std::apply([&](const auto&... args) {
        (serializeArg(preallocated_serial, args), ...); // Fold expression runs serialize on every arg in order, return values ignored (data is appended to serial)
        }, tuple_args);

    std::vector<char> serial = std::vector<char>(preallocated_serial.begin(), preallocated_serial.end());
    preallocated_serial.clear();
    return serial;
}

//Deserializes a byte array made with serialize into a Tuple
// Types must be supported in deserializeArg
template<typename... Args>
inline removeTupleRefs<std::tuple<Args...>> deserialize(const std::vector<char>& serial) {
    const char* data = serial.data(); // get pointer to start of data that will be incremented as we deserialize
    return std::tuple{ deserializeArg< std::remove_cvref_t<std::remove_cvref_t<Args>>>(data)... }; // Note: using an initializer list guarantees order, but make_tuple does not
}

// Same as deseralize, but allows hinting the return structure by passing in a tuple of the desired structure
template<typename... Args>
inline removeTupleRefs<std::tuple<Args...>> deserialize(const std::vector<char>& serial, const std::tuple<Args...>&) {
    return deserialize<Args...>(serial); // normal call
}

// Helper for DeserializeToTuple to unwrap the Tuple and call the general deserialize<Args...>
template<typename Tuple, std::size_t... I>
inline auto deserializeToTuple(const std::vector<char>& serial, std::index_sequence<I...>) {
    return deserialize<std::tuple_element_t<I, Tuple>...>(serial);
}

//deserializeToTuple works like deserialize but the template arguments are wrapped in a Tuple
template<typename Tuple>
inline auto deserializeToTuple(const std::vector<char>& serial) {
    return deserializeToTuple<Tuple>(serial, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

// Object types that want to support direct serialization must have a copy of
// auto getStructure(ObjectType& o) implemented that returns a tuple of references into their data
template<typename T, typename Ret>
Ret getStructure(T&);

// Deserializes the given data (produced with one of the serialize functions) and writes it over the given object 
// Must override getStructure<object_type> for any object type using this method to know where to write
template<typename T>
inline void deserializeInto(T& obj, const std::vector<char>& serial) {
    auto ref_tuple = getStructure(obj);
    assignTuple(
        ref_tuple,
        deserializeToTuple<removeTupleRefs<decltype(ref_tuple)>>(serial)
    );
}

template<typename T>
inline T deserializeValue(const std::vector<char>& serial) {
	const char* ptr = serial.data();
	return deserializeArg<T>(ptr) ;
}

// Runs a class method on a shared_ptr to an object with the arguments given as bytes generated from serialize(args)
template <typename T, typename Ret, typename... Args>
inline Ret executeSerialized(std::shared_ptr<T> obj, Ret(T::* method)(Args...), const std::vector<char>& args_serial) {
    auto args_tuple = deserialize<Args...>(args_serial);
    return std::apply([&](const auto&... args) {
        return ((*(obj.get())).*method)(args...);
        }, args_tuple);
}

// murmur 64 hash function pulled from https://bitsquid.blogspot.com/2011/08/code-snippet-murmur-hash-inverse-pre.html 
uint64_t inline murmurHash64(const unsigned char* key, size_t len, uint64_t seed){
    const uint64_t m = 0xc6a4a7935bd1e995ULL;
    const int r = 47;
    uint64_t h = seed ^ (len * m);
    const uint64_t* data = (const uint64_t*)key;
    const uint64_t* end = data + (len / 8);
    // hash everything in 64 bit chunks
    while (data != end){
        uint64_t k = *(data++);
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }
    //also hash any bytes not in multiple of 8
    int remaining = len & 7;
    const unsigned char* data2 = (const unsigned char*)end;
    uint64_t k = 0;
    for (int j = 0; j < remaining; j++) {
        k ^= (uint64_t)data2[j] << (8Ull * j);
    }
    k *= m;
    k ^= k >> r;
    k *= m;
    h ^= k;
    h *= m;

    return h;
}

// Hash an std::vector of bytes (like from serialized data)
int64_t inline hashBytes(const std::vector<char>& bytes) {
    const unsigned char* data = (const unsigned char*)&(bytes[0]);
    //printf("size: %lld\n", bytes.size());
    return (int64_t)murmurHash64(data, bytes.size(), 0xbebeeffadecabbefULL);
}

// Hash raw bytes of any object (doesn't work on containers or anything with allocated data)
template <typename T>
int64_t hashRaw(const T& obj) {
    const uint8_t* data = reinterpret_cast<const unsigned char*>(&obj);
    return (int64_t)murmurHash64(data, sizeof(T), 0xbebeeffadecabbefULL);
}

// Convert member function pointer to a key for reverse lookups of id from function pointer
template <typename T, typename Ret, typename... Args>
size_t methodPointerToKey(Ret(T::* method)(Args...)) {
    return hashRaw(method);
}

//AbstractVoidMethod type allows differently templated methods to live in the same map in the registry 
class AbstractVoidMethod {
public:
    virtual void execute(std::shared_ptr<void> obj, const std::vector<char>& args_serial) const = 0;
	virtual ~AbstractVoidMethod() = default;
};

// A wrapper for a void method with a function to execute it on a shared_ptr to an appropriately typed object
// Used to hold and label executable functions in the registry
template <typename T, typename Ret, typename... Args>
struct VoidMethod : AbstractVoidMethod {
    Ret(T::* method)(Args...);

    VoidMethod(Ret(T::* m)(Args...)) : method(m) {}
	~VoidMethod(){} ;

    inline void execute(std::shared_ptr<void> obj, const std::vector<char>& args_serial) const override {
        auto args_tuple = deserialize<Args...>(args_serial);
       /*
        if (args_serial.size() > 0) {
            printf("calling execute with:");
            std::cout << typeid(args_tuple).name() << "\n";
            printTuple(args_tuple);
            
        }
        */
        auto typed_obj = std::static_pointer_cast<T>(obj);
        std::apply([&](const auto&... args) {
            ((*(typed_obj.get())).*method)(args...);
            }, args_tuple);
    }
};


inline float interpolate(float a, float b, float t){ 
	return glm::mix(a, b, t); 
}
inline double interpolate(double a, double b, float t){
	return glm::mix(a, b, t); 
}
/*
//For int bool, char, etc
//Should we try to interpolate int?
template<class I, std::enable_if_t<std::is_integral_v<I>, int> = 0>
I interpolate(I a, I b, float t) { 
	return (t < 0.5f) ? a : b; 
}*/

//Interpolate GLM vectors of any size and type
template<int N, class T>
glm::vec<N, T> interpolate(const glm::vec<N, T>& a, const glm::vec<N, T>& b, float t){
	return glm::mix(a, b, t);
}

inline glm::quat interpolate(const glm::quat& a, const glm::quat& b, float t){
	return glm::slerp(a, b, t);
}

inline void polarDecompose(const glm::mat3& M, glm::quat& outRot, glm::mat3& outStretch){
	// get basis
	glm::vec3 x = glm::vec3(M[0]);
	glm::vec3 y = glm::vec3(M[1]);
	glm::vec3 z = glm::vec3(M[2]);
	// Remove the shear from y and z
	// this is not a proper polar decomposition because this rotation is not necessarily the one closest to the original matrix
	//using a consistent renormalization is enough to get smooth interpolation, but an SVD would be required to get the unique closest rotation
	x = glm::normalize(x);
	z = glm::normalize(glm::cross(x,y));
	y = glm::normalize(glm::cross(z,x));
	
	glm::mat3 R;
	R[0] = x;
	R[1] = y;
	R[2] = z;
	outRot = glm::toQuat(R);//convert to quat for slerp
	outStretch = glm::transpose(R) * M;//original matrix with rotation removed
}

inline glm::mat4 interpolate(const glm::mat4& A, const glm::mat4& B, float t){

	glm::vec3 transA = glm::vec3(A[3]);
	glm::vec3 transB = glm::vec3(B[3]);
	glm::vec3 trans = glm::mix(transA, transB, t);

	glm::mat3 linA = glm::mat3(A);
	glm::mat3 linB = glm::mat3(B);

	glm::quat rotA, rotB;
	glm::mat3 stretchA, stretchB;
	polarDecompose(linA, rotA, stretchA);
	polarDecompose(linB, rotB, stretchB);

	glm::quat rot = glm::slerp(rotA, rotB, t);

	//linearly interpolate componentwise for scale andshear
	glm::mat3 stretch ;
	for(int k = 0; k < 3;k++){
		for(int j=0;j<3;j++){
			stretch[k][j] = stretchA[k][j] * (1.0f-t) + stretchB[k][j] * t ;
		}
	}

	glm::mat4 result = glm::translate(glm::mat4(1.0f), trans) *
		glm::mat4(rot) *
		glm::mat4(stretch);

	return result;
}

// For any undefined data type interpolate will do nearest neighbor
template<class T>
T interpolate(const T& a, const T& b, float t){
	return (t < 0.5f) ? a : b;
}

//Interpolates the top level data of an object which has a getStructure definition
//Linear for floats doubles and vectors
//slerp for quats
//decomposes 4x4 matrices and interpolate the parts
template<class T>
T interpolateObj(const T& A, const T& B, float t){
	T out;
	auto a = getStructure(const_cast<T&>(A));
	auto b = getStructure(const_cast<T&>(B));
	auto o = getStructure(out);

	// Walk the three tuples in parallel and assign the blended value.
	std::apply([&](auto&... outs) {
		std::apply([&](const auto&... as) {
			std::apply([&](const auto&... bs) {
				((outs = interpolate(as, bs, t)), ...);
				}, b);
			}, a);
		}, o);

	return out;
}

// Outer base class makes it possible to put templayed subclasses int one list
class BaseObjectView {
public:
	virtual ~BaseObjectView() = default;
	virtual void createdBase(std::shared_ptr<const void> obj) = 0;
	virtual void updatedBase(std::shared_ptr<const void> obj) = 0;
	virtual void destroyedBase() = 0;
};

//This is the class you want to override for your views and template on what is being viewed
template <typename T>
class ObjectView : public BaseObjectView {
	//static_assert(std::is_base_of<WorldObject, T>::value, "View template must inherit from WorldObject.");

public:
	//Created is called when an observation is made of an object that was not present in the previous observation
	//The view will be newly created with an empty constructor whnthis is called
	virtual void created(std::shared_ptr<const T>& observation) = 0;
	void createdBase(std::shared_ptr<const void> obj) override {
		std::shared_ptr<const T> observation = static_pointer_cast<const T>(obj);
		created(observation);
	}

	//Update is called when an observation is made of an object that was also observed last frame on this same view
	virtual void updated(std::shared_ptr<const T>& observation) = 0;
	void updatedBase(std::shared_ptr<const void> obj) override {
		std::shared_ptr<const T> observation = static_pointer_cast<const T>(obj) ;
		updated(observation);
	}

	//Destroyed is called when an observation that was present in the last observation is no longer observed
	//This view will be deleted immediately after this call (it's destructor will be called after this)
	virtual void destroyed() = 0;
	void destroyedBase() override {
		destroyed();
	}

};


// The Registry holds classes and functions for automatic serialization, deserialization, and execution
class Registry {

public:
    std::unordered_map<int, std::unique_ptr<AbstractVoidMethod>> methods;
    std::unordered_map<int, std::function<std::vector<char>(void*)>> serializers;
    std::unordered_map<int, std::function<std::shared_ptr<void>(const std::vector<char>&)>> deserializers;
    std::unordered_map<std::type_index, int64_t> type_to_id;
    std::unordered_map<size_t, int> method_to_id;
	std::unordered_map<int, std::string> class_name ; // text names of classes for debugging
	std::unordered_map<int, std::string> method_name; // text names of methods for debugging
	std::unordered_map<int, std::function<std::shared_ptr<BaseObjectView>()>> view_factories; //used to create associated views from objects


    // Adds a class to the registry
    template<typename T>
    inline int registerClass(const std::string& debug_name) {
        //std::cout << "Registering class: " << typeid(T).name() << "\n";
        //check if the class being registered has a getStructure implementation that returns a nonempty Tuple
        T new_object;
        auto structure = getStructure(new_object);
        static_assert(std::tuple_size<decltype(structure)>() > 0, "A class with no data is being registered for serialization!");

        int id = (int)(type_to_id.size());
        serializers[id] = [](void* objPtr) -> std::vector<char> {
            auto& obj = *static_cast<T*>(objPtr);
            return serializeTuple(getStructure(obj));
            };

        deserializers[id] = [](const std::vector<char>& serial) -> std::shared_ptr<void> {
            T typed_obj;
            if (serial.size() > 0) { // deserializer can be called with no data to return a default object of the given type
                deserializeInto(typed_obj, serial);
            }
            return std::make_shared<T>(typed_obj);
            };

        type_to_id[std::type_index(typeid(T))] = id;
		class_name[id] = debug_name;
        return id;
    }

	//Adds a class to the registry with an ssociated view class
	template<typename T, typename V>
	inline int registerClass(const std::string& debug_name) {
		// Ensure V inherits from the base view class to prevent runtime crashes
		static_assert(std::is_base_of<BaseObjectView, V>::value,
			"The second template argument fo registerClass must inherit from an ObjectView");

		// Perform the regular regitrations
		int id = registerClass<T>(debug_name);

		// Associate vie factory to make the view class
		view_factories[id] = []() -> std::shared_ptr<BaseObjectView> {
			return std::make_shared<V>();
			};

		return id;
	}

    // Adds a void method on a class to the registry
    template <typename T, typename Ret, typename... Args>
    inline int registerMethod(Ret(T::* method)(Args...), const std::string& debug_name) {
        int method_id = (int)(methods.size());
        static_assert(std::is_same_v<Ret, void>, "registerMethod only supports void functions");
        
        methods[method_id] = std::make_unique<VoidMethod<T, Ret, Args...>>(method);
        size_t key = methodPointerToKey(method);
        method_to_id[key] = method_id;
		method_name[method_id] =debug_name ;
        return method_id;
    }

    //Executes a method in the registry on an object of the appropriate type
    // Serialized args are assumed generated with serialize(args...)
    inline void execute(std::shared_ptr<void> obj, int method_id, const std::vector<char>& args_serial) const {
        auto it = methods.find(method_id);
        if (it == methods.end()) throw std::runtime_error("Unknown method ID");
        it->second->execute(obj, args_serial);
    }

    // Returns the id for the given class infered from a type passed by template
    template<typename T>
    inline int getIdForType() const {
        //std::cout << "getid: " << typeid(T).name() << "\n";
        auto it = type_to_id.find(std::type_index(typeid(T)));
        if (it != type_to_id.end()) {
            return (int)(it->second);
        }
        throw std::runtime_error("Type not registered in Registry");
    }

    // Gets the int id for a registered method from raw function pointer
    template <typename T, typename Ret, typename... Args>
    inline int getIdForMethod(Ret(T::* method)(Args...)) {
        size_t key = methodPointerToKey(method);
        auto it = method_to_id.find(key);
        if (it == method_to_id.end()) {
            throw std::runtime_error("Method not registered in Registry");
        }
        else {
            return it->second;
        }

    }

    //For internal use
    //Serialize an object with the type matching type id
    inline std::vector<char> serializeObj(int type_id, void* objPtr) const {
        auto it = serializers.find(type_id);
        if (it != serializers.end()) {
            return it->second(objPtr);
        }
        throw std::runtime_error("Unknown class id during serialization");
    }

    //Serialize an object into type_id and raw data
    template<typename T>
    inline std::pair<int, std::vector<char>> serializeObj(std::shared_ptr<T>& obj) const {
        int id = getIdForType<T>();
        return { id, serializeObj(id, obj.get()) };
    }

    //Serialize an object into type_id and raw data
    template<typename T>
    inline std::pair<int, std::vector<char>> serializeObj(T* obj) const {
        int id = getIdForType<T>();
        return { id, serializeObj(id, obj) };
    }

    // Deserialize an object 
    inline std::shared_ptr<void> deserializeObj(int type_id, const std::vector<char>& serial) const {
        auto it = deserializers.find(type_id);
        if (it != deserializers.end()) {
            return it->second(serial);
        }
        throw std::runtime_error("Unknown class id during deserialization");
    }
    // Deserialize an object, but with the arguments in a pair so you can call it with the output of serializeObj
    inline std::shared_ptr<void> deserializeObj(const std::pair<int, const std::vector<char>>& id_serial) const {
        return deserializeObj(id_serial.first, id_serial.second);
    }

    //Make a deep copy of an object by serializing it and deserializing it
    template<typename T>
    inline std::shared_ptr<void> deepCopy(T* obj) const{
        auto serial = serializeObj(obj);
        return deserializeObj(serial);
    }

    inline std::shared_ptr<void> deepCopy(void* obj, int type_id) const {
        auto serial = serializeObj(type_id, obj);
        return deserializeObj(type_id, serial);
    }


	// Generate a view object from another object whose class was registered with one
	inline std::shared_ptr<BaseObjectView> createView(int type_id) const {
		auto it = view_factories.find(type_id);
		if (it != view_factories.end()) {
			return it->second();
		}
		return nullptr;
	}
};

class MyClass {
public:
    std::string msg = "";
    int value = 0;
    inline void update(std::string s, int v) {
        msg += s;
        value += v;
    }
    inline void print() const {
        std::cout << msg << " (" << value << ")\n";
    }
};

// Returns the structure of the class data as a reference tuple
auto static getStructure(MyClass& obj) {
    return std::tie(obj.msg, obj.value);
}

inline void testFunctionRegistry() {
    // Register references to all classes and functions we want to serialize
    Registry registry;
    int MYCLASS_ID = registry.registerClass<MyClass>("my_class");
    int UPDATE_ID = registry.registerMethod(&MyClass::update, "update");
    std::shared_ptr<MyClass> obj = std::make_shared<MyClass>(MyClass{ "Hello", 10 });
    // serialize arbitrary parameters into raw bytes
    std::vector<char> args_serial = serialize(std::string(" World"), 5);
    // Execute a function by purely serialized info
    registry.execute(obj, UPDATE_ID, args_serial);
    // serialize object into raw bytes
    std::pair<int, const std::vector<char>> obj_serial = registry.serializeObj(obj);
    // duplicate the object from the raw bytes
    std::shared_ptr<MyClass> obj2 = std::static_pointer_cast<MyClass>(registry.deserializeObj(obj_serial));
    obj2->print(); // prints Hello World (15)
}


#endif // #ifndef _REGISTRY_H_