#ifndef _FLAG_SET_H_
#define _FLAG_SET_H_ 1


#include <string>
#include <unordered_map>
#include "Variant.h"


class FlagSet {

public:

	static inline const std::string APP_PATH = "app_path" ;

	Variant getValue(const std::string& key){
		return flags[key].clone();
	}

	std::string getString(const std::string& key){
		if (flags[key].type_ == Variant::STRING) {
			return flags[key].getString();
		}else{
			return "" ;
		}
	}

	int getInt(const std::string& key) {
		if(this == nullptr){ // asking for a flag when the fla gtool isn't initialized returns 0 instead of crashing
			return 0 ;
		}
		if(flags[key].type_ == Variant::INT){
			return flags[key].getInt();
		}else if(flags[key].type_ == Variant::STRING){
			return stoi(getString(key)) ;
		}
		return 0 ;
	}

	void setValue(const std::string& key, const Variant& value){
		flags[key] = value.clone() ;
	}

	void setInt(const std::string& key, int value){
		setValue(key, Variant(value));
	}

	void setString(const std::string& key, const std::string& value){
		setValue(key, Variant(value));
	}

	// Constructed with flagsfrom console paramters
	FlagSet(int argc, char* argv[]) {
		setString(APP_PATH, std::string(argv[0]));
		for (int k = 1; k < argc; k += 2) {
			std::string key(argv[k]);
			std::string value(argv[k + 1]);
			setString(key, value);
		}
	}

private:
	std::unordered_map<std::string, Variant> flags ;

};
#endif // #ifndef _FLAG_SET_H_