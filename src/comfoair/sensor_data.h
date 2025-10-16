#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <Arduino.h>

namespace comfoair {

// Sensor data structure
struct SensorData {
    float inside_temp;      // Extract air temp (°C)
    float outside_temp;     // Outdoor air temp (°C)
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
    
    // Track if we've ever received CAN data (disables demo mode)
    bool can_data_ever_received;
    
    // Batching for display updates
    bool display_update_pending;
    unsigned long last_can_update;
    
    // Update GUI display with current data
    void updateDisplay();
    
    // Use dummy data when CAN is unavailable
    void useDummyData();
    
    // Timing
    unsigned long last_display_update;
    
    // OPTIMIZED: Update display every 10 seconds for sensor data (low priority)
    static const unsigned long DISPLAY_UPDATE_INTERVAL = 10000; // 10 seconds
};

} // namespace comfoair

#endif