#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <Arduino.h>

namespace comfoair {

class ScreenManager {
public:
    ScreenManager();
    
    // Initialize with backlight control function and IO expander write function
    // ioWriteFn: function to write to IO expander registers (routes to TCA9554 or CH32V003)
    void begin(void (*backlightControlFn)(bool on),
               void (*ioWriteFn)(uint8_t reg, uint8_t val) = nullptr);
    
    // Call this in main loop - MUST be called frequently for V3 software PWM generation
    // V4: No PWM processing needed in loop (hardware handles it)
    void loop();
    
    // Call this from touch callback to wake screen
    void onTouchDetected();
    
    // Manual screen control
    void setScreenOn(bool on);
    bool isScreenOn() const { return screen_is_on; }
    
    // Brightness control (0-100)
    // V3: Adjusts software PWM duty cycle (only works if dimming enabled + R36/R40 soldered)
    // V4: Writes directly to CH32V003 hardware PWM register 0x05 (always works)
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const { return current_brightness; }
    
    // NTM configuration (call these in setup if needed to override secrets.h)
    void setNightTimeModeEnabled(bool enabled);
    void setPermanentNightMode(bool permanent);
    void setNightTimeWindow(uint8_t start_hour, uint8_t end_hour);
    void setWakeDuration(uint32_t duration_ms);
    
    // Get current IO expander output state (for main.cpp to track)
    uint8_t getIOOutputState() const { return io_output_state; }
    
private:
    void (*backlight_control)(bool on);
    void (*io_write)(uint8_t reg, uint8_t val);   // Renamed from tca_write
    
    // IO expander state tracking
    uint8_t io_output_state;   // Current output register value (renamed from tca_output_state)
    
    // Dimming configuration
    bool dimming_enabled;        // true if ANY dimming is active (V3 sw or V4 hw)
    bool use_hardware_pwm;       // true for V4 (CH32V003), false for V3 (TCA9554)
    uint8_t exio_pwm_pin;        // EXIO pin for software PWM — V3 only (5 = EXIO5)
    uint8_t current_brightness;  // 0-100
    
    // PWM state for V3 software PWM (unused on V4)
    unsigned long last_pwm_update;
    bool pwm_state;              // Current PWM pin state (high/low)
    uint16_t pwm_on_time_us;     // ON time in microseconds
    uint16_t pwm_period_us;      // Total period in microseconds (16700us = 60Hz)
    
    // NTM configuration
    bool ntm_enabled;
    bool permanent_ntm;
    uint8_t ntm_start_hour;
    uint8_t ntm_end_hour;
    uint32_t wake_duration_ms;
    
    // Runtime state
    bool screen_is_on;
    bool in_night_time_window;
    unsigned long last_touch_time;
    unsigned long screen_off_time;
    
    // Helper functions
    bool isInNightTimeWindow();
    void updateNightTimeState();
    void turnScreenOn();
    void turnScreenOff();
    void updatePWM();              // V3 software PWM update via I2C
    void calculatePWMTiming();     // V3: Calculate ON/OFF times based on brightness
    void setPWMPin(bool state);    // V3: Set EXIO5 high or low via I2C
    void setHardwareBrightness(uint8_t percent);  // V4: Write to CH32V003 PWM register
};

} // namespace comfoair

#endif // SCREEN_MANAGER_H
