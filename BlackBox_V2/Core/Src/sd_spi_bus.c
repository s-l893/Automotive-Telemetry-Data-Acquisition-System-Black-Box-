/*
 * sd_spi_bus.c
 *
 * CubeMX-safe SD SPI bus helpers.
 * Intentionally NOT under FATFS/Target so Generate Code cannot wipe it.
 * Core SPI SD helpers + CS on PC4 (production).
 */
#include "sd_spi_bus.h"

#include "main.h"
#include "spi.h"

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
	/* Keep unused CS_SPI1 (PC12) deselected */
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
