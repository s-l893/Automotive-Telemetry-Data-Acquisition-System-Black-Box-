/*
 * display.c
 *
 *  Created on: Sep 11, 2026
 *      Author: Sunny Lin
 */
#include "display.h"
#include "main.h"
#include "spi.h"
#include "sd_spi_bus.h"
#include "iwdg.h"

#include <stdint.h>

#define DISPLAY_CS_PIN        GPIO_PIN_14
#define DISPLAY_CS_GPIO_PORT  GPIOB
#define DISPLAY_DC_PIN        DC_SPI1_Pin
#define DISPLAY_DC_GPIO_PORT  DC_SPI1_GPIO_Port
#define DISPLAY_RST_PIN       RESET_SPI1_Pin
#define DISPLAY_RST_GPIO_PORT RESET_SPI1_GPIO_Port
#define NATIVE_WIDTH 240
#define NATIVE_HEIGHT 320
#define LOGICAL_WIDTH  320  // done for custom function to remap points as bitwise toggle for MV MADCT1 doesnt work on chinese branded ili9341
#define LOGICAL_HEIGHT 240

/*
 * Essential bring-up notes (keep these; strip TEMP experiments):
 * - Nucleo: SB13/SB14 OFF, SB62/SB63 ON so PA2/PA3 reach the headers (HW).
 * - Do not call MX_USART2 while DC/RESET use PA2/PA3.
 * - SD disk init leaves SPI1 in Mode3 — reconfig Mode0 before LCD traffic.
 * - Fill buffer is static: default MSP stack is only 1KB.
 * - PB14 (LCD_CS) is in the .ioc but was never emitted into gpio.c.
 */

static void LCD_BusPrepare(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* USART2 must not own PA2/PA3 (DC/RESET) */
	__HAL_RCC_USART2_CLK_ENABLE();
	__HAL_RCC_USART2_FORCE_RESET();
	__HAL_RCC_USART2_RELEASE_RESET();
	__HAL_RCC_USART2_CLK_DISABLE();

	SD_CS_ForceIdleHigh();

	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	HAL_GPIO_WritePin(DISPLAY_CS_GPIO_PORT, DISPLAY_CS_PIN, GPIO_PIN_SET);
	HAL_GPIO_WritePin(DISPLAY_DC_GPIO_PORT, DISPLAY_DC_PIN, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(DISPLAY_RST_GPIO_PORT, DISPLAY_RST_PIN, GPIO_PIN_SET);

	GPIO_InitStruct.Pin = DISPLAY_CS_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(DISPLAY_CS_GPIO_PORT, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = DISPLAY_DC_PIN | DISPLAY_RST_PIN;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(DISPLAY_DC_GPIO_PORT, &GPIO_InitStruct);
	GPIOA->AFR[0] &= ~(0xFFu << (2 * 4)); /* clear USART2 AF on PA2/PA3 */

	/* ILI9341: SPI Mode0 (SD path may have left Mode3) */
	SD_SPI_Reconfig(SPI_BAUDRATEPRESCALER_8, SPI_POLARITY_LOW, SPI_PHASE_1EDGE);
}

/* HELPER GPIO TOGGLE FUNCTIONS FOR CS, DC, RESET */

static void LCD_CS_High(void)
{
	HAL_GPIO_WritePin(DISPLAY_CS_GPIO_PORT, DISPLAY_CS_PIN, GPIO_PIN_SET);
}

static void LCD_CS_Low(void)
{
	HAL_GPIO_WritePin(DISPLAY_CS_GPIO_PORT, DISPLAY_CS_PIN, GPIO_PIN_RESET);
}

static void LCD_DC_High(void)	/* DC REQUIRED TO BE PULLED HIGH FOR DATA */
{
	HAL_GPIO_WritePin(DISPLAY_DC_GPIO_PORT, DISPLAY_DC_PIN, GPIO_PIN_SET);
}

static void LCD_DC_Low(void)	/* DC REQUIRED TO BE PULLED LOW FOR COMMANDS */
{
	HAL_GPIO_WritePin(DISPLAY_DC_GPIO_PORT, DISPLAY_DC_PIN, GPIO_PIN_RESET);
}

static void LCD_RST_Low(void)
{
	HAL_GPIO_WritePin(DISPLAY_RST_GPIO_PORT, DISPLAY_RST_PIN, GPIO_PIN_RESET);
}

static void LCD_RST_High(void)
{
	HAL_GPIO_WritePin(DISPLAY_RST_GPIO_PORT, DISPLAY_RST_PIN, GPIO_PIN_SET);
}

/* SEND BYTE TO ILI9341 */
void LCD_Send_Command(uint8_t cmd, uint8_t *params, int param_count)
{
	LCD_CS_Low();
	LCD_DC_Low();
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	if (param_count > 0) {
		LCD_DC_High();
		HAL_SPI_Transmit(&hspi1, params, param_count, HAL_MAX_DELAY);
	}
	LCD_CS_High();
}

void LCD_Hardware_Reset(void)
{
	LCD_RST_Low();
	HAL_Delay(15);
	LCD_RST_High();
	HAL_Delay(120);
}

void LCD_Init(void)
{
	LCD_BusPrepare();
	LCD_Hardware_Reset();
	LCD_Send_Command(0x11, NULL, 0);
	HAL_Delay(120); /* blocking delays used in init sequence (reset included) */
	uint8_t pixel_format = 0x55;
	LCD_Send_Command(0x3A, &pixel_format, 1);
	uint8_t madct1 = 0x48;
	LCD_Send_Command(0x36, &madct1, 1);
	LCD_Send_Command(0x29, NULL, 0);
	HAL_Delay(33);
}

void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
	uint8_t col_params[4];
	/* fill col_params using x0/x1 high/low bytes, send via 0x2A (CASET) */
	col_params[0] = x0 >> 8;
	col_params[1] = x0 & 0xFF;
	col_params[2] = x1 >> 8;
	col_params[3] = x1 & 0xFF;
	LCD_Send_Command(0x2A, col_params, 4);
	uint8_t row_params[4];
	row_params[0] = y0 >> 8;
	row_params[1] = y0 & 0xFF;
	row_params[2] = y1 >> 8;
	row_params[3] = y1 & 0xFF;
	LCD_Send_Command(0x2B, row_params, 4);
	/* fill row_params using y0/y1 high/low bytes, send via 0x2B (PASET) */
}

void LCD_SetWindow_Logical(uint16_t lx0, uint16_t ly0, uint16_t lx1, uint16_t ly1){
	uint16_t x0_start = lx0;
	uint16_t y0_start = ly0;
	uint16_t x1_start = lx1;
	uint16_t y1_start = ly1;

	uint16_t x0_end = (LOGICAL_HEIGHT - 1) - y0_start;
	uint16_t y0_end = ((NATIVE_HEIGHT - 1) - x0_start);

	uint16_t x1_end = (LOGICAL_HEIGHT - 1) - y1_start;
	uint16_t y1_end = ((NATIVE_HEIGHT - 1) - x1_start); 	// consistent formula for a 90 degree rotation

	if(y1_end < y0_end){		// ensures correct coordinate order
		uint16_t temp = y1_end;
		y1_end = y0_end;
		y0_end = temp;
	}

	if(x1_end < x0_end){		// ensures correct coordinate order
		uint16_t temp = x1_end;
		x1_end = x0_end;
		x0_end = temp;
	}

	LCD_SetWindow(x0_end, y0_end, x1_end, y1_end);
}

static void LCD_Send_Data(uint8_t *data, uint32_t size)
{
	HAL_SPI_Transmit(&hspi1, data, size, HAL_MAX_DELAY);
}

void LCD_FillScreen(uint16_t colour)
{
	/* static: 640B on a 1KB MSP stack HardFaults mid-fill */
	static uint8_t colour_buffer[640];
	uint8_t ramwr = 0x2C;
	int k;

	LCD_SetWindow_Logical(0, 0, LOGICAL_WIDTH - 1, LOGICAL_HEIGHT - 1);

	LCD_CS_Low();
	LCD_DC_Low();
	HAL_SPI_Transmit(&hspi1, &ramwr, 1, HAL_MAX_DELAY);
	LCD_DC_High();

	/* SPLIT UINT16T VARIABLE INTO UINT8T INTO ARRAY — one row (320 px) */
	for (k = 0; k < 640; k++) {
		if (k % 2 == 0) {
			colour_buffer[k] = colour >> 8;
		} else {
			colour_buffer[k] = colour & 0xFF;
		}
	}

	for (k = 0; k < LOGICAL_HEIGHT; k++) {
		LCD_Send_Data(colour_buffer, 640);
		if ((k & 0x1F) == 0) {
			HAL_IWDG_Refresh(&hiwdg);
		}
	}

	LCD_CS_High();
}

void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour){
	LCD_SetWindow_Logical(x, y, x + w - 1, y + h - 1);
	uint8_t ramwr = 0x2C;
	LCD_CS_Low();
	LCD_DC_Low();
	HAL_SPI_Transmit(&hspi1, &ramwr, 1, HAL_MAX_DELAY); // memory write mode
	LCD_DC_High();
	static uint8_t rect[2000]; // colour data for rectangle
	for (int i = 0; i < 2000; i++){
		if (i % 2 == 0){
			rect[i] = colour >> 8;
		}
		else{
			rect[i] = colour & 0xFF;

		}
	}
	uint32_t total_bytes = w * h * 2;
	uint32_t full_chunks = total_bytes / 2000;
	uint32_t leftover = total_bytes % 2000;

	for (int k = 0; k < full_chunks; k++){
		LCD_Send_Data(rect, 2000);
		if (k % 32 == 0){
			HAL_IWDG_Refresh(&hiwdg);
		}
	}

	if (leftover != 0){
		LCD_Send_Data(rect, leftover);
	}


	LCD_CS_High();
}
