#include "../GUI.h"

#ifdef GUI_EXTERNAL_CUSTOM_CALLBACK_FUNCTION_FILE
 #include GUI_EXTERNAL_CUSTOM_CALLBACK_FUNCTION_FILE
#endif

#include <stdio.h>
#include <Arduino.h>

// External control manager interface
extern void control_manager_increase_fan_speed();
extern void control_manager_decrease_fan_speed();
extern void control_manager_activate_boost();
extern void control_manager_set_temp_profile(uint8_t profile);

// Deferred refresh flag
static volatile bool refresh_requested = false;

// ============================================================================
// MINIMAL DEBOUNCE - For Protection Only
// ============================================================================
// With proper touch state machine, we only need minimal debounce protection
// against potential LVGL quirks. The touch callback handles the real work.

static unsigned long last_plus_press = 0;
static unsigned long last_minus_press = 0;
static unsigned long last_boost_press = 0;
static unsigned long last_dropdown_change = 0;

// Very short debounce - just enough to catch any potential double-firing
static const unsigned long BUTTON_DEBOUNCE_MS = 50;  // Reduced from 100ms!

static bool is_too_soon(unsigned long* last_press_time) {
    unsigned long now = millis();
    unsigned long elapsed = now - *last_press_time;
    
    if (elapsed < BUTTON_DEBOUNCE_MS) {
        // This should rarely happen with proper touch state
        printf("⚠️  Button debounce triggered (safety catch: %lu ms)\n", elapsed);
        return true;
    }
    
    *last_press_time = now;
    return false;
}

// ============================================================================
// DISPLAY REFRESH MANAGEMENT
// ============================================================================

void GUI_request_display_refresh(void) {
    refresh_requested = true;
}

void GUI_process_display_refresh(void) {
    if (refresh_requested) {
        lv_refr_now(NULL);
        refresh_requested = false;
    }
}

// ============================================================================
// BUTTON EVENT HANDLERS - Simplified!
// ============================================================================

void GUI_event__Button__screen__BSpeedPlus__Clicked (lv_event_t* event) {
    if (is_too_soon(&last_plus_press)) return;
    
    printf("✅ [+] Fan Speed Increase\n");
    control_manager_increase_fan_speed();
}

void GUI_event__Button__screen__BSpeedMinus__Clicked (lv_event_t* event) {
    if (is_too_soon(&last_minus_press)) return;
    
    printf("✅ [-] Fan Speed Decrease\n");
    control_manager_decrease_fan_speed();
}

void GUI_event__Button__screen__buttonspeedboost__Clicked (lv_event_t* event) {
    if (is_too_soon(&last_boost_press)) return;
    
    printf("✅ [BOOST] Activated\n");
    control_manager_activate_boost();
}

// ============================================================================
// DROPDOWN EVENT HANDLER
// ============================================================================

void GUI_event__Dropdown__screen__modedropdown__Value_Changed (lv_event_t* event) {
    if (is_too_soon(&last_dropdown_change)) return;
    
    lv_obj_t *dropdown = lv_event_get_target(event);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    
    printf("✅ [DROPDOWN] Profile changed to: %d\n", selected);
    
    control_manager_set_temp_profile((uint8_t)selected);
    
    lv_obj_invalidate(dropdown);
    GUI_request_display_refresh();
}

// ============================================================================
// DROPDOWN STYLING
// ============================================================================

static void dropdown_list_styling_cb(lv_event_t *e) {
    lv_obj_t *list = lv_event_get_target(e);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x002138), LV_PART_MAIN);
    lv_obj_set_style_text_color(list, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_color(list, lv_color_hex(0xFFC802), LV_PART_SELECTED);
    lv_obj_set_style_text_color(list, lv_color_hex(0x000000), LV_PART_SELECTED);
}

void GUI_init_dropdown_styling(void) {
    lv_obj_t *list = lv_dropdown_get_list(GUI_Dropdown__screen__modedropdown);
    if (list) {
        lv_obj_set_style_bg_color(list, lv_color_hex(0x002138), LV_PART_MAIN);
        lv_obj_set_style_text_color(list, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_bg_color(list, lv_color_hex(0xFFC802), LV_PART_SELECTED);
        lv_obj_set_style_text_color(list, lv_color_hex(0x000000), LV_PART_SELECTED);
        
        lv_obj_add_event_cb(list, dropdown_list_styling_cb, LV_EVENT_CLICKED, NULL);
        
        printf("Dropdown list styled\n");
    }
}

void GUI_init_fan_speed_display(void) {
    printf("Fan speed display initialization handled by control manager\n");
}

// ============================================================================
// HOW THIS WORKS WITH THE NEW TOUCH CALLBACK
// ============================================================================
//
// Scenario 1: Press and HOLD button
// -----------------------------------
// Time 0ms:   Touch callback detects finger down
//             → Sets touch_active=true, reports PRESSED
// Time 5ms:   LVGL processes touch, button detects press
//             → Fires CLICKED event
//             → This handler runs, increments fan speed
// Time 10ms:  Touch callback still reports PRESSED (finger held)
//             → LVGL sees button is still pressed
//             → NO new CLICKED event (button is in PRESSED state)
// Time 500ms: Still holding...
//             → Still PRESSED state
//             → Still NO new CLICKED events
// Time 1000ms: Finger lifts
//             → Touch callback reports RELEASED
//             → Button returns to normal state
//
// Result: ONE button press, ONE speed change ✅
//
//
// Scenario 2: Quick double tap
// ------------------------------
// Time 0ms:   First tap - finger down
//             → CLICKED event → Speed increases
// Time 100ms: Finger up
//             → touch_active=false
// Time 150ms: Second tap - finger down again
//             → touch_active=true (NEW press detected!)
//             → CLICKED event → Speed increases again
// Time 250ms: Finger up
//
// Result: TWO button presses, TWO speed changes ✅
//
//
// Why minimal debounce (50ms)?
// ----------------------------
// - Main protection is in touch callback state machine
// - This 50ms is just a safety net for any LVGL quirks
// - Short enough to allow rapid tapping (~20 taps/second)
// - Long enough to catch potential glitches
//
// Old debounce (100-500ms) was fighting against the touch system.
// New approach: touch callback does state detection, GUI events just add safety.