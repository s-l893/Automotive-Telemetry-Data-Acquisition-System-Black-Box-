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
    SIG_THROTTLE,
    SIG_SHIFTER,
    SIG_GEAR,
    SIG_VCM,
    SIG_ECT,
    SIG_TRANS_TEMP,
    SIG_COUNT
} signal_id_t;

typedef enum {
    DECODE_RAW16,   /* 2-byte combine, no scale/offset (e.g. RPM) */
    DECODE_LINEAR,  /* 1 byte -> (raw * scale) + offset (e.g. throttle %, temps) */
    DECODE_NIBBLE,  /* 1 byte -> high or low nibble, raw integer (e.g. shifter, gear) */
    DECODE_BIT      /* 1 byte -> single bit, 0/1 (e.g. VCM active) */
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
    uint8_t         byte_offset;   /* primary byte (RAW16 high byte, LINEAR/NIBBLE/BIT byte) */
    uint8_t         byte_offset2;  /* RAW16 low byte only */
    float           scale;         /* LINEAR only */
    float           offset;        /* LINEAR only */
    nibble_select_t nibble;        /* NIBBLE only */
    uint8_t         bit_position;  /* BIT only */
} can_signal_def_t;

typedef struct {
    float values[SIG_COUNT];
    bool  valid[SIG_COUNT]; /* true once at least one frame has decoded this signal */
} vehicle_state_t;

extern vehicle_state_t vehicle_state;

void CAN_Decode_ProcessFrame(uint32_t can_id, const uint8_t *data, uint8_t dlc);

#endif /* INC_CAN_DECODE_H_ */
