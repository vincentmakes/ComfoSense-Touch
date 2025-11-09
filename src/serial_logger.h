#ifndef SERIAL_LOGGER_H
#define SERIAL_LOGGER_H

#include <Arduino.h>
#include "ota/ota.h"

// Custom Print wrapper that logs everything to both Serial and OTA buffer
class SerialLogger : public Print {
  private:
    static const int LINE_BUFFER_SIZE = 256;
    char currentLine[LINE_BUFFER_SIZE];
    int linePos = 0;
    
  public:
    // Pass-through for Serial.begin()
    void begin(unsigned long baud) {
      Serial.begin(baud);
      currentLine[0] = '\0';
      linePos = 0;
    }
    
    // Override write() - this is called by all print/println functions
    size_t write(uint8_t c) override {
      // Write to actual Serial
      Serial.write(c);
      
      // Build up line for OTA log buffer
      if (c == '\n') {
        // Line complete - add to OTA log
        if (linePos > 0) {
          currentLine[linePos] = '\0';  // Null terminate
          comfoair::OTA::addLog(currentLine);
          linePos = 0;
        }
      } else if (c != '\r') {  // Ignore carriage returns
        if (linePos < LINE_BUFFER_SIZE - 1) {
          currentLine[linePos++] = (char)c;
        }
      }
      
      return 1;
    }
    
    // Override write for buffer (for efficiency)
    size_t write(const uint8_t *buffer, size_t size) override {
      // Write to actual Serial
      Serial.write(buffer, size);
      
      // Process each byte for OTA log
      for (size_t i = 0; i < size; i++) {
        uint8_t c = buffer[i];
        if (c == '\n') {
          if (linePos > 0) {
            currentLine[linePos] = '\0';  // Null terminate
            comfoair::OTA::addLog(currentLine);
            linePos = 0;
          }
        } else if (c != '\r') {
          if (linePos < LINE_BUFFER_SIZE - 1) {
            currentLine[linePos++] = (char)c;
          }
        }
      }
      
      return size;
    }
};

// Global instance to replace Serial
extern SerialLogger LogSerial;

#endif