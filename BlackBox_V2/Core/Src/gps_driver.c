/*
 * gps_driver.c
 *
 *  Created on: Jul 24, 2026
 *      Author: Sunny Lin
 */
#include "gps_driver.h"
#include "main.h"
#include <stdbool.h>

char nmea_buffer[85];
char nmea_parse_buffer[85];

volatile uint16_t nmea_index = 0;
volatile bool nmea_line_ready = false;
static uint8_t rx_byte;

typedef struct {
	bool locked;
	float latitude;
	float longitude;
	float speed;
} gps_data_t;

gps_data_t gps;

void GPS_Driver_Init(void){
	gps.locked = false;
	gps.speed = 0.0;
	gps.latitude = 0.0;
	gps.longitude = 0.0;
	nmea_index = 0;
	HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

void GPS_Driver_Update(void){
	if (nmea_line_ready){
		rmc_splitter();
	}
}

float parse_lat_long(char* field, char hemisphere, int degree_digits){
	char deg_str[4]; // 3 DIGITS + TERMINATOR
	strncpy(deg_str, field, degree_digits);
	deg_str[degree_digits] = '\0';

	float degrees = atof (deg_str); // convert to float
	float minutes = atof(fields + degree_digits);

	float decimal = degrees + (minutes / 60.0); // formula to convert degrees and minutes into decimal degree

	if (hemisphere == 'S' || hemisphere == 'W'){ // prevent (-)s
		decimal = -decimal;
	}
	return decimal;
}

void rmc_splitter(void){
	char *status;
	char *valid = "A";
//	float time;
	char *lat_hemi;
	char *long_hemi;
	char *lat_field;
	char *long_field;
	char *date;
	float speed;

	strtok(nmea_parse_buffer, ","); // FIELD 0
	//time = strtok(NULL, ","); //FIELD 1 (time)
	status = strtok(NULL, ","); // FIELD 2 (STATUS)
	if (!(strcmp(status, valid))){
		gps.locked = true;

		lat_field = strtok(NULL, ','); // FIELD 3 (LAT)
		lat_hemi = strtok(NULL, ','); // FIELD 4 (LAT HEMI)
		float latitude = parse_lat_long(lat_field,lat_hemi[0], 2);

		long_field = strtok (NULL, ','); // FIELD 5 (LONG)
		long_hemi = strtok(NULL, ','); // FIELD 6 (LONG HEMI)
		float longitude = parse_lat_long (long_field, long_hemi[0], 3);

		gps.latitude = latitude; // UPDATE STRUCT
		gps.longitude = longitude;

		speed = atof(strtok(NULL, ',')); // FIELD 7
		speed = speed * 1.852; // KNOTS TO KM/H
		gps.speed = speed;

		strtok(NULL, ','); // FIELD 8
		date = strtok(NULL, ','); // FIELD 9

	}
	else{
		gps.locked = false;
	}
	nmea_line_ready = false;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if (nmea_index < 85){ // prevent buffer overflow
		if (rx_byte == '\n'){ // check for end of bytes
			nmea_buffer[nmea_index] = '\0'; // manually add NTERMINATOR
			memcpy(nmea_parse_buffer,nmea_buffer, nmea_index + 1); // +1 for terminator
			nmea_line_ready = true;
			nmea_index = 0; // reset buffer w index

		}
		else{
			nmea_buffer[nmea_index] = rx_byte;
			nmea_index++;
		}
	}

	HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}
