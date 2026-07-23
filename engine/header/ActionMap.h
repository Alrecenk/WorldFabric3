#ifndef _ACTION_MAP_H_
#define _ACTION_MAP_H_ 1

#include "Utilities.h"

#include <memory>
#include <map>
#include <mutex>


class BaseActionReceiver;
class ActionTrigger;

//This is the base action, you probably don't want to override it directly.
//Your custom actions should probably override a subclass that defines the collision type
// like RayAction, BoxAction, or UniversalAction
class Action {
public:
	int context = 0 ;
	virtual std::vector<std::shared_ptr<ActionTrigger>> findTriggers(ActionMap* action_map, std::shared_ptr<Action>& action) ;
};

//Action triggers have an axis aligned bounding box and a context that is used for their collision with action
//You can override this to add metadata to a trigger that will be accessible in the reciever when actions are caught.
class ActionTrigger{
public:
	int context = 0;
	glm::vec3 min;
	glm::vec3 max;
	int id = -1;
	std::shared_ptr<BaseActionReceiver> action_receiver;

	ActionTrigger() ;
	ActionTrigger(int ctx, const glm::vec3& box_min, const glm::vec3& box_max, std::shared_ptr<BaseActionReceiver>& receiver) : context(ctx),min(box_min),max(box_max), action_receiver(receiver){
	}

	
};

// Outer base class makes it possible to put templated subclasses into one map
class BaseActionReceiver {
public:
	virtual ~BaseActionReceiver() = default;
	//virtual void receiveActionBase(std::shared_ptr<Action>& action, std::shared_ptr<ActionTrigger>& trigger) = 0;
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
	//3) The action reciever overrides Actionreceiver
	virtual void receiveAction(std::shared_ptr<T>& action, std::shared_ptr<ActionTrigger>& trigger) = 0;

};


class ActionMap{
public:	
	std::map<int,std::shared_ptr<ActionTrigger>> triggers ;
	//TODO ActionMap needs like a kd-tree or something so it can search geometry faster when there are a lot of triggers
	int next_trigger_id = 1 ;
	std::mutex lock ;

	int addtrigger(std::shared_ptr<ActionTrigger> trigger){
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

	template <typename T>
	void performAction(std::shared_ptr<T>& action) {
		lock.lock();
		std::vector<std::shared_ptr<ActionTrigger>> triggers = action->findTriggers(this) ;
		for(auto& trigger : triggers){
			std::shared_ptr<ActionReceiver<T>> receiver = dynamic_pointer_cast<ActionReceiver<T>>(trigger->receiver);
			if(receiver){
				receiver->receieveAction(action,trigger) ;
			}
		}
		lock.unlock();
	}
};

//Universal actions have no geometry and will hit all triggers capable of recieving the action type
//You can override this to add more meta-data to be passed along or to gate who recieves the object by type
//Useful for things like character controllers where the id of the controlled object is known
class UniversalAction : Action {
	std::vector<std::shared_ptr<ActionTrigger>> findTriggers(ActionMap* action_map, std::shared_ptr<Action>& action) override {
		std::vector<std::shared_ptr<ActionTrigger>> hits ;
		for(auto& [id, trigger] : action_map->triggers){
			if(trigger->context == action->context){
				hits.push_back(trigger) ;
			}
		}
		return hits ;
	}
}

#endif // #ifndef _ACTION_MAP_H_