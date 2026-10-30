/*
 * Copyright (c) 2026 Luix333
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_input_processor_spacemouse

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zmk/spacemouse/spacemouse.h>

struct sm_ip_config {
    int32_t gain;
};

static int sm_ip_handle_event(const struct device *dev, struct input_event *event,
                              uint32_t param1, uint32_t param2,
                              struct zmk_input_processor_state *state) {
    ARG_UNUSED(state);
    const struct sm_ip_config *cfg = dev->config;

    if (event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    uint8_t axis_spec;

    if (event->code == INPUT_REL_X) {
        axis_spec = (uint8_t)param1;
    } else if (event->code == INPUT_REL_Y) {
        axis_spec = (uint8_t)param2;
    } else {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    zmk_spacemouse_add(axis_spec, event->value, cfg->gain);

    event->value = 0;
    event->sync = false;

    return ZMK_INPUT_PROC_STOP;
}

static const struct zmk_input_processor_driver_api sm_ip_driver_api = {
    .handle_event = sm_ip_handle_event,
};

#define SM_IP_INST(n)                                                                              \
    static const struct sm_ip_config sm_ip_config_##n = {                                          \
        .gain = DT_INST_PROP(n, gain),                                                             \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, &sm_ip_config_##n, POST_KERNEL,                     \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &sm_ip_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SM_IP_INST)
