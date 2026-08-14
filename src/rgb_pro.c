/*
 * ZMK Pro RGB — Core
 *
 * The LED mapping arrays (key_to_led, key_col, key_row) must be provided
 * by the user in a file called "rgb_pro_led_map.h" placed in their shield
 * directory (e.g. config/boards/shields/myboard/rgb_pro_led_map.h).
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led_strip.h>
#include <zephyr/drivers/regulator.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/keymap.h>

#if IS_ENABLED(CONFIG_RGB_PRO_CAPS_INDICATOR)
#endif

#include <zmk_vfx_pro_rgb/rgb_pro.h>
#include <zmk_vfx_pro_rgb/effects.h>

/* Board-specific LED mapping — provided by the user's zmk-config. */
#include <rgb_pro_led_map.h>

LOG_MODULE_REGISTER(rgb_pro, CONFIG_ZMK_LOG_LEVEL);

#define STRIP_CHOSEN DT_CHOSEN(zmk_underglow)
#define STRIP_NUM    DT_PROP(STRIP_CHOSEN, chain_length)
#define NKEYS        RGB_PRO_KEYS

BUILD_ASSERT(STRIP_NUM >= NKEYS, "LED chain shorter than RGB_PRO_NUM_KEYS");

static const struct device *led_strip = DEVICE_DT_GET(STRIP_CHOSEN);

/* ---- Shared globals ---- */
struct led_rgb pixels[NKEYS];
uint8_t reactive[NKEYS];

/* LED-indexed geometry tables, built at init from the user's key-indexed
 * tables (key_to_led, key_col, key_row). Effects always index these by
 * physical LED number, not key position. */
uint8_t led_col[NKEYS];
uint8_t led_row[NKEYS];

static void build_led_geometry(void) {
    for (int pos = 0; pos < NKEYS; pos++) {
        uint8_t led = key_to_led[pos];
        if (led < NKEYS) {
            led_col[led] = key_col[pos];
            led_row[led] = key_row[pos];
        }
    }
}

struct rgb_pro_state state = {
    .on     = IS_ENABLED(CONFIG_RGB_PRO_START_ON),
    .effect = RGB_PRO_EFF_SOLID,
    .hue    = 0,
    .sat    = 100,
    .brt    = CONFIG_RGB_PRO_BRT_START,
    .speed  = 4,
};

/* ---- HSB to RGB (integer math) ---- */
struct led_rgb rgbp_hsb(uint16_t h, uint8_t s, uint8_t b) {
    uint32_t rp = 0, gp = 0, bp = 0;
    uint8_t sector = (h / 60) % 6;
    uint32_t f = h % 60, v = b;
    uint32_t p = v * (100 - s) / 100;
    uint32_t q = v * (100 - (s * f) / 60) / 100;
    uint32_t t = v * (100 - (s * (60 - f)) / 60) / 100;
    switch (sector) {
    case 0: rp=v; gp=t; bp=p; break; case 1: rp=q; gp=v; bp=p; break;
    case 2: rp=p; gp=v; bp=t; break; case 3: rp=p; gp=q; bp=v; break;
    case 4: rp=t; gp=p; bp=v; break; case 5: rp=v; gp=p; bp=q; break;
    }
    return (struct led_rgb){.r=(uint8_t)(rp*255/100),.g=(uint8_t)(gp*255/100),.b=(uint8_t)(bp*255/100)};
}

struct led_rgb rgbp_scale(struct led_rgb c, uint8_t factor) {
    c.r=(uint16_t)c.r*factor/255; c.g=(uint16_t)c.g*factor/255; c.b=(uint16_t)c.b*factor/255;
    return c;
}

/* ---- Reactive decay ---- */
static void decay_reactive(void) {
    for (int i = 0; i < NKEYS; i++) {
        reactive[i] = reactive[i] > CONFIG_RGB_PRO_REACTIVE_DECAY
            ? reactive[i] - CONFIG_RGB_PRO_REACTIVE_DECAY : 0;
    }
}


/* ---- Dispatch ---- */
static void render_current(void) {
    enum rgb_pro_effect e = state.effect;
    if      (e >= RGB_PRO_EFF_FLAG)            rgbp_render_extra(e);
    else if (e >= RGB_PRO_EFF_TYPING_HEATMAP)  rgbp_render_reactive(e);
    else if (e >= RGB_PRO_EFF_RAINDROPS)       rgbp_render_animated(e);
    else                                       rgbp_render_base(e);
}

/* ---- Tick ---- */
static void rgb_pro_tick(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(rgb_pro_work, rgb_pro_tick);

static void rgb_pro_tick(struct k_work *work) {
    if (!state.on) return;
    render_current();
    decay_reactive();
    rgbp_render_overlays();
    led_strip_update_rgb(led_strip, pixels, STRIP_NUM);
    state.phase += state.speed;
    k_work_reschedule(&rgb_pro_work, K_MSEC(CONFIG_RGB_PRO_TICK_MS));
}

static void clear_strip(void) {
    memset(pixels, 0, sizeof(pixels));
    led_strip_update_rgb(led_strip, pixels, STRIP_NUM);
}

/* ---- Ext-power (optional) ---- */
#if DT_NODE_EXISTS(DT_NODELABEL(rgb_pro_pwr))
static const struct device *led_pwr = DEVICE_DT_GET(DT_NODELABEL(rgb_pro_pwr));
static void ext_power_set(bool on) {
    if (!led_pwr || !device_is_ready(led_pwr)) return;
    on ? regulator_enable(led_pwr) : regulator_disable(led_pwr);
}
#else
static void ext_power_set(bool on) { ARG_UNUSED(on); }
#endif

static void start_anim(void) { ext_power_set(true); k_work_reschedule(&rgb_pro_work, K_NO_WAIT); }
static void stop_anim(void) { k_work_cancel_delayable(&rgb_pro_work); clear_strip(); ext_power_set(false); }

/* ---- Behavior command ---- */
int rgb_pro_command(uint8_t cmd, uint8_t param) {
    ARG_UNUSED(param);
    switch (cmd) {
    case RGB_PRO_CMD_TOGGLE:   state.on = !state.on; break;
    case RGB_PRO_CMD_ON:       state.on = true; break;
    case RGB_PRO_CMD_OFF:      state.on = false; break;
    case RGB_PRO_CMD_EFF_NEXT: state.effect = (state.effect + 1) % RGB_PRO_EFF_NUM; break;
    case RGB_PRO_CMD_EFF_PREV: state.effect = (state.effect + RGB_PRO_EFF_NUM - 1) % RGB_PRO_EFF_NUM; break;
    case RGB_PRO_CMD_HUE_UP:   state.hue = (state.hue + CONFIG_RGB_PRO_HUE_STEP) % 360; break;
    case RGB_PRO_CMD_HUE_DN:   state.hue = (state.hue + 360 - CONFIG_RGB_PRO_HUE_STEP) % 360; break;
    case RGB_PRO_CMD_BRT_UP:   state.brt = MIN(100, state.brt + CONFIG_RGB_PRO_BRT_STEP); break;
    case RGB_PRO_CMD_BRT_DN:   state.brt = state.brt > CONFIG_RGB_PRO_BRT_STEP ? state.brt - CONFIG_RGB_PRO_BRT_STEP : 0; break;
    case RGB_PRO_CMD_SPD_UP:   if (state.speed < 10) state.speed++; break;
    case RGB_PRO_CMD_SPD_DN:   if (state.speed > 1)  state.speed--; break;
    default: return -ENOTSUP;
    }
    state.on ? start_anim() : stop_anim();
    return 0;
}

/* ---- Key listener ---- */
static int rgb_pro_key_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *ev = as_zmk_position_state_changed(eh);
    if (!ev || !ev->state || ev->position >= NKEYS) return ZMK_EV_EVENT_BUBBLE;
    uint8_t led = key_to_led[ev->position];
    if (led < NKEYS) { reactive[led] = 255; rgbp_reactive_note_press(led); }
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(rgb_pro_keys, rgb_pro_key_listener);
ZMK_SUBSCRIPTION(rgb_pro_keys, zmk_position_state_changed);

/* ---- Idle listener ---- */
#if IS_ENABLED(CONFIG_RGB_PRO_AUTO_OFF_IDLE)
static int rgb_pro_activity_listener(const zmk_event_t *eh) {
    const struct zmk_activity_state_changed *ev = as_zmk_activity_state_changed(eh);
    if (!ev) return ZMK_EV_EVENT_BUBBLE;
    (ev->state == ZMK_ACTIVITY_ACTIVE && state.on) ? start_anim() : stop_anim();
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(rgb_pro_activity, rgb_pro_activity_listener);
ZMK_SUBSCRIPTION(rgb_pro_activity, zmk_activity_state_changed);
#endif

/* ---- Init ---- */
static int rgb_pro_init(void) {
    if (!device_is_ready(led_strip)) { LOG_ERR("LED strip not ready"); return -ENODEV; }
    build_led_geometry();
    if (state.on) start_anim();
    return 0;
}
SYS_INIT(rgb_pro_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
