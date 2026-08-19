/*
 * imu.c
 *
 *  Created on: Aug 18, 2026
 *      Author: Sunny Lin
 */

#include "imu.h"
#include "fault.h"
#include "fsm_sys.h"
#include <stdbool.h>
#include <stdint.h>
#include "main.h"

#define IMU_ADDR 0x68
#define WAKE_REG 0x6B


void imu_init(){

	uint8_t write_address = IMU_ADDR << 1;
	uint8_t sleep_bit = 0x00;
	uint8_t who_bit;
	HAL_StatusTypeDef imu_status = HAL_I2C_Mem_Write(&hi2c1, write_address, 107, I2C_MEMADD_SIZE_8BIT, &sleep_bit, 1, 100);
	if (imu_status != HAL_OK){
		fault_flags.imu_fault = true;
	}
	HAL_StatusTypeDef imu_handshake = HAL_I2C_Mem_Read(&hi2c1, write_address, 117, I2C_MEMADD_SIZE_8BIT, &who_bit, 1, 100);
	if (imu_handshake != HAL_OK){
		fault_flags.imu_fault = true;
	}
	if (who_bit != 0x68){
		fault_flags.imu_handshake_fault = true;
	}
}

