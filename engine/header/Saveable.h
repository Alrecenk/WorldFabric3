#ifndef _SAVEABLE_H_
#define _SAVEABLE_H_ 1

#include "SavePlugin.h"

class Saveable {
public:
	int file_id = -1; // id of this object within its file (does persist across runs)
	SavePlugin::FileSet* file_pointer = nullptr; // link is used to detach a file quickly without having to know the filename

	virtual ~Saveable() {} // Makes saveable polymorphic (dynamic castable) and guarantees proper destructor get called when file items are deleted
};
#endif // #ifndef _SAVEABLE_H_