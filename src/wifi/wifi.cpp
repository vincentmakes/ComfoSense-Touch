#include "../secrets.h"
#include <WiFi.h>
#include "wifi.h"
#include "../ui/GUI.h"

namespace comfoair {
  
  WiFi::WiFi() : connected(false), last_status_check(0) {
  }
  
  void WiFi::setup() {
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);
    
    ::WiFi.begin(WIFI_SSID, WIFI_PASS);
    ::WiFi.setAutoReconnect(true);
    ::WiFi.persistent(false);
    ::WiFi.mode(WIFI_STA);
    ::WiFi.setSleep(false);
    ::WiFi.setTxPower(WIFI_POWER_8_5dBm);
    ::WiFi.setAutoReconnect(true);
    ::WiFi.setHostname("comfoair-bridge");
    
    // Start as disconnected
    connected = false;
    
    // Non-blocking connection attempt (max 20 seconds)
    int retries = 0;
    while (::WiFi.status() != WL_CONNECTED && retries < 40) {
        delay(500);
        Serial.print(".");
        retries++;
    }
    
    if (::WiFi.status() == WL_CONNECTED) {
        Serial.println("");
        Serial.println("WiFi Connected. IP: ");
        Serial.println(::WiFi.localIP());
        connected = true;
        updateWiFiIcon();
    } else {
        Serial.println("");
        Serial.println("WiFi connection failed - continuing without WiFi");
        connected = false;
    }
  }

  void WiFi::loop() {
    // Periodically check WiFi status
    unsigned long now = millis();
    if (now - last_status_check >= STATUS_CHECK_INTERVAL) {
      bool current_status = (::WiFi.status() == WL_CONNECTED);
      
      // Only update icon when WiFi state changes
      if (current_status && !connected) {
        connected = true;
        updateWiFiIcon();
      }
      else if (!current_status && connected) {
        connected = false;
        updateWiFiIcon();
      }
      
      last_status_check = now;
    }
  }
  
  bool WiFi::isConnected() {
    return connected;
  }
  
  void WiFi::updateWiFiIcon() {
    // Set opacity based on connection status
    lv_opa_t opacity = connected ? LV_OPA_COVER : LV_OPA_50;
    
    // Strategy 5 pattern (from PROJECT_MEMORY_UPDATED.md):
    // 1. Set the style property
    lv_obj_set_style_opa(GUI_Image__screen__wifi, opacity, 0);
    
    // 2. Request display refresh BEFORE invalidate (CRITICAL ORDER)
    GUI_request_display_refresh();
    
    // 3. Invalidate the object
    lv_obj_invalidate(GUI_Image__screen__wifi);
  }
  
}