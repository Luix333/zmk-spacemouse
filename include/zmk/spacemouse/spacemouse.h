/*
 * Copyright (c) 2026 Luix333
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include <zmk/spacemouse/hid.h>

/*
 * Feed trackball counts into one SpaceMouse axis. axis_spec is SM_TX..SM_RZ from
 * <dt-bindings/zmk/spacemouse.h>, optionally OR'd with SM_INV; SM_NONE drops the motion.
 * Safe to call from the input thread. Returns 0, or -EINVAL for an invalid axis_spec.
 */
int zmk_spacemouse_add(uint8_t axis_spec, int32_t counts, int32_t gain_q8);

/* The motion values most recently sent to the host (wire format, little endian). */
void zmk_spacemouse_get_motion(struct zmk_spacemouse_motion_body *body);

/* Transports, implemented in usb_hid.c and hog.c. */
int zmk_spacemouse_usb_send(const struct zmk_spacemouse_motion_body *body);
int zmk_spacemouse_hog_send(const struct zmk_spacemouse_motion_body *body);
