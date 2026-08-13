/*
 * key_led_map.h — Layout macros for per-key LED mapping.
 *
 * This file provides the LED_LAYOUT macro that lets you define your LED
 * wiring visually, just like a ZMK keymap. The actual data (which LED is
 * where) is NOT defined here — you define it in your zmk-config by
 * creating a header that uses these macros and including it via Kconfig.
 *
 * The user's board-specific header must define:
 *   - RGB_PRO_NUM_KEYS     total number of keys / LEDs
 *   - RGB_PRO_MATRIX_ROWS  number of rows in the visual layout
 *   - RGB_PRO_MATRIX_COLS  number of columns in the visual layout
 *   - key_to_led[]         LED chain index per key position
 *   - key_col[]            visual column per key position
 *   - key_row[]            visual row per key position
 *
 * See the README for a complete example.
 */

#pragma once
#include <zephyr/kernel.h>

/*
 * LED_LAYOUT — generic layout macro generator.
 *
 * Call RGB_PRO_LAYOUT_DEFINE(rows, cols) in your board header to generate
 * a LED_LAYOUT macro matching your matrix dimensions. Or define your own
 * LED_LAYOUT macro directly if your bottom row has fewer keys.
 *
 * For standard rectangular matrices, use:
 *   RGB_PRO_LAYOUT_RECT(rows, cols)
 *
 * For non-rectangular matrices (like most keyboards with a shorter bottom
 * row), define LED_LAYOUT manually with the exact number of parameters
 * matching your key count. See the example below.
 */

/*
 * Example for a 6x14 matrix with 79 keys (bottom row has 9):
 *
 * #define LED_LAYOUT(                                                       \
 *     k00,k01,k02,k03,k04,k05,k06,k07,k08,k09,k10,k11,k12,k13,           \
 *     k14,k15,k16,k17,k18,k19,k20,k21,k22,k23,k24,k25,k26,k27,           \
 *     k28,k29,k30,k31,k32,k33,k34,k35,k36,k37,k38,k39,k40,k41,           \
 *     k42,k43,k44,k45,k46,k47,k48,k49,k50,k51,k52,k53,k54,k55,           \
 *     k56,k57,k58,k59,k60,k61,k62,k63,k64,k65,k66,k67,k68,k69,           \
 *     k70,k71,k72,            k73,            k74,k75,k76,k77,k78          \
 * ) { [0]=k00,[1]=k01, ... [78]=k78 }
 *
 * static const uint8_t key_to_led[79] = LED_LAYOUT(
 *     22, 21, 20, 19, ...    // fill with your LED chain indices
 * );
 */
