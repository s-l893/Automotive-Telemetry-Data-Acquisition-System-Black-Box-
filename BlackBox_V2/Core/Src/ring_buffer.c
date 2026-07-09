/*
 * ring_buffer.c
 *
 *  Created on: Jul 7, 2026
 *      Author: Sunny Lin
 */

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	uint8_t *buffer;
	uint16_t head;
	uint16_t tail;
	uint16_t count;
	uint16_t capacity;
	uint16_t dropped_count;
} ring_buffer_t;


void RingBuffer_Init(volatile ring_buffer_t *rb, uint16_t capacity, uint8_t *buffer){
	rb->head = 0;
	rb->tail = 0;
	rb->count = 0;
	rb->dropped_count = 0;
	rb->buffer = buffer;
	rb->capacity = capacity;
}
bool RingBuffer_Push(volatile ring_buffer_t *rb, uint8_t data){
	rb -> buffer[rb->head] = data;
	rb->head = (rb->head + 1) % rb->capacity;
	if (rb->count == rb->capacity){
		rb -> tail = (rb -> tail + 1) % rb->capacity;
		rb->dropped_count++;
	}
	else{
		rb->count++;
	}
	return true;
}
bool RingBuffer_Pop(volatile ring_buffer_t *rb, uint8_t *data_out){
	if (rb->count == 0){
		return false;
	}
	*data_out = rb->buffer[rb->tail];
	rb -> tail = (rb->tail + 1) % rb->capacity;
	rb -> count--;
	return true;
}



