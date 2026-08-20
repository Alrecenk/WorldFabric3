#include "AsyncPlugin.h"
#include "Utilities.h"

// wraps the run function on the thread to time it
void AsyncPlugin::runTimed() {
	if(async_enabled && stagger_micros > 0){
		std::this_thread::sleep_for(std::chrono::microseconds(stagger_micros));
	}
	auto start_time = now();
	running = true;
	run();
	running = false;
	run_time += microsBetween(start_time, now());
}


void AsyncPlugin::start(){
	thread = std::thread(&AsyncPlugin::work, this);
	thread.detach() ;
}

void AsyncPlugin::stop(){
	stopped = true ;
	std::unique_lock<std::mutex> lock(signal_mutex);
	cv.notify_all();
}

void AsyncPlugin::work() {
	while(!stopped){
		std::unique_lock<std::mutex> lock(signal_mutex);
		cv.wait(lock, [this] { return ready || stopped ;}); 
		lock.unlock();
		if(stopped){
			ready = false;
			break ;
		}
		
		runTimed();
		ready = false;
	}
	ready = false;
	running = false;
}

void AsyncPlugin::startPlugins(std::vector<std::shared_ptr<AsyncPlugin>>& plugins){
	for (auto& p : plugins) {
		if(p->async_enabled){
			p->start();
		}
	}
}

void AsyncPlugin::stopPlugins(std::vector<std::shared_ptr<AsyncPlugin>>& plugins) {
	for (auto& p : plugins) {
		if (p->async_enabled) {
			p->stop();
			p->cv.notify_one();
		}
	}
	bool any_active = true;
	while(any_active){
		any_active = false ;
		for (auto& p : plugins) {
			any_active |= p->running;
		}
		if (!any_active) {	
			return ;
		}
		std::this_thread::sleep_for(std::chrono::microseconds(500));
	}
}

bool AsyncPlugin::runPlugins(std::vector<std::shared_ptr<AsyncPlugin>>& plugins) {
	// Join everything from the last frame
	bool any_active = false;
	for (auto& p : plugins) {
		any_active |= p->running || p->ready;
		p->cv.notify_one();
	}
	if(any_active){
		//printf("Skipping run because last frame didn't finish!\n");
		std::this_thread::sleep_for(std::chrono::microseconds(500));
		return false;; // TODO wait and try again?
	}

	//Spawn threads for the run functions or just run them depending on async setting
	for (auto& p : plugins) { // Start all the async threads first
		if (p->async_enabled) {
			p->ready = true ;
			p->cv.notify_one();
		}
	}
	
	auto start_time = now();
	for (auto& p : plugins) { // then run the non-async in order on the main thread
		if (!p->async_enabled) {
			p->runTimed();
		}
	}
	return true ;
}

// call to start an input chain
int AsyncPlugin::inputStart(){
	int num = current_input++ ;
	input_time[num] =  now() ;
	input_time.erase(input_time.begin(),input_time.lower_bound(num-inputs_held)) ;
	return num ;
}

// call to print print the delay result of a completed input_chain
void AsyncPlugin::inputDisplay(int num, int phase, bool last){
	if(num > 0 && displayed_input.find(num) == displayed_input.end()){
		
		printf("input: %d  phase: %d, Delay: %ld\n", num, phase, microsBetween(input_time[num], now())) ;
		
		if (last){
			displayed_input.insert(num);
			std::vector<int> to_delete ;
			for(auto& old_num : displayed_input){
				if(old_num < num - inputs_held){
					to_delete.push_back(old_num) ;
				}
			}
			for(auto& old_num : to_delete){
				displayed_input.erase(old_num) ;
			}
		}
			
	}
}
