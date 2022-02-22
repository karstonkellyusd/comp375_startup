#include <cstdio>
#include <cstdlib>
#include <pthread.h>

#include <thread>
#include <chrono>	// for times (e.g. seconds)

#include "BoundedBuffer.hpp"

const size_t BUFFER_CAPACITY = 10;
const size_t NUM_CONSUMERS = 5;

/**
 * Function run by a consumer.
 *
 * @param buffer A bounded buffered, shared amongst several threads.
 */
void consume(BoundedBuffer &buffer) {
	printf("Starting a consumer\n");

	// Consume a value from the buffer every 0 to 9 seconds.
	while (true) {
		std::chrono::seconds sleep_time(rand() % 10);
		std::this_thread::sleep_for(sleep_time);

		int item = buffer.getItem();
		printf("Consumed: %d\n", item);
	}
}

/**
 * Function run by a producer.
 *
 * @param buffer A bounded buffered, shared amongst several threads.
 */
void produce(BoundedBuffer &buffer) {
	printf("Starting a producer\n");

	// Produce a random value between 1 and 100 every 0 to 2 seconds, adding
	//  it to the buffer.
	while (true) {
		std::chrono::seconds sleep_time(rand() % 3);
		std::this_thread::sleep_for(sleep_time);

		int new_item = rand()%100 + 1;

		printf("Produced: %d\n", new_item);
		buffer.putItem(new_item);
	}
}

int main() {
	BoundedBuffer buff(BUFFER_CAPACITY);

	// create only a single producer thread
	std::thread producer(produce, std::ref(buff));

	// create a pool of consumer threads
	for (size_t i = 0; i < NUM_CONSUMERS; ++i) {
		std::thread consumer(consume, std::ref(buff));

		// let the consumers run without us waiting to join with them
		consumer.detach();
	}

	producer.join(); 	// Wait for producer to finish.
						// This is like waiting for Godot though.
}
