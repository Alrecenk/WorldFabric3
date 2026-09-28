#ifndef _ACTION_MAP_H_
#define _ACTION_MAP_H_ 1

#include "Utilities.h"

#include <memory>
#include <map>
#include <mutex>
#include <queue>


// Outer base class makes it possible to put templated subclasses into one map
class BaseActionReceiver {
public:
	virtual ~BaseActionReceiver() = default;
};

//Action triggers have an axis aligned bounding box and a context that is used for their collision with action
//You can override this to add metadata to a trigger that will be accessible in the reciever when actions are caught.
class ActionTrigger {
public:
	int context = 0;
	glm::vec3 min;
	glm::vec3 max;
	int id = -1;
	BaseActionReceiver* action_receiver;

	ActionTrigger() {}

	ActionTrigger(int ctx, const glm::vec3& box_min, const glm::vec3& box_max, BaseActionReceiver* receiver) : context(ctx), min(box_min), max(box_max), action_receiver(receiver) {
	}
};

class ActionMap;

//This is the base action, you probably don't want to override it directly.
//Your custom actions should probably override a subclass that defines the collision type
// like RayAction, BoxAction, or UniversalAction
class Action {
public:
	int context = 0 ;

	virtual std::priority_queue<
		std::pair<float, std::shared_ptr<ActionTrigger>>,
		std::vector<std::pair<float, std::shared_ptr<ActionTrigger>>>,
		std::greater<std::pair<float, std::shared_ptr<ActionTrigger>>>
	> findTriggers(ActionMap* action_map, Action* action)  = 0;

	virtual ~Action() = default;
};


//This is the class you want to override for your recievers and the template is the action you want to be able to receive
//You can override this multiple times with different templates to get multiple action types
template <typename T>
class ActionReceiver : public BaseActionReceiver {
	//static_assert(std::is_base_of<WorldObject, T>::value, "View template must inherit from WorldObject.");

public:
	//recieveAction is called when an action collides with a trigger that has this object as its reciever
	// Coloision occurs when
	//1) The trigger and action are in the same context.
	//2) The geometry of the trigger intersect the geometry of the action.
	//3) The action reciever overrides ActionReceiver for the type of action
	virtual void receiveAction(T* action, ActionTrigger* trigger) = 0;

	//An action may wish to send a signal to an item it previously intersected
	// For instance to alert an item being interacted with that it not longer intersectsthe trigger
	virtual void receiveSignal(int signal_type, T* action, ActionTrigger* trigger){};

};


class ActionMap{
public:	
	std::map<int,std::shared_ptr<ActionTrigger>> triggers ;
	//TODO ActionMap needs like a kd-tree or something so it can search geometry faster when there are a lot of triggers
	int next_trigger_id = 1 ;
	std::mutex lock ;

	int addTrigger(std::shared_ptr<ActionTrigger> trigger){
		lock.lock();
		int id = next_trigger_id ;
		next_trigger_id++;
		triggers[id] = trigger ;
		lock.unlock();
		return id ;
	}
	void removeTrigger(int id){
		lock.lock();
		triggers.erase(id);
		lock.unlock();
	}

	void moveTrigger(int id,const glm::vec3& new_min, const glm::vec3& new_max){
		auto iter = triggers.find(id);
		if(iter!= triggers.end()){
			iter->second->min = new_min ;
			iter->second->max = new_max ;
		}
	}

	//Immediately performs an action, sending it to all recievers whose triggers are hit
	// returns the number of triggers the action was sent to
	template <typename T>
	int performAction(T* action) {
		lock.lock();
		auto triggers = action->findTriggers(this, action);
		lock.unlock();
		int performed = 0;
		while (!triggers.empty()) {
			auto trigger = triggers.top();
			triggers.pop();
			ActionReceiver<T>* receiver = dynamic_cast<ActionReceiver<T>*>(trigger.second->action_receiver);
			if (receiver) {
				receiver->receiveAction(action, trigger.second.get());
				performed++;
			}
		}
		return performed;
	}

	//Convenience for shared_ptr insted of raw
	template <typename T>
	int performAction(std::shared_ptr<T>& action) {
		return performAction(action.get());
	}
};

//Universal actions have no geometry and will hit all triggers capable of recieving the action type
//You can override this to add more meta-data to be passed along or to gate who recieves the object by type
//Useful for things like character controllers where the id of the controlled object is known
class UniversalAction : public Action {
public:
	std::priority_queue<
		std::pair<float, std::shared_ptr<ActionTrigger>>,
		std::vector<std::pair<float, std::shared_ptr<ActionTrigger>>>,
		std::greater<std::pair<float, std::shared_ptr<ActionTrigger>>>
	> findTriggers(ActionMap* action_map, Action* action) override {
		std::priority_queue<
			std::pair<float, std::shared_ptr<ActionTrigger>>,
			std::vector<std::pair<float, std::shared_ptr<ActionTrigger>>>,
			std::greater<std::pair<float, std::shared_ptr<ActionTrigger>>>
		> hits ;
		for(auto& [id, trigger] : action_map->triggers){
			if(trigger->context == action->context){
				hits.push({0.0f,trigger}) ;
			}
		}
		return hits ;
	}

	virtual ~UniversalAction() = default;
};

//Ray actions perform a ray-trace against triggers to determine what is intersecting
//You can override this to add more meta-data to be passed along or to gate who recieves the object by type
//Useful primarily for mouse clicks and hovers, but could be useful for hit-scan weapons or other types of pointers
class RayAction : public Action {
public:
	glm::vec3 origin;
	glm::vec3 direction;
	std::priority_queue<
		std::pair<float, std::shared_ptr<ActionTrigger>>,
		std::vector<std::pair<float, std::shared_ptr<ActionTrigger>>>,
		std::greater<std::pair<float, std::shared_ptr<ActionTrigger>>>
	> findTriggers(ActionMap* action_map, Action* action) override {
		std::priority_queue<
			std::pair<float, std::shared_ptr<ActionTrigger>>,
			std::vector<std::pair<float, std::shared_ptr<ActionTrigger>>>,
			std::greater<std::pair<float, std::shared_ptr<ActionTrigger>>>
		> hits;
		for (auto& [id, trigger] : action_map->triggers) {
			if (trigger->context == action->context){
				float t = rayTraceBoundingBox(origin, direction, trigger->min, trigger->max) ;
				if(t >= 0){
					hits.push({t, trigger});
				}
			}
		}
		return hits;
	}
	virtual ~RayAction() = default;
};

//Box action perform an AABB
//You can override this to add more meta-data to be passed along or to gate who recieves the object by type
//Useful primarily for mouse clicks and hovers, but could be useful for hit-scan weapons or other types of pointers
class BoxAction : public Action {
public:
	glm::vec3 min;
	glm::vec3 max;
	std::priority_queue<
		std::pair<float, std::shared_ptr<ActionTrigger>>,
		std::vector<std::pair<float, std::shared_ptr<ActionTrigger>>>,
		std::greater<std::pair<float, std::shared_ptr<ActionTrigger>>>
	> findTriggers(ActionMap* action_map, Action* action) override {
		std::priority_queue<
			std::pair<float, std::shared_ptr<ActionTrigger>>,
			std::vector<std::pair<float, std::shared_ptr<ActionTrigger>>>,
			std::greater<std::pair<float, std::shared_ptr<ActionTrigger>>>
		> hits;
		glm::vec3 action_pos = (min+max)*0.5f ;
		for (auto& [id, trigger] : action_map->triggers) {
			if (trigger->context == action->context && boundingBoxCollision(min,max, trigger->min,trigger->max)) {
				hits.push({glm::distance(action_pos,(trigger->min + trigger->max)*0.5f),trigger});// TODO remove sqrt
			}
		}
		return hits;
	}
	virtual ~BoxAction() = default;
};

// A generic pick up and place ray action
// Held is the item held by the mouse at the beginning of the action, next_held is the item held at the end with -1 assumed to be no item held in eithr slot
class RayGrab : public virtual RayAction {
public:
	int64_t held = -1;
	int64_t next_held = -1;
	int64_t hover = -1 ; // The closest object undr the cursor, regardless of action taken
	float hover_depth = -1 ; // the depth of the hover ray collision origin + direction * hover_depth
	bool clicked = false; // if the mouse was just pressed down thisframe
	bool released = false; // if the mouse was just released this frame
	bool pressed = false;
	int hits = 0 ;

	ActionTrigger* active_item; // Reciever should set this when it is held to be notified on actions in case it no longer intersects
	static inline int RELEASED = 1 ;
	static inline int CLICKED = 2;
	static inline int UPDATED = 3 ;

	void performAction(const glm::vec3& origin, const glm::vec3& direction, bool button_down){
		this->origin = origin ;
		this->direction= direction ;
		clicked = button_down && !pressed ;
		released = !button_down && pressed;
		pressed = button_down ;
		hover = -1 ;
		hover_depth = FLT_MAX ;
		
		//perform action doe the rya cast, alterting everything touched
		hits = getTool<ActionMap>()->performAction(this) ;
		
		//If an item sets held and active itme then ti will receive updates regardless of if it is still intersecting
		if(active_item != nullptr){
			ActionReceiver<RayGrab>* receiver = dynamic_cast<ActionReceiver<RayGrab>*>(active_item->action_receiver);
			if (receiver) {
				if (released) {
					receiver->receiveSignal(RELEASED, this, active_item);
				}else if(clicked) {
					receiver->receiveSignal(CLICKED, this, active_item);
				}else{
					receiver->receiveSignal(UPDATED, this, active_item);
				}
			}
		}


		held = next_held ;
		

	}

	
};

#endif // #ifndef _ACTION_MAP_H_