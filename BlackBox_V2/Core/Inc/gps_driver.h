/*
 * gps_driver.h
 *
 *  Created on: Jul 24, 2026
 *      Author: Sunny Lin
 */

#ifndef GPS_DRIVER_H_
#define GPS_DRIVER_H_

#include <stdbool.h>

typedef struct {
	bool locked;
	float latitude;
	float longitude;
	float speed; // km/h
} gps_data_t;

extern gps_data_t gps;

void GPS_Driver_Init(void);
void GPS_Driver_Update(void);

#endif /* GPS_DRIVER_H_ */
