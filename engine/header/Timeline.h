#ifndef _Timeline_H_
#define _Timeline_H_ 1

#include "glm/glm.hpp"
#include "Registry.h"


#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <string>
#include <map>
#include <unordered_map>
#include <queue> 
#include <unordered_set>
#include <mutex>

class Timeline {

public:

	float max_info_speed = 1E4f; // in game units/second
	float min_event_duration = 0.0001f; // minimum amount of time between an event running and its data changes being readable
	float history_kept = 1.5f; // how much history is kept in seconds (usually overridden by default in worldplugin)
	float max_read_distance = FLT_MAX ; // The maximum distance an object can be read from
	static inline double vantage_warp_fraction = 0 ; // multiplier for warp in runtimes due to vantage position (needs to be 0 for servers but can be close to 1 for clients)
	static inline double object_move_fraction = 0.49 ; // multiplier of max_info_speed for object movement
	static inline double duration_step_fraction = 0.49f ; // multiplier of min_event_Duration for group step
	static inline Timeline* world = nullptr; // Timeline::world is used internally when running to reference the currently runnignworld's parameters
	static inline std::recursive_mutex world_lock; // static lock for world pointer if two timelines try to run together

	static inline long event_runs = 0;// used for analysis of timeline performance
	static inline long event_unruns = 0;

	//static inline bool PRINT_RUNNING = false; // prefer logger



	class WorldEvent {
	public:
		glm::vec3 dispatch_position = glm::vec3(0, 0, 0); // position of where this event was issued
		double dispatch_time = -1.0; //time the event was issued
		double target_run_time = -1.0; // Time the event wants to be run (from its perspective atthe object)
		double actual_run_time = -1.0; // Time event would actually run at the object, changes with calls to getRunTime until event runs
		glm::vec3 actual_run_position = glm::vec3(0, 0, 0); // position of event at actual_run_time, changes with calls to getRunTime until event runs
		double write_time = -1.0; //Time the data change of this event becomes available (not set until after the event has been run)
		std::shared_ptr<WorldEvent> parent; // if spawned by another event then track that
		int64_t object_id = -1; // Object this event will be run on
		int rollbacks = 0;


		virtual ~WorldEvent() = default; // Force to be polymorphic

		virtual void run(std::shared_ptr<WorldEvent> this_event) = 0;

		//get time this event would run from the vantage point
		virtual double getRunTime(Timeline* timeline, const glm::vec3& vantage) = 0;

		virtual void print() = 0;

	};

	//Base class used for all objects that can exist in the timeline
	class WorldObject {
	public:
		glm::vec3 position = glm::vec3(0, 0, 0);
		int64_t id = -1; // Id of object, written by timeline, can be read in events but not modified
		double time = -1.0; // Current time at object, written by timeline, can be read in events but not modified
		glm::vec3 event_position = glm::vec3(0, 0, 0); // Position event was executed at, can be read in events but not modified
		std::shared_ptr<WorldEvent> writing_event; // for internal use to let spawned events know their parent
		bool destroyed = false;// set to true to "delete" an object, this will cause attempted reads aftyerward to return nullptr and event executions to fail
		int64_t random_seed = 0 ;

		WorldObject() = default;
		WorldObject(const glm::vec3& position) : position(position) {};

		virtual ~WorldObject() = default; // Force to be polymorphic

		// returns the type id of the object in the given registry
		virtual int getTypeId(Registry* r) const = 0 ;

		//Read another object in the timeline, speed of info will be enforced
		//Returns nullptr if the object doesn't exist or isn't yet readable
		std::shared_ptr<const WorldObject> read(int64_t read_id) const;


		template<typename T>
		std::shared_ptr<const T> read(int64_t read_id) const {
			std::shared_ptr<const T> result = dynamic_pointer_cast<const T>(read(read_id));
			return result;
		}

		//Queue an event to run a function, speed of info will be enforced
		template<typename... Args>
		void inline queue(int64_t obj_id, double target_time, int func_id, const Args&... args) {
			std::shared_ptr<VoidEvent> event = std::make_shared<VoidEvent>(obj_id, func_id, time, serialize(args...));
			event->dispatch_position = event_position;
			event->dispatch_time = time + world->min_event_duration;
			event->parent = writing_event;

			//printf("Queueing void event with params : %d at time %f at (%f,%f,%f)\n", func_id, time, event_position.x, event_position.y, event_position.z);
			//internally queued events should never cause rollback, so just add it to the queue
			world->pending_events.insert(event);
			world->new_events.insert(event);
		}

		template<>
		void inline queue(int64_t obj_id, double target_time, int func_id){
			std::shared_ptr<VoidEvent> event = std::make_shared<VoidEvent>(obj_id, func_id, target_time, std::vector<char>());
			event->dispatch_position = event_position;//if called on a read object dispatch position should be event object then this could be const
			event->dispatch_time = time + world->min_event_duration;
			event->parent = writing_event;

			//internally queued events should never cause rollback, so just add it to the queue
			world->pending_events.insert(event);
			world->new_events.insert(event);
		}

		//Queue an event to run a function, speed of info will be enforced
		template <typename T, typename Ret, typename... Args>
		void inline queue(int64_t obj_id, double target_time, Ret(T::* method)(const Args&...), const Args&... args){
			int method_id = world->registry->getIdForMethod(method);
			queue(obj_id, target_time, method_id, args...);
		}

		template <typename T, typename Ret>
		void inline queue(int64_t obj_id, double target_time, Ret(T::* method)()) {
			int method_id = world->registry->getIdForMethod(method);
			queue(obj_id, target_time, method_id);
		}

		//Queue an event to create an object in the timeline, speed of info will be enforced
		//Returns the automatically generated unique id the object will have when it has been created
		int64_t create(std::shared_ptr<WorldObject> new_object, double target_time);

		//produce a seeded random number between 0 and 1 that can be used safely in timeline events
		float random(){ //TODO make const and use random_seed on event object, not this can be used on read object functions
			if(random_seed == 0){
				random_seed = hashBytes(serialize(position, time,id));
			}else{
				random_seed = hashBytes(serialize(random_seed));
			}
			return ((random_seed & 0xffffff) ^ ((random_seed >> 24) & 0xffffff)) / (float)0xffffff;
		}

		virtual void print() const = 0;
	};


	static inline char VOID_EVENT = 1;

	// Runs a void function an object as an event
	class VoidEvent : public WorldEvent {
	public:
		int method_id = -1; // which function will be called
		std::vector<char> args; // Serialized arguments for the method

		void run(std::shared_ptr<WorldEvent> this_event) override;

		VoidEvent(int64_t obj_id, int func_id, double time, const std::vector<char>& func_args) {
			object_id = obj_id;
			method_id = func_id;
			args = func_args;
			target_run_time = time;
		}

		double getRunTime(Timeline* timeline, const glm::vec3& vantage) override;

		void print();

		~VoidEvent() = default;
	};


	static inline char CREATE_EVENT = 2;
	//An event that creates an object
	class CreateEvent : public WorldEvent {
	public:
		// ID the object will have needs to be reserved in advance to prevent conflicts
		std::shared_ptr<WorldObject> new_object;

		void run(std::shared_ptr<WorldEvent> this_event) override;

		CreateEvent(int64_t reserved, std::shared_ptr<WorldObject> obj, double time) {
			object_id = reserved;
			new_object = obj;
			target_run_time = time;
		}

		double getRunTime(Timeline* timeline, const glm::vec3& vantage) override;
		void print();

		~CreateEvent() = default;
	};

	class ObjectHistory {

	public:

		ObjectHistory() {
			throw std::runtime_error("Object history is being cvreated empty!");
		}

		ObjectHistory(std::shared_ptr<WorldObject> first_instant) {
			addInstant(first_instant);
		}

		// Returns the most recent version of the object that can be read from the given vantage point obeying max_info_speed and max_read_distance
		std::shared_ptr<const WorldObject> read(const glm::vec3& vantage, const double time);

		// Returns the most recent version of the object that can be read from the given vantage point obeying max_info_speed but not obeying max read distance
		std::shared_ptr<const WorldObject> readFar(const glm::vec3& vantage, const double time);

		// Returns the state of this object at the given time (used for base state where time warp is not used)
		std::shared_ptr<WorldObject> getStateAt(const double time);

		//Returns all states of this history of this object in the given time range
		//will be ordered from newest to oldest
		std::vector<std::shared_ptr<WorldObject>> getStateRange(const double start_time, const double end_time);

		// returns the time and value of the latest instance of this object
		std::shared_ptr<WorldObject> getLatest();

		// removes all but one element of the history before the given base_time
		void cleanHistory(double base_time);

		// Removes all instants after the given time
		void deleteAfter(double base_time);

		void addInstant(std::shared_ptr<WorldObject>& instant);


		std::map<double, std::shared_ptr<WorldObject>> history; // maps time to a state change of an object
		std::shared_ptr<WorldObject> latest;

	};

	double last_vantage_time = 0; // in seconds since beginning of scenario
	glm::vec3 last_vantage = glm::vec3(0, 0, 0);
	double last_clean_time = -1.0;

	std::unordered_map<int64_t, ObjectHistory> objects; // All objects currently in the timeline and their history
	std::unordered_set<std::shared_ptr<WorldEvent>> pending_events; // Events pending run in no particular order
	std::unordered_set<std::shared_ptr<WorldEvent>> event_history; // Events that have been executed but could be rolled back
	std::vector<std::shared_ptr<WorldEvent>> external_events; // Events injected from outside the timeline that need to be included in network updates
	std::unordered_set<std::shared_ptr<WorldEvent>> new_events; //Events created by the last run event
	std::shared_ptr<Registry> registry; // Registry of objects and functions that can be serialized
	
	std::vector<std::pair<glm::vec3, double>> pending_rollbacks ;

	Timeline(std::shared_ptr<Registry> r, float info_speed, float min_event_duration, float max_read_distance, float history_kept) :
		registry(r), max_info_speed(info_speed), min_event_duration(min_event_duration),max_read_distance(max_read_distance), history_kept(history_kept) {
		if (COPY_PACKET == -1) {
			COPY_PACKET = registry->registerClass<CopyPacket>("CopyPacket");
			UPDATE_PACKET = registry->registerClass<UpdatePacket>("UpdatePacket");
		}

	}

	// Used to insert an event from outside the timeline
	// Warning: Queueing an event in the past may immediately trigger rollback, but you'll need to run events to get back to current time
	void queue(const glm::vec3& vantage, double vantage_time, std::shared_ptr<WorldEvent> event);

	// Used to insert an event thatcome from an update
	// Warning: Queueing an event in the past may immediately trigger rollback, but you'll need to run events to get back to current time
	void internalQueue(const glm::vec3& vantage, double vantage_time, std::shared_ptr<WorldEvent> event);


	// Alternate forms of queue that construct void event objects for you
	template <typename... Args>
	void inline queue(const glm::vec3& vantage, double vantage_time, int64_t obj_id, double target_time, int method_id, const Args&... args) {
		std::shared_ptr<VoidEvent> event = std::make_shared<VoidEvent>(obj_id, method_id, target_time, serialize(args...));
		queue(vantage, vantage_time, event);
	}

	template <>
	void inline queue(const glm::vec3& vantage, double vantage_time, int64_t obj_id, double target_time, int method_id) {
		std::shared_ptr<VoidEvent> event = std::make_shared<VoidEvent>(obj_id, method_id, target_time, std::vector<char>());
		queue(vantage, vantage_time, event);
	}

	template <typename T, typename Ret, typename... Args>
	void inline queue(const glm::vec3& vantage, double vantage_time, int64_t obj_id, double target_time, Ret(T::* method)(const Args&...), const Args&... args) {
		int method_id = registry->getIdForMethod(method);
		auto serial = serialize(args...);
		std::shared_ptr<VoidEvent> event = std::make_shared<VoidEvent>(obj_id, method_id, target_time, serial);
		queue(vantage, vantage_time, event);
	}

	template <typename T, typename Ret>
	void inline queue(const glm::vec3& vantage, double vantage_time, int64_t obj_id, double target_time, Ret(T::* method)()) {
		int method_id = registry->getIdForMethod(method);
		std::shared_ptr<VoidEvent> event = std::make_shared<VoidEvent>(obj_id, method_id, target_time, std::vector<char>());
		queue(vantage, vantage_time, event);
	}

	// Construct and queue a create event from outside the timeline
	// Warning: Queueing an event in the past may immediately trigger rollback, but you'll need to run events to get back to current time
	int64_t create(const glm::vec3& vantage, double vantage_time, std::shared_ptr<WorldObject> new_object, double target_time);

	// Runs an event that should be in pending_events and moves it to event_history
	void runEvent(std::shared_ptr<WorldEvent>);

	//Runs all events that could run before the given vantage
	// Note: vantage cannot be a reference because other threads change it and the underlying algorithms here require it be constant
	void run(const glm::vec3 vantage, double vantage_time);

	// Runs the next event that can run from the given vantage point if there is one
	// Returns whether an event was run
	// good for debugging but innefficient, consider using other run methods for general application
	bool runNext(const glm::vec3& vantage, double vantage_time);

	//implementation of run that runs events one at a time for simpler debugging
	void runSorted(const glm::vec3& vantage, double vantage_time);

	// Runs the next batch of events that can't possible depend on each other
	bool runBatch(const glm::vec3& vantage, double vantage_time);

	// Runs batches until done
	void runBatched(const glm::vec3& vantage, double vantage_time);

	
	//Run pending events via heap
	void runHeap(const glm::vec3& vantage, double vantage_time);

	//Helper for runHeap
	// Given a vector of events on the same object
	// moves the earliest running event to the end and returns its run time
	double moveEarliestLast(std::vector<std::shared_ptr<WorldEvent>>& events, const glm::vec3& vantage);

	//returns whether an event could effect another event
	bool couldEffect(const std::shared_ptr<WorldEvent>& cause, const std::shared_ptr<WorldEvent>& effect);

	//rolls back all events and object changes that have occured withing the light cone of the trigger
	void rollback(const glm::vec3& trigger_position, double trigger_time);

	void applyPendingRollbacks();

	//read an object from the timeline at a given vantage point and time obeying maxinfo speed and max distance
	std::shared_ptr<const WorldObject> read(int64_t object_id, const glm::vec3& vantage, double vantage_time);

	//read an object from the timeline at a given vantage point and time obeying max info speed but not max distance
	std::shared_ptr<const WorldObject> readFar(int64_t object_id, const glm::vec3& vantage, double vantage_time);

	//Returns all readable entities from the given vantage
	std::vector<std::shared_ptr<const WorldObject>> observe(const glm::vec3& vantage, double vantage_time);

	// Returns the value of all objects at the given time (i.e. the latest instance before the time)
	std::map<int64_t, std::shared_ptr<WorldObject>> getBaseObjects(double time);

	//serializes a WorldObject with all of the variables needed to recreate it on another tineline
	//which is more than the dervied objects will serialize
	std::vector<char> serializeWorldObject(std::shared_ptr<WorldObject> object);

	//serializes a WorldEvent with all of the variables needed to recreate it on another timeline
	std::pair<char, std::vector<char>> serializeWorldEvent(std::shared_ptr<WorldEvent> event);

	//applies an update given by getupdateFor
	//First element is objects, second is event, events also have types
	void applyUpdate(const std::pair<std::vector<std::vector<char>>, std::vector<std::pair<char, std::vector<char>>>>& update);


	// logs an injected event that hasn't run yet with its dispatch data 
	void logInjectedEvent(std::shared_ptr<WorldEvent> event) ;

	//Print a void event to the console
	void printVoidEvent(VoidEvent* event);
	
	static inline double preciseDistance(const glm::vec3& a, const glm::vec3& b) {
		double dx = (double)a.x - (double)b.x;
		double dy = (double)a.y - (double)b.y;
		double dz = (double)a.z - (double)b.z;
		return sqrt(dx * dx + dy * dy + dz * dz);

	}

	static inline float impreciseDistance(const glm::vec3& a, const glm::vec3& b) {
		float dx = a.x - b.x;
		float dy = a.y - b.y;
		float  dz = a.z - b.z;
		return sqrtf(dx * dx + dy * dy + dz * dz);

	}


	static inline int COPY_PACKET = -1 ;
	// A Packet containing all the information to duplicate the state of a timeline
	struct CopyPacket{
		float max_info_speed = -1.0f;
		float min_event_duration = -1.0f;
		float max_read_distance = -1.0f;
		float history_kept = -1.0f;
		double last_vantage_time = -1.0f;
		std::string version ;
		glm::vec3 last_vantage;

		std::vector<std::vector<char>> objects ;
		std::vector<std::pair<char, std::vector<char>>> pending_events ;
		std::vector<std::pair<char, std::vector<char>>> event_history ;

		// include a generic place for extension data
		std::vector<double> extra_double_data ;
		std::vector<std::string> extra_string_data ;
		

		void print() const {
			printf("Copy packet: time = %lf  objects = %d  pending_events = %d, history = %d\n", last_vantage_time, (int)objects.size(), (int)pending_events.size(), (int)event_history.size()) ;
		}
	};

	Timeline(std::shared_ptr<Registry>& r, CopyPacket& packet) ;


	// Returns a seralized update to duplicate the currently active state of this timeline
	// Copies everything after earliest_time
	CopyPacket copy(double earliest_time);


	static inline int UPDATE_PACKET = -1;
	// A Packet containing all the information to duplicate the state of a timeline
	struct UpdatePacket {
		//Sends of hash of objects at a specific time for checking sync failure
		double hash_time = 1.0 ;
		int64_t object_hash = -1 ;
		double last_run_time;
		// events that were externally queued and need to be injected
		std::vector<std::pair<char, std::vector<char>>> external_events;

		// include a generic place for extension data
		std::vector<double> extra_double_data;
		std::vector<std::string> extra_string_data;
	};

	//Applies an update packet 
	//returns if still in sync
	bool applyUpdate(UpdatePacket& packet) ;

	//Returns an update with all external events since the last time this was called
	UpdatePacket getPendingUpdate();


	//Returns the hash of all objects at the given instant 
	//for consistency time should be far enough in the past to negate time warp
	int64_t computeObjectHash(double time) ;

	//log to the extended log every event in an update packet
	void logUpdate(UpdatePacket& packet, const std::string& log_action);
};


auto static getStructure(Timeline::CopyPacket& obj) {
	return std::tie(obj.max_info_speed,obj.min_event_duration, obj.max_read_distance, obj.history_kept,obj.last_vantage_time, obj.last_vantage, obj.objects, obj.pending_events, obj.event_history, obj.extra_double_data, obj.extra_string_data, obj.version);
}
auto static getStructure(Timeline::UpdatePacket& obj) {
	return std::tie(obj.hash_time, obj.object_hash, obj.last_run_time, obj.external_events, obj.extra_double_data, obj.extra_string_data);
}

//Alias top-level idea so they can be referenced without mention of Timeline
using WorldObject = Timeline::WorldObject;
using WorldEvent = Timeline::WorldEvent;
using CreateEvent = Timeline::CreateEvent;
using VoidEvent = Timeline::VoidEvent;

#endif // #ifndef _TIMELINE_H_