/*
 * ZMK Pro RGB — Windows Dynamic Lighting through a receiver dongle (keyboard side)
 *
 * The dongle exposes HID LampArray to Windows on its own USB port and forwards
 * the colours here over Bluetooth, through the GATT service declared below
 * (wire format in include/zmk_vfx_pro_rgb/lamp_remote.h).
 *
 * The keyboard shows the host's colours only while the dongle's Bluetooth
 * profile is the active output. Switching to another profile (another PC, a
 * phone) brings the keyboard's own effects back; switching back shows the
 * host's last frame again. When the dongle disconnects, the keyboard forgets
 * the host and returns to autonomous mode, as the USB version does on unplug.
 *
 * Status overlays, on/off and brightness behave as with USB LampArray.
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/spinlock.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#include <zmk/ble.h>
#include <zmk/endpoints.h>

#include <zmk_vfx_pro_rgb/rgb_pro.h>
#include <zmk_vfx_pro_rgb/effects.h>
#include <zmk_vfx_pro_rgb/lamp_remote.h>

LOG_MODULE_DECLARE(rgb_pro, CONFIG_ZMK_LOG_LEVEL);

#define LAMP_COUNT RGB_PRO_KEYS
BUILD_ASSERT(LAMP_COUNT <= RGBP_LR_MAX_LAMPS, "lamp index must fit one byte on the wire");

/* ---- State ---- */
static struct k_spinlock lr_lock;
static struct rgbp_lr_rgb staging[LAMP_COUNT]; /* being assembled from writes */
static struct rgbp_lr_rgb active[LAMP_COUNT];  /* last completed frame        */
static bool autonomous = true;
static bool host_known;
static bt_addr_le_t host_addr; /* the dongle that is writing */

static void note_writer(struct bt_conn *conn) {
    const bt_addr_le_t *dst = bt_conn_get_dst(conn);

    if (!host_known || bt_addr_le_cmp(dst, &host_addr) != 0) {
        bt_addr_le_copy(&host_addr, dst);
        host_known = true;
    }
}

/* ---- GATT writes from the dongle ---- */

static ssize_t write_frame(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                           const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (offset) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }

    bool complete = false;
    k_spinlock_key_t key = k_spin_lock(&lr_lock);
    int err = rgbp_lr_apply(staging, LAMP_COUNT, buf, len, &complete);
    if (!err) {
        note_writer(conn);
        if (complete) {
            memcpy(active, staging, sizeof(active));
        }
    }
    k_spin_unlock(&lr_lock, key);

    if (err) {
        LOG_WRN("lamp frame rejected (%d bytes)", len);
        return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
    }
    return len;
}

static ssize_t write_control(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             const void *buf, uint16_t len, uint16_t offset, uint8_t flags) {
    if (offset) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    if (len != 1) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
    }

    bool now_autonomous = ((const uint8_t *)buf)[0] & 1;

    k_spinlock_key_t key = k_spin_lock(&lr_lock);
    bool changed = (autonomous != now_autonomous);
    autonomous = now_autonomous;
    note_writer(conn);
    k_spin_unlock(&lr_lock, key);

    if (changed) {
        LOG_INF("LampArray (dongle): %s mode", now_autonomous ? "autonomous" : "host");
    }
    return len;
}

/* Declared after ZMK's HID service on purpose: static services are placed in
 * name order, so this one sits after "hog_svc" and the HID attribute handles
 * that hosts have cached stay where they were. */
BT_GATT_SERVICE_DEFINE(rgbp_lamp_svc, BT_GATT_PRIMARY_SERVICE(BT_UUID_DECLARE_128(RGBP_LR_UUID_SERVICE)),
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(RGBP_LR_UUID_FRAME),
                                              BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_frame, NULL),
                       BT_GATT_CHARACTERISTIC(BT_UUID_DECLARE_128(RGBP_LR_UUID_CONTROL),
                                              BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                                              BT_GATT_PERM_WRITE_ENCRYPT, NULL, write_control,
                                              NULL));

/* ---- Dongle gone: forget the host ---- */
static void disconnected(struct bt_conn *conn, uint8_t reason) {
    k_spinlock_key_t key = k_spin_lock(&lr_lock);
    if (host_known && bt_addr_le_cmp(bt_conn_get_dst(conn), &host_addr) == 0) {
        autonomous = true;
        host_known = false;
        memset(staging, 0, sizeof(staging));
        memset(active, 0, sizeof(active));
    }
    k_spin_unlock(&lr_lock, key);
}

BT_CONN_CB_DEFINE(rgbp_lamp_conn_cb) = {
    .disconnected = disconnected,
};

/* ---- Renderer hook (called from the tick in rgb_pro.c) ---- */
bool rgbp_lamp_array_render(void) {
    /* Only while the dongle is where the keystrokes go. */
    if (zmk_endpoint_get_selected().transport != ZMK_TRANSPORT_BLE) {
        return false;
    }
    const bt_addr_le_t *active_profile = zmk_ble_active_profile_addr();

#if IS_ENABLED(CONFIG_RGB_PRO_LAMP_ARRAY_APPLY_BRT)
    const uint8_t scale = (uint8_t)((state.brt * 255) / 100);
#endif

    k_spinlock_key_t key = k_spin_lock(&lr_lock);
    bool host = !autonomous && host_known && active_profile &&
                bt_addr_le_cmp(active_profile, &host_addr) == 0;
    if (host) {
        for (int i = 0; i < LAMP_COUNT; i++) {
            struct led_rgb c = {.r = active[i].r, .g = active[i].g, .b = active[i].b};
#if IS_ENABLED(CONFIG_RGB_PRO_LAMP_ARRAY_APPLY_BRT)
            c = rgbp_scale(c, scale);
#endif
            pixels[i] = c;
        }
    }
    k_spin_unlock(&lr_lock, key);
    return host;
}
