/*
 * touch_driver.c
 *
 *  Created on: Sep 16, 2026
 *      Author: Sunny Lin
 */

#include "touch_driver.h"
#include "main.h"
#include "fault.h"
#include <math.h>

#include <stdbool.h>

#define rawx1 0
#define rawy1 4095
#define pixelx1 0
#define pixely1 239
#define rawx2 4095
#define rawy2 0
#define pixelx2 319
#define pixely2 0

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
	HAL_GPIO_WritePin(CS_SPI2_Port, CS_SPI2_Pin, GPIO_PIN_SET);
}

void Touch_CS_Low(void){
	HAL_GPIO_WritePin(CS_SPI2_Port, CS_SPI2_Pin, GPIO_PIN_RESET);
}

void Touch_ReadRaw(uint16_t *raw_x, uint16_t *raw_y){
	uint8_t control_byte_x = 0xD0;
	uint8_t control_byte_y = 0x90;

	Touch_CS_Low();

	uint8_t tx[3] = {control_byte_x, 0, 0};
	uint8_t rx[3];
	HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3, HAL_MAX_DELAY);
	uint16_t raw_value_x = (rx[1] << 8) | (rx[2] >> 4);
	Touch_CS_High();

	Touch_CS_Low();
	tx[0] = control_byte_y;
	HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3, HAL_MAX_DELAY);
	uint16_t raw_value_y = (rx[1] << 8) | (rx[2] >> 4);
	Touch_CS_High();

	*raw_x = raw_value_x;
	*raw_y = raw_value_y;

}

void Touch_Init(void){
	Touch_CS_High();
	touch_event_flag = false;

}



void Touch_Update(void){

	if(!touch_in_progress){ // handles initial touch event from ISR
		if (touch_event_flag){
			touch_timer = HAL_GetTick();
			touch_in_progress = true;

			Touch_ReadRaw(&touch_start_raw_x, &touch_start_raw_y);
			touch_event_flag = false;
		}
	}
	else{
		if(HAL_GPIO_ReadPin(TOUCH_IRQ_GPIO_PORT, TOUCH_IRQ_Pin) == GPIO_PIN_RESET){ // continues updating the latest touch point
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
