/*
 * sd_spi_bus.c
 *
 * CubeMX-safe SD SPI bus helpers + debug probe.
 * Intentionally NOT under FATFS/Target so Generate Code cannot wipe it.
 */
#include "sd_spi_bus.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "main.h"
#include "spi.h"
#include "iwdg.h"
#include "usart.h"

#if SD_CS_USE_PC4
#define SD_CS_Pin       GPIO_PIN_4
#define SD_CS_GPIO_Port GPIOC
#else
#define SD_CS_Pin       CS_SPI1_Pin
#define SD_CS_GPIO_Port CS_SPI1_GPIO_Port
#endif

volatile uint8_t sd_fail_stage = 0;
volatile uint8_t sd_last_r1 = 0xFF;
volatile uint8_t sd_m0_r1 = 0xFF;
volatile uint8_t sd_m3_r1 = 0xFF;
volatile uint8_t sd_bus_idle = 0x00;

void SD_CS_ForceIdleHigh(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	__HAL_RCC_GPIOC_CLK_ENABLE();

	/* Force CS candidates to push-pull outputs (survives Cube/pinmux surprises) */
	GPIO_InitStruct.Pin = GPIO_PIN_4 | CS_SPI1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4 | CS_SPI1_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CS_SPI2_GPIO_Port, CS_SPI2_Pin, GPIO_PIN_SET);
}

static void SD_CS_High(void)
{
	HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
#if SD_CS_USE_PC4
	/* Keep original CS_SPI1 (PC12) deselected while testing PC4 */
	HAL_GPIO_WritePin(CS_SPI1_GPIO_Port, CS_SPI1_Pin, GPIO_PIN_SET);
#endif
}

static void SD_CS_Low(void)
{
	HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
#if SD_CS_USE_PC4
	HAL_GPIO_WritePin(CS_SPI1_GPIO_Port, CS_SPI1_Pin, GPIO_PIN_SET);
#endif
}

void SD_Deselect(void)
{
	uint8_t dummy = 0xFF;
	uint8_t rx;
	SD_CS_High();
	HAL_SPI_TransmitReceive(&hspi1, &dummy, &rx, 1, HAL_MAX_DELAY);
}

void SD_Select(void)
{
	SD_CS_Low();
}

void SD_Dummy(void)
{
	uint8_t tx = 0xFF, rx;
	HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, HAL_MAX_DELAY);
}

void SD_SPI_Reconfig(uint32_t baud, uint32_t cpol, uint32_t cpha)
{
	hspi1.Init.BaudRatePrescaler = baud;
	hspi1.Init.CLKPolarity = cpol;
	hspi1.Init.CLKPhase = cpha;
	HAL_SPI_Init(&hspi1);
}

void SD_SendCommand(uint8_t cmd, uint32_t arg, uint8_t crc)
{
	uint8_t frame[6];
	frame[0] = 0x40 | cmd;
	frame[1] = (uint8_t)(arg >> 24);
	frame[2] = (uint8_t)(arg >> 16);
	frame[3] = (uint8_t)(arg >> 8);
	frame[4] = (uint8_t)(arg);
	frame[5] = crc;

	uint8_t rx;
	for (int i = 0; i < 6; i++) {
		HAL_SPI_TransmitReceive(&hspi1, &frame[i], &rx, 1, HAL_MAX_DELAY);
	}
}

void SD_ReadR7(uint8_t *response)
{
	uint8_t tx = 0xFF, rx = 0xFF;
	int timeout = 1000;
	while (timeout--) {
		HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, HAL_MAX_DELAY);
		if ((rx & 0x80) == 0) {
			break;
		}
	}
	response[0] = rx;

	for (int i = 1; i < 5; i++) {
		HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, HAL_MAX_DELAY);
		response[i] = rx;
	}
}

uint8_t SD_ReadR1(void)
{
	uint8_t tx = 0xFF, rx = 0xFF;
	int timeout = 1000;
	while (timeout--) {
		HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, HAL_MAX_DELAY);
		if ((rx & 0x80) == 0) {
			break;
		}
	}
	return rx;
}

#if SD_SPI_DEBUG
static void SD_DebugPrint(const char *s)
{
	HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), HAL_MAX_DELAY);
}

static void SD_DebugPrintf(const char *fmt, ...)
{
	char buf[160];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	SD_DebugPrint(buf);
}

static uint8_t SD_ProbeCmd0(void)
{
	uint8_t response;

	SD_Deselect();
	HAL_Delay(1);
	SD_Select();
	SD_SendCommand(0, 0, 0x95);
	response = SD_ReadR1();
	SD_Deselect();
	return response;
}

void SD_SPI_DebugProbe(void)
{
	uint8_t tx = 0xFF, rx;
	GPIO_PinState cs_idle, cs_active;
	int i;

	HAL_IWDG_Refresh(&hiwdg);

	SD_DebugPrint("\r\n======== SD SPI DEBUG PROBE ========\r\n");
#if SD_CS_USE_PC4
	SD_DebugPrint("CS=PC4 (TEMP)  SCK=PA5  MISO=PA6  MOSI=PA7\r\n");
#else
	SD_DebugPrint("CS=PC12  SCK=PA5  MISO=PA6  MOSI=PA7\r\n");
#endif
	SD_DebugPrint("Serial: ST-Link VCP USART2 115200\r\n");

	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	HAL_SPI_Init(&hspi1);
	HAL_Delay(10);

	/* Drive each CS candidate low briefly and report readback — finds clamp vs wrong pin */
	{
		uint32_t moder = GPIOC->MODER;
		uint32_t pc4_mode = (moder >> (4 * 2)) & 0x3u;
		uint32_t pc12_mode = (moder >> (12 * 2)) & 0x3u;
		SD_DebugPrintf("GPIOC MODER PC4=%lu PC12=%lu (1=output)\r\n",
		               (unsigned long)pc4_mode, (unsigned long)pc12_mode);
	}

	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CS_SPI1_GPIO_Port, CS_SPI1_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
	SD_DebugPrintf("Drive PC4 low -> IDR PC4=%u PC12=%u ODR_PC4=%u\r\n",
	               (unsigned)HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4),
	               (unsigned)HAL_GPIO_ReadPin(CS_SPI1_GPIO_Port, CS_SPI1_Pin),
	               (unsigned)((GPIOC->ODR & GPIO_PIN_4) ? 1u : 0u));
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_SET);

	HAL_GPIO_WritePin(CS_SPI1_GPIO_Port, CS_SPI1_Pin, GPIO_PIN_RESET);
	SD_DebugPrintf("Drive PC12 low -> IDR PC4=%u PC12=%u ODR_PC12=%u\r\n",
	               (unsigned)HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_4),
	               (unsigned)HAL_GPIO_ReadPin(CS_SPI1_GPIO_Port, CS_SPI1_Pin),
	               (unsigned)((GPIOC->ODR & CS_SPI1_Pin) ? 1u : 0u));
	HAL_GPIO_WritePin(CS_SPI1_GPIO_Port, CS_SPI1_Pin, GPIO_PIN_SET);

	cs_idle = HAL_GPIO_ReadPin(SD_CS_GPIO_Port, SD_CS_Pin);
	SD_CS_Low();
	cs_active = HAL_GPIO_ReadPin(SD_CS_GPIO_Port, SD_CS_Pin);
	SD_CS_High();
	/* Raw pin levels: idle should be 1, selected should be 0 */
	SD_DebugPrintf("Active CS pin level idle=%u active=%u (expect 1,0)\r\n",
	               (unsigned)cs_idle, (unsigned)cs_active);

	SD_DebugPrint("Idle MISO: ");
	for (i = 0; i < 8; i++) {
		HAL_SPI_TransmitReceive(&hspi1, &tx, &rx, 1, HAL_MAX_DELAY);
		SD_DebugPrintf("%02X ", rx);
	}
	SD_DebugPrint("(want FF)\r\n");

	/* #region agent log */
	/* Hypotheses:
	 * A: PA6-PA7 short not actually present at MCU pads during test
	 * B: SPI AF MOSI (PA7) not driving the pad
	 * C: SPI AF MISO (PA6) not sampling the pad
	 * D: External clamp holds MISO high even with a short
	 * E: HAL_SPI_TransmitReceive failing (non-OK status)
	 */
	{
		GPIO_InitTypeDef gi = {0};
		GPIO_PinState s0, s1;
		uint8_t pattern[4] = {0xA5, 0x5A, 0x00, 0xFF};
		uint8_t rxb[4];
		HAL_StatusTypeDef st[4];
		uint32_t cr1 = SPI1->CR1;
		uint32_t pa_moder = GPIOA->MODER;
		uint32_t pa_afrl = GPIOA->AFR[0];

		/* --- GPIO bit-bang short detect (no SPI) --- */
		HAL_SPI_DeInit(&hspi1);
		gi.Pin = GPIO_PIN_7;
		gi.Mode = GPIO_MODE_OUTPUT_PP;
		gi.Pull = GPIO_NOPULL;
		gi.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(GPIOA, &gi);
		gi.Pin = GPIO_PIN_6;
		gi.Mode = GPIO_MODE_INPUT;
		gi.Pull = GPIO_NOPULL;
		HAL_GPIO_Init(GPIOA, &gi);

		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_RESET);
		HAL_Delay(1);
		s0 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, GPIO_PIN_SET);
		HAL_Delay(1);
		s1 = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6);

		SD_DebugPrintf("GPIO bang PA7->PA6: low=%u high=%u ",
		               (unsigned)s0, (unsigned)s1);
		if (s0 == GPIO_PIN_RESET && s1 == GPIO_PIN_SET) {
			SD_DebugPrint("(SHORT PRESENT at MCU)\r\n");
		} else if (s0 == GPIO_PIN_SET && s1 == GPIO_PIN_SET) {
			SD_DebugPrint("(NO SHORT — PA6 stuck high / open)\r\n");
		} else {
			SD_DebugPrint("(UNEXPECTED — check wiring)\r\n");
		}
		/* DBGJ for debug-101f2d.log */
		SD_DebugPrintf("DBGJ{\"sessionId\":\"101f2d\",\"hypothesisId\":\"A\",\"location\":\"sd_spi_bus.c:gpio_bang\",\"message\":\"pa6_follows_pa7\",\"data\":{\"low\":%u,\"high\":%u,\"short\":%u},\"timestamp\":%lu}\r\n",
		               (unsigned)s0, (unsigned)s1,
		               (unsigned)(s0 == GPIO_PIN_RESET && s1 == GPIO_PIN_SET),
		               (unsigned long)HAL_GetTick());

		/* Restore SPI1 AF on PA5/6/7 */
		hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
		hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
		hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
		HAL_SPI_Init(&hspi1);
		HAL_Delay(2);

		cr1 = SPI1->CR1;
		pa_moder = GPIOA->MODER;
		pa_afrl = GPIOA->AFR[0];
		SD_DebugPrintf("SPI1 CR1=0x%04lX SPE=%lu\r\n",
		               (unsigned long)cr1,
		               (unsigned long)((cr1 >> 6) & 1u));
		SD_DebugPrintf("PA5/6/7 mode=%lu/%lu/%lu af=%lu/%lu/%lu (want mode2 af5)\r\n",
		               (unsigned long)((pa_moder >> 10) & 3u),
		               (unsigned long)((pa_moder >> 12) & 3u),
		               (unsigned long)((pa_moder >> 14) & 3u),
		               (unsigned long)((pa_afrl >> 20) & 0xFu),
		               (unsigned long)((pa_afrl >> 24) & 0xFu),
		               (unsigned long)((pa_afrl >> 28) & 0xFu));

		SD_DebugPrint("Loopback TX A5 5A 00 FF -> RX ");
		SD_CS_High();
		for (i = 0; i < 4; i++) {
			st[i] = HAL_SPI_TransmitReceive(&hspi1, &pattern[i], &rxb[i], 1, HAL_MAX_DELAY);
			SD_DebugPrintf("%02X ", rxb[i]);
		}
		SD_DebugPrint("\r\n");
		SD_DebugPrintf("Loopback HAL st=%d/%d/%d/%d (0=OK)\r\n",
		               (int)st[0], (int)st[1], (int)st[2], (int)st[3]);
		SD_DebugPrint("  open bus => FF FF FF FF; PA6-PA7 short at MCU => A5 5A 00 FF\r\n");
		SD_DebugPrintf("DBGJ{\"sessionId\":\"101f2d\",\"hypothesisId\":\"B\",\"location\":\"sd_spi_bus.c:spi_loop\",\"message\":\"spi_loopback\",\"data\":{\"rx0\":%u,\"rx1\":%u,\"rx2\":%u,\"rx3\":%u,\"st0\":%d,\"echo_ok\":%u},\"timestamp\":%lu}\r\n",
		               (unsigned)rxb[0], (unsigned)rxb[1], (unsigned)rxb[2], (unsigned)rxb[3],
		               (int)st[0],
		               (unsigned)(rxb[0] == 0xA5 && rxb[1] == 0x5A && rxb[2] == 0x00 && rxb[3] == 0xFF),
		               (unsigned long)HAL_GetTick());
		SD_DebugPrintf("DBGJ{\"sessionId\":\"101f2d\",\"hypothesisId\":\"C\",\"location\":\"sd_spi_bus.c:pinmux\",\"message\":\"spi1_pinmux\",\"data\":{\"CR1\":%lu,\"pa6_mode\":%lu,\"pa7_mode\":%lu,\"pa6_af\":%lu,\"pa7_af\":%lu},\"timestamp\":%lu}\r\n",
		               (unsigned long)cr1,
		               (unsigned long)((pa_moder >> 12) & 3u),
		               (unsigned long)((pa_moder >> 14) & 3u),
		               (unsigned long)((pa_afrl >> 24) & 0xFu),
		               (unsigned long)((pa_afrl >> 28) & 0xFu),
		               (unsigned long)HAL_GetTick());
	}
	/* #endregion */

	SD_DebugPrint("Mode0 CMD0: ");
	for (i = 0; i < 5; i++) {
		HAL_IWDG_Refresh(&hiwdg);
		SD_DebugPrintf("%02X ", SD_ProbeCmd0());
		HAL_Delay(2);
	}
	SD_DebugPrint("(want 01 or 7F)\r\n");

	hspi1.Init.CLKPolarity = SPI_POLARITY_HIGH;
	hspi1.Init.CLKPhase = SPI_PHASE_2EDGE;
	HAL_SPI_Init(&hspi1);
	HAL_Delay(5);

	SD_DebugPrint("Mode3 CMD0: ");
	for (i = 0; i < 5; i++) {
		HAL_IWDG_Refresh(&hiwdg);
		SD_DebugPrintf("%02X ", SD_ProbeCmd0());
		HAL_Delay(2);
	}
	SD_DebugPrint("(want 01)\r\n");
	SD_DebugPrint("======== END SD SPI DEBUG ========\r\n");
}
#else
void SD_SPI_DebugProbe(void) { /* SD_SPI_DEBUG disabled */ }
#endif /* SD_SPI_DEBUG */
