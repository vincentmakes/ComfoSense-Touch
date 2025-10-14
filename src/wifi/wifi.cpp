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
    ::WiFi.setSleep(false);                        // reliability > idle power
    ::WiFi.setTxPower(WIFI_POWER_8_5dBm);          // cooler; raise to 15 dBm if needed
    ::WiFi.setAutoReconnect(true);
    ::WiFi.setHostname("comfoair-bridge");
    
    // Start as disconnected
    connected = false;
    Serial.println("WiFi initial state: disconnected (connected=false)");
    
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
        Serial.println("WiFi state after connection: connected (connected=true)");
        
        // CRITICAL: Force immediate icon update when connected during setup
        Serial.println("Calling updateWiFiIcon() immediately after connection in setup()");
        updateWiFiIcon();
    } else {
        Serial.println("");
        Serial.println("WiFi connection failed - continuing without WiFi");
        connected = false;
        Serial.println("WiFi state: disconnected (connected=false)");
    }
    
    // Note: Icon will be updated in first loop() call after GUI is ready
  }

  void WiFi::loop() {
    // Periodically check WiFi status
    unsigned long now = millis();
    if (now - last_status_check >= STATUS_CHECK_INTERVAL) {
      bool current_status = (::WiFi.status() == WL_CONNECTED);
      
      Serial.printf("[WiFi Loop] Current WiFi status: %s, connected variable: %s\n", 
                    current_status ? "CONNECTED" : "DISCONNECTED",
                    connected ? "true" : "false");
      
      // Only update icon when WiFi connects (disconnected -> connected)
      // Icon starts at 50% opacity by default, only brighten when connected
      if (current_status && !connected) {
        Serial.println("[WiFi Loop] State change detected: DISCONNECTED -> CONNECTED");
        connected = true;
        updateWiFiIcon();
        Serial.println("WiFi connected - updating icon to 100% opacity");
      }
      // If WiFi disconnects, update back to 50%
      else if (!current_status && connected) {
        Serial.println("[WiFi Loop] State change detected: CONNECTED -> DISCONNECTED");
        connected = false;
        updateWiFiIcon();
        Serial.println("WiFi disconnected - updating icon to 50% opacity");
      }
      else {
        Serial.println("[WiFi Loop] No state change - icon update skipped");
      }
      
      last_status_check = now;
    }
  }
  
  bool WiFi::isConnected() {
    return connected;
  }
  
  void WiFi::updateWiFiIcon() {
    Serial.printf("WiFi status: %s\n", connected ? "Connected" : "Disconnected");
    
    // Set opacity based on connection status
    // Connected: LV_OPA_COVER (255) = fully opaque
    // Disconnected: LV_OPA_50 (127) = 50% opacity
    lv_opa_t opacity = connected ? LV_OPA_COVER : LV_OPA_50;
    
    Serial.printf("Setting WiFi icon opacity to: %d (%s)\n", 
                  opacity, 
                  opacity == LV_OPA_COVER ? "100%" : "50%");
    
    // Strategy 5 pattern (from PROJECT_MEMORY_UPDATED.md):
    // 1. Set the style property
    lv_obj_set_style_opa(GUI_Image__screen__wifi, opacity, 0);
    
    Serial.println("Style opacity set");
    
    // 2. Request display refresh BEFORE invalidate (CRITICAL ORDER)
    GUI_request_display_refresh();
    
    Serial.println("Display refresh requested");
    
    // 3. Invalidate the object
    lv_obj_invalidate(GUI_Image__screen__wifi);
    
    Serial.println("Object invalidated");
    Serial.printf("WiFi icon update complete - should be at %d%% opacity\n", 
                  opacity == LV_OPA_COVER ? 100 : 50);
  }
  
}