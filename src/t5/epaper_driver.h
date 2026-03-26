#ifndef T5_EPAPER_DRIVER_H
#define T5_EPAPER_DRIVER_H

#include <lvgl.h>

// Initialize the e-paper display and LVGL display driver
// Returns the LVGL display object
lv_display_t* epaper_init_display();

// Request a full refresh (call periodically to prevent ghosting)
void epaper_request_full_refresh();

// Check if a full refresh is needed and perform it
void epaper_check_full_refresh();

#endif // T5_EPAPER_DRIVER_H
