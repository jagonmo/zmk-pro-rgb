/*
 * Tanda 3 - Reactive (key-triggered) effects, ported from QMK RGB Matrix.
 *
 * Two mechanisms:
 *   1. reactive[] (per-LED intensity, set to 255 on press, decayed by the
 *      core) - used by the simple reactive effects.
 *   2. A small ring of recent presses (position + age) - used by the
 *      spatial effects (wide/cross/nexus/splash) that radiate from the key.
 */

#include <zmk_pro_rgb/effects.h>

#define NKEYS RGB_PRO_KEYS

/* ---- Recent-press ring (for spatial effects) ---- */
#define HIST 8
struct press { uint8_t led; uint16_t frame; bool used; };
static struct press hist[HIST];
static uint8_t hist_head;

/* Called by the core listener path via this helper (see note in core). */
void rgbp_reactive_note_press(uint8_t led); /* fwd */
void rgbp_reactive_note_press(uint8_t led) {
    hist[hist_head].led = led;
    hist[hist_head].frame = state.phase;
    hist[hist_head].used = true;
    hist_head = (hist_head + 1) % HIST;
}

/* Manhattan-ish distance between two LEDs in grid units. */
static int grid_dist(uint8_t a, uint8_t b) {
    int dc = (int)led_col[a] - led_col[b];
    int dr = ((int)led_row[a] - led_row[b]) * 2; /* rows taller */
    if (dc < 0) dc = -dc;
    if (dr < 0) dr = -dr;
    return dc + dr;
}

/* ---- Simple reactive (use reactive[] buffer) ---- */

static void e_reactive_simple(void) {
    /* pressed keys light to full, everything else off */
    for (int i = 0; i < NKEYS; i++) {
        uint8_t v = (uint32_t)state.brt * reactive[i] / 255;
        pixels[i] = rgbp_hsb(state.hue, state.sat, v);
    }
}

static void e_reactive(void) {
    /* like simple but hue shifts slightly with intensity */
    for (int i = 0; i < NKEYS; i++) {
        if (reactive[i]) {
            uint16_t h = (state.hue + (255 - reactive[i]) / 4) % 360;
            uint8_t v = (uint32_t)state.brt * reactive[i] / 255;
            pixels[i] = rgbp_hsb(h, state.sat, v);
        } else {
            pixels[i] = (struct led_rgb){0, 0, 0};
        }
    }
}

/* ---- Spatial reactive (radiate from recent presses) ----
 * shape: 0=wide(row), 1=cross, 2=nexus, 3=splash(radial)
 * multi: use all history entries (true) or just the latest (false)
 * solid: keep background at base color (true) or black (false)
 */
static void e_spatial(int shape, bool multi, bool solid) {
    /* background */
    struct led_rgb bg = solid ? rgbp_hsb(state.hue, state.sat, state.brt / 8)
                              : (struct led_rgb){0, 0, 0};
    for (int i = 0; i < NKEYS; i++) {
        pixels[i] = bg;
    }

    int start = multi ? 0 : (HIST - 1);
    for (int h = start; h < HIST; h++) {
        int idx = multi ? h : ((hist_head + HIST - 1) % HIST);
        if (!hist[idx].used) {
            continue;
        }
        uint16_t age = state.phase - hist[idx].frame;
        if (age > 40) {
            continue; /* faded out */
        }
        uint8_t life = 255 - (age * 6); /* 0..255 as it ages */
        uint8_t src = hist[idx].led;

        for (int i = 0; i < NKEYS; i++) {
            int show = 0;
            int dc = (int)led_col[i] - led_col[src];
            int dr = (int)led_row[i] - led_row[src];
            int adc = dc < 0 ? -dc : dc;
            int adr = dr < 0 ? -dr : dr;

            switch (shape) {
            case 0: /* wide: same row, spreading horizontally */
                if (adr == 0 && adc <= (age / 3)) show = 1;
                break;
            case 1: /* cross: same row or same column, within radius */
                if ((adr == 0 || adc == 0) && (adc + adr) <= (age / 3)) show = 1;
                break;
            case 2: /* nexus: column + a horizontal bar at source row */
                if (adc == 0 || (adr == 0 && adc <= (age / 3))) show = 1;
                break;
            default: { /* splash: radial ring expanding with age */
                int d = grid_dist(i, src);
                int radius = age / 2;
                if (d >= radius - 1 && d <= radius + 1) show = 1;
                break;
            }
            }

            if (show) {
                uint16_t hue = (state.hue + age * 4) % 360;
                uint8_t v = (uint32_t)state.brt * life / 255;
                struct led_rgb c = rgbp_hsb(hue, state.sat, v);
                /* additive-ish: take the brighter */
                if (c.r + c.g + c.b > pixels[i].r + pixels[i].g + pixels[i].b) {
                    pixels[i] = c;
                }
            }
        }

        if (!multi) {
            break;
        }
    }
}

/* Typing heatmap: keys stay warm-colored proportional to reactive[]. */
static void e_typing_heatmap(void) {
    for (int i = 0; i < NKEYS; i++) {
        /* map intensity to hue: blue(240) cold -> red(0) hot */
        uint16_t h = 240 - (240 * reactive[i] / 255);
        uint8_t v = reactive[i] ? state.brt : state.brt / 10;
        pixels[i] = rgbp_hsb(h, state.sat, v);
    }
}

/* Digital rain: green columns with falling bright heads. */
static void e_digital_rain(void) {
    for (int i = 0; i < NKEYS; i++) {
        /* per-column falling head based on phase and a column offset */
        uint8_t col = led_col[i];
        uint8_t head = (state.phase / 2 + col * 7) % (RGB_PRO_ROWS * 2);
        int d = (int)led_row[i] - head;
        uint8_t v;
        if (d == 0) {
            v = state.brt;                 /* bright head */
        } else if (d > 0 && d < 4) {
            v = state.brt / (d + 1);       /* fading tail */
        } else {
            v = 0;
        }
        pixels[i] = rgbp_hsb(120, state.sat, v); /* green */
    }
}

/* ---- Dispatch ---- */
void rgbp_render_reactive(enum rgb_pro_effect eff) {
    switch (eff) {
    case RGB_PRO_EFF_TYPING_HEATMAP:            e_typing_heatmap(); break;
    case RGB_PRO_EFF_DIGITAL_RAIN:              e_digital_rain(); break;
    case RGB_PRO_EFF_SOLID_REACTIVE_SIMPLE:     e_reactive_simple(); break;
    case RGB_PRO_EFF_SOLID_REACTIVE:            e_reactive(); break;
    case RGB_PRO_EFF_SOLID_REACTIVE_CROSS:      e_spatial(1, true,  false); break;
    case RGB_PRO_EFF_SPLASH:                    e_spatial(3, true,  false); break;
    default: {
        struct led_rgb base = rgbp_hsb(state.hue, state.sat, state.brt / 6);
        for (int i = 0; i < NKEYS; i++) {
            pixels[i] = reactive[i]
                ? rgbp_scale(rgbp_hsb(state.hue, state.sat, state.brt), reactive[i])
                : base;
        }
        break;
    }
    }
}
