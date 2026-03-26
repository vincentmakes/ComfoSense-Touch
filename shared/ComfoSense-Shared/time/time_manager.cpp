#include "time_manager.h"
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>

namespace comfoair {

TimeManager::TimeManager()
    : time_synced(false),
      remote_client_mode(false),
      timezone("CET-1CEST,M3.5.0,M10.5.0/3"),
      waiting_for_device_time(false),
      last_update(0),
      last_sync_check(0),
      device_time_request_timestamp(0) {
}

void TimeManager::setup() {
    Serial.println("TimeManager: Starting NTP sync...");
    syncTime();
}

void TimeManager::syncTime() {
    // First, sync with NTP to get UTC time
    Serial.println("TimeManager: Starting NTP sync...");
    configTime(0, 0, "time.google.com", "pool.ntp.org");

    Serial.print("TimeManager: Waiting for NTP sync");

    // Wait up to 10 seconds for time sync
    int retry = 0;
    time_t now = 0;
    struct tm timeinfo;

    while (retry < 20) {  // 20 * 500ms = 10 seconds
        time(&now);
        localtime_r(&now, &timeinfo);

        if (timeinfo.tm_year > (2020 - 1900)) {  // If year > 2020, we have valid time
            break;
        }

        Serial.print(".");
        delay(500);
        retry++;
    }

    if (timeinfo.tm_year > (2020 - 1900)) {
        Serial.println(" SUCCESS");

        // Set the timezone
        setenv("TZ", timezone, 1);
        Serial.printf("TimeManager: Timezone set to: %s\n", timezone);
        tzset();

        // Get the local time after timezone is set
        time(&now);
        localtime_r(&now, &timeinfo);

        Serial.printf("TimeManager: NTP sync successful - %04d-%02d-%02d %02d:%02d:%02d %s\n",
                      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                      timeinfo.tm_isdst ? "DST" : "STD");
        time_synced = true;
        last_sync_check = millis();

        // Device time sync (only if not remote client and callback is set)
        if (!remote_client_mode) {
            checkAndSyncDeviceTime();
        } else {
            Serial.println("TimeManager: Device time sync SKIPPED (Remote Client Mode)");
        }
    } else {
        Serial.println(" FAILED");
        Serial.println("TimeManager: NTP sync failed");
        time_synced = false;
    }
}

void TimeManager::checkAndSyncDeviceTime() {
    if (remote_client_mode) {
        Serial.println("TimeManager: checkAndSyncDeviceTime() - SKIPPED (Remote Client Mode)");
        return;
    }

    if (!device_time_request_callback) {
        Serial.println("TimeManager: No device time request callback set, skipping device time sync");
        return;
    }

    Serial.println("TimeManager: Waiting 2 seconds for MVHR to be ready...");
    delay(2000);

    Serial.println("TimeManager: Requesting device time...");
    device_time_request_callback();
    waiting_for_device_time = true;
    device_time_request_timestamp = millis();
}

void TimeManager::onDeviceTimeReceived(uint32_t device_seconds) {
    if (remote_client_mode) {
        Serial.println("TimeManager: onDeviceTimeReceived() - IGNORED (Remote Client Mode)");
        return;
    }

    waiting_for_device_time = false;

    Serial.println("TimeManager: Processing device time response...");

    // Convert device seconds to time_t
    time_t device_time = deviceSecondsToTime(device_seconds);

    // Get current local time
    time_t now_timestamp;
    time(&now_timestamp);

    // Get time structures
    struct tm device_tm, local_tm;
    localtime_r(&device_time, &device_tm);
    localtime_r(&now_timestamp, &local_tm);

    // Calculate difference
    int time_diff = abs((int)difftime(now_timestamp, device_time));

    // Log all times for debugging
    char device_str[64], local_str[64];
    strftime(device_str, sizeof(device_str), "%Y-%m-%d %H:%M:%S", &device_tm);
    strftime(local_str, sizeof(local_str), "%Y-%m-%d %H:%M:%S %Z", &local_tm);

    Serial.printf("TimeManager: Device time: %s\n", device_str);
    Serial.printf("TimeManager: Local time:  %s\n", local_str);
    Serial.printf("TimeManager: Difference:  %d seconds\n", time_diff);

    // Check if difference exceeds threshold
    if (time_diff > TIME_DIFFERENCE_THRESHOLD) {
        Serial.printf("TimeManager: Time difference (%d sec) exceeds threshold (%d sec)\n",
                      time_diff, TIME_DIFFERENCE_THRESHOLD);
        Serial.println("TimeManager: Device time sync needed - host project should handle this");
        // Note: The actual device time setting is CAN-specific and handled by the host project
        // via the ComfoAir class. This shared library only detects the drift.
    } else {
        Serial.println("TimeManager: Device time is synchronized (within threshold)");
    }

    last_sync_check = millis();
}

uint32_t TimeManager::timeToDeviceSeconds(time_t unix_time) {
    // Device epoch: 2000-01-01 00:00:00
    // Unix epoch:   1970-01-01 00:00:00
    // Difference: 946684800 seconds (30 years)
    const time_t DEVICE_EPOCH_OFFSET = 946684800;
    return (uint32_t)(unix_time - DEVICE_EPOCH_OFFSET);
}

time_t TimeManager::deviceSecondsToTime(uint32_t device_seconds) {
    const time_t DEVICE_EPOCH_OFFSET = 946684800;
    return (time_t)(device_seconds + DEVICE_EPOCH_OFFSET);
}

String TimeManager::getTimeString() {
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    char time_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);

    return String(time_str);
}

String TimeManager::getDateString() {
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    char date_str[32];
    strftime(date_str, sizeof(date_str), "%d %b. %Y", &timeinfo);

    return String(date_str);
}

void TimeManager::updateDisplay() {
    if (!time_synced) {
        return;
    }

    String timeStr = getTimeString();
    String dateStr = getDateString();

    if (display_callback) {
        display_callback(timeStr.c_str(), dateStr.c_str());
    }
}

void TimeManager::loop() {
    if (!time_synced) {
        return;
    }

    unsigned long now = millis();

    // Update display periodically
    if (now - last_update >= UPDATE_INTERVAL) {
        updateDisplay();
        last_update = now;
    }

    // Periodic re-sync
    if (remote_client_mode) {
        // Remote client: Only re-sync NTP every 8 hours
        if (now - last_sync_check >= SYNC_CHECK_INTERVAL) {
            Serial.println("TimeManager: 8 hours elapsed, re-syncing with NTP...");
            syncTime();
        }
    } else {
        // Normal mode: Re-sync NTP + device time every 8 hours
        if (now - last_sync_check >= SYNC_CHECK_INTERVAL) {
            Serial.println("TimeManager: 8 hours elapsed, re-syncing with NTP and device...");
            syncTime();
        }

        // Handle timeout for device time request (5 seconds)
        if (waiting_for_device_time && (now - device_time_request_timestamp > 5000)) {
            Serial.println("TimeManager: Device time request timeout");
            waiting_for_device_time = false;
            last_sync_check = now;
        }
    }
}

} // namespace comfoair
