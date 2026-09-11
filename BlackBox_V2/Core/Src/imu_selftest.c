/*
 * imu_selftest.c
 *
 * Exercises the real imu.c APIs (not a parallel I2C path) and prints
 * structured lines for the host Python tester.
 */
#include "imu_selftest.h"

#if IMU_SELFTEST_ENABLE

#include "imu.h"
#include "fault.h"
#include "i2c.h"
#include "iwdg.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define IMU_SELFTEST_SAMPLE_COUNT 25U
#define IMU_SELFTEST_SAMPLE_PERIOD_MS 200U

static uint32_t sample_count;
static uint32_t last_sample_ms;
static uint8_t streaming;

static void st_print(const char *s)
{
	DBG_Print(s);
}

static void st_print_init_result(void)
{
	char line[160];

	/* Stage 1: real imu_init() wake + WHO_AM_I + fault flags */
	snprintf(line, sizeof(line),
	         "IMUTEST INIT who=0x%02X wake_hal=%d who_hal=%d err=0x%lX f=%u hs=%u\r\n",
	         imu_who_am_i,
	         (int)imu_wake_write_status,
	         (int)imu_who_read_status,
	         (unsigned long)hi2c1.ErrorCode,
	         (unsigned)fault_flags.imu_fault,
	         (unsigned)fault_flags.imu_handshake_fault);
	st_print(line);

	/* Stage 2: real imu_calibrate() offsets (called from imu_init) */
	snprintf(line, sizeof(line),
	         "IMUTEST CAL ox=%d oy=%d oz=%d\r\n",
	         imu_offset.offset_x, imu_offset.offset_y, imu_offset.offset_z);
	st_print(line);
}

static void st_print_sample(void)
{
	char line[160];
	snprintf(line, sizeof(line),
	         "IMUTEST SAMPLE n=%lu t=%lu ax=%d ay=%d az=%d accel_hal=%d f=%u hs=%u\r\n",
	         (unsigned long)sample_count,
	         (unsigned long)imu.timestamp,
	         imu.accel_x, imu.accel_y, imu.accel_z,
	         (int)imu_accel_read_status,
	         (unsigned)fault_flags.imu_fault,
	         (unsigned)fault_flags.imu_handshake_fault);
	st_print(line);
}

void IMU_SelfTest_Run(void)
{
	sample_count = 0;
	last_sample_ms = 0;
	streaming = 1;

	st_print("\r\n======== IMU DRIVER SELFTEST ========\r\n");
	st_print("IMUTEST BEGIN\r\n");
	st_print("IMUTEST NOTE calling real imu_init()/imu_calibrate()/imu_read()\r\n");

	/* THIS is the production driver path under test */
	imu_init();
	st_print_init_result();

	st_print("IMUTEST STREAM start keep board flat for automatic stages\r\n");
	last_sample_ms = HAL_GetTick();
}

void IMU_SelfTest_Tick(void)
{
	uint32_t now;

	if (!streaming) {
		return;
	}

	now = HAL_GetTick();
	if ((now - last_sample_ms) < IMU_SELFTEST_SAMPLE_PERIOD_MS) {
		return;
	}
	last_sample_ms = now;

	/* Real imu_read() — offset subtraction + fault gating included */
	imu_read();
	sample_count++;
	st_print_sample();

	if (sample_count >= IMU_SELFTEST_SAMPLE_COUNT) {
		st_print("IMUTEST END\r\n");
		st_print("======== IMU DRIVER SELFTEST DONE ========\r\n");
		streaming = 0;
	}
}

#endif /* IMU_SELFTEST_ENABLE */
