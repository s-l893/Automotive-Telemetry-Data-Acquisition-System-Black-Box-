/*
 * display.c
 *
 *  Created on: Sep 11, 2026
 *      Author: Sunny Lin
 */
#include "fsm_sys.c"
#include "fsm_ui.c"
#include "display.h"

#include <stdint.h>



#define DISPLAY_CS_PIN        GPIO_PIN_14
#define DISPLAY_CS_GPIO_PORT  GPIOB
#define DISPLAY_DC_PIN		  GPIO_PIN_2
#define DISPLAY_DC_GPIO_PORT  GPIOA
#define DISPLAY_RST_PIN		  GPIO_PIN_3
#define DISPLAY_RST_GPIO_PORT GPIOA
#define WIDTH 320
#define HEIGHT 240

// HELPER GPIO TOGGLE FUNCTIONS FOR CS, DC, RESET

static void LCD_CS_High(void)
{
	HAL_GPIO_WritePin(DISPLAY_CS_GPIO_PORT, DISPLAY_CS_PIN, GPIO_PIN_SET);
}

static void LCD_CS_Low(void)
{
	HAL_GPIO_WritePin(DISPLAY_CS_GPIO_PORT, DISPLAY_CS_PIN, GPIO_PIN_RESET);
}

static void LCD_DC_High(void)	// DC REQUIRED TO BE PULLED HIGH FOR DATA
{
	HAL_GPIO_WritePin(DISPLAY_DC_GPIO_PORT, DISPLAY_DC_PIN, GPIO_PIN_SET);
}

static void LCD_DC_Low(void)	// DC REQUIRED TO BE PULLED LOW FOR COMMANDS
{
	HAL_GPIO_WritePin(DISPLAY_DC_GPIO_PORT, DISPLAY_DC_PIN, GPIO_PIN_RESET);
}

static void LCD_RST_Low(void) {
	HAL_GPIO_WritePin(DISPLAY_RST_GPIO_PORT, DISPLAY_RST_PIN, GPIO_PIN_RESET);
}

static void LCD_RST_High(void){
	HAL_GPIO_WritePin(DISPLAY_RST_GPIO_PORT, DISPLAY_RST_PIN, GPIO_PIN_SET);
}

// SEND BYTE TO ILI9341
void LCD_Send_Command(uint8_t cmd, uint8_t *params, int param_count){
	LCD_CS_Low();
	LCD_DC_Low();
	HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);
	if (param_count > 0){
		LCD_DC_High();
		HAL_SPI_Transmit(&hspi1, params, param_count, HAL_MAX_DELAY);
	}
	LCD_CS_High();

}

void LCD_Hardware_Reset(void){
	LCD_RST_Low();
	HAL_Delay(15);
	LCD_RST_High();
	HAL_Delay(120);
}

void LCD_Init(){
	LCD_Hardware_Reset();
	LCD_Send_Command(0x11, NULL, 0);
	HAL_Delay(120); // blocking delays used in init sequence (reset included)
	uint8_t pixel_format = 0x55;
	LCD_Send_Command(0x3A, &pixel_format, 1);
	uint8_t madct1 = 0x28;
	LCD_Send_Command(0x36, &madct1, 1);
	LCD_Send_Command(0x29, NULL, 0);
	HAL_Delay(33);
}

void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1){
    uint8_t col_params[4];
    // fill col_params using x0/x1 high/low bytes, send via 0x2A (CASET)
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
    // fill row_params using y0/y1 high/low bytes, send via 0x2B (PASET)
}

static void LCD_Send_Data (uint8_t *data, uint32_t size){
	HAL_SPI_Transmit(&hspi1, data, size, HAL_MAX_DELAY);
}

void LCD_FillScreen(uint16_t colour){
	LCD_SetWindow(0,0, WIDTH - 1, HEIGHT - 1);
	LCD_CS_Low();
	LCD_DC_Low();

	uint8_t data = 0x2C;
	HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);
	LCD_DC_High();

	uint8_t colour_buffer[480]; // FILL BUFFER IN SMALLER CHUNKS FOR RAM COMPATIBILITY

	for (int k = 0; k < 480; k++){ // SPLIT UINT16T VARIABLE INTO UINT8T INTO ARRAY
		if (k % 2 == 0){
			colour_buffer[k] = colour >> 8;
		}
		else{
			colour_buffer[k] = colour & 0xFF;
	}
	}

	for (int i = 0; i < 320; i++){
		LCD_Send_Data(colour_buffer, 480);
	}

	LCD_CS_High();
}
