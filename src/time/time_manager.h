#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include "time.h"

namespace comfoair {

class TimeManager {
public:
    TimeManager();
    void setup();
    void loop();
    void updateDisplay();

private:
    bool time_synced;
    unsigned long last_update;
    static const unsigned long UPDATE_INTERVAL = 15000; // Update every 15 second
    
    void syncTime();
    String getTimeString();
    String getDateString();
};

} // namespace comfoair

#endif