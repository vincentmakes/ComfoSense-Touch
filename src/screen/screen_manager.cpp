#include "screen_manager.h"
#include "../board_config.h"
#include "../secrets.h"
#include <time.h>

#include "../serial_logger.h"
#define Serial LogSerial 


// EXIO pin definitions for V3 software PWM (1-based, subtract 1 for bit position)
#define EXIO_PWM 5  // EXIO5 connected to AP3032 FB pin (V3 only)

namespace comfoair {

ScreenManager::ScreenManager() 
    : backlight_control(nullptr),
      io_write(nullptr),
      io_output_state(0x0E),   // V3 default: backlight+LCD+touch on (0b00001110)
                                // V4: overwritten during begin() to 0xFF
      dimming_enabled(false),
      use_hardware_pwm(false),
      exio_pwm_pin(EXIO_PWM),
      current_brightness(DIMMING_DEFAULT_BRIGHTNESS),
      last_pwm_update(0),
      pwm_state(false),
      pwm_on_time_us(0),
      pwm_period_us(16700),  // 60Hz = 16700us period
                             // Flicker-free with R40=0Ω (no filtering), range is limited by R36
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

void ScreenManager::begin(void (*backlightControlFn)(bool on),
                           void (*ioWriteFn)(uint8_t reg, uint8_t val)) {
    backlight_control = backlightControlFn;
    io_write = ioWriteFn;
    
    Serial.println("\n=== Screen Manager Initialized ===");
    
    // Set initial IO output state based on board version
    if (isTouchLCDv4()) {
        io_output_state = 0xBF;  // V4: CH32V003 all high EXCEPT BEE_EN (bit 6)
    }
    Serial.printf("IO Output State: 0x%02X\n", io_output_state);
    
    // ================================================================
    // DIMMING CONFIGURATION — version-aware
    // ================================================================
    #ifdef DIMMING_ENABLED
      #if DIMMING_ENABLED
        if (isTouchLCDv3() && io_write) {
            // ---- V3: Software PWM via TCA9554 EXIO5 ----
            dimming_enabled = true;
            use_hardware_pwm = false;
            Serial.printf("Dimming: SOFTWARE PWM on EXIO%d via I2C (V3, 60Hz)\n", exio_pwm_pin);
            Serial.printf("Default Brightness: %d%%\n", current_brightness);
            Serial.println("Hardware: EXIO5 → AP3032 FB pin (R40=0Ω, direct connection)");
            Serial.println("Note: Using inverted mapping (100=brightest, 0=darkest)");
            
            // Initialize PWM timing
            calculatePWMTiming();
            
            // Start EXIO5 LOW (brightest/most stable) during init
            Serial.println("[Init] Setting EXIO5 LOW for stable startup");
            setPWMPin(false);
            pwm_state = false;
            last_pwm_update = micros();
            
            delay(100);  // Give AP3032 time to stabilize
            Serial.println("[Init] AP3032 stabilized, PWM will start in loop()");
            
        } else if (isTouchLCDv4() && io_write) {
            // ---- V4: Hardware PWM via CH32V003 register 0x05 ----
            // R36=62K and R40=10K are factory-populated on V4
            // The CH32V003 generates flicker-free PWM on its dedicated EXIO_PWM pin
            dimming_enabled = true;
            use_hardware_pwm = true;
            Serial.println("Dimming: HARDWARE PWM via CH32V003 reg 0x05 (V4)");
            Serial.printf("Default Brightness: %d%%\n", current_brightness);
            Serial.println("Hardware: CH32V003 EXIO_PWM → R40(10K) → AP3032 FB");
            Serial.printf("PWM range: 0–%d (safe max)\n", CH32V003_PWM_MAX);
            
            // Set initial brightness
            setHardwareBrightness(current_brightness);
            
        } else {
            dimming_enabled = false;
            use_hardware_pwm = false;
            Serial.println("Dimming: DISABLED (no IO write function provided)");
        }
      #else
        dimming_enabled = false;
        use_hardware_pwm = false;
        Serial.println("Dimming: DISABLED (DIMMING_ENABLED = false)");
      #endif
    #else
      dimming_enabled = false;
      use_hardware_pwm = false;
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
    
    // Start with screen on
    turnScreenOn();
}

void ScreenManager::loop() {
    // ================================================================
    // V3 SOFTWARE PWM — must be called every loop iteration
    // V4: No processing needed — CH32V003 handles PWM in hardware
    // ================================================================
    if (dimming_enabled && !use_hardware_pwm && screen_is_on) {
        // V3 only: Software PWM with startup delay
        static bool pwm_started = false;
        static unsigned long start_time = millis();
        
        if (!pwm_started && (millis() - start_time) > 2000) {
            Serial.println("[PWM] Starting V3 software PWM after initialization delay");
            pwm_started = true;
        }
        
        if (pwm_started) {
            updatePWM();
        }
    }
    
    // ================================================================
    // NIGHT TIME MODE — common to both V3 and V4
    // ================================================================
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

// ============================================================================
// V3 SOFTWARE PWM FUNCTIONS
// These are only used on V3 boards — guarded by use_hardware_pwm check
// ============================================================================

void ScreenManager::updatePWM() {
    if (!io_write || !dimming_enabled || use_hardware_pwm) return;
    
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
    // V3 software PWM only — do NOT call on V4!
    if (!io_write || use_hardware_pwm) return;
    
    // CRITICAL SAFEGUARD: Don't modify IO state if screen is off
    // This prevents PWM from accidentally turning backlight back on
    if (!screen_is_on) {
        return;
    }
    
    // Update the TCA output state for EXIO5 (bit 4, since EXIO5 is pin 5, 1-indexed)
    uint8_t bit_position = exio_pwm_pin - 1;  // EXIO5 -> bit 4
    
    uint8_t old_state = io_output_state;
    
    if (state) {
        io_output_state |= (1 << bit_position);
    } else {
        io_output_state &= ~(1 << bit_position);
    }
    
    // DEBUG: Log state changes periodically (first 10 seconds)
    static unsigned long last_debug = 0;
    static unsigned long start_time = millis();
    static int toggle_count = 0;
    
    toggle_count++;
    
    if (millis() - last_debug >= 2000 && (millis() - start_time) < 20000) {
        Serial.printf("[PWM DEBUG V3] Toggles/2s: %d, State=%d, IO: 0x%02X -> 0x%02X, Bri=%d%%, ON=%dus\n", 
                      toggle_count, state, old_state, io_output_state, 
                      current_brightness, pwm_on_time_us);
        last_debug = millis();
        toggle_count = 0;
    }
    
    // Write to TCA9554 output register
    io_write(TCA9554_REG_OUTPUT, io_output_state);
}

void ScreenManager::calculatePWMTiming() {
    // V3 only: Invert brightness for intuitive control
    // AP3032 FB works opposite: HIGH = darker, LOW = brighter
    // So we invert: user 100 → 0% duty (brightest), user 0 → 100% duty (darkest)
    
    uint8_t inverted_brightness = 100 - current_brightness;
    pwm_on_time_us = (uint16_t)((inverted_brightness * pwm_period_us) / 100);
    
    Serial.printf("[Dimming V3] Brightness: %d%% -> Inverted: %d%% -> ON time: %dus/%dus (%.1f%% duty)\n",
                  current_brightness, inverted_brightness, pwm_on_time_us, pwm_period_us,
                  (pwm_on_time_us * 100.0f) / pwm_period_us);
}

// ============================================================================
// V4 HARDWARE PWM FUNCTION
// Single I2C write to CH32V003 register 0x05 — flicker-free, instant
// ============================================================================

void ScreenManager::setHardwareBrightness(uint8_t percent) {
    if (!io_write || !use_hardware_pwm) return;
    
    // V4 CH32V003 PWM → R40 → AP3032 FB pin
    // Higher duty = more FB pull-down = DIMMER
    // But the response is non-linear: duty 0-80 all look equally bright
    // because AP3032 is at full current. Visible dimming starts ~duty 80.
    // Map 0-100% to the useful visible range for natural-feeling control:
    //   100% → duty 30  (brightest with slight headroom)
    //   0%   → duty 240 (very dim but not fully off — turnScreenOff handles that)
    static const uint8_t DUTY_BRIGHT = 30;   // 100% brightness
    static const uint8_t DUTY_DIM    = 240;  // 0% brightness
    
    uint8_t duty = DUTY_DIM - (uint8_t)(((uint16_t)percent * (DUTY_DIM - DUTY_BRIGHT)) / 100);
    io_write(CH32V003_REG_PWM, duty);
    
    Serial.printf("[Dimming V4] Brightness: %d%% → PWM duty: %d (range %d–%d)\n", 
                  percent, duty, DUTY_BRIGHT, DUTY_DIM);
}

// ============================================================================
// COMMON FUNCTIONS — work on both V3 and V4
// ============================================================================

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
        
        if (use_hardware_pwm) {
            // V4: One I2C write, done
            setHardwareBrightness(brightness);
        } else if (dimming_enabled) {
            // V3: Recalculate software PWM timing
            calculatePWMTiming();
        }
        
        Serial.printf("[Dimming] Brightness changed to %d%%\n", brightness);
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

// ============================================================================
// SCREEN ON/OFF — version-aware
// ============================================================================

void ScreenManager::turnScreenOn() {
    if (screen_is_on) {
        return; // Already on
    }
    
    Serial.println("[Screen] Turning ON");
    
    if (use_hardware_pwm) {
        // ---- V4: Hardware PWM — just set brightness ----
        setHardwareBrightness(current_brightness);
        
    } else {
        // ---- V3: Software PWM + backlight bit ----
        
        // Ensure backlight bit is set in io_output_state
        io_output_state |= (1 << V3_BIT_BL_EN);
        
        // If dimming enabled, calculate PWM timing FIRST
        if (dimming_enabled && io_write) {
            calculatePWMTiming();
            
            // Set initial PWM state based on brightness
            if (current_brightness <= 50) {
                pwm_state = false;
                setPWMPin(false);
                Serial.printf("[Screen] Starting with PWM LOW (brightness=%d%%)\n", current_brightness);
            } else {
                pwm_state = true;
                setPWMPin(true);
                Serial.printf("[Screen] Starting with PWM HIGH (brightness=%d%%)\n", current_brightness);
            }
            
            last_pwm_update = micros();
        }
        
        // Turn on backlight (EXIO2 via TCA9554) AFTER setting PWM state
        if (backlight_control) {
            backlight_control(true);
        }
        
        // Final write to ensure both backlight and PWM state are correct
        if (io_write) {
            io_write(TCA9554_REG_OUTPUT, io_output_state);
            Serial.printf("[Screen] IO state after ON: 0x%02X (brightness=%d%%)\n", 
                          io_output_state, current_brightness);
        }
    }
    
    screen_is_on = true;
    screen_off_time = 0;
}

void ScreenManager::turnScreenOff() {
    if (!screen_is_on) {
        return; // Already off
    }
    
    Serial.println("[Screen] Turning OFF");
    
    if (use_hardware_pwm) {
        // ---- V4: Hardware PWM — set duty to max (inverted: max = off) ----
        if (io_write) {
            io_write(CH32V003_REG_PWM, CH32V003_PWM_MAX);
        }
        
    } else {
        // ---- V3: Stop software PWM + clear backlight bit ----
        
        // Stop PWM FIRST (before turning off backlight)
        if (dimming_enabled && io_write) {
            pwm_state = false;
            setPWMPin(false);  // Set EXIO5 low
        }
        
        // Turn off backlight (EXIO2 via TCA9554)
        if (backlight_control) {
            backlight_control(false);
        }
        
        // Ensure backlight bit is cleared in io_output_state
        io_output_state &= ~(1 << V3_BIT_BL_EN);
        
        // Force final write to ensure backlight is OFF
        if (io_write) {
            io_write(TCA9554_REG_OUTPUT, io_output_state);
            Serial.printf("[Screen] IO state after OFF: 0x%02X\n", io_output_state);
        }
    }
    
    screen_is_on = false;
    screen_off_time = millis();
}

} // namespace comfoair
