#pragma once

#include <zephyr/kernel.h>
#include <zmk_vfx_pro_rgb/effects.h>

/* Behavior commands (param1 of &rgb_pro). */
#define RGB_PRO_CMD_TOGGLE   0
#define RGB_PRO_CMD_ON       1
#define RGB_PRO_CMD_OFF      2
#define RGB_PRO_CMD_EFF_NEXT 3
#define RGB_PRO_CMD_EFF_PREV 4
#define RGB_PRO_CMD_HUE_UP   5
#define RGB_PRO_CMD_HUE_DN   6
#define RGB_PRO_CMD_BRT_UP   7
#define RGB_PRO_CMD_BRT_DN   8
#define RGB_PRO_CMD_SPD_UP   9
#define RGB_PRO_CMD_SPD_DN   10
#define RGB_PRO_CMD_RANDOM   11  /* jump to a random effect now */
#define RGB_PRO_CMD_DIR_TOG  12  /* reverse animation direction */
#define RGB_PRO_CMD_DIR_FWD  13
#define RGB_PRO_CMD_DIR_REV  14

int rgb_pro_command(uint8_t cmd, uint8_t param);
