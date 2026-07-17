#include "Timeline.h"
#include "WorldPlugin.h"
#include <queue>


using WorldObject = Timeline::WorldObject;
using WorldEvent = Timeline::WorldEvent;
using CreateEvent = Timeline::CreateEvent;
using VoidEvent = Timeline::VoidEvent;


//Read another object in the timeline, speed of info will be enforced
//Returns nullptr if the object doesn't exist or isn't yet readable
std::shared_ptr<const WorldObject> Timeline::WorldObject::read(int64_t read_id) const {
	return world->read(read_id, event_position, time);
}

//Queue an event to create an object in the timeline at the given time, speed of info will be enforced (so it may not be made at that time)
//Returns the automatically generated unique id the object will have when it has been created
int64_t Timeline::WorldObject::create(std::shared_ptr<WorldObject> new_object, double target_time) {
	world_lock.lock();
	// hash object with time to get a unique id
	int type_id = new_object->getTypeId(world->registry.get());
	std::vector<char> d = world->registry->serializeObj(type_id, new_object.get());
	int64_t reserved_id = hashBytes(d) ^ hashRaw(id ^ type_id ^ (*(int64_t*)&target_time));
	//printf("internal reserved: %lld  from %lld , %lld\n", reserved_id, hashBytes(d), hashRaw(id ^ type_id ^ (*(int64_t*)&target_time)) ) ;

	std::shared_ptr<CreateEvent> event = std::make_shared<CreateEvent>(reserved_id, new_object, target_time);
	event->dispatch_position = event_position;
	event->dispatch_time = time + world->min_event_duration;
	event->parent = writing_event;
	world->pending_events.insert(event);
	world->new_events.insert(event);
	world_lock.unlock();
	return reserved_id;
}

std::shared_ptr<const WorldObject> Timeline::ObjectHistory::read(const glm::vec3& vantage, const double time) {
	if (history.empty()) {
		throw std::runtime_error("Object history is empty on read.");
	}
	auto it = history.rbegin();
	double distance = preciseDistance((it->second)->position, vantage) ;
	double readable_time = it->first + distance / world->max_info_speed;
	while (readable_time >= time) {
		it++;
		if (it == history.rend()) {
			return nullptr; // earliest element wasn't readable by time
		}
		distance = preciseDistance((it->second)->position, vantage) ;
		readable_time = it->first + distance / world->max_info_speed;
	}
	if (readable_time < time && !it->second->destroyed) {
		if(distance <= world->max_read_distance){
			return it->second;
		}else{
			//printf("read beyond distance from %f,%f,%f\n", vantage.x, vantage.y, vantage.z);
			//it->second->print();
			//printf("Why!?\n");
			return nullptr ;
		}
	}else {
		return nullptr;
	}
}

std::shared_ptr<const WorldObject> Timeline::ObjectHistory::readFar(const glm::vec3& vantage, const double time) {
	if (history.empty()) {
		throw std::runtime_error("Object history is empty on read.");
	}
	auto it = history.rbegin();
	double readable_time = it->first + preciseDistance((it->second)->position, vantage) / world->max_info_speed;
	while (readable_time >= time) {
		it++;
		if (it == history.rend()) {
			return nullptr; // earliest element wasn't readable by time
		}
		readable_time = it->first + preciseDistance((it->second)->position, vantage) / world->max_info_speed;
	}
	if (readable_time < time && !it->second->destroyed) {
		return it->second;
	}
	else {
		return nullptr;
	}
}

// Returns the state of this object at the given time (used for base state where time warp is not used)
std::shared_ptr<WorldObject> Timeline::ObjectHistory::getStateAt(const double time) {
	if (history.empty()) {
		throw std::runtime_error("Object history is empty on getStateAt.");
	}
	auto it = history.rbegin();
	double readable_time = it->first;
	while (readable_time > time) {
		it++;
		if (it == history.rend()) {
			return nullptr; // earliest element wasn't readable by time
		}
		readable_time = it->first;
	}
	if (readable_time <= time && !it->second->destroyed) {
		return it->second;
	}
	else {
		return nullptr ;
	}
}

//Returns all states of this history of this object in the given time range
//will be ordered from newest to oldest
std::vector<std::shared_ptr<WorldObject>> Timeline::ObjectHistory::getStateRange(const double start_time, const double end_time) {
	if (history.empty()) {
		throw std::runtime_error("Object history is emprty on getStateRange.");
	}
	std::vector<std::shared_ptr<WorldObject>> result;
	auto it = history.rbegin();
	double readable_time = it->first;
	while (readable_time > start_time) {
		if (readable_time <= end_time && !it->second->destroyed) {
			result.push_back(it->second);
		}
		it++;
		if (it == history.rend()) {
			return result; // for through then we're done
		}
		readable_time = it->first;
	}
	return result;

}

// returns the time and value of the latest instance of this object
std::shared_ptr<WorldObject> Timeline::ObjectHistory::getLatest() {
	return latest ;
}


// removes all but one element of the history before the given base_time
void Timeline::ObjectHistory::cleanHistory(double base_time) {
	int s = (int)history.size();
	auto it = history.lower_bound(base_time); // first element greater than or equal to time
	if (it != history.begin()) {
		auto keep = std::prev(it); // last element not greater than time so keep one more so we definitely have it at time
		if (keep != history.begin()) {
			history.erase(history.begin(), keep);
		}
	}
	if (history.size() == 0) {
		printf("history cleaned to 0? it was %d.\n", s);
	}
}

// Removes all instants after the given time
void Timeline::ObjectHistory::deleteAfter(double time) {
	auto keep = history.upper_bound(time);	
	//debug check
	/*
	for (auto& [t, instant] : history) {
		if (t <= time && instant->writing_event && instant->writing_event->actual_run_time < 0) {
			printf("Object was left whose event was rolled back!\n");
		}
		if(t > time && instant->writing_event && instant->writing_event->actual_run_time >= 0){
			printf("Object was rolled back whose event was not rolled back\n");
		}
	}*/
	
	history.erase(keep, history.end());
	if(history.size() != 0){
		latest = history.rbegin()->second ;
	}else{
		latest = nullptr ;
	}
	
}

void Timeline::ObjectHistory::addInstant(std::shared_ptr<WorldObject>& instant) {
	history[instant->time] = instant;
	latest = instant ;
}

// Runs an event that should be in pending_events and moves it to event_history
void Timeline::runEvent(std::shared_ptr<WorldEvent> event) {
	//debug check
	/*
	for(auto& effect : event_history){
		if(couldEffect(event, effect)){
			printf("event running after an event it could effect!\n");
			printf("cause rollbacks = %d:\n", event->rollbacks) ;
			event->print();
			printf("effect rollbacks = %d:\n", effect->rollbacks);
			effect->print();
		}
	}
	
	if(PRINT_RUNNING){
		event->print();
	}
	*/
	//printf("event run:");
	//event->print();

	auto it = pending_events.find(event);
	if (it == pending_events.end()) {
		throw std::runtime_error("Attempting to run an event that isn't pending? You probably wanted queue. runEvent is only public for technical reasons.");
	}
	pending_events.erase(it);
	new_events.clear();
	event->run(event);
	event_history.insert(event);
	event_runs++;
}


void Timeline::VoidEvent::run(std::shared_ptr<WorldEvent> this_event) {
	//printf("void event is running\n");
	auto it = world->objects.find(object_id);
	if (it == world->objects.end()) {
		throw std::runtime_error("VoidEvent is attempting to run but can't find it's object!");
	}
	// Get the latest instance for the object where we want to run the event
	std::shared_ptr<WorldObject> latest = it->second.getLatest();
	if (!latest) {
		throw std::runtime_error("VoidEvent attempting to run but object not found!");
	}
	if(latest->destroyed){
		return ;
	}
	// make a new instance by copying wit the serializer
	std::shared_ptr<WorldObject> new_latest = std::static_pointer_cast<WorldObject>(world->registry->deepCopy(latest.get(), latest->getTypeId(world->registry.get())));
	// mark the object and event with the run time and position
	new_latest->time = actual_run_time; // can be read when executing event, and should contain event time in that case
	new_latest->id = latest->id; // id is not saved so won't be copied with deepCopy
	new_latest->position = latest->position;
	new_latest->event_position = new_latest->position;
	new_latest->writing_event = this_event;
	// use the registry to execute the code for the event (events have Timeline::world and obj.time for use)
	world->registry->execute(new_latest, method_id, args);

	//Compute event duration to enforce speed constraints if the object was moved
	double event_duration = fmax(world->min_event_duration, preciseDistance(latest->position, new_latest->position) / (world->max_info_speed*object_move_fraction));
	//Add event duration to get write_time
	write_time = actual_run_time + event_duration;
	new_latest->time = write_time;


	// Place the new value into the object's history at the appropriate time
	it->second.addInstant(new_latest);
}

void Timeline::CreateEvent::run(std::shared_ptr<WorldEvent> this_event) {
	//printf("Createevent is running\n");
	auto it = world->objects.find(object_id);
	if (it != world->objects.end()) {
		printf("Duplicate ID: %lld\n", object_id);
		throw std::runtime_error("CreateEvent is attempting to run but an object already has its reserved id!");
	}
	//printf("Created: %lld\n", object_id) ;
	// Can't run event until the event dispatch has reached where the object will be
	//float time_at_obj = fmax(target_run_time, dispatch_time + glm::distance(new_object->position, dispatch_position) / max_info_speed);

	// make a new instance by copying with the serializer
	//std::shared_ptr<WorldObject> new_latest = new_object->deepCopy(world->registry.get());
	std::shared_ptr<WorldObject> new_latest = std::static_pointer_cast<WorldObject>(world->registry->deepCopy(new_object.get(), new_object->getTypeId(world->registry.get())));
	new_object->time = actual_run_time + world->min_event_duration; // create events still have event duration but the object couldn't move so it's just the min
	new_object->id = object_id; // time and id aren't expected to be in the serializer so we have to set them manually
	new_object->writing_event = this_event;
	//world->objects[object_id].addInstant(new_object);
	world->objects.emplace(object_id, new_object);
}

double Timeline::CreateEvent::getRunTime(Timeline* timeline, const glm::vec3& vantage) {
	//printf("getting create event run time\n");
	// Can't run event until the event dispatch has reached where the object will be
	actual_run_position = new_object->position;
	actual_run_time = fmax(target_run_time, dispatch_time + preciseDistance(actual_run_position, dispatch_position) / world->max_info_speed);
	// calculate time this event running at obj would be visible from the vantage point of execution
	double time_from_vantage = actual_run_time  + vantage_warp_fraction*preciseDistance(actual_run_position, vantage) / world->max_info_speed;
	return time_from_vantage ;
}


double Timeline::VoidEvent::getRunTime(Timeline* timeline, const glm::vec3& vantage) {
	//printf("getting void event run time\n");
	auto it = timeline->objects.find(object_id);
	if (it == timeline->objects.end()) {
		//printf("Void event wants to run at invalid id %I64d\n", object_id);
		return FLT_MAX; // Event is on an object that doesn't exist...yet, so it's not a bug
	}
	// Get the latest instance for the object where we want to run the event
	std::shared_ptr<WorldObject> latest = it->second.getLatest();
	/*
	if (!latest) {
		printf("Void event wants to run at empty history %I64d\n", object_id);
		return FLT_MAX; // this really shouldnt ever happen
	}
	*/
	actual_run_position = latest->position;
	// can't run event at object before it's latest state is written 
	actual_run_time = fmax(target_run_time, latest->time);
	// Can't run event until the event dispatch has reached the object
	actual_run_time = fmax(actual_run_time, dispatch_time + preciseDistance(actual_run_position, dispatch_position) / world->max_info_speed);
	// calculate time this event running at obj would be visible from the vantage point of execution
	double time_from_vantage = actual_run_time  + vantage_warp_fraction * preciseDistance(actual_run_position, vantage) / world->max_info_speed;
	return time_from_vantage;
}

void Timeline::VoidEvent::print() {
	if(world){
		world->printVoidEvent(this);
	}else{
		printf("Void event with function %d at object %lld and time = target %lf, actual %lf\n", method_id, object_id, target_run_time, actual_run_time);
		std::vector<char> serial = serialize(dispatch_position, dispatch_time, object_id, target_run_time, method_id, args);
		printf("  hash:  %lld, dispatch_time: %f, dispatch_position: %f,%f,%f\n", hashBytes(serial), dispatch_time, dispatch_position.x, dispatch_position.y, dispatch_position.z);
	}

}

//Print a void event to the console
void Timeline::printVoidEvent(VoidEvent* void_event){
	std::string cls = "unknown";
	if (void_event != nullptr) {
		if (objects.find(void_event->object_id) != objects.end()) {
			std::shared_ptr<WorldObject> o = objects[void_event->object_id].getLatest();
			cls = registry->class_name[o->getTypeId(registry.get())];
		}
		std::string mth = registry->method_name[void_event->method_id];
		printf("%lld running %s::%s at time %lf and position (%f, %f, %f)  dispatched from (%f,%f,%f) with target time %lf\n", void_event->object_id, cls.c_str(), mth.c_str(), void_event->actual_run_time,
			void_event->actual_run_position.x,void_event->actual_run_position.y, void_event->actual_run_position.z,
			void_event->dispatch_position.x, void_event->dispatch_position.y, void_event->dispatch_position.z,
			void_event->target_run_time) ;
	}
}

void Timeline::CreateEvent::print() {
	printf("Create event at object %lld and time %lf\n", object_id, target_run_time);
	new_object->print();
}



// Used to insert an event from outside the timeline
// Warning: Queueing an event in the past may immediately trigger rollback, but you'll need to run events to get back to current time
void Timeline::queue(const glm::vec3& dispatch_vantage, double dispatch_time, std::shared_ptr<WorldEvent> event) {
	world_lock.lock();
	event->dispatch_position = dispatch_vantage;
	event->dispatch_time = dispatch_time;
	if (last_vantage_time >= event->dispatch_time + vantage_warp_fraction * preciseDistance(last_vantage, event->dispatch_position) / max_info_speed) {
		pending_rollbacks.emplace_back(event->dispatch_position, event->dispatch_time);
	}
	external_events.emplace_back(event);
	pending_events.insert(event);
	world_lock.unlock();

	if (WorldPlugin::log_type == WorldPlugin::INJECTED_EVENTS || WorldPlugin::log_type == WorldPlugin::INJECTED_EXTENDED) {
		logInjectedEvent(event);
	}
}

// Used to insert an event from an update
// Warning: Queueing an event in the past may immediately trigger rollback, but you'll need to run events to get back to current time
void Timeline::internalQueue(const glm::vec3& dispatch_vantage, double dispatch_time, std::shared_ptr<WorldEvent> event) {
	world_lock.lock();
	event->dispatch_position = dispatch_vantage;
	event->dispatch_time = dispatch_time;
	if (last_vantage_time >= event->dispatch_time + vantage_warp_fraction * preciseDistance(last_vantage, event->dispatch_position) / max_info_speed) {
		pending_rollbacks.emplace_back(event->dispatch_position, event->dispatch_time) ;
	}
	pending_events.insert(event);
	world_lock.unlock();

	if(event->dispatch_time < last_vantage_time - history_kept){
		printf("Event receieved too far in the past, desync likely!\n");
	}

	if (WorldPlugin::log_type == WorldPlugin::INJECTED_EVENTS || WorldPlugin::log_type == WorldPlugin::INJECTED_EXTENDED) {
		logInjectedEvent(event);
	}

}

// logs an injected event that hasn't run yet with its dispatch data 
void Timeline::logInjectedEvent(std::shared_ptr<WorldEvent> event){
	std::string cls = "unknown";
	VoidEvent* void_event = dynamic_cast<VoidEvent*>(event.get());
	if (void_event != nullptr) {
		if (objects.find(event->object_id) != objects.end()) {
			std::shared_ptr<WorldObject> o = objects[event->object_id].getLatest();
			cls = registry->class_name[o->getTypeId(registry.get())];
		}
		std::string mth = registry->method_name[void_event->method_id];
		WorldPlugin::log->logOrdered(event->dispatch_time, cls + "::" + mth, event->object_id, event->dispatch_time, event->dispatch_position.x, event->dispatch_position.y, event->dispatch_position.z);
		if(WorldPlugin::log_type == WorldPlugin::INJECTED_EXTENDED){
			WorldPlugin::extended_log->logOrdered(event->dispatch_time, "injected", event->object_id, event->dispatch_time, event->dispatch_position.x, event->dispatch_position.y, event->dispatch_position.z);
		}
	}
	CreateEvent* create_event = dynamic_cast<CreateEvent*>(event.get());
	if (create_event != nullptr) {
		cls = registry->class_name[create_event->new_object->getTypeId(registry.get())];

		WorldPlugin::log->logOrdered(event->dispatch_time, cls + "::" + cls, event->object_id, event->dispatch_time, event->dispatch_position.x, event->dispatch_position.y, event->dispatch_position.z);
		if (WorldPlugin::log_type == WorldPlugin::INJECTED_EXTENDED) {
			WorldPlugin::extended_log->logOrdered(event->dispatch_time, "injected", event->object_id, event->dispatch_time, event->dispatch_position.x, event->dispatch_position.y, event->dispatch_position.z);
		}
	}
}


// Construct and queue a create event from outside the timeline
	// Warning: Queueing an event in the past may immediately trigger rollback, but you'll need to run events to get back to current time
int64_t Timeline::create(const glm::vec3& vantage, double vantage_time, std::shared_ptr<WorldObject> new_object, double target_time) {
	// hash object with time to get a unique id
	int type_id = new_object->getTypeId(registry.get());
	auto d = registry->serializeObj(type_id, new_object.get());
	int64_t reserved_id = hashBytes(d) ^ hashRaw(type_id ^ (*(int64_t*)&target_time));

	//printf("create at time: %f, reserved: %lld  from %lld , %lld\n", (float)target_time, reserved_id, hashBytes(d), hashRaw(type_id ^ (*(int64_t*)&target_time)));

	std::shared_ptr<CreateEvent> event = std::make_shared<CreateEvent>(reserved_id, new_object, target_time);
	//printf("queuing a create event with target time %f \n", event->target_run_time);
	queue(vantage, vantage_time, event);
	return reserved_id;
}

//read an object from the timeline at a given vantage point and time
// May return nullptr if the object can't be seen from the given vantage
std::shared_ptr<const WorldObject> Timeline::read(int64_t object_id, const glm::vec3& vantage, double vantage_time) {
	auto it = objects.find(object_id);
	if (it == objects.end()) { // don't create a history where there isn't one
		return nullptr ;
	}

	return it->second.read(vantage, vantage_time); // may still return nullptr if it's destroyed or can't be read yet
}

//read an object from the timeline at a given vantage point and time
// May return nullptr if the object can't be seen from the given vantage
std::shared_ptr<const WorldObject> Timeline::readFar(int64_t object_id, const glm::vec3& vantage, double vantage_time) {
	auto it = objects.find(object_id);
	if (it == objects.end()) { // don't create a history where there isn't one
		return nullptr;
	}

	return it->second.readFar(vantage, vantage_time); // may still return nullptr if it's destroyed or can't be read yet
}

//Runs all events that could run before the given vantage
void Timeline::run(const glm::vec3 vantage, double vantage_time) {

	applyPendingRollbacks();

	//runBatched(vantage, vantage_time);
	//runSorted(vantage, vantage_time);
	runHeap(vantage, vantage_time) ;

	world_lock.lock();
	last_vantage_time = vantage_time;
	last_vantage = vantage;

	
	//only clean the history periodically since it's kind of expensive and having a little extra is fine
	if (vantage_time - last_clean_time > history_kept * 0.5f) {
		double clear_time = vantage_time - history_kept;
		for (auto& [id, history] : objects) {
			history.cleanHistory(clear_time);
		}
		

		std::map<double,std::vector<std::shared_ptr<WorldEvent>>> event_deletes; // map on time allows to be sorted by actual game time
		for (auto& event : event_history) {
			if (event->actual_run_time < clear_time) {
					event_deletes[event->actual_run_time].push_back(event);
			}
		}
		for (auto&[time, event_list] : event_deletes) { // log in gametime order
			for(auto&event : event_list){
				if (WorldPlugin::log_type == WorldPlugin::FINAL_EVENTS) {
					std::shared_ptr<WorldObject> o = objects[event->object_id].getLatest(); 
					std::string cls = registry->class_name[o->getTypeId(registry.get())] ;
					VoidEvent* void_event = dynamic_cast<VoidEvent*>(event.get());
					if (void_event != nullptr) {
						std::string mth = registry->method_name[void_event->method_id] ;
						WorldPlugin::log->log(cls + "::" + mth, event->object_id, event->actual_run_time, event->target_run_time, event->dispatch_time, event->actual_run_position.x, event->actual_run_position.y, event->actual_run_position.z);
					}
					CreateEvent* create_event = dynamic_cast<CreateEvent*>(event.get());
					if (create_event != nullptr) {
						WorldPlugin::log->log(cls + "::" + cls, event->object_id, event->actual_run_time, event->target_run_time, event->dispatch_time, event->actual_run_position.x, event->actual_run_position.y, event->actual_run_position.z);
					}
				}
				//actually delete the event after logging
				event->parent.reset(); // break the chain of event parents which would otherwise outlive the events indefinitely
				event_history.erase(event);
			}
		}

		last_clean_time = vantage_time;
	}

	
	world_lock.unlock();

}

// Runs the next event that can run from the given vantage point if there is one
	// Returns whether an event was run
	// good for debugging but innefficient, consider using other run methods for general application
bool Timeline::runNext(const glm::vec3& vantage, double vantage_time) {

	world_lock.lock();
	world = this ;
	double first_run_time = FLT_MAX;
	double first_dispatch_time = FLT_MAX;
	double first_target_time = FLT_MAX ;
	std::shared_ptr<WorldEvent> first_event = std::shared_ptr<WorldEvent>(nullptr);
	for (auto& event : pending_events) {
		double run_time = event->getRunTime(this, vantage);
		double target_time = event->target_run_time ;
		double dispatch_time = event->dispatch_time ;
		bool first = false;
		if (run_time < first_run_time) { 
			first  = true ;
		}else if(run_time == first_run_time){
			if(target_time < first_target_time){
				first= true ;
			}else if(target_time == first_target_time){
				if(dispatch_time < first_dispatch_time){
					first = true ;
				}else if(dispatch_time == first_dispatch_time){
					if(event->object_id == first_event->object_id){
						printf("runtime ambiguity!  An event was probably added twice:\n");
						event->print();
						first_event->print();
						printf("Parent events:\n");
						if (event->parent) {
							event->parent->print();
						}

						if (first_event->parent) {
							first_event->parent->print();
						}

						throw std::runtime_error("Runtime ambiguity tie could not be broken, synchronization lost.");
					}
				}
			}
		}

		if(first){
			first_run_time = run_time;
			first_target_time = target_time ;
			first_dispatch_time = dispatch_time ;
			first_event = event;			
		}
	}
	//printf("Best run: at time %f\n", first_run_time);
	if (first_run_time <= vantage_time) {
		runEvent(first_event);
		world_lock.unlock();
		return true;
	}
	world_lock.unlock();
	return false;
}

//implementation of run that runs events one at a time for simpler debugging
void Timeline::runSorted(const glm::vec3& vantage, double vantage_time){
	while(runNext(vantage,vantage_time) );
}


// Runs the next batch of events that can't possible depend on each other
bool Timeline::runBatch(const glm::vec3& vantage, double vantage_time){

	world_lock.lock();
	world = this;
	int num_run = 0 ;
	
	// next run on each object
	static std::unordered_map<int64_t, std::pair<double, std::shared_ptr<WorldEvent>>> next ; 
	next.clear();
	double earliest_run_time = FLT_MAX ;
	for (auto& event : pending_events) {
		double run_time = event->getRunTime(this, vantage);
		/*
		if(run_time > vantage_time){
			continue ;
		}
		run_time = event->actual_run_time ;
		*/
		double target_time = event->target_run_time;
		double dispatch_time = event->dispatch_time;
		


		if(next.find(event->object_id) == next.end()){
			next[event->object_id] = std::make_pair(run_time, event) ;
			earliest_run_time = fmin(earliest_run_time, run_time);
		}else{
			bool first = false;
			double first_run_time = next[event->object_id].first;
			double first_dispatch_time = next[event->object_id].second->dispatch_time ;
			double first_target_time = next[event->object_id].second->target_run_time;

			if (run_time < first_run_time) {
				first = true;
			}
			else if (run_time == first_run_time) {
				if (target_time < first_target_time) {
					first = true;
				}
				else if (target_time == first_target_time) {
					if (dispatch_time < first_dispatch_time) {
						first = true;
					}
					else if (dispatch_time == first_dispatch_time) {
							printf("runtime ambiguity!  An event was probably added twice:\n");
							event->print();
							//next[event->object_id].second->print();
							printf("Parent events:\n");
							if (event->parent) {
								event->parent->print();
								if (event->parent->parent) {
									event->parent->parent->print();
								}else{
									printf("----no grandparent----\n");
								}
							}else{
								printf("----no parent----\n");
							}

							next[event->object_id].second->print();
							printf("Parent events:\n");
							
							if (next[event->object_id].second->parent) {
								next[event->object_id].second->parent->print();
								if (next[event->object_id].second->parent->parent) {
									next[event->object_id].second->parent->parent->print();
								}else {
									printf("----no grandparent----\n");
								}
							}else {
								printf("----no parent----\n");
							}
							throw std::runtime_error("Runtime ambiguity tie could not be broken, synchronization lost.");
					}
				}
			}
	
			if (first) {
				next[event->object_id] = std::make_pair(run_time, event);
				earliest_run_time = fmin(earliest_run_time,run_time) ;
			}
		}
	}

	double allowed_time = fmin(vantage_time, earliest_run_time + min_event_duration * duration_step_fraction);
	for(auto& [id, pair] : next){
		double& run_time = pair.first ;
		std::shared_ptr<WorldEvent>& event = pair.second ;
		
		if(run_time <= allowed_time){
			runEvent(event);
			num_run++;
		}
	}
	world_lock.unlock();
	//printf("Num run: %d  Earliest: %lf\n", num_run, earliest_run_time) ;
	return num_run > 0 ;
}

//implementation of run that runs events one at a time for simpler debugging
void Timeline::runBatched(const glm::vec3& vantage, double vantage_time) {
	while (runBatch(vantage, vantage_time));
}

// Given a vector of events on the same object
// moves the earliest running event to the end and returns its run time
double Timeline::moveEarliestLast(std::vector<std::shared_ptr<WorldEvent>>& events, const glm::vec3& vantage){
	int first_index = 0;
	double first_run_time = events[first_index]->getRunTime(this, vantage);
	double first_dispatch_time = events[first_index]->dispatch_time;
	double first_target_time = events[first_index]->target_run_time;
	for (int c = 1; c < events.size(); c++) {
		double run_time = events[c]->getRunTime(this, vantage);
		double dispatch_time = events[c]->dispatch_time;
		double target_time = events[c]->target_run_time;
		bool first = false;
		if (run_time < first_run_time) {
			first = true;
		}else if (run_time == first_run_time) {
			if (target_time < first_target_time) {
				first = true;
			}else if (target_time == first_target_time) {
				if (dispatch_time < first_dispatch_time) {
					first = true;
				}else if (dispatch_time == first_dispatch_time) {
					printf("runtime ambiguity!  An event was probably added twice:\n");
					events[c]->print();
					events[first_index]->print();
					throw std::runtime_error("Runtime ambiguity tie could not be broken, synchronization lost.");
				}
			}
		}
		if (first) {
			first_index = c; // set this event to first
			first_run_time = run_time;
			first_dispatch_time = dispatch_time;
			first_target_time = target_time;
		}
	}
	//Move it to the end of the list
	std::shared_ptr<WorldEvent> temp = events[events.size()-1] ;
	events[events.size() - 1] = events[first_index];
	events[first_index] = temp ;
	return first_run_time ;
}


//Run pending events via heap
void Timeline::runHeap(const glm::vec3& vantage, double vantage_time){
	world_lock.lock();
	world = this;

	// priority queue for getting the object with the lowest next event run time first
	std::priority_queue<std::pair<double, int64_t>, std::vector<std::pair<double, int64_t>>, std::greater<std::pair<double, int64_t>> > queue ;
	// map from object to next earliest runtime and all events pending on that object
	std::unordered_map< int64_t, std::pair<double, std::vector<std::shared_ptr<WorldEvent>>>> object_events;

	//Group all events by object
	for (auto& event : pending_events) {
		if(event->target_run_time <= vantage_time){
			object_events[event->object_id].second.push_back(event) ;
		}
	}
	// Put the first event for each object into the priority queue
	for(auto& [object_id, time_and_events] : object_events){
		time_and_events.first = moveEarliestLast(time_and_events.second, vantage);
		queue.push({ time_and_events.first, object_id}) ;
	}
			
	double last_run_time = -1 ;
	while(!queue.empty()){
		//Fetch event with the earliest next run time
		double queue_time = queue.top().first ;
		int64_t object_id = queue.top().second ;
		queue.pop();
		auto& time_and_events = object_events[object_id] ;
		if(queue_time != time_and_events.first){ // entry in queue is stale, time has changed
			continue ; //skip it
		}
		if(queue_time > vantage_time){ // next event is past run time
			world_lock.unlock();
			return ; // we're done
		}
		
		runEvent(time_and_events.second.back()); // Run next event
		time_and_events.second.pop_back(); // remove it from the object event list
		time_and_events.first = FLT_MAX ; // There can be duplicate queue elements, so make sure time is unset so they don't run with no object
		bool need_to_requeue_ran = false;
		if(time_and_events.second.size() > 0){ // if there are more events on that object
			time_and_events.first = moveEarliestLast(time_and_events.second, vantage); // recompute the next time and move the earliest to the end of the list
			need_to_requeue_ran = true;
		}
		
		bool have_requeued_ran = false;
		//Queue any new events the event we just ran might have created
		for (auto& new_event : new_events) {
			int64_t new_object_id = new_event->object_id ;
			if (new_event->target_run_time <= vantage_time) { // if new event is even trying to run this cycle
				double new_run_time = new_event->getRunTime(this, vantage);
				auto& new_time_and_events = object_events[new_object_id] ;
				
				if(new_time_and_events.second.size()  == 0 ){ // currently no events queued on the object of the new event
					new_time_and_events.second.push_back(new_event);
					new_time_and_events.first = new_run_time ;
					queue.push({ new_time_and_events.first, new_object_id });
					have_requeued_ran |= new_object_id == object_id ;
				}else{// there were other events on that object queued
					// Pull the previous leader out
					double old_run_time = new_time_and_events.first ;
					std::shared_ptr<WorldEvent> old_earliest = new_time_and_events.second.back();
					new_time_and_events.second.pop_back();
					double old_target_time = old_earliest->target_run_time;
					double old_dispatch_time = old_earliest->dispatch_time ;

					double new_target_time = new_event->target_run_time;
					double new_dispatch_time = new_event->dispatch_time;
					// Check if new event should run before the current leader for that object
					bool first = false;
					if (new_run_time < old_run_time) {
						first = true;
					}else if (new_run_time == old_run_time) {
						if (new_target_time < old_target_time) {
							first = true;
						}else if (new_target_time == old_target_time) {
							if (new_dispatch_time < old_dispatch_time) {
								first = true;
							}else if (new_dispatch_time == old_dispatch_time) {
								printf("runtime ambiguity!  An event was probably added twice:\n");
								old_earliest->print();
								new_event->print();
								throw std::runtime_error("Runtime ambiguity tie could not be broken, synchronization lost.");
							}
						}
					}

					if(first){ // new event should run before the previous earliest on that object
						new_time_and_events.second.push_back(old_earliest);
						new_time_and_events.second.push_back(new_event); // new event is now at the end
						if(new_time_and_events.first != new_run_time){ // time changed
							new_time_and_events.first = new_run_time ;
							queue.push({ new_time_and_events.first, new_object_id }); // requeue, old one will be skipped because time no longer matches
							have_requeued_ran |= new_object_id == object_id;
						}
					}else{ // new event does not preempt existing first event
						// keep the previous earliest at the end and leave the time and queue alone
						new_time_and_events.second.push_back(new_event);
						new_time_and_events.second.push_back(old_earliest); 
					}
				}
			}
		}
		// Put the events for the object we just ran on back into the queue if we didn't already while adding new events
		if(need_to_requeue_ran && !have_requeued_ran){
			queue.push({ time_and_events.first, object_id });
		}


	}
	
	world_lock.unlock();

}

//returns whether an event could effect another event
bool Timeline::couldEffect(const std::shared_ptr<WorldEvent>& cause, const std::shared_ptr<WorldEvent>& effect){
	if( cause->object_id == effect->object_id){
		return cause->actual_run_time < effect->actual_run_time ;
	}else{
		return effect->actual_run_time >= cause->actual_run_time + min_event_duration + preciseDistance(effect->actual_run_position, cause->actual_run_position) / max_info_speed ;
	}
}

//rolls back all events and object changes that have occured withing the light cone of the trigger
void Timeline::rollback(const glm::vec3& trigger_position, double trigger_time) {
	world_lock.lock();
	//printf("Rolling back to %f\n", trigger_time) ;
	std::unordered_set<std::shared_ptr<WorldEvent>> event_rollbacks;
	std::map<int64_t, double> object_rollbacks; // earliest event time on each object that needs rolled back
	for (auto& h_event : event_history) {
		// Is an already ran event in the light cone of the rollback?
		if (h_event->actual_run_time  >= trigger_time + vantage_warp_fraction * preciseDistance(h_event->actual_run_position, trigger_position) / max_info_speed) {
			event_unruns++;
			event_rollbacks.insert(h_event);
			// Keep track of how far we need to makerollbacks to objects affected by these events
			auto it = object_rollbacks.find(h_event->object_id);
			if (it == object_rollbacks.end()) {
				object_rollbacks[h_event->object_id] = h_event->actual_run_time;
			}
			else {
				it->second = fmin(it->second, h_event->actual_run_time);
			}
		}
	}

	//Find events that are pending that were spawned by an event that was rolled back
	std::vector<std::shared_ptr<WorldEvent>> pending_rollbacks;
	for (auto& p_event : pending_events) {
		if (event_rollbacks.find(p_event->parent) != event_rollbacks.end()) {
			pending_rollbacks.push_back(p_event);
		}
	}
	// unpend events spawned by now rolled back events
	for (auto& p_event : pending_rollbacks) {
		pending_events.erase(p_event);
	}

	for (auto& event : event_rollbacks) {
		event_history.erase(event); // remove form history
		event->rollbacks++;
		event->actual_run_time = -1.0 ;

		// only repend if it wasn't spawned by another rolled back event
		if (event_rollbacks.find(event->parent) == event_rollbacks.end()) {
			pending_events.insert(event);
		}
	}

	//Rollback the objects
	for (auto& [id, time] : object_rollbacks) {
		//printf("Deleting object %lld after %f\n", id, time);
		objects[id].deleteAfter(time);
		if (objects[id].history.empty()) {
			objects.erase(id);
		}
	}

	//last_vantage_time = trigger_time;
	//runBatched(last_vantage,last_vantage_time) ;
	world_lock.unlock();
}

//Returns all readable entities from the given vantage
std::vector<std::shared_ptr<const WorldObject>> Timeline::observe(const glm::vec3& vantage, double vantage_time) {
	world_lock.lock();
	std::vector<std::shared_ptr<const WorldObject>> observed;
	for (auto& [id, o] : objects) {
		std::shared_ptr<const WorldObject> i = readFar(id, vantage, vantage_time);
		if (i) {
			observed.push_back(i);
		}
	}
	world_lock.unlock();
	return observed;
}

void Timeline::applyPendingRollbacks(){
	if(pending_rollbacks.size() == 0){
		return ;
	}
	world_lock.lock();
	double earliest_trigger = FLT_MAX;
	for (auto& r : pending_rollbacks) {
		earliest_trigger = fmin(earliest_trigger, r.second);
	}
	if(earliest_trigger > last_vantage_time){
		pending_rollbacks.clear();
		world_lock.unlock();
		return ;
	}

	//printf(" %d Rolling back to %f\n", (int)pending_rollbacks.size(),earliest_trigger) ;
	std::unordered_set<std::shared_ptr<WorldEvent>> event_rollbacks;
	std::map<int64_t, double> object_rollbacks; // earliest event time on each object that needs rolled back
	for (auto& h_event : event_history) {
		// Is an already ran event in the light cone of a rollback?
		if (h_event->actual_run_time >= earliest_trigger){
			bool rolling_back = false;
			for (auto& r : pending_rollbacks) {
				rolling_back |= h_event->actual_run_time >= r.second + vantage_warp_fraction * preciseDistance(h_event->actual_run_position, r.first) / max_info_speed ;
			}
		

			if(rolling_back) {
				event_unruns++;
				event_rollbacks.insert(h_event);
				// Keep track of how far we need to makerollbacks to objects affected by these events
				auto it = object_rollbacks.find(h_event->object_id);
				if (it == object_rollbacks.end()) {
					object_rollbacks[h_event->object_id] = h_event->actual_run_time;
				}
				else {
					it->second = fmin(it->second, h_event->actual_run_time);
				}
			}
		}
	}

	//Find events that are pending that were spawned by an event that was rolled back
	std::vector<std::shared_ptr<WorldEvent>> pending_rollback_events;
	for (auto& p_event : pending_events) {
		if (event_rollbacks.find(p_event->parent) != event_rollbacks.end()) {
			pending_rollback_events.push_back(p_event);
		}
	}
	// unpend events spawned by now rolled back events
	for (auto& p_event : pending_rollback_events) {
		pending_events.erase(p_event);
	}

	for (auto& event : event_rollbacks) {
		event_history.erase(event); // remove form history
		event->rollbacks++;
		event->actual_run_time = -1.0;

		// only repend if it wasn't spawned by another rolled back event
		if (event_rollbacks.find(event->parent) == event_rollbacks.end()) {
			pending_events.insert(event);
		}
	}

	//Rollback the objects
	for (auto& [id, time] : object_rollbacks) {
		//printf("Deleting object %lld after %f\n", id, time);
		objects[id].deleteAfter(time);
		if (objects[id].history.empty()) {
			objects.erase(id);
		}
	}

	//last_vantage_time = trigger_time;
	//runBatched(last_vantage,last_vantage_time) ;


	pending_rollbacks.clear();
	world_lock.unlock();
}


// Returns the value of all objects at the given time (i.e. the latest instance before the time)
std::map<int64_t, std::shared_ptr<WorldObject>> Timeline::getBaseObjects(double time) {
	world_lock.lock();
	std::map<int64_t, std::shared_ptr<WorldObject>> base_objects;
	//printf("getting base objects at %lf\n", time);
	for (auto& [id, history] : objects) {
		std::shared_ptr<WorldObject> o = history.getStateAt(time);
		if (o) {
			base_objects[id] = o;
		}else{
			/*
			printf("ID %lld exists in objects with %d states but not in base objects\n", id, (int)history.history.size()) ;
			for(auto& [ot, o]: history.history){
				printf("  %lf, ", ot) ;
			}
			printf("\n") ;
			*/
		}
	}
	world_lock.unlock();
	return base_objects;
}

//serializes a WorldObject with all of the variables needed to recreate it on another tineline
//which is more than the derived objects will serialize
std::vector<char> Timeline::serializeWorldObject(std::shared_ptr<WorldObject> object) {
	int type_id = object->getTypeId(registry.get());
	std::vector<char> object_data = registry->serializeObj(type_id, object.get());
	double write_time = object->time;
	int64_t object_id = object->id;
	return serialize(object_id, type_id, write_time, object_data);
}


//serializes a WorldEvent with all of the variables needed to recreate it on another timeline
std::pair<char, std::vector<char>> Timeline::serializeWorldEvent(std::shared_ptr<WorldEvent> event) {
	VoidEvent* void_event = dynamic_cast<VoidEvent*>(event.get());
	if (void_event != nullptr) {
		//printf("serializing void event\n");
		return std::make_pair(VOID_EVENT,
			serialize(void_event->dispatch_position, void_event->dispatch_time, void_event->object_id, void_event->target_run_time, void_event->method_id, void_event->args)
		);
	}
	CreateEvent* create_event = dynamic_cast<CreateEvent*>(event.get());
	if (create_event != nullptr) {
		//printf("serializing create event\n");
		int object_type_id = create_event->new_object->getTypeId(registry.get());
		std::vector<char> object_data = registry->serializeObj(object_type_id, create_event->new_object.get());

		return std::make_pair(CREATE_EVENT,
			serialize(create_event->dispatch_position, create_event->dispatch_time, create_event->object_id, create_event->target_run_time, object_type_id, object_data)
		);
	}
	printf("Event type not serializable!?\n");
	return std::make_pair(0, std::vector<char>());
}


Timeline::Timeline(std::shared_ptr<Registry>& r, CopyPacket& packet){
	registry = r ;
	if(COPY_PACKET == -1){
		COPY_PACKET = registry->registerClass<CopyPacket>("CopyPacket");
		UPDATE_PACKET = registry->registerClass<UpdatePacket>("UpdatePacket");
	}
	max_info_speed = packet.max_info_speed ;
	min_event_duration = packet.min_event_duration;
	history_kept = packet.history_kept ;
	last_vantage_time = packet.last_vantage_time;
	last_vantage = packet.last_vantage ;
	//printf("Copied data speed: %f event_dur: %f  history:%f\n", max_info_speed, min_event_duration, history_kept) ;

	world_lock.lock();
	for (auto& object_serial : packet.objects) {
		const auto& [object_id, type_id, write_time, object_data] = deserialize<int64_t, int, double, std::vector<char>>(object_serial);
		//insert the update object directly into the timeline
		std::shared_ptr<WorldObject> new_object = std::static_pointer_cast<WorldObject>(registry->deserializeObj(type_id, object_data));
		new_object->time = write_time; 
		new_object->id = object_id; // time and id aren't expected to be in the serializer so we have to set them manually
		if(objects.find(object_id) == objects.end()){
			objects.emplace(object_id, new_object);
		}else{
			objects[object_id].addInstant(new_object);
		}
		//printf("Adding copied object id: %lld  time:%f\n", new_object->id, new_object->time);
	}

	//printf("Objects after copy: %d\n", (int)objects.size());


	for (auto& [event_type, event_serial] : packet.pending_events) {
		if (event_type == VOID_EVENT) {
			const auto& [dispatch_position, dispatch_time, object_id, target_time, method_id, arg_serial] = deserialize<glm::vec3, double, int64_t, double, int, std::vector<char>>(event_serial);
			std::shared_ptr<VoidEvent> event = std::make_shared<VoidEvent>(object_id, method_id, target_time, arg_serial);
			
			event->dispatch_position = dispatch_position;
			event->dispatch_time = dispatch_time;
			//printf("Adding pending event:\n");
			//event->print();
			pending_events.emplace(event);
			
		}else if (event_type == CREATE_EVENT) {
			const auto& [dispatch_position, dispatch_time, object_id, target_time, type_id, object_data] = deserialize<glm::vec3, double, int64_t, double, int, std::vector<char>>(event_serial);
			std::shared_ptr<WorldObject> obj = std::static_pointer_cast<WorldObject>(registry->deserializeObj(type_id, object_data));
			std::shared_ptr<CreateEvent> event = std::make_shared<CreateEvent>(object_id, obj, target_time);

			event->dispatch_position = dispatch_position;
			event->dispatch_time = dispatch_time;
			//printf("Adding pending event:\n");
			//event->print();
			pending_events.emplace(event);
			

		}
	}
	//printf("Pending events after copy: %d\n", (int)pending_events.size()) ;

	for (auto& [event_type, event_serial] : packet.event_history) {
		if (event_type == VOID_EVENT) {
			const auto& [dispatch_position, dispatch_time, object_id, target_time, method_id, arg_serial] = deserialize<glm::vec3, double, int64_t, double, int, std::vector<char>>(event_serial);
			std::shared_ptr<VoidEvent> event = std::make_shared<VoidEvent>(object_id, method_id, target_time, arg_serial);
			event->dispatch_position = dispatch_position;
			event->dispatch_time = dispatch_time;
			event_history.emplace(event);
		}
		else if (event_type == CREATE_EVENT) {
			const auto& [dispatch_position, dispatch_time, object_id, target_time, type_id, object_data] = deserialize<glm::vec3, double, int64_t, double, int, std::vector<char>>(event_serial);
			std::shared_ptr<WorldObject> obj = std::static_pointer_cast<WorldObject>(registry->deserializeObj(type_id, object_data));
			std::shared_ptr<CreateEvent> event = std::make_shared<CreateEvent>(object_id, obj, target_time);
			event->dispatch_position = dispatch_position;
			event->dispatch_time = dispatch_time;
			event_history.emplace(event);

		}
	}


	world_lock.unlock();
}


// Returns a seralized update to duplicate the currently active state of this timeline
	// First element is objects, second is event, events also have types
Timeline::CopyPacket Timeline::copy(double earliest_time) {
	CopyPacket update;
	update.max_info_speed = max_info_speed;
	update.min_event_duration = min_event_duration;
	update.max_read_distance = max_read_distance ;
	update.history_kept = history_kept;
	update.last_vantage = last_vantage;
	update.last_vantage_time = last_vantage_time ;
	world_lock.lock();
	std::vector<std::shared_ptr<WorldEvent>> base_events;
	for (auto& event : pending_events) {
		update.pending_events.emplace_back(serializeWorldEvent(event));
	}

	
	for (auto& event : event_history) {
		//event->print();
		if (event->actual_run_time >= earliest_time) {
			update.event_history.emplace_back(serializeWorldEvent(event));
		}
	}

	for (auto& [id, history] : objects) {
		std::shared_ptr<WorldObject> o = history.getStateAt(earliest_time); // so get latest state
		if(o){
			update.objects.push_back(serializeWorldObject(o));
		}
		std::vector<std::shared_ptr<WorldObject>> instants = history.getStateRange(earliest_time, FLT_MAX);

		for (int k = (int)instants.size() -1; k >=0; k--) {//get state range is newest to oldest bbut for packet we need oldest to newest
			update.objects.push_back(serializeWorldObject(instants[k]));
		}
	}
	external_events.clear(); // these aren't external anymore if we're doing a full copy after
	//printf("Objects in copy packet: %d\n", (int)update.objects.size()) ;
	world_lock.unlock();
	return update;

}

//Applies an update packet 
	//returns if still in sync TODO
bool Timeline::applyUpdate(UpdatePacket& packet){
	for (auto& [event_type, event_serial] : packet.external_events) {
		if (event_type == VOID_EVENT) {
			const auto& [dispatch_position, dispatch_time, object_id, target_time, method_id, arg_serial] = deserialize<glm::vec3, double, int64_t, double, int, std::vector<char>>(event_serial);

			std::shared_ptr<VoidEvent> event = std::make_shared<VoidEvent>(object_id, method_id, target_time, arg_serial);
			internalQueue(dispatch_position, dispatch_time, event);

		}
		else if (event_type == CREATE_EVENT) {
			const auto& [dispatch_position, dispatch_time, object_id, target_time, type_id, object_data] = deserialize<glm::vec3, double, int64_t, double, int, std::vector<char>>(event_serial);

			std::shared_ptr<WorldObject> obj = std::static_pointer_cast<WorldObject>(registry->deserializeObj(type_id, object_data));
			std::shared_ptr<CreateEvent> event = std::make_shared<CreateEvent>(object_id, obj, target_time);
			internalQueue(dispatch_position, dispatch_time, event);

		}
	}
	return true ;
}


//Logs an update packet
void Timeline::logUpdate(UpdatePacket& packet, const std::string& log_action) {
	for (auto& [event_type, event_serial] : packet.external_events) {
		if (event_type == VOID_EVENT) {
			const auto& [dispatch_position, dispatch_time, object_id, target_time, method_id, arg_serial] = deserialize<glm::vec3, double, int64_t, double, int, std::vector<char>>(event_serial);

			std::shared_ptr<VoidEvent> event = std::make_shared<VoidEvent>(object_id, method_id, target_time, arg_serial);
			WorldPlugin::extended_log->logOrdered(dispatch_time, log_action, object_id, dispatch_time, dispatch_position.x, dispatch_position.y, dispatch_position.z);

		}else if (event_type == CREATE_EVENT) {
			const auto& [dispatch_position, dispatch_time, object_id, target_time, type_id, object_data] = deserialize<glm::vec3, double, int64_t, double, int, std::vector<char>>(event_serial);

			std::shared_ptr<WorldObject> obj = std::static_pointer_cast<WorldObject>(registry->deserializeObj(type_id, object_data));
			std::shared_ptr<CreateEvent> event = std::make_shared<CreateEvent>(object_id, obj, target_time);
			
			WorldPlugin::extended_log->logOrdered(dispatch_time, log_action, object_id, dispatch_time, dispatch_position.x, dispatch_position.y, dispatch_position.z);


		}
	}
}

//Returns an update with all external events since the last time this was called
Timeline::UpdatePacket Timeline::getPendingUpdate(){
	UpdatePacket update ;
	/*
	update.hash_time = hash_time ;
	update.object_hash = computeObjectHash(hash_time) ; 
	*/
	for(int k= 0 ; k < external_events.size();k++){
		update.external_events.emplace_back(serializeWorldEvent(external_events[k]));
		if (WorldPlugin::log_type == WorldPlugin::INJECTED_EXTENDED) {
			auto& event = external_events[k];
			WorldPlugin::extended_log->logOrdered(event->dispatch_time, "getPendingUpdate", event->object_id, event->dispatch_time, event->dispatch_position.x, event->dispatch_position.y, event->dispatch_position.z);
		}
	}
	update.last_run_time = last_vantage_time ;
	external_events.clear();
	return update ;
	
}

//Returns the hash of all objects at the given instant
//for consistency time should be far enough in the past to negate time warp
int64_t Timeline::computeObjectHash(double hash_time){
	/*
	static std::unordered_map<double, int64_t> cached ;
	auto it = cached.find(hash_time) ;
	if(it != cached.end()){
		return it->second ;
	}
	*/
	std::map<int64_t, std::shared_ptr<WorldObject>> base_objects = getBaseObjects(hash_time);
	std::vector<int64_t> hashes ;

	for (auto& [id, object] : base_objects) {
		std::vector<char> serial = serializeWorldObject(object);
		hashes.emplace_back(hashBytes(serial));
	}
	int64_t total_hash = hashBytes(serialize(hashes)) ;
	//cached[hash_time] = total_hash ;
	return total_hash ;
}