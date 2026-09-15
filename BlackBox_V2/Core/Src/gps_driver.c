/*
 * gps_driver.c
 *
 *  Created on: Jul 24, 2026
 *      Author: Sunny Lin
 */
#include "gps_driver.h"
#include "main.h"
#include "fault.h"
#include "usart.h"
#include <stdlib.h>
#include <string.h>

#define NMEA_BUF_LEN 85
#define GPS_TIMEOUT 1500

static char nmea_buffer[NMEA_BUF_LEN];
static char nmea_parse_buffer[NMEA_BUF_LEN];
static volatile uint16_t nmea_index = 0;
static volatile bool nmea_line_ready = false;
static uint8_t rx_byte;
static volatile uint32_t last_gps_activity_tick = 0;

gps_data_t gps;

static float parse_lat_long(char *field, char hemisphere, int degree_digits);
static void rmc_splitter(void);

void GPS_Driver_Init(void){
	gps.locked = false;
	gps.speed = 0.0f;
	gps.latitude = 0.0f;
	gps.longitude = 0.0f;
	nmea_index = 0;
	nmea_line_ready = false;
	HAL_UART_Receive_IT(&huart4, &rx_byte, 1);
}

void GPS_Driver_Update(void){
	if (nmea_line_ready){
		rmc_splitter();
	}
	if (HAL_GetTick() - last_gps_activity_tick > GPS_TIMEOUT){
		fault_flags.gps_fault = true;
		gps.locked = false;
	}
	else {
		fault_flags.gps_fault = false;
	}
}

static float parse_lat_long(char *field, char hemisphere, int degree_digits){
	char deg_str[4];
	if (field == NULL || field[0] == '\0'){
		return 0.0f;
	}
	strncpy(deg_str, field, (size_t)degree_digits);
	deg_str[degree_digits] = '\0';
	float degrees = (float)atof(deg_str);
	float minutes = (float)atof(field + degree_digits);
	float decimal = degrees + (minutes / 60.0f);
	if (hemisphere == 'S' || hemisphere == 'W'){
		decimal = -decimal;
	}
	return decimal;
}

static void rmc_splitter(void){
	char *talker;
	char *status;
	char *lat_hemi;
	char *long_hemi;
	char *lat_field;
	char *long_field;
	char *speed_field;

	nmea_line_ready = false;

	talker = strtok(nmea_parse_buffer, ","); // FIELD 0
	if (talker == NULL || strstr(talker, "RMC") == NULL){
		return;
	}

	(void)strtok(NULL, ","); // FIELD 1 (time)
	status = strtok(NULL, ","); // FIELD 2 (STATUS)
	if (status == NULL || status[0] != 'A'){
		gps.locked = false;
		return;
	}

	lat_field = strtok(NULL, ","); // FIELD 3 (LAT)
	lat_hemi = strtok(NULL, ","); // FIELD 4 (LAT HEMI)
	long_field = strtok(NULL, ","); // FIELD 5 (LONG)
	long_hemi = strtok(NULL, ","); // FIELD 6 (LONG HEMI)
	speed_field = strtok(NULL, ","); // FIELD 7

	if (lat_field == NULL || lat_hemi == NULL ||
	    long_field == NULL || long_hemi == NULL || speed_field == NULL){
		gps.locked = false;
		return;
	}

	gps.locked = true;
	gps.latitude = parse_lat_long(lat_field, lat_hemi[0], 2);
	gps.longitude = parse_lat_long(long_field, long_hemi[0], 3);
	gps.speed = (float)atof(speed_field) * 1.852f; // KNOTS TO KM/H

}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if (huart->Instance != UART4){
		return;
	}

	if (rx_byte == '\r'){
		// ignore CR
	}
	else if (rx_byte == '\n'){
		if (nmea_index < NMEA_BUF_LEN){
			nmea_buffer[nmea_index] = '\0';
			memcpy(nmea_parse_buffer, nmea_buffer, (size_t)nmea_index + 1U);
			nmea_line_ready = true;
			last_gps_activity_tick = HAL_GetTick();
		}
		nmea_index = 0;
	}
	else if (nmea_index < (NMEA_BUF_LEN - 1U)){
		nmea_buffer[nmea_index++] = (char)rx_byte;
	}
	else{
		nmea_index = 0; // overflow: drop line and resync
	}

	HAL_UART_Receive_IT(&huart4, &rx_byte, 1);
}
