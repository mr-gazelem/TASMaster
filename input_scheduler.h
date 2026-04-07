#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <furi.h>
#include "tasd_parser.h"
#include "hid_switch.h"

// Callback type - called every frame with the current input
// This is what actually sends the HID report to the Switch
typedef void (*HIDSendCallback)(const SwitchReport* report);

// The scheduler - tracks playback state
typedef struct {
    const TASMovie* movie; // The loaded movie (not owned)
    uint32_t current_frame; // Which frame we are on
    bool playing; // Is playback active
    bool paused; // Is playback paused
    HIDSendCallback send_cb; // Function to call each frame
    FuriTimer* timer; // Hardware timer driving playback
} InputScheduler;

// Set up the scheduler with a loaded movie and a send callback
void scheduler_init(InputScheduler* s, const TASMovie* movie, HIDSendCallback cb);

// Start or resume playback
void scheduler_start(InputScheduler* s);

// Pause playback (keeps current frame)
void scheduler_pause(InputScheduler* s);

// Stop playback and reset to frame 0
void scheduler_stop(InputScheduler* s);

// Clean up the timer when done with the scheduler
void scheduler_free(InputScheduler* s);
