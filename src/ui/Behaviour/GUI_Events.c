#include "../GUI.h"

#ifdef GUI_EXTERNAL_CUSTOM_CALLBACK_FUNCTION_FILE
 #include GUI_EXTERNAL_CUSTOM_CALLBACK_FUNCTION_FILE
#endif

#include <stdio.h>
//#include "../../mqtt/mqtt.h"
//#include "../../secrets.h"

//extern comfoair::MQTT *mqtt;

// Current fan speed (0-3)
static uint8_t current_fan_speed = 2;  // Start at speed 2
static bool boost_active = false;

// Deferred refresh flag - set by events, processed in main loop
static volatile bool refresh_requested = false;

// Function to update fan speed visuals
static void update_fan_speed_display(uint8_t speed, bool boost) {
    printf("Updating fan display: speed=%d, boost=%d\n", speed, boost);
    
    // Hide all fan speed images first (use LV_OPA_0 = fully transparent)
    lv_obj_set_style_opa(GUI_Image__screen__fanspeed0, LV_OPA_0, 0);
    lv_obj_set_style_opa(GUI_Image__screen__fanspeed1, LV_OPA_0, 0);
    lv_obj_set_style_opa(GUI_Image__screen__fanspeed2, LV_OPA_0, 0);
    lv_obj_set_style_opa(GUI_Image__screen__fanspeed3, LV_OPA_0, 0);
    lv_obj_set_style_opa(GUI_Image__screen__fanspeedboost, LV_OPA_0, 0);
    
    printf("All images hidden\n");
    
    lv_obj_t *active_image = NULL;
    
    // If boost is active, show boost image (LV_OPA_COVER = fully opaque)
    if (boost) {
        lv_obj_set_style_opa(GUI_Image__screen__fanspeedboost, LV_OPA_COVER, 0);
        active_image = GUI_Image__screen__fanspeedboost;
        printf("Fan mode: BOOST (image set to opaque)\n");
    } else {
        // Otherwise show the current speed
        switch(speed) {
            case 0:
                lv_obj_set_style_opa(GUI_Image__screen__fanspeed0, LV_OPA_COVER, 0);
                active_image = GUI_Image__screen__fanspeed0;
                printf("Fan speed set to: 0 (image set to opaque)\n");
                break;
            case 1:
                lv_obj_set_style_opa(GUI_Image__screen__fanspeed1, LV_OPA_COVER, 0);
                active_image = GUI_Image__screen__fanspeed1;
                printf("Fan speed set to: 1 (image set to opaque)\n");
                break;
            case 2:
                lv_obj_set_style_opa(GUI_Image__screen__fanspeed2, LV_OPA_COVER, 0);
                active_image = GUI_Image__screen__fanspeed2;
                printf("Fan speed set to: 2 (image set to opaque)\n");
                break;
            case 3:
                lv_obj_set_style_opa(GUI_Image__screen__fanspeed3, LV_OPA_COVER, 0);
                active_image = GUI_Image__screen__fanspeed3;
                printf("Fan speed set to: 3 (image set to opaque)\n");
                break;
        }
    }
    
    // Invalidate for redraw (but don't force immediate refresh)
    if (active_image) {
        lv_obj_invalidate(active_image);
        lv_obj_t *parent = lv_obj_get_parent(active_image);
        if (parent) {
            lv_obj_invalidate(parent);
        }
    }
    
    // Request deferred refresh instead of immediate
    GUI_request_display_refresh();
    
    printf("Display refresh requested\n");
}

/*
static void send_fan_speed_command(uint8_t speed) {
    if (!mqtt) {
        printf("MQTT not ready; fan speed command not sent\n");
        return;
    }

    const char topic[] = MQTT_PREFIX "/commands/ventilation_level";
    char payload[] = { (char)('0' + speed), '\0' };
    mqtt->writeToTopic(topic, payload);
    printf("Fan speed command published: %d\n", speed);
}
*/

// Initialize function - call this once at startup to set initial display
void GUI_init_fan_speed_display(void) {
    update_fan_speed_display(current_fan_speed, boost_active);
    printf("Fan speed display initialized to: %d\n", current_fan_speed);
}


void GUI_event__Button__screen__BSpeedPlus__Clicked (lv_event_t* event) {
    printf("BSpeedPlus button clicked! Current speed: %d\n", current_fan_speed);
    
    // Increase speed (max 3)
    if (current_fan_speed < 3) {
        current_fan_speed++;
        boost_active = false;  // Exit boost mode
        printf("Increasing to speed: %d\n", current_fan_speed);
        update_fan_speed_display(current_fan_speed, boost_active);
       // send_fan_speed_command(current_fan_speed);
    } else {
        printf("Already at max speed (3)\n");
    }
}


void GUI_event__Button__screen__BSpeedMinus__Clicked (lv_event_t* event) {
    printf("BSpeedMinus button clicked! Current speed: %d\n", current_fan_speed);
    
    // Decrease speed (min 0)
    if (current_fan_speed > 0) {
        current_fan_speed--;
        boost_active = false;  // Exit boost mode
        printf("Decreasing to speed: %d\n", current_fan_speed);
        update_fan_speed_display(current_fan_speed, boost_active);
       // send_fan_speed_command(current_fan_speed);
    } else {
        printf("Already at min speed (0)\n");
    }
}


void GUI_event__Button__screen__buttonspeedboost__Clicked (lv_event_t* event) {
    // Activate boost mode (separate command, shows boost image)
    boost_active = true;
    current_fan_speed = 3;  // Internally track as speed 3
    update_fan_speed_display(current_fan_speed, boost_active);
    
    // TODO: Send boost command here (with timing handled separately)
    printf("Boost command activated!\n");
}


void GUI_event__Dropdown__screen__modedropdown__Value_Changed (lv_event_t* event) {
    lv_obj_t *dropdown = (lv_obj_t*)lv_event_get_target(event);
    uint16_t sel = lv_dropdown_get_selected(dropdown);
    
    char selected_text[32];
    lv_dropdown_get_selected_str(dropdown, selected_text, sizeof(selected_text));
    
    printf("Mode changed to: %d (%s)\n", sel, selected_text);
    // 0 = NORMAL, 1 = HEATING, 2 = COOLING
    
    // Invalidate the dropdown for redraw
    lv_obj_invalidate(dropdown);
    
    // Close the dropdown list after selection
    lv_dropdown_close(dropdown);
    
    // Request deferred refresh instead of immediate
    GUI_request_display_refresh();
    
    printf("Dropdown display update requested\n");
    
    // TODO: Send mode change command here
}


// Dropdown list styling callback - regular C function
static void dropdown_list_styling_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_t *target = (lv_obj_t*)lv_event_get_target(e);
        lv_obj_t *list = lv_dropdown_get_list(target);
        if (list) {
            printf("Dropdown list pointer: %p\n", list);
            
            // Get list position
            lv_area_t coords;
            lv_obj_get_coords(list, &coords);
            printf("List coords: x1=%d, y1=%d, x2=%d, y2=%d\n", 
                   coords.x1, coords.y1, coords.x2, coords.y2);
            
            // Remove hidden flag and scrollbars
            lv_obj_remove_flag(list, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(list, LV_OBJ_FLAG_CLICKABLE);
            
            // Hide scrollbars completely
            lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_OFF);
            
            // Apply proper styling matching the dropdown button colors
            // Background: cyan/blue (#00C7FF)
            lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(list, lv_color_hex(0x00C7FF), 0);
            
            // Text: white with larger font (largebold_1 matches dropdown button)
            lv_obj_set_style_text_color(list, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_text_font(list, &largebold_1, 0);
            
            // Selected item: yellow (#FFC802)
            lv_obj_set_style_bg_color(list, lv_color_hex(0xFFC802), LV_PART_SELECTED);
            lv_obj_set_style_bg_opa(list, LV_OPA_COVER, LV_PART_SELECTED);
            
            // Border: 1px white
            lv_obj_set_style_border_width(list, 1, 0);
            lv_obj_set_style_border_color(list, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_opa(list, LV_OPA_COVER, 0);
            lv_obj_set_style_border_side(list, LV_BORDER_SIDE_FULL, 0);
            
            // Padding for better readability (increased for larger font)
            lv_obj_set_style_pad_all(list, 15, 0);
            
            // Line spacing for better readability with larger font
            lv_obj_set_style_text_line_space(list, 15, 0);
            
            // Rounded corners to match button
            lv_obj_set_style_radius(list, 20, 0);
            
            // Try to force it to the foreground
            lv_obj_move_foreground(list);
            lv_obj_invalidate(list);
            
            // Force parent to redraw
            lv_obj_t *parent = lv_obj_get_parent(list);
            if (parent) {
                lv_obj_invalidate(parent);
            }
            
            // Request deferred refresh
            GUI_request_display_refresh();
            
            printf("Dropdown list styled and refresh requested\n");
        } else {
            printf("Dropdown list is NULL!\n");
        }
    }
}


// Dropdown list styling - call this once after GUI init
void GUI_init_dropdown_styling(void) {
    if (GUI_Dropdown__screen__modedropdown) {
        // Add event callback to style dropdown list when opened
        lv_obj_add_event_cb(GUI_Dropdown__screen__modedropdown, 
                           dropdown_list_styling_cb, 
                           LV_EVENT_CLICKED, 
                           NULL);
        printf("Dropdown visibility fix installed\n");
    }
}

// Request a display refresh (called from event callbacks)
void GUI_request_display_refresh(void) {
    refresh_requested = true;
}

// Process pending display refresh (called from main loop)
void GUI_process_display_refresh(void) {
    if (refresh_requested) {
        refresh_requested = false;
        lv_refr_now(NULL);
    }
}