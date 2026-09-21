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
#include "rtc.h"
#include <stdlib.h>
#include <string.h>

#define NMEA_BUF_LEN 128
#define GPS_TIMEOUT_MS 5000U

static char nmea_buffer[NMEA_BUF_LEN];
static char nmea_parse_buffer[NMEA_BUF_LEN];
static volatile uint16_t nmea_index = 0;
static volatile bool nmea_line_ready = false;
static uint8_t rx_byte;
static volatile uint32_t last_gps_activity_tick = 0;

gps_data_t gps;

static float parse_lat_long(char *field, char hemisphere, int degree_digits);
static void rmc_splitter(void);
static void gps_uart_restart_rx(void);
static bool parse_hhmmss(const char *s, uint8_t *hh, uint8_t *mm, uint8_t *ss);
static bool parse_ddmmyy(const char *s, uint8_t *dd, uint8_t *mo, uint8_t *yy);
static void gps_sync_rtc(uint8_t hh, uint8_t mm, uint8_t ss,
			 uint8_t dd, uint8_t mo, uint8_t yy);

void GPS_Driver_Init(void){
	gps.locked = false;
	gps.rtc_synced = false;
	gps.speed = 0.0f;
	gps.latitude = 0.0f;
	gps.longitude = 0.0f;
	nmea_index = 0;
	nmea_line_ready = false;
	last_gps_activity_tick = HAL_GetTick();
	fault_flags.gps_fault = false;
	gps_uart_restart_rx();
}

void GPS_Driver_Update(void){
	if (nmea_line_ready){
		rmc_splitter();
	}

	if ((HAL_GetTick() - last_gps_activity_tick) > GPS_TIMEOUT_MS){
		fault_flags.gps_fault = true;
		gps.locked = false;
		gps_uart_restart_rx();
	}
	else{
		fault_flags.gps_fault = false;
	}
}

static void gps_uart_restart_rx(void){
	HAL_UART_Abort_IT(&huart4);
	huart4.ErrorCode = HAL_UART_ERROR_NONE;
	HAL_UART_Receive_IT(&huart4, &rx_byte, 1);
}

static bool parse_hhmmss(const char *s, uint8_t *hh, uint8_t *mm, uint8_t *ss){
	char part[3];
	if (s == NULL || strlen(s) < 6U){
		return false;
	}
	part[2] = '\0';
	part[0] = s[0]; part[1] = s[1]; *hh = (uint8_t)atoi(part);
	part[0] = s[2]; part[1] = s[3]; *mm = (uint8_t)atoi(part);
	part[0] = s[4]; part[1] = s[5]; *ss = (uint8_t)atoi(part);
	return (*hh <= 23U && *mm <= 59U && *ss <= 59U);
}

static bool parse_ddmmyy(const char *s, uint8_t *dd, uint8_t *mo, uint8_t *yy){
	char part[3];
	if (s == NULL || strlen(s) < 6U){
		return false;
	}
	part[2] = '\0';
	part[0] = s[0]; part[1] = s[1]; *dd = (uint8_t)atoi(part);
	part[0] = s[2]; part[1] = s[3]; *mo = (uint8_t)atoi(part);
	part[0] = s[4]; part[1] = s[5]; *yy = (uint8_t)atoi(part);
	return (*dd >= 1U && *dd <= 31U && *mo >= 1U && *mo <= 12U);
}

static void gps_sync_rtc(uint8_t hh, uint8_t mm, uint8_t ss,
			 uint8_t dd, uint8_t mo, uint8_t yy){
	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0};

	sTime.Hours = hh;
	sTime.Minutes = mm;
	sTime.Seconds = ss;
	sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
	sTime.StoreOperation = RTC_STOREOPERATION_RESET;

	sDate.WeekDay = RTC_WEEKDAY_MONDAY; // unused weekday
	sDate.Month = mo;
	sDate.Date = dd;
	sDate.Year = yy; /* 0–99 → 2000–2099 */

	if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK){
		return;
	}
	if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK){
		return;
	}
	gps.rtc_synced = true;
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
	char *time_field;
	char *status;
	char *lat_hemi;
	char *long_hemi;
	char *lat_field;
	char *long_field;
	char *speed_field;
	char *date_field;
	uint8_t hh, mm, ss, dd, mo, yy;

	nmea_line_ready = false;

	talker = strtok(nmea_parse_buffer, ","); // FIELD 0
	if (talker == NULL || strstr(talker, "RMC") == NULL){
		return;
	}

	time_field = strtok(NULL, ","); // FIELD 1 (time hhmmss.sss)
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
	(void)strtok(NULL, ","); // FIELD 8 (course)
	date_field = strtok(NULL, ","); // FIELD 9 (ddmmyy)

	if (lat_field == NULL || lat_hemi == NULL ||
	    long_field == NULL || long_hemi == NULL || speed_field == NULL){
		gps.locked = false;
		return;
	}

	gps.locked = true;
	gps.latitude = parse_lat_long(lat_field, lat_hemi[0], 2);
	gps.longitude = parse_lat_long(long_field, long_hemi[0], 3);
	gps.speed = (float)atof(speed_field) * 1.852f; // KNOTS TO KM/H

	/* One-shot RTC load from GPS (UTC) so session filenames use real time */
	if (!gps.rtc_synced &&
	    parse_hhmmss(time_field, &hh, &mm, &ss) &&
	    parse_ddmmyy(date_field, &dd, &mo, &yy)){
		gps_sync_rtc(hh, mm, ss, dd, mo, yy);
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
	if (huart->Instance != UART4){
		return;
	}

	last_gps_activity_tick = HAL_GetTick();

	if (rx_byte == '\r'){
		// ignore CR
	}
	else if (rx_byte == '\n'){
		if (nmea_index < NMEA_BUF_LEN){
			nmea_buffer[nmea_index] = '\0';
			memcpy(nmea_parse_buffer, nmea_buffer, (size_t)nmea_index + 1U);
			nmea_line_ready = true;
		}
		nmea_index = 0;
	}
	else if (nmea_index < (NMEA_BUF_LEN - 1U)){
		nmea_buffer[nmea_index++] = (char)rx_byte;
	}
	else{
		nmea_index = 0;
	}

	if (HAL_UART_Receive_IT(&huart4, &rx_byte, 1) != HAL_OK){
		gps_uart_restart_rx();
	}
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart){
	if (huart->Instance != UART4){
		return;
	}
	__HAL_UART_CLEAR_OREFLAG(huart);
	__HAL_UART_CLEAR_FEFLAG(huart);
	__HAL_UART_CLEAR_NEFLAG(huart);
	__HAL_UART_CLEAR_PEFLAG(huart);
	huart->ErrorCode = HAL_UART_ERROR_NONE;
	nmea_index = 0;
	HAL_UART_Receive_IT(&huart4, &rx_byte, 1);
}
