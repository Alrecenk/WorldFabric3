#ifndef _STATE_MACHINE_H_
#define _STATE_MACHINE_H_ 1

#include "MachineState.h"

#include <string>
#include <memory>
#include <unordered_map>


class StateMachine {

public:

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

	void run();

private:
	
	std::unordered_map<std::string, std::shared_ptr<MachineState>> states;
	std::string current_state ="";
	
};
#endif // #ifndef _STATE_MACHINE_H_