#include "time_manager.h"
#include <time.h>
#include <sys/time.h>
#include "lwip/apps/sntp.h"
#include "../ui/GUI.h"

namespace comfoair {

TimeManager::TimeManager() {
  time_synced = false;
  last_update = 0;
}

void TimeManager::setup() {
  Serial.println("Setting up NTP time sync...");
  
  // Set initial placeholder values
  if (GUI_Label__screen__time) {
    lv_label_set_text(GUI_Label__screen__time, "12:00");
  }
  if (GUI_Label__screen__date) {
    lv_label_set_text(GUI_Label__screen__date, "1 Jan. 2025");
  }
  
  // Configure NTP with longer timeout
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, "pool.ntp.org");
  sntp_setservername(1, "time.nist.gov");
  sntp_setservername(2, "time.google.com");
  sntp_init();
  
  // Set timezone to Paris (CET/CEST)
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();
  
  Serial.println("NTP time sync configured for Paris timezone");
  Serial.println("Waiting for time sync...");
  
  // Initial sync attempt with retries
  for (int i = 0; i < 10 && !time_synced; i++) {
    syncTime();
    if (!time_synced) {
      Serial.printf("Retry %d/10...\n", i + 1);
      delay(1000);
    }
  }
}

void TimeManager::loop() {
  unsigned long now = millis();
  
  // Update display every minute
  if (now - last_update >= UPDATE_INTERVAL || last_update == 0) {
    last_update = now;
    
    // Try to sync if not already synced
    if (!time_synced) {
      syncTime();
    }
    
    // Update display
    updateDisplay();
  }
}

void TimeManager::syncTime() {
  struct tm timeinfo;
  
  // Wait for valid time (year > 2020)
  time_t now = time(nullptr);
  if (now > 1577836800) { // Jan 1, 2020
    if (getLocalTime(&timeinfo, 1000)) { // 1000ms timeout
      time_synced = true;
      Serial.println("✓ Time synchronized with NTP server");
      Serial.printf("Current time: %02d:%02d:%02d on %d/%d/%d\n", 
                    timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec,
                    timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      return;
    }
  }
  
  Serial.println("✗ Waiting for NTP sync...");
}

String TimeManager::getTimeString() {
  if (!time_synced) {
    return "12:00";
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    return "12:00";
  }
  
  char timeStr[6];
  sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  return String(timeStr);
}

String TimeManager::getDateString() {
  if (!time_synced) {
    return "1 Jan. 2025";
  }
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 100)) {
    return "1 Jan. 2025";
  }
  
  // Month names in English
  const char* months[] = {
    "Jan.", "Feb.", "March", "April", "May", "June",
    "July", "Aug.", "Sept.", "Oct.", "Nov.", "Dec."
  };
  
  char dateStr[20];
  sprintf(dateStr, "%d %s %d", 
          timeinfo.tm_mday, 
          months[timeinfo.tm_mon], 
          timeinfo.tm_year + 1900);
  
  return String(dateStr);
}

bool TimeManager::isTimeSynced() {
  return time_synced;
}

void TimeManager::updateDisplay() {
  // Get formatted strings (will return defaults if not synced)
  String timeStr = getTimeString();
  String dateStr = getDateString();
  
  // Update LVGL labels - use simple update without manual invalidation
  if (GUI_Label__screen__time) {
    lv_label_set_text(GUI_Label__screen__time, timeStr.c_str());
  }
  
  if (GUI_Label__screen__date) {
    lv_label_set_text(GUI_Label__screen__date, dateStr.c_str());
  }
  
  // Don't call GUI_request_display_refresh() - let lv_timer_handler() do it naturally
  
  if (time_synced) {
    Serial.printf("Display updated: %s - %s\n", timeStr.c_str(), dateStr.c_str());
  }
}

} // namespace comfoair