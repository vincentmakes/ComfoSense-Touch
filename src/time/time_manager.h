#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include "time.h"

namespace comfoair {

// Forward declaration
class ComfoAir;

class TimeManager {
public:
    TimeManager();
    void setup();
    void loop();
    void updateDisplay();
    void setComfoAir(ComfoAir* comfo_ptr);
    
    // Callback for when device time is received from CAN bus
    void onDeviceTimeReceived(uint32_t device_seconds);

private:
    bool time_synced;
    unsigned long last_update;
    unsigned long last_sync_check;
    
    // OPTIMIZED: Display updates every 1 second (fast, no NTP calls)
    static const unsigned long UPDATE_INTERVAL = 10000; // 10 second
    
    // OPTIMIZED: NTP sync every 8 hours instead of every hour
    static const unsigned long SYNC_CHECK_INTERVAL = 28800000; // 8 hours (8 * 60 * 60 * 1000)
    
    static const int TIME_DIFFERENCE_THRESHOLD = 60; // 60 seconds = 1 minute
    
    ComfoAir* comfoair;
    bool waiting_for_device_time;
    unsigned long device_time_request_timestamp;
    
    void syncTime();
    void checkAndSyncDeviceTime();
    void setDeviceTime(time_t ntp_time);
    String getTimeString();
    String getDateString();
    
    // Helper to convert time_t to seconds since 2000-01-01
    uint32_t timeToDeviceSeconds(time_t unix_time);
    // Helper to convert device seconds to time_t
    time_t deviceSecondsToTime(uint32_t device_seconds);
};

} // namespace comfoair

#endif