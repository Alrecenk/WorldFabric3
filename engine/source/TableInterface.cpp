#include "TableInterface.h"

using std::map;
using std::unordered_map;
using std::vector;
using std::unique_ptr;
using std::string;

// Maps from string key to a list of Readers that requested that key
map<string, vector<TableInterface*>> TableInterface::pending_requests;
//All pending writes in order of added as Variants with string key and Variant value
std::vector<Variant> TableInterface::pending_writes ;
//Maps a key to a cached Variant
unordered_map<string, Variant> TableInterface::cache;
// allows to return a reference to a null
Variant TableInterface::null_variant =  Variant() ;

// Used for wite packets to the server
const std::string TableInterface::KEY = "k";
const std::string TableInterface::VALUE= "v";

// virtual destructor makes sure the right destructor is called on child classes
TableInterface::~TableInterface() {
    // Make sure element has no pending requests with a pointer to it
    if (pending != 0) {
        for (const auto&[key, value]: TableInterface::pending_requests) {
            vector<TableInterface*> &requesters = TableInterface::pending_requests[key];
            for (int k = 0; k < requesters.size(); k++) {
                if (requesters[k] == this) {
                    requesters.erase(requesters.begin() + k);
                    break;
                }
            }
        }

    }
}

// Call this function to attempt to imemdiately fetch table data that has been fetched before from the cache
// Returns nullptr if the data is not in the cache, and then you'll need to use the async method
const Variant& TableInterface::getCachedTableData(string key) {
    // in the cache
    if (TableInterface::cache.find(key) != TableInterface::cache.end()) {
        return TableInterface::cache[key];
    } else {
        return null_variant;
    }
}

// Remove an item from the cache, allowing an updated version to be requested from the server
void TableInterface::uncache(std::string key){
    if (TableInterface::cache.find(key) != TableInterface::cache.end()){
        TableInterface::cache.erase(key);
    }
}

// Call this function to asynchronously request data from the remote table
// recieveTableData will be called when the data is returned
void TableInterface::requestTableData(string key) {
    pending++;
    TableInterface::pending_requests[key].push_back(this);
}

// Call to write data to the remote table
// Note: ovrwrites any existing data. Every class and client are writing to the same table, tread carefully
void TableInterface::writeTableData(const std::string& key, const Variant& value){
    map<string,Variant> write_map;
    write_map[TableInterface::KEY] = Variant(key);
    write_map[TableInterface::VALUE] = value.clone();
    pending_writes.emplace_back(write_map); // construct in vector to avoid another copy
}

// Returns a list of all pending requested keys in no particular order
// Used by the table socket client to batch and send requests to the server
vector<string> TableInterface::getRequestedKeys() {
    vector<string> keys;
    keys.reserve(TableInterface::pending_requests.size());
    for (const auto &pair : TableInterface::pending_requests) {
        keys.push_back(pair.first);
    }
    return keys;
}

// Called by the table socket client when a batch of data comes back from the server
// Distributes the data to all the objects that made requests
void TableInterface::receiveTableData(const map<string, Variant> &data) {
    // Pass the data to requesters
    for (const auto&[key, value]: data) {
        TableInterface::cache[key] = value.clone() ; // TODO could a pointer move avoid this copy if input data is allowed to be destroyed?
        serveItem(key);
    }
}

//Serves an item to its requesters
void TableInterface::serveItem(std::string key){
    // Serve item to requesters
    vector<TableInterface*> &requesters = TableInterface::pending_requests[key];
    for (auto &requester : requesters) {
        requester->pending--;
        requester->receiveTableData(key, TableInterface::cache[key]);
    }
    // Remove all the served requests from pending
    TableInterface::pending_requests.erase(key);
}

// resolves all pending requests whose data is already in the cache
void TableInterface::serveCachedRequests() {
    vector<string> served_keys;
    for (const auto &[key, requesters] : TableInterface::pending_requests) {
        // in the cache
        if (TableInterface::cache.find(key) != TableInterface::cache.end()){
                if(TableInterface::cache[key].defined()){ // Don't serve rerequests of nulls from cache
                    for (auto &requester : requesters) {
                        requester->pending--;
                        requester->receiveTableData(key, TableInterface::cache[key]);
                    }
                    // can't remove keys while iterating the same map
                    served_keys.push_back(key);
                }
        }
    }
    // Remove all the served requests from pending requests
    for (const auto &key: served_keys) {
        TableInterface::pending_requests.erase(key);
    }
}

//Clears all the items in the cache
void TableInterface::clearCache(){
    cache.clear();
}