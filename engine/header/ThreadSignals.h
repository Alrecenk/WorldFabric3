#ifndef _THREAD_SIGNALS_H_
#define _THREAD_SIGNALS_H_ 1

#include <mutex>
#include <condition_variable>
#include <unordered_map>

class ThreadSignals {
public:
	// Blocks until the given signal has been activated.
	// If the signal is already active, returns immediately.
	void waitFor(int signal) {
		std::unique_lock<std::mutex> lk(signals_lock); // this lock the signlas mutex when it creates
		cv.wait(lk, [&] { return signals[signal]; });// but this immediately unlocks and then only locks it when checking signals
	}

	// Activates the given signal and wakes all waiting threads.
	void signal(int signal) {
		signals_lock.lock();
		signals[signal] = true;
		signals_lock.unlock();
		cv.notify_all(); // wake up every waiting thread to check their conditions
	}

	// Resets the given signal so future waitFor() calls will block again
	void reset(int signal) {
		signals_lock.lock();
		signals[signal] = false;
		signals_lock.unlock();
	}

	//Reset all signals
	void resetAll(){
		signals_lock.lock();
		signals.clear();
		signals_lock.unlock();
	}

	//Set all signals
	void signalAll() {
		signals_lock.lock();
		for(auto& [signal,value] : signals){
			signals[signal] = true;
		}
		signals_lock.unlock();
		cv.notify_all(); // wake up every waiting thread to check their conditions
	}

	void sleep(int millis_to_wait){
		std::this_thread::sleep_for(std::chrono::milliseconds(millis_to_wait));
	}

private:
	std::mutex signals_lock;
	std::condition_variable cv;
	std::unordered_map<int, bool> signals; // default false
};

#endif // #ifndef _THREAD_SIGNALS_H_