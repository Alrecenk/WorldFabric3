#ifndef _TRACE_APP_H_
#define _TRACE_APP_H_ 1

#include "AsyncPlugin.h"
#include "MachineState.h"

class TraceApp : public MachineState {

public:

	static inline const std::string state_name = "trace_demo_state";

	TraceApp();

	//Called every frame while the state is active
	void run() override;

	// Called when switching into this state before the first time run is called
	void enter(std::shared_ptr<MachineState> from) override;

	// Called when switching out of this state after the last time run is called
	void exit(std::shared_ptr<MachineState> to) override;

private:

	int pawn_id;
	float current_angle = 0;
	int light_effect_id = -1;
	int mouse_particle_id = -1;
	std::chrono::high_resolution_clock::time_point last_run_time;
	std::chrono::high_resolution_clock::time_point current_time;
};
#endif // #ifndef _TRACE_APP_H_