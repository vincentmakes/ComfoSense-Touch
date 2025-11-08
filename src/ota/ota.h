#ifndef OTA_H
#define OTA_H

#include <Arduino.h>

namespace comfoair {
  class OTA {
    public:
      void setup();
      void loop();
      
      // Serial logging buffer
      static void addLog(const char* message);
      
    private:
      static const int LOG_BUFFER_SIZE = 100;  // Keep last 100 messages
      static String logBuffer[LOG_BUFFER_SIZE];
      static int logIndex;
      static int logCount;
  };
}

#endif