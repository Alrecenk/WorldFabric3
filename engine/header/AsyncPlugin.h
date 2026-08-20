#ifndef _ASYNC_PLUGIN_H_
#define _ASYNC_PLUGIN_H_ 1

#include <thread>
#include <mutex>
#include <vector>
#include "Utilities.h" // for now() on time initializations
#include <map>
#include <unordered_set>

class AsyncPlugin{
public:
    std::thread thread ;
    std::recursive_mutex lock;
    // accumulating microseconds of function executions
    long run_time=0 ;
	int stagger_micros = 0;
	std::chrono::high_resolution_clock::time_point last_run_time = now();
	

	
	//Timings of when input reaches a plugin
	static inline int current_input = 1 ;
	static inline int inputs_held = 1000 ;
	static inline std::map<int, std::chrono::high_resolution_clock::time_point> input_time;
	static inline std::unordered_set<int> displayed_input ;

    // Called on every plug-in befoe any plug-ins are run
    virtual void initialize() = 0;

    // Runs the plugin asynchronously
    virtual void run() = 0;

    // wraps the run function on the thread to time it
    void runTimed();

    bool async_enabled = true; // whether this plugin will block the main prep thread with run or spawn its own thread for running

	bool ready = false;// whether wants to execture run
	bool running = false; // whether currently executingrun
	bool stopped = false; // whether wants to stop

	std::mutex signal_mutex;
	std::condition_variable cv;

	void start();
	void work();
	void stop();


	static void startPlugins(std::vector<std::shared_ptr<AsyncPlugin>>& plugins);

	static void stopPlugins(std::vector<std::shared_ptr<AsyncPlugin>>& plugins);

	// Run all plugins for a frame
	static bool runPlugins(std::vector<std::shared_ptr<AsyncPlugin>>& plugins);

	static inline const std::string SHUTDOWN_FLAG = "shut_down" ;

	//Debug functions for tracking input latency
	static int inputStart(); // call to start an input chain
	static void inputDisplay(int num, int phase, bool last = true ); // call to print the delay

	
};
#endif // #ifndef _ASYNC_PLUGIN_H_