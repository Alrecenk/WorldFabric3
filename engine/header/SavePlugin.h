#ifndef _SAVE_PLUGIN_H_
#define _SAVE_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "Utilities.h"
#include "FileInterface.h"

#include <stdio.h>
#include <cstdlib>

#include <iostream>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

class Saveable;


class SavePlugin : public AsyncPlugin {

public:

	static inline std::string tag = "SaveLink";

	class FileSet {
	public:
		
		int next_file_id = 1; 
		std::unordered_map<int, std::shared_ptr<Saveable>> elements; // maps file id to main data local id
		std::shared_ptr<FileInterface> file_interface; // which saving and loading interface is to be used for this file, needs to be consistent across runs
		bool json = false; // whether this file is saved and loaded using ascii

		void add(std::shared_ptr<Saveable> element);
	};
	

	SavePlugin();

	// Called on every plug-in before any plug-ins are run
	void initialize() override;

	void run() override;

	// Pushes all items in the tagged set attached to the file out to the hard drive
	// Run asynchronously on the save plugin thread, allowing execution to continue immdiately from this call (check isSaving for completion)
	void save(const std::string& file);

	// Pulls the contents of a file and creates new tagged data items for them calling load on each after they have all been loaded
	// Run asynchronously on the save plugin thread, allowing execution to continue immdiately from this call (check isLoading for completion)
	void load(const std::string& file);

	// Attachs a file interface to the given file which provides functions for serializing and deserializing all of the items in that file
	void setInterface(const std::string& file, std::shared_ptr<FileInterface> file_interface);

	// Attaches a saveable element to a file to be included with save/load calls on that file
	// Also assigns a file_id to the object and returns it, which will be maintained across saving and loading which can be used as a pointer
	int attach(std::shared_ptr<Saveable> element, const std::string& file);

	// Get the next id and increment the id, useful if you want to use the file_id to construct an object before adding it
	int reserveID(std::string file);

	// same as other attach except using a pre-reserved id (undefined behavior if you call with an id that wasn't reserved)
	void attach(int reserved_id, std::shared_ptr<Saveable> element, const std::string& file);

	// Removes an element from a file, settings its file_id and file_interface to -1 but otherwise not changing it
	void detach(std::shared_ptr<Saveable> element);

	//Removes an entire file from save/load tracking (allows loading multiple copies of a file but only one can save)
	void detach(std::string file);

	bool isSaving();

	bool isLoading();

	std::shared_ptr<FileInterface> getInterface(const std::string& file);

	// Moves all of the items currently attahed to from_file to be attached to to_file instead
	void move(const std::string& from_file, const std::string& to_file, std::shared_ptr<FileInterface> file_interface);

	// Sets whether a file is json or a more efficient binary
	void setJSON(const std::string& file, bool json);

	//Sometimes you gotta check folders, seems like that's something a saveplugin should do
	static bool folderExists(const std::string& path);

	//returns if successful
	static bool createFolder(const std::string& path);

	static std::string getAppDataLocation(const std::string app_name);


private:
	std::unordered_map <std::string, FileSet> files;
	std::recursive_mutex recursive_lock;
	std::unordered_set<std::string> save_queue; // current files waiting to be saved
	std::unordered_set<std::string> load_queue; // current files waiting to be loaded
};
#endif // #ifndef _SAVE_PLUGIN_H_
