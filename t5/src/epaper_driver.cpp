#include "epaper_driver.h"
#include "board_config.h"
#include <Arduino.h>
#include "epd_driver.h"

// =============================================================================
// Framebuffer (4bpp = 2 pixels per byte, managed by us)
// =============================================================================
static uint8_t* epd_framebuffer = nullptr;

// LVGL draw buffers (allocated in PSRAM)
static uint8_t* lv_buf1 = nullptr;
static uint8_t* lv_buf2 = nullptr;

// Full refresh scheduling
static unsigned long last_full_refresh = 0;
static bool full_refresh_requested = false;
static const unsigned long FULL_REFRESH_INTERVAL_MS = 30 * 60 * 1000; // 30 minutes

// Track the overall dirty area across flush chunks for batched update
static int32_t dirty_x1 = INT32_MAX, dirty_y1 = INT32_MAX;
static int32_t dirty_x2 = 0, dirty_y2 = 0;

// =============================================================================
// LVGL flush callback — converts L8 grayscale to epdiy 4bpp framebuffer
// =============================================================================
static void epaper_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;
    int32_t w = x2 - x1 + 1;

    // Convert LVGL L8 (8-bit grayscale) to epdiy 4bpp framebuffer
    // epdiy stores 2 pixels per byte: high nibble = even pixel, low nibble = odd pixel
    for (int32_t y = y1; y <= y2; y++) {
        for (int32_t x = x1; x <= x2; x++) {
            // Get the L8 pixel value (0-255)
            uint8_t gray = px_map[(y - y1) * w + (x - x1)];

            // Convert 8-bit to 4-bit (0-15)
            uint8_t gray4 = gray >> 4;

            // Pack into 4bpp framebuffer
            uint8_t* buf_ptr = &epd_framebuffer[y * (EPD_WIDTH / 2) + x / 2];
            if (x % 2) {
                *buf_ptr = (*buf_ptr & 0x0F) | (gray4 << 4);
            } else {
                *buf_ptr = (*buf_ptr & 0xF0) | gray4;
            }
        }
    }

    // Expand dirty area to cover this chunk
    if (x1 < dirty_x1) dirty_x1 = x1;
    if (y1 < dirty_y1) dirty_y1 = y1;
    if (x2 > dirty_x2) dirty_x2 = x2;
    if (y2 > dirty_y2) dirty_y2 = y2;

    // When this is the last flush chunk, push the accumulated dirty area to display
    if (lv_display_flush_is_last(disp)) {
        Rect_t update_area = {
            .x = dirty_x1,
            .y = dirty_y1,
            .width = dirty_x2 - dirty_x1 + 1,
            .height = dirty_y2 - dirty_y1 + 1
        };

        epd_poweron();
        epd_draw_grayscale_image(update_area, epd_framebuffer);
        epd_poweroff();

        // Reset dirty tracking
        dirty_x1 = INT32_MAX;
        dirty_y1 = INT32_MAX;
        dirty_x2 = 0;
        dirty_y2 = 0;
    }

    lv_display_flush_ready(disp);
}

// =============================================================================
// Initialize e-paper display and LVGL
// =============================================================================
lv_display_t* epaper_init_display() {
    Serial.println("EPaper: Initializing epd_driver...");

    // Initialize the e-paper driver (LilyGo-EPD47 library)
    epd_init();

    // Allocate framebuffer in PSRAM (4bpp = EPD_WIDTH * EPD_HEIGHT / 2 bytes)
    size_t fb_size = EPD_WIDTH * EPD_HEIGHT / 2;
    epd_framebuffer = (uint8_t*)ps_calloc(sizeof(uint8_t), fb_size);
    if (!epd_framebuffer) {
        Serial.println("EPaper: ERROR - Failed to allocate framebuffer!");
        return nullptr;
    }
    Serial.printf("EPaper: Framebuffer allocated (%d bytes in PSRAM)\n", fb_size);

    // Clear display on boot (full refresh)
    epd_poweron();
    epd_clear();
    epd_poweroff();
    last_full_refresh = millis();

    Serial.println("EPaper: Display cleared (full refresh)");

    // Create LVGL display
    lv_display_t* disp = lv_display_create(EPD_WIDTH, EPD_HEIGHT);
    if (!disp) {
        Serial.println("EPaper: ERROR - Failed to create LVGL display!");
        return nullptr;
    }

    // Set flush callback
    lv_display_set_flush_cb(disp, epaper_flush_cb);

    // Set color format to L8 (8-bit grayscale)
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_L8);

    // Allocate LVGL draw buffers in PSRAM
    // Partial rendering: 960 * 60 = 57600 bytes per buffer
    size_t buf_size = EPD_WIDTH * 60;
    lv_buf1 = (uint8_t*)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
    lv_buf2 = (uint8_t*)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);

    if (!lv_buf1 || !lv_buf2) {
        Serial.println("EPaper: ERROR - Failed to allocate LVGL buffers!");
        return nullptr;
    }

    lv_display_set_buffers(disp, lv_buf1, lv_buf2, buf_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    Serial.printf("EPaper: LVGL display created (%dx%d, L8 grayscale)\n",
                  EPD_WIDTH, EPD_HEIGHT);

    return disp;
}

// =============================================================================
// Full refresh management (prevents ghosting)
// =============================================================================
void epaper_request_full_refresh() {
    full_refresh_requested = true;
}

void epaper_check_full_refresh() {
    unsigned long now = millis();

    // Check if periodic full refresh is due
    if (full_refresh_requested || (now - last_full_refresh >= FULL_REFRESH_INTERVAL_MS)) {
        Serial.println("EPaper: Performing full refresh (ghosting prevention)");

        epd_poweron();
        epd_clear();
        // Redraw the entire framebuffer after clearing
        Rect_t full = epd_full_screen();
        epd_draw_grayscale_image(full, epd_framebuffer);
        epd_poweroff();

        last_full_refresh = now;
        full_refresh_requested = false;
    }
}
