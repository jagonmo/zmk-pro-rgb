/*
 * Status overlays — drawn on top of whatever effect is running.
 *
 *   caps    : blinks a key while caps lock is active
 *   status  : BLE/USB output + active profile state, shown on a chosen layer
 *   battery : battery level as a red-to-green bar, shown on a chosen layer
 *
 * All positions are given as KEY POSITIONS (same order as the keymap), not
 * raw LED indices — the module converts them through key_to_led[].
 */

#include <zmk_vfx_pro_rgb/effects.h>

#if IS_ENABLED(CONFIG_RGB_PRO_CAPS_INDICATOR)
#include <zmk/hid_indicators.h>
#endif

#if IS_ENABLED(CONFIG_RGB_PRO_STATUS_OVERLAY)
#include <zmk/endpoints.h>
#include <zmk/ble.h>
#endif

#if IS_ENABLED(CONFIG_RGB_PRO_BATTERY_INDICATOR)
#include <zmk/battery.h>
#endif

#if IS_ENABLED(CONFIG_RGB_PRO_STATUS_OVERLAY) || IS_ENABLED(CONFIG_RGB_PRO_BATTERY_INDICATOR)
#include <zmk/keymap.h>
#endif

#define NKEYS RGB_PRO_KEYS

/* Paint a key position (not an LED index). */
static inline void paint_key(uint8_t pos, struct led_rgb c) {
    if (pos >= NKEYS) return;
    uint8_t led = key_to_led[pos];
    if (led < NKEYS) pixels[led] = c;
}

static inline bool blink(uint16_t period_ms) {
    return (k_uptime_get() / (period_ms / 2)) & 1;
}

#define OFF   ((struct led_rgb){0, 0, 0})
#define RED   ((struct led_rgb){255, 0, 0})
#define GREEN ((struct led_rgb){0, 255, 0})
#define BLUE  ((struct led_rgb){0, 0, 255})
#define DIM_W ((struct led_rgb){12, 12, 12})

/* ---- Caps lock ---- */
#if IS_ENABLED(CONFIG_RGB_PRO_CAPS_INDICATOR)
static void caps_overlay(void) {
    zmk_hid_indicators_t ind = zmk_hid_indicators_get_current_profile();
    if (!(ind & 0x02)) return; /* bit 1 = caps lock (USB HID spec) */

    struct led_rgb c = blink(CONFIG_RGB_PRO_CAPS_BLINK_MS)
        ? (struct led_rgb){CONFIG_RGB_PRO_CAPS_R,
                           CONFIG_RGB_PRO_CAPS_G,
                           CONFIG_RGB_PRO_CAPS_B}
        : OFF;
    paint_key(CONFIG_RGB_PRO_CAPS_KEY, c);
}
#else
static void caps_overlay(void) {}
#endif

/* ---- BLE / USB status ---- */
#if IS_ENABLED(CONFIG_RGB_PRO_STATUS_OVERLAY)
static void status_overlay(void) {
    if (zmk_keymap_highest_layer_active() != CONFIG_RGB_PRO_STATUS_LAYER) return;

    bool on = blink(CONFIG_RGB_PRO_STATUS_BLINK_MS);

    /* Output selector: blue = BLE, red = USB. */
    struct zmk_endpoint_instance ep = zmk_endpoint_get_selected();
    paint_key(CONFIG_RGB_PRO_STATUS_OUT_KEY,
              ep.transport == ZMK_TRANSPORT_USB ? RED : BLUE);

    /* Profile keys. Only the active profile's real state is queryable
     * through ZMK's public API; the rest are shown dim. */
    int active = zmk_ble_active_profile_index();

    for (int i = 0; i < CONFIG_RGB_PRO_STATUS_BT_COUNT; i++) {
        uint8_t pos = CONFIG_RGB_PRO_STATUS_BT_KEY_BASE + i;
        struct led_rgb c;

        if (i != active) {
            c = DIM_W;
        } else if (zmk_ble_active_profile_is_connected()) {
            c = BLUE;                       /* connected: solid blue */
        } else if (zmk_ble_active_profile_is_open()) {
            c = on ? GREEN : OFF;           /* unpaired: blinking green */
        } else {
            c = on ? BLUE : OFF;            /* paired, not connected: blinking blue */
        }
        paint_key(pos, c);
    }
}
#else
static void status_overlay(void) {}
#endif

/* ---- Battery level ---- */
#if IS_ENABLED(CONFIG_RGB_PRO_BATTERY_INDICATOR)
static void battery_overlay(void) {
    if (zmk_keymap_highest_layer_active() != CONFIG_RGB_PRO_BATTERY_LAYER) return;

    uint8_t pct = zmk_battery_state_of_charge();
    const int count = CONFIG_RGB_PRO_BATTERY_KEY_COUNT;

    /* Hue sweeps red (0deg) at 1% to green (120deg) at 100%. */
    uint16_t hue = (uint16_t)pct * 120 / 100;
    struct led_rgb lit = rgbp_hsb(hue, 100, 100);

    /* Bar fill: at least one segment lit while the battery reports > 0. */
    int filled = (pct * count + 99) / 100;
    if (filled > count) filled = count;

    for (int i = 0; i < count; i++) {
        paint_key(CONFIG_RGB_PRO_BATTERY_KEY_START + i, i < filled ? lit : OFF);
    }
}
#else
static void battery_overlay(void) {}
#endif

/* ---- Entry point, called once per frame after the effect renders ---- */
void rgbp_render_overlays(void) {
    caps_overlay();
    status_overlay();
    battery_overlay();
}
