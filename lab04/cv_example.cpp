#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

using std::cout;

std::condition_variable cv;
bool ready = false;

/**
 * Waits for ready to be true before exiting.
 * Another thread will be the one to set ready and notify us when that happens.
 * 
 * @param m Shared mutex to use with a lock.
 */
void waitForReady(std::mutex &m) {
	// Step 1: Acquire the mutex lock via a unique lock
	//  If the mutex is already locked, we will wait here until it is unlocked.
	std::unique_lock<std::mutex> cv_lock(m);

	// Step 2: If not ready, wait
	while (!ready) {
		cout << "Worker: Not ready yet... waiting!\n";
		cv.wait(cv_lock); // gives up the lock while we wait
		// after returning from wait, we own the lock again
	}

	// Step 3: We're ready to move on now!
	cout << "Worker: Done waiting!\n";

	// cv_lock is a local variable so it gets "deconstructed" before we return.
	// Deconstruction gives up the lock.
}

int main() {
	// Create a mutex to allow for mutually exclusive access to a lock (i.e.
	// only one thread will be able to lock the lock at a time).
	std::mutex shared_mutex;

	// Create a "worker" thread, and have it run the waitForReady function
	// (defined above). The parameter we will use to call the waitForReady
	// function is given as the second parameter to thread's constructor.
	std::thread worker(waitForReady, std::ref(shared_mutex));

	cout << "Main: Going to sleep for 3 seconds.\n";
	std::this_thread::sleep_for(std::chrono::seconds(3));

	// Create a lock, so we don't try to access "ready" at the same time
	//  as the worker thread.
	std::unique_lock<std::mutex> lk(shared_mutex); // wait here until shared_mutex is unlocked
	ready = true;

	cout << "Main: Notifying the worker...\n";
	cv.notify_one();
	lk.unlock(); // Give up the lock so that the worker can acquire it after waiting

	// Wait here for the worker to finish.
	worker.join();
}

