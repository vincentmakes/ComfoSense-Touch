#include "../secrets.h"
#include <WiFi.h>
#include "wifi.h"
#include "../ui/GUI.h"



namespace comfoair {
  
  WiFi::WiFi() : connected(false), 
                 last_status_check(0), 
                 last_reconnect_attempt(0),
                 connection_lost_time(0),
                 reconnect_attempts(0) {
  }
  
  void WiFi::setup() {
    Serial.println();
    Serial.print("WiFi: Connecting to ");
    Serial.println(WIFI_SSID);
    
    // Configure WiFi before connecting
    ::WiFi.mode(WIFI_STA);
    ::WiFi.persistent(false);           // Don't save WiFi config to flash (reduces wear)
    ::WiFi.setAutoReconnect(true);      // Enable ESP32's built-in reconnection
    ::WiFi.setSleep(false);             // Disable WiFi sleep for better reliability
    ::WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Maximum power for better range (was 8.5dBm)
    ::WiFi.setHostname("comfoair-bridge");
    
    // Start connection
    ::WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    // Start as disconnected
    connected = false;
    
    // Non-blocking connection attempt with timeout
    Serial.print("WiFi: Waiting for connection");
    unsigned long start_time = millis();
    int dots = 0;
    
    while (::WiFi.status() != WL_CONNECTED && 
           (millis() - start_time) < CONNECTION_TIMEOUT) {
        delay(500);
        Serial.print(".");
        dots++;
        
        // Show progress every 10 dots
        if (dots % 10 == 0) {
          Serial.printf(" %ds", (millis() - start_time) / 1000);
        }
    }
    Serial.println();
    
    if (::WiFi.status() == WL_CONNECTED) {
        connected = true;
        reconnect_attempts = 0;
        logConnectionStats();
        updateWiFiIcon();
    } else {
        Serial.println("WiFi: Initial connection failed - will retry in background");
        Serial.println("WiFi: System will continue without WiFi for now");
        connected = false;
        connection_lost_time = millis();
    }
  }

  void WiFi::loop() {
    unsigned long now = millis();
    
    // Periodically check WiFi status
    if (now - last_status_check >= STATUS_CHECK_INTERVAL) {
      wl_status_t status = ::WiFi.status();
      bool current_status = (status == WL_CONNECTED);
      
      // Connection state changed
      if (current_status != connected) {
        if (current_status) {
          // Just connected!
          Serial.println("WiFi: Connection restored!");
          connected = true;
          reconnect_attempts = 0;
          logConnectionStats();
          updateWiFiIcon();
        } else {
          // Just disconnected
          Serial.printf("WiFi: Connection lost (reason: ");
          switch(status) {
            case WL_NO_SSID_AVAIL:
              Serial.print("SSID not available");
              break;
            case WL_CONNECT_FAILED:
              Serial.print("connection failed");
              break;
            case WL_CONNECTION_LOST:
              Serial.print("connection lost");
              break;
            case WL_DISCONNECTED:
              Serial.print("disconnected");
              break;
            default:
              Serial.printf("unknown status %d", status);
              break;
          }
          Serial.println(")");
          
          connected = false;
          connection_lost_time = now;
          reconnect_attempts = 0;
          updateWiFiIcon();
        }
      }
      
      // If disconnected, attempt reconnection
      if (!connected) {
        attemptReconnect();
      } else {
        // Connected - log signal strength periodically (every 30 seconds)
        static unsigned long last_rssi_log = 0;
        if (now - last_rssi_log >= 30000) {
          int8_t rssi = getSignalStrength();
          if (rssi < -80) {
            Serial.printf("WiFi: Warning - weak signal: %d dBm\n", rssi);
          }
          last_rssi_log = now;
        }
      }
      
      last_status_check = now;
    }
  }
  
  void WiFi::attemptReconnect() {
    unsigned long now = millis();
    
    // Determine reconnect interval (fast for first attempts, slower after)
    unsigned long interval = (reconnect_attempts < MAX_FAST_RECONNECT_ATTEMPTS) 
                             ? RECONNECT_FAST_INTERVAL 
                             : RECONNECT_INTERVAL;
    
    // Time to try reconnecting?
    if (now - last_reconnect_attempt >= interval) {
      reconnect_attempts++;
      
      Serial.printf("WiFi: Reconnection attempt #%d", reconnect_attempts);
      if (reconnect_attempts <= MAX_FAST_RECONNECT_ATTEMPTS) {
        Serial.print(" (fast mode)");
      }
      Serial.println();
      
      // Disconnect first to clean up state
      ::WiFi.disconnect(false, false);  // Don't erase config, don't turn off WiFi
      delay(100);
      
      // Attempt to reconnect
      ::WiFi.begin(WIFI_SSID, WIFI_PASS);
      
      last_reconnect_attempt = now;
      
      // Log how long we've been disconnected
      unsigned long disconnected_duration = (now - connection_lost_time) / 1000;
      Serial.printf("WiFi: Disconnected for %lu seconds\n", disconnected_duration);
    }
  }
  
  bool WiFi::isConnected() {
    return connected;
  }
  
  int8_t WiFi::getSignalStrength() {
    if (!connected) {
      return -100;  // Return very weak signal if not connected
    }
    return ::WiFi.RSSI();  // Returns signal strength in dBm
  }
  
  void WiFi::logConnectionStats() {
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.println("WiFi: CONNECTION ESTABLISHED");
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    Serial.print("  IP Address:    ");
    Serial.println(::WiFi.localIP());
    Serial.print("  Gateway:       ");
    Serial.println(::WiFi.gatewayIP());
    Serial.print("  Subnet Mask:   ");
    Serial.println(::WiFi.subnetMask());
    Serial.print("  MAC Address:   ");
    Serial.println(::WiFi.macAddress());
    
    int8_t rssi = getSignalStrength();
    Serial.print("  Signal (RSSI): ");
    Serial.print(rssi);
    Serial.print(" dBm");
    
    // Signal quality interpretation
    if (rssi >= -50) {
      Serial.println(" (Excellent)");
    } else if (rssi >= -60) {
      Serial.println(" (Good)");
    } else if (rssi >= -70) {
      Serial.println(" (Fair)");
    } else if (rssi >= -80) {
      Serial.println(" (Weak)");
    } else {
      Serial.println(" (Very Weak - expect issues)");
    }
    
    Serial.print("  Channel:       ");
    Serial.println(::WiFi.channel());
    
    if (reconnect_attempts > 0) {
      unsigned long downtime = (millis() - connection_lost_time) / 1000;
      Serial.printf("  Reconnected after %d attempts (%lu seconds)\n", 
                    reconnect_attempts, downtime);
    }
    
    Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
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