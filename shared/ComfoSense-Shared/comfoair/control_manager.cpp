#include "control_manager.h"
#include "../mqtt/mqtt.h"

namespace comfoair {

ControlManager::ControlManager()
    : mqtt(nullptr),
      remote_client_mode(false),
      mqtt_prefix("comfoair"),
      current_fan_speed(2),
      speed_before_boost(2),
      current_temp_profile(255),
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
    current_temp_profile = 255;

    if (remote_client_mode) {
        Serial.println("ControlManager: Ready (Remote Client Mode - commands via MQTT)");
    } else {
        Serial.println("ControlManager: Ready (will send CAN commands when buttons pressed)");
    }
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
        if (boost_timer_display_callback) {
            boost_timer_display_callback(minutes_remaining);
        }
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
    if (boost_timer_display_callback) {
        boost_timer_display_callback(0);
    }

    // Update display
    updateDisplay();

    // Send command to MVHR
    sendFanSpeedCommand(current_fan_speed);
}

void ControlManager::updateDisplay() {
    if (fan_speed_display_callback) {
        fan_speed_display_callback(current_fan_speed, boost_timer_active);
    }

    if (boost_timer_active && boost_timer_display_callback) {
        int minutes = getRemainingBoostMinutes();
        boost_timer_display_callback(minutes);
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

        if (remote_client_mode) {
            pending_fan_speed = current_fan_speed;
            fan_speed_command_pending = true;
            last_fan_speed_change = millis();
            Serial.printf("ControlManager: Fan speed command pending (will send after %lu ms debounce)\n", COMMAND_DEBOUNCE_MS);
        } else {
            sendFanSpeedCommand(current_fan_speed);
        }
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

        if (remote_client_mode) {
            pending_fan_speed = current_fan_speed;
            fan_speed_command_pending = true;
            last_fan_speed_change = millis();
            Serial.printf("ControlManager: Fan speed command pending (will send after %lu ms debounce)\n", COMMAND_DEBOUNCE_MS);
        } else {
            sendFanSpeedCommand(current_fan_speed);
        }
    } else {
        Serial.println("ControlManager: Already at min speed (0)");
    }
}

void ControlManager::activateBoost() {
    if (boost_timer_active) {
        // Pressing boost again extends by 20 minutes
        Serial.println("ControlManager: Extending boost by 20 minutes");
        boost_end_time += BOOST_DURATION_MS;

        // Update display with new time
        int minutes = getRemainingBoostMinutes();
        if (boost_timer_display_callback) {
            boost_timer_display_callback(minutes);
        }
        Serial.printf("ControlManager: Boost extended to %d minutes\n", minutes);
    } else {
        // Start new boost - 20 minutes at speed 3
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
    if (temp_profile_display_callback) {
        temp_profile_display_callback(profile);
    }

    if (remote_client_mode) {
        pending_temp_profile = profile;
        temp_profile_command_pending = true;
        last_temp_profile_change = millis();
        Serial.printf("ControlManager: Temp profile command pending (will send after %lu ms debounce)\n", COMMAND_DEBOUNCE_MS);
    } else {
        sendTempProfileCommand(profile);
    }
}

// ============================================================================
// LOOP - Process Pending Commands AND Boost Timer
// ============================================================================

void ControlManager::loop() {
    // Update boost timer every loop iteration
    updateBoostTimer();

    // Process debounced commands in remote client mode
    if (remote_client_mode) {
        processPendingCommands();
    }
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
// CAN/MQTT FEEDBACK HANDLERS
// ============================================================================

void ControlManager::updateFanSpeedFromCAN(uint8_t speed) {
    last_can_feedback = millis();
    demo_mode = false;

    // Only update if speed actually changed AND we're not in an active boost
    // (During boost, we control the speed locally)
    if (!boost_timer_active && speed != current_fan_speed) {
        current_fan_speed = speed;
        speed_before_boost = speed;  // Update our baseline

        if (remote_client_mode) {
            Serial.printf("ControlManager: Fan speed updated from MQTT: %d\n", speed);

            if (fan_speed_command_pending && speed == pending_fan_speed) {
                Serial.println("ControlManager: Pending fan speed command confirmed - clearing");
                fan_speed_command_pending = false;
            }
        } else {
            Serial.printf("ControlManager: Fan speed updated from CAN: %d\n", speed);
        }

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

        if (remote_client_mode) {
            Serial.printf("ControlManager: Temperature profile updated from MQTT: %s\n",
                          profile_names[profile]);

            if (temp_profile_command_pending && profile == pending_temp_profile) {
                Serial.println("ControlManager: Pending temp profile command confirmed - clearing");
                temp_profile_command_pending = false;
            }
        } else {
            Serial.printf("ControlManager: Temperature profile updated from CAN: %s\n",
                          profile_names[profile]);
        }

        if (temp_profile_display_callback) {
            temp_profile_display_callback(profile);
        }
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

    if (remote_client_mode) {
        if (mqtt) {
            Serial.printf("ControlManager: Sending MQTT command: %s\n", commands[speed]);
            char topic[64];
            snprintf(topic, sizeof(topic), "%s/commands/%s", mqtt_prefix, commands[speed]);
            mqtt->writeToTopic(topic, "");
        }
    } else {
        if (command_send_callback) {
            Serial.printf("ControlManager: Sending CAN command: %s\n", commands[speed]);
            command_send_callback(commands[speed]);
        }
    }
}

void ControlManager::sendTempProfileCommand(uint8_t profile) {
    const char* commands[] = {
        "temp_profile_normal",
        "temp_profile_cool",
        "temp_profile_warm"
    };

    if (profile > 2) return;

    if (remote_client_mode) {
        if (mqtt) {
            Serial.printf("ControlManager: Sending MQTT command: %s\n", commands[profile]);
            char topic[64];
            snprintf(topic, sizeof(topic), "%s/commands/%s", mqtt_prefix, commands[profile]);
            mqtt->writeToTopic(topic, "");
        }
    } else {
        if (command_send_callback) {
            Serial.printf("ControlManager: Sending CAN command: %s\n", commands[profile]);
            command_send_callback(commands[profile]);
        }
    }
}

} // namespace comfoair
