/*
 * display.h
 *
 *  Created on: Sep 11, 2026
 *      Author: Sunny Lin
 */

#ifndef INC_DISPLAY_H_
#define INC_DISPLAY_H_
#include <stdint.h>

/* ILI9341 panel on this board needs R/B swap vs standard RGB565 */
static inline uint16_t LCD_BGR565(uint16_t rgb)
{
	return (uint16_t)(((rgb & 0x001FU) << 11) | (rgb & 0x07E0U) | ((rgb & 0xF800U) >> 11));
}

#define LCD_COL_BLACK  0x0000U
#define LCD_COL_WHITE  LCD_BGR565(0xFFFFU)
#define LCD_COL_RED    LCD_BGR565(0xF800U)
#define LCD_COL_GREEN  LCD_BGR565(0x07E0U)
#define LCD_COL_YELLOW LCD_BGR565(0xFFE0U)
#define LCD_COL_GRAY   LCD_BGR565(0x8410U)
#define LCD_COL_DARK   LCD_BGR565(0x2104U)

#define LCD_WIDTH  320
#define LCD_HEIGHT 240

void LCD_Send_Command(uint8_t cmd, uint8_t *params, int param_count);
void LCD_Hardware_Reset(void);
void LCD_Init(void);
void LCD_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void LCD_SetWindow_Logical(uint16_t lx0, uint16_t ly0, uint16_t lx1, uint16_t ly1);
void LCD_FillScreen(uint16_t colour);
void LCD_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour);
void LCD_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t colour);
void LCD_DrawChar(uint16_t x, uint16_t y, char c, uint16_t fg, uint16_t bg, uint8_t scale);
void LCD_DrawString(uint16_t x, uint16_t y, const char *s, uint16_t fg, uint16_t bg, uint8_t scale);
void LCD_DrawTextField(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		uint16_t bg, uint16_t fg, uint8_t scale, const char *s);
void LCD_DrawBitmap1bpp(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		const uint8_t *bits, uint16_t fg, uint16_t bg);

#endif /* INC_DISPLAY_H_ */
