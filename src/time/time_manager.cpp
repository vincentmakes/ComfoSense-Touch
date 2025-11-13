#include "time_manager.h"
#include "../comfoair/comfoair.h"
#include "../ui/GUI.h"
#include "../secrets.h"
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include "../board_config.h"  // For hasDisplay()

namespace comfoair {

TimeManager::TimeManager()
    : time_synced(false),
      waiting_for_device_time(false),
      last_update(0),
      last_sync_check(0),
      device_time_request_timestamp(0),
      comfoair(nullptr) {
}

void TimeManager::setComfoAir(ComfoAir* comfo_ptr) {
    comfoair = comfo_ptr;
    Serial.println("TimeManager: ComfoAir instance linked");
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
        
        // Set the timezone using the user-configured value from secrets.h
        // This converts UTC time to local time with proper DST handling
        #ifdef TIMEZONE
            setenv("TZ", TIMEZONE, 1);
            Serial.printf("TimeManager: Timezone set to: %s\n", TIMEZONE);
        #else
            // Fallback to CET if not defined
            setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
            Serial.println("TimeManager: Timezone set to: CET-1CEST,M3.5.0,M10.5.0/3 (default)");
        #endif
        tzset();
        
        // Get the local time after timezone is set
        time(&now);
        localtime_r(&now, &timeinfo);
        
        Serial.printf("TimeManager: NTP sync successful - %04d-%02d-%02d %02d:%02d:%02d %s\n",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                     timeinfo.tm_isdst ? "CEST" : "CET");
        time_synced = true;
        last_sync_check = millis();
        
        // ====================================================================
        // DEVICE TIME SYNC (only in non-remote client mode)
        // ====================================================================
        #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
            Serial.println("TimeManager: Device time sync SKIPPED (Remote Client Mode)");
        #else
            // After NTP sync, check device time
            checkAndSyncDeviceTime();
        #endif
        // ====================================================================
    } else {
        Serial.println(" FAILED");
        Serial.println("TimeManager: NTP sync failed");
        time_synced = false;
    }
}

void TimeManager::checkAndSyncDeviceTime() {
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        // In remote client mode, skip device time sync
        Serial.println("TimeManager: checkAndSyncDeviceTime() - SKIPPED (Remote Client Mode)");
        return;
    #else
        if (!comfoair) {
            Serial.println("TimeManager: ComfoAir not set, cannot check device time");
            return;
        }
        
        // ====================================================================
        // RELIABILITY FIX: Wait for CAN bus and MVHR to be ready
        // ====================================================================
        // After NTP sync or boot, give the MVHR a moment to be ready
        // to respond to time requests
        // ====================================================================
        Serial.println("TimeManager: Waiting 2 seconds for MVHR to be ready...");
        delay(2000);
        
        Serial.println("TimeManager: Requesting device time from CAN bus...");
        
        // Request time from device via CAN bus
        comfoair->requestDeviceTime();
        waiting_for_device_time = true;
        device_time_request_timestamp = millis();
    #endif
}

void TimeManager::onDeviceTimeReceived(uint32_t device_seconds) {
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        // In remote client mode, ignore device time responses
        Serial.println("TimeManager: onDeviceTimeReceived() - IGNORED (Remote Client Mode)");
        return;
    #else
        // ====================================================================
        // FIX: Don't check waiting_for_device_time flag
        // ====================================================================
        // The flag can be false if first request timed out, but we still
        // want to process the response from subsequent requests.
        // It's safe to process any device time response we receive.
        // ====================================================================
        
        // Clear the flag (in case we were waiting)
        waiting_for_device_time = false;
        
        Serial.println("TimeManager: Processing device time response...");
        
        // ====================================================================
        
        // Convert device seconds to time_t
        time_t device_time = deviceSecondsToTime(device_seconds);
        
        // Get current local time
        time_t now_timestamp;
        time(&now_timestamp);
        
        // Get both local and UTC time structures
        struct tm device_tm, local_tm, utc_tm;
        localtime_r(&device_time, &device_tm);
        localtime_r(&now_timestamp, &local_tm);

        
        
        
        // Calculate difference (compare device time with local time for threshold check)
        int time_diff = abs((int)difftime(now_timestamp, device_time));
        
        // Log all times for debugging
        char device_str[64], local_str[64], cet_str[64];

        
        strftime(device_str, sizeof(device_str), "%Y-%m-%d %H:%M:%S", &device_tm);
        strftime(local_str, sizeof(local_str), "%Y-%m-%d %H:%M:%S %Z", &local_tm);
        
        Serial.printf("TimeManager: Device time: %s\n", device_str);
        Serial.printf("TimeManager: Local time:  %s(will send this to MVHR)\n", local_str);

        Serial.printf("TimeManager: Difference:  %d seconds\n", time_diff);
        
        // Check if difference exceeds threshold
        if (time_diff > TIME_DIFFERENCE_THRESHOLD) {
            Serial.printf("TimeManager: Time difference (%d sec) exceeds threshold (%d sec)\n", 
                         time_diff, TIME_DIFFERENCE_THRESHOLD);
            Serial.println("TimeManager: Setting device time to CET (MVHR expects CET)...");
            
            // Send local time
            setDeviceTime(now_timestamp);
        } else {
            Serial.println("TimeManager: Device time is synchronized (within threshold)");
        }
        
        last_sync_check = millis();
    #endif
}

void TimeManager::setDeviceTime(time_t ntp_time) {
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        // In remote client mode, skip device time setting
        Serial.println("TimeManager: setDeviceTime() - SKIPPED (Remote Client Mode)");
        return;
    #else
        if (!comfoair) {
            Serial.println("TimeManager: ComfoAir not set, cannot set device time");
            return;
        }
        
        // Convert NTP time to device seconds
        uint32_t device_seconds = timeToDeviceSeconds(ntp_time);
        
        struct tm timeinfo;
        localtime_r(&ntp_time, &timeinfo);
        
        Serial.printf("TimeManager: Setting device time to: %04d-%02d-%02d %02d:%02d:%02d\n",
                     timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        
        // Send time to device via CAN bus
        comfoair->setDeviceTime(device_seconds);
        
        Serial.println("TimeManager: Device time set command sent");
    #endif
}

uint32_t TimeManager::timeToDeviceSeconds(time_t unix_time) {
    // Device epoch: 2000-01-01 00:00:00
    // Unix epoch:   1970-01-01 00:00:00
    // Difference: 946684800 seconds (30 years)
    const time_t DEVICE_EPOCH_OFFSET = 946684800;
    
    return (uint32_t)(unix_time - DEVICE_EPOCH_OFFSET);
}

time_t TimeManager::deviceSecondsToTime(uint32_t device_seconds) {
    // Device epoch: 2000-01-01 00:00:00
    const time_t DEVICE_EPOCH_OFFSET = 946684800;
    
    return (time_t)(device_seconds + DEVICE_EPOCH_OFFSET);
}

String TimeManager::getTimeString() {
    time_t now;
    struct tm timeinfo;
    
    // Get current time from system clock (no NTP call)
    time(&now);
    localtime_r(&now, &timeinfo);
    
    char time_str[16];
    strftime(time_str, sizeof(time_str), "%H:%M", &timeinfo);
    
    return String(time_str);
}

String TimeManager::getDateString() {
    time_t now;
    struct tm timeinfo;
    
    // Get current time from system clock (no NTP call)
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
    
    // Get time from local system clock (VERY fast, no NTP calls)
    String timeStr = getTimeString();
    String dateStr = getDateString();
    
    // ✅ **STRATEGY 5 PATTERN** (but optimized to reduce refresh frequency)
    // 1. Set the text
    lv_label_set_text(GUI_Label__screen__time, timeStr.c_str());
    lv_label_set_text(GUI_Label__screen__date, dateStr.c_str());
    
    // 2. Request display refresh (this calls lv_refr_now())
    // ✅ OPTIMIZATION: Only refresh every second (not a problem for time display)
    // This reduces from potentially 60Hz to 1Hz for time updates
    GUI_request_display_refresh();
    
    // 3. Invalidate the objects
    lv_obj_invalidate(GUI_Label__screen__time);
    lv_obj_invalidate(GUI_Label__screen__date);
}

void TimeManager::loop() {
    if (!time_synced) {
        return;
    }
    
    // Update display every 1 second (just reads local clock, very fast)
    unsigned long now = millis();
    if (now - last_update >= UPDATE_INTERVAL) {
            if (hasDisplay()) {
        updateDisplay();
    }
 
        last_update = now;
    }
    
    // ========================================================================
    // PERIODIC RE-SYNC
    // ========================================================================
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        // In remote client mode: Only re-sync NTP every 8 hours (no device sync)
        if (now - last_sync_check >= SYNC_CHECK_INTERVAL) {
            Serial.println("TimeManager: 8 hours elapsed, re-syncing with NTP...");
            syncTime(); // Re-sync with NTP only
        }
    #else
        // In normal mode: Re-sync NTP + device time every 8 hours
        if (now - last_sync_check >= SYNC_CHECK_INTERVAL) {
            Serial.println("TimeManager: 8 hours elapsed, re-syncing with NTP and device...");
            syncTime(); // Re-sync with NTP
            if (time_synced) {
                checkAndSyncDeviceTime(); // Then check device time
            }
        }
        
        // Handle timeout for device time request (5 seconds)
        if (waiting_for_device_time && (now - device_time_request_timestamp > 5000)) {
            Serial.println("TimeManager: Device time request timeout");
            waiting_for_device_time = false;
            last_sync_check = now; // Reset to retry in another 8 hours
        }
    #endif
    // ========================================================================
}

} // namespace comfoair