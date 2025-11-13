#include "comfoair.h"
#include "commands.h"
#include "sensor_data.h"
#include "filter_data.h"
#include "control_manager.h"
#include "error_data.h" 
#include "../time/time_manager.h"
#include "../mqtt/mqtt.h"
#include "../secrets.h"

#include "../serial_logger.h"
#define Serial LogSerial 

// Only include TWAI wrapper in non-remote client mode
#if !defined(REMOTE_CLIENT_MODE) || !REMOTE_CLIENT_MODE
  #include "twai_wrapper.h"
#endif


void printFrame2(CAN_FRAME *message)
{ /*
  Serial.print(message->id, HEX);
  if (message->extended) Serial.print(" X ");
  else Serial.print(" S ");   
  Serial.print(message->length, DEC);
  for (int i = 0; i < message->length; i++) {
    Serial.print(" ");
    if (message->data.byte[i] < 0x10) Serial.print("0");
    Serial.print(message->data.byte[i], HEX); 
  }
  Serial.println(); */
}

// ============================================================================
// âœ… PROPER: Deduplication based on LAST SENT COMMAND (not CAN state)
// This prevents echo loops while allowing legitimate commands
// ============================================================================
#define subscribe(command) if (mqtt) { mqtt->subscribeTo(MQTT_PREFIX "/commands/" command, [this](char const * _1,uint8_t const * _2, int _3) { \
    Serial.print("Received: "); \
    Serial.println(command); \
    \
    /* Extract requested speed from command */ \
    uint8_t requested_speed = 255; /* 255 = not a speed command */ \
    if (strcmp(command, "ventilation_level_0") == 0) requested_speed = 0; \
    else if (strcmp(command, "ventilation_level_1") == 0) requested_speed = 1; \
    else if (strcmp(command, "ventilation_level_2") == 0) requested_speed = 2; \
    else if (strcmp(command, "ventilation_level_3") == 0) requested_speed = 3; \
    \
    /* Check if this is a duplicate of the last command we sent */ \
    unsigned long now = millis(); \
    bool is_duplicate = (requested_speed != 255 && \
                        requested_speed == this->last_sent_fan_speed && \
                        now - this->last_fan_speed_command_time < 2000); /* 2 second window */ \
    \
    if (is_duplicate) { \
      Serial.printf("    Duplicate ignored - just sent speed %d %lu ms ago\n", \
                    requested_speed, now - this->last_fan_speed_command_time); \
    } else { \
      Serial.printf("  Sending command to MVHR\n"); \
      this->comfoMessage.sendCommand(command); \
      \
      /* Track this command */ \
      if (requested_speed != 255) { \
        this->last_sent_fan_speed = requested_speed; \
        this->last_fan_speed_command_time = now; \
      } \
    } \
  }); }

extern comfoair::MQTT *mqtt;
char mqttTopicMsgBuf[30];
char mqttTopicValBuf[30];
char otherBuf[30];


namespace comfoair {
  ComfoAir::ComfoAir() : 
    sensorManager(nullptr), 
    filterManager(nullptr), 
    controlManager(nullptr),
    timeManager(nullptr),
    errorManager(nullptr),
    last_sent_fan_speed(255),
    last_fan_speed_command_time(0),
    current_fan_speed(255) {}  // â† Time-based deduplication

  void ComfoAir::setSensorDataManager(SensorDataManager* manager) {
    sensorManager = manager;
    Serial.println("ComfoAir: SensorDataManager linked ");
  }

  void ComfoAir::setFilterDataManager(FilterDataManager* manager) {
    filterManager = manager;
    Serial.println("ComfoAir: FilterDataManager linked ");
  }

  void ComfoAir::setControlManager(ControlManager* manager) {
    controlManager = manager;
    Serial.println("ComfoAir: ControlManager linked ");
  }

  void ComfoAir::setTimeManager(TimeManager* manager) {
    timeManager = manager;
    Serial.println("ComfoAir: TimeManager linked ");
  }

  // ============================================================================
  // â† NEW FUNCTION
  // ============================================================================
  void ComfoAir::setErrorDataManager(ErrorDataManager* manager) {
    errorManager = manager;
    Serial.println("ComfoAir: ErrorDataManager linked ");
  }
  // ============================================================================

  bool ComfoAir::sendCommand(const char* command) {
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
      Serial.println("ComfoAir: sendCommand() called in Remote Client Mode - command ignored");
      return false;
    #else
      return comfoMessage.sendCommand(command);
    #endif
  }

  void ComfoAir::requestDeviceTime() {
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
      Serial.println("ComfoAir: requestDeviceTime() not supported in Remote Client Mode");
    #else
      Serial.println("ComfoAir: Requesting device time via CAN...");
      comfoMessage.requestTime();
    #endif
  }

  void ComfoAir::setDeviceTime(uint32_t device_seconds) {
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
      Serial.println("ComfoAir: setDeviceTime() not supported in Remote Client Mode");
    #else
      Serial.printf("ComfoAir: Setting device time to %u seconds since 2000-01-01\n", 
                    device_seconds);
      comfoMessage.setTime(device_seconds);
    #endif
  }

  void ComfoAir::handleDeviceTimeResponse(uint32_t device_seconds) {
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
      Serial.println("ComfoAir: handleDeviceTimeResponse() not used in Remote Client Mode");
    #else
      if (timeManager) {
        timeManager->onDeviceTimeReceived(device_seconds);
      } else {
        Serial.println("ComfoAir: Received device time but TimeManager not linked!");
      }
    #endif
  }

  // ============================================================================
  // âœ… NEW: Request filter days remaining
  // ============================================================================
  void ComfoAir::requestFilterDays() {
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
      Serial.println("ComfoAir: requestFilterDays() not supported in Remote Client Mode");
    #else
      Serial.println("ComfoAir: Requesting filter days via CAN...");
      // Send RTR - uses local variable, won't pollute global message state
      // MVHR will respond when ready, we don't wait for response here
      comfoMessage.requestFilterDays();
    #endif
  }

  void ComfoAir::setup() {

    
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
      // ========================================================================
      // REMOTE CLIENT MODE - NO CAN INITIALIZATION
      // ========================================================================
      Serial.println("\n=== Remote Client Mode - CAN Bus DISABLED ===");
      Serial.println("All communication via MQTT");
      Serial.println("Waiting for MQTT data from bridge device...");
      Serial.println("=== Remote Client Ready ===\n");
      
    #else
      // ========================================================================
      // NORMAL MODE - INITIALIZE CAN BUS
      // ========================================================================
      Serial.println("\n=== CAN Bus Initialization ===");
      Serial.println("Board: Waveshare ESP32-S3-Touch-LCD-4");
      Serial.println("Using native TWAI driver");
      
      // CAN pins set in twai_wrapper.h (GPIO6 TX, GPIO0 RX)
      if (!CAN0.begin(50000)) {
        Serial.println("CAN init FAILED!");
        return;
      }
      CAN0.watchFor();
      
      Serial.println("CAN initialized at 50 kbps");
      Serial.println("=== CAN Bus Ready ===\n");
      
      // Subscribe to MQTT commands (only if MQTT is enabled)
      if (mqtt) {
          // Subscribe to MQTT commands
          subscribe("ventilation_level_0");
          subscribe("ventilation_level_1");
          subscribe("ventilation_level_2");
          subscribe("ventilation_level_3");
          subscribe("boost_10_min");
          subscribe("boost_20_min");
          subscribe("boost_30_min");
          subscribe("boost_60_min");
          subscribe("boost_end");
          subscribe("auto");
          subscribe("manual");
          subscribe("bypass_activate_1h");
          subscribe("bypass_deactivate_1h");
          subscribe("bypass_auto");
          subscribe("ventilation_supply_only");
          subscribe("ventilation_supply_only_reset");
          subscribe("ventilation_extract_only");
          subscribe("ventilation_extract_only_reset");
          subscribe("temp_profile_normal");
          subscribe("temp_profile_cool");
          subscribe("temp_profile_warm");

          mqtt->subscribeTo(MQTT_PREFIX "/commands/" "ventilation_level", [this](char const * _1,uint8_t const * _2, int _3) {
            sprintf(otherBuf, "ventilation_level_%d", _2[0] - 48);
            Serial.print("Received: ");
            Serial.println(_1);
            Serial.println(otherBuf);

            
            // Check for duplicate
            uint8_t requested_speed = _2[0] - 48;
            unsigned long now = millis();
            bool is_duplicate = (requested_speed <= 3 && 
                                requested_speed == this->last_sent_fan_speed && 
                                now - this->last_fan_speed_command_time < 2000);
            
            if (is_duplicate) {
              Serial.printf(" Duplicate ignored - just sent speed %d %lu ms ago\n", 
                            requested_speed, now - this->last_fan_speed_command_time);

            } else {
              Serial.printf("  Sending command to MVHR\n");

              this->last_sent_fan_speed = requested_speed;
              this->last_fan_speed_command_time = now;
              return this->comfoMessage.sendCommand(otherBuf);
            }
          });
          
          mqtt->subscribeTo(MQTT_PREFIX "/commands/" "set_mode", [this](char const * _1,uint8_t const * _2, int _3) {
            if (memcmp("auto", _2, 4) == 0) {
              Serial.print("Received: ");
              Serial.println(_1);
              return this->comfoMessage.sendCommand("auto");
            } else {
              return this->comfoMessage.sendCommand("manual");
            }
          });
             Serial.println("MQTT subscriptions complete");

      } else {
        Serial.println(" MQTT disabled - skipping subscriptions");
      }
      
      // ✅ Request filter days at startup (after 10 seconds)
      // Filter data changes very slowly, so no rush to get it immediately
      Serial.println("ComfoAir: Waiting 8s before requesting filter days...");
      delay(8000);
      requestFilterDays();

    #endif
  }
  
  void ComfoAir::loop() {
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
      // In remote client mode, do nothing - all data comes from MQTT
      return;
      
    #else
      // ========================================================================
      // NORMAL MODE - PROCESS CAN BUS MESSAGES
      // ========================================================================
      static bool first_message = true;
      static unsigned long last_can_rx_report = 0;
      static int can_rx_count = 0;
      
      // ✅ Request filter days periodically (every 6 hours)
      // Filter data changes very slowly (days), so hourly requests are excessive
      static unsigned long last_filter_request = 0;
      static bool filter_request_initialized = false;
      
      // Initialize on first loop to prevent immediate request after startup
      if (!filter_request_initialized) {
        last_filter_request = millis();
        filter_request_initialized = true;
      }
      
      if (millis() - last_filter_request >= 14400000) {  // 21600000ms = 6 hours // 14400000 = 4he
        Serial.println("ComfoAir: 6-hour filter days request...");
        requestFilterDays();
        last_filter_request = millis();
      }
      
      CAN_FRAME incoming;
      while (CAN0.read(incoming)) {
        can_rx_count++;
        
        // Report CAN RX rate every 10 seconds
        if (millis() - last_can_rx_report >= 10000) {
          Serial.printf("[CAN] Received %d frames in last 10s (%.1f/sec)\n", 
                        can_rx_count, can_rx_count / 10.0);
          can_rx_count = 0;
          last_can_rx_report = millis();
        }
        
        this->canMessage = incoming;
        
        if (first_message) {
          Serial.println("\nFIRST CAN FRAME RECEIVED! ");
          first_message = false;
        }
        
        printFrame2(&this->canMessage);
        
        // âœ… CRITICAL: Zero out the decoded message structure before decoding
        memset(&this->decodedMessage, 0, sizeof(DecodedMessage));
        
        if (this->comfoMessage.decode(&this->canMessage, &this->decodedMessage)) {
          // âœ… Make local copies IMMEDIATELY after decode to prevent corruption
          char decoded_name[40];
          char decoded_val[15];
          strncpy(decoded_name, this->decodedMessage.name, 39);
          decoded_name[39] = '\0';
          strncpy(decoded_val, this->decodedMessage.val, 14);
          decoded_val[14] = '\0';
          
          /*
          Serial.print("  â†’ ");
          Serial.print(decoded_name);
          Serial.print(" = ");
          Serial.println(decoded_val);
          */
          /* DEBUG: Print manager status and message details
          Serial.printf("  â†’ Manager status: sensorManager=%s, filterManager=%s, controlManager=%s\n",
                        sensorManager ? "LINKED" : "NULL",
                        filterManager ? "LINKED" : "NULL",
                        controlManager ? "LINKED" : "NULL");
          Serial.printf("  â†’ Message name: '%s' (length=%d)\n", 
                        this->decodedMessage.name, 
                        strlen(this->decodedMessage.name));
          */
          // **Check for device time response**
          if (strcmp(decoded_name, "device_time") == 0) {
            uint32_t device_seconds = strtoul(decoded_val, NULL, 10);
            Serial.printf(" Device time: %u seconds since 2000\n", device_seconds);
            handleDeviceTimeResponse(device_seconds);
          }
          
          /*DEBUG: Print manager status and message details 
          Serial.printf("  â†’ Manager status: sensorManager=%s, filterManager=%s, controlManager=%s\n",
                        sensorManager ? "LINKED" : "NULL",
                        filterManager ? "LINKED" : "NULL",
                        controlManager ? "LINKED" : "NULL");
          Serial.printf("  â†’ Message name: '%s' (length=%d)\n", 
                        decoded_name, 
                        strlen(decoded_name));
                        */
          
          // Publish to MQTT - use local copies
          if (mqtt) {
            sprintf(mqttTopicMsgBuf, "%s/%s", MQTT_PREFIX, decoded_name);
            sprintf(mqttTopicValBuf, "%s", decoded_val);
            mqtt->writeToTopic(mqttTopicMsgBuf, mqttTopicValBuf);
          }
          // âœ… DEBUG: Check routing logic
         // Serial.println("  â†’ Checking sensor data routing...");
          
          // Route sensor data
          if (sensorManager) {
            if (strcmp(decoded_name, "extract_air_temp") == 0) {
              Serial.println(" MATCH: extract_air_temp - calling updateInsideTemp()");
              sensorManager->updateInsideTemp(atof(decoded_val));
            }
            else if (strcmp(decoded_name, "outdoor_air_temp") == 0) {
              Serial.println("  MATCH: outdoor_air_temp - calling updateOutsideTemp()");
              sensorManager->updateOutsideTemp(atof(decoded_val));
            }
            else if (strcmp(decoded_name, "extract_air_humidity") == 0) {
              Serial.println("  MATCH: extract_air_humidity - calling updateInsideHumidity()");
              sensorManager->updateInsideHumidity(atof(decoded_val));
            }
            else if (strcmp(decoded_name, "outdoor_air_humidity") == 0) {
              Serial.println("  MATCH: outdoor_air_humidity - calling updateOutsideHumidity()");
              sensorManager->updateOutsideHumidity(atof(decoded_val));
            }
            else {
              //Serial.printf("  âœ— NO MATCH: '%s' is not a sensor data message\n", decoded_name);
            }
          } else {
            // Serial.println("  sensorManager is NULL!"); // Note: sensorManager is NULL in bridge mode (headless) - this is expected
          }
          
          // Route filter data
          if (filterManager) {
            if (strcmp(decoded_name, "remaining_days_filter_replacement") == 0) {
              Serial.println("  MATCH: remaining_days_filter_replacement - calling updateFilterDays()");
              filterManager->updateFilterDays(atoi(decoded_val));
            }
          }
          
          // Route control feedback
          if (controlManager) {
            if (strcmp(decoded_name, "fan_speed") == 0) {
              Serial.println("  MATCH: fan_speed - calling updateFanSpeedFromCAN()");
              
              // âœ… FIXED: Track current fan speed for deduplication
              current_fan_speed = atoi(decoded_val);
              
              controlManager->updateFanSpeedFromCAN(current_fan_speed);
            }
            else if (strcmp(decoded_name, "temp_profile") == 0) {
              Serial.println(" MATCH: temp_profile - calling updateTempProfileFromCAN()");
              uint8_t profile = 0;
              if (strcmp(decoded_val, "cold") == 0) profile = 1;
              else if (strcmp(decoded_val, "warm") == 0) profile = 2;
              controlManager->updateTempProfileFromCAN(profile);
            }
          }
          
          // ============================================================================
          // â† NEW: Route error/alarm messages
          // ============================================================================
          if (errorManager) {
            // Determine if error is active based on value string
            bool error_value = (strcmp(decoded_val, "ACTIVE") == 0 || 
                              strcmp(decoded_val, "REPLACE") == 0 ||
                              strcmp(decoded_val, "WARNING") == 0);
            
            if (strcmp(decoded_name, "error_overheating") == 0) {
              errorManager->updateErrorOverheating(error_value);
            }
            else if (strcmp(decoded_name, "error_temp_sensor_p_oda") == 0) {
              errorManager->updateErrorTempSensorPODA(error_value);
            }
            else if (strcmp(decoded_name, "error_preheat_location") == 0) {
              errorManager->updateErrorPreheatLocation(error_value);
            }
            else if (strcmp(decoded_name, "error_ext_pressure_eha") == 0) {
              errorManager->updateErrorExtPressureEHA(error_value);
            }
            else if (strcmp(decoded_name, "error_ext_pressure_sup") == 0) {
              errorManager->updateErrorExtPressureSUP(error_value);
            }
            else if (strcmp(decoded_name, "error_tempcontrol_p_oda") == 0) {
              errorManager->updateErrorTempControlPODA(error_value);
            }
            else if (strcmp(decoded_name, "error_tempcontrol_sup") == 0) {
              errorManager->updateErrorTempControlSUP(error_value);
            }
            else if (strcmp(decoded_name, "alarm_filter") == 0) {
              errorManager->updateAlarmFilter(error_value);
            }
            else if (strcmp(decoded_name, "warning_system") == 0) {
              errorManager->updateWarningSystem(error_value);
            }
          }
          // ============================================================================
        }
      }
    #endif
  }
}