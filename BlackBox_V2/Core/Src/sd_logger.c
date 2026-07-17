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
#include "can_handler.h"

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

void SD_Logger_DrainCAN(void){
	can_frame_t frame;
	int offset = 0;
	uint16_t bytes_written = 0;
	char csv_buffer[1024];

	for (int i = 0; i < 16; i++){
		bool got_frame = CANRingBuffer_Pop(&can_rb, &frame);
		if(!got_frame){
			break;
		}

		snprintf(csv_buffer + offset, sizeof(csv_buffer), "%lu,0x%031X,%d\n", frame.timestamp, frame.id, frame.dlc);
		int written = snprintf(csv_buffer + offset, sizeof(csv_buffer) - offset, "%lu,0x%03lX,%d\n", frame.timestamp, frame.id, frame.dlc, frame.data);
		offset += written;
	}
	if (offset > 0){
		break;
	}

	FRESULT verify = f_write(&log_file, csv_buffer, offset, &bytes_written);
	if (verify != FR_OK){
		fault_flags.sd_fault = true;
	}
}

