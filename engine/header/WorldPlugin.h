#ifndef _WORLD_PLUGIN_H_
#define _WORLD_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "Variant.h"
#include "Utilities.h"

#include "Timeline.h"
#include "Sockets.h"
#include "CSVLog.h"

#include <stdio.h>
#include <cstdlib>

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <unordered_map>

class WorldPlugin : public AsyncPlugin, public PacketReceiver {

public:

	static inline std::string tag = "WorldLink";
	static inline int micros_before_considered_disconnected = 2000000; //microseconds

	static inline float history_kept = 1.5f;
	static inline float clock_adjust_rate = 0.05f ;
	static inline float client_vantage_warp_fraction = 0.9f;
	static inline float server_vantage_warp_fraction = 0.0f;
	static inline float extra_sync_depth = 0.5f ;

	//Observations look ahead by a tiny amount in seconds so player actions are visible the frame they are on
	static inline double observation_look_ahead = 1.0/240.0 ;

	static const inline int READY_TO_RUN_TIMELINE = 57239654 ;
	static const inline int OBSERVABLES_READY = 48548251;
	static const inline int PACKETS_SENT = 68461548;
	

	//Location of world data in extended attributes of timeline packets
	static const inline int NAME_EXTRA_INDEX = 0;
	
	static const inline int LAST_RECEIVE_TIME_EXTRA_INDEX = 0;
	static const inline int TIME_SPEED_EXTRA_INDEX = 1;


	static inline std::shared_ptr<CSVLog> log = nullptr;
	static inline std::shared_ptr<CSVLog> extended_log = nullptr;
	enum LogType {
		DISABLED,
		FINAL_EVENTS, // logs event when they are cleaned up,shows final eentual event calls only
		RUNNING_EVENTS, // logs event whren they run, will miltilog with rollback
		INJECTED_EVENTS, // logs events when they are entered into the timeline, no internally generated events or rollback
		INJECTED_EXTENDED // injected events but also fills the extended logwith breadcrumbs for tracking event flow around injection
	};
	static inline LogType log_type = DISABLED;

	struct World{
		std::string name ;
		std::shared_ptr<Timeline> timeline ;
		float time_speed = 0 ; // multiplier on the rate of time in this world
		glm::vec3 vantage_point = glm::vec3(0,0,0) ;
		double current_time  = 0 ;		
		std::chrono::high_resolution_clock::time_point system_time_of_current_time ;
		float max_read_distance = 1E3;
		float max_info_speed = 1E4;

		std::vector<std::shared_ptr<Timeline::WorldEvent>> pending_local_events; //events to be added to the timeline on next run
		double next_event_time = 0;// Used when time isn't specified for externally injected events

	};

	struct Connection{
		//int socket_id = -1;//id on the socket for sending data to this client
		//std::string address ;
		std::chrono::high_resolution_clock::time_point last_packet_received_time = now() ;
		std::vector<std::shared_ptr<std::vector<char>>> update_packet_queue ; // where updates go when they are rcieved

		std::vector<std::shared_ptr<std::vector<char>>> update_packets ; //whjere updates sit while theyare being processed ;

		bool ready = false;
		bool disconnected = false;
		bool wants_full_sync = false; // will be set to true if client sent a packet requesting a full sync
		bool mismatched_version = false; // will be set to true if a client with a mismatched version attempts to connect
		std::map<std::string, double> last_absolute_time  ;// the latest absolute time of a received packet, used to estimate ping and sync clocks
	};

	WorldPlugin();

	~WorldPlugin();

	// Called on every plug-in before any plug-ins are run
	void initialize() override;

	//Runs the plugin on it's own thread at the time as all others
	void run() override;

	//Runs a world forward by the given amount of time
	void run(const std::string& world_name, double dt);


	//Creates an empty world with the given properties, starts paused at time 0
	// Returns if successful (may fail if world_name is taken already or parameters are invalid
	bool createWorld(const std::string& world_name, float info_speed, float min_event_duration, float max_read_distance);

	// Sets the local viewing position within the given world
	void setVantagePoint(const std::string& world_name, const glm::vec3& vantage) ;

	//Sets the rate of the passage of time in a world 
	//0 is paused, 1.0 is real-time, negative numbers are not permitted
	void setTimeSpeed(const std::string& world_name, float speed);

	//Returns the current time in the given world at the vantage point
	double getWorldTime(const std::string& world_name) ;

	//Returns the current time in the given world at the given point
	double getWorldTime(const std::string& world_name, const glm::vec3& point);

	//Returns a list of the names of currently active worlds running locally
	std::vector<std::string> getLocalWorldList();

	// Saves a world to a raw chunk of data, which can be loaded later
	Variant saveWorld(const std::string& world_name);

	// Load a world from a chunk of saved data
	bool loadWorld(const std::string& world_name, Variant& world_data);

	// Removes a local world, deleting it's local contents
	void removeWorld(const std::string& local_world);

	// Returns the state of all objects observed on the given world from the current time and local vantage point
	std::vector<std::shared_ptr<const WorldObject>> observe(const std::string& local_world);

	// Observe a specific object whose type you know
	template<typename T>
	std::shared_ptr<const T> observe(const std::string& local_world, int64_t id){
		lock.lock();
		auto iter = worlds.find(local_world);
		if (iter != worlds.end()) {
			for (auto& obj : observation_buffer[local_world]) {
				if (obj->id == id) {
					std::shared_ptr<const T> result = dynamic_pointer_cast<const T>(obj) ;
					lock.unlock();
					return result ;
				}
			}
		}
		lock.unlock();
		return nullptr;
	}

	template<typename T>
	std::shared_ptr<const T> observeNearest(const std::string& local_world) {
		std::shared_ptr<const T> closest;
		std::vector<std::shared_ptr<const WorldObject>> objects = observe(local_world);
		if (objects.size() == 0) {
			return closest;
		}
		float closest_distance = FLT_MAX;
		glm::vec3 vantage = worlds[local_world].vantage_point;
		for (std::shared_ptr<const WorldObject>& obj : objects) {
			std::shared_ptr<const T> inst = dynamic_pointer_cast<const T>(obj);
			if (inst) {
				float distance = glm::distance2(inst->position, vantage);
				if (distance < closest_distance) {
					distance = closest_distance;
					closest = inst;
				}
			}
		}
		return closest;
	}

	//Calls created, updated and destroyed on all views as required with the latest observations
	void view();

	//Calls update on any views currently active
	void viewUpdate();

	//Creates and destroys views and calls the appropriate functions on them to amke views match current observatrions
	void viewCreateDestroy(const std::string& local_world);

	template<typename V> 
	std::shared_ptr<V> getView(const std::string& local_world, int64_t id) {
		if (views.find(id) == views.end()) { // Observation has no view
			return nullptr ;
		}else {
			return dynamic_pointer_cast<V>(views[id]);
		}
	}

	template<typename V>
	std::shared_ptr<V> getView(const std::shared_ptr<const WorldObject>& observation) {
		if (!observation) {
			return nullptr;
		} else {
			return dynamic_pointer_cast<V>(views[observation->id]);
		}
	}

	// Start hosting your worlds on the given port
	// returns if probably successful (can't know for sure)
	bool host(int port, const std::string& version);

	//Start hosting with a socket that has already been created (like from steam)
	bool host(std::shared_ptr<Socket>& new_socket, const std::string& version) ;

	bool amHosting(){
		return hosting ;
	}

	//Create a connection using a preexisting socket (like from Steam)
	void connect(std::shared_ptr<Socket>& new_socket, const std::string& version);
	
	//request a connection to a remote World server (nonblocking)
	void connect(const std::string& address, int port, const std::string& version);

	//request a connection to a remote world server (blocking)
	void runConnect(const std::string& address, int port, const std::string& version) ;

	// For the client: Disconnects from a remote host
	// Worlds will continue to run, but will stop synchronizing
	void disconnect();

	//Deletes all worlds
	// Not a good idea when connected, but might be useful when restarting a server or switching a client to a server 
	// World are auto-cleared when connecting but not when disconnecting or hosting
	void clearWorlds();

	void actuallyClearWorlds();

	// For the client: check if requestConnect has succeeded, but also reports if the host has disconnected
	bool connected();

	// For the server: Check if a specific remote connection is still active
	bool connected(int id);

	//Called when a packet is received
	void receivePacket(int sender_id, const std::vector<char>& data) override;

	//Called when a connection is established, 
	//sender_id is generated by the socket and can be used to identify recieved packet sources or send data back through the socket
	void onSocketConnect(int sender_id) override;

	//Called when a connection is closed, either remotely or because the socket holding it was closed
	void onSocketClose(int sender_id) override;

	//returns the ping in seconds
	double getPing();

	//returns the time step of the last frame in seconds
	float getTimeStep();

	static void enableEventLogging(const std::string& file, LogType type){
		log = std::shared_ptr<CSVLog>(new CSVLog(file, "action","object", "time", "x","y","z"));
		log_type = type ;
	}

	static void enableEventLogging(const std::string& file, const std::string& extended_file, LogType type) {
		log = std::shared_ptr<CSVLog>(new CSVLog(file, "action", "object", "time", "x", "y", "z"));
		extended_log = std::shared_ptr<CSVLog>(new CSVLog(extended_file, "action", "object", "time", "x", "y", "z"));
		log_type = type;
	}

	// queue a void event directly via method pointer
	template <typename T, typename Ret, typename... Args>
	void inline queue(const std::string& world_name, int64_t obj_id, double target_time, Ret(T::* method)(const Args&...), const Args&... args) {
		lock.lock();
		auto iter = worlds.find(world_name);
		if (iter != worlds.end()) {
			World& world = iter->second;
			int method_id = registry->getIdForMethod(method);
			auto serial = serialize(args...);
			std::shared_ptr<Timeline::VoidEvent> event = std::make_shared<Timeline::VoidEvent>(obj_id, method_id, target_time, serial);
			world.pending_local_events.push_back(event);
		}else {
			printf("Got an event queued for a world that wasn't found: %s\n", world_name.c_str());
		}
		lock.unlock();
	}

	// queue a void event directly via method pointer
	// Same as above but target time is defaulted to now
	template <typename T, typename Ret, typename... Args>
	void inline queue(const std::string& world_name, int64_t obj_id, Ret(T::* method)(const Args&...), const Args&... args) {
		auto iter = worlds.find(world_name);
		if (iter != worlds.end()) {
			World& world = iter->second;
			if(world.timeline == nullptr){
				printf("queueing event on world which exists without a timeline : world = %s, object = %I64d? \n", world_name.c_str(), obj_id);
			}else{
				world.next_event_time += world.timeline->min_event_duration * 0.01;
				queue(world_name, obj_id, world.next_event_time, method, args...);
			}
		}else {
			printf("Got an event queued for a world that wasn't found: %s\n", world_name.c_str());
		}
	}

	// Construct and queue a create event from outside the timeline
	int64_t create(const std::string& world_name, std::shared_ptr<WorldObject> new_object, double target_time){
		lock.lock();
		auto iter = worlds.find(world_name);
		if (iter != worlds.end()) {
			World& world = iter->second;
			int type_id = new_object->getTypeId(registry.get());
			auto d = registry->serializeObj(type_id, new_object.get());
			int64_t reserved_id = hashBytes(d) ^ hashRaw(type_id ^ (*(int64_t*)&target_time));
			std::shared_ptr<Timeline::CreateEvent> event = std::make_shared<Timeline::CreateEvent>(reserved_id, new_object, target_time);
			world.pending_local_events.push_back(event);
			lock.unlock();
			return reserved_id;
		}else {
			printf("Got a creation queued for a world that wasn't found: %s\n", world_name.c_str());
		}
		lock.unlock();
		return -1 ;
	}

	// Construct and queue a create event from outside the timeline
	// Same as above but t5arget time is defaulted to now
	int64_t create(const std::string& world_name, std::shared_ptr<WorldObject> new_object) {
		auto iter = worlds.find(world_name);
		if (iter != worlds.end()) {
			World& world = iter->second;
			world.next_event_time += world.timeline->min_event_duration * 0.01;
			return create(world_name, new_object,world.next_event_time) ;
		}
		throw std::runtime_error("Attempting to create an object in a world that doesn't exist!");
		return -1 ;
	}

	//Passthrough to registry to allow registering classes directly on the world plugin
	template<typename T>
	inline int registerClass(const std::string& debug_name){
		return registry->registerClass<T>(debug_name) ;
	}


	//Passthrough to registry to allow registering classes directly on the world plugin
	//This takes a second template for a view class that can be instantiated fro man object fo the base class
	template<typename T, typename V>
	inline void registerClass(const std::string& debug_name) {
		registry->registerClass<T, V>(debug_name);
	}

	//Passthrough method to registry to allow registering methods directly on the world plugin
	template <typename T, typename Ret, typename... Args>
	inline void registerMethod(Ret(T::* method)(Args...), const std::string& debug_name) {
		registry->registerMethod(method, debug_name) ;
	}

private:

	std::shared_ptr<Registry> registry = std::make_shared<Registry>();// classes and functions to be used in a world
	int WORLD_PACKET_ID = -1 ; // registry id o th world packet type
	bool hosting = false ; // whether this plugin is currently hosting
	std::shared_ptr<Socket> socket; // could be a UDPClientSocket or a UDPServerSocket
	std::unordered_map<std::string, World> worlds ;// active worlds
	std::unordered_map<int, Connection> connections; // active network connections
	std::chrono::high_resolution_clock::time_point last_run_time = now();

	
	std::vector<std::shared_ptr<Timeline::CopyPacket>> copy_packets ; //if a world is being copied from another world, then that packet waits here until processed
	std::map <std::string, std::vector<std::shared_ptr<const WorldObject>>> observation_buffer ;

	std::recursive_mutex packet_lock ; // lock specifically for processing packets since the main lock is used for cross plugin access

	std::string version = "none" ;
	bool wait_enabled = false;

	int simulated_lag_micros = 0; // 0 disables fake lag path entirely
	int simulated_jitter = 0 ;
	std::shared_ptr<SlowPacketReceiver> fake_lag ;
	double absolute_time = 0; // absolute time in seconds since this plugin started_running
	double ping = 0 ; // most recent estimate of ping
	float dt = 0 ; // last time step of the world plugin
	bool clear_worlds = false;
	
	std::thread connecting_thread ;
	bool connection_pending = false;

	
	std::unordered_map<int64_t, std::shared_ptr<BaseObjectView>> views ; // TODO separate views by world to prevent possible collision
	
};


#endif // #ifndef _WORLD_PLUGIN_H_
