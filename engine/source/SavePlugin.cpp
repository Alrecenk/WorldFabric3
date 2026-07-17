#include "SavePlugin.h"
#include "Saveable.h"
#include <filesystem>

SavePlugin::SavePlugin() {

}

// Called on every plug-in before any plug-ins are run
void SavePlugin::initialize() {
	
}

void SavePlugin::run() {
	recursive_lock.lock();
	for (const std::string& file : save_queue) {
		FileSet& file_set = files[file];

		//printf("Saving %s...\n", file.c_str());
		std::map<int, Variant> serial;
		for (auto& [file_id, item] : file_set.elements) {
			Variant serialized_item = file_set.file_interface->save(item);
			if (serialized_item.defined()) {
				serial[file_id] = std::move(serialized_item);
			}
		}

		Variant bytes = Variant(serial);

		if (file_set.json) {
			bytes.saveJSONFile(file);

		}else {
			
			//bytes.printFormatted();
			bytes.saveFile(file);
		}
		//printf("saved %s!\n", file.c_str());
	}
	
	save_queue.clear();

	for (const std::string& file : load_queue) {
		FileSet& file_set = files[file];
		if (file_set.json) {
			//printf("loading a JSON...\n");

			Variant bytes = Variant::loadJSONFile(file);// TODO make actual hard drive access async
			if (!bytes.defined()) {
				printf("File could not load.\n");
				continue;
			}

			//bytes.printFormatted();

			std::map<std::string, Variant> serial = bytes.getObject(); // JSON will always load as a string object


			for (auto& [file_id_string, data] : serial) {
				int file_id = std::stoi(file_id_string);
				//printf("Calling load for file_id: %d\n", file_id);
				std::shared_ptr<Saveable> loaded_item = file_set.file_interface->load(file_id, data);
				if (loaded_item) { // an interface could choose not to load something, which would be indicated with a nullptr return
					loaded_item->file_id = file_id;
					loaded_item->file_pointer = &file_set;
					file_set.add(loaded_item);
				}
			}
		}else {
			//printf("Starting Loading var %s...\n", file.c_str());
			Variant bytes = Variant::loadFile(file, Variant::INT_OBJECT);
			if (!bytes.defined()) {
				printf("File could not load.\n");
				continue;
			}
			//bytes.printFormatted();


			std::map<int, Variant> serial = bytes.getIntObject();


			for (auto& [file_id, data] : serial) {
				std::shared_ptr<Saveable> loaded_item = file_set.file_interface->load(file_id, data);
				if (loaded_item) { // an interface could choose not to load something, which would be indicated with a nullptr return
					loaded_item->file_id = file_id;
					loaded_item->file_pointer = &file_set;
					file_set.add(loaded_item);
				}
			}
			//printf("Finished Loading var %s...\n", file.c_str());
		}
	}
	load_queue.clear();

	recursive_lock.unlock();
	
}

void SavePlugin::FileSet::add(std::shared_ptr<Saveable> element) {
	elements[element->file_id] = element;
	next_file_id = std::max(next_file_id, element->file_id + 1);
}

// Pushes all items in the tagged set attached to the file out to the hard drive
// Run asynchronously on the save plugin thread, allowing execution to continue immdiately from this call (check isSaving for completion)
void SavePlugin::save(const std::string& file) {
	recursive_lock.lock();
	//printf("Save Queued %s\n", file.c_str());
	save_queue.insert(file);
	recursive_lock.unlock();
}

// Pulls the contents of a file and creates new tagged data items for them calimng load on each after they have all been oaded
// Run asynchronously on the save plugin thread, allowing execution to continue immdiately from this call (check isLoading for completion)
void SavePlugin::load(const std::string& file) {// TODO make all the string params const&
	recursive_lock.lock();
	load_queue.insert(file);
	recursive_lock.unlock();
}


// Attachs a file interface to the given file which provides functions for serializing and deserializing all of the items in that file
void SavePlugin::setInterface(const std::string& file, std::shared_ptr<FileInterface> file_interface) {
	files[file].file_interface = file_interface;
}

// Attaches a saveable element to a file to be included with save/load calls on that file
// Also assigns a file_id to the object and returns it, which will be maintained across saving and loading which can be used as a pointer
int SavePlugin::attach(std::shared_ptr<Saveable> element, const std::string& file) {
	recursive_lock.lock();
	FileSet& file_set = files[file];
	element->file_id = file_set.next_file_id;
	file_set.next_file_id++;
	element->file_pointer = &file_set;
	file_set.elements[element->file_id] = element;
	recursive_lock.unlock();
	return element->file_id;
}


// Get the next id and increment the id, useful if you want to use the file_ID to construct an object before adding it
int SavePlugin::reserveID(std::string file) {
	FileSet& file_set = files[file];
	int r = file_set.next_file_id;
	file_set.next_file_id++;
	return r; 
}
// same as other attach except using a prereserved id
void SavePlugin::attach(int reserved_id, std::shared_ptr<Saveable> element, const std::string& file) {
	recursive_lock.lock();
	FileSet& file_set = files[file];
	element->file_id = reserved_id;
	element->file_pointer = &file_set;
	file_set.elements[element->file_id] = element;
	recursive_lock.unlock();
}

// Removes an element from a file, settings its file_id and file_interface to -1 but otherwise not changing it
void SavePlugin::detach(std::shared_ptr<Saveable> element) {
	recursive_lock.lock();
	if (element->file_pointer != nullptr) {
		//printf("detaching..\n");
		FileSet* file_set = element->file_pointer;
		file_set->elements.erase(element->file_id);
		//element->file_id = -1;
		element->file_pointer = nullptr;
	}
	recursive_lock.unlock();
}

//Removes an entire file from save/load tracking (allows loading multiple copies of a file but only one can save)
void SavePlugin::detach(std::string file) {
	recursive_lock.lock();
	//printf("detaching file: %s\n", file.c_str());
	FileSet& to_remove = files[file];
	std::vector<int> id_to_remove;
	for (auto& [id, element] : to_remove.elements) {
		if (element->file_pointer == &to_remove) {
			//printf("detaching..\n");
			FileSet* file_set = element->file_pointer;
			
			id_to_remove.push_back(element->file_id);
			//element->file_id = -1;
			element->file_pointer = nullptr;
		}
	}
	for (auto id : id_to_remove) {
		to_remove.elements.erase(id);
	}
	files.erase(file);
	recursive_lock.unlock();
}

bool SavePlugin::isSaving() {
	//printf("isSaving checked %d !\n", (int)(save_queue.size()));
	return save_queue.size() != 0;
}

bool SavePlugin::isLoading() {
	//if (load_queue.size() != 0) {
		//printf("isLoading checked true %d !\n", (int)(save_queue.size()));
	//}
	return load_queue.size() != 0;
}

std::shared_ptr<FileInterface> SavePlugin::getInterface(const std::string& file) {
	return files[file].file_interface;

}

// Moves all of the items currently attahed to from_file to be attached to to_file instead
void SavePlugin::move(const std::string& from_file, const std::string& to_file, std::shared_ptr<FileInterface> file_interface) {
	recursive_lock.lock();
	setInterface(to_file, file_interface);

	FileSet& from_file_set = files[from_file];
	FileSet& to_file_set = files[to_file];
	to_file_set.next_file_id = from_file_set.next_file_id;
	for (auto& [file_id, item] : from_file_set.elements) {
		to_file_set.elements[file_id] = item;;
		item->file_pointer = &to_file_set;
	}
	files.erase(from_file);
	recursive_lock.unlock();
}

// Sets whether a file is json or a more efficient binary
void SavePlugin::setJSON(const std::string& file, bool json) {
	files[file].json = json;
}

//Sometimes you gotta make folders, seems like that's something a saveplugin should do
bool SavePlugin::folderExists(const std::string& path) {
	return std::filesystem::is_directory(path);
}
//returns if successful
bool SavePlugin::createFolder(const std::string& path) {
	return std::filesystem::create_directories(path);
}

std::string SavePlugin::getAppDataLocation(const std::string app_name) {

	char* appdata_path;
	size_t appdata_path_len;
	std::string apps_data_location = "";

	if (_dupenv_s(&appdata_path, &appdata_path_len, "APPDATA") == 0) {
		apps_data_location = std::string(appdata_path) + "/" + app_name;
		free(appdata_path); // Don't forget to free the memory!
	}
	else {
		printf("APPDATA environment variable not found. Appdata will be saved in the app's folder.\n");
	}
	return apps_data_location;
}