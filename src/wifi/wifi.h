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
      
    private:
      bool connected;
      unsigned long last_status_check;
      static const unsigned long STATUS_CHECK_INTERVAL = 1000; // Check every 1 second (for faster testing)
  };
}
#endif