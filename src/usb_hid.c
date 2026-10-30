/*
 * Copyright (c) 2026 Luix333
 *
 * SPDX-License-Identifier: MIT
 *
 * Second USB HID interface carrying the SpaceMouse report map. Mirrors ZMK's app/src/usb_hid.c.
 */

#include <errno.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/sys/util.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/usb.h>
#include <zmk/spacemouse/hid.h>
#include <zmk/spacemouse/spacemouse.h>

BUILD_ASSERT(CONFIG_USB_HID_DEVICE_COUNT > CONFIG_ZMK_SPACEMOUSE_USB_HID_INDEX,
             "SpaceMouse needs its own USB HID device: set CONFIG_USB_HID_DEVICE_COUNT=2");

#define HID_GET_REPORT_TYPE_MASK 0xff00
#define HID_GET_REPORT_ID_MASK 0x00ff

#define HID_REPORT_TYPE_INPUT 0x100
#define HID_REPORT_TYPE_OUTPUT 0x200

static const struct device *hid_dev;

static K_SEM_DEFINE(hid_sem, 1, 1);

static struct zmk_spacemouse_motion_report tx_report = {.id = ZMK_SPACEMOUSE_REPORT_ID_MOTION};
static struct zmk_spacemouse_motion_report get_motion_report = {
    .id = ZMK_SPACEMOUSE_REPORT_ID_MOTION};
static struct zmk_spacemouse_buttons_report get_buttons_report = {
    .id = ZMK_SPACEMOUSE_REPORT_ID_BUTTONS};

static void in_ready_cb(const struct device *dev) { k_sem_give(&hid_sem); }

static int get_report_cb(const struct device *dev, struct usb_setup_packet *setup, int32_t *len,
                         uint8_t **data) {
    if ((setup->wValue & HID_GET_REPORT_TYPE_MASK) != HID_REPORT_TYPE_INPUT) {
        return -ENOTSUP;
    }

    switch (setup->wValue & HID_GET_REPORT_ID_MASK) {
    case ZMK_SPACEMOUSE_REPORT_ID_MOTION:
        zmk_spacemouse_get_motion(&get_motion_report.body);
        *data = (uint8_t *)&get_motion_report;
        *len = sizeof(get_motion_report);
        return 0;
    case ZMK_SPACEMOUSE_REPORT_ID_BUTTONS:
        *data = (uint8_t *)&get_buttons_report;
        *len = sizeof(get_buttons_report);
        return 0;
    default:
        return -EINVAL;
    }
}

static int set_report_cb(const struct device *dev, struct usb_setup_packet *setup, int32_t *len,
                         uint8_t **data) {
    /* The driver sets the LED (report 4); there is no LED to drive, so accept and ignore it. */
    if ((setup->wValue & HID_GET_REPORT_TYPE_MASK) == HID_REPORT_TYPE_OUTPUT &&
        (setup->wValue & HID_GET_REPORT_ID_MASK) == ZMK_SPACEMOUSE_REPORT_ID_LEDS) {
        return 0;
    }
    return -ENOTSUP;
}

static const struct hid_ops ops = {
    .int_in_ready = in_ready_cb,
    .get_report = get_report_cb,
    .set_report = set_report_cb,
};

int zmk_spacemouse_usb_send(const struct zmk_spacemouse_motion_body *body) {
    if (hid_dev == NULL) {
        return -ENODEV;
    }

    switch (zmk_usb_get_status()) {
    case USB_DC_SUSPEND:
        return usb_wakeup_request();
    case USB_DC_ERROR:
    case USB_DC_RESET:
    case USB_DC_DISCONNECTED:
    case USB_DC_UNKNOWN:
        return -ENODEV;
    default:
        k_sem_take(&hid_sem, K_MSEC(30));
        tx_report.body = *body;
        int err = hid_int_ep_write(hid_dev, (uint8_t *)&tx_report, sizeof(tx_report), NULL);
        if (err) {
            k_sem_give(&hid_sem);
        }
        return err;
    }
}

static int zmk_spacemouse_usb_init(void) {
    hid_dev = device_get_binding("HID_" STRINGIFY(CONFIG_ZMK_SPACEMOUSE_USB_HID_INDEX));
    if (hid_dev == NULL) {
        LOG_ERR("SpaceMouse: unable to locate HID_%d", CONFIG_ZMK_SPACEMOUSE_USB_HID_INDEX);
        return -EINVAL;
    }

    usb_hid_register_device(hid_dev, zmk_spacemouse_report_desc,
                            sizeof(zmk_spacemouse_report_desc), &ops);
    usb_hid_init(hid_dev);

    return 0;
}

/* Same priority as ZMK's own HID registration (95): both must precede usb_enable (96). */
SYS_INIT(zmk_spacemouse_usb_init, APPLICATION, CONFIG_ZMK_USB_HID_INIT_PRIORITY);
