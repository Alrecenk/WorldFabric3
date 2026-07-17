#include "Variant.h"
#include <cstring> // for memcpy
#include <algorithm> // for find_if
#include <fstream> // for load from file
#include <sstream> // for json load from file
#include <iterator>

using std::string;
using std::map;
using std::vector;



Variant::Variant() {
    type_ = NULL_VARIANT;
    ptr = nullptr;
}

Variant::Variant(const map<string, Variant>& obj) {
    type_ = OBJECT;
    int total_size = Variant::getSize(obj);
    //printf("total size: %i\n", total_size);
    ptr = (byte *) malloc(total_size);
    byte *input_ptr = ptr;
    ((int *) ptr)[0] = (int)obj.size();
    input_ptr += 4;
    map<string, int *> linker;
    // Write the list of keys and types
    for (auto const&[key, data]:obj) {
        //int key_size = Variant::getSize(key);
        //Variant keyvar(key);
        //memcpy(input_ptr, keyvar.ptr, key_size);
        // Build key variant in place for efficiency
        int str_length = (int)key.length();
        ((int *) input_ptr)[0] = str_length;
        memcpy(input_ptr + 4, key.c_str(), str_length);
        int key_size = 4 + str_length ;

        input_ptr += key_size;
        input_ptr[0] = data.type_;
        input_ptr += 1;
        ((int *) input_ptr)[0] = 0; // place holder for relative position
        linker[key] = (int *) input_ptr;
        input_ptr += 4;
    }
    //write the actual objects
    for (auto const&[key, data]:obj) {
        int size = data.getSize();
        if(data.ptr != nullptr){
            memcpy(input_ptr, data.ptr, size);
        }
        // Link relative pointer into first array
        *linker[key] = (int) (input_ptr - (ptr)); // Might throw a warning but should still work in 64 bit environment because it's relative
        input_ptr += size;
    }
}


Variant::Variant(const map<int, Variant> &obj) {
    type_ = INT_OBJECT;
    int total_size = Variant::getSize(obj);
    //printf("total size: %i\n", total_size);
    ptr = (byte *) malloc(total_size);
    byte *input_ptr = ptr;
    ((int *) ptr)[0] = (int)obj.size();
    input_ptr += 4;
    map<int, int *> linker;
    // Write the list of keys and types
    for (auto const&[key, data]:obj) {
        Variant keyvar(key);
        int key_size = keyvar.getSize();
        memcpy(input_ptr, keyvar.ptr, key_size);
        input_ptr += key_size;
        input_ptr[0] = data.type_;
        input_ptr += 1;
        ((int *) input_ptr)[0] = 0; // place holder for relative position
        linker[key] = (int *) input_ptr;
        input_ptr += 4;
    }
    //write the actual objects
    for (auto const&[key, data]:obj) {
        int size = data.getSize();
        if(data.ptr != nullptr){
            memcpy(input_ptr, data.ptr, size);
        }
        // Link relative pointer into first array
        *linker[key] = (int) (input_ptr -
                              (ptr)); // Might throw a warning but should still work in 64 bit environment because it's relative
        input_ptr += size;
    }
}


Variant::Variant(const map<byte, Variant> &obj) {
    type_ = BYTE_OBJECT;
    int total_size = Variant::getSize(obj);
    //printf("total size: %i\n", total_size);
    ptr = (byte *) malloc(total_size);
    byte *input_ptr = ptr;
    ((int *) ptr)[0] = (int)obj.size();
    input_ptr += 4;
    map<byte, int *> linker;
    // Write the list of keys and types
    for (auto const&[key, data]:obj) {
        Variant keyvar(key);
        int key_size = keyvar.getSize();
        memcpy(input_ptr, keyvar.ptr, key_size);
        input_ptr += key_size;
        input_ptr[0] = data.type_;
        input_ptr += 1;
        ((int *) input_ptr)[0] = 0; // place holder for relative position
        linker[key] = (int *) input_ptr;
        input_ptr += 4;
    }
    //write the actual objects
    for (auto const&[key, data]:obj) {
        int size = data.getSize();
        if(data.ptr != nullptr){
            memcpy(input_ptr, data.ptr, size);
        }
        // Link relative pointer into first array
        *linker[key] = (int) (input_ptr -
                              (ptr)); // Might throw a warning but should still work in 64 bit environment because it's relative
        input_ptr += size;
    }
}


Variant::Variant(const string& data) {
    //printf("Making variant from string: %s\n", data.c_str());
    int length = (int)data.length();
    type_ = STRING;
    ptr = (byte *) malloc(4 + length);
    ((int *) ptr)[0] = length;
    memcpy(ptr + 4, data.c_str(), length);
}

Variant::Variant(int data) {
    type_ = INT;
    ptr = (byte *) malloc(4);
    ((int *) ptr)[0] = data;
}

Variant::Variant(short data) {
    type_ = SHORT;
    ptr = (byte *) malloc(2);
    ((short *) ptr)[0] = data;
}

Variant::Variant(byte data) {
    type_ = BYTE;
    ptr = (byte *) malloc(1);
    (ptr)[0] = data;
}

Variant::Variant(float data) {
    type_ = FLOAT;
    ptr = (byte *) malloc(4);
    ((float *) ptr)[0] = data;
}

Variant::Variant(double data) {
    type_ = DOUBLE;
    ptr = (byte *) malloc(8);
    ((double *) ptr)[0] = data;
}

Variant::Variant(const int *data, int array_length) {
    type_ = INT_ARRAY;
    ptr = (byte *) malloc(4 + 4 * array_length);
    ((int *) ptr)[0] = array_length;
    memcpy(ptr + 4, data, 4 * array_length);
}

Variant::Variant(const short *data, int array_length) {
    type_ = SHORT_ARRAY;
    ptr = (byte *) malloc(4 + 2 * array_length);
    ((int *) ptr)[0] = array_length;
    memcpy(ptr + 4, data, 2 * array_length);
}

Variant::Variant(const byte *data, int array_length) {
    type_ = BYTE_ARRAY;
    ptr = (byte *) malloc(4 + array_length);
    ((int *) ptr)[0] = array_length;
    memcpy(ptr + 4, data, array_length);
}

Variant::Variant(const float *data, int array_length) {
    type_ = FLOAT_ARRAY;
    ptr = (byte *) malloc(4 + 4 * array_length);
    ((int *) ptr)[0] = array_length;
    memcpy(ptr + 4, data, 4 * array_length);
}

Variant::Variant(const glm::vec3& data){
    type_ = FLOAT_ARRAY;
    int array_length = 3 ;
    ptr = (byte *) malloc(4 + 4 * array_length);
    ((int *) ptr)[0] = array_length;
    memcpy(ptr + 4, &data, 4 * array_length);
}

Variant::Variant(const glm::vec2& data) {
    type_ = FLOAT_ARRAY;
    int array_length = 2;
    ptr = (byte*)malloc(4 + 4 * array_length);
    ((int*)ptr)[0] = array_length;
    memcpy(ptr + 4, &data, 4 * array_length);
}

Variant::Variant(const glm::mat4& data){
    type_ = FLOAT_ARRAY;
    int array_length = 16 ;
    ptr = (byte *) malloc(4 + 4 * array_length);
    ((int *) ptr)[0] = array_length;
    memcpy(ptr + 4, &data, 4 * array_length);
}

Variant::Variant(const glm::quat& data){
    type_ = FLOAT_ARRAY;
    int array_length = 4 ;
    ptr = (byte *) malloc(4 + 4 * array_length);
    ((int *) ptr)[0] = array_length;
    memcpy(ptr + 4, &data, 4 * array_length);
}

Variant::Variant(const double *data, int array_length) {
    type_ = DOUBLE_ARRAY;
    ptr = (byte *) malloc(4 + 8 * array_length);
    ((int *) ptr)[0] = array_length;
    memcpy(ptr + 4, data, 8 * array_length);
}

Variant::Variant(const vector<Variant> &array) {
    type_ = VARIANT_ARRAY;
    int total_size = Variant::getSize(array);
    ptr = (byte *) malloc(total_size);
    byte *input_ptr = ptr;
    ((int *) ptr)[0] = (int)array.size();
    input_ptr += 4;
    vector<byte *> linker(array.size());
    //printf("total size: %i\n", total_size);
    // Write the list of types and allocate pointers
    for (int k = 0; k < array.size(); k++) {
        input_ptr[0] = array[k].type_;
        input_ptr += 1;
        //((int*)input_ptr)[0] = 0; // place holder for relative position
        linker[k] = input_ptr;
        input_ptr += 4;
    }
    //write the actual objects
    for (int k = 0; k < array.size(); k++) {
        int size = array[k].getSize();
        memcpy(input_ptr, array[k].ptr, size);
        // Link relative pointer into first array
        ((int *) linker[k])[0] = (int) (input_ptr -
                                        (ptr)); // Might throw a warning but should still work in 64 bit environment because it's relative
        input_ptr += size;
    }
}

Variant::Variant(const Variant *source) {
    type_ = source->type_;
    if (source->type_ != NULL_VARIANT) {
        int size = source->getSize();
        ptr = (byte *) malloc(size);
        memcpy(ptr, source->ptr, size);
    }else{
        ptr = nullptr;
    }
}

Variant::Variant(const Variant &source) {
    type_ = source.type_;
    if (source.type_ != NULL_VARIANT) {
        int size = source.getSize();
        ptr = (byte *) malloc(size);
        memcpy(ptr, source.ptr, size);
    }else{
        ptr = nullptr;
    }
}

Variant::Variant(const Type type, const byte *input_ptr) {
    type_ = type;
    if(type != NULL_VARIANT){
        int size = Variant::getSize(type, input_ptr);
        ptr = (byte *) malloc(size);
        memcpy(ptr, input_ptr, size);
    }else{
        ptr = nullptr;
    }
}

Variant::Variant(Variant &&source) noexcept {
    type_ = source.type_;
    ptr = source.ptr;
    source.type_ = NULL_VARIANT;
    source.ptr = nullptr;
    source.cached_hash = false;
}

// Variants own their pointers and free them when deallocated
Variant::~Variant() {
    if(ptr != nullptr && type_ == NULL_VARIANT){
        printf ("Freeing null variant with a pointer! Serialized data is corrupted. \n");
    }
    free(ptr);
    ptr = nullptr;
    type_ = NULL_VARIANT;
}


Variant &Variant::operator=(Variant &&source) {
    if (ptr != nullptr) {
        free(ptr);// Whether freeing a null pointer is valid depends on system
    }
    type_ = source.type_;
    ptr = source.ptr;
    source.type_ = NULL_VARIANT;
    source.ptr = nullptr;

    hash_cached = false;
    cached_array.clear();
    cached_object.clear();
    source.cached_array.clear();
    source.cached_object.clear();

    return *this;
}

int Variant::getSize() const {
    return Variant::getSize(type_, ptr);
}

// returns the size of the data pointed to by a variant's pointer (does not include type)
int Variant::getSize(const Type type, const byte *input_ptr) {
    if (type == OBJECT) {
        int num_keys = *((int *) input_ptr);
        const byte *sinput_ptr = input_ptr + 4;
        for (int k = 0; k < num_keys; k++) {
            sinput_ptr = sinput_ptr + getSize(STRING, sinput_ptr);
            Type type = (Type) (*((char *) sinput_ptr));
            sinput_ptr = sinput_ptr + 1;
            int rel_input_ptr = *((int *) sinput_ptr);
            sinput_ptr = sinput_ptr + 4;
            if (k == num_keys - 1) { // Last item is at end
                return rel_input_ptr + getSize(
                        type, input_ptr +
                              rel_input_ptr); // so size is relative pointer to end of last item
            }
        }
        return 4; // empty object takes 4 bytes for its size
    }
    if (type == INT_OBJECT) {
        int num_keys = *((int *) input_ptr);
        const byte *sinput_ptr = input_ptr + 4;
        for (int k = 0; k < num_keys; k++) {
            sinput_ptr = sinput_ptr + 4; // one int for key
            Type type = (Type) (*((char *) sinput_ptr));
            sinput_ptr = sinput_ptr + 1;
            int rel_input_ptr = *((int *) sinput_ptr);
            sinput_ptr = sinput_ptr + 4;
            if (k == num_keys - 1) { // Last item is at end
                return rel_input_ptr + getSize(
                        type, input_ptr +
                              rel_input_ptr); // so size is relative pointer to end of last item
            }
        }
        return 4; // empty object takes 4 bytes for its size
    }
    if (type == BYTE_OBJECT) {
        int num_keys = *((int *) input_ptr);
        const byte *sinput_ptr = input_ptr + 4;
        for (int k = 0; k < num_keys; k++) {
            sinput_ptr = sinput_ptr + 1; // 1 byte for key
            Type type = (Type) (*((char *) sinput_ptr));
            sinput_ptr = sinput_ptr + 1;
            int rel_input_ptr = *((int *) sinput_ptr);
            sinput_ptr = sinput_ptr + 4;
            if (k == num_keys - 1) { // Last item is at end
                return rel_input_ptr + getSize(
                        type, input_ptr +
                              rel_input_ptr); // so size is relative pointer to end of last item
            }
        }
        return 4; // empty object takes 4 bytes for its size
    } else if (type == STRING) {
        return 4 + (*((int *) input_ptr));// Actually just a byte array serialized
    } else if (type == INT) {
        return 4;
    } else if (type == SHORT) {
        return 2;
    } else if (type == BYTE) {
        return 1;
    } else if (type == FLOAT) {
        return 4;
    } else if (type == DOUBLE) {
        return 8;
    } else if (type == INT_ARRAY) {
        return 4 + (*((int *) input_ptr)) * 4;
    } else if (type == SHORT_ARRAY) {
        return 4 + (*((int *) input_ptr)) * 2;
    } else if (type == BYTE_ARRAY) {
        return 4 + (*((int *) input_ptr));
    } else if (type == FLOAT_ARRAY) {
        return 4 + (*((int *) input_ptr)) * 4;
    } else if (type == DOUBLE_ARRAY) {
        return 4 + (*((int *) input_ptr)) * 8;
    } else if (type == VARIANT_ARRAY) {
        int length = *((int *) input_ptr);
        const byte *sinput_ptr = input_ptr + 4;
        for (int k = 0; k < length; k++) {
            Type type = (Type) (*((byte *) sinput_ptr));
            sinput_ptr = sinput_ptr + 1;
            int rel_input_ptr = *((int *) sinput_ptr);
            sinput_ptr = sinput_ptr + 4;
            if (k == length - 1) { // Last item is at end
                return rel_input_ptr + getSize(
                        type, input_ptr +
                              rel_input_ptr); // so size is relative pointer to end of last item
            }
        }
        return 4;// if no elements
    } else if (type == NULL_VARIANT) {
        return 0;
    }
    printf("Unrecognized type in C++ serializer! Data corruption likely.\n");
    return 0;
}

// returns the size that would need to be allocated to serialize the given object (does not include type)
int Variant::getSize(const map<string, Variant> &obj) {
    int size = 4; // num_keys
    for (auto const&[key, data]:obj) {
        size += getSize(key);
        size += 5; // type and pointer
        size += data.getSize();
    }
    return size;
}

// returns the size that would need to be allocated to serialize the given object (does not include type)
int Variant::getSize(const map<int, Variant> &obj) {
    int size = 4; // num_keys
    for (auto const&[key, data]:obj) {
        size += 4; // int key
        size += 5; // type and pointer
        size += data.getSize();
    }
    return size;
}

// returns the size that would need to be allocated to serialize the given object (does not include type)
int Variant::getSize(const map<byte, Variant> &obj) {
    int size = 4; // num_keys
    for (auto const&[key, data]:obj) {
        size += 1; // byte key
        size += 5; // type and pointer
        size += data.getSize();
    }
    return size;
}

// returns the size that would need to be allocated to serialize the given string (does not include type)
int Variant::getSize(const string &data) {
    return 4 + (int)data.length();
}

// returns the size that would need to be allocated to serialize the given variant array (does not include type)
int Variant::getSize(const vector<Variant> &array) {
    int size = 4; // length
    for (const auto & var : array) {
        size += 5; // type and pointer
        size += var.getSize();
    }
    return size;
}

// Returns a string serialized with this library or its Javascript counterpart (might be UTF-8!)
string Variant::deserializeString(const byte *input_ptr) {
    int length = *((int *) input_ptr);// in bytes, not necessarily characters
    return string((char *) (input_ptr + 4), length);
}

// Returns the string on a Variant holding a string
string Variant::getString() const {
    return Variant::deserializeString(ptr);
}

// Given a pointer to a JS style object returns a map with Variants to the internal elements
// Copies all data so use wisely
map<string, Variant> Variant::deserializeObject(byte *input_ptr) {
    byte *start_input_ptr = input_ptr;
    map<string, Variant> obj;
    int num_keys = *((int *) input_ptr);
    input_ptr = input_ptr + 4;
    for (int k = 0; k < num_keys; k++) {
        string key = deserializeString(input_ptr);
        input_ptr = input_ptr + Variant::getSize(key);
        Type type = (Type) (*((char *) input_ptr));
        input_ptr = input_ptr + 1;
        int rel_input_ptr = *((int *) input_ptr);
        input_ptr = input_ptr + 4;
        Variant v(type, start_input_ptr + rel_input_ptr);
        obj.emplace(key, v);
    }
    return obj;
}


// Given a pointer to an object  with int keys returns a map with Variants to the internal elements
// Copies all data so use wisely
map<int, Variant> Variant::deserializeIntObject(byte *input_ptr) {
    byte *start_input_ptr = input_ptr;
    map<int, Variant> obj;
    int num_keys = *((int *) input_ptr);
    input_ptr = input_ptr + 4;
    for (int k = 0; k < num_keys; k++) {
        int key = *((int *) input_ptr);
        input_ptr = input_ptr + 4;
        Type type = (Type) (*((char *) input_ptr));
        input_ptr = input_ptr + 1;
        int rel_input_ptr = *((int *) input_ptr);
        input_ptr = input_ptr + 4;
        Variant v(type, start_input_ptr + rel_input_ptr);
        obj.emplace(key, v);
    }
    return obj;
}

// Given a pointer to an object  with int keys returns a map with Variants to the internal elements
// Copies all data so use wisely
map<byte, Variant> Variant::deserializeByteObject(byte *input_ptr) {
    byte *start_input_ptr = input_ptr;
    map<byte, Variant> obj;
    int num_keys = *((int *) input_ptr);
    input_ptr = input_ptr + 4;
    for (int k = 0; k < num_keys; k++) {
        byte key = *((byte *) input_ptr);
        input_ptr = input_ptr + 1;
        Type type = (Type) (*((char *) input_ptr));
        input_ptr = input_ptr + 1;
        int rel_input_ptr = *((int *) input_ptr);
        input_ptr = input_ptr + 4;
        Variant v(type, start_input_ptr + rel_input_ptr);
        obj.emplace(key, v);
    }
    return obj;
}

// Returns the object on a Variant holding an Object
map<string, Variant> Variant::getObject() const {
    if (type_ != OBJECT) {
        printf("string object is not a string object! %i\n", type_);
    }
    return Variant::deserializeObject(ptr);
}

// Returns the object on a Variant holding an Object
map<int, Variant> Variant::getIntObject() const {
    if (type_ != INT_OBJECT) {
        printf("int object is not an int object! %i\n", type_);
    }
    return Variant::deserializeIntObject(ptr);
}

// Returns the object on a Variant holding an Object
map<byte, Variant> Variant::getByteObject() const {
    if (type_ != BYTE_OBJECT) {
        printf("byte object is not a byte object! %i\n", type_);
    }
    return Variant::deserializeByteObject(ptr);
}

// Given a pointer to a JS array returns a map with pointers to the internal elements
// Copies all data so use wisely
vector<Variant> Variant::deserializeVariantArray(byte *input_ptr) {
    byte *start_input_ptr = input_ptr;
    int length = *((int *) input_ptr);
    vector<Variant> array;
    input_ptr = input_ptr + 4;
    for (int k = 0; k < length; k++) {
        Type type = (Type) (*((byte *) input_ptr));
        input_ptr = input_ptr + 1;
        int rel_input_ptr = *((int *) input_ptr);
        input_ptr = input_ptr + 4;
        Variant v(
                type, start_input_ptr +
                      rel_input_ptr);// Not commutative, use input_ptr first to get input_ptr
        array.push_back(v);
    }
    return array;
}

// Returns the array on a Variant holding a Variant array
vector<Variant> Variant::getVariantArray() const {
    return Variant::deserializeVariantArray(ptr);
}

Variant  Variant::operator [](int i){
    if(type_ == VARIANT_ARRAY){
        if(cached_array.size() == 0){
            cached_array = getVariantArray();
        }
        if( i < cached_array.size()){
            return cached_array[i];
        }else{
            return Variant();
        }
        //return getVariantArray()[i];
    }else if(type_ == INT_OBJECT){
        return getIntObject()[i];
    }else if(type_ == BYTE_OBJECT){
        return getByteObject()[(byte)i];
    }else if(type_ == INT_ARRAY){
        return Variant(getIntArray()[i]);
    }else if(type_ == FLOAT_ARRAY){
        return Variant(getFloatArray()[i]);
    }else if(type_ == DOUBLE_ARRAY){
        return Variant(getDoubleArray()[i]);
    }else if(type_ == BYTE_ARRAY){
        return Variant(getByteArray()[i]);
    }else if(type_ == SHORT_ARRAY){
        return Variant(getShortArray()[i]);
    }else{
        return Variant();
    }
}

Variant  Variant::operator [](std::string i){
    if(type_ == OBJECT){
        if(cached_object.size() == 0){
            cached_object = getObject();
        }
        if(cached_object.find(i) != cached_object.end()){
            return cached_object[i];
        }else{
            return Variant();
        }
    }else{
        return Variant();
    }
}

Variant Variant::operator [](Variant i){
    if(i.type_ == INT){
        return this->operator[](i.getInt());
    }else if(i.type_ == STRING){
        return this->operator[](i.getString());
    }else{
        return Variant();
    }

}

bool Variant::defined() const{
    return type_ != 0;
}

// Convenience functions for extracting raw types
int Variant::getInt() const {
    return *(int *) ptr;
}

short Variant::getShort() const {
    return *(short *) ptr;
}

float Variant::getFloat() const {
    return *(float *) ptr;
}

double Variant::getDouble() const {
    return *(double *) ptr;
}

byte Variant::getByte() const {
    return *(ptr);
}

// Returns the length of any array type variant (including variant array)
int Variant::getArrayLength() const {
    return getInt(); // Array length is just a preceding int
}

//Convenience methods for returning array pointers
//These do not copy and aren't malloc locations so will crash if you try to free them.
int *Variant::getIntArray() const {
    return (int *) (ptr + 4); // skip length
}

byte *Variant::getByteArray() const {
    return (ptr + 4); // skip length
}

float *Variant::getFloatArray() const {
    return (float *) (ptr + 4); // skip length
}

glm::vec3 Variant::getVec3() const{
    return *((glm::vec3*)getFloatArray()) ;
}

glm::vec2 Variant::getVec2() const {
    return *((glm::vec2*)getFloatArray());
}

glm::mat4 Variant::getMat4() const{
    return *((glm::mat4*)getFloatArray()) ;
}

glm::quat Variant::getQuat() const{
    return *((glm::quat*)getFloatArray()) ;
}

double *Variant::getDoubleArray() const {
    return (double *) (ptr + 4); // skip length
}

short *Variant::getShortArray() const {
    return (short *) (ptr + 4); // skip length
}

/* Numbers passed from javascript could be int, double, or float depending on how they're writtten or stored
This function detects what type came in and converts it to a 32 bit float */
float Variant::getNumberAsFloat() const{
    if(type_ == Variant::INT){
        return (float)*((int*)(ptr)) ;
    }else if(type_ == Variant::DOUBLE){
        return (float)*((double*)(ptr)) ;
    }else if(type_ == Variant::FLOAT){
        return *((float*)(ptr)) ;
    }else{
        return 0;
    }
}

// Prints this variant to the console using printf (no newline)
void Variant::print() const {
    printf("%s", Variant::deserializeToString(type_, ptr).c_str());
}

// Prints a serialized object to a string
string Variant::deserializeToString(char type, byte *input_ptr) {
    string res;
    if (type == INT) {
        res = std::to_string(*((int *) input_ptr));
    } else if (type == SHORT) {
        res = std::to_string(*((short *) input_ptr));
    } else if (type == BYTE) {
        res = std::to_string(*((char *) input_ptr));
    } else if (type == FLOAT) {
        res = std::to_string(*((float *) input_ptr));
    } else if (type == DOUBLE) {
        res = std::to_string(*((double *) input_ptr));
    } else if (type == OBJECT) {
        map<string, Variant> obj = deserializeObject(input_ptr);
        res = "{";
        byte first = true;
        for (auto const&[key, data]:obj) {
            if (!first) {
                res += ",";
            }
            first = false;
            res += "\"" + key + "\":";
            res += deserializeToString(data.type_, data.ptr);
        }
        res += "}";
    } else if (type == INT_OBJECT) {
        map<int, Variant> obj = deserializeIntObject(input_ptr);
        res = "{";
        byte first = true;
        for (auto const&[key, data]:obj) {
            if (!first) {
                res += ",";
            }
            first = false;
            res += "\"" + std::to_string(key) + "\":";
            res += deserializeToString(data.type_, data.ptr);
        }
        res += "}";
    } else if (type == BYTE_OBJECT) {
        map<byte, Variant> obj = deserializeByteObject(input_ptr);
        res = "{";
        byte first = true;
        for (auto const&[key, data]:obj) {
            if (!first) {
                res += ",";
            }
            first = false;
            res += "\""  + std::to_string(key) + "\":";
            res += deserializeToString(data.type_, data.ptr);

        }
        res += "}";
    } else if (type == STRING) {
        res = "\"" + deserializeString(input_ptr) + "\"";
    } else if (type == VARIANT_ARRAY) {
        vector<Variant> array = deserializeVariantArray(input_ptr);
        res = "[";
        byte first = true;
        for (int k = 0; k < array.size(); k++) {
            if (!first) {
                res += ",";
            }
            first = false;
            Variant data = array[k];
            res += deserializeToString(data.type_, data.ptr);
        }
        res += "]";
    } else if (type == INT_ARRAY) {
        int length = *((int *) input_ptr);
        input_ptr += 4;
        res = "[";
        byte first = true;
        for (int k = 0; k < length; k++) {
            if (!first) {
                res += ",";
            }
            first = false;
            res += std::to_string(*((int *) input_ptr));
            input_ptr += 4;
        }
        res += "]";
    } else if (type == SHORT_ARRAY) {
        int length = *((int *) input_ptr);
        input_ptr += 4;
        res = "[";
        byte first = true;
        for (int k = 0; k < length; k++) {
            if (!first) {
                res += ",";
            }
            first = false;
            res += std::to_string(*((short *) input_ptr));
            input_ptr += 2;
        }
        res += "]";
    } else if (type == BYTE_ARRAY) {
        int length = *((int *) input_ptr);
        input_ptr += 4;
        res = "[";
        byte first = true;
        for (int k = 0; k < length; k++) {
            if (!first) {
                res += ",";
            }
            first = false;
            res += std::to_string(*((byte *) input_ptr));
            input_ptr += 1;
        }
        res += "]";
    } else if (type == FLOAT_ARRAY) {
        int length = *((int *) input_ptr);
        input_ptr += 4;
        res = "[";
        byte first = true;
        for (int k = 0; k < length; k++) {
            if (!first) {
                res += ",";
            }
            first = false;
            res += std::to_string(*((float *) input_ptr));
            input_ptr += 4;
        }
        res += "]";
    } else if (type == DOUBLE_ARRAY) {
        int length = *((int *) input_ptr);
        input_ptr += 4;
        res = "[";
        byte first = true;
        for (int k = 0; k < length; k++) {
            if (!first) {
                res += ",";
            }
            first = false;
            res += std::to_string(*((double *) input_ptr));
            input_ptr += 8;
        }
        res += "]";
    } else if (type == NULL_VARIANT) {
        res = "NULL";
    } else {
        res = "Unknown type:" + std::string(1, type);
    }
    return res;
}

string Variant::toString() const {
    return Variant::deserializeToString(type_, ptr);
}


// Returns a deep copy of the given variant
Variant Variant::clone() const {
    return Variant(this);
}




Variant Variant::parseJSON(const std::string& json){
    int key_start = -1;

    int state = 0 ; 
    bool got_value = false;
    
    string key ;
    
    map<string,Variant> obj ;
    int c = 0 ;
    for( int c = 0; c < json.length(); c++){
        if(state == 0){ // 0 = whitespace before key, 
            if( json[c] == '"'){
                state = 1 ;
                key_start = c+1;
            }
        }else if(state == 1){//1 = in key, 
            if( json[c] == '"'){
                key = json.substr(key_start, c-key_start);
                state = 2 ;
            }
        }else if(state == 2){//2 = whitespace before :,
            if(json[c] == ':'){
                state = 3 ;
                got_value = false;
            }
        }else if(state == 3){ // 3 = approaching value
            std::pair<Variant,int> val = Variant::parseJSONValue(json, c);
            obj[key] = val.first.clone();
            c = val.second;
            state = 0 ;
        }
    }
    return Variant(obj);
}

Variant Variant::parseJSONArray(const std::string& json){
    vector<Variant> array ;
    int value_start = 1;
    while(value_start < json.length()-1){
        if(json[value_start] == ',' || json[value_start] == ']' || json[value_start] == '}'){
            value_start++;
            continue;
        }
        std::pair<Variant,int> val = Variant::parseJSONValue(json, value_start);
        array.push_back(val.first.clone());
        value_start = val.second;
    }
    return Variant(array);
}

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    ltrim(s);
    rtrim(s);
}

std::pair<Variant,int> Variant::parseJSONValue(const std::string& json, int value_start){
    Variant var ;
    int c ;
    bool got_value =false ;
    for(c = value_start; c < json.length();c++){
        if(json[c] == '{'){
            // Find the matching }
            int obj_end = c + 1;
            int open = 1 ;
            while(open != 0){
                obj_end++;
                if(json[obj_end] == '{'){
                    open++;
                }else if(json[obj_end] == '}'){
                    open--;
                }
            }
            string value = json.substr(c,obj_end-c+1);
            var = Variant::parseJSON(value);
            c = obj_end;
            got_value = true;
        }else if(json[c] == '['){
            // Find the matching ]
            int array_end = c ;
            int open = 1 ;
            while(open != 0){
                array_end++;
                if(json[array_end] == '['){
                    open++;
                }else if(json[array_end] == ']'){
                    open--;
                }
            }
            string value = json.substr(c,array_end-c+1);
            var = Variant::parseJSONArray(value);
            c = array_end;
            got_value = true;
        }else if(json[c] == '"'){
            int string_end = (int)json.find('"', c+1);
            string value = json.substr(c+1,string_end-c-1); // don't pick up " for strings
            var = Variant(value) ;
            c = string_end ;
            got_value = true;
        }else if(json[c] == ',' || json[c] == ']' || json[c] == '}'){ // got to , not inside nested structure
            if(!got_value && value_start != c){ // must be a number if nothing else
                string value = json.substr(value_start, c - value_start);
                trim(value);
                if(value == "true"){ // TODO proper trimming!
                    var = Variant((int)1);
                }else if(value == "false" || value == ""){
                    var = Variant((int)0);
                }else if(value.find('.') == string::npos){
                    int i = std::stoi(value);
                    var = Variant(i);
                }else{
                    double d = std::stod(value);
                    var = Variant(d);
                }
            }
            break;
        }

    }
    return std::pair<Variant, int>(var, c);
}

void Variant::printFormatted() const{
    std::string s = this->toString() ;
    if(type_ != Variant::OBJECT){
        printf("%s\n", s.c_str());
    }else{
        Variant::printJSON(s);
    }
}

void Variant::printJSON(const std::string& json){
    
    int indents = 0;
    int line_start = 0 ;
    bool stop_comma_break = false;

    for(int k=0;k < json.length(); k++){
        char c = json[k];
         if(c == '[' && k!=0){
            bool found_nest = false;
            int j=k+1;
            while(json[j] != ']'){
                if(json[j] == '[' || json[j] == '{'){
                    found_nest = true;
                }
                j++;
            }
            stop_comma_break = !found_nest ;
        }

        

        if(!stop_comma_break){
            if(c == ',' || c == '{' || c == '['){
                string blanks(indents*2, ' ');
                printf("%s%s \n",blanks.c_str(), json.substr(line_start,k-line_start+1).c_str());
                line_start = k+1 ;
            }
            if(c == '}' || c == ']'){
                string blanks(indents*2, ' ');
                printf("%s%s \n",blanks.c_str(), json.substr(line_start,k-line_start).c_str());
                line_start = k ;
            }
        }
        if(c == '}' || c == ']'){
            indents--;
        }
        if(c == '{' || c == '['){
            indents++;
        }
        if(c == ']' || c == '}'){
            stop_comma_break = false;
        }
    }

    if(json[0] == '['){
        printf("]\n");
    }else if(json[0] == '{'){
        printf("}\n");
    }

}


// returns the hash of a variant
int Variant::hash(){
    //return Variant::murmur((uint8_t*)ptr, getSize(), 17);
    
    if (!hash_cached && defined()) {
        computeHash();
    }
    return cached_hash;
    
}

void Variant::computeHash(){
    cached_hash = Variant::murmur((uint8_t*)ptr, getSize(), 17);
    hash_cached = true;
}

int Variant::hash(string s){
    return Variant(s).hash();
}


uint32_t Variant::murmurscramble(uint32_t k) {
    k *= 0xcc9e2d51;
    k = (k << 15) | (k >> 17);
    k *= 0x1b873593;
    return k;
}

// I grabbed a hash function that seemed pretty good from wikipedia
// It's in the public domain
uint32_t Variant::murmur(const uint8_t* key, size_t len, uint32_t seed) {
    uint32_t h = seed;
    uint32_t k;
    /* Read in groups of 4. */
    for (size_t i = len >> 2; i; i--) {
        // Here is a source of differing results across endiannesses.
        // A swap here has no effects on hash properties though.
        memcpy(&k, key, sizeof(uint32_t));
        key += sizeof(uint32_t);
        h ^= murmurscramble(k);
        h = (h << 13) | (h >> 19);
        h = h * 5 + 0xe6546b64;
    }
    /* Read the rest. */
    k = 0;
    for (size_t i = len & 3; i; i--) {
        k <<= 8;
        k |= key[i - 1];
    }
    // A swap is *not* necessary here because the preceding loop already
    // places the low bytes in the low places according to whatever endianness
    // we use. Swaps only apply when the memory is copied in a chunk.
    h ^= murmurscramble(k);
    /* Finalize. */
    h ^= len;
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

void Variant::makeFillableByteArray(int size){
    if(type_ != NULL_VARIANT){
        free(ptr);
    }
    type_ = Variant::BYTE_ARRAY ;
    ptr = (byte*) malloc(4 + size) ;
    *(int*)(ptr) = size ;
}

void Variant::makeFillableShortArray(int size) {
    if (type_ != NULL_VARIANT) {
        free(ptr);
    }
    type_ = Variant::SHORT_ARRAY;
    ptr = (byte*)malloc(4 + 2 *  size);
    *(int*)(ptr) = size;
}

void Variant::makeFillableIntArray(int size){
    if(type_ != NULL_VARIANT){
        free(ptr);
    }
    type_ = Variant::INT_ARRAY ;
    ptr = (byte*) malloc(4 * ( 1 + size)) ;
    *(int*)(ptr) = size ;
}

void Variant::makeFillableFloatArray(int size){
    if(type_ != NULL_VARIANT){
        free(ptr);
    }
    type_ = Variant::FLOAT_ARRAY ;
    ptr = (byte*) malloc(4 * ( 1 + size)) ;
    *(int*)(ptr) = size ;
}

void Variant::makeFillableDoubleArray(int size){
    if(type_ != NULL_VARIANT){
        free(ptr);
    }
    type_ = Variant::DOUBLE_ARRAY ;
    ptr = (byte*) malloc(4 + 8 * size) ;
    *(int*)(ptr) = size ;
}

Variant Variant::loadFileBytes(std::string file_path) {
    std::ifstream input(file_path, std::ios::binary);

    if (input.fail()) {
        printf("Failed to load file: %s\n", file_path.c_str());
        return Variant();
    }

    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(input), {});
    //printf("File size: %d\n", buffer.size());
    Variant result;
    result.makeFillableByteArray((int)buffer.size());
    byte* bytes = result.getByteArray();
    for (int k = 0; k < buffer.size(); k++) {
        bytes[k] = buffer[k];
    }
    return result;
}

	// Saves the raw bytes of this Variant to a file
   // Works with any type of variant but the type is not saved
void Variant::saveFile(const std::string& file_path) {
    std::ofstream output(file_path, std::ios::binary);
    if (output) {
        int length = getSize();
        output.write((char*)ptr, length);
        output.close();
    } else {
        printf("Failed to save file: %s\n", file_path.c_str());
    }

}

// Saves the raw bytes of this Variant to a file but skips array length
//This is for when you need to save raw bytes that won't be loaded as a Variant
void Variant::saveBytesToFile(const std::string& file_path) {
	std::ofstream output(file_path, std::ios::binary);
	if (output) {
		int length = getSize();
		output.write((char*)(ptr+4), length); // skip amount
		output.close();
	}
	else {
		printf("Failed to save file: %s\n", file_path.c_str());
	}

}

Variant Variant::loadFile(std::string file_path, Type type) {
    std::ifstream input(file_path, std::ios::binary);
    if (input.fail()) {
        printf("Failed to load file: %s\n", file_path.c_str());
        return Variant();
    }
    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(input), {});
    //printf("File size: %d\n", buffer.size());
    Variant result;
    result.ptr = (byte*)malloc((int)buffer.size());
    result.type_ = type;
    for (int k = 0; k < buffer.size(); k++) {
        result.ptr[k] = buffer[k];
    }
    return result;
}

// Saves the generated JSON of this variant to a file as ascii text
    // Variant should be some type of object to work with loadJSONFile
void Variant::saveJSONFile(std::string file_path) {
    std::ofstream output(file_path);
    if (output) {
        std::string json = toString();
        output << json;
        output.close();
    }else {
        printf("Failed to save file: %s\n", file_path.c_str());
    }
    
}

// Loads any JSON file into a Variant as a string object at the top level
Variant Variant::loadJSONFile(std::string file_path) {
    std::ifstream input(file_path);
    if (input.fail()) {
        printf("Failed to load file: %s\n", file_path.c_str());
        return Variant();
    }
    std::ostringstream oss; // pass through a thing to force full buffere load?
    oss << input.rdbuf();
    std::string json = oss.str();
    return parseJSON(json);
}


//Read amount of bytes starting the given array position and place them into the given buffer
void Variant::readBytes(int pos, int amount, byte* buffer) {
        memcpy(buffer, ptr+4+pos, amount);
}

//Reads a short from the given position in a byte array
short Variant::readShort(int pos) {
    return *(short*)(ptr+4+pos);
}

//Reads an int from the given position in a byte array
int Variant::readInt(int pos) {
    return *(int*)(ptr + 4 + pos);
}