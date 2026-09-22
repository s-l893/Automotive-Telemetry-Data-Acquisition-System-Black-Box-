/*
 * can_decode.c
 *
 *  Created on: Sep 17, 2026
 *      Author: Sunny Lin
 */


#include "can_decode.h"

vehicle_state_t vehicle_state;

/*
 * 2016 Accord V6 6AT: subgear is estimated from RPM/speed ratio (not a CAN PID).
 * RPM + speed share 0x158; shifter PRNDS on 0x188.
 */
static const can_signal_def_t can_signal_table[] = {
    // signal_id        name          can_id  type            b0 b1  scale           offset   nibble       bit
    { SIG_SPEED_KPH,  "Speed",      0x158, DECODE_RAW16,   0, 1, 0.01f,           0.0f,   NIBBLE_HIGH, 0 },
    { SIG_RPM,        "RPM",        0x158, DECODE_RAW16,   2, 3, 0.0f,            0.0f,   NIBBLE_HIGH, 0 },
    { SIG_THROTTLE,   "Throttle",   0x17C, DECODE_LINEAR,  0, 0, 100.0f / 255.0f, 0.0f,   NIBBLE_HIGH, 0 },
    /* 0x188 byte3: 0=S, 1=P, 2=R, 4=N, 8=D */
    { SIG_SHIFTER,    "Shifter",    0x188, DECODE_NIBBLE,  3, 0, 0.0f,            0.0f,   NIBBLE_LOW,  0 },
    { SIG_VCM,        "VCM",        0x1A6, DECODE_LINEAR,  1, 0, 1.0f,            0.0f,   NIBBLE_HIGH, 0 },
    { SIG_ECT,        "ECT",        0x324, DECODE_LINEAR,  0, 0, 1.0f,            -40.0f, NIBBLE_HIGH, 0 },
    { SIG_TRANS_TEMP, "TransTemp",  0x324, DECODE_LINEAR,  4, 0, 1.0f,            -40.0f, NIBBLE_HIGH, 0 },
};

#define CAN_SIGNAL_TABLE_COUNT (sizeof(can_signal_table) / sizeof(can_signal_table[0]))

static float Decode_Raw16(const uint8_t *data, const can_signal_def_t *def)
{
    float raw = (float)((data[def->byte_offset] << 8) | data[def->byte_offset2]);
    if (def->scale != 0.0f) {
        raw *= def->scale;
    }
    return raw;
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

/*
 * 6AT ratio windows: R = RPM / speed_kph (factory gear ratios × final drive).
 * Only meaningful in D/S above ~5 km/h (converter slip below that).
 */
static void estimate_subgear(void)
{
    int shifter;
    float rpm;
    float speed;
    float ratio;
    uint8_t gear;

    if (!vehicle_state.valid[SIG_SHIFTER]) {
        return;
    }

    shifter = (int)vehicle_state.values[SIG_SHIFTER] & 0x0F;
    /* Forward drive only: 0x08=D, 0x00=S */
    if (shifter != 0x08 && shifter != 0x00) {
        vehicle_state.values[SIG_GEAR] = 0.0f;
        vehicle_state.valid[SIG_GEAR] = true;
        return;
    }

    if (!vehicle_state.valid[SIG_RPM] || !vehicle_state.valid[SIG_SPEED_KPH]) {
        return;
    }

    rpm = vehicle_state.values[SIG_RPM];
    speed = vehicle_state.values[SIG_SPEED_KPH];

    if (speed < 5.0f) {
        gear = 1U; /* creeping / stopped in D/S */
    } else {
        ratio = rpm / speed;
        if (ratio >= 85.0f) {
            gear = 1U;
        } else if (ratio >= 55.0f) {
            gear = 2U;
        } else if (ratio >= 40.0f) {
            gear = 3U;
        } else if (ratio >= 29.0f) {
            gear = 4U;
        } else if (ratio >= 21.0f) {
            gear = 5U;
        } else {
            gear = 6U;
        }
    }

    vehicle_state.values[SIG_GEAR] = (float)gear;
    vehicle_state.valid[SIG_GEAR] = true;
}

void CAN_Decode_ProcessFrame(uint32_t can_id, const uint8_t *data, uint8_t dlc)
{
    (void)dlc;

    for (uint32_t i = 0; i < CAN_SIGNAL_TABLE_COUNT; i++) {
        const can_signal_def_t *def = &can_signal_table[i];
        if (def->can_id != can_id) {
            continue;
        }

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

    if (can_id == 0x158U || can_id == 0x188U) {
        estimate_subgear();
    }
}
