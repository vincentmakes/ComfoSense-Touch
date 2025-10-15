#ifndef CONTROL_MANAGER_H
#define CONTROL_MANAGER_H

#include <Arduino.h>

namespace comfoair {

// Forward declaration
class ComfoAir;

class ControlManager {
public:
    ControlManager();
    
    void setup();
    void setComfoAir(ComfoAir* comfo);
    
    // Button command triggers (called from GUI events)
    void increaseFanSpeed();
    void decreaseFanSpeed();
    void activateBoost();
    void setTempProfile(uint8_t profile); // 0=NORMAL, 1=HEATING, 2=COOLING
    
    // CAN feedback handlers (called when CAN messages received)
    void updateFanSpeedFromCAN(uint8_t speed);
    void updateBoostStateFromCAN(bool active);
    void updateTempProfileFromCAN(uint8_t profile);
    
    // Getters
    uint8_t getCurrentFanSpeed() { return current_fan_speed; }
    bool isBoostActive() { return boost_active; }
    uint8_t getCurrentTempProfile() { return current_temp_profile; }
    
private:
    ComfoAir* comfoair;
    
    // Current state
    uint8_t current_fan_speed;    // 0-3
    bool boost_active;
    uint8_t current_temp_profile; // 0=NORMAL, 1=HEATING, 2=COOLING
    
    // Demo mode flag (auto-detected based on CAN availability)
    bool demo_mode;
    unsigned long last_can_feedback;
    static const unsigned long CAN_TIMEOUT = 5000; // 5 seconds without CAN = demo mode
    
    // Send CAN command (if available)
    void sendFanSpeedCommand(uint8_t speed);
    void sendBoostCommand();
    void sendTempProfileCommand(uint8_t profile);
    
    // Check if in demo mode
    bool isDemoMode();
};

} // namespace comfoair

#endif
