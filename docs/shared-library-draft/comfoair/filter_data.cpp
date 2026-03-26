#include "filter_data.h"

namespace comfoair {

FilterDataManager::FilterDataManager()
    : filter_days_remaining(DUMMY_DAYS), has_data(false), last_update(0), warning_threshold(100) {
}

void FilterDataManager::setup() {
    Serial.println("FilterDataManager: Initializing...");

    // Start with dummy data (99 days)
    filter_days_remaining = DUMMY_DAYS;
    has_data = false;

    // Update display
    updateDisplay();

    // Update warning icon based on threshold
    updateWarningIcon();

    Serial.println("FilterDataManager: Ready (using 99 days until CAN data available)");
}

void FilterDataManager::loop() {
    // Check if data is stale (no update for 24+ hours)
    if (has_data && shouldUseDummyData()) {
        Serial.println("FilterDataManager: CAN data stale (>24h), reverting to 99 days");
        filter_days_remaining = DUMMY_DAYS;
        has_data = false;
        updateDisplay();
        updateWarningIcon();
    }
}

void FilterDataManager::updateFilterDays(int days) {
    // DEDUPLICATION: Ignore if same value received within 5 seconds
    // This prevents issues when MVHR sends multiple responses to RTR
    static int last_days_value = -1;
    static unsigned long last_update_time = 0;
    unsigned long now = millis();

    if (days == last_days_value && (now - last_update_time) < 5000) {
        Serial.printf("FilterDataManager: Ignoring duplicate value %d (< 5s since last update)\n", days);
        return;
    }

    last_days_value = days;
    last_update_time = now;

    filter_days_remaining = days;
    has_data = true;
    last_update = millis();

    Serial.printf("FilterDataManager: Filter days updated: %d days %s\n",
                  days,
                  days <= warning_threshold ? "(WARNING THRESHOLD!)" : "");

    updateDisplay();
    updateWarningIcon();
}

int FilterDataManager::getFilterDays() {
    return filter_days_remaining;
}

bool FilterDataManager::shouldUseDummyData() {
    if (!has_data) return true;

    // Account for millis() rollover (happens every ~49 days)
    unsigned long now = millis();
    unsigned long elapsed;

    if (now >= last_update) {
        elapsed = now - last_update;
    } else {
        // Rollover occurred
        elapsed = (0xFFFFFFFF - last_update) + now + 1;
    }

    return elapsed > DATA_TIMEOUT;
}

void FilterDataManager::updateDisplay() {
    Serial.printf("FilterDataManager: Updating display - %d days remaining %s\n",
                  filter_days_remaining,
                  has_data ? "(CAN)" : "(DUMMY)");

    if (display_callback) {
        display_callback(filter_days_remaining, has_data);
    }
}

void FilterDataManager::updateWarningIcon() {
    bool should_show_warning = (filter_days_remaining <= warning_threshold);

    if (should_show_warning) {
        Serial.printf("FilterDataManager: Showing warning icon (%d days <= %d threshold)\n",
                      filter_days_remaining, warning_threshold);
    } else {
        Serial.printf("FilterDataManager: Hiding warning icon (%d days > %d threshold)\n",
                      filter_days_remaining, warning_threshold);
    }

    if (warning_callback) {
        warning_callback(should_show_warning);
    }
}

} // namespace comfoair
