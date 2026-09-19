/*
 * fsm_ui.c
 *
 * Live vehicle dashboard (320x240). Black background, white telemetry,
 * redline flash on gear/RPM, green leaf when VCM/ECO is active.
 */

#include "fsm_ui.h"
#include "display.h"
#include "can_decode.h"
#include "imu.h"
#include "gps_driver.h"
#include "fault.h"
#include "fsm_sys.h"
#include "main.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

#define UI_REFRESH_MS          50U
#define UI_G_REFRESH_MS        150U
#define IMU_LSB_PER_G          16384.0f
#define REDLINE_START_RPM      6200.0f
#define REDLINE_MAX_RPM        6600.0f
#define REDLINE_PERIOD_SLOW_MS 400U
#define REDLINE_PERIOD_FAST_MS 80U

#define BOX_TEMP_X   2
#define BOX_TEMP_Y   2
#define BOX_TEMP_W   118
#define BOX_TEMP_H   58

#define BOX_G_X      2
#define BOX_G_Y      64
#define BOX_G_W      118
#define BOX_G_H      92

#define BOX_PEAK_X   2
#define BOX_PEAK_Y   168
#define BOX_PEAK_W   118
#define BOX_PEAK_H   70

#define CTR_X        148
#define CTR_Y        8
#define CTR_W        168
#define CTR_H        152

#define GEAR_X       200
#define GEAR_Y       8
#define GEAR_W       110
#define GEAR_H       48

#define RPM_X        168
#define RPM_Y        60
#define RPM_W        140
#define RPM_H        48

#define BOX_LEAF_X   124
#define BOX_LEAF_Y   168
#define BOX_LEAF_W   72
#define BOX_LEAF_H   70

#define BOX_STAT_X   200
#define BOX_STAT_Y   168
#define BOX_STAT_W   118
#define BOX_STAT_H   70

typedef enum {
	UI_LIVE_DATA = 0,
	UI_FAULT_SCREEN,
	UI_TACHOMETER,
	UI_MENU,
} ui_state_t;

static ui_state_t ui_state = UI_LIVE_DATA;
static uint32_t last_ui_ms = 0;
static uint32_t last_g_ui_ms = 0;
static float peak_lat_g = 0.0f;
static float peak_long_g = 0.0f;
static bool chrome_drawn = false;
static bool leaf_visible = false;

static char prev_gear[8];
static char prev_rpm[16];
static char prev_trtl[16];
static char prev_atf[12];
static char prev_ect[12];
static char prev_lat[16];
static char prev_long[16];
static char prev_gps[16];
static char prev_max_lat[16];
static char prev_max_long[16];
static char prev_fault[24];
static char prev_log[24];
static uint16_t prev_gear_fg = 0xFFFF;
static uint16_t prev_rpm_fg = 0xFFFF;
static bool prev_flash_on = true;

static const uint8_t leaf_bits[24 * 3] = {
	0x00, 0x18, 0x00,
	0x00, 0x3C, 0x00,
	0x00, 0x7E, 0x00,
	0x00, 0xFF, 0x00,
	0x01, 0xFF, 0x80,
	0x03, 0xFF, 0xC0,
	0x07, 0xFF, 0xE0,
	0x0F, 0xEF, 0xF0,
	0x1F, 0xC7, 0xF8,
	0x3F, 0x83, 0xFC,
	0x3F, 0x01, 0xFC,
	0x7E, 0x00, 0xFE,
	0x7C, 0x00, 0x7E,
	0x78, 0x00, 0x3E,
	0x70, 0x08, 0x1E,
	0x60, 0x1C, 0x0E,
	0x40, 0x3E, 0x06,
	0x00, 0x7F, 0x00,
	0x00, 0xFF, 0x80,
	0x01, 0xFF, 0xC0,
	0x00, 0xFF, 0x80,
	0x00, 0x7F, 0x00,
	0x00, 0x3E, 0x00,
	0x00, 0x1C, 0x00,
};

static float clampf(float v, float lo, float hi)
{
	if (v < lo) {
		return lo;
	}
	if (v > hi) {
		return hi;
	}
	return v;
}

static float fabsf_local(float v)
{
	return (v < 0.0f) ? -v : v;
}

static void format_gear(char *out, size_t n)
{
	/* Shifter: byte3 & 0x0F. Gear: byte4 & 0x1F (active ratio when rolling/manual). */
	int shifter = (int)vehicle_state.values[SIG_SHIFTER] & 0x0F;
	int gear = (int)vehicle_state.values[SIG_GEAR] & 0x1F;
	char mode = '?';

	if (!vehicle_state.valid[SIG_SHIFTER] && !vehicle_state.valid[SIG_GEAR]) {
		snprintf(out, n, "--");
		return;
	}

	if (vehicle_state.valid[SIG_SHIFTER]) {
		switch (shifter) {
		case 0x00: mode = 'S'; break;
		case 0x01: mode = 'P'; break;
		case 0x02: mode = 'R'; break;
		case 0x04: mode = 'N'; break;
		case 0x08: mode = 'D'; break;
		default: mode = '?'; break;
		}
	}

	if (!vehicle_state.valid[SIG_GEAR] || mode == 'P' || mode == 'R' || mode == 'N') {
		snprintf(out, n, "%c", mode);
	} else if (gear >= 1 && gear <= 6) {
		snprintf(out, n, "%c%d", mode, gear);
	} else if (gear == 10 || gear == 11) {
		snprintf(out, n, "%c-", mode);
	} else {
		/* 0 at rest / solenoid handoff junk — letter only */
		snprintf(out, n, "%c", mode);
	}
}

static void redline_style(float rpm, uint16_t *fg_out, bool *flash_on_out)
{
	if (rpm < REDLINE_START_RPM) {
		*fg_out = LCD_COL_WHITE;
		*flash_on_out = true;
		return;
	}

	float t = clampf((rpm - REDLINE_START_RPM) / (REDLINE_MAX_RPM - REDLINE_START_RPM), 0.0f, 1.0f);
	uint32_t period = REDLINE_PERIOD_SLOW_MS -
			(uint32_t)(t * (float)(REDLINE_PERIOD_SLOW_MS - REDLINE_PERIOD_FAST_MS));
	if (period < REDLINE_PERIOD_FAST_MS) {
		period = REDLINE_PERIOD_FAST_MS;
	}

	uint32_t half = period / 2U;
	if (half == 0U) {
		half = 1U;
	}
	*flash_on_out = ((HAL_GetTick() / half) & 1U) == 0U;
	*fg_out = LCD_COL_RED;
}

static void draw_chrome(void)
{
	LCD_FillScreen(LCD_COL_BLACK);

	LCD_DrawRect(BOX_TEMP_X, BOX_TEMP_Y, BOX_TEMP_W, BOX_TEMP_H, LCD_COL_WHITE);
	LCD_DrawRect(BOX_TEMP_X + BOX_TEMP_W / 2, BOX_TEMP_Y, 1, BOX_TEMP_H, LCD_COL_WHITE);
	LCD_DrawString(BOX_TEMP_X + 6, BOX_TEMP_Y + 4, "ATF", LCD_COL_GRAY, LCD_COL_BLACK, 1);
	LCD_DrawString(BOX_TEMP_X + BOX_TEMP_W / 2 + 6, BOX_TEMP_Y + 4, "ECT", LCD_COL_GRAY, LCD_COL_BLACK, 1);

	LCD_DrawString(BOX_G_X + 4, BOX_G_Y + 4, "LAT G", LCD_COL_GRAY, LCD_COL_BLACK, 1);
	LCD_DrawString(BOX_G_X + 4, BOX_G_Y + 36, "LONG G", LCD_COL_GRAY, LCD_COL_BLACK, 1);

	LCD_DrawRect(BOX_PEAK_X, BOX_PEAK_Y, BOX_PEAK_W, BOX_PEAK_H, LCD_COL_WHITE);
	LCD_DrawString(BOX_PEAK_X + 4, BOX_PEAK_Y + 4, "MAX LAT", LCD_COL_GRAY, LCD_COL_BLACK, 1);
	LCD_DrawString(BOX_PEAK_X + 4, BOX_PEAK_Y + 36, "MAX LONG", LCD_COL_GRAY, LCD_COL_BLACK, 1);

	LCD_DrawRect(BOX_LEAF_X, BOX_LEAF_Y, BOX_LEAF_W, BOX_LEAF_H, LCD_COL_WHITE);
	LCD_DrawString(BOX_LEAF_X + 10, BOX_LEAF_Y + 4, "ECO", LCD_COL_GRAY, LCD_COL_BLACK, 1);

	LCD_DrawRect(BOX_STAT_X, BOX_STAT_Y, BOX_STAT_W, BOX_STAT_H, LCD_COL_WHITE);
	LCD_DrawString(298, RPM_Y + 20, "RPM", LCD_COL_GRAY, LCD_COL_BLACK, 1);

	chrome_drawn = true;
	leaf_visible = false;
	prev_gear[0] = prev_rpm[0] = prev_trtl[0] = '\0';
	prev_atf[0] = prev_ect[0] = prev_lat[0] = prev_long[0] = prev_gps[0] = '\0';
	prev_max_lat[0] = prev_max_long[0] = prev_fault[0] = prev_log[0] = '\0';
	prev_gear_fg = prev_rpm_fg = 0xFFFF;
	prev_flash_on = true;
}

static void update_field(char *prev, size_t prev_n, const char *next,
		uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		uint16_t fg, uint8_t scale, bool force)
{
	if (!force && strncmp(prev, next, prev_n) == 0) {
		return;
	}
	snprintf(prev, prev_n, "%s", next);
	LCD_DrawTextField(x, y, w, h, LCD_COL_BLACK, fg, scale, next);
}

static void format_faults(char *out, size_t n)
{
	if (fault_flags.sd_fault) {
		snprintf(out, n, "SD FAULT");
	} else if (fault_flags.can_fault) {
		snprintf(out, n, "CAN FAULT");
	} else if (fault_flags.imu_fault || fault_flags.imu_handshake_fault) {
		snprintf(out, n, "IMU FAULT");
	} else if (fault_flags.gps_fault) {
		snprintf(out, n, "GPS FAULT");
	} else if (fault_flags.touch_fault) {
		snprintf(out, n, "TOUCH FAULT");
	} else {
		snprintf(out, n, "OK");
	}
}

static void format_logging(char *out, size_t n)
{
	if (current_state == SYS_LOGGING) {
		snprintf(out, n, "LOGGING");
	} else if (current_state == SYS_IDLE) {
		snprintf(out, n, "IDLE");
	} else if (current_state == SYS_FAULT) {
		snprintf(out, n, "FAULT");
	} else {
		snprintf(out, n, "INIT");
	}
}

static void draw_live_data(bool force)
{
	char buf[24];
	float rpm = vehicle_state.valid[SIG_RPM] ? vehicle_state.values[SIG_RPM] : 0.0f;
	float throttle = vehicle_state.valid[SIG_THROTTLE] ? vehicle_state.values[SIG_THROTTLE] : 0.0f;
	float atf = vehicle_state.valid[SIG_TRANS_TEMP] ? vehicle_state.values[SIG_TRANS_TEMP] : 0.0f;
	float ect = vehicle_state.valid[SIG_ECT] ? vehicle_state.values[SIG_ECT] : 0.0f;
	float long_g = (float)imu.accel_x / IMU_LSB_PER_G;
	float lat_g = (float)imu.accel_y / IMU_LSB_PER_G;
	uint16_t warn_fg;
	bool flash_on;
	bool warn_dirty;

	if (fabsf_local(lat_g) > peak_lat_g) {
		peak_lat_g = fabsf_local(lat_g);
	}
	if (fabsf_local(long_g) > peak_long_g) {
		peak_long_g = fabsf_local(long_g);
	}

	redline_style(rpm, &warn_fg, &flash_on);
	warn_dirty = force || (flash_on != prev_flash_on) || (warn_fg != prev_gear_fg);

	if (vehicle_state.valid[SIG_TRANS_TEMP]) {
		snprintf(buf, sizeof(buf), "%dC", (int)(atf + (atf >= 0 ? 0.5f : -0.5f)));
	} else {
		snprintf(buf, sizeof(buf), "--C");
	}
	update_field(prev_atf, sizeof(prev_atf), buf,
			BOX_TEMP_X + 4, BOX_TEMP_Y + 20, 52, 32, LCD_COL_WHITE, 2, force);

	if (vehicle_state.valid[SIG_ECT]) {
		snprintf(buf, sizeof(buf), "%dC", (int)(ect + (ect >= 0 ? 0.5f : -0.5f)));
	} else {
		snprintf(buf, sizeof(buf), "--C");
	}
	update_field(prev_ect, sizeof(prev_ect), buf,
			BOX_TEMP_X + BOX_TEMP_W / 2 + 4, BOX_TEMP_Y + 20, 52, 32, LCD_COL_WHITE, 2, force);

	if (force || ((HAL_GetTick() - last_g_ui_ms) >= UI_G_REFRESH_MS)) {
		last_g_ui_ms = HAL_GetTick();
		snprintf(buf, sizeof(buf), "%+.2f", lat_g);
		update_field(prev_lat, sizeof(prev_lat), buf,
				BOX_G_X + 4, BOX_G_Y + 14, 110, 18, LCD_COL_WHITE, 2, force);
		snprintf(buf, sizeof(buf), "%+.2f", long_g);
		update_field(prev_long, sizeof(prev_long), buf,
				BOX_G_X + 4, BOX_G_Y + 46, 110, 18, LCD_COL_WHITE, 2, force);
		snprintf(buf, sizeof(buf), "%s", gps.locked ? "LOCKED" : "UNLOCKED");
		update_field(prev_gps, sizeof(prev_gps), buf,
				BOX_G_X + 4, BOX_G_Y + 72, 110, 16,
				gps.locked ? LCD_COL_GREEN : LCD_COL_WHITE, 1, force);

		snprintf(buf, sizeof(buf), "%.2f G", peak_lat_g);
		update_field(prev_max_lat, sizeof(prev_max_lat), buf,
				BOX_PEAK_X + 4, BOX_PEAK_Y + 16, 110, 18, LCD_COL_WHITE, 2, force);
		snprintf(buf, sizeof(buf), "%.2f G", peak_long_g);
		update_field(prev_max_long, sizeof(prev_max_long), buf,
				BOX_PEAK_X + 4, BOX_PEAK_Y + 48, 110, 18, LCD_COL_WHITE, 2, force);
	}

	format_gear(buf, sizeof(buf));
	if (warn_dirty || strncmp(prev_gear, buf, sizeof(prev_gear)) != 0 || prev_gear_fg != warn_fg) {
		uint16_t fg = flash_on ? warn_fg : LCD_COL_BLACK;
		snprintf(prev_gear, sizeof(prev_gear), "%s", buf);
		prev_gear_fg = warn_fg;
		LCD_DrawTextField(GEAR_X, GEAR_Y, GEAR_W, GEAR_H, LCD_COL_BLACK, fg, 5, buf);
	}

	if (vehicle_state.valid[SIG_RPM]) {
		snprintf(buf, sizeof(buf), "%d", (int)(rpm + 0.5f));
	} else {
		snprintf(buf, sizeof(buf), "----");
	}
	if (warn_dirty || strncmp(prev_rpm, buf, sizeof(prev_rpm)) != 0 || prev_rpm_fg != warn_fg) {
		uint16_t fg = flash_on ? warn_fg : LCD_COL_BLACK;
		snprintf(prev_rpm, sizeof(prev_rpm), "%s", buf);
		prev_rpm_fg = warn_fg;
		LCD_DrawTextField(RPM_X, RPM_Y, RPM_W, RPM_H, LCD_COL_BLACK, fg, 4, buf);
	}

	if (vehicle_state.valid[SIG_THROTTLE]) {
		snprintf(buf, sizeof(buf), "TRTL:%d%%", (int)(throttle + 0.5f));
	} else {
		snprintf(buf, sizeof(buf), "TRTL:--%%");
	}
	update_field(prev_trtl, sizeof(prev_trtl), buf,
			RPM_X, CTR_Y + 118, CTR_W - 16, 28, LCD_COL_WHITE, 2, force);

	prev_flash_on = flash_on;

	{
		bool vcm_on = vehicle_state.valid[SIG_VCM] && (vehicle_state.values[SIG_VCM] >= 0.5f);
		if (force || vcm_on != leaf_visible) {
			leaf_visible = vcm_on;
			LCD_FillRect(BOX_LEAF_X + 8, BOX_LEAF_Y + 22, 56, 40, LCD_COL_BLACK);
			if (vcm_on) {
				LCD_DrawBitmap1bpp(BOX_LEAF_X + 24, BOX_LEAF_Y + 28, 24, 24,
						leaf_bits, LCD_COL_GREEN, LCD_COL_BLACK);
			}
		}
	}

	format_faults(buf, sizeof(buf));
	update_field(prev_fault, sizeof(prev_fault), buf,
			BOX_STAT_X + 4, BOX_STAT_Y + 10, BOX_STAT_W - 8, 24,
			(fault_flags.sd_fault || fault_flags.can_fault) ? LCD_COL_RED : LCD_COL_WHITE,
			1, force);

	format_logging(buf, sizeof(buf));
	update_field(prev_log, sizeof(prev_log), buf,
			BOX_STAT_X + 4, BOX_STAT_Y + 38, BOX_STAT_W - 8, 24, LCD_COL_WHITE, 1, force);
}

void UI_ResetSessionPeaks(void)
{
	peak_lat_g = 0.0f;
	peak_long_g = 0.0f;
	prev_max_lat[0] = '\0';
	prev_max_long[0] = '\0';
}

void UI_Init(void)
{
	ui_state = UI_LIVE_DATA;
	UI_ResetSessionPeaks();
	draw_chrome();
	draw_live_data(true);
	last_ui_ms = HAL_GetTick();
}

void UI_FSM_Tick(void)
{
	uint32_t now = HAL_GetTick();

	if (!chrome_drawn) {
		draw_chrome();
		draw_live_data(true);
		last_ui_ms = now;
		return;
	}

	if ((now - last_ui_ms) < UI_REFRESH_MS) {
		return;
	}
	last_ui_ms = now;

	switch (ui_state) {
	case UI_LIVE_DATA:
	default:
		draw_live_data(false);
		break;
	}
}
