#include "screen_manager.h"
#include "../secrets.h"
#include <time.h>

// TCA9554 register addresses
#define TCA_REG_OUTPUT 0x01
#define TCA_REG_CONFIG 0x03

// EXIO pin definitions (1-based, so subtract 1 for bit position)
#define EXIO_PWM 5  // EXIO5 connected to AP3032 FB pin

namespace comfoair {

ScreenManager::ScreenManager() 
    : backlight_control(nullptr),
      tca_write(nullptr),
      tca_output_state(0x0E),  // CRITICAL: Initialize with backlight ON (bit 1,2,3 = 1)
                                // 0x0E = 0b00001110 = EXIO2(BL), EXIO3(LCD_RST), EXIO4 ON
      dimming_enabled(false),
      exio_pwm_pin(EXIO_PWM),
      current_brightness(DIMMING_DEFAULT_BRIGHTNESS),
      last_pwm_update(0),
      pwm_state(false),
      pwm_on_time_us(0),
      pwm_period_us(16700),  // 80Hz = 12500us period OR 60Hz = 16700us
                             // Flicker-free with R40=0Î© (no filtering), range is limited by R36
      ntm_enabled(NTM_ENABLED),
      permanent_ntm(NTM_PERMANENT),
      ntm_start_hour(NTM_START_HOUR),
      ntm_end_hour(NTM_END_HOUR),
      wake_duration_ms(NTM_WAKE_DURATION_SEC * 1000UL),
      screen_is_on(!NTM_PERMANENT),  // FIXED: Start with screen OFF if permanent mode
      in_night_time_window(false),
      last_touch_time(0),
      screen_off_time(0)
{
}

void ScreenManager::begin(void (*backlightControlFn)(bool on),
                           void (*tcaWriteFn)(uint8_t reg, uint8_t val)) {
    backlight_control = backlightControlFn;
    tca_write = tcaWriteFn;
    
    Serial.println("\n=== Screen Manager Initialized ===");
    Serial.printf("TCA Output State: 0x%02X (preserves backlight)\n", tca_output_state);
    
    // DEBUG: Show brightness value from secrets.h
    //Serial.printf("Current Brightness Value: %d%% (from DIMMING_DEFAULT_BRIGHTNESS)\n", current_brightness);
    
    // Check if dimming is enabled
    #ifdef DIMMING_ENABLED
      #if DIMMING_ENABLED
        if (tca_write) {
            dimming_enabled = true;
            Serial.printf("Dimming: ENABLED on EXIO%d via I2C (Software PWM @ 80Hz)\n", exio_pwm_pin);
            Serial.printf("Default Brightness: %d%%\n", current_brightness);
            Serial.println("Hardware: EXIO5 â†’ AP3032 FB pin (R40=0Î©, direct connection)");
            Serial.println("Note: Using 80Hz PWM with inverted mapping (100=brightest, 0=darkest)");
            
            // CRITICAL: Set EXIO5 to a stable middle state during initialization
            // This prevents AP3032 oscillation with low R40 values
            calculatePWMTiming();
            
            // Initialize EXIO5 to LOW state (brightest/most stable)
            // This gives AP3032 time to stabilize before PWM starts
            Serial.println("[Init] Setting EXIO5 LOW for stable startup");
            setPWMPin(false);
            pwm_state = false;
            last_pwm_update = micros();
            
            delay(100);  // Give AP3032 time to stabilize with low R40
            
            Serial.println("[Init] AP3032 stabilized, PWM will start in loop()");
        } else {
            dimming_enabled = false;
            Serial.println("Dimming: DISABLED (no TCA write function provided)");
        }
      #else
        dimming_enabled = false;
        Serial.println("Dimming: DISABLED (DIMMING_ENABLED = false)");
      #endif
    #else
      dimming_enabled = false;
      Serial.println("Dimming: DISABLED (DIMMING_ENABLED not defined)");
    #endif
    
    Serial.printf("NTM Enabled: %s\n", ntm_enabled ? "YES" : "NO");
    
    if (ntm_enabled) {
        if (permanent_ntm) {
            Serial.println("NTM Mode: PERMANENT (always active)");
        } else {
            Serial.printf("NTM Window: %02d:00 to %02d:00\n", ntm_start_hour, ntm_end_hour);
        }
        Serial.printf("Wake Duration: %lu seconds\n", wake_duration_ms / 1000);
    }
    
    // Initialize screen state based on NTM configuration
    if (permanent_ntm) {
        // In permanent night mode, screen starts OFF (waiting for touch)
        Serial.println("[NTM] Starting with screen OFF (permanent mode)");
        // Don't call turnScreenOff() here - just leave it off from hardware init
        // The screen_is_on flag is already false from constructor
        // We just need to ensure the backlight stays off
        if (backlight_control) {
            backlight_control(false);
        }
    } else {
        // Not in permanent mode - start with screen ON
        Serial.println("[Init] Starting with screen ON");
        turnScreenOn();
    }
}

void ScreenManager::loop() {
    // CRITICAL: Update PWM if dimming is enabled and screen is on
    // This MUST be called frequently (every loop iteration) for stable PWM
    // At 80Hz, we need updates at least every 6ms (half the 12.5ms period)
    
    // STARTUP DELAY: Don't start PWM for first 2 seconds to allow display init
    static bool pwm_started = false;
    static unsigned long start_time = millis();
    
    if (dimming_enabled && screen_is_on) {
        if (!pwm_started && (millis() - start_time) > 2000) {
            Serial.println("[PWM] Starting PWM after initialization delay");
            pwm_started = true;
        }
        
        if (pwm_started) {
            updatePWM();
        }
    }
    
    if (!ntm_enabled) {
        return; // NTM disabled, screen stays on always
    }
    
    // Periodic diagnostic logging (every 60 seconds in permanent mode)
    if (permanent_ntm) {
        static unsigned long last_diagnostic = 0;
        if (millis() - last_diagnostic >= 60000) {
            Serial.printf("[NTM Diagnostic] screen_is_on=%d, last_touch_time=%lu, time_since_touch=%lu ms\n",
                         screen_is_on, last_touch_time, 
                         last_touch_time > 0 ? (millis() - last_touch_time) : 0);
            last_diagnostic = millis();
        }
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
        unsigned long current_time = millis();
        unsigned long elapsed;
        
        // Handle millis() overflow safely
        if (current_time >= last_touch_time) {
            elapsed = current_time - last_touch_time;
        } else {
            // Overflow occurred, wrap around
            elapsed = (ULONG_MAX - last_touch_time) + current_time + 1;
        }
        
        if (elapsed >= wake_duration_ms) {
            Serial.printf("[NTM] Wake duration expired (%lu ms), turning screen off\n", elapsed);
            turnScreenOff();
            last_touch_time = 0; // Reset
        }
    }
    
    // CRITICAL: In permanent NTM, if screen is off with no active touch timer, keep it off
    // This prevents the screen from mysteriously turning on after hours of inactivity
    if ((permanent_ntm || in_night_time_window) && !screen_is_on && last_touch_time == 0) {
        // Screen is correctly off during NTM, waiting for touch
        // Ensure it stays off even if other code tries to turn it on
        return;
    }
    
    // Safety check: In permanent NTM, if screen is somehow on without a touch timer, turn it off
    // This handles edge cases where screen state becomes inconsistent
    if (permanent_ntm && screen_is_on && last_touch_time == 0) {
        Serial.println("[NTM] WARNING: Screen on in permanent mode without touch timer - correcting");
        turnScreenOff();
    }
}

void ScreenManager::updatePWM() {
    if (!tca_write || !dimming_enabled) return;
    
    unsigned long now = micros();
    unsigned long elapsed = now - last_pwm_update;
    
    // Handle edge case: brightness 0 = always off
    if (pwm_on_time_us == 0) {
        if (pwm_state) {
            pwm_state = false;
            setPWMPin(false);
        }
        return;
    }
    
    // Handle edge case: brightness 100 = always on
    if (pwm_on_time_us >= pwm_period_us) {
        if (!pwm_state) {
            pwm_state = true;
            setPWMPin(true);
        }
        return;
    }
    
    // Normal PWM operation
    if (pwm_state) {
        // Currently ON, check if we need to turn OFF
        if (elapsed >= pwm_on_time_us) {
            pwm_state = false;
            setPWMPin(false);
            last_pwm_update = now;
        }
    } else {
        // Currently OFF, check if we need to start new period
        uint16_t off_time_us = pwm_period_us - pwm_on_time_us;
        if (elapsed >= off_time_us) {
            pwm_state = true;
            setPWMPin(true);
            last_pwm_update = now;
        }
    }
}

void ScreenManager::setPWMPin(bool state) {
    if (!tca_write) return;
    
    // Update the TCA output state for EXIO5 (bit 4, since EXIO5 is pin 5, and pins are 1-indexed)
    uint8_t bit_position = exio_pwm_pin - 1;  // EXIO5 -> bit 4
    
    uint8_t old_state = tca_output_state;
    
    if (state) {
        // Set bit high
        tca_output_state |= (1 << bit_position);
    } else {
        // Set bit low
        tca_output_state &= ~(1 << bit_position);
    }
    
    // DEBUG: Log state changes periodically (first 10 seconds)
    static unsigned long last_debug = 0;
    static unsigned long start_time = millis();
    static int toggle_count = 0;
    
    toggle_count++;
    
    if (millis() - last_debug >= 2000 && (millis() - start_time) < 20000) {
        Serial.printf("[PWM DEBUG] Toggles in last 2s: %d, State=%d, TCA: 0x%02X -> 0x%02X, Brightness=%d%%, ON_time=%dus\n", 
                      toggle_count, state, old_state, tca_output_state, 
                      current_brightness, pwm_on_time_us);
        last_debug = millis();
        toggle_count = 0;
    }
    
    // Write to TCA9554 output register
    tca_write(TCA_REG_OUTPUT, tca_output_state);
}

void ScreenManager::calculatePWMTiming() {
    // Invert brightness for intuitive control:
    // AP3032 FB works opposite: HIGH = darker, LOW = brighter
    // So we invert: user 100 â†’ 0% duty (brightest), user 0 â†’ 100% duty (darkest)
    
    uint8_t inverted_brightness = 100 - current_brightness;
    pwm_on_time_us = (uint16_t)((inverted_brightness * pwm_period_us) / 100);
    
    Serial.printf("[Dimming] Brightness: %d%% -> Inverted: %d%% -> ON time: %dus/%dus (%.1f%% duty)\n",
                  current_brightness, inverted_brightness, pwm_on_time_us, pwm_period_us,
                  (pwm_on_time_us * 100.0f) / pwm_period_us);
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

void ScreenManager::setBrightness(uint8_t brightness) {
    if (brightness > 100) brightness = 100;
    
    if (current_brightness != brightness) {
        current_brightness = brightness;
        calculatePWMTiming();
        
        if (dimming_enabled) {
            Serial.printf("[Dimming] Brightness changed to %d%%\n", brightness);
        }
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
    
    // Turn on backlight (EXIO2 via TCA9554)
    if (backlight_control) {
        backlight_control(true);
    }
    
    // If dimming enabled, start software PWM
    if (dimming_enabled && tca_write) {
        calculatePWMTiming();
        last_pwm_update = micros();
        pwm_state = true;
        setPWMPin(true);  // Start with PWM pin high
    }
    
    screen_is_on = true;
    screen_off_time = 0;
}

void ScreenManager::turnScreenOff() {
    if (!screen_is_on) {
        return; // Already off
    }
    
    Serial.println("[Screen] Turning OFF");
    
    // Turn off backlight (EXIO2 via TCA9554)
    if (backlight_control) {
        backlight_control(false);
    }
    
    // If dimming enabled, stop PWM (set pin LOW)
    if (dimming_enabled && tca_write) {
        pwm_state = false;
        setPWMPin(false);
    }
    
    screen_is_on = false;
    screen_off_time = millis();
}

} // namespace comfoair