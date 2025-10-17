#include "control_manager.h"
#include "comfoair.h"
#include "../ui/GUI.h"

// C wrapper functions to interface with C GUI events
extern "C" {
    void GUI_update_fan_speed_display_from_cpp(uint8_t speed, bool boost);
    void GUI_update_temp_profile_display_from_cpp(uint8_t profile);
}

namespace comfoair {

ControlManager::ControlManager() 
    : comfoair(nullptr), 
      current_fan_speed(2), 
      boost_active(false),
      current_temp_profile(0),
      demo_mode(true),
      last_can_feedback(0) {
}

void ControlManager::setup() {
    Serial.println("ControlManager: Initializing...");
    
    // Start in demo mode (assumes no CAN until feedback received)
    demo_mode = false;
    current_fan_speed = 2;
    boost_active = false;
    current_temp_profile = 0;
    
    Serial.println("ControlManager: Ready (will send CAN commands when buttons pressed)");
}

void ControlManager::setComfoAir(ComfoAir* comfo) {
    comfoair = comfo;
    Serial.println("ControlManager: ComfoAir linked");
}

bool ControlManager::isDemoMode() {
    // If we haven't received CAN feedback in CAN_TIMEOUT ms, we're in demo mode
    if (last_can_feedback == 0) return true;
    return (millis() - last_can_feedback) > CAN_TIMEOUT;
}

// ============================================================================
// BUTTON COMMAND TRIGGERS (called from GUI)
// ============================================================================

void ControlManager::increaseFanSpeed() {
    if (current_fan_speed < 3) {
        current_fan_speed++;
        boost_active = false;
        
        Serial.printf("ControlManager: Increase fan speed to %d\n", current_fan_speed);
        
        // ✅ Update display immediately (NO Strategy 5 - user triggered!)
        GUI_update_fan_speed_display_from_cpp(current_fan_speed, boost_active);
        
        // ✅ Send CAN command
        sendFanSpeedCommand(current_fan_speed);
    } else {
        Serial.println("ControlManager: Already at max speed (3)");
    }
}

void ControlManager::decreaseFanSpeed() {
    if (current_fan_speed > 0) {
        current_fan_speed--;
        boost_active = false;
        
        Serial.printf("ControlManager: Decrease fan speed to %d\n", current_fan_speed);
        
        // ✅ Update display immediately (NO Strategy 5 - user triggered!)
        GUI_update_fan_speed_display_from_cpp(current_fan_speed, boost_active);
        
        // ✅ Send CAN command
        sendFanSpeedCommand(current_fan_speed);
    } else {
        Serial.println("ControlManager: Already at min speed (0)");
    }
}

void ControlManager::activateBoost() {
    boost_active = true;
    current_fan_speed = 3; // Track internally as speed 3
    
    Serial.println("ControlManager: Boost activated (20 min)");
    
    // ✅ Update display immediately (NO Strategy 5 - user triggered!)
    GUI_update_fan_speed_display_from_cpp(current_fan_speed, boost_active);
    
    // ✅ Send CAN command
    sendBoostCommand();
}

void ControlManager::setTempProfile(uint8_t profile) {
    if (profile > 2) profile = 0; // Safety check
    
    current_temp_profile = profile;
    
    const char* profile_names[] = {"NORMAL", "COOLING", "HEATING"};
    Serial.printf("ControlManager: Temperature profile set to %s\n", profile_names[profile]);
    
    // ✅ Update display (NO Strategy 5 - user triggered!)
    GUI_update_temp_profile_display_from_cpp(profile);
    
    // ✅ Send CAN command
    sendTempProfileCommand(profile);
}

// ============================================================================
// CAN FEEDBACK HANDLERS (called when CAN messages received)
// ============================================================================

void ControlManager::updateFanSpeedFromCAN(uint8_t speed) {
    last_can_feedback = millis();
    demo_mode = false;
    
    // Only update if different from current state
    if (speed != current_fan_speed || boost_active) {
        current_fan_speed = speed;
        boost_active = false; // CAN fan speed message means not in boost
        
        Serial.printf("ControlManager: Fan speed updated from CAN: %d\n", speed);
        
        // Update display
        GUI_update_fan_speed_display_from_cpp(current_fan_speed, boost_active);
    }
}

void ControlManager::updateBoostStateFromCAN(bool active) {
    last_can_feedback = millis();
    demo_mode = false;
    
    // Only update if different from current state
    if (active != boost_active) {
        boost_active = active;
        if (active) {
            current_fan_speed = 3;
        }
        
        Serial.printf("ControlManager: Boost state updated from CAN: %s\n", 
                      active ? "ACTIVE" : "INACTIVE");
        
        // Update display
        GUI_update_fan_speed_display_from_cpp(current_fan_speed, boost_active);
    }
}

void ControlManager::updateTempProfileFromCAN(uint8_t profile) {
    last_can_feedback = millis();
    demo_mode = false;
    
    // Only update if different from current state
    if (profile != current_temp_profile) {
        current_temp_profile = profile;
        
        const char* profile_names[] = {"NORMAL", "COOLING", "HEATING"};
        Serial.printf("ControlManager: Temperature profile updated from CAN: %s\n",
                      profile_names[profile]);
        
        // Update display
        GUI_update_temp_profile_display_from_cpp(profile);
       
    }
}

// ============================================================================
// CAN COMMAND SENDERS
// ============================================================================

void ControlManager::sendFanSpeedCommand(uint8_t speed) {
    if (!comfoair) {
        Serial.println("ControlManager: ❌ Cannot send command - ComfoAir not linked");
        return;
    }
    
    if (demo_mode) {
        Serial.println("ControlManager: ⏭️ Skipping CAN send (demo mode - no device connected)");
        return;
    }
    
    const char* commands[] = {
        "ventilation_level_0",
        "ventilation_level_1",
        "ventilation_level_2",
        "ventilation_level_3"
    };
    
    if (speed <= 3) {
        Serial.printf("ControlManager: 📡 Sending CAN command: %s\n", commands[speed]);
        comfoair->sendCommand(commands[speed]);
    }
}

void ControlManager::sendBoostCommand() {
    if (!comfoair) {
        Serial.println("ControlManager: ❌ Cannot send command - ComfoAir not linked");
        return;
    }
    
    if (demo_mode) {
        Serial.println("ControlManager: ⏭️ Skipping CAN send (demo mode - no device connected)");
        return;
    }
    
    Serial.println("ControlManager: 📡 Sending CAN command: boost_20_min");
    comfoair->sendCommand("boost_20_min");
}

void ControlManager::sendTempProfileCommand(uint8_t profile) {
    if (!comfoair) {
        Serial.println("ControlManager: ❌ Cannot send command - ComfoAir not linked");
        return;
    }
    
    if (demo_mode) {
        Serial.println("ControlManager: ⏭️ Skipping CAN send (demo mode - no device connected)");
        return;
    }
    
    const char* commands[] = {
        "temp_profile_normal",
        "temp_profile_cool",
        "temp_profile_warm"
    };
    
    if (profile <= 2) {
        Serial.printf("ControlManager: 📡 Sending CAN command: %s\n", commands[profile]);
        comfoair->sendCommand(commands[profile]);
    }
}

} // namespace comfoair

// ============================================================================
// C WRAPPER FUNCTIONS (called from C++ to update C GUI)
// ============================================================================

extern "C" {

// ✅ OPTIMIZED: Images update instantly WITHOUT Strategy 5!
// User-triggered events (buttons) should NOT use GUI_request_display_refresh()
// Only background updates (like TimeManager) need Strategy 5
void GUI_update_fan_speed_display_from_cpp(uint8_t speed, bool boost) {
    // Hide all fan speed images first
    lv_obj_set_style_opa(GUI_Image__screen__fanspeed0, LV_OPA_0, 0);
    lv_obj_set_style_opa(GUI_Image__screen__fanspeed1, LV_OPA_0, 0);
    lv_obj_set_style_opa(GUI_Image__screen__fanspeed2, LV_OPA_0, 0);
    lv_obj_set_style_opa(GUI_Image__screen__fanspeed3, LV_OPA_0, 0);
    lv_obj_set_style_opa(GUI_Image__screen__fanspeedboost, LV_OPA_0, 0);
    
    // Show the appropriate image based on state
    lv_obj_t* active_image = nullptr;
    
    if (boost) {
        active_image = GUI_Image__screen__fanspeedboost;
    } else {
        switch (speed) {
            case 0: active_image = GUI_Image__screen__fanspeed0; break;
            case 1: active_image = GUI_Image__screen__fanspeed1; break;
            case 2: active_image = GUI_Image__screen__fanspeed2; break;
            case 3: active_image = GUI_Image__screen__fanspeed3; break;
        }
    }
    
    if (active_image) {
        lv_obj_set_style_opa(active_image, LV_OPA_COVER, 0);
    }
    
    // ✅ NO GUI_request_display_refresh() - images update on next lv_timer_handler()
    // This is INSTANT because main loop calls lv_timer_handler() every 1ms
    GUI_request_display_refresh();
    // Just invalidate - LVGL will handle the rest
    lv_obj_t* parent = lv_obj_get_parent(GUI_Image__screen__fanspeed0);
    if (parent) {
        lv_obj_invalidate(parent);
    }
    if (active_image) {
        lv_obj_invalidate(active_image);
    }
}

void GUI_update_temp_profile_display_from_cpp(uint8_t profile) {
    // Temperature profile display update logic
    Serial.printf("GUI: Update temp profile display to %d\n", profile);
    
    // If you have visual elements to update, just invalidate them
    // NO GUI_request_display_refresh() needed for user-triggered events!
}

}