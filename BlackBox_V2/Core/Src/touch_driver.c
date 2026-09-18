/*
 * touch_driver.c
 *
 *  Created on: Sep 16, 2026
 *      Author: Sunny Lin
 */

#include "touch_driver.h"
#include "main.h"
#include "fault.h"
#include "spi.h"
#include <math.h>
#include <string.h>
#include <stdbool.h>

#define raw_x1 0
#define raw_y1 4095
#define pixel_x1 0
#define pixel_y1 239
#define raw_x2 4095
#define raw_y2 0
#define pixel_x2 319
#define pixel_y2 0

static volatile bool touch_event_flag = false;
static volatile bool touch_in_progress = false;
static volatile uint32_t touch_timer = 0;
static uint16_t touch_start_raw_x;
static uint16_t touch_start_raw_y;

static uint16_t touch_latest_raw_x;
static uint16_t touch_latest_raw_y;

static int16_t dx;
static int16_t dy;

static uint16_t start_px_x;
static uint16_t start_px_y;

static uint16_t end_px_x;
static uint16_t end_px_y;

static float distance;

touch_data_t touch;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO){
	if (GPIO == TOUCH_IRQ_Pin){
		touch_event_flag = true;
	}

}

void Touch_ToPixel(uint16_t raw_x, uint16_t raw_y, uint16_t *pixel_x, uint16_t *pixel_y){
	float m_x = (float)(pixel_x2 - pixel_x1) / (raw_x2 - raw_x1);
	float b_x = pixel_x1 - m_x * raw_x1;

	float m_y = (float)(pixel_y2 - pixel_y1) / (raw_y2 - raw_y1);
	float b_y = pixel_y1 - m_y * raw_y1;

	*pixel_x = m_x * raw_x + b_x;
	*pixel_y = m_y * raw_y + b_y;
}

void Touch_CS_High(void){
	HAL_GPIO_WritePin(CS_SPI2_GPIO_Port, CS_SPI2_Pin, GPIO_PIN_SET);
}

void Touch_CS_Low(void){
	HAL_GPIO_WritePin(CS_SPI2_GPIO_Port, CS_SPI2_Pin, GPIO_PIN_RESET);
}

/* 12-bit result from XPT2046/ADS7843 8-bit SPI framing.
 * Low 2 bits of cmd = PD1:PD0 = 11 keep ADC on between conversions. */
static uint16_t touch_spi_cmd(uint8_t cmd)
{
	uint8_t tx[3] = {cmd, 0x00, 0x00};
	uint8_t rx[3] = {0};
	HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3, HAL_MAX_DELAY);
	return (uint16_t)((((uint16_t)rx[1] << 8) | rx[2]) >> 3);
}

void Touch_ReadRaw(uint16_t *raw_x, uint16_t *raw_y){
	Touch_CS_Low();
	HAL_Delay(1);
	(void)touch_spi_cmd(0xD3); /* X, ADC on */
	*raw_x = touch_spi_cmd(0xD3);
	(void)touch_spi_cmd(0x93); /* Y */
	*raw_y = touch_spi_cmd(0x93);
	(void)touch_spi_cmd(0xD0); /* power down */
	Touch_CS_High();
}

bool Touch_IsPressed(void)
{
	uint16_t z1, z2;
	uint32_t z;

	Touch_CS_Low();
	HAL_Delay(1);
	z1 = touch_spi_cmd(0xB3);
	z2 = touch_spi_cmd(0xC3);
	(void)touch_spi_cmd(0xD0);
	Touch_CS_High();

	if ((z1 == 0U && z2 == 0U) || (z1 >= 4090U && z2 >= 4090U)) {
		return false;
	}

	z = (uint32_t)z1 + 4095U - (uint32_t)z2;
	return z > 400U;
}

void Touch_ReadPressure(uint16_t *z1_out, uint16_t *z2_out)
{
	Touch_CS_Low();
	HAL_Delay(1);
	*z1_out = touch_spi_cmd(0xB3);
	*z2_out = touch_spi_cmd(0xC3);
	(void)touch_spi_cmd(0xD0);
	Touch_CS_High();
}

void Touch_Init(void){
	/* Ensure CS is a real GPIO (PB3 is JTDO after reset until reconfigured) */
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	__HAL_RCC_GPIOB_CLK_ENABLE();
	HAL_GPIO_WritePin(CS_SPI2_GPIO_Port, CS_SPI2_Pin, GPIO_PIN_SET);
	GPIO_InitStruct.Pin = CS_SPI2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(CS_SPI2_GPIO_Port, &GPIO_InitStruct);

	Touch_CS_High();
	touch_event_flag = false;
	memset(&touch, 0, sizeof(touch));
}



void Touch_Update(void){

	if(!touch_in_progress){ // Z-pressure only (PC13 IRQ unreliable on Nucleo)
		if (Touch_IsPressed()){
			touch_timer = HAL_GetTick();
			touch_in_progress = true;

			Touch_ReadRaw(&touch_start_raw_x, &touch_start_raw_y);
			touch_event_flag = false;
		}
	}
	else{
		if(Touch_IsPressed()){ // continues updating the latest touch point
			Touch_ReadRaw(&touch_latest_raw_x, &touch_latest_raw_y);
		}

		else{ // case for the cycle right after touch is removed
			Touch_ReadRaw(&touch_latest_raw_x, &touch_latest_raw_y); // final read for update
			// convert to pixels from analog value
			Touch_ToPixel(touch_start_raw_x, touch_start_raw_y, &start_px_x, &start_px_y);
			Touch_ToPixel(touch_latest_raw_x, touch_latest_raw_y, &end_px_x, &end_px_y);
			// compute total distance
			dx = end_px_x - start_px_x;
			dy = end_px_y - start_px_y;


			distance = sqrtf(powf(dx, 2) + powf(dy, 2)); // computer actual distance

			if (distance > 40){ // determine swipe or touch
				touch.type = TOUCH_SWIPE;
				// figure out direction of swipe
				if (dx > 0){
					touch.swipe_direction = SWIPE_RIGHT;
				}
				else if(dx < 0){
					touch.swipe_direction = SWIPE_LEFT;
				}

			}
			else{
				touch.type = TOUCH_TAP;
				touch.tap.pixel_x = start_px_x;
				touch.tap.pixel_y = start_px_y;
			}

			touch.gesture_ready = true;
			touch_in_progress = false;
		}
		if ((HAL_GetTick() - touch_timer) > 12000){
			touch_in_progress = false;
			fault_flags.touch_fault = true;
		}
	}
}
