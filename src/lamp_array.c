/*
 * ZMK Pro RGB — HID LampArray (Windows 11 Dynamic Lighting), USB only
 *
 * Exposes the per-key LEDs to Windows through the open HID LampArray
 * standard (HID Usage Tables 1.4, page 0x59) so that Settings ->
 * Personalization -> Dynamic Lighting can drive them without any vendor
 * software.
 *
 * Design
 *   - LampArray lives on its OWN USB HID interface (Zephyr instance "HID_1"),
 *     exactly like Microsoft's ArduinoHidForWindows sample. ZMK's keyboard
 *     descriptor, its BLE HID-over-GATT service and the bonds are untouched,
 *     and no change to ZMK itself is needed.
 *   - Two modes, as in the standard. "Autonomous" (power-up default): the
 *     normal rgb_pro effects run. "Host": Windows paints every lamp and
 *     rgbp_lamp_array_render() copies its last completed frame into pixels[].
 *   - Status overlays (caps lock, BLE/USB, battery) are still drawn on top of
 *     the host frame, and the keyboard's own on/off and brightness apply.
 *   - Lamp ID == physical LED index in the chain (the same index effects use).
 *
 * Report layout and the report descriptor (zmk_vfx_pro_rgb/lamp_array_hid.h)
 * come from Microsoft's ArduinoHidForWindows / RP2040MacropadHidSample (MIT
 * licence, Copyright (c) Microsoft Corporation), generated with Waratah from
 * HID Usage Tables 1.4.0, and are used verbatim; this file only adds the ZMK/
 * Zephyr glue.
 *
 * USB only. For a keyboard used through a Bluetooth receiver dongle, see
 * lamp_remote.c (CONFIG_RGB_PRO_LAMP_ARRAY_REMOTE).
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/spinlock.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>

#include <zmk/event_manager.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

#include <zmk_vfx_pro_rgb/rgb_pro.h>
#include <zmk_vfx_pro_rgb/effects.h>

LOG_MODULE_DECLARE(rgb_pro, CONFIG_ZMK_LOG_LEVEL);

/* A second HID instance is required: HID_0 is ZMK's keyboard. */
BUILD_ASSERT(CONFIG_USB_HID_DEVICE_COUNT >= 2,
             "RGB_PRO_LAMP_ARRAY needs CONFIG_USB_HID_DEVICE_COUNT=2 (HID_0 is the keyboard)");

#define LAMP_COUNT RGB_PRO_KEYS

#include <zmk_vfx_pro_rgb/lamp_array_hid.h>

/* The standard wants micrometres and microseconds on the wire. */
#define LA_PITCH_UM     CONFIG_RGB_PRO_LAMP_ARRAY_PITCH_UM
#define LA_DEPTH_UM     10000 /* 1 cm; lamps sit at z = 0 */
#define LA_UPDATE_US    (CONFIG_RGB_PRO_TICK_MS * 1000)

/* ---- State ---- */
static struct k_spinlock la_lock;
static struct la_color staging[LAMP_COUNT]; /* being assembled by the host   */
static struct la_color active[LAMP_COUNT];  /* last frame the host completed */
static bool autonomous = true;              /* power-up default per the spec */
static uint16_t req_lamp_id;                /* lamp the next response describes */

static const struct device *hid_dev;

/* ---- Feature report handlers ---- */

static int get_array_attributes(int32_t *len, uint8_t **data) {
    static struct la_array_attributes_report r;

    r = (struct la_array_attributes_report){
        .report_id = LA_ID_ARRAY_ATTRIBUTES,
        .lamp_count = LAMP_COUNT,
        .width_um = RGB_PRO_COLS * LA_PITCH_UM,
        .height_um = RGB_PRO_ROWS * LA_PITCH_UM,
        .depth_um = LA_DEPTH_UM,
        .kind = LA_KIND_KEYBOARD,
        .min_update_us = LA_UPDATE_US,
    };
    *data = (uint8_t *)&r;
    *len = sizeof(r);
    return 0;
}

static int get_attr_response(int32_t *len, uint8_t **data) {
    static struct la_attr_response_report r;
    uint16_t id = req_lamp_id < LAMP_COUNT ? req_lamp_id : 0;

    r = (struct la_attr_response_report){
        .report_id = LA_ID_ATTR_RESPONSE,
        .lamp_id = id,
        /* Centre of the key cell, measured from the array's top-left corner. */
        .x_um = led_col[id] * LA_PITCH_UM + LA_PITCH_UM / 2,
        .y_um = led_row[id] * LA_PITCH_UM + LA_PITCH_UM / 2,
        .z_um = 0,
        .update_latency_us = LA_UPDATE_US,
        .purposes = LA_PURPOSE_CONTROL,
        .red_levels = LA_LEVELS_PER_COLOR,
        .green_levels = LA_LEVELS_PER_COLOR,
        .blue_levels = LA_LEVELS_PER_COLOR,
        .intensity_levels = LA_LEVELS_INTENSITY,
        .is_programmable = LA_IS_PROGRAMMABLE,
        .input_binding = 0, /* lamp -> key usage mapping not provided */
    };

    /* The response advances to the next lamp after every read and wraps,
     * so the host can walk the whole array with repeated GetFeature calls. */
    req_lamp_id = (id + 1 < LAMP_COUNT) ? id + 1 : 0;

    *data = (uint8_t *)&r;
    *len = sizeof(r);
    return 0;
}

static int set_attr_request(const uint8_t *data, int32_t len) {
    if (len < (int32_t)sizeof(struct la_attr_request_report)) {
        return -EINVAL;
    }
    const struct la_attr_request_report *r = (const void *)data;

    /* Out of range selects lamp 0, as in Microsoft's reference firmware. */
    req_lamp_id = r->lamp_id < LAMP_COUNT ? r->lamp_id : 0;
    return 0;
}

static int set_multi_update(const uint8_t *data, int32_t len) {
    if (len < (int32_t)sizeof(struct la_multi_update_report)) {
        return -EINVAL;
    }
    const struct la_multi_update_report *r = (const void *)data;
    uint8_t n = MIN(r->lamp_count, LA_MULTI_MAX);

    k_spinlock_key_t key = k_spin_lock(&la_lock);
    for (uint8_t i = 0; i < n; i++) {
        uint16_t id = r->lamp_ids[i];
        if (id < LAMP_COUNT) {
            staging[id] = r->colors[i];
        }
    }
    /* Publish only when the host says the batch is complete, so a frame
     * is never shown half-updated. */
    if (r->flags & LA_FLAG_UPDATE_COMPLETE) {
        memcpy(active, staging, sizeof(active));
    }
    k_spin_unlock(&la_lock, key);
    return 0;
}

static int set_range_update(const uint8_t *data, int32_t len) {
    if (len < (int32_t)sizeof(struct la_range_update_report)) {
        return -EINVAL;
    }
    const struct la_range_update_report *r = (const void *)data;
    uint16_t start = r->lamp_id_start, end = r->lamp_id_end;

    if (start > end || end >= LAMP_COUNT) {
        return 0; /* ignored, per the reference firmware */
    }

    k_spinlock_key_t key = k_spin_lock(&la_lock);
    for (uint16_t id = start; id <= end; id++) {
        staging[id] = r->color;
    }
    if (r->flags & LA_FLAG_UPDATE_COMPLETE) {
        memcpy(active, staging, sizeof(active));
    }
    k_spin_unlock(&la_lock, key);
    return 0;
}

static int set_control(const uint8_t *data, int32_t len) {
    if (len < (int32_t)sizeof(struct la_control_report)) {
        return -EINVAL;
    }
    const struct la_control_report *r = (const void *)data;
    bool now_autonomous = r->autonomous & 1;

    k_spinlock_key_t key = k_spin_lock(&la_lock);
    bool changed = (autonomous != now_autonomous);
    autonomous = now_autonomous;
    k_spin_unlock(&la_lock, key);

    if (changed) {
        LOG_INF("LampArray: %s mode", now_autonomous ? "autonomous" : "host");
    }
    return 0;
}

/* ---- Zephyr legacy USB HID glue ---- */

#define HID_REPORT_TYPE_FEATURE 0x300
#define HID_TYPE_MASK 0xff00
#define HID_ID_MASK 0x00ff

static int get_report_cb(const struct device *dev, struct usb_setup_packet *setup, int32_t *len,
                         uint8_t **data) {
    ARG_UNUSED(dev);

    if ((setup->wValue & HID_TYPE_MASK) != HID_REPORT_TYPE_FEATURE) {
        return -ENOTSUP;
    }

    switch (setup->wValue & HID_ID_MASK) {
    case LA_ID_ARRAY_ATTRIBUTES:
        return get_array_attributes(len, data);
    case LA_ID_ATTR_RESPONSE:
        return get_attr_response(len, data);
    default:
        return -ENOTSUP;
    }
}

static int set_report_cb(const struct device *dev, struct usb_setup_packet *setup, int32_t *len,
                         uint8_t **data) {
    ARG_UNUSED(dev);

    if ((setup->wValue & HID_TYPE_MASK) != HID_REPORT_TYPE_FEATURE) {
        return -ENOTSUP;
    }

    switch (setup->wValue & HID_ID_MASK) {
    case LA_ID_ATTR_REQUEST:
        return set_attr_request(*data, *len);
    case LA_ID_MULTI_UPDATE:
        return set_multi_update(*data, *len);
    case LA_ID_RANGE_UPDATE:
        return set_range_update(*data, *len);
    case LA_ID_CONTROL:
        return set_control(*data, *len);
    default:
        return -ENOTSUP;
    }
}

static const struct hid_ops lamp_array_ops = {
    .get_report = get_report_cb,
    .set_report = set_report_cb,
};

/* Must run before usb_enable() (ZMK_USB_INIT_PRIORITY), like ZMK's own HID init. */
static int rgbp_lamp_array_init(void) {
    hid_dev = device_get_binding("HID_1");
    if (hid_dev == NULL) {
        LOG_ERR("LampArray: HID_1 not found (is CONFIG_USB_HID_DEVICE_COUNT=2?)");
        return -EINVAL;
    }

    usb_hid_register_device(hid_dev, lamp_array_report_desc, sizeof(lamp_array_report_desc),
                            &lamp_array_ops);
    usb_hid_init(hid_dev);
    return 0;
}
SYS_INIT(rgbp_lamp_array_init, APPLICATION, CONFIG_ZMK_USB_HID_INIT_PRIORITY);

/* ---- Cable removed: forget the host ----
 * Windows keeps no state on the keyboard once unplugged, and a battery-powered
 * board does not reboot, so without this it would stay frozen on the last
 * Windows frame. A re-plug re-enumerates and Windows takes control again. */
static int lamp_usb_listener(const zmk_event_t *eh) {
    const struct zmk_usb_conn_state_changed *ev = as_zmk_usb_conn_state_changed(eh);
    if (ev == NULL || ev->conn_state != ZMK_USB_CONN_NONE) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    k_spinlock_key_t key = k_spin_lock(&la_lock);
    autonomous = true;
    req_lamp_id = 0;
    memset(staging, 0, sizeof(staging));
    memset(active, 0, sizeof(active));
    k_spin_unlock(&la_lock, key);
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(rgbp_lamp_usb, lamp_usb_listener);
ZMK_SUBSCRIPTION(rgbp_lamp_usb, zmk_usb_conn_state_changed);

/* ---- Renderer hook (called from the tick in rgb_pro.c) ---- */
bool rgbp_lamp_array_render(void) {
#if IS_ENABLED(CONFIG_RGB_PRO_LAMP_ARRAY_APPLY_BRT)
    const uint8_t scale = (uint8_t)((state.brt * 255) / 100);
#endif

    k_spinlock_key_t key = k_spin_lock(&la_lock);
    bool host = !autonomous;
    if (host) {
        for (int i = 0; i < LAMP_COUNT; i++) {
            /* The intensity channel is ignored: all lamps are RGB and Microsoft's
             * own sample does the same. */
            struct led_rgb c = {.r = active[i].r, .g = active[i].g, .b = active[i].b};
#if IS_ENABLED(CONFIG_RGB_PRO_LAMP_ARRAY_APPLY_BRT)
            c = rgbp_scale(c, scale);
#endif
            pixels[i] = c;
        }
    }
    k_spin_unlock(&la_lock, key);
    return host;
}
