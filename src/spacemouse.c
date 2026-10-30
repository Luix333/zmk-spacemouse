/*
 * Copyright (c) 2026 Luix333
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/endpoints.h>
#include <zmk/spacemouse/engine.h>
#include <zmk/spacemouse/spacemouse.h>

/*
 * A SpaceMouse reports how far its cap is pushed, which CAD programs treat as a velocity.
 * Trackball counts push a virtual cap (the engine accumulators) that springs back once the
 * ball stops. Every REPORT_INTERVAL_MS the current deflection goes to the host; once it is
 * back at zero, ZERO_REPEATS all-zero reports follow, so a single lost report can't leave the
 * view spinning, and then the tick stops until the next motion.
 */

static struct sm_engine engine;
static struct k_spinlock engine_lock;

/* Only touched from tick_work, which never runs concurrently with itself. */
static struct zmk_spacemouse_motion_body last_sent;
static uint8_t zeros_left;

static void tick_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(tick_work, tick_handler);

static int send_motion(const struct zmk_spacemouse_motion_body *body) {
    struct zmk_endpoint_instance endpoint = zmk_endpoint_get_selected();

    switch (endpoint.transport) {
#if IS_ENABLED(CONFIG_ZMK_USB)
    case ZMK_TRANSPORT_USB:
        return zmk_spacemouse_usb_send(body);
#endif
#if IS_ENABLED(CONFIG_ZMK_BLE)
    case ZMK_TRANSPORT_BLE:
        return zmk_spacemouse_hog_send(body);
#endif
    default:
        return -ENOTCONN;
    }
}

static void tick_handler(struct k_work *work) {
    int16_t out[SM_ENGINE_AXES];
    bool moving, idle;

    k_spinlock_key_t key = k_spin_lock(&engine_lock);
    moving = sm_engine_tick(&engine, CONFIG_ZMK_SPACEMOUSE_KEEP_PER_MILLE,
                            CONFIG_ZMK_SPACEMOUSE_DEADBAND, out);
    idle = sm_engine_idle(&engine);
    k_spin_unlock(&engine_lock, key);

    if (moving) {
        zeros_left = CONFIG_ZMK_SPACEMOUSE_ZERO_REPEATS;
    } else if (zeros_left == 0) {
        return;
    } else {
        zeros_left--;
    }

    struct zmk_spacemouse_motion_body body;
    for (int i = 0; i < SM_ENGINE_AXES; i++) {
        body.axes[i] = sys_cpu_to_le16(out[i]);
    }
    last_sent = body;

    int err = send_motion(&body);
    if (err) {
        LOG_DBG("SpaceMouse report not sent (%d)", err);
    }

    if (moving || !idle || zeros_left > 0) {
        k_work_reschedule(&tick_work, K_MSEC(CONFIG_ZMK_SPACEMOUSE_REPORT_INTERVAL_MS));
    }
}

int zmk_spacemouse_add(uint8_t axis_spec, int32_t counts, int32_t gain_q8) {
    k_spinlock_key_t key = k_spin_lock(&engine_lock);
    bool ok = sm_engine_add(&engine, axis_spec, counts, gain_q8);
    k_spin_unlock(&engine_lock, key);

    if (!ok) {
        return -EINVAL;
    }

    /* Starts an idle tick at once; leaves a running cadence alone (k_work_schedule keeps an
     * already-scheduled timeout). */
    k_work_schedule(&tick_work, K_NO_WAIT);
    return 0;
}

void zmk_spacemouse_get_motion(struct zmk_spacemouse_motion_body *body) { *body = last_sent; }
