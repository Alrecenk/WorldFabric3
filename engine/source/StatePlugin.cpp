#include "StatePlugin.h"
#include "Utilities.h"

StatePlugin::StatePlugin(std::shared_ptr<StateMachine> m) {
	std::shared_ptr<StateMachine> app_machine = std::make_shared<StateMachine>(StateMachine());
	machine = m;
}

StatePlugin::StatePlugin() {
	machine = std::make_shared<StateMachine>(StateMachine());
}

// Called on every plug-in before any plug-ins are run
void StatePlugin::initialize() {
	printf("State plugin initialized.\n");
}

void StatePlugin::run() {
	machine->run();
}


// Adds a new element to the set
	// Returns the state id of the given element 
void StatePlugin::add(std::string name, std::shared_ptr<MachineState> state){
	machine->add(name, state);
}

// remove an element from the set, return it if it was found
std::shared_ptr<MachineState> StatePlugin::remove(std::string name){
	return machine->remove(name) ;
}

// Returns if the given state is present
bool StatePlugin::contains(std::string name){
	return machine->contains(name);
}

// Get a state by id
std::shared_ptr<MachineState> StatePlugin::get(std::string name){
	return machine->get(name);
}

// Sets the state that will run next, overriding what the last state set
//This forces a transition immediately, if you're icalling from in a state you proably want to directly set next_state to switch on next frame
void StatePlugin::setState(std::string next_state){
	machine->setState(next_state) ;
}