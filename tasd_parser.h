#pragma once
#include <stdint.h>
#include <stdbool.h>

// TASD console type IDs
#define CONSOLE_SWITCH 0x09

// TASD chunk/packet type IDs
#define TASD_PKT_CONSOLE_TYPE 0x03
#define TASD_PKT_FPS_CHANGE   0x06
#define TASD_PKT_INPUT_CHUNK  0xA1

// A single frame of Switch controller input
typedef struct {
    uint32_t frame; // Which frame this input applies to
    uint8_t buttons1; // Matches SwitchReport buttons1
    uint8_t buttons2; // Matches SwitchReport buttons2
    uint8_t hat; // D-pad direction
    uint16_t lx; // Left stick X
    uint16_t ly; // Left stick Y
    uint16_t rx; // Right stick X
    uint16_t ry; // Right stick Y
} TASFrame;

// A fully loaded TAS movie
typedef struct {
    TASFrame* frames; // Array of all frames
    uint32_t frame_count; // How many frames are in the array
    uint32_t fps; // Playback speed (usually 60)
    uint8_t console; // Console type ID from the file
} TASMovie;

// Load a .tasd file from the SD card into a TASMovie struct
// Returns true on success, false on failure
bool tasd_load(const char* path, TASMovie* out_movie);

// Free the memory allocated by tasd_load
void tasd_free(TASMovie* movie);
