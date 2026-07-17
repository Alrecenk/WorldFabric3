#include "StateMachine.h"

// Adds a new element to the set
// Returns the state id of the given element 
void StateMachine::add(std::string name, std::shared_ptr<MachineState> state) {
	states[name] = state;
}

// remove an element from the set, return it if it was found
std::shared_ptr<MachineState> StateMachine::remove(std::string name) {
	auto state = states[name];
	states.erase(name);
	return state ;
}

// Returns if the given state is present
bool StateMachine::contains(std::string name) {
	return states.find(name) != states.end();
}

// Get a state by id
std::shared_ptr<MachineState> StateMachine::get(std::string name) {
	return states[name];
}

// Stes the state that will run next, overriding what the last state set
void StateMachine::setState(std::string next_state) {
	if (contains(next_state)) {
		std::shared_ptr<MachineState> next = states[next_state];
		std::shared_ptr<MachineState> current;
		if (contains(current_state)) {
			current = states[current_state];
			current->exit(next);
		}
		next->next_state = next_state;
		next->enter_time = now();
		next->enter(current);
		current_state = next_state;
	}
}

void StateMachine::run() {
	//TODO could reuse some of the finds if we didn't use contains here
	//printf("running machine.. %s\n", current_state.c_str());
	if (contains(current_state)) {
		std::shared_ptr<MachineState> current = states[current_state];
		current->run();
		if (current->next_state != current_state && contains(current->next_state)) {
			//printf("switching state to.. %s\n", current->next_state.c_str());
			std::shared_ptr<MachineState> next = states[current->next_state];
			current->exit(next);
			next->next_state = current->next_state; // state stays in itself unless explicitly changed
			next->enter_time = now();
			next->enter(current);
			current_state = current->next_state;
		}
	}

}