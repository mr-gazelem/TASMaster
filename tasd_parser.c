#include "tasd_parser.h"
#include <stdlib.h>
#include <string.h>
#include <furi.h>
#include <storage/storage.h>

#define TAG                    "TASParser"
#define TASD_MAGIC             0x54415344 // "TASD" in ASCII
#define INITIAL_FRAME_CAPACITY 1000

// Helper - read exactly n bytes from file into buffer
// Returns false if it couldn't read enough bytes
static bool read_bytes(File* file, void* buffer, size_t n) {
    size_t got = storage_file_read(file, buffer, n);
    return got == n;
}

// Helper - read a single byte
static bool read_u8(File* file, uint8_t* out) {
    return read_bytes(file, out, 1);
}

// Helper - read a big-endian 2 byte unsigned int
static bool read_u16_be(File* file, uint16_t* out) {
    uint8_t buf[2];
    if(!read_bytes(file, buf, 2)) return false;
    *out = (uint16_t)((buf[0] << 8) | buf[1]);
    return true;
}

// Helper - read a big-endian 4 byte unsigned int
static bool read_u32_be(File* file, uint32_t* out) {
    uint8_t buf[4];
    if(!read_bytes(file, buf, 4)) return false;
    *out = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) |
           ((uint32_t)buf[3]);
    return true;
}

bool tasd_load(const char* path, TASMovie* out_movie) {
    // Zero out the output struct first
    memset(out_movie, 0, sizeof(TASMovie));
    out_movie->fps = 60; // default

    // Open storage
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);

    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        FURI_LOG_E(TAG, "Failed to open file: %s", path);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // --- Read and verify the TASD header ---
    uint32_t magic;
    if(!read_u32_be(file, &magic) || magic != TASD_MAGIC) {
        FURI_LOG_E(TAG, "Invalid TASD magic number");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // Read version (2 bytes, we accept any version)
    uint16_t version;
    if(!read_u16_be(file, &version)) {
        FURI_LOG_E(TAG, "Failed to read version");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    FURI_LOG_I(TAG, "TASD version: %d", version);

    // --- Allocate initial frame array ---
    uint32_t capacity = INITIAL_FRAME_CAPACITY;
    out_movie->frames = malloc(sizeof(TASFrame) * capacity);
    if(!out_movie->frames) {
        FURI_LOG_E(TAG, "Out of memory");
        storage_file_close(file);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    // --- Read chunks until end of file ---
    uint32_t current_frame = 0;
    bool success = true;

    while(true) {
        // Read packet type (1 byte)
        uint8_t pkt_type;
        if(!read_u8(file, &pkt_type)) {
            // End of file - this is normal
            break;
        }

        // Read payload length (2 bytes, big endian)
        uint16_t pkt_len;
        if(!read_u16_be(file, &pkt_len)) {
            FURI_LOG_E(TAG, "Failed to read packet length");
            success = false;
            break;
        }

        if(pkt_type == TASD_PKT_CONSOLE_TYPE) {
            // Console type packet - 1 byte payload
            uint8_t console;
            if(!read_u8(file, &console)) {
                success = false;
                break;
            }
            out_movie->console = console;
            FURI_LOG_I(TAG, "Console type: 0x%02X", console);

        } else if(pkt_type == TASD_PKT_FPS_CHANGE) {
            // FPS packet - numerator and denominator as u32 each
            uint32_t num, den;
            if(!read_u32_be(file, &num) || !read_u32_be(file, &den)) {
                success = false;
                break;
            }
            if(den > 0) out_movie->fps = num / den;
            FURI_LOG_I(TAG, "FPS: %lu", out_movie->fps);

        } else if(pkt_type == TASD_PKT_INPUT_CHUNK) {
            // Input chunk - frame count (u32) then button data
            uint32_t frame_count;
            if(!read_u32_be(file, &frame_count)) {
                success = false;
                break;
            }

            // Read button bytes for Switch:
            // buttons1, buttons2, hat, lx(u16), ly(u16), rx(u16), ry(u16)
            uint8_t buttons1, buttons2, hat;
            uint16_t lx, ly, rx, ry;

            if(!read_u8(file, &buttons1) || !read_u8(file, &buttons2) || !read_u8(file, &hat) ||
               !read_u16_be(file, &lx) || !read_u16_be(file, &ly) || !read_u16_be(file, &rx) ||
               !read_u16_be(file, &ry)) {
                success = false;
                break;
            }

            // This input applies for frame_count frames in a row
            for(uint32_t i = 0; i < frame_count; i++) {
                // Grow array if needed
                if(out_movie->frame_count >= capacity) {
                    capacity *= 2;
                    TASFrame* new_frames = realloc(out_movie->frames, sizeof(TASFrame) * capacity);
                    if(!new_frames) {
                        FURI_LOG_E(TAG, "Out of memory during realloc");
                        success = false;
                        break;
                    }
                    out_movie->frames = new_frames;
                }

                // Store the frame
                TASFrame* f = &out_movie->frames[out_movie->frame_count];
                f->frame = current_frame++;
                f->buttons1 = buttons1;
                f->buttons2 = buttons2;
                f->hat = hat;
                f->lx = lx;
                f->ly = ly;
                f->rx = rx;
                f->ry = ry;
                out_movie->frame_count++;
            }

        } else {
            // Unknown packet type - skip its payload
            FURI_LOG_W(TAG, "Skipping unknown packet type: 0x%02X", pkt_type);
            uint8_t skip;
            for(uint16_t i = 0; i < pkt_len; i++) {
                if(!read_u8(file, &skip)) {
                    success = false;
                    break;
                }
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    if(!success) {
        tasd_free(out_movie);
        return false;
    }

    FURI_LOG_I(TAG, "Loaded %lu frames at %lu fps", out_movie->frame_count, out_movie->fps);
    return true;
}

void tasd_free(TASMovie* movie) {
    if(movie->frames) {
        free(movie->frames);
        movie->frames = NULL;
    }
    movie->frame_count = 0;
}
