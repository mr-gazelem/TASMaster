#pragma once
#include <stdint.h>

// Nintendo Switch Pro Controller USB identifiers
#define SWITCH_VID 0x057E
#define SWITCH_PID 0x2009

// HID Report structure - this is what gets sent to the Switch each frame
typedef struct {
    uint8_t buttons1; // Y X B A SR SL R ZR
    uint8_t buttons2; // Minus Plus Rstick Lstick Home Capture
    uint8_t hat; // D-pad (0=Up, 1=UpRight, 2=Right, 3=DownRight,
        //        4=Down, 5=DownLeft, 6=Left, 7=UpLeft, 8=Neutral)
    uint16_t lx; // Left stick X  (0-4095, center=2048)
    uint16_t ly; // Left stick Y  (0-4095, center=2048)
    uint16_t rx; // Right stick X (0-4095, center=2048)
    uint16_t ry; // Right stick Y (0-4095, center=2048)
} __attribute__((packed)) SwitchReport;

// --- Button bitmasks for buttons1 ---
#define SW_BTN_Y  (1 << 0)
#define SW_BTN_X  (1 << 1)
#define SW_BTN_B  (1 << 2)
#define SW_BTN_A  (1 << 3)
#define SW_BTN_R  (1 << 6)
#define SW_BTN_ZR (1 << 7)

// --- Button bitmasks for buttons2 ---
#define SW_BTN_MINUS  (1 << 0)
#define SW_BTN_PLUS   (1 << 1)
#define SW_BTN_RSTICK (1 << 2)
#define SW_BTN_LSTICK (1 << 3)
#define SW_BTN_HOME   (1 << 4)
#define SW_BTN_CAP    (1 << 5)
#define SW_BTN_L      (1 << 6)
#define SW_BTN_ZL     (1 << 7)

// --- HAT directions ---
#define HAT_UP        0
#define HAT_UPRIGHT   1
#define HAT_RIGHT     2
#define HAT_DOWNRIGHT 3
#define HAT_DOWN      4
#define HAT_DOWNLEFT  5
#define HAT_LEFT      6
#define HAT_UPLEFT    7
#define HAT_NEUTRAL   8

// Stick center value
#define STICK_CENTER 2048
