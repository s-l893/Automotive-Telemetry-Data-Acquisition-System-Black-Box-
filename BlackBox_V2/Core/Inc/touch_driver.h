/*
 * touch_driver.h
 *
 *  Created on: Sep 16, 2026
 *      Author: Sunny Lin
 */

#ifndef TARGET_TOUCH_DRIVER_H_
#define TARGET_TOUCH_DRIVER_H_

#include <stdint.h>
#include <stdbool.h>

void Touch_ToPixel(uint16_t raw_x, uint16_t raw_y, uint16_t *pixel_x, uint16_t *pixel_y);
void Touch_CS_High(void);
void Touch_CS_Low(void);
void Touch_ReadRaw(uint16_t *raw_x, uint16_t *raw_y);
bool Touch_IsPressed(void);
void Touch_ReadPressure(uint16_t *z1_out, uint16_t *z2_out);
void Touch_Init(void);
void Touch_Update(void);

typedef enum {
    TOUCH_NONE,
    TOUCH_TAP,
    TOUCH_SWIPE
} touch_gesture_t;

typedef enum {
    SWIPE_LEFT,
    SWIPE_RIGHT
} swipe_dir_t;

typedef struct {
    bool gesture_ready;
    touch_gesture_t type;
    union {
        struct {
            uint16_t pixel_x;
            uint16_t pixel_y;
        } tap;
        swipe_dir_t swipe_direction;
    };
} touch_data_t;

extern touch_data_t touch;
#endif /* TARGET_TOUCH_DRIVER_H_ */

