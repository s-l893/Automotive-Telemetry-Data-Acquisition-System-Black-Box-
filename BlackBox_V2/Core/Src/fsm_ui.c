/*
 * fsm_ui.c
 *
 *  Created on: Jul 7, 2026
 *      Author: Sunny Lin
 */

// UI LIFECYCLE
typedef enum{
	UI_LIVE_DATA,
	UI_FAULT_SCREEN,
	UI_TACHOMETER,
	UI_MENU,
} ui_state_t;

