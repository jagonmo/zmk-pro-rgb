/*
 * Extra - Custom effects (flag, layer-color).
 * Compiled but NOT part of the active RGBP_EFF cycle. Reachable only if
 * state.effect is set to one of these explicitly.
 */

#include <zmk_pro_rgb/effects.h>

#include <zmk/keymap.h>

/* Flag: horizontal rainbow across columns, travelling left-to-right, with a
 * slight per-row skew so it undulates like a flag. */
static void e_flag(void) {
    for (int i = 0; i < RGB_PRO_KEYS; i++) {
        uint16_t col_hue = led_col[i] * 360 / RGB_PRO_COLS;
        uint16_t row_skew = led_row[i] * 18;
        uint16_t h = (state.hue + state.phase + col_hue + row_skew) % 360;
        pixels[i] = rgbp_hsb(h, state.sat, state.brt);
    }
}

/* Layer-color: base hue by highest active layer; pressed keys flash the
 * complementary color. */
static void e_layer_color(void) {
    uint8_t layer = zmk_keymap_highest_layer_active();
    uint16_t h = (state.hue + layer * 40) % 360;
    struct led_rgb base = rgbp_hsb(h, state.sat, state.brt);
    for (int i = 0; i < RGB_PRO_KEYS; i++) {
        pixels[i] = base;
        if (reactive[i]) {
            struct led_rgb hot = rgbp_hsb((h + 180) % 360, state.sat, state.brt);
            pixels[i] = rgbp_scale(hot, reactive[i]);
        }
    }
}

/* Complement: fixed base-hue background; pressed keys flash the complementary
 * color (hue + 180). Like layer-color but the background never changes. */
static void e_complement(void) {
    struct led_rgb base = rgbp_hsb(state.hue, state.sat, state.brt);
    struct led_rgb comp = rgbp_hsb((state.hue + 180) % 360, state.sat, state.brt);
    for (int i = 0; i < RGB_PRO_KEYS; i++) {
        pixels[i] = reactive[i] ? rgbp_scale(comp, reactive[i]) : base;
    }
}

void rgbp_render_extra(enum rgb_pro_effect eff) {
    switch (eff) {
    case RGB_PRO_EFF_FLAG:        e_flag(); break;
    case RGB_PRO_EFF_LAYER_COLOR: e_layer_color(); break;
    case RGB_PRO_EFF_COMPLEMENT:  e_complement(); break;
    default:                        e_flag(); break;
    }
}
