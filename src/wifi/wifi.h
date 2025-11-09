#ifndef COMFOWIFI_H
#define COMFOWIFI_H
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
      
      // Timing constants
      static const unsigned long STATUS_CHECK_INTERVAL = 5000;      // Check status every 5 seconds
      static const unsigned long RECONNECT_INTERVAL = 10000;        // Try reconnecting every 10 seconds
      static const unsigned long RECONNECT_FAST_INTERVAL = 2000;    // Fast reconnect for first 3 attempts
      static const unsigned long CONNECTION_TIMEOUT = 20000;        // Max time to wait for initial connection
      static const int MAX_FAST_RECONNECT_ATTEMPTS = 3;            // Number of fast reconnect attempts
      
      void attemptReconnect();
      void logConnectionStats();
  };
}
#endif