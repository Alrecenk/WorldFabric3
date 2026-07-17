#ifndef _FILE_INTERFACE_H_
#define _FILE_INTERFACE_H_ 1

#include "Variant.h"
#include <memory>

class Saveable;

class FileInterface {
public:

	// Serializes a given element
	virtual Variant save(std::shared_ptr<Saveable> element) = 0;

	// Loads an element serialized with the save function 
	// Element needes to be returned to keep SavePlugin get and save working
	// but if you want to do anything with it on load you need to do so here
	virtual std::shared_ptr<Saveable> load(int file_id, Variant& serial) = 0;

	// does the actual deserialization but doesn't add to external objects
	virtual std::shared_ptr<Saveable> deserialize(Variant& data) = 0;


};
#endif // #ifndef _FILE_INTERFACE_H_
