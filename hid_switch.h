#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <furi.h>

// Nintendo Switch Pro Controller USB identifiers
#define SWITCH_VID 0x057E
#define SWITCH_PID 0x2009

// HID Report structure
typedef struct {
    uint8_t  buttons1;
    uint8_t  buttons2;
    uint8_t  hat;
    uint16_t lx;
    uint16_t ly;
    uint16_t rx;
    uint16_t ry;
} __attribute__((packed)) SwitchReport;

// Button bitmasks - buttons1
#define SW_BTN_Y   (1 << 0)
#define SW_BTN_X   (1 << 1)
#define SW_BTN_B   (1 << 2)
#define SW_BTN_A   (1 << 3)
#define SW_BTN_R   (1 << 6)
#define SW_BTN_ZR  (1 << 7)

// Button bitmasks - buttons2
#define SW_BTN_MINUS  (1 << 0)
#define SW_BTN_PLUS   (1 << 1)
#define SW_BTN_RSTICK (1 << 2)
#define SW_BTN_LSTICK (1 << 3)
#define SW_BTN_HOME   (1 << 4)
#define SW_BTN_CAP    (1 << 5)
#define SW_BTN_L      (1 << 6)
#define SW_BTN_ZL     (1 << 7)

// HAT directions
#define HAT_UP        0
#define HAT_UPRIGHT   1
#define HAT_RIGHT     2
#define HAT_DOWNRIGHT 3
#define HAT_DOWN      4
#define HAT_DOWNLEFT  5
#define HAT_LEFT      6
#define HAT_UPLEFT    7
#define HAT_NEUTRAL   8

#define STICK_CENTER  2048

// Public functions
bool hid_switch_init(void);
void hid_switch_deinit(void);
void hid_switch_send_report(const SwitchReport* report);