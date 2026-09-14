/*
 * imu_pipeline_test.h
 *
 * TEMP: mirrors each CSV row written by SD_Logger_DrainCAN() over USART2
 * so tools/test_imu_pipeline.py can validate:
 *   Step 1 — real CAN rows with IMU columns
 *   Step 2 — 0xFFFF sentinel rows (~200 ms) with live IMU
 *
 * Set to 0 when done. Conflicts with display DC/RESET on PA2/PA3 while on.
 */
#ifndef INC_IMU_PIPELINE_TEST_H_
#define INC_IMU_PIPELINE_TEST_H_

#ifndef IMU_PIPELINE_TEST_ENABLE
#define IMU_PIPELINE_TEST_ENABLE 0
#endif

#if IMU_PIPELINE_TEST_ENABLE
void IMU_PipelineTest_Init(void);
void IMU_PipelineTest_MirrorCsvChunk(const char *chunk, int len);
#endif

#endif /* INC_IMU_PIPELINE_TEST_H_ */
