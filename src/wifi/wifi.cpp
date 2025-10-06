#include "../secrets.h"
#include <WiFi.h>
#include "wifi.h"

namespace comfoair {
  void WiFi::setup() {




      Serial.println();
      Serial.print("Connecting to ");
      Serial.println(WIFI_SSID);
      ::WiFi.begin(WIFI_SSID, WIFI_PASS);
      ::WiFi.setAutoReconnect(true);
      ::WiFi.persistent(false);
      ::WiFi.mode(WIFI_STA);
      ::WiFi.setSleep(false);                        // reliability > idle power
      ::WiFi.setTxPower(WIFI_POWER_8_5dBm);          // cooler; raise to 15 dBm if needed
      ::WiFi.setAutoReconnect(true);
      ::WiFi.setHostname("comfoair-bridge");
      while (::WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print(".");
      }
      Serial.println("");
      Serial.println("WiFi Connected. IP: ");
      Serial.println(::WiFi.localIP());
  }

  void WiFi::loop() {
      //  NOthing to do!
  }
} 