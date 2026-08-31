/*
 * Tanda 1 - Base (non-reactive) effects, ported from QMK RGB Matrix.
 *
 * All effects write the physical pixels[] buffer using the shared state
 * (hue/sat/brt/phase) and the geometry tables (key_row/key_col). Math is
 * integer-only to keep the nRF52840 light.
 */

#include <zmk_vfx_pro_rgb/effects.h>

#define NKEYS RGB_PRO_KEYS

/* Center of the board in column/row units, x2 for half-step precision. */
#define CX2 (RGB_PRO_COLS - 1)   /* 13 -> center col = 6.5 */
#define CY2 (RGB_PRO_ROWS - 1)   /* 5  -> center row = 2.5 */

/* 16-point integer atan2 approximation returning 0-359 degrees.
 * Good enough for smooth pinwheel/spiral hue assignment. */
static uint16_t atan2_deg(int dx, int dy) {
    if (dx == 0 && dy == 0) {
        return 0;
    }
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    int a;
    if (adx >= ady) {
        a = (ady * 45) / (adx == 0 ? 1 : adx); /* 0-45 within octant */
    } else {
        a = 90 - (adx * 45) / (ady == 0 ? 1 : ady);
    }
    /* place in the right quadrant */
    if (dx >= 0 && dy >= 0) {
        return a;                 /* Q1 */
    } else if (dx < 0 && dy >= 0) {
        return 180 - a;           /* Q2 */
    } else if (dx < 0 && dy < 0) {
        return 180 + a;           /* Q3 */
    } else {
        return 360 - a;           /* Q4 */
    }
}

/* Chebyshev-ish distance from center (col/row scaled to a common unit). */
static uint16_t dist_center(int i) {
    int dx = led_col[i] * 2 - CX2;
    int dy = led_row[i] * 4 - CY2 * 2; /* rows are taller; scale up */
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return (uint16_t)(dx + dy);
}

/* ---- Base fills ---- */

static void e_solid(void) {
    struct led_rgb c = rgbp_hsb(state.hue, state.sat, state.brt);
    for (int i = 0; i < NKEYS; i++) pixels[i] = c;
}

static void e_breathing(void) {
    /* Sine-shaped brightness instead of a linear triangle: the fade
     * decelerates near full/empty and accelerates through the middle,
     * which reads as a natural breath rather than a metronome. */
    uint8_t s = rgbp_sin8((uint8_t)(state.phase & 0xFF));
    uint8_t v = (uint32_t)state.brt * s / 255;
    struct led_rgb c = rgbp_hsb(state.hue, state.sat, v);
    for (int i = 0; i < NKEYS; i++) pixels[i] = c;
}

/* Band: a moving bright/saturated band across columns. */
static void e_band_val(void) {
    uint8_t pos = state.phase % (RGB_PRO_COLS * 2);
    for (int i = 0; i < NKEYS; i++) {
        int d = led_col[i] - pos;
        if (d < 0) d = -d;
        int v = state.brt - d * (state.brt / 4);
        if (v < 0) v = 0;
        pixels[i] = rgbp_hsb(state.hue, state.sat, v);
    }
}

static void e_band_sat(void) {
    uint8_t pos = state.phase % (RGB_PRO_COLS * 2);
    for (int i = 0; i < NKEYS; i++) {
        int d = led_col[i] - pos;
        if (d < 0) d = -d;
        int s = state.sat - d * (state.sat / 4);
        if (s < 0) s = 0;
        pixels[i] = rgbp_hsb(state.hue, s, state.brt);
    }
}

/* Pinwheel: angle from center drives the band. Single arm (band goes
 * 0-180-0 once per revolution) -> use the reference speed directly. */
static void e_band_pinwheel(bool sat) {
    for (int i = 0; i < NKEYS; i++) {
        int dx = led_col[i] * 2 - CX2;
        int dy = led_row[i] * 4 - CY2 * 2;
        uint16_t ang = atan2_deg(dx, dy);
        int d = (ang + state.phase * RGB_PRO_SPEED_REF) % 360;
        int band = d < 180 ? d : 360 - d; /* 0-180 triangle */
        if (sat) {
            uint8_t s = state.sat - band * state.sat / 180;
            pixels[i] = rgbp_hsb(state.hue, s, state.brt);
        } else {
            uint8_t v = state.brt - band * state.brt / 180;
            pixels[i] = rgbp_hsb(state.hue, state.sat, v);
        }
    }
}

/* Spiral: angle + distance from center. The distance term already sweeps
 * RGB_PRO_MAX_DIST*12 degrees across the board (that's what makes it a
 * spiral, not a pinwheel) -> that's the "arm count" for this effect, so
 * the temporal term is scaled down in proportion. */
static void e_band_spiral(bool sat) {
    const int arms = 1 + (RGB_PRO_MAX_DIST * 12) / 360;
    const int k = RGB_PRO_SPEED_REF > arms ? RGB_PRO_SPEED_REF / arms : 1;
    for (int i = 0; i < NKEYS; i++) {
        int dx = led_col[i] * 2 - CX2;
        int dy = led_row[i] * 4 - CY2 * 2;
        uint16_t ang = atan2_deg(dx, dy);
        int d = (ang + dist_center(i) * 12 + state.phase * k) % 360;
        int band = d < 180 ? d : 360 - d;
        if (sat) {
            uint8_t s = state.sat - band * state.sat / 180;
            pixels[i] = rgbp_hsb(state.hue, s, state.brt);
        } else {
            uint8_t v = state.brt - band * state.brt / 180;
            pixels[i] = rgbp_hsb(state.hue, state.sat, v);
        }
    }
}

/* ---- Rainbow cycles ---- */

static void e_cycle_all(void) {
    uint16_t h = (state.hue + state.phase * RGB_PRO_SPEED_REF) % 360;
    struct led_rgb c = rgbp_hsb(h, state.sat, state.brt);
    for (int i = 0; i < NKEYS; i++) pixels[i] = c;
}

static void e_cycle_left_right(void) {
    for (int i = 0; i < NKEYS; i++) {
        uint16_t h = (state.hue + state.phase * RGB_PRO_SPEED_REF + led_col[i] * 360 / RGB_PRO_COLS) % 360;
        pixels[i] = rgbp_hsb(h, state.sat, state.brt);
    }
}

static void e_cycle_up_down(void) {
    for (int i = 0; i < NKEYS; i++) {
        uint16_t h = (state.hue + state.phase * RGB_PRO_SPEED_REF + led_row[i] * 360 / RGB_PRO_ROWS) % 360;
        pixels[i] = rgbp_hsb(h, state.sat, state.brt);
    }
}

static void e_moving_chevron(void) {
    for (int i = 0; i < NKEYS; i++) {
        int c = led_col[i];
        int r = led_row[i];
        int chev = c + (r < RGB_PRO_ROWS / 2 ? r : RGB_PRO_ROWS - 1 - r);
        uint16_t h = (state.hue + state.phase * RGB_PRO_SPEED_REF + chev * 24) % 360;
        pixels[i] = rgbp_hsb(h, state.sat, state.brt);
    }
}

/* Out-in: distance-from-center already sweeps roughly MAX_DIST*10 degrees
 * across the board -- that's this effect's "ring count", so scale the
 * temporal term down to match a single ring's speed to the reference. */
static void e_cycle_out_in(void) {
    const int rings = 1 + (RGB_PRO_MAX_DIST * 10) / 360;
    const int k = RGB_PRO_SPEED_REF > rings ? RGB_PRO_SPEED_REF / rings : 1;
    for (int i = 0; i < NKEYS; i++) {
        uint16_t h = (state.hue + state.phase * k + dist_center(i) * 10) % 360;
        pixels[i] = rgbp_hsb(h, state.sat, state.brt);
    }
}

static void e_cycle_out_in_dual(void) {
    const int rings = 1 + (RGB_PRO_MAX_DIST * 12) / 360;
    const int k = RGB_PRO_SPEED_REF > rings ? RGB_PRO_SPEED_REF / rings : 1;
    for (int i = 0; i < NKEYS; i++) {
        /* two mirrored halves left/right of center */
        int dc = led_col[i] * 2 - CX2;
        if (dc < 0) dc = -dc;
        uint16_t h = (state.hue + state.phase * k + dc * 12) % 360;
        pixels[i] = rgbp_hsb(h, state.sat, state.brt);
    }
}

static void e_cycle_pinwheel(void) {
    for (int i = 0; i < NKEYS; i++) {
        int dx = led_col[i] * 2 - CX2;
        int dy = led_row[i] * 4 - CY2 * 2;
        uint16_t h = (state.hue + state.phase * RGB_PRO_SPEED_REF + atan2_deg(dx, dy)) % 360;
        pixels[i] = rgbp_hsb(h, state.sat, state.brt);
    }
}

/* Spiral: angle sweeps once per revolution (1 arm), distance adds
 * roughly MAX_DIST*12 degrees of extra winding -- same "arm count"
 * logic as e_band_spiral() above. */
static void e_cycle_spiral(void) {
    const int arms = 1 + (RGB_PRO_MAX_DIST * 12) / 360;
    const int k = RGB_PRO_SPEED_REF > arms ? RGB_PRO_SPEED_REF / arms : 1;
    for (int i = 0; i < NKEYS; i++) {
        int dx = led_col[i] * 2 - CX2;
        int dy = led_row[i] * 4 - CY2 * 2;
        uint16_t h = (state.hue + state.phase * k + atan2_deg(dx, dy) + dist_center(i) * 12) % 360;
        pixels[i] = rgbp_hsb(h, state.sat, state.brt);
    }
}

/* ---- Beacons / pinwheels (value pulses, single hue) ---- */

static void e_dual_beacon(void) {
    for (int i = 0; i < NKEYS; i++) {
        int dx = led_col[i] * 2 - CX2;
        int dy = led_row[i] * 4 - CY2 * 2;
        uint16_t ang = (atan2_deg(dx, dy) + state.phase * RGB_PRO_SPEED_REF) % 360;
        int band = ang < 180 ? ang : 360 - ang;
        uint8_t v = state.brt * band / 180;
        pixels[i] = rgbp_hsb(state.hue, state.sat, v);
    }
}

static void e_rainbow_beacon(void) {
    for (int i = 0; i < NKEYS; i++) {
        int dx = led_col[i] * 2 - CX2;
        int dy = led_row[i] * 4 - CY2 * 2;
        uint16_t ang = (atan2_deg(dx, dy) + state.phase * RGB_PRO_SPEED_REF) % 360;
        pixels[i] = rgbp_hsb((state.hue + ang) % 360, state.sat, state.brt);
    }
}

/* Rainbow pinwheels: angle is doubled (ang*2) to give the pattern 2 arms
 * -> halve the reference speed so each arm moves at the same rate as a
 * single-arm effect. */
static void e_rainbow_pinwheels(void) {
    const int k = RGB_PRO_SPEED_REF / 2 > 0 ? RGB_PRO_SPEED_REF / 2 : 1;
    for (int i = 0; i < NKEYS; i++) {
        int dx = led_col[i] * 2 - CX2;
        int dy = led_row[i] * 4 - CY2 * 2;
        uint16_t ang = (atan2_deg(dx, dy) * 2 + state.phase * k) % 360;
        pixels[i] = rgbp_hsb((state.hue + ang) % 360, state.sat, state.brt);
    }
}

/* ---- Dispatch ---- */

void rgbp_render_base(enum rgb_pro_effect eff) {
    switch (eff) {
    case RGB_PRO_EFF_SOLID:                 e_solid(); break;
    case RGB_PRO_EFF_BREATHING:             e_breathing(); break;
    case RGB_PRO_EFF_BAND_SAT:              e_band_sat(); break;
    case RGB_PRO_EFF_BAND_VAL:              e_band_val(); break;
    case RGB_PRO_EFF_BAND_PINWHEEL_SAT:     e_band_pinwheel(true); break;
    case RGB_PRO_EFF_BAND_PINWHEEL_VAL:     e_band_pinwheel(false); break;
    case RGB_PRO_EFF_BAND_SPIRAL_SAT:       e_band_spiral(true); break;
    case RGB_PRO_EFF_BAND_SPIRAL_VAL:       e_band_spiral(false); break;
    case RGB_PRO_EFF_CYCLE_ALL:             e_cycle_all(); break;
    case RGB_PRO_EFF_CYCLE_LEFT_RIGHT:      e_cycle_left_right(); break;
    case RGB_PRO_EFF_CYCLE_UP_DOWN:         e_cycle_up_down(); break;
    case RGB_PRO_EFF_RAINBOW_MOVING_CHEVRON: e_moving_chevron(); break;
    case RGB_PRO_EFF_CYCLE_OUT_IN:          e_cycle_out_in(); break;
    case RGB_PRO_EFF_CYCLE_OUT_IN_DUAL:     e_cycle_out_in_dual(); break;
    case RGB_PRO_EFF_CYCLE_PINWHEEL:        e_cycle_pinwheel(); break;
    case RGB_PRO_EFF_CYCLE_SPIRAL:          e_cycle_spiral(); break;
    case RGB_PRO_EFF_DUAL_BEACON:           e_dual_beacon(); break;
    case RGB_PRO_EFF_RAINBOW_BEACON:        e_rainbow_beacon(); break;
    case RGB_PRO_EFF_RAINBOW_PINWHEELS:     e_rainbow_pinwheels(); break;
    default:                                  e_solid(); break;
    }
}
