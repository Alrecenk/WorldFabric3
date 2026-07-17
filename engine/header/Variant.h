#ifndef _VARIANT_H_
#define _VARIANT_H_ 1

#include <map>
#include <vector>
#include <string>
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

typedef unsigned char byte;


class Variant {
  public:
    enum Type {
        NULL_VARIANT = 0,
        OBJECT = 1,
        STRING = 2,
        INT = 3,
        SHORT = 4,
        BYTE = 5,
        FLOAT = 6,
        DOUBLE = 7,
        INT_ARRAY = 8,
        SHORT_ARRAY = 9,
        BYTE_ARRAY = 10,
        FLOAT_ARRAY = 11,
        DOUBLE_ARRAY = 12,
        VARIANT_ARRAY = 13,
        INT_OBJECT = 14,
        BYTE_OBJECT = 15
    };
    //TODO make const and use initializer lists on constructors or make private and provide accessors that give copies?
    Type type_ = NULL_VARIANT; // Type of object pointed to by ptr
    byte* ptr = nullptr; // Pointer to data
    bool hash_cached = false;
    int cached_hash = 0;


    std::vector<Variant> cached_array = std::vector<Variant>();
    std::map<std::string, Variant> cached_object = std::map<std::string, Variant>() ;
    /* Constructor is overloaded for every supported type
    Complex objects are built from other Variants so must be built from the bottom up
    Creation of new Variants always copy the source data in its entirety (use wisely)
    */
    Variant(); //makes a NULL_VARIANT
    explicit Variant(const std::map<std::string, Variant>& obj);

    explicit Variant(const std::map<int, Variant>& obj);

    explicit Variant(const std::map<byte, Variant>& obj);

    explicit Variant(const std::string& data);

    explicit Variant(int data);

    explicit Variant(short data);

    explicit Variant(byte data);

    explicit Variant(float data);

    explicit Variant(double data);

    Variant(const int* data, int array_length);

    Variant(const short* data, int array_length);

    Variant(const byte* data, int array_length);

    Variant(const float* data, int array_length);

    Variant(const glm::vec3& data); // maps to float*

    Variant(const glm::vec2& data); // maps to float*

    Variant(const glm::mat4& data); // maps to float*

    Variant(const glm::quat& data); // maps to float*

    Variant(const double* data, int array_length);

    explicit Variant(const std::vector<Variant>& array);

    // deep copy constructor for pointer
    explicit Variant(const Variant* source);

    // explicitly override copy constrctor to be deep
    Variant(const Variant& a);

    // deep copy from externally serialized data
    Variant(const Type type, const byte* input_ptr);

    // explicit move constructor removes copies with explicit moves
    Variant(Variant&& source) noexcept;

    // explicit move = operator allows variant = std::move(other_variant)
    Variant& operator=(Variant&& other);

    // Variants own their pointers and free them when deallocated
    ~Variant();

    // returns the size of the data pointed to by a variant's pointer (does not include type)
    // allows checking from serialized data without the copy of creating a Variant
    static int getSize(const Type type, const byte* input_ptr);

    int getSize() const; // convenient if you've got a Variant already

    // returns the size that would need to be allocated to serialize the given object (does not include type)
    static int getSize(const std::map<std::string, Variant>& obj);

    // returns the size that would need to be allocated to serialize the given object (does not include type)
    static int getSize(const std::map<int, Variant>& obj);

    // returns the size that would need to be allocated to serialize the given object (does not include type)
    static int getSize(const std::map<byte, Variant>& obj);

    // returns the size that would need to be allocated to serialize the given string (does not include type)
    static int getSize(const std::string& data);

    // returns the size that would need to be allocated to serialize the given variant array (does not include type)
    static int getSize(const std::vector<Variant>& array);

    // Returns a string serialized with this library or its Javascript counterpart (might be UTF-8!)
    static std::string deserializeString(const byte* input_ptr);

    // Returns the string on a Variant holding a string
    std::string getString() const;

    // Given a pointer to a JS style object returns a map with Variants to the internal elements
    // Copies all data so use wisely
    static std::map<std::string, Variant> deserializeObject(byte* input_ptr);

    // Given a pointer to an object with int keys returns a map with Variants to the internal elements
    // Copies all data so use wisely
    static std::map<int, Variant> deserializeIntObject(byte* input_ptr);

    // Given a pointer to an object  with byte keys returns a map with Variants to the internal elements
    // Copies all data so use wisely
    static std::map<byte, Variant> deserializeByteObject(byte* input_ptr);

    // Returns the object on a Variant holding an Object
    std::map<std::string, Variant> getObject() const;

    // Returns the object on a Variant holding an int Object
    std::map<int, Variant> getIntObject() const;

    // Returns the object on a Variant holding an Object
    std::map<byte, Variant> getByteObject() const;

    // Given a pointer to a JS array returns a map with pointers to the internal elements
    // Copies all data so use wisely
    static std::vector<Variant> deserializeVariantArray(byte* input_ptr);

    // Returns the array on a Variant holding a Variant array
    std::vector<Variant> getVariantArray() const;

    Variant operator [](int i);

    Variant operator [](std::string i);

    Variant operator [](Variant i);

    bool defined() const;
    


    // Convenience functions for extracting raw types
    int getInt() const;

    short getShort() const;

    float getFloat() const;

    double getDouble() const;

    byte getByte() const;

    // Returns the length of any array type variant (including variant array)
    int getArrayLength() const;

    //Convenience methods for returning array pointers
    //These do not copy and do not need to be freed.
    int* getIntArray() const;

    byte* getByteArray() const;

    float* getFloatArray() const;

    glm::vec3 getVec3() const;

    glm::vec2 getVec2() const;

    glm::mat4 getMat4() const;

    glm::quat getQuat() const;

    double* getDoubleArray() const;

    short* getShortArray() const;

/* Numbers passed from javascript could be int, double, or float depending on how they're writtten or stored
This function detects what type came in and converts it to a 32 bit float */
    float getNumberAsFloat() const;

    // Prints this variant to the console using printf (no newline)
    void print() const;

    // Prints this variant to the console using printf with nice formatting for nested JSON
    void printFormatted() const;

    static void printJSON(const std::string& json);

    // Prints a serialized object to a string
    static std::string deserializeToString(char type, byte* input_ptr);

    std::string toString() const;

    // Returns a deep copy of the given variant
    Variant clone() const;

    static Variant parseJSON(const std::string& json);

    static Variant parseJSONArray(const std::string& json);

    static std::pair<Variant,int> parseJSONValue(const std::string& json, int value_start);


    // returns the hash of a variant (uses cached version if it's been called before)
    int hash();

    // Recomputes the hash
    void computeHash();

    // return hash of a string
    static int hash(std::string s);

    // I grabbed a hash function that seemed pretty good from wikipedia
    // It's in the public domain
    static uint32_t murmur(const uint8_t* key, size_t len, uint32_t seed);

    static uint32_t murmurscramble(uint32_t k);

    void makeFillableByteArray(int size);

    void makeFillableShortArray(int size);

    void makeFillableIntArray(int size);

    void makeFillableFloatArray(int size);

    void makeFillableDoubleArray(int size);

    // Loads the raw bytes of a file into a byte array Variant
    static Variant loadFileBytes(std::string file_path);

    // Saves the raw bytes of this Variant to a file
    // Works with any type of variant but the type is not saved
    void saveFile(const std::string& file_path);

	// Saves the raw bytes of this Variant to a file but skips array length
	//This is for when you need to save raw bytes that won't be loaded as a Variant
	void saveBytesToFile(const std::string& file_path) ;

    // Loads a Variant saved with SaveFile
    static Variant loadFile(std::string file_path, Type type);

    //Read amount of bytes starting the given array position and place them into the given buffer
    void readBytes(int pos, int amount, byte* buffer);

    //Reads a short from the given position in a byte array
    short readShort(int pos);

    //Reads an int from the given position in a byte array
    int readInt(int pos);


    // Saves the generated JSON of this variant to a file as ascii text
    // Variant should be some type of object to work with loadJSONFile
    void saveJSONFile(std::string file_path);

    // Loads any JSON file into a Variant as a string object at the top level
    static Variant loadJSONFile(std::string file_path);

};

#endif // #ifndef _VARIANT_H_