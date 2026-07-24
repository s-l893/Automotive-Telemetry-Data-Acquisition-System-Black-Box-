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
#include <stdio.h>

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
	UINT bytes_written = 0; // total bytes written
	char csv_buffer[1024]; // buffer to write to csv log

	for (int i = 0; i < 16; i++){
		bool got_frame = CANRingBuffer_Pop(&can_rb, &frame);
		if(!got_frame){
			break;
		}
		//DLC indicates how many of the 8 slots contain real data

		uint8_t padded_data[8]; // Temporary buffer
		for (int j = 0; j < 8; j++){
			if (j >= frame.dlc){
				padded_data[j] = 0;
			}
			else if (j < frame.dlc){
				padded_data[j] = frame.data[j];
			}
		}
		// %d specifiers for each data byte position (out of 8)
		int written = snprintf(csv_buffer + offset, sizeof(csv_buffer) - offset, "%lu,0x%03lX,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", frame.timestamp, frame.id, frame.dlc, padded_data[0], padded_data[1], padded_data[2], padded_data[3],
		        padded_data[4], padded_data[5], padded_data[6], padded_data[7]);
		offset += written;
	}
	if (offset > 0){
		FRESULT verify = f_write(&log_file, csv_buffer, offset, &bytes_written);
		if (verify != FR_OK){
			fault_flags.sd_fault = true;
		}
	}


}

void flush_ring_buffers(void){
	int drain_count = 0;
	while (can_rb.count > 0){
		if (drain_count>= 1000){
			break;
		}
		SD_Logger_DrainCAN();
		drain_count++;
	}
}

