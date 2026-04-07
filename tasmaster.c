#include <furi.h>
#include <gui/gui.h>
#include <dialogs/dialogs.h>
#include <storage/storage.h>
#include <string.h>
#include <stdlib.h>

#include "hid_switch.h"
#include "tasd_parser.h"
#include "input_scheduler.h"

#define TAG "TASMaster"

// --- App states ---
typedef enum {
    StateFileSelect, // Waiting for user to pick a file
    StateReady, // File loaded, ready to play
    StatePlaying, // Currently playing back inputs
    StatePaused, // Paused mid-playback
    StateDone, // Playback finished
    StateError, // Something went wrong
} TASMasterState;

// --- Main app struct ---
typedef struct {
    Gui* gui;
    ViewPort* view_port;
    FuriMessageQueue* event_queue;
    TASMasterState state;
    TASMovie movie;
    InputScheduler scheduler;
    char filepath[256];
    char error_msg[64];
} TASMasterApp;

// --- HID send callback ---
// This is called every frame by the scheduler
// For now it logs the report - real USB HID send goes here later
static void hid_send_frame(const SwitchReport* report) {
    FURI_LOG_D(
        TAG,
        "Frame: b1=0x%02X b2=0x%02X hat=%d lx=%d ly=%d",
        report->buttons1,
        report->buttons2,
        report->hat,
        report->lx,
        report->ly);

    // TODO: replace log with real USB HID report send
    // furi_hal_usb_hid_send_report(report, sizeof(SwitchReport));
}

// --- Draw callback ---
static void draw_callback(Canvas* canvas, void* ctx) {
    TASMasterApp* app = ctx;
    canvas_clear(canvas);

    // Draw app title bar
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 12, "TASMaster");

    // Draw a divider line under the title
    canvas_draw_line(canvas, 0, 15, 128, 15);

    canvas_set_font(canvas, FontSecondary);

    switch(app->state) {
    case StateFileSelect:
        canvas_draw_str(canvas, 2, 28, "No file loaded");
        canvas_draw_str(canvas, 2, 40, "Press OK to browse");
        canvas_draw_str(canvas, 2, 52, "for a .tasd file");
        break;

    case StateReady: {
        // Show filename (just the last part after the last /)
        const char* name = strrchr(app->filepath, '/');
        name = name ? name + 1 : app->filepath;
        canvas_draw_str(canvas, 2, 27, name);

        // Show movie info
        char info[32];
        snprintf(
            info, sizeof(info), "Frames: %lu  FPS: %lu", app->movie.frame_count, app->movie.fps);
        canvas_draw_str(canvas, 2, 39, info);

        canvas_draw_str(canvas, 2, 55, "[OK] Play  [Back] Cancel");
        break;
    }

    case StatePlaying: {
        char progress[32];
        snprintf(
            progress,
            sizeof(progress),
            "Frame: %lu / %lu",
            app->scheduler.current_frame,
            app->movie.frame_count);
        canvas_draw_str(canvas, 2, 28, "Playing...");
        canvas_draw_str(canvas, 2, 40, progress);

        // Draw a simple progress bar
        uint32_t bar_width = 0;
        if(app->movie.frame_count > 0) {
            bar_width = (app->scheduler.current_frame * 120) / app->movie.frame_count;
        }
        canvas_draw_frame(canvas, 2, 48, 122, 8);
        if(bar_width > 0) {
            canvas_draw_box(canvas, 3, 49, bar_width, 6);
        }

        canvas_draw_str(canvas, 2, 63, "[OK] Pause  [Back] Stop");
        break;
    }

    case StatePaused: {
        char progress[32];
        snprintf(
            progress,
            sizeof(progress),
            "Frame: %lu / %lu",
            app->scheduler.current_frame,
            app->movie.frame_count);
        canvas_draw_str(canvas, 2, 28, "Paused");
        canvas_draw_str(canvas, 2, 40, progress);
        canvas_draw_str(canvas, 2, 63, "[OK] Resume  [Back] Stop");
        break;
    }

    case StateDone:
        canvas_draw_str(canvas, 2, 28, "Playback complete!");
        canvas_draw_str(canvas, 2, 40, "Press Back to return");
        break;

    case StateError:
        canvas_draw_str(canvas, 2, 28, "Error:");
        canvas_draw_str(canvas, 2, 40, app->error_msg);
        canvas_draw_str(canvas, 2, 55, "Press Back to return");
        break;
    }
}

// --- Input callback ---
static void input_callback(InputEvent* event, void* ctx) {
    TASMasterApp* app = ctx;
    furi_message_queue_put(app->event_queue, event, 0);
}

// --- Main app entry point ---
int32_t tasmaster_app(void* p) {
    UNUSED(p);

    // Allocate and zero the app struct
    TASMasterApp* app = malloc(sizeof(TASMasterApp));
    memset(app, 0, sizeof(TASMasterApp));
    app->state = StateFileSelect;

    // Set up GUI
    app->event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    app->gui = furi_record_open(RECORD_GUI);

    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);

    FURI_LOG_I(TAG, "TASMaster started");

    // --- Main event loop ---
    InputEvent event;
    bool running = true;

    while(running) {
        // Wait up to 100ms for an input event, then redraw anyway
        // This keeps the progress bar updating during playback
        if(furi_message_queue_get(app->event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypeShort) {
                switch(app->state) {
                case StateFileSelect:
                    if(event.key == InputKeyOk) {
                        DialogsApp* dialogs = furi_record_open(RECORD_DIALOGS);
                        DialogsFileBrowserOptions opts;
                        dialog_file_browser_set_basic_options(&opts, ".tasd", NULL);

                        // FuriString is required by the Flipper SDK dialog API
                        FuriString* furi_path = furi_string_alloc_set("/ext/tas");

                        bool picked =
                            dialog_file_browser_show(dialogs, furi_path, furi_path, &opts);
                        furi_record_close(RECORD_DIALOGS);

                        if(picked) {
                            // Copy result into our char array for storage
                            strncpy(
                                app->filepath,
                                furi_string_get_cstr(furi_path),
                                sizeof(app->filepath) - 1);
                            furi_string_free(furi_path);

                            FURI_LOG_I(TAG, "Loading file: %s", app->filepath);
                            if(tasd_load(app->filepath, &app->movie)) {
                                scheduler_init(&app->scheduler, &app->movie, hid_send_frame);
                                app->state = StateReady;
                            } else {
                                strncpy(
                                    app->error_msg, "Failed to load file", sizeof(app->error_msg));
                                app->state = StateError;
                            }
                        } else {
                            furi_string_free(furi_path);
                        }
                    } else if(event.key == InputKeyBack) {
                        running = false;
                    }
                    break;

                case StateReady:
                    if(event.key == InputKeyOk) {
                        scheduler_start(&app->scheduler);
                        app->state = StatePlaying;
                    } else if(event.key == InputKeyBack) {
                        tasd_free(&app->movie);
                        app->state = StateFileSelect;
                    }
                    break;

                case StatePlaying:
                    if(event.key == InputKeyOk) {
                        scheduler_pause(&app->scheduler);
                        app->state = StatePaused;
                    } else if(event.key == InputKeyBack) {
                        scheduler_stop(&app->scheduler);
                        tasd_free(&app->movie);
                        app->state = StateFileSelect;
                    }
                    break;

                case StatePaused:
                    if(event.key == InputKeyOk) {
                        scheduler_start(&app->scheduler);
                        app->state = StatePlaying;
                    } else if(event.key == InputKeyBack) {
                        scheduler_stop(&app->scheduler);
                        tasd_free(&app->movie);
                        app->state = StateFileSelect;
                    }
                    break;

                case StateDone:
                    if(event.key == InputKeyBack) {
                        tasd_free(&app->movie);
                        app->state = StateFileSelect;
                    }
                    break;

                case StateError:
                    if(event.key == InputKeyBack) {
                        app->state = StateFileSelect;
                    }
                    break;
                }
            }
        }

        // Check if playback finished naturally
        if(app->state == StatePlaying && !app->scheduler.playing) {
            scheduler_free(&app->scheduler);
            app->state = StateDone;
        }

        view_port_update(app->view_port);
    }

    // --- Cleanup ---
    FURI_LOG_I(TAG, "TASMaster exiting");
    scheduler_free(&app->scheduler);
    tasd_free(&app->movie);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    furi_record_close(RECORD_GUI);
    furi_message_queue_free(app->event_queue);
    free(app);

    return 0;
}
