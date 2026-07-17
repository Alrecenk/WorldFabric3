#ifndef _STATE_PLUGIN_H_
#define _STATE_PLUGIN_H_ 1

#include "AsyncPlugin.h"
#include "stateMachine.h"

class StatePlugin: public AsyncPlugin {

public:
	static inline std::string tag = "StateLink";
	std::shared_ptr<StateMachine> machine;

	StatePlugin(std::shared_ptr<StateMachine> m);

	StatePlugin();

	// Called on every plug-in before any plug-ins are run
	void initialize() override;

	void run() override;

	// Adds a new element to the set
	// Returns the state id of the given element 
	void add(std::string name, std::shared_ptr<MachineState> state);

	// remove an element from the set, return it if it was found
	std::shared_ptr<MachineState> remove(std::string name);

	// Returns if the given state is present
	bool contains(std::string name);

	// Get a state by id
	std::shared_ptr<MachineState> get(std::string name);

	// Stes the state that will run next, overriding what the last state set
	void setState(std::string next_state);


private:
	

};
#endif // #ifndef _STATE_PLUGIN_H_
