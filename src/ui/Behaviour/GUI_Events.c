#include "../GUI.h"

#ifdef GUI_EXTERNAL_CUSTOM_CALLBACK_FUNCTION_FILE
 #include GUI_EXTERNAL_CUSTOM_CALLBACK_FUNCTION_FILE
#endif

#include <stdio.h>

// External control manager instance (defined in main.cpp)
extern void* global_control_manager;

// C interface to C++ control manager
extern void control_manager_increase_fan_speed();
extern void control_manager_decrease_fan_speed();
extern void control_manager_activate_boost();
extern void control_manager_set_temp_profile(uint8_t profile);

// Deferred refresh flag - set by events, processed in main loop
static volatile bool refresh_requested = false;

// ==================================================
//  TOUCH DEBOUNCING - Prevent double button presses
// ==================================================

static unsigned long last_button_press_time = 0;
static const unsigned long BUTTON_DEBOUNCE_MS = 300;  // 300ms debounce

static bool is_button_debounced() {
    unsigned long now = millis();
    if (now - last_button_press_time < BUTTON_DEBOUNCE_MS) {
        printf("Button press ignored (debounce)\n");
        return true;  // Still in debounce period
    }
    last_button_press_time = now;
    return false;  // Allow press
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
// BUTTON EVENT HANDLERS - WITH DEBOUNCING
// ============================================================================

void GUI_event__Button__screen__BSpeedPlus__Clicked (lv_event_t* event) {
    if (is_button_debounced()) return;  // ✅ DEBOUNCE CHECK
    
    printf("BSpeedPlus button clicked!\n");
    control_manager_increase_fan_speed();
}

void GUI_event__Button__screen__BSpeedMinus__Clicked (lv_event_t* event) {
    if (is_button_debounced()) return;  // ✅ DEBOUNCE CHECK
    
    printf("BSpeedMinus button clicked!\n");
    control_manager_decrease_fan_speed();
}

void GUI_event__Button__screen__buttonspeedboost__Clicked (lv_event_t* event) {
    if (is_button_debounced()) return;  // ✅ DEBOUNCE CHECK
    
    printf("Boost button clicked!\n");
    control_manager_activate_boost();
}

// ============================================================================
// DROPDOWN EVENT HANDLER - IMPROVED
// ============================================================================

void GUI_event__Dropdown__screen__modedropdown__Value_Changed (lv_event_t* event) {
    lv_obj_t *dropdown = lv_event_get_target(event);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    
    printf("Dropdown value changed to: %d\n", selected);
    
    control_manager_set_temp_profile((uint8_t)selected);
    
    // ✅ Force dropdown to update immediately
    lv_obj_invalidate(dropdown);
    GUI_request_display_refresh();
}

// ==================================================
// DROPDOWN STYLING CALLBACK
// ==================================================

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

// NOTE: GUI_update_fan_speed_display_from_cpp() and GUI_update_temp_profile_display_from_cpp()
// are defined in control_manager.cpp, NOT here, to avoid multiple definition errors