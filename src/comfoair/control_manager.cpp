#include "control_manager.h"
#include "comfoair.h"
#include "../ui/GUI.h"
#include "../mqtt/mqtt.h"
#include "../secrets.h"

// C wrapper functions to interface with C GUI events
extern "C" {
    void GUI_update_fan_speed_display_from_cpp(uint8_t speed, bool boost);
    void GUI_update_temp_profile_display_from_cpp(uint8_t profile);
    void GUI_update_boost_timer_display(int minutes_remaining);
}

namespace comfoair {

ControlManager::ControlManager() 
    : comfoair(nullptr),
      mqtt(nullptr),
      current_fan_speed(2), 
      speed_before_boost(2),
      current_temp_profile(0),
      boost_timer_active(false),
      boost_end_time(0),
      last_timer_update(0),
      demo_mode(true),
      last_can_feedback(0),
      last_fan_speed_change(0),
      last_temp_profile_change(0),
      pending_fan_speed(2),
      pending_temp_profile(0),
      fan_speed_command_pending(false),
      temp_profile_command_pending(false) {
}

void ControlManager::setup() {
    Serial.println("ControlManager: Initializing...");
    
    demo_mode = false;
    current_fan_speed = 2;
    speed_before_boost = 2;
    boost_timer_active = false;
    boost_end_time = 0;
    current_temp_profile = 0;
    
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        Serial.println("ControlManager: Ready (Remote Client Mode - commands via MQTT)");
    #else
        Serial.println("ControlManager: Ready (will send CAN commands when buttons pressed)");
    #endif
}

void ControlManager::setComfoAir(ComfoAir* comfo) {
    comfoair = comfo;
    Serial.println("ControlManager: ComfoAir linked");
}

void ControlManager::setMQTT(MQTT* mqtt_client) {
    mqtt = mqtt_client;
    Serial.println("ControlManager: MQTT linked for remote command sending");
}

bool ControlManager::isDemoMode() {
    if (last_can_feedback == 0) return true;
    return (millis() - last_can_feedback) > CAN_TIMEOUT;
}

int ControlManager::getRemainingBoostMinutes() {
    if (!boost_timer_active) return 0;
    
    unsigned long now = millis();
    if (now >= boost_end_time) return 0;
    
    unsigned long remaining_ms = boost_end_time - now;
    int minutes = (remaining_ms + 59999) / 60000;  // Round up to next minute
    return minutes;
}

// ============================================================================
//  BOOST TIMER MANAGEMENT (completely local to ESP32)
// ============================================================================

void ControlManager::updateBoostTimer() {
    if (!boost_timer_active) return;
    
    unsigned long now = millis();
    
    // Check if boost timer expired
    if (now >= boost_end_time) {
        Serial.println("ControlManager: Boost timer expired");
        endBoost();
        return;
    }
    
    // Update display every minute (60 seconds)
    if (now - last_timer_update >= 60000) {
        last_timer_update = now;
        int minutes_remaining = getRemainingBoostMinutes();
        GUI_update_boost_timer_display(minutes_remaining);
        Serial.printf("ControlManager: Boost timer - %d minutes remaining\n", minutes_remaining);
    }
}

void ControlManager::endBoost() {
    if (!boost_timer_active) return;
    
    boost_timer_active = false;
    boost_end_time = 0;
    
    Serial.printf("ControlManager: Boost ended, returning to speed %d\n", speed_before_boost);
    
    // Return to previous speed
    current_fan_speed = speed_before_boost;
    
    // Hide the timer display
    GUI_update_boost_timer_display(0);
    
    // Update display
    updateDisplay();
    
    // Send command to MVHR
    sendFanSpeedCommand(current_fan_speed);
}

void ControlManager::updateDisplay() {
    GUI_update_fan_speed_display_from_cpp(current_fan_speed, boost_timer_active);
    
    if (boost_timer_active) {
        int minutes = getRemainingBoostMinutes();
        GUI_update_boost_timer_display(minutes);
    }
}

// ============================================================================
// BUTTON COMMAND TRIGGERS (called from GUI)
// ============================================================================

void ControlManager::increaseFanSpeed() {
    // Manual interaction cancels boost
    if (boost_timer_active) {
        Serial.println("ControlManager: Manual speed change - cancelling boost");
        endBoost();
        return;  // endBoost() already updates display and sends command
    }
    
    if (current_fan_speed < 3) {
        current_fan_speed++;
        speed_before_boost = current_fan_speed;  // Remember for future boost
        
        Serial.printf("ControlManager: Increase fan speed to %d\n", current_fan_speed);
        
        // Update display immediately
        updateDisplay();
        
        #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
            pending_fan_speed = current_fan_speed;
            fan_speed_command_pending = true;
            last_fan_speed_change = millis();
            Serial.printf("ControlManager: Fan speed command pending (will send after %d ms debounce)\n", COMMAND_DEBOUNCE_MS);
        #else
            sendFanSpeedCommand(current_fan_speed);
        #endif
    } else {
        Serial.println("ControlManager: Already at max speed (3)");
    }
}

void ControlManager::decreaseFanSpeed() {
    // Manual interaction cancels boost
    if (boost_timer_active) {
        Serial.println("ControlManager: Manual speed change - cancelling boost");
        endBoost();
        return;  // endBoost() already updates display and sends command
    }
    
    if (current_fan_speed > 0) {
        current_fan_speed--;
        speed_before_boost = current_fan_speed;  // Remember for future boost
        
        Serial.printf("ControlManager: Decrease fan speed to %d\n", current_fan_speed);
        
        // Update display immediately
        updateDisplay();
        
        #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
            pending_fan_speed = current_fan_speed;
            fan_speed_command_pending = true;
            last_fan_speed_change = millis();
            Serial.printf("ControlManager: Fan speed command pending (will send after %d ms debounce)\n", COMMAND_DEBOUNCE_MS);
        #else
            sendFanSpeedCommand(current_fan_speed);
        #endif
    } else {
        Serial.println("ControlManager: Already at min speed (0)");
    }
}

void ControlManager::activateBoost() {
    if (boost_timer_active) {
        // FEATURE 4: Pressing boost again extends by 20 minutes
        Serial.println("ControlManager: Extending boost by 20 minutes");
        boost_end_time += BOOST_DURATION_MS;
        
        // Update display with new time
        int minutes = getRemainingBoostMinutes();
        GUI_update_boost_timer_display(minutes);
        Serial.printf("ControlManager: Boost extended to %d minutes\n", minutes);
    } else {
        // FEATURE 1 & 2: Start new boost - 20 minutes at speed 3
        Serial.println("ControlManager: Activating boost (20 minutes at speed 3)");
        
        // Save current speed to return to later
        speed_before_boost = current_fan_speed;
        
        // Set to speed 3
        current_fan_speed = 3;
        
        // Start timer
        boost_timer_active = true;
        boost_end_time = millis() + BOOST_DURATION_MS;
        last_timer_update = millis();
        
        // Update display
        updateDisplay();
        
        // Send speed 3 command to MVHR
        sendFanSpeedCommand(3);
        
        Serial.printf("ControlManager: Boost started - will return to speed %d after 20 minutes\n", speed_before_boost);
    }
}

void ControlManager::setTempProfile(uint8_t profile) {
    // Manual interaction cancels boost
    if (boost_timer_active) {
        Serial.println("ControlManager: Manual profile change - cancelling boost");
        endBoost();
    }
    
    if (profile > 2) profile = 0;
    
    current_temp_profile = profile;
    
    const char* profile_names[] = {"NORMAL", "COOLING", "HEATING"};
    Serial.printf("ControlManager: Temperature profile set to %s\n", profile_names[profile]);
    
    // Update display immediately
    GUI_update_temp_profile_display_from_cpp(profile);
    
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        pending_temp_profile = profile;
        temp_profile_command_pending = true;
        last_temp_profile_change = millis();
        Serial.printf("ControlManager: Temp profile command pending (will send after %d ms debounce)\n", COMMAND_DEBOUNCE_MS);
    #else
        sendTempProfileCommand(profile);
    #endif
}

// ============================================================================
// LOOP - Process Pending Commands AND Boost Timer
// ============================================================================

void ControlManager::loop() {
    // Update boost timer every loop iteration
    updateBoostTimer();
    
    // Process debounced commands in remote client mode
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        processPendingCommands();
    #endif
}

void ControlManager::processPendingCommands() {
    unsigned long now = millis();
    
    if (fan_speed_command_pending) {
        if (now - last_fan_speed_change >= COMMAND_DEBOUNCE_MS) {
            Serial.printf("ControlManager: Debounce complete - sending fan speed command: %d\n", pending_fan_speed);
            sendFanSpeedCommand(pending_fan_speed);
            fan_speed_command_pending = false;
        }
    }
    
    if (temp_profile_command_pending) {
        if (now - last_temp_profile_change >= COMMAND_DEBOUNCE_MS) {
            Serial.printf("ControlManager: Debounce complete - sending temp profile command: %d\n", pending_temp_profile);
            sendTempProfileCommand(pending_temp_profile);
            temp_profile_command_pending = false;
        }
    }
}

// ============================================================================
// CAN FEEDBACK HANDLERS
// ============================================================================

void ControlManager::updateFanSpeedFromCAN(uint8_t speed) {
    last_can_feedback = millis();
    demo_mode = false;
    
    // Only update if speed actually changed AND we're not in an active boost
    // (During boost, we control the speed locally)
    if (!boost_timer_active && speed != current_fan_speed) {
        current_fan_speed = speed;
        speed_before_boost = speed;  // Update our baseline
        
        #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
            Serial.printf("ControlManager: Fan speed updated from MQTT: %d\n", speed);
            
            if (fan_speed_command_pending && speed == pending_fan_speed) {
                Serial.println("ControlManager: Pending fan speed command confirmed - clearing");
                fan_speed_command_pending = false;
            }
        #else
            Serial.printf("ControlManager: Fan speed updated from CAN: %d\n", speed);
        #endif
        
        // Update display
        updateDisplay();
    }
}

void ControlManager::updateTempProfileFromCAN(uint8_t profile) {
    last_can_feedback = millis();
    demo_mode = false;
    
    if (profile != current_temp_profile) {
        current_temp_profile = profile;
        
        const char* profile_names[] = {"NORMAL", "COOLING", "HEATING"};
        
        #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
            Serial.printf("ControlManager: Temperature profile updated from MQTT: %s\n",
                          profile_names[profile]);
            
            if (temp_profile_command_pending && profile == pending_temp_profile) {
                Serial.println("ControlManager: Pending temp profile command confirmed - clearing");
                temp_profile_command_pending = false;
            }
        #else
            Serial.printf("ControlManager: Temperature profile updated from CAN: %s\n",
                          profile_names[profile]);
        #endif
        
        GUI_update_temp_profile_display_from_cpp(profile);
    }
}

// ============================================================================
// COMMAND SENDERS
// ============================================================================

void ControlManager::sendFanSpeedCommand(uint8_t speed) {
    const char* commands[] = {
        "ventilation_level_0",
        "ventilation_level_1",
        "ventilation_level_2",
        "ventilation_level_3"
    };
    
    if (speed > 3) return;
    
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        if (mqtt) {
            Serial.printf("ControlManager: Sending MQTT command: %s\n", commands[speed]);
            char topic[50];
            snprintf(topic, sizeof(topic), MQTT_PREFIX "/commands/%s", commands[speed]);
            mqtt->writeToTopic(topic, "");
        }
    #else
        if (comfoair) {
            Serial.printf("ControlManager: Sending CAN command: %s\n", commands[speed]);
            comfoair->sendCommand(commands[speed]);
        }
    #endif
}

void ControlManager::sendTempProfileCommand(uint8_t profile) {
    const char* commands[] = {
        "temp_profile_normal",
        "temp_profile_cool",
        "temp_profile_warm"
    };
    
    if (profile > 2) return;
    
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        if (mqtt) {
            Serial.printf("ControlManager: Sending MQTT command: %s\n", commands[profile]);
            char topic[50];
            snprintf(topic, sizeof(topic), MQTT_PREFIX "/commands/%s", commands[profile]);
            mqtt->writeToTopic(topic, "");
        }
    #else
        if (comfoair) {
            Serial.printf("ControlManager: Sending CAN command: %s\n", commands[profile]);
            comfoair->sendCommand(commands[profile]);
        }
    #endif
}

} // namespace comfoair

// ============================================================================
// C WRAPPER FUNCTIONS (called from C++ to update C GUI)
// ============================================================================

extern "C" {

void GUI_update_fan_speed_display_from_cpp(uint8_t speed, bool boost) {
    Serial.printf("GUI: Updating fan speed display - speed=%d, boost=%d\n", speed, boost);
    
    // 1. Hide all fan speed images first
    lv_obj_set_style_opa(GUI_Image__screen__fanspeed0, LV_OPA_0, 0);
    lv_obj_set_style_opa(GUI_Image__screen__fanspeed1, LV_OPA_0, 0);
    lv_obj_set_style_opa(GUI_Image__screen__fanspeed2, LV_OPA_0, 0);
    lv_obj_set_style_opa(GUI_Image__screen__fanspeed3, LV_OPA_0, 0);
    lv_obj_set_style_opa(GUI_Image__screen__fanspeedboost, LV_OPA_0, 0);
    
    // 2. Show the appropriate image based on state
    lv_obj_t* active_image = nullptr;
    
    if (boost) {
        // FEATURE 5: Show boost fan image during boost
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
    
    // 3. Request display refresh
    GUI_request_display_refresh();
    
    // 4. Invalidate the objects
    lv_obj_t* parent = lv_obj_get_parent(GUI_Image__screen__fanspeed0);
    if (parent) {
        lv_obj_invalidate(parent);
    }
    if (active_image) {
        lv_obj_invalidate(active_image);
    }
    
    Serial.println("GUI: Fan speed display updated");
}

void GUI_update_temp_profile_display_from_cpp(uint8_t profile) {
    // Temperature profile display update logic
    Serial.printf("GUI: Update temp profile display to %d\n", profile);
    // TODO: Implement temperature profile display updates if needed
}

void GUI_update_boost_timer_display(int minutes_remaining) {
    // âœ… FEATURE 2: Display minutes remaining in centerfan placeholder
    // Small font, white color, no background
    
    if (minutes_remaining > 0) {
        char timer_text[4];
        if (minutes_remaining > 99) minutes_remaining = 99;  // Cap at 99
        snprintf(timer_text, sizeof(timer_text), "%02d", minutes_remaining);
        
        lv_label_set_text(GUI_Label__screen__boosttimer, timer_text);
        
        // Set style: small font (defaultsmall_1), white color
        lv_obj_set_style_text_font(GUI_Label__screen__boosttimer, &defaultsmall_1, 0);
        lv_obj_set_style_text_color(GUI_Label__screen__boosttimer, lv_color_hex(0xFFFFFF), 0);
        
        // Make visible
        lv_obj_clear_flag(GUI_Label__screen__boosttimer, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Timer expired or not active - hide the label
        lv_obj_add_flag(GUI_Label__screen__boosttimer, LV_OBJ_FLAG_HIDDEN);
    }
    
    lv_obj_invalidate(GUI_Label__screen__boosttimer);
}

}  // extern "C"