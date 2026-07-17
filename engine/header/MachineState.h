#ifndef _MACHINE_STATE_H_
#define _MACHINE_STATE_H_ 1

#include "Utilities.h"
#include <string>
#include <memory>

class StateMachine;

class MachineState {
public:
    std::string name;
    std::string next_state; // tells the state machine holding this which state to run next
	std::chrono::high_resolution_clock::time_point enter_time ;
    // Runs the state
    virtual void run() = 0;

    // Called when switching into this sate before the first time run is claled
    virtual void enter(std::shared_ptr<MachineState> from) = 0;

    // Called when switching outof this state after the last time run is called
    virtual void exit(std::shared_ptr<MachineState> to) = 0;


	//return the time since entering state in milliseconds
	long millisInState(){
		return millisBetween(enter_time, now()) ;
	}

private:
    
    
};
#endif // #ifndef _MACHINE_STATE_H_
