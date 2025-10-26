#ifndef FILTER_DATA_H
#define FILTER_DATA_H

#include <Arduino.h>
#include "../secrets.h"

namespace comfoair {

class FilterDataManager {
public:
    FilterDataManager();
    
    void setup();
    void loop();
    
    // Update filter days from CAN
    void updateFilterDays(int days);
    
    // Get current filter days
    int getFilterDays();
    
private:
    int filter_days_remaining;
    bool has_data;
    unsigned long last_update;
    
    // Update GUI display
    void updateDisplay();
    
    // Update warning icon visibility
    void updateWarningIcon();
    
    // Check if we should use dummy data (no CAN update for >24 hours)
    bool shouldUseDummyData();
    
    static const unsigned long DATA_TIMEOUT = 86400000; // 24 hours in milliseconds
    static const int DUMMY_DAYS = 99;

    #ifdef WARNING_THRESHOLD_DAYS
            static const int WARNING_THRESHOLD = WARNING_THRESHOLD_DAYS;  //as per config in secrets.h
    #else
            static const int WARNING_THRESHOLD = 100 ; // Show warning when <= 21 days
    #endif


};

} // namespace comfoair

#endif
