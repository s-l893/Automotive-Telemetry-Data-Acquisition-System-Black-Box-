/*
 * touch_cal_test.h
 *
 * TEMP: two-corner raw ADC capture — results written to SD as TOUCHCAL.TXT
 * LCD colors guide you (no UART required).
 *
 * Set TOUCH_CAL_ENABLE to 0 (or delete this file + touch_cal_test.c and
 * the #include / call in main.c) when done.
 */
#ifndef INC_TOUCH_CAL_TEST_H_
#define INC_TOUCH_CAL_TEST_H_

#ifndef TOUCH_CAL_ENABLE
#define TOUCH_CAL_ENABLE 1
#endif

#if TOUCH_CAL_ENABLE
void Touch_Cal_Run(void);
#endif

#endif /* INC_TOUCH_CAL_TEST_H_ */
