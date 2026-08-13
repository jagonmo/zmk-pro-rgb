/*
 * Tanda 2 - Animated (RNG / stateful) effects, ported from QMK RGB Matrix.
 *
 * These need either a PRNG (raindrops) or a per-frame moving function
 * (hue wave/pendulum, pixel flow). State that must persist across frames
 * lives in file-static buffers.
 */

#include <zmk_pro_rgb/effects.h>

#define NKEYS RGB_PRO_KEYS

/* ---- xorshift8 PRNG (shared via effects.h) ---- */
static uint8_t rng_state = 0xA3;
uint8_t rgbp_rand8(void) {
    uint8_t x = rng_state;
    x ^= x << 1;
    x ^= x >> 1;
    x ^= x << 2;
    rng_state = x ? x : 0x5B; /* avoid lockup at 0 */
    return rng_state;
}

/* Per-LED hue buffer for raindrop-style effects (persists across frames). */
static uint8_t drop_hue[NKEYS];
static uint8_t drop_val[NKEYS];
static bool drops_init;

static void ensure_drops(void) {
    if (drops_init) {
        return;
    }
    for (int i = 0; i < NKEYS; i++) {
        drop_hue[i] = rgbp_rand8();
        drop_val[i] = 0;
    }
    drops_init = true;
}

/* Raindrops: occasionally light a random key at the base hue, then all fade. */
static void e_raindrops(bool jellybean) {
    ensure_drops();

    /* every few frames, seed a new drop */
    if ((state.phase & 0x03) == 0) {
        uint8_t k = rgbp_rand8() % NKEYS;
        drop_val[k] = 255;
        drop_hue[k] = jellybean ? rgbp_rand8()          /* random hue */
                                : (state.hue * 255 / 360); /* base hue */
    }

    for (int i = 0; i < NKEYS; i++) {
        if (drop_val[i] > 6) {
            drop_val[i] -= 6;
        } else {
            drop_val[i] = 0;
        }
        uint16_t h = drop_hue[i] * 360 / 255;
        uint8_t v = (uint32_t)state.brt * drop_val[i] / 255;
        pixels[i] = rgbp_hsb(h, state.sat, v);
    }
}

/* Hue breathing: whole board one hue, value breathes. */
static void e_hue_breathing(void) {
    uint8_t p = state.phase & 0xFF;
    uint8_t tri = (state.phase & 0x100) ? (255 - p) : p;
    uint8_t v = (uint32_t)state.brt * tri / 255;
    struct led_rgb c = rgbp_hsb(state.hue, state.sat, v);
    for (int i = 0; i < NKEYS; i++) {
        pixels[i] = c;
    }
}

/* Hue pendulum: hue swings back and forth over a range across columns. */
static void e_hue_pendulum(void) {
    uint8_t p = state.phase & 0xFF;
    int swing = (state.phase & 0x100) ? (255 - p) : p; /* triangle 0-255 */
    for (int i = 0; i < NKEYS; i++) {
        uint16_t h = (state.hue + key_col[i] * 8 + swing) % 360;
        pixels[i] = rgbp_hsb(h, state.sat, state.brt);
    }
}

/* Hue wave: a sine-ish hue wave travelling across columns. */
static void e_hue_wave(void) {
    for (int i = 0; i < NKEYS; i++) {
        uint16_t h = (state.hue + (key_col[i] * 20) + state.phase * 3) % 360;
        pixels[i] = rgbp_hsb(h, state.sat, state.brt);
    }
}

/* Pixel fractal: mirrored moving bands from center columns outward. */
static void e_pixel_fractal(void) {
    for (int i = 0; i < NKEYS; i++) {
        int dc = key_col[i] * 2 - (RGB_PRO_COLS - 1);
        if (dc < 0) dc = -dc;
        uint8_t band = (dc * 16 + state.phase * 4) & 0xFF;
        uint8_t v = (uint32_t)state.brt * band / 255;
        pixels[i] = rgbp_hsb(state.hue, state.sat, v);
    }
}

/* Pixel flow: each LED wanders in brightness pseudo-randomly but smoothly. */
static void e_pixel_flow(void) {
    for (int i = 0; i < NKEYS; i++) {
        uint8_t n = (i * 37 + state.phase * 5) & 0xFF;
        uint8_t tri = (n & 0x80) ? (255 - n) : n;
        uint16_t h = (state.hue + i * 4) % 360;
        uint8_t v = (uint32_t)state.brt * tri / 255;
        pixels[i] = rgbp_hsb(h, state.sat, v);
    }
}

/* Pixel rain: random keys flash on at random hue, then fade (like raindrops
 * but denser and full random hue). */
static void e_pixel_rain(void) {
    ensure_drops();
    if ((state.phase & 0x01) == 0) {
        uint8_t k = rgbp_rand8() % NKEYS;
        drop_val[k] = 255;
        drop_hue[k] = rgbp_rand8();
    }
    for (int i = 0; i < NKEYS; i++) {
        if (drop_val[i] > 10) {
            drop_val[i] -= 10;
        } else {
            drop_val[i] = 0;
        }
        uint16_t h = drop_hue[i] * 360 / 255;
        uint8_t v = (uint32_t)state.brt * drop_val[i] / 255;
        pixels[i] = rgbp_hsb(h, state.sat, v);
    }
}

/* ---- Dispatch ---- */
void rgbp_render_animated(enum rgb_pro_effect eff) {
    switch (eff) {
    case RGB_PRO_EFF_RAINDROPS:            e_raindrops(false); break;
    case RGB_PRO_EFF_JELLYBEAN_RAINDROPS:  e_raindrops(true); break;
    case RGB_PRO_EFF_HUE_BREATHING:        e_hue_breathing(); break;
    case RGB_PRO_EFF_HUE_PENDULUM:         e_hue_pendulum(); break;
    case RGB_PRO_EFF_HUE_WAVE:             e_hue_wave(); break;
    case RGB_PRO_EFF_PIXEL_FRACTAL:        e_pixel_fractal(); break;
    case RGB_PRO_EFF_PIXEL_FLOW:           e_pixel_flow(); break;
    case RGB_PRO_EFF_PIXEL_RAIN:           e_pixel_rain(); break;
    default: {
        struct led_rgb c = rgbp_hsb(state.hue, state.sat, state.brt);
        for (int i = 0; i < NKEYS; i++) pixels[i] = c;
        break;
    }
    }
}
