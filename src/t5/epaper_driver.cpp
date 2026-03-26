#include "epaper_driver.h"
#include "board_config.h"
#include <Arduino.h>
#include <epd_driver.h>

// =============================================================================
// E-paper framebuffer (4bpp = 2 pixels per byte)
// =============================================================================
static uint8_t* epd_framebuffer = nullptr;

// LVGL draw buffers (allocated in PSRAM)
static uint8_t* lv_buf1 = nullptr;
static uint8_t* lv_buf2 = nullptr;

// Full refresh scheduling
static unsigned long last_full_refresh = 0;
static bool full_refresh_requested = false;
static const unsigned long FULL_REFRESH_INTERVAL_MS = 30 * 60 * 1000; // 30 minutes

// =============================================================================
// LVGL flush callback — converts L8 grayscale to epdiy 4bpp framebuffer
// =============================================================================
static void epaper_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t x2 = area->x2;
    int32_t y2 = area->y2;

    // Convert LVGL L8 (8-bit grayscale) to epdiy 4bpp framebuffer
    // epdiy stores 2 pixels per byte: high nibble = even pixel, low nibble = odd pixel
    for (int32_t y = y1; y <= y2; y++) {
        for (int32_t x = x1; x <= x2; x++) {
            // Get the L8 pixel value (0-255)
            uint8_t gray = px_map[(y - y1) * (x2 - x1 + 1) + (x - x1)];

            // Convert 8-bit to 4-bit (0-15)
            uint8_t gray4 = gray >> 4;

            // Calculate position in epdiy framebuffer
            int32_t fb_index = y * (EPD_WIDTH / 2) + x / 2;

            if (x % 2 == 0) {
                // Even pixel: high nibble
                epd_framebuffer[fb_index] = (epd_framebuffer[fb_index] & 0x0F) | (gray4 << 4);
            } else {
                // Odd pixel: low nibble
                epd_framebuffer[fb_index] = (epd_framebuffer[fb_index] & 0xF0) | gray4;
            }
        }
    }

    // Check if this is the last flush chunk — if so, push to display
    if (lv_display_flush_is_last(disp)) {
        // Use partial update for the dirty area
        Rect_t update_area = {
            .x = x1,
            .y = y1,
            .width = x2 - x1 + 1,
            .height = y2 - y1 + 1
        };

        epd_poweron();
        epd_draw_grayscale_image(update_area, epd_framebuffer);
        epd_poweroff();
    }

    lv_display_flush_ready(disp);
}

// =============================================================================
// Initialize e-paper display and LVGL
// =============================================================================
lv_display_t* epaper_init_display() {
    Serial.println("EPaper: Initializing epdiy driver...");

    // Initialize epdiy
    epd_init();

    // Allocate framebuffer in PSRAM (4bpp = EPD_WIDTH * EPD_HEIGHT / 2 bytes)
    epd_framebuffer = (uint8_t*)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!epd_framebuffer) {
        Serial.println("EPaper: ERROR - Failed to allocate framebuffer!");
        return nullptr;
    }
    Serial.printf("EPaper: Framebuffer allocated (%d bytes in PSRAM)\n",
                  EPD_WIDTH * EPD_HEIGHT / 2);

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
    // Use partial rendering with moderate buffer size (960 * 60 = 57600 bytes per buffer)
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
        epd_draw_grayscale_image(epd_full_screen(), epd_framebuffer);
        epd_poweroff();

        last_full_refresh = now;
        full_refresh_requested = false;
    }
}
