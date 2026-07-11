/*
 * can_ring_buffer.c
 *
 *  Created on: Jul 10, 2026
 *      Author: Sunny Lin
 */

// SECONDARY RING BUFFER WRITTEN TO PERMIT USABILITY FOR LARGER SIZED CAN FRAMES
// LOGIC REMAINS SAME

#include <stdbool.h>
#include <stdint.h>
#include "can_ring_buffer.h"


void CANRingBuffer_Init(volatile can_ring_buffer_t *rb, uint16_t capacity, can_frame_t *buffer){
	rb->head = 0;
	rb->tail = 0;
	rb->count = 0;
	rb->dropped_count = 0;
	rb->buffer = buffer;
	rb->capacity = capacity;
}
bool CANRingBuffer_Push(volatile can_ring_buffer_t *rb, can_frame_t data){
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
bool CANRingBuffer_Pop(volatile can_ring_buffer_t *rb, can_frame_t *data_out){
	if (rb->count == 0){
		return false;
	}
	*data_out = rb->buffer[rb->tail];
	rb -> tail = (rb->tail + 1) % rb->capacity;
	rb -> count--;
	return true;
}
