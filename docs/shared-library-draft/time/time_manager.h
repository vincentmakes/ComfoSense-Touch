#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <Arduino.h>
#include <functional>
#include "time.h"

namespace comfoair {

// Callback types for device time sync (CAN-specific, set by host project)
using DeviceTimeSyncCallback = std::function<void(time_t ntp_time)>;
using DeviceTimeRequestCallback = std::function<void()>;

class TimeManager {
public:
    TimeManager();
    void setup();
    void loop();
    void updateDisplay();

    // Set timezone (POSIX format, e.g., "CET-1CEST,M3.5.0,M10.5.0/3")
    void setTimezone(const char* tz) {
        timezone = tz;
    }

    // Set remote client mode (skips device time sync)
    void setRemoteClientMode(bool remote) {
        remote_client_mode = remote;
    }

    // Display callback - set by each project to wire its own UI
    void setDisplayCallback(std::function<void(const char* time_str, const char* date_str)> cb) {
        display_callback = cb;
    }

    // Device time sync callbacks (CAN-specific, set by host project)
    // These allow the host project to wire CAN-based device time sync
    void setDeviceTimeRequestCallback(DeviceTimeRequestCallback cb) {
        device_time_request_callback = cb;
    }

    // Called when device time is received (from CAN or other source)
    void onDeviceTimeReceived(uint32_t device_seconds);

    // Check if time is synced
    bool isTimeSynced() { return time_synced; }

private:
    bool time_synced;
    bool remote_client_mode;
    const char* timezone;
    unsigned long last_update;
    unsigned long last_sync_check;

    static const unsigned long UPDATE_INTERVAL = 10000; // 10 seconds
    static const unsigned long SYNC_CHECK_INTERVAL = 28800000; // 8 hours
    static const int TIME_DIFFERENCE_THRESHOLD = 10; // seconds

    // Display callback (set by host project)
    std::function<void(const char* time_str, const char* date_str)> display_callback;

    // Device time sync callbacks (set by host project, CAN-specific)
    DeviceTimeRequestCallback device_time_request_callback;

    bool waiting_for_device_time;
    unsigned long device_time_request_timestamp;

    void syncTime();
    void checkAndSyncDeviceTime();
    String getTimeString();
    String getDateString();

    // Helper to convert time_t to seconds since 2000-01-01
    uint32_t timeToDeviceSeconds(time_t unix_time);
    // Helper to convert device seconds to time_t
    time_t deviceSecondsToTime(uint32_t device_seconds);
};

} // namespace comfoair

#endif
