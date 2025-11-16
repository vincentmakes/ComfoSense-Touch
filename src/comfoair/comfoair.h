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
      
      // ✅ Time-based deduplication (tracks SENT commands, not CAN state)
      uint8_t last_sent_fan_speed;  // Last speed we SENT via command
      unsigned long last_fan_speed_command_time;  // When we sent it
      uint8_t current_fan_speed;  // Current speed from CAN (for display)
      
      // Handle device time response
      void handleDeviceTimeResponse(uint32_t device_seconds);
  };
}

#endif