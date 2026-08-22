/*
 * imu.h
 *
 *  Created on: Aug 18, 2026
 *      Author: Sunny Lin
 */

#ifndef INC_IMU_H_
#define INC_IMU_H_

#include <stdint.h>

typedef struct {
	int16_t accel_x;
	int16_t accel_y;
	int16_t accel_z;
	int32_t timestamp;
} imu_frame;

typedef struct {
	int16_t offset_x;
	int16_t offset_y;
	int16_t offset_z;
} imu_calibration;

extern imu_frame imu;
extern imu_calibration imu_offset;
extern uint8_t imu_who_am_i;

void imu_init(void);
void imu_read(void);
void imu_calibrate(void);

#endif /* INC_IMU_H_ */
