/*
 * sd_mount.h
 *
 *  Created on: Jul 7, 2026
 *      Author: Sunny Lin
 */

#ifndef INC_SD_LOGGER_H_
#define INC_SD_LOGGER_H_

#include <stdint.h>
#include <stdbool.h>

extern volatile bool sd_mount;

bool SD_Logger_Init(void);
void start_new_session_file(void);
void close_session_file(void);
void sd_recovery(void);
void unmount_sd(void);
void flush_ring_buffers(void);

#endif /* INC_SD_LOGGER_H_ */
