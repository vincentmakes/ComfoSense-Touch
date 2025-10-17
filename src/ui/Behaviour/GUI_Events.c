#include "../GUI.h"

#ifdef GUI_EXTERNAL_CUSTOM_CALLBACK_FUNCTION_FILE
 #include GUI_EXTERNAL_CUSTOM_CALLBACK_FUNCTION_FILE
#endif

#include <stdio.h>
#include <Arduino.h>

extern void control_manager_increase_fan_speed();
extern void control_manager_decrease_fan_speed();
extern void control_manager_activate_boost();
extern void control_manager_set_temp_profile(uint8_t profile);

static volatile bool refresh_requested = false;


// ============================================================================
// OPTIMIZED DEBOUNCE - Faster double-tap capability
// ============================================================================

static unsigned long last_plus_press = 0;
static unsigned long last_minus_press = 0;
static unsigned long last_boost_press = 0;
static unsigned long last_dropdown_change = 0;

// ⚡ OPTIMIZED: 100ms debounce
static const unsigned long BUTTON_DEBOUNCE_MS = 50;

static bool is_too_soon(unsigned long* last_press_time) {
    unsigned long now = millis();
    unsigned long elapsed = now - *last_press_time;
    
    if (elapsed < BUTTON_DEBOUNCE_MS) {
        printf("⚠️  Button debounce (%lu ms since last)\n", elapsed);
        return true;
    }
    
    *last_press_time = now;
    return false;
}

// ============================================================================
// DISPLAY REFRESH - ONLY FOR BACKGROUND UPDATES (like TimeManager)
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
// BUTTON HANDLERS - Fast and responsive WITHOUT forced refresh!
// ============================================================================

void GUI_event__Button__screen__BSpeedPlus__Clicked (lv_event_t* event) {
    if (is_too_soon(&last_plus_press)) return;
    
    printf("✅ [+] Fan Speed\n");
    control_manager_increase_fan_speed();
    GUI_request_display_refresh();
    // ✅ NO GUI_request_display_refresh() - images update on next timer cycle
    // This happens in < 1ms because main loop calls lv_timer_handler() constantly
}

void GUI_event__Button__screen__BSpeedMinus__Clicked (lv_event_t* event) {
    if (is_too_soon(&last_minus_press)) return;
    
    printf("✅ [-] Fan Speed\n");
    control_manager_decrease_fan_speed();
    GUI_request_display_refresh();
    // ✅ NO forced refresh needed
}

void GUI_event__Button__screen__buttonspeedboost__Clicked (lv_event_t* event) {
    if (is_too_soon(&last_boost_press)) return;
    
    printf("✅ [BOOST]\n");
    control_manager_activate_boost();
    GUI_request_display_refresh();
    // ✅ NO forced refresh needed
}

//==============TEST=============
// Called on LV_EVENT_PRESSED to make sure the dropdown list gets drawn immediately



void GUI_event__Dropdown__screen__modedropdown__Ready(lv_event_t* e) {
    lv_obj_t *dropdown= lv_event_get_target(e);
//essential for the list to display instantly
    GUI_request_display_refresh();   

}

//============================


// ============================================================================
// DROPDOWN HANDLER - Closes dropdown WITHOUT blocking refresh
// ============================================================================

void GUI_event__Dropdown__screen__modedropdown__Value_Changed (lv_event_t* event) {
    if (is_too_soon(&last_dropdown_change)) return;
    
    lv_obj_t *dropdown = lv_event_get_target(event);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    
    printf("✅ [DROPDOWN] %d\n", selected);
        // ✅ Close dropdown and invalidate - NO forced refresh!
    lv_dropdown_close(dropdown);
    GUI_request_display_refresh();   
    lv_obj_invalidate(dropdown);

    control_manager_set_temp_profile((uint8_t)selected);
    

    
    // Dropdown will close on next lv_timer_handler() cycle (< 1ms)
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
        
        printf("Dropdown styled\n");
    }
}

void GUI_init_fan_speed_display(void) {
    printf("Fan speed display handled by control manager\n");
}

// ============================================================================
// KEY INSIGHT: Strategy 5 is ONLY for background updates!
// ============================================================================
// 
// USE Strategy 5 (GUI_request_display_refresh) for:
//   ✅ TimeManager updating time/date labels every second
//   ✅ Sensor data updating in background
//   ✅ Any non-user-triggered label text updates
//
// DON'T USE Strategy 5 for:
//   ❌ Button presses (user-triggered)
//   ❌ Dropdown (user-triggered)
//   ❌ Touch events
//   ❌ Image updates (these are fast enough with just invalidate)
//
// Why? lv_refr_now() blocks for 50-200ms. When you stack multiple calls,
// you get multi-second delays. User-triggered events should rely on the
// main loop's lv_timer_handler() which is called every ~1ms.
//
// ============================================================================