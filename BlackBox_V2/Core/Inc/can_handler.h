/*
 * can_handler.h
 *
 *  Created on: Jul 9, 2026
 *      Author: Sunny Lin
 */

#ifndef INC_CAN_HANDLER_H_
#define INC_CAN_HANDLER_H_

#include "can_ring_buffer.h"
#include <stdbool.h>

extern volatile can_ring_buffer_t can_rb;
extern volatile bool can_frame_received_flag;
extern volatile uint32_t last_can_frame;

void can_handler_init(void);

void CAN_Handler_RecoverBusOff(void);

#endif /* INC_CAN_HANDLER_H_ */
