#include "input_scheduler.h"
#include <furi.h>
#include <stdlib.h>
#include <string.h>

#define TAG "TASScheduler"

// This function is called by the timer every frame
// It builds a SwitchReport from the current TASFrame and fires the callback
static void scheduler_timer_callback(void* ctx) {
    InputScheduler* s = ctx;

    if(!s->playing || s->paused) return;

    // Check if we have reached the end of the movie
    if(s->current_frame >= s->movie->frame_count) {
        FURI_LOG_I(TAG, "Playback complete");
        s->playing = false;
        furi_timer_stop(s->timer);
        return;
    }

    // Get the current frame data
    const TASFrame* f = &s->movie->frames[s->current_frame];

    // Build the HID report
    SwitchReport report;
    memset(&report, 0, sizeof(SwitchReport));
    report.buttons1 = f->buttons1;
    report.buttons2 = f->buttons2;
    report.hat = f->hat;
    report.lx = f->lx;
    report.ly = f->ly;
    report.rx = f->rx;
    report.ry = f->ry;

    // Send it
    if(s->send_cb) {
        s->send_cb(&report);
    }

    // Advance to next frame
    s->current_frame++;

    FURI_LOG_D(TAG, "Frame %lu / %lu", s->current_frame, s->movie->frame_count);
}

void scheduler_init(InputScheduler* s, const TASMovie* movie, HIDSendCallback cb) {
    memset(s, 0, sizeof(InputScheduler));
    s->movie = movie;
    s->send_cb = cb;
    s->current_frame = 0;
    s->playing = false;
    s->paused = false;

    // Calculate the interval in milliseconds between frames
    // e.g. 60fps = 1000ms / 60 = ~16ms per frame
    uint32_t interval_ms = 1000 / movie->fps;

    // Allocate the timer - it will call scheduler_timer_callback every interval
    s->timer = furi_timer_alloc(scheduler_timer_callback, FuriTimerTypePeriodic, s);

    FURI_LOG_I(
        TAG,
        "Scheduler ready - %lu fps, %lu ms/frame, %lu total frames",
        movie->fps,
        interval_ms,
        movie->frame_count);
}

void scheduler_start(InputScheduler* s) {
    if(!s->timer) return;

    s->playing = true;
    s->paused = false;

    uint32_t interval_ms = 1000 / s->movie->fps;
    furi_timer_start(s->timer, furi_ms_to_ticks(interval_ms));

    FURI_LOG_I(TAG, "Playback started at frame %lu", s->current_frame);
}

void scheduler_pause(InputScheduler* s) {
    if(!s->playing) return;

    s->paused = true;
    furi_timer_stop(s->timer);

    FURI_LOG_I(TAG, "Playback paused at frame %lu", s->current_frame);
}

void scheduler_stop(InputScheduler* s) {
    s->playing = false;
    s->paused = false;
    s->current_frame = 0;

    if(s->timer) furi_timer_stop(s->timer);

    FURI_LOG_I(TAG, "Playback stopped");
}

void scheduler_free(InputScheduler* s) {
    if(s->timer) {
        furi_timer_stop(s->timer);
        furi_timer_free(s->timer);
        s->timer = NULL;
    }
}
