/*
 * Copyright (c) 2026 Luix333
 *
 * SPDX-License-Identifier: MIT
 */

#include <zmk/spacemouse/engine.h>

#define LIMIT_Q8 (SM_ENGINE_LIMIT << SM_ENGINE_SHIFT)  /* 89600 */

void sm_engine_reset(struct sm_engine *e)
{
    for (int i = 0; i < SM_ENGINE_AXES; i++)
        e->acc[i] = 0;
}

bool sm_engine_add(struct sm_engine *e, uint8_t axis_spec, int32_t counts, int32_t gain_q8)
{
    uint8_t axis = axis_spec & SM_ENGINE_AXIS_MASK;
    if (axis > 5)
        return false;

    int64_t delta = (int64_t)counts * (int64_t)gain_q8;
    if (axis_spec & SM_ENGINE_INV)
        delta = -delta;

    int64_t sum = (int64_t)e->acc[axis] + delta;
    if (sum < -(int64_t)LIMIT_Q8)
        sum = -(int64_t)LIMIT_Q8;
    if (sum > (int64_t)LIMIT_Q8)
        sum = (int64_t)LIMIT_Q8;

    e->acc[axis] = (int32_t)sum;
    return true;
}

bool sm_engine_tick(struct sm_engine *e, uint16_t keep_per_mille, uint8_t deadband,
                    int16_t out[SM_ENGINE_AXES])
{
    if (keep_per_mille > 1000)
        keep_per_mille = 1000;

    int32_t zero_below = ((int32_t)deadband << SM_ENGINE_SHIFT) + 128;
    bool active = false;

    for (int i = 0; i < SM_ENGINE_AXES; i++) {
        int32_t v = e->acc[i];

        /* Round half away from zero. */
        int32_t o = v >= 0 ? (v + 128) >> 8 : -(((-v) + 128) >> 8);

        /* Clamp to [-350, 350]. */
        if (o < -350)
            o = -350;
        if (o > 350)
            o = 350;

        /* Deadband. */
        int32_t ao = o >= 0 ? o : -o;
        if (ao <= (int32_t)deadband)
            o = 0;

        out[i] = (int16_t)o;
        if (o != 0)
            active = true;

        /* Decay accumulator. */
        int64_t n = ((int64_t)v * keep_per_mille) / 1000;
        int64_t an = n >= 0 ? n : -n;
        e->acc[i] = (an < (int64_t)zero_below) ? 0 : (int32_t)n;
    }

    return active;
}

bool sm_engine_idle(const struct sm_engine *e)
{
    for (int i = 0; i < SM_ENGINE_AXES; i++)
        if (e->acc[i] != 0)
            return false;
    return true;
}
