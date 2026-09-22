/*
 * can_decode.h
 *
 *  Created on: Sep 17, 2026
 *      Author: Sunny Lin
 */

#ifndef INC_CAN_DECODE_H_
#define INC_CAN_DECODE_H_

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SIG_RPM = 0,
    SIG_SPEED_KPH,  /* vehicle speed km/h from 0x158 bytes 0-1 */
    SIG_THROTTLE,
    SIG_SHIFTER,
    SIG_GEAR,       /* estimated 1-6 from RPM/speed ratio in D/S */
    SIG_VCM,
    SIG_ECT,
    SIG_TRANS_TEMP,
    SIG_COUNT
} signal_id_t;

typedef enum {
    DECODE_RAW16,   /* 2-byte combine; optional scale if scale != 0 */
    DECODE_LINEAR,  /* 1 byte -> (raw * scale) + offset */
    DECODE_NIBBLE,  /* high or low nibble */
    DECODE_BIT      /* single bit */
} decode_type_t;

typedef enum {
    NIBBLE_HIGH,
    NIBBLE_LOW
} nibble_select_t;

typedef struct {
    signal_id_t     signal_id;
    const char     *name;
    uint32_t        can_id;
    decode_type_t   decode_type;
    uint8_t         byte_offset;
    uint8_t         byte_offset2;
    float           scale;
    float           offset;
    nibble_select_t nibble;
    uint8_t         bit_position;
} can_signal_def_t;

typedef struct {
    float values[SIG_COUNT];
    bool  valid[SIG_COUNT];
} vehicle_state_t;

extern vehicle_state_t vehicle_state;

void CAN_Decode_ProcessFrame(uint32_t can_id, const uint8_t *data, uint8_t dlc);

#endif /* INC_CAN_DECODE_H_ */
