#include "sensor_data.h"
#include "../ui/GUI.h"

namespace comfoair {

SensorDataManager::SensorDataManager() 
    : last_display_update(0), 
      can_data_ever_received(false),
      display_update_pending(false),
      last_can_update(0) {
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
    
    // Once we've received ANY CAN data, stop the demo mode entirely
    if (can_data_ever_received) {
        //  BATCHED UPDATES: Check if 2 seconds elapsed since last display update
        // This reduces from 100+/sec to 0.5/sec while still feeling responsive
        if (display_update_pending && (now - last_display_update >= 2000)) {
            updateDisplay();
            display_update_pending = false;
            last_display_update = now;
            
            Serial.println("SensorDataManager: Display updated (2-second batch)");
        }
        return;
    }
    
    // Demo mode: periodically update display with dummy data
    if (now - last_display_update >= 10000) { //changed from 10000 to 100
        updateDisplay();
        last_display_update = now;
    }
}

void SensorDataManager::updateInsideTemp(float temp) {
    current_data.inside_temp = temp;
    current_data.valid = true;
    current_data.last_update = millis();
    can_data_ever_received = true;  // Disable demo mode forever
    
    Serial.printf("SensorData: Inside temp updated: %.1f°C (CAN data received)\n", temp);
    
    //  Mark for display update (will be shown within 2 seconds)
    last_can_update = millis();
    display_update_pending = true;
}

void SensorDataManager::updateOutsideTemp(float temp) {
    current_data.outside_temp = temp;
    current_data.valid = true;
    current_data.last_update = millis();
    can_data_ever_received = true;  // Disable demo mode forever
    
    Serial.printf("SensorData: Outside temp updated: %.1f°C (CAN data received)\n", temp);
    
    //  Mark for display update (will be shown within 2 seconds)
    last_can_update = millis();
    display_update_pending = true;
}

void SensorDataManager::updateInsideHumidity(float humidity) {
    current_data.inside_humidity = humidity;
    current_data.valid = true;
    current_data.last_update = millis();
    can_data_ever_received = true;  // Disable demo mode forever
    
    Serial.printf("SensorData: Inside humidity updated: %.0f%% (CAN data received)\n", humidity);
    
    //  Mark for display update (will be shown within 2 seconds)
    last_can_update = millis();
    display_update_pending = true;
}

void SensorDataManager::updateOutsideHumidity(float humidity) {
    current_data.outside_humidity = humidity;
    current_data.valid = true;
    current_data.last_update = millis();
    can_data_ever_received = true;  // Disable demo mode forever
    
    Serial.printf("SensorData: Outside humidity updated: %.0f%% (CAN data received)\n", humidity);
    
    //  Mark for display update (will be shown within 2 seconds)
    last_can_update = millis();
    display_update_pending = true;
}

SensorData SensorDataManager::getData() {
    return current_data;
}

bool SensorDataManager::isDataStale() {
    // Data never goes stale - it "sticks" at last received value
    return false;
}

void SensorDataManager::useDummyData() {
    current_data.inside_temp = 23.0f;
    current_data.outside_temp = 20.5f;
    current_data.inside_humidity = 45.0f;
    current_data.outside_humidity = 50.0f;
    current_data.valid = false; // Mark as dummy data
    current_data.last_update = millis();
    
    Serial.println("SensorDataManager: Using dummy data");
}

void SensorDataManager::updateDisplay() {
    // Format temperature and humidity strings
    char inside_temp_str[16];
    char outside_temp_str[16];
    char inside_hum_str[16];
    char outside_hum_str[16];
    
    snprintf(inside_temp_str, sizeof(inside_temp_str), "%.1f°C", current_data.inside_temp);
    snprintf(outside_temp_str, sizeof(outside_temp_str), "%.1f°C", current_data.outside_temp);
    snprintf(inside_hum_str, sizeof(inside_hum_str), "%.0f%%", current_data.inside_humidity);
    snprintf(outside_hum_str, sizeof(outside_hum_str), "%.0f%%", current_data.outside_humidity);
    
    Serial.printf("SensorDataManager: Updating display - Inside: %s/%s, Outside: %s/%s %s\n",
                  inside_temp_str, inside_hum_str,
                  outside_temp_str, outside_hum_str,
                  current_data.valid ? "(CAN)" : "(DUMMY)");
    
    //  **STRATEGY 5 PATTERN** with 2-second batching
    // 1. Set the text
    lv_label_set_text(GUI_Label__screen__insideTemp, inside_temp_str);
    lv_label_set_text(GUI_Label__screen__outsideTemp, outside_temp_str);
    lv_label_set_text(GUI_Label__screen__insideHum, inside_hum_str);
    lv_label_set_text(GUI_Label__screen__outsideHum, outside_hum_str);
    
    // 2. Request display refresh (calls lv_refr_now())
    //  Only called every 2 seconds max (batched), not on every CAN frame
    // Reduces from 100+/sec to 0.5/sec while still feeling responsive
    GUI_request_display_refresh();
    
    // 3. Invalidate the objects
    lv_obj_invalidate(GUI_Label__screen__insideTemp);
    lv_obj_invalidate(GUI_Label__screen__outsideTemp);
    lv_obj_invalidate(GUI_Label__screen__insideHum);
    lv_obj_invalidate(GUI_Label__screen__outsideHum);
    
    Serial.println("SensorDataManager: Display refresh requested (Strategy 5)");
}

} // namespace comfoair