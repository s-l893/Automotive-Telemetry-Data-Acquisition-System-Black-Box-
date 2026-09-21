/*
 * can_decode.c
 *
 *  Created on: Sep 17, 2026
 *      Author: Sunny Lin
 */


#include "can_decode.h"

vehicle_state_t vehicle_state;

// PIDS FOR 2016 HONDA ACCORD V6: SHOULD ADDITIONAL PIDS BE LOGGED IN THE FUTURE, IT CAN EASILY BE UPDATED WITHOUT ADDING NEW LOGIC
// (UNLESS A NEW DATA TYPE IS INTRODUCED)
static const can_signal_def_t can_signal_table[] = {
    // signal_id        name          can_id  type            b0 b1  scale           offset   nibble       bit
    { SIG_RPM,        "RPM",        0x158, DECODE_RAW16,   2, 3, 0.0f,            0.0f,   NIBBLE_HIGH, 0 },
    { SIG_THROTTLE,   "Throttle",   0x17C, DECODE_LINEAR,  0, 0, 100.0f / 255.0f, 0.0f,   NIBBLE_HIGH, 0 },
    /* 0x188: PRNDS bitmask only (0=S,1=P,2=R,4=N,8=D). Byte0 is solenoid state, not 1-6. */
    { SIG_SHIFTER,    "Shifter",    0x188, DECODE_NIBBLE,  3, 0, 0.0f,            0.0f,   NIBBLE_LOW,  0 },
    /* Engaged ratio 1-6 on 0x1A4 (original 2016 map) — separate from shifter on 0x188 */
    { SIG_GEAR,       "Gear",       0x1A4, DECODE_NIBBLE,  1, 0, 0.0f,            0.0f,   NIBBLE_LOW,  0 },
    /* VCM: store full byte1; UI tests bit0 (and bit1 as fallback) */
    { SIG_VCM,        "VCM",        0x1A6, DECODE_LINEAR,  1, 0, 1.0f,            0.0f,   NIBBLE_HIGH, 0 },
    { SIG_ECT,        "ECT",        0x324, DECODE_LINEAR,  0, 0, 1.0f,            -40.0f, NIBBLE_HIGH, 0 },
    { SIG_TRANS_TEMP, "TransTemp",  0x324, DECODE_LINEAR,  4, 0, 1.0f,            -40.0f, NIBBLE_HIGH, 0 },
};

#define CAN_SIGNAL_TABLE_COUNT (sizeof(can_signal_table) / sizeof(can_signal_table[0]))

static float Decode_Raw16(const uint8_t *data, const can_signal_def_t *def)
{
    return (float)((data[def->byte_offset] << 8) | data[def->byte_offset2]);
}

static float Decode_Linear(const uint8_t *data, const can_signal_def_t *def)
{
    return ((float)data[def->byte_offset] * def->scale) + def->offset;
}

static float Decode_Nibble(const uint8_t *data, const can_signal_def_t *def)
{
    uint8_t byte = data[def->byte_offset];
    if (def->nibble == NIBBLE_HIGH) {
        return (float)((byte >> 4) & 0x0F);
    }
    return (float)(byte & 0x0F);
}

static float Decode_Bit(const uint8_t *data, const can_signal_def_t *def)
{
    return (float)((data[def->byte_offset] >> def->bit_position) & 0x01);
}

void CAN_Decode_ProcessFrame(uint32_t can_id, const uint8_t *data, uint8_t dlc)
{
    (void)dlc;

    for (uint32_t i = 0; i < CAN_SIGNAL_TABLE_COUNT; i++) {
        const can_signal_def_t *def = &can_signal_table[i];
        if (def->can_id != can_id) {
            continue;
        }
// METHODS OF DECODING TYPES OF DATA
        float value;
        switch (def->decode_type) {
            case DECODE_RAW16:
                value = Decode_Raw16(data, def);
                break;
            case DECODE_LINEAR:
                value = Decode_Linear(data, def);
                break;
            case DECODE_NIBBLE:
                value = Decode_Nibble(data, def);
                break;
            case DECODE_BIT:
                value = Decode_Bit(data, def);
                break;
            default:
                continue;
        }

        vehicle_state.values[def->signal_id] = value;
        vehicle_state.valid[def->signal_id]  = true;
    }
}
