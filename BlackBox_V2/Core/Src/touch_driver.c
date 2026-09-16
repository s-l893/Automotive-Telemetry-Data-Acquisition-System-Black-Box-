/*
 * touch_driver.c
 *
 *  Created on: Sep 16, 2026
 *      Author: Sunny Lin
 */

#include "touch_driver.h"
#include "main.h"

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

void Touch_ReadRaw(uint16_t *raw_x, uint16_t *raw_y){
	uint16_t tx = {control_byte, 0, 0};
	uint16_t rx = {;
	HAL_SPI_TransmitReceive(&hspi2, *tx, *rx, 1, HAL_MAX_DELAY);
}
