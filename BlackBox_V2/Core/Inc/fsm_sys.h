/*
 * fsm_sys.h
 *
 *  Created on: Jul 7, 2026
 *      Author: Sunny Lin
 */

#ifndef SRC_FSM_SYS_H_
#define SRC_FSM_SYS_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
	SYS_INIT,	// PERIPHERAL INIT ATTEMP
	SYS_IDLE, // POWER ON, WAITING FOR CAN
	SYS_LOGGING, // FIRST CAN FRAME RECEIVED
	SYS_FAULT, // FAULT WITH SOMETHING
	SYS_SHUTDOWN // SHUTDOWN SEQUENCE
} sys_state_t;


void SYS_FSM_TICK(void);
void SD_Logger_DrainCAN(void);

extern sys_state_t current_state;

#endif /* SRC_FSM_SYS_H_ */
