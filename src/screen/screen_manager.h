#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <Arduino.h>

namespace comfoair {

class ScreenManager {
public:
    ScreenManager();
    
    // Initialize with backlight control function and TCA9554 write function
    // tcaWriteFn: function to write to TCA9554 registers
    void begin(void (*backlightControlFn)(bool on),
               void (*tcaWriteFn)(uint8_t reg, uint8_t val) = nullptr);
    
    // Call this in main loop - MUST be called frequently for PWM generation
    void loop();
    
    // Call this from touch callback to wake screen
    void onTouchDetected();
    
    // Manual screen control
    void setScreenOn(bool on);
    bool isScreenOn() const { return screen_is_on; }
    
    // Brightness control (0-100, only works if dimming enabled)
    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const { return current_brightness; }
    
    // NTM configuration (call these in setup if needed to override secrets.h)
    void setNightTimeModeEnabled(bool enabled);
    void setPermanentNightMode(bool permanent);
    void setNightTimeWindow(uint8_t start_hour, uint8_t end_hour);
    void setWakeDuration(uint32_t duration_ms);
    
    // Get current TCA9554 output state (for main.cpp to track)
    uint8_t getTCAOutputState() const { return tca_output_state; }
    
private:
    void (*backlight_control)(bool on);
    void (*tca_write)(uint8_t reg, uint8_t val);
    
    // TCA9554 state tracking
    uint8_t tca_output_state;  // Current output register value
    
    // Dimming configuration
    bool dimming_enabled;
    uint8_t exio_pwm_pin;       // EXIO pin for PWM (5 = EXIO5)
    uint8_t current_brightness; // 0-100
    
    // PWM state for software PWM
    unsigned long last_pwm_update;
    bool pwm_state;             // Current PWM pin state (high/low)
    uint16_t pwm_on_time_us;    // ON time in microseconds
    uint16_t pwm_period_us;     // Total period in microseconds (5000us = 200Hz)
    
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
    void updatePWM();           // Software PWM update via I2C
    void calculatePWMTiming();  // Calculate ON/OFF times based on brightness
    void setPWMPin(bool state); // Set EXIO5 high or low via I2C
};

} // namespace comfoair

#endif // SCREEN_MANAGER_H