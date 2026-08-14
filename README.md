# ZMK Pro RGB

> Module name: `zmk-vfx-pro-rgb` &nbsp;•&nbsp; Repository: `zmk-pro-rgb`

A ZMK module that replaces the stock RGB underglow with a full **per-key
reactive RGB system**, featuring 36 animated effects ported from QMK's
RGB Matrix.

Designed to be **reusable across any ZMK keyboard** with WS2812/SK6812
addressable LEDs. All board-specific configuration (LED wiring, matrix
geometry, caps indicator) is done from your zmk-config — the module
itself is hardware-agnostic.

## Features

- **36 effects** — base patterns, animated, per-key reactive, and custom
- **Per-key reactivity** — splash, cross, heatmap, digital rain
- **Status overlays** — caps lock, BLE/USB output, active profile, battery
- **Runtime controls** — hue, brightness, speed, effect cycling
- **Random on wake** — a fresh effect and colour every time you come back
- **Fully configurable** via Kconfig (no code changes needed)

## Quick Start

### 1. Add the module to your `config/west.yml`

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: jagonmo
      url-base: https://github.com/jagonmo
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: zmk-pro-rgb
      remote: jagonmo
      revision: main
  self:
    path: config
```

### 2. Create your LED mapping

Create `config/rgb_pro_led_map.h` in your zmk-config:

```c
#pragma once
#include <zephyr/kernel.h>

/* Define a layout macro matching your physical key arrangement.
 * Each parameter is a key position (0 to NUM_KEYS-1). */
#define LED_LAYOUT(                                         \
    k00, k01, k02, k03, k04,                               \
    k05, k06, k07, k08, k09                                \
) { [0]=k00,[1]=k01,[2]=k02,[3]=k03,[4]=k04,               \
    [5]=k05,[6]=k06,[7]=k07,[8]=k08,[9]=k09 }

/* Place the WS2812 chain index at each key's physical position. */
const uint8_t key_to_led[] = LED_LAYOUT(
    0,  1,  2,  3,  4,       /* row 0: LED chain order */
    9,  8,  7,  6,  5        /* row 1: reversed (serpentine) */
);

/* Visual column (for horizontal effects). */
const uint8_t key_col[] = LED_LAYOUT(
    0,  1,  2,  3,  4,
    0,  1,  2,  3,  4
);

/* Visual row (for vertical effects). */
const uint8_t key_row[] = LED_LAYOUT(
    0,  0,  0,  0,  0,
    1,  1,  1,  1,  1
);
```

### 3. Configure in your `.conf`

```ini
CONFIG_ZMK_RGB_UNDERGLOW=n
CONFIG_RGB_PRO=y

# Must match your LED map
CONFIG_RGB_PRO_NUM_KEYS=10
CONFIG_RGB_PRO_MATRIX_ROWS=2
CONFIG_RGB_PRO_MATRIX_COLS=5

# Startup defaults
CONFIG_RGB_PRO_START_ON=y
CONFIG_RGB_PRO_BRT_START=40

# Overlays (optional) — key positions are in KEYMAP ORDER
CONFIG_RGB_PRO_CAPS_INDICATOR=y
CONFIG_RGB_PRO_CAPS_KEY=42

CONFIG_RGB_PRO_STATUS_OVERLAY=y
CONFIG_RGB_PRO_STATUS_LAYER=1
CONFIG_RGB_PRO_STATUS_OUT_KEY=14
CONFIG_RGB_PRO_STATUS_BT_KEY_BASE=15

CONFIG_RGB_PRO_BATTERY_INDICATOR=y
CONFIG_RGB_PRO_BATTERY_LAYER=1
CONFIG_RGB_PRO_BATTERY_KEY_START=0
CONFIG_RGB_PRO_BATTERY_KEY_COUNT=4

CONFIG_LED_STRIP=y
CONFIG_ZMK_HID_INDICATORS=y
```

### 4. Add the behavior to your overlay

```dts
behaviors {
    rgb_pro: behavior_rgb_pro {
        compatible = "zmk,behavior-rgb-pro";
        #binding-cells = <2>;
    };
};
```

### 5. Use in your keymap

```c
#define RGBP_TOG 0   /* toggle on/off */
#define RGBP_EFF 3   /* next effect */
#define RGBP_EFR 4   /* prev effect */
#define RGBP_HUI 5   /* hue up */
#define RGBP_HUD 6   /* hue down */
#define RGBP_BRI 7   /* brightness up */
#define RGBP_BRD 8   /* brightness down */
#define RGBP_SPI 9   /* speed up */
#define RGBP_SPD 10  /* speed down */

&rgb_pro RGBP_TOG 0   /* toggle RGB */
&rgb_pro RGBP_EFF 0   /* next effect */
```

### 6. Power regulator (if needed)

If your LEDs are powered via a GPIO-controlled regulator, add this to
your overlay with the label `rgb_pro_pwr`:

```dts
rgb_pro_pwr: rgb_pro_pwr {
    compatible = "regulator-fixed";
    regulator-name = "rgb_pro_pwr";
    enable-gpios = <&gpio0 YOUR_PIN GPIO_ACTIVE_LOW>;
    regulator-boot-on;
};
```

## Effects

| #  | Name                    | Type     |
|----|-------------------------|----------|
| 1  | Solid                   | Base     |
| 2  | Breathing               | Base     |
| 3  | Band Saturation         | Base     |
| 4  | Band Value              | Base     |
| 5  | Band Pinwheel Sat       | Base     |
| 6  | Band Pinwheel Val       | Base     |
| 7  | Band Spiral Sat         | Base     |
| 8  | Band Spiral Val         | Base     |
| 9  | Cycle All               | Base     |
| 10 | Cycle Left-Right        | Base     |
| 11 | Cycle Up-Down           | Base     |
| 12 | Rainbow Moving Chevron  | Base     |
| 13 | Cycle Out-In            | Base     |
| 14 | Cycle Out-In Dual       | Base     |
| 15 | Cycle Pinwheel          | Base     |
| 16 | Cycle Spiral            | Base     |
| 17 | Dual Beacon             | Base     |
| 18 | Rainbow Beacon          | Base     |
| 19 | Rainbow Pinwheels       | Base     |
| 20 | Raindrops               | Animated |
| 21 | Jellybean Raindrops     | Animated |
| 22 | Hue Breathing           | Animated |
| 23 | Hue Pendulum            | Animated |
| 24 | Hue Wave                | Animated |
| 25 | Pixel Fractal           | Animated |
| 26 | Pixel Flow              | Animated |
| 27 | Pixel Rain              | Animated |
| 28 | Typing Heatmap          | Reactive |
| 29 | Digital Rain            | Reactive |
| 30 | Solid Reactive Simple   | Reactive |
| 31 | Solid Reactive          | Reactive |
| 32 | Solid Reactive Cross    | Reactive |
| 33 | Splash                  | Reactive |
| 34 | Flag                    | Extra    |
| 35 | Layer Color             | Extra    |
| 36 | Complement              | Extra    |

## Random on wake

With `CONFIG_RGB_PRO_RANDOM_ON_WAKE=y`, the keyboard picks a new effect from
the list every time it wakes from idle.

Effects that paint from a single base colour (solid, breathing, the band and
reactive families, ...) also get a random hue. Rainbow-style effects (the
cycle family, flag, hue wave, ...) build their own palette, so only the
effect changes for those.

## Overlays

Overlays paint on top of whichever effect is running. All positions are given
as **key positions in keymap order**, not raw LED indices — the module maps
them through your `key_to_led[]` table.

Overlays always draw at full brightness and are **not** affected by the
brightness controls (`RGBP_BRI` / `RGBP_BRD`), so status information stays
readable even with the effects dimmed all the way down.

### Caps lock
Blinks one key while caps lock is on. Colour and blink rate are configurable.
Needs `CONFIG_ZMK_HID_INDICATORS=y`.

### Output / Bluetooth status
Shown while a chosen layer is held:

| Key | State | Colour |
|-----|-------|--------|
| Output toggle | BLE selected | Solid blue |
| Output toggle | USB selected | Solid red |
| Active profile | Connected | Solid blue |
| Active profile | Paired, disconnected | Blinking blue |
| Active profile | Not paired | Blinking green |
| Other profiles | — | Dim white |

> Non-active profiles show dim because ZMK keeps its profile table private to
> `app/src/ble.c`; only the active profile's state is reachable from a module.

### Battery
Shown while a chosen layer is held. Fills a run of keys as a bar and sweeps
the colour from red (empty) to green (full).
Needs `CONFIG_ZMK_BATTERY_REPORTING=y` and a `zmk,battery` chosen node.

### Required options

| Overlay  | Also needs                        |
|----------|-----------------------------------|
| Caps     | `CONFIG_ZMK_HID_INDICATORS=y`     |
| Status   | `CONFIG_ZMK_BLE=y`                |
| Battery  | `CONFIG_ZMK_BATTERY_REPORTING=y`  |

## Kconfig Reference

| Option                          | Default | Description                       |
|---------------------------------|---------|-----------------------------------|
| `CONFIG_RGB_PRO`                | n       | Enable the module                 |
| `CONFIG_RGB_PRO_NUM_KEYS`       | 0       | Total number of LEDs              |
| `CONFIG_RGB_PRO_MATRIX_ROWS`    | 1       | Rows in the visual layout         |
| `CONFIG_RGB_PRO_MATRIX_COLS`    | 1       | Columns in the visual layout      |
| `CONFIG_RGB_PRO_TICK_MS`        | 33      | Frame interval (ms)               |
| `CONFIG_RGB_PRO_BRT_START`      | 40      | Startup brightness (0-100)        |
| `CONFIG_RGB_PRO_BRT_STEP`       | 10      | Brightness step per press         |
| `CONFIG_RGB_PRO_HUE_STEP`       | 15      | Hue step (degrees)                |
| `CONFIG_RGB_PRO_REACTIVE_DECAY` | 12      | Reactive fade speed               |
| `CONFIG_RGB_PRO_START_ON`       | y       | Start with RGB on                 |
| `CONFIG_RGB_PRO_AUTO_OFF_IDLE`  | y       | Turn off when idle                |
| `CONFIG_RGB_PRO_RANDOM_ON_WAKE` | n       | Random effect after each idle wake |
| `CONFIG_RGB_PRO_CAPS_INDICATOR` | n       | Enable caps-lock overlay          |
| `CONFIG_RGB_PRO_CAPS_KEY`       | 0       | Key position of the caps key      |
| `CONFIG_RGB_PRO_CAPS_R/G/B`     | 0/255/0 | Caps indicator colour             |
| `CONFIG_RGB_PRO_CAPS_BLINK_MS`  | 1000    | Caps blink cycle (ms)             |
| `CONFIG_RGB_PRO_STATUS_OVERLAY` | n       | Enable output/BT status overlay   |
| `CONFIG_RGB_PRO_STATUS_LAYER`   | 1       | Layer that shows the status       |
| `CONFIG_RGB_PRO_STATUS_OUT_KEY` | 0       | Key position of the output toggle |
| `CONFIG_RGB_PRO_STATUS_BT_KEY_BASE` | 0   | Key position of BT profile 0      |
| `CONFIG_RGB_PRO_STATUS_BT_COUNT` | 5      | Number of BT profile keys         |
| `CONFIG_RGB_PRO_STATUS_BLINK_MS` | 600    | Status blink cycle (ms)           |
| `CONFIG_RGB_PRO_BATTERY_INDICATOR` | n    | Enable battery bar                |
| `CONFIG_RGB_PRO_BATTERY_LAYER`  | 1       | Layer that shows the battery bar  |
| `CONFIG_RGB_PRO_BATTERY_KEY_START` | 0    | First key of the bar              |
| `CONFIG_RGB_PRO_BATTERY_KEY_COUNT` | 4    | Number of keys in the bar         |

## License

MIT
