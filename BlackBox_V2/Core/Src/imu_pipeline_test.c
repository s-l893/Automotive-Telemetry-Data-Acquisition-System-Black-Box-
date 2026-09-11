/*
 * imu_pipeline_test.c
 */
#include "imu_pipeline_test.h"

#if IMU_PIPELINE_TEST_ENABLE

#include "usart.h"
#include <stdio.h>
#include <string.h>

void IMU_PipelineTest_Init(void)
{
	MX_USART2_UART_Init();
	DBG_Print("\r\n======== IMU PIPELINE TEST ========\r\n");
	DBG_Print("IMUCSV BEGIN\r\n");
	DBG_Print("Send CAN (or use boot loopback) to enter SYS_LOGGING.\r\n");
	DBG_Print("Then stop CAN (<2.5s) to observe 0xFFFF sentinel rows.\r\n");
}

void IMU_PipelineTest_MirrorCsvChunk(const char *chunk, int len)
{
	/* Prefix each CSV line so the host script can filter noise. */
	const char *p = chunk;
	const char *end = chunk + len;
	char line[192];

	while (p < end) {
		const char *nl = memchr(p, '\n', (size_t)(end - p));
		int n = (nl != NULL) ? (int)(nl - p) : (int)(end - p);
		if (n > 0 && n < (int)(sizeof(line) - 16)) {
			memcpy(line, "IMUCSV ", 7);
			memcpy(line + 7, p, (size_t)n);
			line[7 + n] = '\r';
			line[8 + n] = '\n';
			line[9 + n] = '\0';
			DBG_Print(line);
		}
		if (nl == NULL) {
			break;
		}
		p = nl + 1;
	}
}

#endif /* IMU_PIPELINE_TEST_ENABLE */
