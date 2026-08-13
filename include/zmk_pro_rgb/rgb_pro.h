#pragma once

#include <zephyr/kernel.h>
#include <zmk_pro_rgb/effects.h>

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

int rgb_pro_command(uint8_t cmd, uint8_t param);
