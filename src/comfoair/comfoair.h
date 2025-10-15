#ifndef COMFOAIRClass_H
#define COMFOAIRClass_H
#include "message.h"

// Forward declarations
namespace comfoair {
  class SensorDataManager;
  class FilterDataManager;
  class ControlManager;
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
      
      // Send CAN command
      bool sendCommand(const char* command);
      
    private:
      CAN_FRAME canMessage;
      ComfoMessage comfoMessage;
      DecodedMessage decodedMessage;
      SensorDataManager* sensorManager;
      FilterDataManager* filterManager;
      ControlManager* controlManager;
  };
}

#endif