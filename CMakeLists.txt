# zmk-pro-rgb module CMakeLists
#
# Include paths:
#   1. Module's own headers (effects.h, rgb_pro.h)
#   2. ZMK app headers (zmk/*.h, drivers/behavior.h)
#   3. User's zmk-config shield directories (for rgb_pro_led_map.h)

zephyr_include_directories(include)
zephyr_include_directories(${WEST_TOPDIR}/zmk/app/include)
zephyr_include_directories(${WEST_TOPDIR}/zmk/app/src)

if(CONFIG_RGB_PRO)
  # Scan all shield directories under the user's config so the module
  # finds rgb_pro_led_map.h regardless of the shield name.
  file(GLOB shield_dirs "${ZMK_CONFIG}/boards/shields/*/")
  foreach(dir ${shield_dirs})
    zephyr_include_directories(${dir})
  endforeach()

  zephyr_library()
  zephyr_library_sources(src/rgb_pro.c)
  zephyr_library_sources(src/effects_base.c)
  zephyr_library_sources(src/effects_animated.c)
  zephyr_library_sources(src/effects_reactive.c)
  zephyr_library_sources(src/effects_extra.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_BEHAVIOR_RGB_PRO src/behavior_rgb_pro.c)
endif()
