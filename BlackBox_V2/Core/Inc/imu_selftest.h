/*
 * imu_selftest.h
 *
 * TEMP host-visible harness that exercises the real imu.c driver
 * (imu_init / imu_calibrate / imu_read) and prints parseable lines
 * over USART2 for tools/test_imu_driver.py.
 *
 * Set IMU_SELFTEST_ENABLE to 0 when done testing.
 */
#ifndef INC_IMU_SELFTEST_H_
#define INC_IMU_SELFTEST_H_

#include <stdint.h>

#ifndef IMU_SELFTEST_ENABLE
#define IMU_SELFTEST_ENABLE 0
#endif

#if IMU_SELFTEST_ENABLE
void IMU_SelfTest_Run(void);
void IMU_SelfTest_Tick(void);
#endif

#endif /* INC_IMU_SELFTEST_H_ */
