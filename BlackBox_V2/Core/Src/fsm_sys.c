/*
 * fsm_sys.c
 *
 *  Created on: Jul 7, 2026
 *      Author: Sunny Lin
 *
 */

#include "can_handler.h"
#include "sd_logger.h"
#include "fault.h"
#include "fsm_sys.h"
#include <stdbool.h>
#include <stdint.h>

#define IDLE_SHUTDOWN_TIMEOUT_MS 300000
// VARIABLE DECLARATION
static bool shutdown_complete = false;

// FSM STRUCT DECLARED IN HEADER

sys_state_t current_state = SYS_INIT; // SET INITIAL STATE

void SYS_FSM_TICK(void){
	uint32_t can_timer;

	switch (current_state){
	case SYS_INIT:
		if (sd_mount && peripherals_init){
			current_state = SYS_IDLE;
		}
		else if (!sd_mount){
			fault_flags.sd_fault = true;
			current_state = SYS_FAULT;
		}
	break;
	case SYS_IDLE:
		if (can_frame_received_flag){
			current_state = SYS_LOGGING;
			start_new_session_file();
			activity = HAL_GetTick();
			last_can_frame = HAL_GetTick();
		}
		else{
			uint32_t idle_timer = HAL_GetTick();
			if ((idle_timer - last_can_frame) >= IDLE_SHUTDOWN_TIMEOUT_MS){
				current_state = SYS_SHUTDOWN;
			}
		}
		break;
	case SYS_LOGGING:
		if (can_frame_received_flag){
			last_can_frame = HAL_GetTick();
		}
		else{
			can_timer = HAL_GetTick(); // NON BLOCKING ARCHITECTURE
			if ((can_timer - last_can_frame) >= 2500){ // 2.5S SILENCE TIMEOUT FEATURE
				close_session_file();
				current_state = SYS_IDLE;
			}
		}
		break;
	case SYS_FAULT: // MIGHT WANT TO ADD SOMETHING HERE FOR CAN LATER ON
		sd_recovery(); // ATTEMPT TO RETRY SD MOUNT
		if (sd_mount){
			current_state = SYS_IDLE;
		}
		if ((idle_timer - last_can_frame) >= IDLE_SHUTDOWN_TIMEOUT_MS){

			current_state = SYS_SHUTDOWN;
		}
		break;
	case SYS_SHUTDOWN:
		if (!shutdown_complete){
			flush_ring_buffers(); // CLEAR CIRCULAR BUFFERS
			close_session_file(); // CLEANLY EXIT SD CARD FILE
			unmount_sd();
			shutdown_complete = true;
		}
		break;
		// FOLLOWING THIS BOARD WILL LOSE POWER
	default: // UNDEFINED BEHAVIOR
		current_state = SYS_FAULT;
	}
}
