#ifndef COMFOWIFI_H
#define COMFOWIFI_H

#include <WiFi.h>

namespace comfoair {
  class WiFi {
    public:
      WiFi();
      void setup();
      void loop();
      bool isConnected();
      void updateWiFiIcon();
      int8_t getSignalStrength();  // Returns RSSI in dBm
      
    private:
      bool connected;
      unsigned long last_status_check;
      unsigned long last_reconnect_attempt;
      unsigned long connection_lost_time;
      int reconnect_attempts;
      bool wifi_event_registered;
      
      // Timing constants
      static const unsigned long STATUS_CHECK_INTERVAL = 10000;      // Check status every 10 seconds (less frequent now that we have events)
      static const unsigned long RECONNECT_INTERVAL = 30000;         // Manual reconnect interval (30 seconds)
      static const unsigned long CONNECTION_TIMEOUT = 20000;         // Max time to wait for initial connection
      static const unsigned long MANUAL_RECONNECT_THRESHOLD = 120000; // Try manual reconnect after 2 minutes of auto-reconnect failure
      
      void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
      void logConnectionStats();
  };
}
#endif