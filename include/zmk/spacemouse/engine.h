/*
 * Copyright (c) 2026 Luix333
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>

#define SM_ENGINE_AXES 6
#define SM_ENGINE_LIMIT 350   /* report units */
#define SM_ENGINE_SHIFT 8     /* accumulators are Q8 fixed point */
#define SM_ENGINE_AXIS_MASK 0x07
#define SM_ENGINE_INV 0x80
#define SM_ENGINE_NONE 0xFF

struct sm_engine {
    int32_t acc[SM_ENGINE_AXES]; /* Q8; always within +-(SM_ENGINE_LIMIT << SM_ENGINE_SHIFT) */
};

/* Zero all accumulator slots. */
void sm_engine_reset(struct sm_engine *e);

/* Add trackball counts to the given axis.  axis_spec encodes the axis number
   (lower 3 bits), optional inversion (bit 7), and is otherwise ignored.
   Returns false if the axis number exceeds 5 (including SM_ENGINE_NONE). */
bool sm_engine_add(struct sm_engine *e, uint8_t axis_spec, int32_t counts, int32_t gain_q8);

/* Produce one report (int16_t output per axis) and decay the accumulators.
   keep_per_mille controls the decay rate (capped at 1000).  deadband is in
   report units.  Returns true if any output axis is non-zero. */
bool sm_engine_tick(struct sm_engine *e, uint16_t keep_per_mille, uint8_t deadband,
                    int16_t out[SM_ENGINE_AXES]);

/* Return true when every accumulator slot is zero. */
bool sm_engine_idle(const struct sm_engine *e);
