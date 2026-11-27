/*
 * Lamp colours from a receiver dongle to the keyboard, over Bluetooth.
 *
 * The dongle exposes HID LampArray on its own USB port (Windows Dynamic
 * Lighting talks to it) and forwards the colours to the keyboard through a
 * small GATT service on the keyboard:
 *
 *   FRAME   write / write without response, encrypted link required
 *           u8 flags | u8 start | u8 count | count x (r, g, b)
 *           Colours for lamps [start, start + count). Lamp ID == LED index in
 *           the chain. With RGBP_LR_FLAG_COMPLETE the keyboard shows the frame
 *           it has assembled so far, so a frame is never shown half-updated.
 *   CONTROL write / write without response, encrypted link required
 *           u8 autonomous (1 = keyboard effects, 0 = colours from the host)
 *
 * Only <stdint.h>/<string.h>: everything here is unit-tested on the host.
 */

#pragma once

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* c0f3e1a0-5a7d-4e2b-9c1f-2b6d8e4a7f1X, little endian as Zephyr expects. Use
 * with BT_UUID_DECLARE_128() / BT_UUID_INIT_128(). */
#define RGBP_LR_UUID_BYTES(last)                                                                   \
    (last), 0x7f, 0x4a, 0x8e, 0x6d, 0x2b, 0x1f, 0x9c, 0x2b, 0x4e, 0x7d, 0x5a, 0xa0, 0xe1, 0xf3,  \
        0xc0
#define RGBP_LR_UUID_SERVICE RGBP_LR_UUID_BYTES(0x10)
#define RGBP_LR_UUID_FRAME   RGBP_LR_UUID_BYTES(0x11)
#define RGBP_LR_UUID_CONTROL RGBP_LR_UUID_BYTES(0x12)

#define RGBP_LR_FLAG_COMPLETE 0x01
#define RGBP_LR_HDR           3
#define RGBP_LR_MAX_LAMPS     255 /* start and count are one byte */

struct rgbp_lr_rgb {
    uint8_t r, g, b;
};

/* Lamps that fit in one write for a given ATT MTU (payload = MTU - 3). */
static inline int rgbp_lr_lamps_per_write(uint16_t mtu) {
    int payload = (int)mtu - 3;
    int n = (payload - RGBP_LR_HDR) / 3;
    return n > 0 ? n : 0;
}

/* ---- keyboard side ---- */

/* Copy one FRAME write into `staging` (n_lamps entries). Returns 0 and sets
 * *complete, or -EINVAL for a malformed or out-of-range write, in which case
 * `staging` is not touched. */
static inline int rgbp_lr_apply(struct rgbp_lr_rgb *staging, int n_lamps, const uint8_t *data,
                                size_t len, bool *complete) {
    if (len < RGBP_LR_HDR) {
        return -EINVAL;
    }
    uint8_t flags = data[0], start = data[1], count = data[2];

    if (len != RGBP_LR_HDR + 3u * count || (int)start + (int)count > n_lamps) {
        return -EINVAL;
    }
    memcpy(&staging[start], &data[RGBP_LR_HDR], 3u * count);
    *complete = (flags & RGBP_LR_FLAG_COMPLETE) != 0;
    return 0;
}

/* ---- dongle side ----
 * The frame is split into fixed chunks of `per` lamps (the last one shorter).
 * Only chunks that differ from what the keyboard already has are sent; the
 * last chunk sent carries RGBP_LR_FLAG_COMPLETE. */

static inline bool rgbp_lr_chunk_dirty(const struct rgbp_lr_rgb *cur,
                                       const struct rgbp_lr_rgb *sent, int n_lamps, int start,
                                       int per) {
    int count = n_lamps - start < per ? n_lamps - start : per;
    return memcmp(&cur[start], &sent[start], 3u * (unsigned)count) != 0;
}

/* First dirty chunk at or after `from`, or -1. `from` is a chunk start. */
static inline int rgbp_lr_next_dirty(const struct rgbp_lr_rgb *cur,
                                     const struct rgbp_lr_rgb *sent, int n_lamps, int from,
                                     int per) {
    if (per <= 0) {
        return -1;
    }
    for (int s = from; s < n_lamps; s += per) {
        if (rgbp_lr_chunk_dirty(cur, sent, n_lamps, s, per)) {
            return s;
        }
    }
    return -1;
}

/* Encode the chunk starting at `start` into `buf`. Returns its length, or 0
 * if it does not fit `cap`. */
static inline size_t rgbp_lr_encode(uint8_t *buf, size_t cap, const struct rgbp_lr_rgb *cur,
                                    int n_lamps, int start, int per, bool complete) {
    int count = n_lamps - start < per ? n_lamps - start : per;
    size_t len = RGBP_LR_HDR + 3u * (unsigned)count;

    if (count <= 0 || len > cap) {
        return 0;
    }
    buf[0] = complete ? RGBP_LR_FLAG_COMPLETE : 0;
    buf[1] = (uint8_t)start;
    buf[2] = (uint8_t)count;
    memcpy(&buf[RGBP_LR_HDR], &cur[start], 3u * (unsigned)count);
    return len;
}
