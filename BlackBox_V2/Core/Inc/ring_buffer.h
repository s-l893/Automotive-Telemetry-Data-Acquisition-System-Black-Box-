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

void RingBuffer_Init(void);
bool RingBuffer_Push(bool);
bool RingBuffer_Pop(bool);



#endif /* INC_RING_BUFFER_H_ */
