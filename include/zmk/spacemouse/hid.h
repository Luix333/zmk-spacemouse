/*
 * Copyright (c) 2026 Luix333
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <zephyr/toolchain.h>

/*
 * HID report map of a 3Dconnexion "SpaceMouse Pro Wireless (cabled)", as reverse-engineered by
 * AndunHH/spacemouse (SpaceMouseHID.h, absolute-input variant). The official 3DxWare driver
 * accepts a device with this map and the USB id 256f:c631. Report ids 1, 3 and 4 collide with
 * ZMK's own (1, 2, 3), which is why this lives on its own USB interface and BLE HID service.
 */

#define ZMK_SPACEMOUSE_REPORT_ID_MOTION 0x01
#define ZMK_SPACEMOUSE_REPORT_ID_BUTTONS 0x03
#define ZMK_SPACEMOUSE_REPORT_ID_LEDS 0x04

/* X, Y, Z, Rx, Ry, Rz as little-endian int16, each -350..350. */
struct zmk_spacemouse_motion_body {
    int16_t axes[6];
} __packed;

struct zmk_spacemouse_motion_report {
    uint8_t id;
    struct zmk_spacemouse_motion_body body;
} __packed;

/* 32 button bits. This module never presses any; the report exists because the driver expects it. */
struct zmk_spacemouse_buttons_body {
    uint8_t bits[4];
} __packed;

struct zmk_spacemouse_buttons_report {
    uint8_t id;
    struct zmk_spacemouse_buttons_body body;
} __packed;

static const uint8_t zmk_spacemouse_report_desc[] = {
    0x05, 0x01,       // Usage Page (Generic Desktop)
    0x09, 0x08,       // Usage (Multi-Axis Controller)
    0xA1, 0x01,       // Collection (Application)
    0xA1, 0x00,       //   Collection (Physical)
    0x85, 0x01,       //     Report ID (1): translation + rotation
    0x16, 0xA2, 0xFE, //     Logical Minimum (-350)
    0x26, 0x5E, 0x01, //     Logical Maximum (350)
    0x36, 0x88, 0xFA, //     Physical Minimum (-1400)
    0x46, 0x78, 0x05, //     Physical Maximum (1400)
    0x55, 0x0C,       //     Unit Exponent (-4)
    0x65, 0x11,       //     Unit (SI Linear, Length: Centimeter)
    0x09, 0x30,       //     Usage (X)
    0x09, 0x31,       //     Usage (Y)
    0x09, 0x32,       //     Usage (Z)
    0x09, 0x33,       //     Usage (Rx)
    0x09, 0x34,       //     Usage (Ry)
    0x09, 0x35,       //     Usage (Rz)
    0x75, 0x10,       //     Report Size (16)
    0x95, 0x06,       //     Report Count (6)
    0x81, 0x02,       //     Input (Data, Var, Abs)
    0xC0,             //   End Collection
    0xA1, 0x00,       //   Collection (Physical)
    0x85, 0x03,       //     Report ID (3): buttons
    0x15, 0x00,       //     Logical Minimum (0)
    0x25, 0x01,       //     Logical Maximum (1)
    0x75, 0x01,       //     Report Size (1)
    0x95, 0x20,       //     Report Count (32)
    0x05, 0x09,       //     Usage Page (Button)
    0x19, 0x01,       //     Usage Minimum (Button 1)
    0x29, 0x20,       //     Usage Maximum (Button 32)
    0x81, 0x02,       //     Input (Data, Var, Abs)
    0xC0,             //   End Collection
    0xA1, 0x02,       //   Collection (Logical)
    0x85, 0x04,       //     Report ID (4): LED, set by the driver
    0x05, 0x08,       //     Usage Page (LEDs)
    0x09, 0x4B,       //     Usage (Generic Indicator)
    0x15, 0x00,       //     Logical Minimum (0)
    0x25, 0x01,       //     Logical Maximum (1)
    0x95, 0x01,       //     Report Count (1)
    0x75, 0x01,       //     Report Size (1)
    0x91, 0x02,       //     Output (Data, Var, Abs)
    0x95, 0x01,       //     Report Count (1)
    0x75, 0x07,       //     Report Size (7)
    0x91, 0x03,       //     Output (Const, Var, Abs)
    0xC0,             //   End Collection
    0xC0,             // End Collection
};
