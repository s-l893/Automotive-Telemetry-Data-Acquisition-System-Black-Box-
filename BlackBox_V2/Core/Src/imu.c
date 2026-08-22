/*
 * imu.c
 *
 *  Created on: Aug 18, 2026
 *      Author: Sunny Lin
 */

#include "imu.h"
#include "fault.h"
#include "i2c.h"
#include "iwdg.h"
#include <stdbool.h>
#include <stdint.h>
#include "main.h"

#define IMU_ADDR 0x68
#define WAKE_REG 0x6B
#define WRITE_ADDRESS (IMU_ADDR << 1)

imu_frame imu;
imu_calibration imu_offset;
uint8_t imu_who_am_i = 0;

void imu_read(void){
	if (!fault_flags.imu_fault && !fault_flags.imu_handshake_fault ){
		uint8_t buffer[6];
		HAL_StatusTypeDef accel_status =  HAL_I2C_Mem_Read(&hi2c1, WRITE_ADDRESS, 59, I2C_MEMADD_SIZE_8BIT, buffer, 6, 100);
		if (accel_status != HAL_OK){
			fault_flags.imu_fault = true;
			return;
		}
		imu.accel_x = (int16_t)((buffer[0] << 8) | buffer[1]);
		imu.accel_y = (int16_t)((buffer[2] << 8) | buffer[3]);
		imu.accel_z = (int16_t)((buffer[4] << 8) | buffer[5]);
		imu.accel_x -= imu_offset.offset_x;
		imu.accel_y -= imu_offset.offset_y;
		imu.accel_z -= imu_offset.offset_z;
		imu.timestamp = HAL_GetTick();
	}
}

void imu_calibrate(void){ // ZERO CALIBRATION UPON START
	int32_t sum_x = 0;
	int32_t sum_y = 0;
	int32_t sum_z = 0;
	if ((fault_flags.imu_fault == false) && (fault_flags.imu_handshake_fault == 0)){

		for (int i = 0; i < 50; i++){
			imu_read();
			sum_x += imu.accel_x;
			sum_y += imu.accel_y;
			sum_z += imu.accel_z;
			HAL_IWDG_Refresh(&hiwdg);
			HAL_Delay(20);
		}

		imu_offset.offset_x = (int16_t)(sum_x / 50);
		imu_offset.offset_y = (int16_t)(sum_y / 50);
		imu_offset.offset_z = (int16_t)(sum_z / 50);
	}
}

void imu_init(void){

	uint8_t sleep_bit = 0x00;
	imu_who_am_i = 0;
	HAL_StatusTypeDef imu_status = HAL_I2C_Mem_Write(&hi2c1, WRITE_ADDRESS, WAKE_REG, I2C_MEMADD_SIZE_8BIT, &sleep_bit, 1, 100);
	if (imu_status != HAL_OK){
		fault_flags.imu_fault = true;
	}
	HAL_StatusTypeDef imu_handshake = HAL_I2C_Mem_Read(&hi2c1, WRITE_ADDRESS, 117, I2C_MEMADD_SIZE_8BIT, &imu_who_am_i, 1, 100);
	if (imu_handshake != HAL_OK){
		fault_flags.imu_fault = true;
	}
	if (imu_who_am_i != 0x68){
		fault_flags.imu_handshake_fault = true;
	}
	imu_calibrate();
}
