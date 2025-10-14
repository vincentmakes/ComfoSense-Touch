#include "sensor_data.h"
#include "../ui/GUI.h"

namespace comfoair {

SensorDataManager::SensorDataManager() 
    : last_display_update(0) {
    // Initialize with dummy data
    current_data.inside_temp = 23.0f;
    current_data.outside_temp = 20.5f;
    current_data.inside_humidity = 45.0f;
    current_data.outside_humidity = 50.0f;
    current_data.valid = false;
    current_data.last_update = 0;
}

void SensorDataManager::setup() {
    Serial.println("SensorDataManager: Initializing...");
    
    // Start with dummy data displayed
    useDummyData();
    updateDisplay();
    
    Serial.println("SensorDataManager: Ready (using dummy data until CAN data available)");
}

void SensorDataManager::loop() {
    unsigned long now = millis();
    
    // Check if data is stale - revert to dummy data
    if (current_data.valid && isDataStale()) {
        Serial.println("SensorDataManager: CAN data stale, reverting to dummy data");
        useDummyData();
    }
    
    // Update display periodically
    if (now - last_display_update >= DISPLAY_UPDATE_INTERVAL) {
        updateDisplay();
        last_display_update = now;
    }
}

void SensorDataManager::updateInsideTemp(float temp) {
    current_data.inside_temp = temp;
    current_data.valid = true;
    current_data.last_update = millis();
}

void SensorDataManager::updateOutsideTemp(float temp) {
    current_data.outside_temp = temp;
    current_data.valid = true;
    current_data.last_update = millis();
}

void SensorDataManager::updateInsideHumidity(float humidity) {
    current_data.inside_humidity = humidity;
    current_data.valid = true;
    current_data.last_update = millis();
}

void SensorDataManager::updateOutsideHumidity(float humidity) {
    current_data.outside_humidity = humidity;
    current_data.valid = true;
    current_data.last_update = millis();
}

SensorData SensorDataManager::getData() {
    return current_data;
}

bool SensorDataManager::isDataStale() {
    if (!current_data.valid) return true;
    return (millis() - current_data.last_update) > DATA_STALE_TIMEOUT;
}

void SensorDataManager::useDummyData() {
    current_data.inside_temp = 23.0f;
    current_data.outside_temp = 20.5f;
    current_data.inside_humidity = 45.0f;
    current_data.outside_humidity = 50.0f;
    current_data.valid = false; // Mark as dummy data
    current_data.last_update = millis();
}

void SensorDataManager::updateDisplay() {
    // Format temperature and humidity strings
    // Temperature: 1 decimal place, "C" suffix (no degree symbol)
    // Humidity: 0 decimal places (integer), "%" suffix
    char inside_temp_str[16];
    char outside_temp_str[16];
    char inside_hum_str[16];
    char outside_hum_str[16];
    
    snprintf(inside_temp_str, sizeof(inside_temp_str), "%.1fC", current_data.inside_temp);
    snprintf(outside_temp_str, sizeof(outside_temp_str), "%.1fC", current_data.outside_temp);
    snprintf(inside_hum_str, sizeof(inside_hum_str), "%.0f%%", current_data.inside_humidity);
    snprintf(outside_hum_str, sizeof(outside_hum_str), "%.0f%%", current_data.outside_humidity);
    
    Serial.printf("SensorDataManager: Updating display - Inside: %s/%s, Outside: %s/%s %s\n",
                  inside_temp_str, inside_hum_str,
                  outside_temp_str, outside_hum_str,
                  current_data.valid ? "(CAN)" : "(DUMMY)");
    
    // **Use Strategy 5 pattern (from PROJECT_MEMORY_UPDATED.md):**
    // 1. Set the text
    lv_label_set_text(GUI_Label__screen__insideTemp, inside_temp_str);
    lv_label_set_text(GUI_Label__screen__outsideTemp, outside_temp_str);
    lv_label_set_text(GUI_Label__screen__insideHum, inside_hum_str);
    lv_label_set_text(GUI_Label__screen__outsideHum, outside_hum_str);
    
    // 2. Request display refresh (CRITICAL ORDER - before invalidate)
    GUI_request_display_refresh();
    
    // 3. Invalidate the objects
    lv_obj_invalidate(GUI_Label__screen__insideTemp);
    lv_obj_invalidate(GUI_Label__screen__outsideTemp);
    lv_obj_invalidate(GUI_Label__screen__insideHum);
    lv_obj_invalidate(GUI_Label__screen__outsideHum);
}

} // namespace comfoair