#ifndef CONTROL_MANAGER_H
#define CONTROL_MANAGER_H

#include <Arduino.h>
#include <functional>

namespace comfoair {

// Forward declaration
class MQTT;

// Callback types for display updates
using FanSpeedDisplayCallback = std::function<void(uint8_t speed, bool boost)>;
using TempProfileDisplayCallback = std::function<void(uint8_t profile)>;
using BoostTimerDisplayCallback = std::function<void(int minutes_remaining)>;

// Callback type for sending commands (abstracts CAN vs MQTT)
using CommandSendCallback = std::function<void(const char* command)>;

class ControlManager {
public:
    ControlManager();

    void setup();
    void loop();  // Process pending commands AND boost timer

    // Set MQTT client for remote command sending
    void setMQTT(MQTT* mqtt_client);

    // Set command sender callback (for CAN mode, set to ComfoAir::sendCommand)
    void setCommandSendCallback(CommandSendCallback cb) {
        command_send_callback = cb;
    }

    // Set MQTT prefix for command topics (default: "comfoair")
    void setMqttPrefix(const char* prefix) {
        mqtt_prefix = prefix;
    }

    // Set remote client mode
    void setRemoteClientMode(bool remote) {
        remote_client_mode = remote;
    }

    // Display callbacks - set by each project to wire its own UI
    void setFanSpeedDisplayCallback(FanSpeedDisplayCallback cb) {
        fan_speed_display_callback = cb;
    }
    void setTempProfileDisplayCallback(TempProfileDisplayCallback cb) {
        temp_profile_display_callback = cb;
    }
    void setBoostTimerDisplayCallback(BoostTimerDisplayCallback cb) {
        boost_timer_display_callback = cb;
    }

    // Button command triggers (called from GUI events)
    void increaseFanSpeed();
    void decreaseFanSpeed();
    void activateBoost();  // Activate or extend boost by 20 minutes
    void setTempProfile(uint8_t profile); // 0=NORMAL, 1=COOLING, 2=HEATING

    // CAN/MQTT feedback handlers (called when state updates received)
    void updateFanSpeedFromCAN(uint8_t speed);
    void updateTempProfileFromCAN(uint8_t profile);

    // Getters
    uint8_t getCurrentFanSpeed() { return current_fan_speed; }
    bool isBoostActive() { return boost_timer_active; }
    uint8_t getCurrentTempProfile() { return current_temp_profile; }
    int getRemainingBoostMinutes();  // Returns minutes remaining in boost

private:
    MQTT* mqtt;

    // Runtime mode flag (replaces compile-time REMOTE_CLIENT_MODE)
    bool remote_client_mode;
    const char* mqtt_prefix;

    // Current state
    uint8_t current_fan_speed;     // 0-3 (actual fan speed)
    uint8_t speed_before_boost;    // Speed to return to after boost ends
    uint8_t current_temp_profile;  // 0=NORMAL, 1=COOLING, 2=HEATING

    // LOCAL BOOST TIMER (managed entirely by ESP32)
    bool boost_timer_active;           // Is boost timer running?
    unsigned long boost_end_time;      // millis() when boost should end
    unsigned long last_timer_update;   // Last time we updated the display
    static const unsigned long BOOST_DURATION_MS = 20 * 60 * 1000;  // 20 minutes in ms
    static const unsigned long TIMER_UPDATE_INTERVAL = 60000;       // Update display every 60 seconds

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

    // Display callbacks (set by host project)
    FanSpeedDisplayCallback fan_speed_display_callback;
    TempProfileDisplayCallback temp_profile_display_callback;
    BoostTimerDisplayCallback boost_timer_display_callback;

    // Command send callback (for CAN mode)
    CommandSendCallback command_send_callback;

    // Internal helper functions
    void processPendingCommands();
    void updateBoostTimer();
    void endBoost();
    void updateDisplay();

    // Send command (via CAN callback or MQTT depending on configuration)
    void sendFanSpeedCommand(uint8_t speed);
    void sendTempProfileCommand(uint8_t profile);

    // Check if in demo mode
    bool isDemoMode();
};

} // namespace comfoair

#endif
