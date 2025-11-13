#include "../secrets.h"
#include <WiFi.h>
#include "wifi.h"
#include "../ui/GUI.h"
#include "../board_config.h"  // For hasDisplay()

namespace comfoair {
  
  WiFi::WiFi() : connected(false), 
                 last_status_check(0), 
                 last_reconnect_attempt(0),
                 connection_lost_time(0),
                 reconnect_attempts(0),
                 wifi_event_registered(false) {
  }
  
  void WiFi::setup() {
    Serial.println();
    Serial.print("WiFi: Connecting to ");
    Serial.println(WIFI_SSID);
    
    // Register WiFi event handlers BEFORE connecting
    if (!wifi_event_registered) {
      ::WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        this->onWiFiEvent(event, info);
      });
      wifi_event_registered = true;
    }
    
    // Configure WiFi before connecting
    ::WiFi.mode(WIFI_STA);
    ::WiFi.persistent(false);           // Don't save WiFi config to flash (reduces wear)
    ::WiFi.setAutoReconnect(true);      // Enable ESP32's built-in reconnection
    ::WiFi.setSleep(false);             // Disable WiFi sleep for better reliability
    ::WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Maximum power for better range
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

  void WiFi::onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch(event) {
      case ARDUINO_EVENT_WIFI_STA_CONNECTED:
        Serial.println("WiFi: Event - Connected to AP");
        break;
        
      case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.println("WiFi: Event - Got IP address");
        connected = true;
        reconnect_attempts = 0;
        logConnectionStats();
        updateWiFiIcon();
        break;
        
      case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        {
          wifi_err_reason_t reason = (wifi_err_reason_t)info.wifi_sta_disconnected.reason;
          Serial.printf("WiFi: Event - Disconnected (reason: %d - ", reason);
          
          switch(reason) {
            case WIFI_REASON_AUTH_EXPIRE:
            case WIFI_REASON_AUTH_LEAVE:
              Serial.print("Authentication expired/left");
              break;
            case WIFI_REASON_ASSOC_EXPIRE:
            case WIFI_REASON_ASSOC_LEAVE:
              Serial.print("Association expired/left");
              break;
            case WIFI_REASON_BEACON_TIMEOUT:
              Serial.print("Beacon timeout");
              break;
            case WIFI_REASON_NO_AP_FOUND:
              Serial.print("AP not found");
              break;
            case WIFI_REASON_HANDSHAKE_TIMEOUT:
              Serial.print("Handshake timeout");
              break;
            case WIFI_REASON_CONNECTION_FAIL:
              Serial.print("Connection failed");
              break;
            default:
              Serial.printf("Other - %d", reason);
              break;
          }
          Serial.println(")");
          
          if (connected) {
            connected = false;
            connection_lost_time = millis();
            reconnect_attempts = 0;
            updateWiFiIcon();
            
            // ESP32's setAutoReconnect(true) will handle reconnection automatically
            Serial.println("WiFi: Auto-reconnect will attempt to restore connection...");
          }
        }
        break;
        
      case ARDUINO_EVENT_WIFI_STA_LOST_IP:
        Serial.println("WiFi: Event - Lost IP address");
        break;
        
      default:
        break;
    }
  }

  void WiFi::loop() {
    unsigned long now = millis();
    
    // Periodically check WiFi status (less frequently now, as events handle most state changes)
    if (now - last_status_check >= STATUS_CHECK_INTERVAL) {
      wl_status_t status = ::WiFi.status();
      bool current_status = (status == WL_CONNECTED);
      
      // Only handle state changes not caught by events (redundant safety check)
      if (current_status != connected) {
        if (current_status) {
          Serial.println("WiFi: Status check detected connection (missed event?)");
          connected = true;
          reconnect_attempts = 0;
          logConnectionStats();
          updateWiFiIcon();
        } else if (connected) {
          Serial.println("WiFi: Status check detected disconnection (missed event?)");
          connected = false;
          connection_lost_time = now;
          reconnect_attempts = 0;
          updateWiFiIcon();
        }
      }
      
      // If disconnected for extended period, force a manual reconnection attempt
      // This is a fallback in case auto-reconnect gets stuck
      if (!connected) {
        unsigned long disconnected_duration = now - connection_lost_time;
        
        // After 2 minutes of failed auto-reconnect, try manual intervention
        if (disconnected_duration >= MANUAL_RECONNECT_THRESHOLD) {
          if (now - last_reconnect_attempt >= RECONNECT_INTERVAL) {
            reconnect_attempts++;
            Serial.printf("WiFi: Manual reconnection attempt #%d (auto-reconnect may be stuck)\n", 
                         reconnect_attempts);
            
            // Full reset and reconnect
            ::WiFi.disconnect(true, true);  // Erase config and turn off WiFi
            delay(1000);  // Allow proper shutdown
            
            // Reconfigure and reconnect
            ::WiFi.mode(WIFI_STA);
            ::WiFi.setAutoReconnect(true);
            ::WiFi.setSleep(false);
            ::WiFi.setTxPower(WIFI_POWER_19_5dBm);
            ::WiFi.begin(WIFI_SSID, WIFI_PASS);
            
            last_reconnect_attempt = now;
            connection_lost_time = now;  // Reset disconnection timer
            
            Serial.printf("WiFi: Total downtime: %lu seconds\n", disconnected_duration / 1000);
          }
        }
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
    Serial.println("================================================");
    Serial.println("WiFi: CONNECTION ESTABLISHED");
    Serial.println("================================================");
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
    
    Serial.println("================================================");
  }
  
  void WiFi::updateWiFiIcon() {
    // CRITICAL: Only update display if it exists (prevents crash on headless boards)
    if (!hasDisplay()) {
      return;
    }
    
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