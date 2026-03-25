#ifndef COMFOAIRClass_H
#define COMFOAIRClass_H
#include "message.h"

// Forward declarations
namespace comfoair {
  class SensorDataManager;
  class FilterDataManager;
  class ControlManager;
  class TimeManager;
  class ErrorDataManager;  // ← NEW
}

namespace comfoair {
  class ComfoAir {
    public:
      ComfoAir();
      void setup();
      void loop();
      void setSensorDataManager(SensorDataManager* manager);
      void setFilterDataManager(FilterDataManager* manager);
      void setControlManager(ControlManager* manager);
      void setTimeManager(TimeManager* manager);
      void setErrorDataManager(ErrorDataManager* manager);  // ← NEW
      
      // Send CAN command
      bool sendCommand(const char* command);
      
      // Time synchronization methods
      void requestDeviceTime();
      void setDeviceTime(uint32_t device_seconds);
      
      // Data request methods (RTR)
      void requestFilterDays();        // PDOID 192 - Filter days remaining
      void requestTargetTemp();        // PDOID 212 - Target temperature
      void requestBypassStatus();      // PDOID 66  - Bypass activation mode
      void requestOperatingMode();     // PDOID 49  - Operating mode
      
    private:
      CAN_FRAME canMessage;
      ComfoMessage comfoMessage;
      DecodedMessage decodedMessage;
      SensorDataManager* sensorManager;
      FilterDataManager* filterManager;
      ControlManager* controlManager;
      TimeManager* timeManager;
      ErrorDataManager* errorManager;  // ← NEW
      
      // Echo detection: ignore MQTT commands matching CAN-confirmed speed
      unsigned long last_fan_speed_command_time;  // When we last sent a speed command
      uint8_t current_fan_speed;  // CAN-confirmed MVHR speed (used for echo detection)
      
      // Handle device time response
      void handleDeviceTimeResponse(uint32_t device_seconds);

      // Non-blocking slow data request state machine
      uint8_t slow_data_step = 0;
      unsigned long last_slow_data_step_time = 0;
  };
}

#endif