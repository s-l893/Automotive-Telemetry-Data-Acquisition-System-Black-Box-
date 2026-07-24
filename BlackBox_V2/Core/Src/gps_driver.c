/*
 * gps_driver.c
 *
 *  Created on: Jul 24, 2026
 *      Author: Sunny Lin
 */
#include "gps_driver.h"
#include <stdbool.h>


typedef struct {
	bool locked;
	float latitude;
	float longitude;
	float speed;
} gps_data_t;

gps_data_t gps;

void GPS_Driver_Init(void){
	gps.locked = false;
	static float gps_start = HAL_GetTick();
	gps.speed = 0.0;
	gps.latitude = 0.0;
	gps.longitude = 0.0;
}

void GPS_Driver_Update(void){
	if (gps.locked){

	}
}
