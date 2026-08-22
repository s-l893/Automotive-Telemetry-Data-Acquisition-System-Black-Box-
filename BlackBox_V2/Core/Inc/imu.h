/*
 * imu.h
 *
 *  Created on: Aug 18, 2026
 *      Author: Sunny Lin
 */
#include <stdint.h>

typedef struct {
	int16_t accel_x;
	int16_t accel_y;
	int16_t accel_z;
	int32_t timestamp;
} imu_frame;

imu_frame imu;

typedef struct {
	int16_t offset_x;
	int16_t offset_y;
	int16_t offset_z;
} imu_calibration;

void imu_init(void);

void imu_read(void);

void imu_calibrate(void);


