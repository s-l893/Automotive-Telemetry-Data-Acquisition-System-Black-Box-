/*
 * fault.h
 *
 *  Created on: Jul 7, 2026
 *      Author: Sunny Lin
 */

#ifndef INC_FAULT_H_
#define INC_FAULT_H_
#include <stdbool.h>

	// FAULTS
typedef struct {
	bool sd_fault; // HARD FAULT
	bool gps_fault; // SOFT FAULT
	bool imu_fault; // SOFT FAULT
	bool can_fault; // HARD FAULT
} fault_flags_t;

extern fault_flags_t fault_flags;

#endif /* INC_FAULT_H_ */
