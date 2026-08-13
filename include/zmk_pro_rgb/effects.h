#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/led_strip.h>

/* Matrix geometry from Kconfig (set by the user's .conf). */
#define RGB_PRO_COLS CONFIG_RGB_PRO_MATRIX_COLS
#define RGB_PRO_ROWS CONFIG_RGB_PRO_MATRIX_ROWS
#define RGB_PRO_KEYS CONFIG_RGB_PRO_NUM_KEYS

/* ---- Central effect enum ----
 * Order defines the cycle order for RGBP_EFF/RGBP_EFR. */
enum rgb_pro_effect {
    /* Base (non-reactive) */
    RGB_PRO_EFF_SOLID = 0,
    RGB_PRO_EFF_BREATHING,
    RGB_PRO_EFF_BAND_SAT,
    RGB_PRO_EFF_BAND_VAL,
    RGB_PRO_EFF_BAND_PINWHEEL_SAT,
    RGB_PRO_EFF_BAND_PINWHEEL_VAL,
    RGB_PRO_EFF_BAND_SPIRAL_SAT,
    RGB_PRO_EFF_BAND_SPIRAL_VAL,
    RGB_PRO_EFF_CYCLE_ALL,
    RGB_PRO_EFF_CYCLE_LEFT_RIGHT,
    RGB_PRO_EFF_CYCLE_UP_DOWN,
    RGB_PRO_EFF_RAINBOW_MOVING_CHEVRON,
    RGB_PRO_EFF_CYCLE_OUT_IN,
    RGB_PRO_EFF_CYCLE_OUT_IN_DUAL,
    RGB_PRO_EFF_CYCLE_PINWHEEL,
    RGB_PRO_EFF_CYCLE_SPIRAL,
    RGB_PRO_EFF_DUAL_BEACON,
    RGB_PRO_EFF_RAINBOW_BEACON,
    RGB_PRO_EFF_RAINBOW_PINWHEELS,

    /* Animated (RNG / stateful) */
    RGB_PRO_EFF_RAINDROPS,
    RGB_PRO_EFF_JELLYBEAN_RAINDROPS,
    RGB_PRO_EFF_HUE_BREATHING,
    RGB_PRO_EFF_HUE_PENDULUM,
    RGB_PRO_EFF_HUE_WAVE,
    RGB_PRO_EFF_PIXEL_FRACTAL,
    RGB_PRO_EFF_PIXEL_FLOW,
    RGB_PRO_EFF_PIXEL_RAIN,

    /* Reactive (key-triggered) */
    RGB_PRO_EFF_TYPING_HEATMAP,
    RGB_PRO_EFF_DIGITAL_RAIN,
    RGB_PRO_EFF_SOLID_REACTIVE_SIMPLE,
    RGB_PRO_EFF_SOLID_REACTIVE,
    RGB_PRO_EFF_SOLID_REACTIVE_CROSS,
    RGB_PRO_EFF_SPLASH,

    /* Custom extras */
    RGB_PRO_EFF_FLAG,
    RGB_PRO_EFF_LAYER_COLOR,
    RGB_PRO_EFF_COMPLEMENT,

    RGB_PRO_EFF_NUM,
};

/* ---- Shared global state (defined in rgb_pro.c) ---- */
struct rgb_pro_state {
    bool on;
    enum rgb_pro_effect effect;
    uint16_t hue;   /* 0-359 */
    uint8_t sat;    /* 0-100 */
    uint8_t brt;    /* 0-100 */
    uint8_t speed;  /* 1-10 */
    uint16_t phase; /* animation counter */
};

extern struct rgb_pro_state state;
extern struct led_rgb pixels[];
extern uint8_t reactive[];

/* LED mapping tables (defined by the user's board-specific header,
 * included from rgb_pro.c). */
extern const uint8_t key_to_led[];
extern const uint8_t key_col[];
extern const uint8_t key_row[];

/* ---- Shared helpers (defined in rgb_pro.c) ---- */
struct led_rgb rgbp_hsb(uint16_t h, uint8_t s, uint8_t b);
struct led_rgb rgbp_scale(struct led_rgb c, uint8_t factor);

/* ---- PRNG (defined in effects_animated.c) ---- */
uint8_t rgbp_rand8(void);

/* ---- Effect renderers ---- */
void rgbp_render_base(enum rgb_pro_effect eff);
void rgbp_render_animated(enum rgb_pro_effect eff);
void rgbp_render_reactive(enum rgb_pro_effect eff);
void rgbp_render_extra(enum rgb_pro_effect eff);

/* Record a key press for spatial reactive effects. */
void rgbp_reactive_note_press(uint8_t led);
