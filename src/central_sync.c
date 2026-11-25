/*
 * Dongle side of the status/battery overlays.
 *
 * ZMK's split link carries no layer or Bluetooth state, so the dongle (central,
 * no LEDs) sends it to the keyboard as &rgb_pro behavior invocations with the
 * internal RGB_PRO_CMD_SYNC_* commands. The keyboard stores them and draws the
 * overlays itself.
 *
 * The state is sent as soon as it changes and then repeated every
 * CONFIG_RGB_PRO_SYNC_PERIOD_MS. The repetition matters: the central has no
 * "peripheral connected" event, and ZMK drops a command for a peripheral that
 * is not connected or not ready yet, without retrying.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/behavior.h>
#include <zmk/keymap.h>
#include <zmk/endpoints.h>
#include <zmk/split/central.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/endpoint_changed.h>
#if IS_ENABLED(CONFIG_ZMK_BLE)
#include <zmk/ble.h>
#include <zmk/events/ble_active_profile_changed.h>
#endif

#include <zmk_vfx_pro_rgb/commands.h>

LOG_MODULE_REGISTER(rgb_pro_sync, CONFIG_ZMK_LOG_LEVEL);

#if !DT_HAS_COMPAT_STATUS_OKAY(zmk_behavior_rgb_pro)
#error "RGB_PRO_FORWARD_SYNC needs the rgb_pro behavior node (compatible zmk,behavior-rgb-pro)"
#endif

/* Same device name the keyboard's keymap uses: ZMK finds the behavior on the
 * peripheral by this name. */
#define RGBP_DEV DEVICE_DT_NAME(DT_INST(0, zmk_behavior_rgb_pro))

static void send_to_keyboard(uint8_t cmd, uint8_t param) {
    struct zmk_behavior_binding binding = {
        .behavior_dev = RGBP_DEV,
        .param1 = cmd,
        .param2 = param,
    };
    struct zmk_behavior_binding_event event = {
        .position = 0,
        .timestamp = k_uptime_get(),
    };

    for (uint8_t i = 0; i < ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT; i++) {
        int err = zmk_split_central_invoke_behavior(i, &binding, event, true);
        if (err) {
            LOG_DBG("sync cmd %d to peripheral %d failed: %d", cmd, i, err);
        }
    }
}

static uint8_t status_packed(void) {
    bool usb = zmk_endpoint_get_selected().transport == ZMK_TRANSPORT_USB;
#if IS_ENABLED(CONFIG_ZMK_BLE)
    int st = zmk_ble_active_profile_is_connected() ? RGB_PRO_BT_CONNECTED
           : zmk_ble_active_profile_is_open()      ? RGB_PRO_BT_OPEN
                                                   : RGB_PRO_BT_PAIRED;
    return RGB_PRO_STATUS_PACK(usb, zmk_ble_active_profile_index(), st);
#else
    return RGB_PRO_STATUS_PACK(usb, 0, RGB_PRO_BT_PAIRED);
#endif
}

static void sync_work_handler(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(sync_work, sync_work_handler);

static void sync_work_handler(struct k_work *work) {
    send_to_keyboard(RGB_PRO_CMD_SYNC_LAYER, zmk_keymap_highest_layer_active());
    send_to_keyboard(RGB_PRO_CMD_SYNC_STATUS, status_packed());

    k_work_reschedule(&sync_work, K_MSEC(CONFIG_RGB_PRO_SYNC_PERIOD_MS));
}

/* Any change that the overlays show: send now instead of waiting for the period. */
static int sync_listener(const zmk_event_t *eh) {
    k_work_reschedule(&sync_work, K_NO_WAIT);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(rgbp_sync, sync_listener);
ZMK_SUBSCRIPTION(rgbp_sync, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(rgbp_sync, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(rgbp_sync, zmk_ble_active_profile_changed);
#endif

static int sync_init(void) {
    k_work_schedule(&sync_work, K_SECONDS(1));
    return 0;
}

SYS_INIT(sync_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
