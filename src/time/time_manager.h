#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>

namespace comfoair {
  class TimeManager {
    public:
      TimeManager();
      void setup();
      void loop();
      
      // Get formatted time string (HH:MM)
      String getTimeString();
      
      // Get formatted date string (dd mmmm yyyy)
      String getDateString();
      
      // Check if time is synchronized
      bool isTimeSynced();
      
    private:
      bool time_synced;
      unsigned long last_update;
      static const unsigned long UPDATE_INTERVAL = 60000; // Update every minute
      
      void syncTime();
      void updateDisplay();
  };
}

#endif