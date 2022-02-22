/**
 * Implementation of the BoundedBuffer class.
 * See the associated header file (BoundedBuffer.hpp) for the declaration of
 * this class.
 */
#include <cstdio>

#include "BoundedBuffer.hpp"

/**
 * Constructor that sets capacity to the given value. The buffer itself is
 * initialized to en empty queue.
 *
 * @param max_size The desired capacity for the buffer.
 */
BoundedBuffer::BoundedBuffer(int max_size) {
	capacity = max_size;

	// buffer field implicitly has its default (no-arg) constructor called.
	// This means we have a new buffer with no items in it.
}

/**
 * Gets the first item from the buffer then removes it.
 */
int BoundedBuffer::getItem() {
	int item = this->buffer.front(); // "this" refers to the calling object...
	buffer.pop(); // ... but like Java it is optional (no this in front of buffer on this line)
	return item;
}

/**
 * Adds a new item to the back of the buffer.
 *
 * @param new_item The item to put in the buffer.
 */
void BoundedBuffer::putItem(int new_item) {
	buffer.push(new_item);
}
