/*
 * can_ring_buffer.h
 *
 *  Created on: Jul 10, 2026
 *      Author: Sunny Lin
 */

#ifndef SRC_CAN_RING_BUFFER_H_
#define SRC_CAN_RING_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t id;
    uint8_t dlc;        // data length, 0-8
    uint8_t data[8];
    uint32_t timestamp; // when it was received, useful for logging
} can_frame_t;

typedef struct {
	can_frame_t *buffer;
	uint16_t head;
	uint16_t tail;
	uint16_t count;
	uint16_t capacity;
	uint16_t dropped_count;
} can_ring_buffer_t;


void CANRingBuffer_Init(volatile can_ring_buffer_t*, uint16_t, can_frame_t*);
bool CANRingBuffer_Push(volatile can_ring_buffer_t*, can_frame_t);
bool CANRingBuffer_Pop(volatile can_ring_buffer_t*, can_frame_t*);



#endif /* SRC_CAN_RING_BUFFER_H_ */
