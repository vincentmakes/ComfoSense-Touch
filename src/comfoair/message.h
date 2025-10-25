#ifndef COMFOMESSAGE_H
#define COMFOMESSAGE_H

#include <inttypes.h>
#include <PubSubClient.h>
#include <map>
#include "twai_wrapper.h"  // Changed from esp32_can.h
#include <vector>      
#include <cstdint> 

namespace comfoair {
  struct DecodedMessage {
    char name[40];
    char val[15];
  };

  class ComfoMessage {
    public:
      ComfoMessage();
      void test(const char * test, const char * name, const char * expectedValue);
      bool send(uint8_t length, uint8_t * buf);
      bool send(std::vector<uint8_t> *buf);
      bool decode(CAN_FRAME *frame, DecodedMessage *message);
      bool sendCommand(char const * command);
      
      // Time synchronization methods
      bool requestTime();
      bool setTime(uint32_t secondsSince2000);
      bool setTimeFromDateTime(uint16_t year, uint8_t month, uint8_t day, 
                               uint8_t hour, uint8_t minute, uint8_t second);

    private:
      uint8_t sequence;
      
      // Helper for date/time calculations
      uint32_t dateTimeToSeconds(uint16_t year, uint8_t month, uint8_t day,
                                 uint8_t hour, uint8_t minute, uint8_t second);
      bool isLeapYear(uint16_t year);
  };
}

#endif