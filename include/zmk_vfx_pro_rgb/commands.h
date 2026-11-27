#pragma once

/* No includes on purpose: this header is shared by the keyboard code and by the
 * dongle's sync code, which is built without the effect engine. */

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

/* Sent by the dongle to the keyboard, never bound to a key. They only store
 * state for the status/battery overlays and must not start or stop the
 * animation. */
#define RGB_PRO_CMD_SYNC_LAYER   15  /* param: highest active layer */
#define RGB_PRO_CMD_SYNC_STATUS  16  /* param: packed output / BLE profile state */

/* Absolute brightness, param = 0-100 %. Bindable to a key
 * (&rgb_pro 17 50 = 50 %) and sent by the receiver dongle
 * when the PC picks a brightness. */
#define RGB_PRO_CMD_BRT_SET      17

/* Packed status (param of RGB_PRO_CMD_SYNC_STATUS):
 *   bit 0    : selected output is USB (otherwise BLE)
 *   bits 1-3 : active BLE profile index (0-7)
 *   bits 4-5 : state of the active BLE profile */
#define RGB_PRO_BT_CONNECTED 0
#define RGB_PRO_BT_OPEN      1  /* no bond: advertising for a new host */
#define RGB_PRO_BT_PAIRED    2  /* bonded but not connected */

#define RGB_PRO_STATUS_PACK(usb, profile, st) \
    ((uint8_t)(((usb) ? 1u : 0u) | (((unsigned)(profile) & 0x7u) << 1) | (((unsigned)(st) & 0x3u) << 4)))
#define RGB_PRO_STATUS_USB(s)     ((s) & 0x1u)
#define RGB_PRO_STATUS_PROFILE(s) (((s) >> 1) & 0x7u)
#define RGB_PRO_STATUS_STATE(s)   (((s) >> 4) & 0x3u)
