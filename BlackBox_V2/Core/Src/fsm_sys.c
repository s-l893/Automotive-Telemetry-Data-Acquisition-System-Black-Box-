/*
 * fsm_sys.c
 *
 *  Created on: Jul 7, 2026
 *      Author: Sunny Lin
 *
 */

// VARIABLE DECLARATION
static bool shutdown_complete = false;



// FSM DECLARATION
	// LOGGING LIFECYCLE
typedef enum {
	SYS_INIT,	// PERIPHERAL INIT ATTEMP
	SYS_IDLE, // POWER ON, WAITING FOR CAN
	SYS_LOGGING, // FIRST CAN FRAME RECEIVED
	SYS_FAULT, // FAULT WITH SOMETHING
	SYS_SHUTDOWN // SHUTDOWN SEQUENCE
} sys_state_t;

	// FAULTS
typedef struct {
	bool sd_fault; // HARD FAULT
	bool gps_fault; // SOFT FAULT
	bool imu_fault; // SOFT FAULT
	bool can_fault; // HARD FAULT
} fault_flags_t;

sys_state_t current_state = SYS_INIT; // SET INITIAL STATE
fault_flags_t fault_flags;

void SYS_FSM_TICK(){
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
			last_can_frame = HAL_GetTick();
		}
		if (ignition_off_detected){
			current_state = SYS_SHUTDOWN;
		}
		break;
	case SYS_LOGGING:
		if (can_frame_received_flag){
			last_can_frame = HAL_GetTick();
		}
		else{
			can_timer = HAL_GetTick(); // NON BLOCKING ARCHTECTURE
			if ((can_timer - last_can_frame) >= 2500){ // 2.5S SILENCE TIMEOUT FEATURE
				current_state = SYS_IDLE;
				close_session_file();
			}
		if (ignition_off_detected){
			current_state = SYS_SHUTDOWN;
		}
		}
		break;
	case SYS_FAULT: // MIGHT WANT TO ADD SOMETHING HERE FOR CAN LATER ON
		sd_recovery(); // ATTEMPT TO RETRY SD MOUNT
		if (sd_mount){
			current_state = SYS_IDLE;
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
