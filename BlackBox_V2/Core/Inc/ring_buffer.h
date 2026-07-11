/*
 * ring_buffer.h
 *
 *  Created on: Jul 7, 2026
 *      Author: Sunny Lin
 */

#ifndef INC_RING_BUFFER_H_
#define INC_RING_BUFFER_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
	uint8_t *buffer;
	uint16_t head;
	uint16_t tail;
	uint16_t count;
	uint16_t capacity;
	uint16_t dropped_count;
} ring_buffer_t;


void RingBuffer_Init(volatile ring_buffer_t*, uint16_t, uint8_t*);
bool RingBuffer_Push(volatile ring_buffer_t*, uint8_t);
bool RingBuffer_Pop(volatile ring_buffer_t*, uint8_t*);




#endif /* INC_RING_BUFFER_H_ */
