/*
 * SPDX-License-Identifier: MIT
 *
 * &ar KEY: toggles auto-repeat of KEY. While active, KEY is tapped over and over with a random
 * pause (min-delay-ms..max-delay-ms) between taps. Only one key repeats at a time: arming another
 * key replaces the current one, pressing the same key again stops it. Repeating also stops when
 * the configured game layer is deactivated.
 */

#define DT_DRV_COMPAT zmk_behavior_auto_repeat

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/random/random.h>
#include <zephyr/logging/log.h>
#include <drivers/behavior.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1,
             "Only one zmk,behavior-auto-repeat instance is supported");

#define MIN_DELAY_MS DT_INST_PROP(0, min_delay_ms)
#define MAX_DELAY_MS DT_INST_PROP(0, max_delay_ms)
#define HOLD_MS DT_INST_PROP(0, hold_ms)
#define GAME_LAYER DT_INST_PROP(0, game_layer)

BUILD_ASSERT(MAX_DELAY_MS >= MIN_DELAY_MS, "max-delay-ms must be >= min-delay-ms");

static struct k_work_delayable repeat_work;
static bool active;
static bool key_down;
static uint32_t active_key;

static k_timeout_t random_delay(void) {
    uint32_t r;
    sys_rand_get(&r, sizeof(r));
    return K_MSEC(MIN_DELAY_MS + (r % (MAX_DELAY_MS - MIN_DELAY_MS + 1)));
}

static void repeat_handler(struct k_work *work) {
    if (!active) {
        return;
    }

    if (key_down) {
        key_down = false;
        raise_zmk_keycode_state_changed_from_encoded(active_key, false, k_uptime_get());
        k_work_reschedule(&repeat_work, random_delay());
    } else {
        key_down = true;
        raise_zmk_keycode_state_changed_from_encoded(active_key, true, k_uptime_get());
        k_work_reschedule(&repeat_work, K_MSEC(HOLD_MS));
    }
}

static void stop_repeat(void) {
    if (!active) {
        return;
    }

    active = false;
    k_work_cancel_delayable(&repeat_work);
    if (key_down) {
        key_down = false;
        raise_zmk_keycode_state_changed_from_encoded(active_key, false, k_uptime_get());
    }
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    uint32_t key = binding->param1;

    if (active && active_key == key) {
        stop_repeat();
        return ZMK_BEHAVIOR_OPAQUE;
    }

    stop_repeat();
    active = true;
    active_key = key;
    key_down = false;
    k_work_reschedule(&repeat_work, random_delay());
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static int auto_repeat_layer_listener(const zmk_event_t *eh) {
    if (active && !zmk_keymap_layer_active(zmk_keymap_layer_index_to_id(GAME_LAYER))) {
        stop_repeat();
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(auto_repeat, auto_repeat_layer_listener);
ZMK_SUBSCRIPTION(auto_repeat, zmk_layer_state_changed);

static int behavior_auto_repeat_init(const struct device *dev) {
    k_work_init_delayable(&repeat_work, repeat_handler);
    return 0;
}

static const struct behavior_driver_api behavior_auto_repeat_driver_api = {
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

BEHAVIOR_DT_INST_DEFINE(0, behavior_auto_repeat_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_auto_repeat_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
