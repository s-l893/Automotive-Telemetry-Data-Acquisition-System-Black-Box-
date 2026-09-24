/*
 * sd_logger.c
 *
 *  Created on: Jul 7, 2026
 *      Author: Sunny Lin
 */

#include "fault.h"
#include "fatfs.h"
#include "sd_spi_bus.h"
#include <stdbool.h>
#include "main.h"
#include "can_handler.h"
#include <stdio.h>
#include "imu.h"
#include "gps_driver.h"
#include "can_decode.h"
#include "rtc.h"

FATFS fs;
FIL log_file;

char filename[32];
char header[128];
char footer[128];
volatile bool sd_mount = false;

// timer for faster imu polls
static uint32_t last_row_write_time = 0;

bool SD_Logger_Init(void) {
	// initial sd card mount
	FRESULT res = f_mount(&fs, USERPath, 1);
	if (res != FR_OK) {
		// additional mount attempt in case of a slow soft reset
		HAL_Delay(100);
		f_mount(NULL, USERPath, 0);
		res = f_mount(&fs, USERPath, 1);
	}
	sd_mount = (res == FR_OK);
	if (res != FR_OK){
		fault_flags.sd_fault = true;
	}

	return sd_mount;
}

void start_new_session_file(void){
	static int session_number = 0;
	RTC_TimeTypeDef sTime = {0};
	RTC_DateTypeDef sDate = {0};
// FILENAME NAMING LOGIC, BASED ON RTC SYNC FROM GPS, OTHERWISE FALLS BACK TO LOG.000.CSV.
	if (gps.rtc_synced){
		HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
		HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); // comes after gettime

		snprintf(filename, sizeof(filename), "%02u%02u%02u%02u.CSV",
			 (unsigned)sDate.Month, (unsigned)sDate.Date,
			 (unsigned)sTime.Hours, (unsigned)sTime.Minutes);
	}
	else{
		snprintf(filename, sizeof(filename), "LOG_%03d.CSV", session_number);
		session_number++;
	}

	FRESULT res = f_open(&log_file, filename, FA_CREATE_ALWAYS | FA_WRITE);
	if (res != FR_OK){
		fault_flags.sd_fault = true;
	}
	else if (res == FR_OK){
		int header_len = snprintf(header, sizeof(header), "timestamp, id, RPM, trtl, lat, long, speed, accel_x, accel_y, accel_z\n");
		UINT bytes_written;
		FRESULT res1 = f_write(&log_file, header, header_len, &bytes_written);
		if (res1 != FR_OK){
			fault_flags.sd_fault = true;
		}
	}

}

void close_session_file(void){
	// Commit cached data before closing so removal/power-down does not delete it
	int footer_len = snprintf(footer, sizeof(footer), "END OF SESSION\n");
	UINT bytes_written;
	FRESULT res = f_write(&log_file, footer, footer_len, &bytes_written);
	if (res != FR_OK){
		fault_flags.sd_fault = true;
	}
	f_sync(&log_file);
	f_close(&log_file);
}

void sd_recovery(void) {
	// non-blocking remount attempt while the system FSM is in SYS_FAULT
	static uint32_t last_attempt = 0;
	uint32_t now = HAL_GetTick();
	if ((now - last_attempt) >= 3500){
		last_attempt = now;
		FRESULT res = f_mount(&fs, USERPath, 1);
		if (res == FR_OK){
			fault_flags.sd_fault = false;
			sd_mount = true;
		}
	}
}

void unmount_sd(void){
	f_mount(NULL, USERPath, 0);
	sd_mount = false;
}

void SD_Logger_DrainCAN(void){
	can_frame_t frame;
	int offset = 0;
	UINT bytes_written = 0; // total bytes written
	char csv_buffer[2048];  // batch several CAN frames into one FatFs write

	// Limit work per FSM tick so logging does not block the rest of the system
	for (int i = 0; i < 16; i++){
		bool got_frame = CANRingBuffer_Pop(&can_rb, &frame);
		if(!got_frame){
			break;
		}

		// decode the popped frames
		CAN_Decode_ProcessFrame(frame.id, frame.data, frame.dlc);

	// legacy architecture no longer needed, csv will log straight readable data instead of codes
		// uint8_t padded_data[8];

		// Write csv line on the frame that carries rpm or throttle
		// allows for lowest latency digital tachometer updates

		if (frame.id == 0x158 || frame.id == 0x17C){
			int written = snprintf(csv_buffer + offset, sizeof(csv_buffer) - offset,
				"%lu,0x%03lX,%.1f,%.1f,%f,%f,%f,%d,%d,%d\n",
				frame.timestamp, frame.id, 			// IMU divided by 16384 to convert to G
				vehicle_state.values[SIG_RPM], vehicle_state.values[SIG_THROTTLE], gps.latitude, gps.longitude, gps.speed, imu.accel_x/16384.0f, imu.accel_y/16384.0f, imu.accel_z/16384.0f);
			offset += written;
			last_row_write_time = HAL_GetTick();
		}
	}

	if (offset == 0){ // TIMEOUT FEATURE FOR EMPTY CAN ROWS WITH IMU DATA
		if ((HAL_GetTick() - last_row_write_time) >= 200){
			int imu_written = snprintf(csv_buffer + offset, sizeof(csv_buffer) - offset,
				"%lu,0x%03lX,%.1f,%.1f,%f,%f,%f,%d,%d,%d\n",
				HAL_GetTick(), 0xFFFFUL, vehicle_state.values[SIG_RPM], vehicle_state.values[SIG_THROTTLE], gps.latitude, gps.longitude, gps.speed,	imu.accel_x/16384.0f, imu.accel_y/16384.0f, imu.accel_z/16384.0f);
			offset += imu_written;
			last_row_write_time = HAL_GetTick();
		}
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

bool SD_Logger_Sync(void) {
	FRESULT res = f_sync(&log_file);
	if (res != FR_OK) {
		fault_flags.sd_fault = true;
		return false;
	}
	return true;
}
