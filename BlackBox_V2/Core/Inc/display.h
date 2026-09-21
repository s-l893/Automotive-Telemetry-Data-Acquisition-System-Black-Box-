/*
 * display.h
 *
 *  Created on: Sep 11, 2026
 *      Author: Sunny Lin
 */

#ifndef INC_DISPLAY_H_
#define INC_DISPLAY_H_
#include <stdint.h>

void LCD_Send_Command(uint8_t cmd, uint8_t *params, int param_count);
void LCD_Hardware_Reset(void);
void LCD_Init(void);
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_SetWindow_Logical(uint16_t lx0, uint16_t ly0, uint16_t lx1, uint16_t ly1);
void LCD_FillScreen(uint16_t colour);
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour);

#endif /* INC_DISPLAY_H_ */
