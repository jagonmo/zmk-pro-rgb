/*
 * Status overlays — drawn on top of whatever effect is running.
 *
 *   caps    : blinks a key while caps lock is active
 *   num/scroll : light a key for num lock / scroll lock (off by default)
 *   status  : BLE/USB output + active profile state, shown on a chosen layer
 *   battery : battery level as a red-to-green bar, shown on a chosen layer
 *
 * All positions are given as KEY POSITIONS (same order as the keymap), not
 * raw LED indices — the module converts them through key_to_led[].
 */

#include <zmk_vfx_pro_rgb/effects.h>
#include <zmk_vfx_pro_rgb/commands.h>

/* Any overlay that shows a lock LED of the host. */
#if defined(CONFIG_RGB_PRO_CAPS_INDICATOR) || defined(CONFIG_RGB_PRO_NUM_INDICATOR) || \
    defined(CONFIG_RGB_PRO_SCROLL_INDICATOR)
#define RGBP_LOCK_OVERLAYS 1
#else
#define RGBP_LOCK_OVERLAYS 0
#endif

#if RGBP_LOCK_OVERLAYS && RGB_PRO_IS_CENTRAL
#include <zmk/hid_indicators.h>
#elif RGBP_LOCK_OVERLAYS && RGB_PRO_HAS_HOST_INDICATORS
#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>
#endif

#if defined(CONFIG_RGB_PRO_STATUS_OVERLAY) && RGB_PRO_IS_CENTRAL
#include <zmk/endpoints.h>
#include <zmk/ble.h>
#endif

#if IS_ENABLED(CONFIG_RGB_PRO_BATTERY_INDICATOR)
#include <zmk/battery.h>
#endif

#if (defined(CONFIG_RGB_PRO_STATUS_OVERLAY) || defined(CONFIG_RGB_PRO_BATTERY_INDICATOR)) && RGB_PRO_IS_CENTRAL
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

/* Two flashes, a gap between them, then a longer pause, repeating.
 * Timeline per cycle: [on_ms lit][gap_ms off][on_ms lit][pause_ms off] */
static inline bool double_pulse(uint16_t on_ms, uint16_t gap_ms, uint16_t pause_ms) {
    uint32_t period = 2u * on_ms + gap_ms + pause_ms;
    uint32_t t = (uint32_t)(k_uptime_get() % period);
    return t < on_ms || (t >= on_ms + gap_ms && t < 2u * on_ms + gap_ms);
}

#define OFF   ((struct led_rgb){0, 0, 0})
#define RED   ((struct led_rgb){255, 0, 0})
#define GREEN ((struct led_rgb){0, 255, 0})
#define BLUE  ((struct led_rgb){0, 0, 255})
#define DIM_W ((struct led_rgb){12, 12, 12})

/* ---- State received from the dongle (used on a peripheral behind one) ---- */
static volatile uint8_t synced_layer;
static volatile uint8_t synced_status;

void rgbp_sync_layer(uint8_t layer)   { synced_layer = layer; }
void rgbp_sync_status(uint8_t packed) { synced_status = packed; }

#if (defined(CONFIG_RGB_PRO_STATUS_OVERLAY) || defined(CONFIG_RGB_PRO_BATTERY_INDICATOR)) && \
    RGB_PRO_HAS_LAYER_STATE
static inline uint8_t overlay_layer(void) {
#if RGB_PRO_IS_CENTRAL
    return zmk_keymap_highest_layer_active();
#else
    return synced_layer;
#endif
}
#endif

/* ---- Lock LEDs (HID indicators come from the host) ---- */
/* USB HID LED bits: 0 num lock, 1 caps lock, 2 scroll lock. */
#define RGBP_LED_NUM    0x01
#define RGBP_LED_CAPS   0x02
#define RGBP_LED_SCROLL 0x04

#if RGBP_LOCK_OVERLAYS && RGB_PRO_HAS_HOST_INDICATORS
#if RGB_PRO_IS_CENTRAL
static inline zmk_hid_indicators_t host_indicators(void) {
    return zmk_hid_indicators_get_current_profile();
}
#else
/* Peripheral behind a dongle: the state arrives as an event from the central,
 * and only when it changes. Keep the last value. */
static volatile zmk_hid_indicators_t peripheral_indicators;

static int host_indicators_listener(const zmk_event_t *eh) {
    const struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    if (ev) peripheral_indicators = ev->indicators;
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(rgbp_host_indicators, host_indicators_listener);
ZMK_SUBSCRIPTION(rgbp_host_indicators, zmk_hid_indicators_changed);

static inline zmk_hid_indicators_t host_indicators(void) { return peripheral_indicators; }
#endif
#endif /* RGBP_LOCK_OVERLAYS && RGB_PRO_HAS_HOST_INDICATORS */

#if defined(CONFIG_RGB_PRO_CAPS_INDICATOR) && RGB_PRO_HAS_HOST_INDICATORS
static void caps_overlay(void) {
    zmk_hid_indicators_t ind = host_indicators();
    if (!(ind & RGBP_LED_CAPS)) return;

#if IS_ENABLED(CONFIG_RGB_PRO_CAPS_STYLE_DOUBLE)
    bool lit = double_pulse(CONFIG_RGB_PRO_CAPS_DOUBLE_ON_MS,
                            CONFIG_RGB_PRO_CAPS_DOUBLE_GAP_MS,
                            CONFIG_RGB_PRO_CAPS_DOUBLE_PAUSE_MS);
#else
    bool lit = blink(CONFIG_RGB_PRO_CAPS_BLINK_MS);
#endif
    struct led_rgb c = lit
        ? (struct led_rgb){CONFIG_RGB_PRO_CAPS_R,
                           CONFIG_RGB_PRO_CAPS_G,
                           CONFIG_RGB_PRO_CAPS_B}
        : OFF;
    paint_key(CONFIG_RGB_PRO_CAPS_KEY, c);
}
#else
static void caps_overlay(void) {}
#endif

/* Steady colour while the lock is on (or, with SHOW_WHEN_OFF, while it is
 * off: num lock is on most of the time, so "off" is the state worth seeing). */
#if defined(CONFIG_RGB_PRO_NUM_INDICATOR) && RGB_PRO_HAS_HOST_INDICATORS
static void num_overlay(void) {
    bool on = host_indicators() & RGBP_LED_NUM;
    if (on == IS_ENABLED(CONFIG_RGB_PRO_NUM_SHOW_WHEN_OFF)) return;
    paint_key(CONFIG_RGB_PRO_NUM_KEY, (struct led_rgb){CONFIG_RGB_PRO_NUM_R,
                                                       CONFIG_RGB_PRO_NUM_G,
                                                       CONFIG_RGB_PRO_NUM_B});
}
#else
static void num_overlay(void) {}
#endif

#if defined(CONFIG_RGB_PRO_SCROLL_INDICATOR) && RGB_PRO_HAS_HOST_INDICATORS
static void scroll_overlay(void) {
    if (!(host_indicators() & RGBP_LED_SCROLL)) return;
    paint_key(CONFIG_RGB_PRO_SCROLL_KEY, (struct led_rgb){CONFIG_RGB_PRO_SCROLL_R,
                                                          CONFIG_RGB_PRO_SCROLL_G,
                                                          CONFIG_RGB_PRO_SCROLL_B});
}
#else
static void scroll_overlay(void) {}
#endif

/* ---- BLE / USB status (host link: read on the central, from the dongle on a peripheral) ---- */
#if defined(CONFIG_RGB_PRO_STATUS_OVERLAY) && RGB_PRO_HAS_LAYER_STATE
struct status_view {
    bool usb;     /* selected output is USB */
    int active;   /* active BLE profile */
    int state;    /* RGB_PRO_BT_* */
};

static struct status_view status_now(void) {
#if RGB_PRO_IS_CENTRAL
    struct status_view v;
    v.usb = zmk_endpoint_get_selected().transport == ZMK_TRANSPORT_USB;
    v.active = zmk_ble_active_profile_index();
    v.state = zmk_ble_active_profile_is_connected() ? RGB_PRO_BT_CONNECTED
            : zmk_ble_active_profile_is_open()      ? RGB_PRO_BT_OPEN
                                                    : RGB_PRO_BT_PAIRED;
    return v;
#else
    uint8_t s = synced_status;
    return (struct status_view){.usb = RGB_PRO_STATUS_USB(s),
                                .active = RGB_PRO_STATUS_PROFILE(s),
                                .state = RGB_PRO_STATUS_STATE(s)};
#endif
}

static void status_overlay(void) {
    if (overlay_layer() != CONFIG_RGB_PRO_STATUS_LAYER) return;

    bool on = blink(CONFIG_RGB_PRO_STATUS_BLINK_MS);
    struct status_view sv = status_now();

    /* Output selector: blue = BLE, red = USB. */
    paint_key(CONFIG_RGB_PRO_STATUS_OUT_KEY, sv.usb ? RED : BLUE);

    /* Profile keys. Only the active profile's real state is queryable
     * through ZMK's public API; the rest are shown dim. */
    for (int i = 0; i < CONFIG_RGB_PRO_STATUS_BT_COUNT; i++) {
        uint8_t pos = CONFIG_RGB_PRO_STATUS_BT_KEY_BASE + i;
        struct led_rgb c;

        if (i != sv.active) {
            c = DIM_W;
        } else if (sv.state == RGB_PRO_BT_CONNECTED) {
            c = BLUE;                       /* connected: solid blue */
        } else if (sv.state == RGB_PRO_BT_OPEN) {
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

/* ---- Battery level (each half shows its own pack) ---- */
#if defined(CONFIG_RGB_PRO_BATTERY_INDICATOR) && RGB_PRO_HAS_LAYER_STATE
static void battery_overlay(void) {
    if (overlay_layer() != CONFIG_RGB_PRO_BATTERY_LAYER) return;

    uint8_t pct = zmk_battery_state_of_charge();
    const int count = CONFIG_RGB_PRO_BATTERY_KEY_COUNT;

    /* Hue sweeps red (0deg) to green (120deg). Anything at or below the
     * floor stays pure red, so a low battery reads unmistakably as red
     * instead of drifting into orange. */
    const int floor_pct = CONFIG_RGB_PRO_BATTERY_RED_BELOW;
    uint16_t hue = (pct <= floor_pct)
        ? 0
        : (uint16_t)(pct - floor_pct) * 120 / (100 - floor_pct);
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
    num_overlay();
    scroll_overlay();
    status_overlay();
    battery_overlay();
}
