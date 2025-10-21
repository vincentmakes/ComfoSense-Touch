#include "screen_manager.h"
#include "../secrets.h"
#include <time.h>

namespace comfoair {

ScreenManager::ScreenManager() 
    : backlight_control(nullptr),
      ntm_enabled(NTM_ENABLED),
      permanent_ntm(NTM_PERMANENT),
      ntm_start_hour(NTM_START_HOUR),
      ntm_end_hour(NTM_END_HOUR),
      wake_duration_ms(NTM_WAKE_DURATION_SEC * 1000UL),
      screen_is_on(true),
      in_night_time_window(false),
      last_touch_time(0),
      screen_off_time(0)
{
}

void ScreenManager::begin(void (*backlightControlFn)(bool on)) {
    backlight_control = backlightControlFn;
    
    Serial.println("\n=== Screen Manager Initialized ===");
    Serial.printf("NTM Enabled: %s\n", ntm_enabled ? "YES" : "NO");
    
    if (ntm_enabled) {
        if (permanent_ntm) {
            Serial.println("NTM Mode: PERMANENT (always active)");
        } else {
            Serial.printf("NTM Window: %02d:00 to %02d:00\n", ntm_start_hour, ntm_end_hour);
        }
        Serial.printf("Wake Duration: %lu seconds\n", wake_duration_ms / 1000);
    }
    
    // Start with screen on
    turnScreenOn();
}

void ScreenManager::loop() {
    if (!ntm_enabled) {
        return; // NTM disabled, screen stays on always
    }
    
    // Update whether we're in the night time window
    updateNightTimeState();
    
    // If not in night time window (and not permanent), screen should be on
    if (!in_night_time_window && !permanent_ntm) {
        if (!screen_is_on) {
            turnScreenOn();
        }
        return;
    }
    
    // We're in night time window (or permanent mode is active)
    
    // Check if we need to turn screen off after wake duration expires
    if (screen_is_on && last_touch_time > 0) {
        unsigned long elapsed = millis() - last_touch_time;
        
        if (elapsed >= wake_duration_ms) {
            Serial.printf("[NTM] Wake duration expired (%lu ms), turning screen off\n", elapsed);
            turnScreenOff();
            last_touch_time = 0; // Reset
        }
    }
    
    // If screen is off and we're in NTM, it should stay off until touch
    if (!screen_is_on && last_touch_time == 0) {
        // Screen is correctly off during NTM, waiting for touch
        return;
    }
}

void ScreenManager::onTouchDetected() {
    if (!ntm_enabled) {
        return; // NTM disabled, touches don't affect screen state
    }
    
    // If we're in night time window (or permanent mode) and screen is off, wake it
    if ((in_night_time_window || permanent_ntm) && !screen_is_on) {
        Serial.println("[NTM] Touch detected - waking screen");
        turnScreenOn();
        last_touch_time = millis();
    }
    // If screen is already on during NTM, reset the wake timer
    else if ((in_night_time_window || permanent_ntm) && screen_is_on) {
        Serial.println("[NTM] Touch detected - resetting wake timer");
        last_touch_time = millis();
    }
}

void ScreenManager::setScreenOn(bool on) {
    if (on) {
        turnScreenOn();
    } else {
        turnScreenOff();
    }
}

void ScreenManager::setNightTimeModeEnabled(bool enabled) {
    ntm_enabled = enabled;
    Serial.printf("[NTM] Mode %s\n", enabled ? "ENABLED" : "DISABLED");
}

void ScreenManager::setPermanentNightMode(bool permanent) {
    permanent_ntm = permanent;
    Serial.printf("[NTM] Permanent mode: %s\n", permanent ? "YES" : "NO");
}

void ScreenManager::setNightTimeWindow(uint8_t start_hour, uint8_t end_hour) {
    ntm_start_hour = start_hour % 24;
    ntm_end_hour = end_hour % 24;
    Serial.printf("[NTM] Night window updated: %02d:00 to %02d:00\n", 
                  ntm_start_hour, ntm_end_hour);
}

void ScreenManager::setWakeDuration(uint32_t duration_ms) {
    wake_duration_ms = duration_ms;
    Serial.printf("[NTM] Wake duration updated: %lu seconds\n", duration_ms / 1000);
}

bool ScreenManager::isInNightTimeWindow() {
    if (permanent_ntm) {
        return true; // Permanent mode = always in "night time"
    }
    
    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    uint8_t current_hour = timeinfo.tm_hour;
    
    // Handle time windows that cross midnight
    if (ntm_start_hour <= ntm_end_hour) {
        // Normal window: e.g., 22:00 to 06:00 same day
        // This is actually unusual - typically crosses midnight
        return (current_hour >= ntm_start_hour && current_hour < ntm_end_hour);
    } else {
        // Window crosses midnight: e.g., 22:00 to 06:00 (next day)
        return (current_hour >= ntm_start_hour || current_hour < ntm_end_hour);
    }
}

void ScreenManager::updateNightTimeState() {
    bool was_in_window = in_night_time_window;
    in_night_time_window = isInNightTimeWindow();
    
    // Detect transition into night time window
    if (!was_in_window && in_night_time_window) {
        Serial.println("[NTM] Entering night time window");
        if (screen_is_on) {
            Serial.println("[NTM] Turning screen off (entering night time)");
            turnScreenOff();
        }
    }
    // Detect transition out of night time window
    else if (was_in_window && !in_night_time_window && !permanent_ntm) {
        Serial.println("[NTM] Exiting night time window");
        if (!screen_is_on) {
            Serial.println("[NTM] Turning screen on (day time)");
            turnScreenOn();
        }
    }
}

void ScreenManager::turnScreenOn() {
    if (screen_is_on) {
        return; // Already on
    }
    
    Serial.println("[Screen] Turning ON");
    
    if (backlight_control) {
        backlight_control(true);
    }
    
    screen_is_on = true;
    screen_off_time = 0;
}

void ScreenManager::turnScreenOff() {
    if (!screen_is_on) {
        return; // Already off
    }
    
    Serial.println("[Screen] Turning OFF");
    
    if (backlight_control) {
        backlight_control(false);
    }
    
    screen_is_on = false;
    screen_off_time = millis();
}

} // namespace comfoair
