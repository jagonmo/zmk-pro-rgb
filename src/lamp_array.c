/*
 * ZMK Pro RGB — HID LampArray (Windows 11 Dynamic Lighting), USB only
 *
 * Exposes the per-key LEDs to Windows through the open HID LampArray
 * standard (HID Usage Tables 1.4, page 0x59) so that Settings ->
 * Personalization -> Dynamic Lighting can drive them without any vendor
 * software.
 *
 * Design
 *   - LampArray lives on its OWN USB HID interface (Zephyr instance "HID_1"),
 *     exactly like Microsoft's ArduinoHidForWindows sample. ZMK's keyboard
 *     descriptor, its BLE HID-over-GATT service and the bonds are untouched,
 *     and no change to ZMK itself is needed.
 *   - Two modes, as in the standard. "Autonomous" (power-up default): the
 *     normal rgb_pro effects run. "Host": Windows paints every lamp and
 *     rgbp_lamp_array_render() copies its last completed frame into pixels[].
 *   - Status overlays (caps lock, BLE/USB, battery) are still drawn on top of
 *     the host frame, and the keyboard's own on/off and brightness apply.
 *   - Lamp ID == physical LED index in the chain (the same index effects use).
 *
 * Report layout and the report descriptor below come from Microsoft's
 * ArduinoHidForWindows / RP2040MacropadHidSample (MIT licence,
 * Copyright (c) Microsoft Corporation), generated with Waratah from HID
 * Usage Tables 1.4.0. They are used verbatim; this file only adds the ZMK/
 * Zephyr glue.
 *
 * USB only: Bluetooth would need the same reports as extra GATT
 * characteristics in the HID service, which this module does not do.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/init.h>
#include <zephyr/spinlock.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>

#include <zmk/event_manager.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/usb.h>

#include <zmk_vfx_pro_rgb/rgb_pro.h>
#include <zmk_vfx_pro_rgb/effects.h>

LOG_MODULE_DECLARE(rgb_pro, CONFIG_ZMK_LOG_LEVEL);

/* A second HID instance is required: HID_0 is ZMK's keyboard. */
BUILD_ASSERT(CONFIG_USB_HID_DEVICE_COUNT >= 2,
             "RGB_PRO_LAMP_ARRAY needs CONFIG_USB_HID_DEVICE_COUNT=2 (HID_0 is the keyboard)");

#define LAMP_COUNT RGB_PRO_KEYS

/* ---- Report IDs (fixed by the descriptor below) ---- */
#define LA_ID_ARRAY_ATTRIBUTES 1 /* Feature, get  */
#define LA_ID_ATTR_REQUEST     2 /* Feature, set  */
#define LA_ID_ATTR_RESPONSE    3 /* Feature, get  */
#define LA_ID_MULTI_UPDATE     4 /* Feature, set  */
#define LA_ID_RANGE_UPDATE     5 /* Feature, set  */
#define LA_ID_CONTROL          6 /* Feature, set  */

#define LA_MULTI_MAX 8

#define LA_FLAG_UPDATE_COMPLETE BIT(0)

#define LA_KIND_KEYBOARD     1
#define LA_PURPOSE_CONTROL   0x01
#define LA_LEVELS_PER_COLOR  255
#define LA_LEVELS_INTENSITY  1
#define LA_IS_PROGRAMMABLE   1

/* The standard wants micrometres and microseconds on the wire. */
#define LA_PITCH_UM     CONFIG_RGB_PRO_LAMP_ARRAY_PITCH_UM
#define LA_DEPTH_UM     10000 /* 1 cm; lamps sit at z = 0 */
#define LA_UPDATE_US    (CONFIG_RGB_PRO_TICK_MS * 1000)

/* ---- Report structs. Each begins with the report ID, which Zephyr includes
 * in the data stage (same convention as ZMK's own HID report structs). ---- */
struct la_color {
    uint8_t r, g, b, intensity;
} __packed;

struct la_array_attributes_report {
    uint8_t report_id;
    uint16_t lamp_count;
    uint32_t width_um;
    uint32_t height_um;
    uint32_t depth_um;
    uint32_t kind;
    uint32_t min_update_us;
} __packed;

struct la_attr_request_report {
    uint8_t report_id;
    uint16_t lamp_id;
} __packed;

struct la_attr_response_report {
    uint8_t report_id;
    uint16_t lamp_id;
    uint32_t x_um;
    uint32_t y_um;
    uint32_t z_um;
    uint32_t update_latency_us;
    uint32_t purposes;
    uint8_t red_levels;
    uint8_t green_levels;
    uint8_t blue_levels;
    uint8_t intensity_levels;
    uint8_t is_programmable;
    uint8_t input_binding;
} __packed;

struct la_multi_update_report {
    uint8_t report_id;
    uint8_t lamp_count;
    uint8_t flags;
    uint16_t lamp_ids[LA_MULTI_MAX];
    struct la_color colors[LA_MULTI_MAX];
} __packed;

struct la_range_update_report {
    uint8_t report_id;
    uint8_t flags;
    uint16_t lamp_id_start;
    uint16_t lamp_id_end;
    struct la_color color;
} __packed;

struct la_control_report {
    uint8_t report_id;
    uint8_t autonomous;
} __packed;

/* Sizes from the descriptor's own table (ReportSizeInBytes + 1 for the ID). */
BUILD_ASSERT(sizeof(struct la_array_attributes_report) == 23, "report 1 size");
BUILD_ASSERT(sizeof(struct la_attr_request_report) == 3, "report 2 size");
BUILD_ASSERT(sizeof(struct la_attr_response_report) == 29, "report 3 size");
BUILD_ASSERT(sizeof(struct la_multi_update_report) == 51, "report 4 size");
BUILD_ASSERT(sizeof(struct la_range_update_report) == 10, "report 5 size");
BUILD_ASSERT(sizeof(struct la_control_report) == 2, "report 6 size");

/* ---- HID report descriptor (Microsoft, HID Usage Tables 1.4.0, 292 bytes) ---- */
static const uint8_t lamp_array_report_desc[] = {
    0x05, 0x59,                      // UsagePage(Lighting And Illumination[0x0059])
    0x09, 0x01,                      // UsageId(LampArray[0x0001])
    0xA1, 0x01,                      // Collection(Application)
    0x85, 0x01,                      //     ReportId(1)
    0x09, 0x02,                      //     UsageId(LampArrayAttributesReport[0x0002])
    0xA1, 0x02,                      //     Collection(Logical)
    0x09, 0x03,                      //         UsageId(LampCount[0x0003])
    0x15, 0x00,                      //         LogicalMinimum(0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,    //         LogicalMaximum(65,535)
    0x95, 0x01,                      //         ReportCount(1)
    0x75, 0x10,                      //         ReportSize(16)
    0xB1, 0x03,                      //         Feature(Constant, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0x09, 0x04,                      //         UsageId(BoundingBoxWidthInMicrometers[0x0004])
    0x09, 0x05,                      //         UsageId(BoundingBoxHeightInMicrometers[0x0005])
    0x09, 0x06,                      //         UsageId(BoundingBoxDepthInMicrometers[0x0006])
    0x09, 0x07,                      //         UsageId(LampArrayKind[0x0007])
    0x09, 0x08,                      //         UsageId(MinUpdateIntervalInMicroseconds[0x0008])
    0x27, 0xFF, 0xFF, 0xFF, 0x7F,    //         LogicalMaximum(2,147,483,647)
    0x95, 0x05,                      //         ReportCount(5)
    0x75, 0x20,                      //         ReportSize(32)
    0xB1, 0x03,                      //         Feature(Constant, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0xC0,                            //     EndCollection()
    0x85, 0x02,                      //     ReportId(2)
    0x09, 0x20,                      //     UsageId(LampAttributesRequestReport[0x0020])
    0xA1, 0x02,                      //     Collection(Logical)
    0x09, 0x21,                      //         UsageId(LampId[0x0021])
    0x27, 0xFF, 0xFF, 0x00, 0x00,    //         LogicalMaximum(65,535)
    0x95, 0x01,                      //         ReportCount(1)
    0x75, 0x10,                      //         ReportSize(16)
    0xB1, 0x02,                      //         Feature(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0xC0,                            //     EndCollection()
    0x85, 0x03,                      //     ReportId(3)
    0x09, 0x22,                      //     UsageId(LampAttributesResponseReport[0x0022])
    0xA1, 0x02,                      //     Collection(Logical)
    0x09, 0x21,                      //         UsageId(LampId[0x0021])
    0xB1, 0x02,                      //         Feature(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0x09, 0x23,                      //         UsageId(PositionXInMicrometers[0x0023])
    0x09, 0x24,                      //         UsageId(PositionYInMicrometers[0x0024])
    0x09, 0x25,                      //         UsageId(PositionZInMicrometers[0x0025])
    0x09, 0x27,                      //         UsageId(UpdateLatencyInMicroseconds[0x0027])
    0x09, 0x26,                      //         UsageId(LampPurposes[0x0026])
    0x27, 0xFF, 0xFF, 0xFF, 0x7F,    //         LogicalMaximum(2,147,483,647)
    0x95, 0x05,                      //         ReportCount(5)
    0x75, 0x20,                      //         ReportSize(32)
    0xB1, 0x02,                      //         Feature(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0x09, 0x28,                      //         UsageId(RedLevelCount[0x0028])
    0x09, 0x29,                      //         UsageId(GreenLevelCount[0x0029])
    0x09, 0x2A,                      //         UsageId(BlueLevelCount[0x002A])
    0x09, 0x2B,                      //         UsageId(IntensityLevelCount[0x002B])
    0x09, 0x2C,                      //         UsageId(IsProgrammable[0x002C])
    0x09, 0x2D,                      //         UsageId(InputBinding[0x002D])
    0x26, 0xFF, 0x00,                //         LogicalMaximum(255)
    0x95, 0x06,                      //         ReportCount(6)
    0x75, 0x08,                      //         ReportSize(8)
    0xB1, 0x02,                      //         Feature(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0xC0,                            //     EndCollection()
    0x85, 0x04,                      //     ReportId(4)
    0x09, 0x50,                      //     UsageId(LampMultiUpdateReport[0x0050])
    0xA1, 0x02,                      //     Collection(Logical)
    0x09, 0x03,                      //         UsageId(LampCount[0x0003])
    0x25, 0x08,                      //         LogicalMaximum(8)
    0x95, 0x01,                      //         ReportCount(1)
    0xB1, 0x02,                      //         Feature(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0x09, 0x55,                      //         UsageId(LampUpdateFlags[0x0055])
    0x25, 0x01,                      //         LogicalMaximum(1)
    0xB1, 0x02,                      //         Feature(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0x09, 0x21,                      //         UsageId(LampId[0x0021])
    0x27, 0xFF, 0xFF, 0x00, 0x00,    //         LogicalMaximum(65,535)
    0x95, 0x08,                      //         ReportCount(8)
    0x75, 0x10,                      //         ReportSize(16)
    0xB1, 0x02,                      //         Feature(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0x09, 0x51,                      //         UsageId(RedUpdateChannel[0x0051])
    0x09, 0x52,                      //         UsageId(GreenUpdateChannel[0x0052])
    0x09, 0x53,                      //         UsageId(BlueUpdateChannel[0x0053])
    0x09, 0x54,                      //         UsageId(IntensityUpdateChannel[0x0054])
    0x09, 0x51,                      //         UsageId(RedUpdateChannel[0x0051])
    0x09, 0x52,                      //         UsageId(GreenUpdateChannel[0x0052])
    0x09, 0x53,                      //         UsageId(BlueUpdateChannel[0x0053])
    0x09, 0x54,                      //         UsageId(IntensityUpdateChannel[0x0054])
    0x09, 0x51,                      //         UsageId(RedUpdateChannel[0x0051])
    0x09, 0x52,                      //         UsageId(GreenUpdateChannel[0x0052])
    0x09, 0x53,                      //         UsageId(BlueUpdateChannel[0x0053])
    0x09, 0x54,                      //         UsageId(IntensityUpdateChannel[0x0054])
    0x09, 0x51,                      //         UsageId(RedUpdateChannel[0x0051])
    0x09, 0x52,                      //         UsageId(GreenUpdateChannel[0x0052])
    0x09, 0x53,                      //         UsageId(BlueUpdateChannel[0x0053])
    0x09, 0x54,                      //         UsageId(IntensityUpdateChannel[0x0054])
    0x09, 0x51,                      //         UsageId(RedUpdateChannel[0x0051])
    0x09, 0x52,                      //         UsageId(GreenUpdateChannel[0x0052])
    0x09, 0x53,                      //         UsageId(BlueUpdateChannel[0x0053])
    0x09, 0x54,                      //         UsageId(IntensityUpdateChannel[0x0054])
    0x09, 0x51,                      //         UsageId(RedUpdateChannel[0x0051])
    0x09, 0x52,                      //         UsageId(GreenUpdateChannel[0x0052])
    0x09, 0x53,                      //         UsageId(BlueUpdateChannel[0x0053])
    0x09, 0x54,                      //         UsageId(IntensityUpdateChannel[0x0054])
    0x09, 0x51,                      //         UsageId(RedUpdateChannel[0x0051])
    0x09, 0x52,                      //         UsageId(GreenUpdateChannel[0x0052])
    0x09, 0x53,                      //         UsageId(BlueUpdateChannel[0x0053])
    0x09, 0x54,                      //         UsageId(IntensityUpdateChannel[0x0054])
    0x09, 0x51,                      //         UsageId(RedUpdateChannel[0x0051])
    0x09, 0x52,                      //         UsageId(GreenUpdateChannel[0x0052])
    0x09, 0x53,                      //         UsageId(BlueUpdateChannel[0x0053])
    0x09, 0x54,                      //         UsageId(IntensityUpdateChannel[0x0054])
    0x26, 0xFF, 0x00,                //         LogicalMaximum(255)
    0x95, 0x20,                      //         ReportCount(32)
    0x75, 0x08,                      //         ReportSize(8)
    0xB1, 0x02,                      //         Feature(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0xC0,                            //     EndCollection()
    0x85, 0x05,                      //     ReportId(5)
    0x09, 0x60,                      //     UsageId(LampRangeUpdateReport[0x0060])
    0xA1, 0x02,                      //     Collection(Logical)
    0x09, 0x55,                      //         UsageId(LampUpdateFlags[0x0055])
    0x25, 0x01,                      //         LogicalMaximum(1)
    0x95, 0x01,                      //         ReportCount(1)
    0xB1, 0x02,                      //         Feature(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0x09, 0x61,                      //         UsageId(LampIdStart[0x0061])
    0x09, 0x62,                      //         UsageId(LampIdEnd[0x0062])
    0x27, 0xFF, 0xFF, 0x00, 0x00,    //         LogicalMaximum(65,535)
    0x95, 0x02,                      //         ReportCount(2)
    0x75, 0x10,                      //         ReportSize(16)
    0xB1, 0x02,                      //         Feature(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0x09, 0x51,                      //         UsageId(RedUpdateChannel[0x0051])
    0x09, 0x52,                      //         UsageId(GreenUpdateChannel[0x0052])
    0x09, 0x53,                      //         UsageId(BlueUpdateChannel[0x0053])
    0x09, 0x54,                      //         UsageId(IntensityUpdateChannel[0x0054])
    0x26, 0xFF, 0x00,                //         LogicalMaximum(255)
    0x95, 0x04,                      //         ReportCount(4)
    0x75, 0x08,                      //         ReportSize(8)
    0xB1, 0x02,                      //         Feature(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0xC0,                            //     EndCollection()
    0x85, 0x06,                      //     ReportId(6)
    0x09, 0x70,                      //     UsageId(LampArrayControlReport[0x0070])
    0xA1, 0x02,                      //     Collection(Logical)
    0x09, 0x71,                      //         UsageId(AutonomousMode[0x0071])
    0x25, 0x01,                      //         LogicalMaximum(1)
    0x95, 0x01,                      //         ReportCount(1)
    0xB1, 0x02,                      //         Feature(Data, Variable, Absolute, NoWrap, Linear, PreferredState, NoNullPosition, NonVolatile, BitField)
    0xC0,                            //     EndCollection()
    0xC0,                            // EndCollection()
};
BUILD_ASSERT(sizeof(lamp_array_report_desc) == 292, "LampArray descriptor size");

/* ---- State ---- */
static struct k_spinlock la_lock;
static struct la_color staging[LAMP_COUNT]; /* being assembled by the host   */
static struct la_color active[LAMP_COUNT];  /* last frame the host completed */
static bool autonomous = true;              /* power-up default per the spec */
static uint16_t req_lamp_id;                /* lamp the next response describes */

static const struct device *hid_dev;

/* ---- Feature report handlers ---- */

static int get_array_attributes(int32_t *len, uint8_t **data) {
    static struct la_array_attributes_report r;

    r = (struct la_array_attributes_report){
        .report_id = LA_ID_ARRAY_ATTRIBUTES,
        .lamp_count = LAMP_COUNT,
        .width_um = RGB_PRO_COLS * LA_PITCH_UM,
        .height_um = RGB_PRO_ROWS * LA_PITCH_UM,
        .depth_um = LA_DEPTH_UM,
        .kind = LA_KIND_KEYBOARD,
        .min_update_us = LA_UPDATE_US,
    };
    *data = (uint8_t *)&r;
    *len = sizeof(r);
    return 0;
}

static int get_attr_response(int32_t *len, uint8_t **data) {
    static struct la_attr_response_report r;
    uint16_t id = req_lamp_id < LAMP_COUNT ? req_lamp_id : 0;

    r = (struct la_attr_response_report){
        .report_id = LA_ID_ATTR_RESPONSE,
        .lamp_id = id,
        /* Centre of the key cell, measured from the array's top-left corner. */
        .x_um = led_col[id] * LA_PITCH_UM + LA_PITCH_UM / 2,
        .y_um = led_row[id] * LA_PITCH_UM + LA_PITCH_UM / 2,
        .z_um = 0,
        .update_latency_us = LA_UPDATE_US,
        .purposes = LA_PURPOSE_CONTROL,
        .red_levels = LA_LEVELS_PER_COLOR,
        .green_levels = LA_LEVELS_PER_COLOR,
        .blue_levels = LA_LEVELS_PER_COLOR,
        .intensity_levels = LA_LEVELS_INTENSITY,
        .is_programmable = LA_IS_PROGRAMMABLE,
        .input_binding = 0, /* lamp -> key usage mapping not provided */
    };

    /* The response advances to the next lamp after every read and wraps,
     * so the host can walk the whole array with repeated GetFeature calls. */
    req_lamp_id = (id + 1 < LAMP_COUNT) ? id + 1 : 0;

    *data = (uint8_t *)&r;
    *len = sizeof(r);
    return 0;
}

static int set_attr_request(const uint8_t *data, int32_t len) {
    if (len < (int32_t)sizeof(struct la_attr_request_report)) {
        return -EINVAL;
    }
    const struct la_attr_request_report *r = (const void *)data;

    /* Out of range selects lamp 0, as in Microsoft's reference firmware. */
    req_lamp_id = r->lamp_id < LAMP_COUNT ? r->lamp_id : 0;
    return 0;
}

static int set_multi_update(const uint8_t *data, int32_t len) {
    if (len < (int32_t)sizeof(struct la_multi_update_report)) {
        return -EINVAL;
    }
    const struct la_multi_update_report *r = (const void *)data;
    uint8_t n = MIN(r->lamp_count, LA_MULTI_MAX);

    k_spinlock_key_t key = k_spin_lock(&la_lock);
    for (uint8_t i = 0; i < n; i++) {
        uint16_t id = r->lamp_ids[i];
        if (id < LAMP_COUNT) {
            staging[id] = r->colors[i];
        }
    }
    /* Publish only when the host says the batch is complete, so a frame
     * is never shown half-updated. */
    if (r->flags & LA_FLAG_UPDATE_COMPLETE) {
        memcpy(active, staging, sizeof(active));
    }
    k_spin_unlock(&la_lock, key);
    return 0;
}

static int set_range_update(const uint8_t *data, int32_t len) {
    if (len < (int32_t)sizeof(struct la_range_update_report)) {
        return -EINVAL;
    }
    const struct la_range_update_report *r = (const void *)data;
    uint16_t start = r->lamp_id_start, end = r->lamp_id_end;

    if (start > end || end >= LAMP_COUNT) {
        return 0; /* ignored, per the reference firmware */
    }

    k_spinlock_key_t key = k_spin_lock(&la_lock);
    for (uint16_t id = start; id <= end; id++) {
        staging[id] = r->color;
    }
    if (r->flags & LA_FLAG_UPDATE_COMPLETE) {
        memcpy(active, staging, sizeof(active));
    }
    k_spin_unlock(&la_lock, key);
    return 0;
}

static int set_control(const uint8_t *data, int32_t len) {
    if (len < (int32_t)sizeof(struct la_control_report)) {
        return -EINVAL;
    }
    const struct la_control_report *r = (const void *)data;
    bool now_autonomous = r->autonomous & 1;

    k_spinlock_key_t key = k_spin_lock(&la_lock);
    bool changed = (autonomous != now_autonomous);
    autonomous = now_autonomous;
    k_spin_unlock(&la_lock, key);

    if (changed) {
        LOG_INF("LampArray: %s mode", now_autonomous ? "autonomous" : "host");
    }
    return 0;
}

/* ---- Zephyr legacy USB HID glue ---- */

#define HID_REPORT_TYPE_FEATURE 0x300
#define HID_TYPE_MASK 0xff00
#define HID_ID_MASK 0x00ff

static int get_report_cb(const struct device *dev, struct usb_setup_packet *setup, int32_t *len,
                         uint8_t **data) {
    ARG_UNUSED(dev);

    if ((setup->wValue & HID_TYPE_MASK) != HID_REPORT_TYPE_FEATURE) {
        return -ENOTSUP;
    }

    switch (setup->wValue & HID_ID_MASK) {
    case LA_ID_ARRAY_ATTRIBUTES:
        return get_array_attributes(len, data);
    case LA_ID_ATTR_RESPONSE:
        return get_attr_response(len, data);
    default:
        return -ENOTSUP;
    }
}

static int set_report_cb(const struct device *dev, struct usb_setup_packet *setup, int32_t *len,
                         uint8_t **data) {
    ARG_UNUSED(dev);

    if ((setup->wValue & HID_TYPE_MASK) != HID_REPORT_TYPE_FEATURE) {
        return -ENOTSUP;
    }

    switch (setup->wValue & HID_ID_MASK) {
    case LA_ID_ATTR_REQUEST:
        return set_attr_request(*data, *len);
    case LA_ID_MULTI_UPDATE:
        return set_multi_update(*data, *len);
    case LA_ID_RANGE_UPDATE:
        return set_range_update(*data, *len);
    case LA_ID_CONTROL:
        return set_control(*data, *len);
    default:
        return -ENOTSUP;
    }
}

static const struct hid_ops lamp_array_ops = {
    .get_report = get_report_cb,
    .set_report = set_report_cb,
};

/* Must run before usb_enable() (ZMK_USB_INIT_PRIORITY), like ZMK's own HID init. */
static int rgbp_lamp_array_init(void) {
    hid_dev = device_get_binding("HID_1");
    if (hid_dev == NULL) {
        LOG_ERR("LampArray: HID_1 not found (is CONFIG_USB_HID_DEVICE_COUNT=2?)");
        return -EINVAL;
    }

    usb_hid_register_device(hid_dev, lamp_array_report_desc, sizeof(lamp_array_report_desc),
                            &lamp_array_ops);
    usb_hid_init(hid_dev);
    return 0;
}
SYS_INIT(rgbp_lamp_array_init, APPLICATION, CONFIG_ZMK_USB_HID_INIT_PRIORITY);

/* ---- Cable removed: forget the host ----
 * Windows keeps no state on the keyboard once unplugged, and a battery-powered
 * board does not reboot, so without this it would stay frozen on the last
 * Windows frame. A re-plug re-enumerates and Windows takes control again. */
static int lamp_usb_listener(const zmk_event_t *eh) {
    const struct zmk_usb_conn_state_changed *ev = as_zmk_usb_conn_state_changed(eh);
    if (ev == NULL || ev->conn_state != ZMK_USB_CONN_NONE) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    k_spinlock_key_t key = k_spin_lock(&la_lock);
    autonomous = true;
    req_lamp_id = 0;
    memset(staging, 0, sizeof(staging));
    memset(active, 0, sizeof(active));
    k_spin_unlock(&la_lock, key);
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(rgbp_lamp_usb, lamp_usb_listener);
ZMK_SUBSCRIPTION(rgbp_lamp_usb, zmk_usb_conn_state_changed);

/* ---- Renderer hook (called from the tick in rgb_pro.c) ---- */
bool rgbp_lamp_array_render(void) {
#if IS_ENABLED(CONFIG_RGB_PRO_LAMP_ARRAY_APPLY_BRT)
    const uint8_t scale = (uint8_t)((state.brt * 255) / 100);
#endif

    k_spinlock_key_t key = k_spin_lock(&la_lock);
    bool host = !autonomous;
    if (host) {
        for (int i = 0; i < LAMP_COUNT; i++) {
            /* The intensity channel is ignored: all lamps are RGB and Microsoft's
             * own sample does the same. */
            struct led_rgb c = {.r = active[i].r, .g = active[i].g, .b = active[i].b};
#if IS_ENABLED(CONFIG_RGB_PRO_LAMP_ARRAY_APPLY_BRT)
            c = rgbp_scale(c, scale);
#endif
            pixels[i] = c;
        }
    }
    k_spin_unlock(&la_lock, key);
    return host;
}
