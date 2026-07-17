#include "ViewPlugin.h"
#include "FlagSet.h"

ViewPlugin::ViewPlugin(){

}

void ViewPlugin::initialize(){

}

void ViewPlugin::run(){
	//std::this_thread::sleep_for(std::chrono::microseconds(simulated_lag_micros));
	if(getTool<FlagSet>()->getInt(AsyncPlugin::SHUTDOWN_FLAG) == 0){
		getTool<WorldPlugin>()->view() ;
	}
}