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
      
    private:
      CAN_FRAME canMessage;
      ComfoMessage comfoMessage;
      DecodedMessage decodedMessage;
      SensorDataManager* sensorManager;
      FilterDataManager* filterManager;
      ControlManager* controlManager;
      TimeManager* timeManager;
      ErrorDataManager* errorManager;  // ← NEW
      
      // Handle device time response
      void handleDeviceTimeResponse(uint32_t device_seconds);
  };
}

#endif