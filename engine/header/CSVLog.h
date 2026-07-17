#ifndef _CSV_LOG_H_
#define _CSV_LOG_H_ 1

#include <fstream>
#include <string>
#include <set>

class CSVLog{
public:
	//TODO add some more toString's for vectors or such, make sure not to use ',' as their internal separators

	//convert common types to a string
	static inline std::string toString(const std::string& v) {
		return v;
	}
	static inline std::string toString(const char* v) {
		return v ? std::string(v) : std::string();
	}
	static inline std::string toString(char v) {
		return std::string(1, v);
	}
	static inline std::string toString(bool v) {
		return v ? "true" : "false";
	}

	//Attempto rely on << for anything else
	template <class T>
	static inline std::string toString(const T& v) {
		std::ostringstream oss;
		oss << std::setprecision(std::numeric_limits<double>::digits10) ;
		oss << v;                     // rely on operator<<
		return oss.str();
	}

	//Escape characters that could break the csv
	static inline std::string escape(const std::string& s){
		if (s.find_first_of(",\"\n\r") == std::string::npos)
			return s;
		std::string out = "\"";
		for (char c : s) {
			if (c == '"'){
				out += "\"\"";   // double the quote per RFC 4180
			}else{
				out += c;
			}
		}
		out += '"';
		return out;
	}

	std::ofstream file; //TODO use a general stream to enable sending logging data over the network?
	
	//holds log lines in a time sorted order so they can be output in that time order instead of when logged
	std::map<double, std::vector<std::string>> sort_buffer ;
	static inline double max_buffer_time = 2.0; // how deep in time the buffer can get before being pushed to the log file

	template <class... Values>
	void log(const Values&... values){
		std::size_t i = 0;
		((file << escape(toString(std::forward<const Values&>(values))) << (++i == sizeof...(Values) ? '\n' : ',')), ...) ;
	}

	//returns a lotg line include \n without actually logging it
	template <class... Values>
	std::string getLogLine(const Values&... values) {
		std::ostringstream oss;
		std::size_t i = 0;
		((oss << escape(toString(std::forward<const Values&>(values))) << (++i == sizeof...(Values) ? '\n' : ',')), ...);
		return oss.str();
	}

	void logLine(const std::string& line){
		file << line ;
	}
	
	template <class... Values>
	void logOrdered(double time, const Values&... values) {
		std::string line = getLogLine(std::forward<const Values&>(values)...) ;
		sort_buffer[time].push_back(line) ;
		// Pop everything offthe buffer that is older than max_bufer_time
		auto it = sort_buffer.begin() ;
		while(it->first < time - max_buffer_time){
			for(auto& line : it->second){
				logLine(line) ;
			}
			sort_buffer.erase(it->first) ;
			it = sort_buffer.begin();
		}
	}

	template <class... Values>
	CSVLog(const std::string& path, const Values&... header):file(path){

		if (!file)
			throw std::runtime_error("CSVLog: cannot open file '" + path + "'");
		log(std::forward<const Values&>(header)...) ;
	}

	void close(){
		file.close();
	}

	~CSVLog(){
		close();
	}

	CSVLog(const CSVLog&) = delete;
	CSVLog& operator=(const CSVLog&) = delete;


	// splits a CSV line into the raw strings
	static inline std::vector<std::string> splitCSVLine(const std::string& line){
		std::vector<std::string> fields;
		std::string cur;
		bool inQuotes = false;

		for (size_t i = 0; i < line.size(); ++i){
			char c = line[i];
			if (c == '"'){
				// escaped quote "" -> a single quote in the data
				if (inQuotes && i + 1 < line.size() && line[i + 1] == '"'){
					cur.push_back('"');
					++i;                     // skip the escaped quote
				}else{
					inQuotes = !inQuotes;   // toggle quoting mode
				}
			}else if (c == ',' && !inQuotes){
				fields.emplace_back(std::move(cur));
				cur.clear();
			}else{
				cur.push_back(c);
			}
		}
		fields.emplace_back(std::move(cur));
		return fields;
	}



	static inline std::vector<std::pair<double, std::string>> loadTimedRows(const std::string& file_path,const std::string& time_header){
		std::ifstream fin(file_path);
		if (!fin)
			throw std::runtime_error("Cannot open CSV file: " + file_path);

		std::string headerLine;
		if (!std::getline(fin, headerLine))
			throw std::runtime_error("CSV file empty: " + file_path);

		auto header_fields = splitCSVLine(headerLine);
		// Find indexof header for time field
		int time_index = -1;
		for (size_t i = 0; i < header_fields.size(); i++){
			if (header_fields[i] == time_header) {
				time_index = (int)i; 
				break; 
			}
		}
		if (time_index == -1){
			throw std::runtime_error("Time header '" + time_header + "' not found in file: " + file_path);
		}
		//sort lines first by time then by the string itself so it's deterministic
		std::map<double, std::multiset<std::string>> rows_map; // multiset allows duplicates
		std::string line;
		while (std::getline(fin, line)){		
			// split the line to extract the time column
			auto fields = splitCSVLine(line);
			const std::string& time_str = fields[time_index];
			double time = std::stod(time_str);
			rows_map[time].insert(line);
		}

		std::vector<std::pair<double, std::string>> out;
		out.reserve(rows_map.size());   // rough estimate grows as needed

		for (const auto& [time, rows_set] : rows_map){
			for (const auto& raw : rows_set){
				out.emplace_back(time, raw);
			}
		}
		return out;
	}

	// returns first index above the current_value
	static inline int firstAbove(std::vector<std::pair<double, std::string>>& log, double time){
		int i = 0 ;
		while(i < log.size() && log[i].first < time){
			i++;
		}
		return i ;
	}

	static inline void findDesync(const std::vector<std::string>& files, const std::string& time_header, double buffer_time){
		std::map<std::string, std::vector<std::pair<double, std::string>>> log ;
		std::map<std::string, int> start_index ;
		
		for(auto& file : files){
			log[file] = loadTimedRows(file, time_header) ;
			start_index[file] = firstAbove(log[file], 0) ;
		}

		// Findwhich log is starting latest
		double min_time = -FLT_MAX ;
		for(auto& [file,i] : start_index ){
			min_time = fmax(min_time,log[file][i].first) ;
		}
		min_time += buffer_time ;
		//jumpall logs to right after that time
		for (auto& [file, i] : start_index) {
			i = firstAbove(log[file],min_time) ;
		}
		
		int offset = 0 ;
		bool in_range = true ;
		for (auto& [file, i] : start_index) {
			in_range &= (i + offset) < log[file].size();
		}
		int desyncs = 0 ;
		while(in_range){
			std::string base = log[files[0]][start_index[files[0]] + offset].second ;
			for(int k=1;k<files.size();k++){
				std::string comp = log[files[k]][start_index[files[k]] + offset].second;
				if(comp != base){
					printf("Desync detected:\n  %s\n  %s\nLines:", base.c_str(), comp.c_str()) ;
					for (auto& [file, start] : start_index) {
						printf("  %s : %d\n", file.c_str(), start + offset + 2) ; // plus 2 is one for header and one because most spreadsheets start at 1 and not 0
					}
					//return ;
					desyncs++;
					if(desyncs>10){
						return ;
					}
				}
			}
			offset++;
			for (auto& [file, start] : start_index) {
				in_range &= start + offset < log[file].size();
			}
		}

		printf("No desync detected between files.\n"); 


	}

};

#endif // #ifndef _REGISTRY_H_