# zmk-pro-rgb module CMakeLists
#
# Include paths:
#   1. Module's own headers (effects.h, rgb_pro.h)
#   2. ZMK app headers (zmk/*.h, drivers/behavior.h)
#   3. User's zmk-config (for the board-specific LED map header)
zephyr_include_directories(include)
zephyr_include_directories(${WEST_TOPDIR}/zmk/app/include)
zephyr_include_directories(${WEST_TOPDIR}/zmk/app/src)

if(CONFIG_RGB_PRO)
  # Add the user's shield directory to the include path so the module
  # can find the board-specific LED mapping header.
  zephyr_include_directories(${ZMK_CONFIG}/boards/shields/flatcat)

  zephyr_library()
  zephyr_library_sources(src/rgb_pro.c)
  zephyr_library_sources(src/effects_base.c)
  zephyr_library_sources(src/effects_animated.c)
  zephyr_library_sources(src/effects_reactive.c)
  zephyr_library_sources(src/effects_extra.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_BEHAVIOR_RGB_PRO src/behavior_rgb_pro.c)
endif()
