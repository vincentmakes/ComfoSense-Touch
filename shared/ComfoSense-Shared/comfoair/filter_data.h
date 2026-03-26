#ifndef FILTER_DATA_H
#define FILTER_DATA_H

#include <Arduino.h>
#include <functional>

namespace comfoair {

class FilterDataManager {
public:
    FilterDataManager();

    void setup();
    void loop();

    // Update filter days from CAN or MQTT
    void updateFilterDays(int days);

    // Get current filter days
    int getFilterDays();

    // Set warning threshold (days remaining before showing warning)
    void setWarningThreshold(int days) {
        warning_threshold = days;
    }

    // Display callbacks - set by each project to wire its own UI
    void setDisplayCallback(std::function<void(int days_remaining, bool has_data)> cb) {
        display_callback = cb;
    }
    void setWarningCallback(std::function<void(bool show_warning)> cb) {
        warning_callback = cb;
    }

private:
    int filter_days_remaining;
    bool has_data;
    unsigned long last_update;
    int warning_threshold;

    // Display callbacks (set by host project)
    std::function<void(int days_remaining, bool has_data)> display_callback;
    std::function<void(bool show_warning)> warning_callback;

    // Update display via callback
    void updateDisplay();

    // Update warning icon via callback
    void updateWarningIcon();

    // Check if we should use dummy data (no CAN update for >24 hours)
    bool shouldUseDummyData();

    static const unsigned long DATA_TIMEOUT = 86400000; // 24 hours in milliseconds
    static const int DUMMY_DAYS = 99;
};

} // namespace comfoair

#endif
