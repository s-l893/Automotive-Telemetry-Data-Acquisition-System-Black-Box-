/*
 * sd_mount.c
 *
 *  Created on: Jul 7, 2026
 *      Author: Sunny Lin
 */

#include "fault.h"
#include "fatfs.h"
#include <stdbool.h>
#include "main.h"

FATFS fs;
FIL log_file;

char filename[32];

// #include "gps.c"

volatile bool sd_mount = false;

bool SD_Logger_Init(void) {
	FRESULT res = f_mount (&fs, "", 1);
	sd_mount = (res == FR_OK);
	if (res != FR_OK){
		fault_flags.sd_fault = true;
	}

	return sd_mount;
}

void start_new_session_file(void){
	FRESULT res = f_open(&log_file, filename, FA_CREATE_ALWAYS | FA_WRITE); //filename variable will be dealt with after gps is coded
	if (res != FR_OK){
		fault_flags.sd_fault = true;
	}
}

void close_session_file(void){
	f_sync(&log_file);
	f_close(&log_file);
}

void sd_recovery(void) {
	static uint32_t last_attempt = 0;
	uint32_t now = HAL_GetTick();
	if ((now - last_attempt) >= 3500){
		last_attempt = now;
		FRESULT res = f_mount (&fs, "", 1);
		if (res == FR_OK){
			fault_flags.sd_fault = false;
			sd_mount = true;
		}
	}
}

void unmount_sd(void){
	f_mount(NULL, "", 0);
	sd_mount = false;
}

