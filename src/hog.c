/*
 * Copyright (c) 2026 Luix333
 *
 * SPDX-License-Identifier: MIT
 *
 * Second HID-over-GATT service carrying the SpaceMouse report map. Mirrors ZMK's app/src/hog.c;
 * a separate service keeps 3Dconnexion's report ids from colliding with ZMK's.
 */

#include <errno.h>
#include <string.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/ble.h>
#include <zmk/spacemouse/hid.h>
#include <zmk/spacemouse/spacemouse.h>

enum {
    HIDS_REMOTE_WAKE = BIT(0),
    HIDS_NORMALLY_CONNECTABLE = BIT(1),
};

struct hids_info {
    uint16_t version;
    uint8_t code;
    uint8_t flags;
} __packed;

struct hids_report {
    uint8_t id;
    uint8_t type;
} __packed;

enum {
    HIDS_INPUT = 0x01,
    HIDS_OUTPUT = 0x02,
};

static struct hids_info info = {
    .version = 0x0000,
    .code = 0x00,
    .flags = HIDS_NORMALLY_CONNECTABLE | HIDS_REMOTE_WAKE,
};

static struct hids_report motion_ref = {.id = ZMK_SPACEMOUSE_REPORT_ID_MOTION, .type = HIDS_INPUT};
static struct hids_report buttons_ref = {.id = ZMK_SPACEMOUSE_REPORT_ID_BUTTONS,
                                         .type = HIDS_INPUT};
static struct hids_report leds_ref = {.id = ZMK_SPACEMOUSE_REPORT_ID_LEDS, .type = HIDS_OUTPUT};

static uint8_t ctrl_point;
static uint8_t leds;

static ssize_t read_hids_info(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                              uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                             sizeof(struct hids_info));
}

static ssize_t read_hids_report_ref(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                    void *buf, uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, attr->user_data,
                             sizeof(struct hids_report));
}

static ssize_t read_report_map(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                               uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, zmk_spacemouse_report_desc,
                             sizeof(zmk_spacemouse_report_desc));
}

static ssize_t read_motion_report(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                                  uint16_t len, uint16_t offset) {
    struct zmk_spacemouse_motion_body body;
    zmk_spacemouse_get_motion(&body);
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &body, sizeof(body));
}

static ssize_t read_buttons_report(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                   void *buf, uint16_t len, uint16_t offset) {
    static const struct zmk_spacemouse_buttons_body none = {0};
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &none, sizeof(none));
}

static ssize_t read_leds_report(struct bt_conn *conn, const struct bt_gatt_attr *attr, void *buf,
                                uint16_t len, uint16_t offset) {
    return bt_gatt_attr_read(conn, attr, buf, len, offset, &leds, sizeof(leds));
}

static ssize_t write_leds_report(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                 const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    /* The driver sets the LED; there is no LED to drive, so remember and ignore it. */
    if (offset != 0) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    if (len != sizeof(leds)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }
    leds = *(const uint8_t *)buf;
    return len;
}

static void input_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value) {}

static ssize_t write_ctrl_point(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    uint8_t *value = attr->user_data;

    if (offset + len > sizeof(ctrl_point)) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    memcpy(value + offset, buf, len);
    return len;
}

BT_GATT_SERVICE_DEFINE(
    zmk_spacemouse_hog_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_HIDS),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_INFO, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_hids_info,
                           NULL, &info),
    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT_MAP, BT_GATT_CHRC_READ, BT_GATT_PERM_READ_ENCRYPT,
                           read_report_map, NULL, NULL),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ_ENCRYPT, read_motion_report, NULL, NULL),
    BT_GATT_CCC(input_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref,
                       NULL, &motion_ref),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_READ_ENCRYPT, read_buttons_report, NULL, NULL),
    BT_GATT_CCC(input_ccc_changed, BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref,
                       NULL, &buttons_ref),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_REPORT,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_READ_ENCRYPT | BT_GATT_PERM_WRITE_ENCRYPT,
                           read_leds_report, write_leds_report, NULL),
    BT_GATT_DESCRIPTOR(BT_UUID_HIDS_REPORT_REF, BT_GATT_PERM_READ_ENCRYPT, read_hids_report_ref,
                       NULL, &leds_ref),

    BT_GATT_CHARACTERISTIC(BT_UUID_HIDS_CTRL_POINT, BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                           BT_GATT_PERM_WRITE, NULL, write_ctrl_point, &ctrl_point));

/* The motion report's value attribute, found at init by its read callback rather than by a
 * hard-coded index into the table above. */
static const struct bt_gatt_attr *motion_attr;

K_THREAD_STACK_DEFINE(sm_hog_q_stack, CONFIG_ZMK_BLE_THREAD_STACK_SIZE);
static struct k_work_q sm_hog_work_q;

K_MSGQ_DEFINE(sm_hog_msgq, sizeof(struct zmk_spacemouse_motion_body),
              CONFIG_ZMK_SPACEMOUSE_BLE_QUEUE_SIZE, 4);

static void send_motion_callback(struct k_work *work) {
    struct zmk_spacemouse_motion_body body;

    while (k_msgq_get(&sm_hog_msgq, &body, K_NO_WAIT) == 0) {
        struct bt_conn *conn = zmk_ble_active_profile_conn();
        if (conn == NULL) {
            /* Nobody to send to: stale motion must not replay after a reconnect. */
            k_msgq_purge(&sm_hog_msgq);
            return;
        }

        struct bt_gatt_notify_params notify_params = {
            .attr = motion_attr,
            .data = &body,
            .len = sizeof(body),
        };

        int err = bt_gatt_notify_cb(conn, &notify_params);
        if (err == -EPERM) {
            bt_conn_set_security(conn, BT_SECURITY_L2);
        } else if (err) {
            LOG_DBG("SpaceMouse notify failed (%d)", err);
        }

        bt_conn_unref(conn);
    }
}

K_WORK_DEFINE(sm_hog_work, send_motion_callback);

int zmk_spacemouse_hog_send(const struct zmk_spacemouse_motion_body *body) {
    if (motion_attr == NULL) {
        return -ENODEV;
    }

    /* Never wait: this runs on the system work queue. When full, drop the oldest report; the
     * newest deflection is the one that matters. */
    if (k_msgq_put(&sm_hog_msgq, body, K_NO_WAIT) != 0) {
        struct zmk_spacemouse_motion_body discarded;
        k_msgq_get(&sm_hog_msgq, &discarded, K_NO_WAIT);
        if (k_msgq_put(&sm_hog_msgq, body, K_NO_WAIT) != 0) {
            return -ENOMEM;
        }
    }

    k_work_submit_to_queue(&sm_hog_work_q, &sm_hog_work);
    return 0;
}

static int zmk_spacemouse_hog_init(void) {
    for (size_t i = 0; i < zmk_spacemouse_hog_svc.attr_count; i++) {
        if (zmk_spacemouse_hog_svc.attrs[i].read == read_motion_report) {
            motion_attr = &zmk_spacemouse_hog_svc.attrs[i];
            break;
        }
    }
    if (motion_attr == NULL) {
        LOG_ERR("SpaceMouse: motion report attribute not found");
        return -ENODEV;
    }

    static const struct k_work_queue_config queue_config = {.name = "SpaceMouse HOG Send Work"};
    k_work_queue_start(&sm_hog_work_q, sm_hog_q_stack, K_THREAD_STACK_SIZEOF(sm_hog_q_stack),
                       CONFIG_ZMK_BLE_THREAD_PRIORITY, &queue_config);

    return 0;
}

SYS_INIT(zmk_spacemouse_hog_init, APPLICATION, CONFIG_ZMK_BLE_INIT_PRIORITY);
