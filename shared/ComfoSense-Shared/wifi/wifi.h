#ifndef COMFOWIFI_H
#define COMFOWIFI_H

#include <WiFi.h>
#include <functional>

namespace comfoair {

// WiFi configuration (passed at runtime instead of compile-time secrets.h)
struct WiFiConfig {
    const char* ssid;
    const char* pass;
    const char* hostname;
};

class WiFi {
public:
    WiFi();
    void configure(const WiFiConfig& config);
    void setup();
    void loop();
    bool isConnected();
    void updateWiFiIcon();
    int8_t getSignalStrength();  // Returns RSSI in dBm

    // Display callback - set by each project to wire its own UI
    void setWiFiIconCallback(std::function<void(bool connected)> cb) {
        wifi_icon_callback = cb;
    }

private:
    WiFiConfig config;
    bool configured;
    bool connected;
    unsigned long last_status_check;
    unsigned long last_reconnect_attempt;
    unsigned long connection_lost_time;
    int reconnect_attempts;
    bool wifi_event_registered;

    // Display callback (set by host project)
    std::function<void(bool connected)> wifi_icon_callback;

    // Timing constants
    static const unsigned long STATUS_CHECK_INTERVAL = 10000;
    static const unsigned long RECONNECT_INTERVAL = 30000;
    static const unsigned long CONNECTION_TIMEOUT = 20000;
    static const unsigned long MANUAL_RECONNECT_THRESHOLD = 120000;

    void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
    void logConnectionStats();
};

} // namespace comfoair

#endif
