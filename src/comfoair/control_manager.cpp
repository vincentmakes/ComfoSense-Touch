#include "control_manager.h"
#include "comfoair.h"
#include "../ui/GUI.h"
#include "../mqtt/mqtt.h"
#include "../secrets.h"

// C wrapper functions to interface with C GUI events
extern "C" {
    void GUI_update_fan_speed_display_from_cpp(uint8_t speed, bool boost);
    void GUI_update_temp_profile_display_from_cpp(uint8_t profile);
}

namespace comfoair {

ControlManager::ControlManager() 
    : comfoair(nullptr),
      mqtt(nullptr),
      current_fan_speed(2), 
      boost_active(false),
      current_temp_profile(0),
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
    
    // Start in demo mode (assumes no CAN until feedback received)
    demo_mode = false;
    current_fan_speed = 2;
    boost_active = false;
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
        
        // Update display immediately (NO Strategy 5 - user triggered!)
        GUI_update_fan_speed_display_from_cpp(current_fan_speed, boost_active);
        
        #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
            // In remote client mode: Mark command as pending, will be sent after 2s delay
            pending_fan_speed = current_fan_speed;
            fan_speed_command_pending = true;
            last_fan_speed_change = millis();
            Serial.printf("ControlManager: Fan speed command pending (will send after %d ms debounce)\n", COMMAND_DEBOUNCE_MS);
        #else
            // In normal mode: Send command immediately
            sendFanSpeedCommand(current_fan_speed);
        #endif
    } else {
        Serial.println("ControlManager: Already at max speed (3)");
    }
}

void ControlManager::decreaseFanSpeed() {
    if (current_fan_speed > 0) {
        current_fan_speed--;
        boost_active = false;
        
        Serial.printf("ControlManager: Decrease fan speed to %d\n", current_fan_speed);
        
        // Update display immediately (NO Strategy 5 - user triggered!)
        GUI_update_fan_speed_display_from_cpp(current_fan_speed, boost_active);
        
        #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
            // In remote client mode: Mark command as pending, will be sent after 2s delay
            pending_fan_speed = current_fan_speed;
            fan_speed_command_pending = true;
            last_fan_speed_change = millis();
            Serial.printf("ControlManager: Fan speed command pending (will send after %d ms debounce)\n", COMMAND_DEBOUNCE_MS);
        #else
            // In normal mode: Send command immediately
            sendFanSpeedCommand(current_fan_speed);
        #endif
    } else {
        Serial.println("ControlManager: Already at min speed (0)");
    }
}

void ControlManager::activateBoost() {
    boost_active = true;
    current_fan_speed = 3; // Track internally as speed 3
    
    Serial.println("ControlManager: Boost activated (20 min)");
    
    // Update display immediately (NO Strategy 5 - user triggered!)
    GUI_update_fan_speed_display_from_cpp(current_fan_speed, boost_active);
    
    // Send command (CAN or MQTT depending on mode)
    sendBoostCommand();
}

void ControlManager::setTempProfile(uint8_t profile) {
    if (profile > 2) profile = 0; // Safety check
    
    current_temp_profile = profile;
    
    const char* profile_names[] = {"NORMAL", "COOLING", "HEATING"};
    Serial.printf("ControlManager: Temperature profile set to %s\n", profile_names[profile]);
    
    // Update display (NO Strategy 5 - user triggered!)
    GUI_update_temp_profile_display_from_cpp(profile);
    
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        // In remote client mode: Mark command as pending, will be sent after 2s delay
        pending_temp_profile = profile;
        temp_profile_command_pending = true;
        last_temp_profile_change = millis();
        Serial.printf("ControlManager: Temp profile command pending (will send after %d ms debounce)\n", COMMAND_DEBOUNCE_MS);
    #else
        // In normal mode: Send command immediately
        sendTempProfileCommand(profile);
    #endif
}

// ============================================================================
// LOOP - Process Pending Commands (Debouncing for Remote Client Mode)
// ============================================================================

void ControlManager::loop() {
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        processPendingCommands();
    #endif
}

void ControlManager::processPendingCommands() {
    unsigned long now = millis();
    
    // Check if fan speed command should be sent
    if (fan_speed_command_pending) {
        if (now - last_fan_speed_change >= COMMAND_DEBOUNCE_MS) {
            Serial.printf("ControlManager: Debounce complete - sending fan speed command: %d\n", pending_fan_speed);
            sendFanSpeedCommand(pending_fan_speed);
            fan_speed_command_pending = false;
        }
    }
    
    // Check if temp profile command should be sent
    if (temp_profile_command_pending) {
        if (now - last_temp_profile_change >= COMMAND_DEBOUNCE_MS) {
            Serial.printf("ControlManager: Debounce complete - sending temp profile command: %d\n", pending_temp_profile);
            sendTempProfileCommand(pending_temp_profile);
            temp_profile_command_pending = false;
        }
    }
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
        
        #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
            Serial.printf("ControlManager: Fan speed updated from MQTT: %d\n", speed);
            
            // Clear any pending command if feedback matches it
            if (fan_speed_command_pending && speed == pending_fan_speed) {
                Serial.println("ControlManager: Pending fan speed command confirmed by feedback - clearing");
                fan_speed_command_pending = false;
            }
        #else
            Serial.printf("ControlManager: Fan speed updated from CAN: %d\n", speed);
        #endif
        
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
        
        #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
            Serial.printf("ControlManager: Boost state updated from MQTT: %s\n", 
                          active ? "ACTIVE" : "INACTIVE");
        #else
            Serial.printf("ControlManager: Boost state updated from CAN: %s\n", 
                          active ? "ACTIVE" : "INACTIVE");
        #endif
        
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
        
        #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
            Serial.printf("ControlManager: Temperature profile updated from MQTT: %s\n",
                          profile_names[profile]);
            
            // Clear any pending command if feedback matches it
            if (temp_profile_command_pending && profile == pending_temp_profile) {
                Serial.println("ControlManager: Pending temp profile command confirmed by feedback - clearing");
                temp_profile_command_pending = false;
            }
        #else
            Serial.printf("ControlManager: Temperature profile updated from CAN: %s\n",
                          profile_names[profile]);
        #endif
        
        // Update display
        GUI_update_temp_profile_display_from_cpp(profile);
       
    }
}

// ============================================================================
// COMMAND SENDERS (CAN or MQTT depending on configuration)
// ============================================================================

void ControlManager::sendFanSpeedCommand(uint8_t speed) {
    const char* commands[] = {
        "ventilation_level_0",
        "ventilation_level_1",
        "ventilation_level_2",
        "ventilation_level_3"
    };
    
    if (speed > 3) return; // Safety check
    
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        // ====================================================================
        // REMOTE CLIENT MODE - Send via MQTT
        // ====================================================================
        if (!mqtt) {
            Serial.println("ControlManager: ❌ Cannot send command - MQTT not linked");
            return;
        }
        
        char topic[64];
        snprintf(topic, sizeof(topic), "%s/commands/%s", MQTT_PREFIX, commands[speed]);
        
        Serial.printf("ControlManager: 📡 Sending MQTT command: %s\n", commands[speed]);
        mqtt->writeToTopic(topic, "");  // Empty payload for command topics
        
    #else
        // ====================================================================
        // NORMAL MODE - Send via CAN
        // ====================================================================
        if (!comfoair) {
            Serial.println("ControlManager: ❌ Cannot send command - ComfoAir not linked");
            return;
        }
        
        if (demo_mode) {
            Serial.println("ControlManager: ⭐️ Skipping CAN send (demo mode - no device connected)");
            return;
        }
        
        Serial.printf("ControlManager: 📡 Sending CAN command: %s\n", commands[speed]);
        comfoair->sendCommand(commands[speed]);
    #endif
}

void ControlManager::sendBoostCommand() {
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        // ====================================================================
        // REMOTE CLIENT MODE - Send via MQTT
        // ====================================================================
        if (!mqtt) {
            Serial.println("ControlManager: ❌ Cannot send command - MQTT not linked");
            return;
        }
        
        char topic[64];
        snprintf(topic, sizeof(topic), "%s/commands/boost_20_min", MQTT_PREFIX);
        
        Serial.println("ControlManager: 📡 Sending MQTT command: boost_20_min");
        mqtt->writeToTopic(topic, "");  // Empty payload for command topics
        
    #else
        // ====================================================================
        // NORMAL MODE - Send via CAN
        // ====================================================================
        if (!comfoair) {
            Serial.println("ControlManager: ❌ Cannot send command - ComfoAir not linked");
            return;
        }
        
        if (demo_mode) {
            Serial.println("ControlManager: ⭐️ Skipping CAN send (demo mode - no device connected)");
            return;
        }
        
        Serial.println("ControlManager: 📡 Sending CAN command: boost_20_min");
        comfoair->sendCommand("boost_20_min");
    #endif
}

void ControlManager::sendTempProfileCommand(uint8_t profile) {
    const char* commands[] = {
        "temp_profile_normal",
        "temp_profile_cool",
        "temp_profile_warm"
    };
    
    if (profile > 2) return; // Safety check
    
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        // ====================================================================
        // REMOTE CLIENT MODE - Send via MQTT
        // ====================================================================
        if (!mqtt) {
            Serial.println("ControlManager: ❌ Cannot send command - MQTT not linked");
            return;
        }
        
        char topic[64];
        snprintf(topic, sizeof(topic), "%s/commands/%s", MQTT_PREFIX, commands[profile]);
        
        Serial.printf("ControlManager: 📡 Sending MQTT command: %s\n", commands[profile]);
        mqtt->writeToTopic(topic, "");  // Empty payload for command topics
        
    #else
        // ====================================================================
        // NORMAL MODE - Send via CAN
        // ====================================================================
        if (!comfoair) {
            Serial.println("ControlManager: ❌ Cannot send command - ComfoAir not linked");
            return;
        }
        
        if (demo_mode) {
            Serial.println("ControlManager: ⭐️ Skipping CAN send (demo mode - no device connected)");
            return;
        }
        
        Serial.printf("ControlManager: 📡 Sending CAN command: %s\n", commands[profile]);
        comfoair->sendCommand(commands[profile]);
    #endif
}

} // namespace comfoair

// ============================================================================
// C WRAPPER FUNCTIONS (called from C++ to update C GUI)
// ============================================================================

extern "C" {


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
    

    GUI_request_display_refresh();

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

}

}