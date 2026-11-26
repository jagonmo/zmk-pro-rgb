/*
 * HID LampArray (HID Usage Tables 1.4, page 0x59): report IDs, report layouts
 * and the report descriptor. Protocol only, no state and no Kconfig.
 *
 * Shared by the keyboard's USB LampArray (lamp_array.c) and by a receiver
 * dongle that exposes LampArray on its own USB port.
 *
 * Report layout and the report descriptor come from Microsoft's
 * ArduinoHidForWindows / RP2040MacropadHidSample (MIT licence,
 * Copyright (c) Microsoft Corporation), generated with Waratah from HID
 * Usage Tables 1.4.0, and are used verbatim.
 *
 * Include it from ONE source file per image: it defines the descriptor.
 */

#pragma once

#include <stdint.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

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
