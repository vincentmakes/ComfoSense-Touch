#include "time_manager.h"
#include "../ui/GUI.h"
#include "../secrets.h"
#include <WiFi.h>

namespace comfoair {

TimeManager::TimeManager() : time_synced(false), last_update(0) {
}

void TimeManager::setup() {
    Serial.println("TimeManager: Initializing...");
    
    // Wait for WiFi connection
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
        delay(500);
        Serial.print(".");
        retries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        syncTime();
    } else {
        Serial.println("\nTimeManager: WiFi not connected, skipping time sync");
    }
}

void TimeManager::syncTime() {
    Serial.println("TimeManager: Syncing time with NTP...");
    
    // Configure NTP
    configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
    
    // Wait for time to be set (max 10 seconds)
    int retries = 0;
    time_t now = 0;
    struct tm timeinfo;
    
    while (retries < 20) {
        time(&now);
        localtime_r(&now, &timeinfo);
        
        if (timeinfo.tm_year > (2020 - 1900)) {
            time_synced = true;
            Serial.println("✓ Time synchronized");
            
            // Log the current time
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%H:%M on %d/%m/%Y", &timeinfo);
            Serial.printf("Time: %s\n", time_str);
            
            // Do initial display update
            updateDisplay();
            break;
        }
        
        delay(500);
        retries++;
    }
    
    if (!time_synced) {
        Serial.println("✗ Time sync failed");
    }
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
    
    Serial.printf("TimeManager: Updating display - %s - %s\n", 
                  timeStr.c_str(), dateStr.c_str());
    
    // **Use Strategy 5 pattern that works:**
    // 1. Set the text
    lv_label_set_text(GUI_Label__screen__time, timeStr.c_str());
    lv_label_set_text(GUI_Label__screen__date, dateStr.c_str());
    
    // 2. Request display refresh (this calls lv_refr_now())
    GUI_request_display_refresh();
    
    // 3. Invalidate the objects
    lv_obj_invalidate(GUI_Label__screen__time);
    lv_obj_invalidate(GUI_Label__screen__date);
    
    Serial.println("TimeManager: Display update complete");
}

void TimeManager::loop() {
    // Only update if time is synced
    if (!time_synced) {
        return;
    }
    
    // Update display every 15 second
    unsigned long now = millis();
    if (now - last_update >= UPDATE_INTERVAL) {
        updateDisplay();
        last_update = now;
    }
}

} // namespace comfoair