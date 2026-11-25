#pragma once

#include <zephyr/kernel.h>
#include <zmk_vfx_pro_rgb/effects.h>

#include <zmk_vfx_pro_rgb/commands.h>

int rgb_pro_command(uint8_t cmd, uint8_t param);
