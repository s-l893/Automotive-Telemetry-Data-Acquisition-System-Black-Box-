/*
 * gps_driver.h
 *
 *  Created on: Jul 24, 2026
 *      Author: Sunny Lin
 */

#ifndef GPS_DRIVER_H_
#define GPS_DRIVER_H_

#include <stdbool.h>
#include <stdint.h>

typedef struct {
	bool locked;
	bool rtc_synced; // true after gps lock for file naming purposes
	float latitude;
	float longitude;
	float speed; // km/h converted from knots
} gps_data_t;

extern gps_data_t gps;

void GPS_Driver_Init(void);
void GPS_Driver_Update(void);

#endif
