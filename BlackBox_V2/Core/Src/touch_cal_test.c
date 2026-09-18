/*
 * touch_cal_test.c
 *
 * TEMP two-corner cal via LCD + SD.
 * Press detect = XY moved from idle baseline (Z alone was missing presses).
 * Yellow corner blinks while waiting so you know it's polling.
 */
#include "touch_cal_test.h"

#if TOUCH_CAL_ENABLE

#include "touch_driver.h"
#include "display.h"
#include "sd_spi_bus.h"
#include "fatfs.h"
#include "main.h"
#include "iwdg.h"
#include "spi.h"

#include <stdio.h>
#include <stdbool.h>

#define HOLD_MS      300U
#define XY_DELTA     120U
#define BLINK_MS     400U

static uint16_t bgr565(uint16_t rgb)
{
	return (uint16_t)(((rgb & 0x001FU) << 11) | (rgb & 0x07E0U) | ((rgb & 0xF800U) >> 11));
}

#define COL_BLUE   bgr565(0x001FU)
#define COL_PURPLE bgr565(0xF81FU)
#define COL_YELLOW bgr565(0xFFE0U)
#define COL_GREEN  bgr565(0x07E0U)
#define COL_RED    bgr565(0xF800U)
#define COL_BLACK  0x0000U
#define COL_WHITE  bgr565(0xFFFFU)
#define COL_GRAY   bgr565(0x8410U)

static uint16_t idle_x;
static uint16_t idle_y;

static void lcd_mode0(void)
{
	SD_SPI_Reconfig(SPI_BAUDRATEPRESCALER_8, SPI_POLARITY_LOW, SPI_PHASE_1EDGE);
}

static void sd_mode3(void)
{
	SD_SPI_Reconfig(SPI_BAUDRATEPRESCALER_8, SPI_POLARITY_HIGH, SPI_PHASE_2EDGE);
	SD_CS_ForceIdleHigh();
}

static bool xy_pressed(uint16_t x, uint16_t y)
{
	int dx = (int)x - (int)idle_x;
	int dy = (int)y - (int)idle_y;
	if (dx < 0) {
		dx = -dx;
	}
	if (dy < 0) {
		dy = -dy;
	}
	/* Both axes must leave the idle neighborhood */
	return (dx > (int)XY_DELTA) && (dy > (int)XY_DELTA);
}

static bool sample_pressed(uint16_t *x_out, uint16_t *y_out)
{
	uint16_t x, y;
	Touch_ReadRaw(&x, &y);
	if (x_out) {
		*x_out = x;
	}
	if (y_out) {
		*y_out = y;
	}
	return xy_pressed(x, y) || Touch_IsPressed();
}

static bool stable(bool want_pressed, uint8_t need)
{
	uint8_t hits = 0;
	for (uint8_t i = 0; i < need; i++) {
		if (sample_pressed(NULL, NULL) == want_pressed) {
			hits++;
		}
		HAL_IWDG_Refresh(&hiwdg);
		HAL_Delay(5);
	}
	return hits == need;
}

static bool write_dbg(const char *why, uint16_t x, uint16_t y, uint16_t z1, uint16_t z2)
{
	FIL f;
	UINT bw = 0;
	char buf[256];
	int n;

	sd_mode3();
	n = snprintf(buf, sizeof(buf),
	             "TOUCH DBG: %s\r\n"
	             "idle_x=%u idle_y=%u\r\n"
	             "now_x=%u now_y=%u\r\n"
	             "z1=%u z2=%u\r\n"
	             "SPI2: SCK=PB13 MOSI=PC1 MISO=PC2 CS=PB3\r\n",
	             why,
	             (unsigned)idle_x, (unsigned)idle_y,
	             (unsigned)x, (unsigned)y,
	             (unsigned)z1, (unsigned)z2);
	if (n <= 0 || f_open(&f, "TOUCHDBG.TXT", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
		return false;
	}
	(void)f_write(&f, buf, (UINT)n, &bw);
	f_sync(&f);
	f_close(&f);
	return true;
}

static bool capture_idle(void)
{
	uint32_t sx = 0, sy = 0;
	uint16_t x = 0, y = 0, z1 = 0, z2 = 0;
	uint16_t xmin = 0xFFFF, xmax = 0, ymin = 0xFFFF, ymax = 0;
	uint32_t t0;
	uint32_t n = 0;

	lcd_mode0();
	LCD_FillScreen(COL_WHITE);

	t0 = HAL_GetTick();
	while ((HAL_GetTick() - t0) < 500U) {
		Touch_ReadRaw(&x, &y);
		Touch_ReadPressure(&z1, &z2);
		sx += x;
		sy += y;
		if (x < xmin) {
			xmin = x;
		}
		if (x > xmax) {
			xmax = x;
		}
		if (y < ymin) {
			ymin = y;
		}
		if (y > ymax) {
			ymax = y;
		}
		n++;
		HAL_IWDG_Refresh(&hiwdg);
		HAL_Delay(5);
	}

	if (n == 0U) {
		n = 1U;
	}
	idle_x = (uint16_t)(sx / n);
	idle_y = (uint16_t)(sy / n);

	/*
	 * All zeros = MISO stuck low / CS/SCK/MOSI not reaching the controller.
	 * All ~4095 with pull-up = MISO floating (no device answering).
	 */
	if ((xmax == 0U && ymax == 0U && z1 == 0U && z2 == 0U) ||
	    (xmin == 0U && ymin == 0U && xmax == 0U && ymax == 0U)) {
		(void)write_dbg("SPI dead — all zeros (check CS=PB3 SCK=PB13 MOSI=PC1 MISO=PC2 + 3V3)",
		                x, y, z1, z2);
		lcd_mode0();
		LCD_FillScreen(COL_RED);
		LCD_FillRect(0, 0, 40, 40, COL_YELLOW); /* mark: SPI dead */
		sd_mode3();
		return false;
	}

	if (xmin >= 4000U && ymin >= 4000U) {
		(void)write_dbg("MISO floats high — controller not answering (CS/power/wiring)",
		                x, y, z1, z2);
		lcd_mode0();
		LCD_FillScreen(COL_RED);
		LCD_FillRect(280, 0, 40, 40, COL_YELLOW); /* mark: no ACK */
		sd_mode3();
		return false;
	}

	return true;
}

static void wait_press(uint16_t bg, uint16_t box_x, uint16_t box_y)
{
	uint32_t last_blink = 0;
	bool on = true;
	uint32_t t0 = HAL_GetTick();

	lcd_mode0();
	LCD_FillScreen(bg);
	LCD_FillRect(box_x, box_y, 24, 24, COL_YELLOW);

	for (;;) {
		uint16_t x, y, z1, z2;
		if (stable(true, 6)) {
			return;
		}

		/* Blink marker so you know polling is alive */
		if ((HAL_GetTick() - last_blink) >= BLINK_MS) {
			last_blink = HAL_GetTick();
			on = !on;
			lcd_mode0();
			LCD_FillRect(box_x, box_y, 24, 24, on ? COL_YELLOW : COL_GRAY);
		}

		/* After 20s dump dbg once and keep waiting */
		if ((HAL_GetTick() - t0) > 20000U) {
			Touch_ReadRaw(&x, &y);
			Touch_ReadPressure(&z1, &z2);
			(void)write_dbg("no press after 20s — touch screen hard", x, y, z1, z2);
			t0 = HAL_GetTick();
		}

		HAL_IWDG_Refresh(&hiwdg);
	}
}

static void wait_release(void)
{
	while (!stable(false, 6)) {
		HAL_IWDG_Refresh(&hiwdg);
	}
	HAL_Delay(150);
}

static void capture_corner(uint16_t bg, uint16_t box_x, uint16_t box_y,
                           uint16_t *out_x, uint16_t *out_y)
{
	uint32_t sum_x = 0, sum_y = 0;
	uint16_t n = 0, rx, ry;
	uint32_t hold_start;

	wait_press(bg, box_x, box_y);

	hold_start = HAL_GetTick();
	while ((HAL_GetTick() - hold_start) < HOLD_MS) {
		if (!sample_pressed(&rx, &ry)) {
			wait_press(bg, box_x, box_y);
			hold_start = HAL_GetTick();
			sum_x = sum_y = 0;
			n = 0;
			continue;
		}
		sum_x += rx;
		sum_y += ry;
		n++;
		HAL_IWDG_Refresh(&hiwdg);
		HAL_Delay(5);
	}

	if (n == 0U) {
		Touch_ReadRaw(&rx, &ry);
		sum_x = rx;
		sum_y = ry;
		n = 1U;
	}

	*out_x = (uint16_t)(sum_x / n);
	*out_y = (uint16_t)(sum_y / n);

	lcd_mode0();
	LCD_FillScreen(COL_BLACK);
	wait_release();
}

static bool write_cal_file(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
	FIL f;
	UINT bw = 0;
	char buf[192];
	int n;

	sd_mode3();
	n = snprintf(buf, sizeof(buf),
	             "TOUCH CAL RAW\r\n"
	             "corner1 (pixel 0,239): raw_x=%u raw_y=%u\r\n"
	             "corner2 (pixel 319,0): raw_x=%u raw_y=%u\r\n"
	             "\r\n"
	             "#define raw_x1 %u\r\n"
	             "#define raw_y1 %u\r\n"
	             "#define raw_x2 %u\r\n"
	             "#define raw_y2 %u\r\n",
	             (unsigned)x1, (unsigned)y1,
	             (unsigned)x2, (unsigned)y2,
	             (unsigned)x1, (unsigned)y1,
	             (unsigned)x2, (unsigned)y2);
	if (n <= 0 || f_open(&f, "TOUCHCAL.TXT", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
		return false;
	}
	if (f_write(&f, buf, (UINT)n, &bw) != FR_OK || bw != (UINT)n) {
		f_close(&f);
		return false;
	}
	f_sync(&f);
	f_close(&f);
	return true;
}

void Touch_Cal_Run(void)
{
	uint16_t x1, y1, x2, y2;
	bool ok;

	if (!capture_idle()) {
		return;
	}

	capture_corner(COL_BLUE, 0, 216, &x1, &y1);
	capture_corner(COL_PURPLE, 296, 0, &x2, &y2);

	ok = write_cal_file(x1, y1, x2, y2);
	lcd_mode0();
	LCD_FillScreen(ok ? COL_GREEN : COL_RED);
	sd_mode3();
}

#endif /* TOUCH_CAL_ENABLE */
