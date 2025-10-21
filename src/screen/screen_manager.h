#ifndef SCREEN_MANAGER_H
#define SCREEN_MANAGER_H

#include <Arduino.h>

namespace comfoair {

class ScreenManager {
public:
    ScreenManager();
    
    // Initialize with backlight control function
    void begin(void (*backlightControlFn)(bool on));
    
    // Call this in main loop
    void loop();
    
    // Call this from touch callback to wake screen
    void onTouchDetected();
    
    // Manual screen control
    void setScreenOn(bool on);
    bool isScreenOn() const { return screen_is_on; }
    
    // NTM configuration (call these in setup if needed to override secrets.h)
    void setNightTimeModeEnabled(bool enabled);
    void setPermanentNightMode(bool permanent);
    void setNightTimeWindow(uint8_t start_hour, uint8_t end_hour);
    void setWakeDuration(uint32_t duration_ms);
    
private:
    void (*backlight_control)(bool on);
    
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
};

} // namespace comfoair

#endif // SCREEN_MANAGER_H
