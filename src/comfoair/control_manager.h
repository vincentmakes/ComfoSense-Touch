#ifndef CONTROL_MANAGER_H
#define CONTROL_MANAGER_H

#include <Arduino.h>

namespace comfoair {

// Forward declarations
class ComfoAir;
class MQTT;

class ControlManager {
public:
    ControlManager();
    
    void setup();
    void loop();  // Process pending commands AND boost timer
    void setComfoAir(ComfoAir* comfo);
    void setMQTT(MQTT* mqtt_client);
    
    // Button command triggers (called from GUI events)
    void increaseFanSpeed();
    void decreaseFanSpeed();
    void activateBoost();  // Activate or extend boost by 20 minutes
    void setTempProfile(uint8_t profile); // 0=NORMAL, 1=HEATING, 2=COOLING
    
    // CAN feedback handlers (called when CAN messages received)
    void updateFanSpeedFromCAN(uint8_t speed);
    void updateTempProfileFromCAN(uint8_t profile);
    
    // Getters
    uint8_t getCurrentFanSpeed() { return current_fan_speed; }
    bool isBoostActive() { return boost_timer_active; }
    uint8_t getCurrentTempProfile() { return current_temp_profile; }
    int getRemainingBoostMinutes();  // Returns minutes remaining in boost
    
private:
    ComfoAir* comfoair;
    MQTT* mqtt;
    
    // Current state
    uint8_t current_fan_speed;     // 0-3 (actual fan speed)
    uint8_t speed_before_boost;    // Speed to return to after boost ends
    uint8_t current_temp_profile;  // 0=NORMAL, 1=HEATING, 2=COOLING
    
    // Ã¢Å“â€¦ LOCAL BOOST TIMER (managed entirely by ESP32)
    bool boost_timer_active;           // Is boost timer running?
    unsigned long boost_end_time;      // millis() when boost should end
    unsigned long last_timer_update;   // Last time we updated the display
    static const unsigned long BOOST_DURATION_MS = 20 * 60 * 1000;  // 20 minutes in ms
    static const unsigned long TIMER_UPDATE_INTERVAL = 60000;       // Update display every 60 seconds (1 minute)
    
    // Demo mode flag (auto-detected based on CAN availability)
    bool demo_mode;
    unsigned long last_can_feedback;
    static const unsigned long CAN_TIMEOUT = 5000; // 5 seconds without CAN = demo mode
    
    // Input debouncing for remote client mode (prevent command flooding)
    unsigned long last_fan_speed_change;
    unsigned long last_temp_profile_change;
    uint8_t pending_fan_speed;
    uint8_t pending_temp_profile;
    bool fan_speed_command_pending;
    bool temp_profile_command_pending;
    static const unsigned long COMMAND_DEBOUNCE_MS = 2000; // Wait 2s before sending command
    
    // Internal helper functions
    void processPendingCommands();
    void updateBoostTimer();  // Ã¢Å“â€¦ NEW: Check and update boost timer every loop
    void endBoost();          // Ã¢Å“â€¦ NEW: End boost and return to previous speed
    void updateDisplay();     // Update fan speed display and timer
    
    // Send command (via CAN or MQTT depending on configuration)
    void sendFanSpeedCommand(uint8_t speed);
    void sendTempProfileCommand(uint8_t profile);
    
    // Check if in demo mode
    bool isDemoMode();
};

} // namespace comfoair

#endif