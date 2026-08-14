#define DT_DRV_COMPAT zmk_behavior_rgb_pro

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/behavior.h>
#include <zmk_vfx_pro_rgb/rgb_pro.h>

LOG_MODULE_DECLARE(rgb_pro, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_keymap_binding_convert_central_state_dependent_params(
    struct zmk_behavior_binding *binding, struct zmk_behavior_binding_event event) {
    /* commands are self-contained; nothing to convert */
    return 0;
}

static int on_keymap_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    return rgb_pro_command((uint8_t)binding->param1, (uint8_t)binding->param2);
}

static int on_keymap_binding_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_rgb_pro_driver_api = {
    .binding_convert_central_state_dependent_params =
        on_keymap_binding_convert_central_state_dependent_params,
    .binding_pressed = on_keymap_binding_pressed,
    .binding_released = on_keymap_binding_released,
};

static int behavior_rgb_pro_init(const struct device *dev) {
    ARG_UNUSED(dev);
    return 0;
}

BEHAVIOR_DT_INST_DEFINE(0, behavior_rgb_pro_init, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                        &behavior_rgb_pro_driver_api);

#endif /* DT_HAS_COMPAT_STATUS_OKAY */
