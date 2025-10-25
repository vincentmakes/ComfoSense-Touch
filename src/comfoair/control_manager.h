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
    void loop();  // NEW: Process pending commands
    void setComfoAir(ComfoAir* comfo);
    void setMQTT(MQTT* mqtt_client);  // NEW: For remote client mode
    
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
    MQTT* mqtt;  // NEW: For remote client mode
    
    // Current state
    uint8_t current_fan_speed;    // 0-3
    bool boost_active;
    uint8_t current_temp_profile; // 0=NORMAL, 1=HEATING, 2=COOLING
    
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
    
    // Process pending commands (called from loop)
    void processPendingCommands();
    
    // Send command (via CAN or MQTT depending on configuration)
    void sendFanSpeedCommand(uint8_t speed);
    void sendBoostCommand();
    void sendTempProfileCommand(uint8_t profile);
    
    // Check if in demo mode
    bool isDemoMode();
};

} // namespace comfoair

#endif