#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <Arduino.h>

namespace comfoair {

// Sensor data structure
struct SensorData {
    float inside_temp;      // Extract air temp (CÂ°)
    float outside_temp;     // Outdoor air temp (CÂ°)
    float inside_humidity;  // Extract air humidity (%)
    float outside_humidity; // Outdoor air humidity (%)
    bool valid;             // Data validity flag
    unsigned long last_update; // Last update timestamp
};

class SensorDataManager {
public:
    SensorDataManager();
    
    void setup();
    void loop();
    
    // Update sensor values from CAN
    void updateInsideTemp(float temp);
    void updateOutsideTemp(float temp);
    void updateInsideHumidity(float humidity);
    void updateOutsideHumidity(float humidity);
    
    // Get current values
    SensorData getData();
    
    // Check if data is stale (no updates for timeout period)
    bool isDataStale();
    
private:
    SensorData current_data;
    
    // Update GUI display with current data
    void updateDisplay();
    
    // Use dummy data when CAN is unavailable
    void useDummyData();
    
    // Timing
    unsigned long last_display_update;
    static const unsigned long DISPLAY_UPDATE_INTERVAL = 2000; // Update display every 2 seconds
    static const unsigned long DATA_STALE_TIMEOUT = 30000; // Consider data stale after 30 seconds
};

} // namespace comfoair

#endif
