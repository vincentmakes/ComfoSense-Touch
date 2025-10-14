#include "filter_data.h"
#include "../ui/GUI.h"

namespace comfoair {

FilterDataManager::FilterDataManager() 
    : filter_days_remaining(DUMMY_DAYS), has_data(false), last_update(0) {
}

void FilterDataManager::setup() {
    Serial.println("FilterDataManager: Initializing...");
    
    // Start with dummy data (99 days)
    filter_days_remaining = DUMMY_DAYS;
    has_data = false;
    
    // Hide warning icon by default (since dummy = 99 days)
    lv_obj_add_flag(GUI_Image__screen__image, LV_OBJ_FLAG_HIDDEN);
    
    // Update display
    updateDisplay();
    
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
    filter_days_remaining = days;
    has_data = true;
    last_update = millis();
    
    Serial.printf("FilterDataManager: Filter days updated: %d days %s\n", 
                  days,
                  days <= WARNING_THRESHOLD ? "(WARNING THRESHOLD!)" : "");
    
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
    char filter_text[32];
    
    // Format: "Filter Change\nin XX days"
    snprintf(filter_text, sizeof(filter_text), "Filter Change\nin %d days", filter_days_remaining);
    
    Serial.printf("FilterDataManager: Updating display - %d days remaining %s\n",
                  filter_days_remaining,
                  has_data ? "(CAN)" : "(DUMMY)");
    
    // **Use Strategy 5 pattern:**
    // 1. Set the text
    lv_label_set_text(GUI_Label__screen__filter, filter_text);
    
    // 2. Request display refresh (CRITICAL ORDER - before invalidate)
    GUI_request_display_refresh();
    
    // 3. Invalidate the object
    lv_obj_invalidate(GUI_Label__screen__filter);
}

void FilterDataManager::updateWarningIcon() {
    // Show warning icon only when filter needs change within WARNING_THRESHOLD days
    bool should_show_warning = (filter_days_remaining <= WARNING_THRESHOLD);
    
    if (should_show_warning) {
        Serial.printf("FilterDataManager: Showing warning icon (%d days <= %d threshold)\n",
                      filter_days_remaining, WARNING_THRESHOLD);
        
        // Make icon visible
        lv_obj_clear_flag(GUI_Image__screen__image, LV_OBJ_FLAG_HIDDEN);
    } else {
        Serial.printf("FilterDataManager: Hiding warning icon (%d days > %d threshold)\n",
                      filter_days_remaining, WARNING_THRESHOLD);
        
        // Hide icon
        lv_obj_add_flag(GUI_Image__screen__image, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Request display refresh for icon visibility change
    GUI_request_display_refresh();
    lv_obj_invalidate(GUI_Image__screen__image);
}

} // namespace comfoair
