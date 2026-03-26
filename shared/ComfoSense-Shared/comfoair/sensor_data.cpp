#include "sensor_data.h"

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
    if (now - last_display_update >= 10000) {
        updateDisplay();
        last_display_update = now;
    }
}

void SensorDataManager::updateInsideTemp(float temp) {
    current_data.inside_temp = temp;
    current_data.valid = true;
    current_data.last_update = millis();
    can_data_ever_received = true;  // Disable demo mode forever

    Serial.printf("SensorData: Inside temp updated: %.1f C\n", temp);

    //  Mark for display update (will be shown within 2 seconds)
    last_can_update = millis();
    display_update_pending = true;
}

void SensorDataManager::updateOutsideTemp(float temp) {
    current_data.outside_temp = temp;
    current_data.valid = true;
    current_data.last_update = millis();
    can_data_ever_received = true;  // Disable demo mode forever

    Serial.printf("SensorData: Outside temp updated: %.1f C\n", temp);

    //  Mark for display update (will be shown within 2 seconds)
    last_can_update = millis();
    display_update_pending = true;
}

void SensorDataManager::updateInsideHumidity(float humidity) {
    current_data.inside_humidity = humidity;
    current_data.valid = true;
    current_data.last_update = millis();
    can_data_ever_received = true;  // Disable demo mode forever

    Serial.printf("SensorData: Inside humidity updated: %.0f%%\n", humidity);

    //  Mark for display update (will be shown within 2 seconds)
    last_can_update = millis();
    display_update_pending = true;
}

void SensorDataManager::updateOutsideHumidity(float humidity) {
    current_data.outside_humidity = humidity;
    current_data.valid = true;
    current_data.last_update = millis();
    can_data_ever_received = true;  // Disable demo mode forever

    Serial.printf("SensorData: Outside humidity updated: %.0f%%\n", humidity);

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
    Serial.printf("SensorDataManager: Updating display - Inside: %.1f C/%.0f%%, Outside: %.1f C/%.0f%% %s\n",
                  current_data.inside_temp, current_data.inside_humidity,
                  current_data.outside_temp, current_data.outside_humidity,
                  current_data.valid ? "(CAN)" : "(DUMMY)");

    if (display_callback) {
        display_callback(current_data);
    }
}

} // namespace comfoair
