# zmk-vfx-pro-rgb — module build configuration

zephyr_include_directories(include)
zephyr_include_directories(${WEST_TOPDIR}/zmk/app/include)
zephyr_include_directories(${WEST_TOPDIR}/zmk/app/src)

if(CONFIG_RGB_PRO)
  # The board-specific rgb_pro_led_map.h lives in the user's zmk-config.
  # Search the config root first, then any shield directories (legacy layout).
  zephyr_include_directories(${ZMK_CONFIG})
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
