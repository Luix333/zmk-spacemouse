/*
 * Copyright (c) 2026 Luix333
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

/* TX/TY/TZ = translations X, Y, Z */
#define SM_TX 0
#define SM_TY 1
#define SM_TZ 2

/* RX/RY/RZ = rotations about X, Y, Z */
#define SM_RX 3
#define SM_RY 4
#define SM_RZ 5

/* SM_INV is OR'd onto an axis to flip it */
#define SM_INV 0x80

/* SM_NONE drops that ball axis */
#define SM_NONE 0xFF
