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

// ============================================================================
// BUTTON EVENT HANDLERS
// ============================================================================

void GUI_event__Button__screen__BSpeedPlus__Clicked (lv_event_t* event) {
    printf("BSpeedPlus button clicked!\n");
    control_manager_increase_fan_speed();
}

void GUI_event__Button__screen__BSpeedMinus__Clicked (lv_event_t* event) {
    printf("BSpeedMinus button clicked!\n");
    control_manager_decrease_fan_speed();
}

void GUI_event__Button__screen__buttonspeedboost__Clicked (lv_event_t* event) {
    printf("Boost button clicked!\n");
    control_manager_activate_boost();
}

void GUI_event__Dropdown__screen__modedropdown__Value_Changed (lv_event_t* event) {
    lv_obj_t *dropdown = (lv_obj_t*)lv_event_get_target(event);
    uint16_t sel = lv_dropdown_get_selected(dropdown);
    
    char selected_text[32];
    lv_dropdown_get_selected_str(dropdown, selected_text, sizeof(selected_text));
    
    printf("Temperature profile changed to: %d (%s)\n", sel, selected_text);
    
    // Close the dropdown list after selection
    lv_dropdown_close(dropdown);
    
    // Send to control manager
    control_manager_set_temp_profile((uint8_t)sel);
}

// ============================================================================
// DROPDOWN STYLING
// ============================================================================

// Dropdown list styling callback - regular C function
static void dropdown_list_styling_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_t *target = (lv_obj_t*)lv_event_get_target(e);
        printf("Dropdown list clicked\n");
        
        lv_obj_t *list = lv_dropdown_get_list(GUI_Dropdown__screen__modedropdown);
        if (list) {
            printf("Dropdown list styling applied\n");
        }
    }
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

// Initialize function - call this once at startup to set initial display
void GUI_init_fan_speed_display(void) {
    // Initial display will be set by control manager
    printf("Fan speed display initialization handled by control manager\n");
}