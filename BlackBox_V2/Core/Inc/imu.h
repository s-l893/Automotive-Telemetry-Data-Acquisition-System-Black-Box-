/*
 * imu.h
 *
 *  Created on: Aug 18, 2026
 *      Author: Sunny Lin
 */

#ifndef INC_IMU_H_
#define INC_IMU_H_

#include <stdint.h>
#include "stm32f4xx_hal.h"

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
extern HAL_StatusTypeDef imu_who_read_status;
extern HAL_StatusTypeDef imu_wake_write_status;
extern HAL_StatusTypeDef imu_accel_read_status;

void imu_init(void);
void imu_read(void);
void imu_calibrate(void);

#endif /* INC_IMU_H_ */
