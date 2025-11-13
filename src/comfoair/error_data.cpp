#include "error_data.h"
#include "../ui/GUI.h"
#include "../board_config.h"  // For hasDisplay()

namespace comfoair {

ErrorDataManager::ErrorDataManager() 
    : error_overheating(false),
      error_temp_sensor_p_oda(false),
      error_preheat_location(false),
      error_ext_pressure_eha(false),
      error_ext_pressure_sup(false),
      error_tempcontrol_p_oda(false),
      error_tempcontrol_sup(false),
      alarm_filter(false),
      warning_system(false),
      has_error_data(false),
      last_update(0) {
}

void ErrorDataManager::setup() {
    Serial.println("ErrorDataManager: Initializing...");
    
    // Start with no errors
    error_overheating = false;
    error_temp_sensor_p_oda = false;
    error_preheat_location = false;
    error_ext_pressure_eha = false;
    error_ext_pressure_sup = false;
    error_tempcontrol_p_oda = false;
    error_tempcontrol_sup = false;
    alarm_filter = false;
    warning_system = false;
    has_error_data = false;
    
    // Warning icon visibility will be managed by filter_data.cpp initially
    // We only show it if errors occur
    
    Serial.println("ErrorDataManager: Ready (no errors)");
}

void ErrorDataManager::loop() {
    // Check if error data is stale (no update for 1+ hour)
    if (has_error_data && shouldClearErrors()) {
        Serial.println("ErrorDataManager: Error data stale (>1h), clearing all errors");
        
        error_overheating = false;
        error_temp_sensor_p_oda = false;
        error_preheat_location = false;
        error_ext_pressure_eha = false;
        error_ext_pressure_sup = false;
        error_tempcontrol_p_oda = false;
        error_tempcontrol_sup = false;
        alarm_filter = false;
        warning_system = false;
        has_error_data = false;
        
        updateWarningIcon();
    }
}

void ErrorDataManager::updateErrorOverheating(bool active) {
    if (error_overheating != active) {
        error_overheating = active;
        has_error_data = true;
        last_update = millis();
        
        if (active) {
            Serial.println("🚨 ErrorDataManager: CRITICAL ERROR - OVERHEATING ACTIVE!");
        } else {
            Serial.println("✅ ErrorDataManager: Overheating error cleared");
        }
        
        updateWarningIcon();
    }
}

void ErrorDataManager::updateErrorTempSensorPODA(bool active) {
    if (error_temp_sensor_p_oda != active) {
        error_temp_sensor_p_oda = active;
        has_error_data = true;
        last_update = millis();
        
        if (active) {
            Serial.println("⚠️  ErrorDataManager: Temperature sensor P-ODA error ACTIVE");
        } else {
            Serial.println("✅ ErrorDataManager: Temperature sensor P-ODA error cleared");
        }
        
        updateWarningIcon();
    }
}

void ErrorDataManager::updateErrorPreheatLocation(bool active) {
    if (error_preheat_location != active) {
        error_preheat_location = active;
        has_error_data = true;
        last_update = millis();
        
        if (active) {
            Serial.println("⚠️  ErrorDataManager: Pre-heater location error ACTIVE");
        } else {
            Serial.println("✅ ErrorDataManager: Pre-heater location error cleared");
        }
        
        updateWarningIcon();
    }
}

void ErrorDataManager::updateErrorExtPressureEHA(bool active) {
    if (error_ext_pressure_eha != active) {
        error_ext_pressure_eha = active;
        has_error_data = true;
        last_update = millis();
        
        if (active) {
            Serial.println("⚠️  ErrorDataManager: Exhaust air pressure too high ACTIVE");
        } else {
            Serial.println("✅ ErrorDataManager: Exhaust air pressure error cleared");
        }
        
        updateWarningIcon();
    }
}

void ErrorDataManager::updateErrorExtPressureSUP(bool active) {
    if (error_ext_pressure_sup != active) {
        error_ext_pressure_sup = active;
        has_error_data = true;
        last_update = millis();
        
        if (active) {
            Serial.println("⚠️  ErrorDataManager: Supply air pressure too high ACTIVE");
        } else {
            Serial.println("✅ ErrorDataManager: Supply air pressure error cleared");
        }
        
        updateWarningIcon();
    }
}

void ErrorDataManager::updateErrorTempControlPODA(bool active) {
    if (error_tempcontrol_p_oda != active) {
        error_tempcontrol_p_oda = active;
        has_error_data = true;
        last_update = millis();
        
        if (active) {
            Serial.println("⚠️  ErrorDataManager: Temperature control P-ODA error ACTIVE");
        } else {
            Serial.println("✅ ErrorDataManager: Temperature control P-ODA error cleared");
        }
        
        updateWarningIcon();
    }
}

void ErrorDataManager::updateErrorTempControlSUP(bool active) {
    if (error_tempcontrol_sup != active) {
        error_tempcontrol_sup = active;
        has_error_data = true;
        last_update = millis();
        
        if (active) {
            Serial.println("⚠️  ErrorDataManager: Temperature control SUP error ACTIVE (bypass malfunction?)");
        } else {
            Serial.println("✅ ErrorDataManager: Temperature control SUP error cleared");
        }
        
        updateWarningIcon();
    }
}

void ErrorDataManager::updateAlarmFilter(bool active) {
    if (alarm_filter != active) {
        alarm_filter = active;
        has_error_data = true;
        last_update = millis();
        
        if (active) {
            Serial.println("⚠️  ErrorDataManager: Filter replacement alarm ACTIVE");
        } else {
            Serial.println("✅ ErrorDataManager: Filter replacement alarm cleared");
        }
        
        updateWarningIcon();
    }
}

void ErrorDataManager::updateWarningSystem(bool active) {
    if (warning_system != active) {
        warning_system = active;
        has_error_data = true;
        last_update = millis();
        
        if (active) {
            Serial.println("⚠️  ErrorDataManager: General system warning ACTIVE");
        } else {
            Serial.println("✅ ErrorDataManager: General system warning cleared");
        }
        
        updateWarningIcon();
    }
}

bool ErrorDataManager::hasAnyError() {
    return has_error_data;
}

bool ErrorDataManager::hasAnyActiveError() {
    return error_overheating ||
           error_temp_sensor_p_oda ||
           error_preheat_location ||
           error_ext_pressure_eha ||
           error_ext_pressure_sup ||
           error_tempcontrol_p_oda ||
           error_tempcontrol_sup ||
           alarm_filter ||
           warning_system;
}

bool ErrorDataManager::shouldClearErrors() {
    if (!has_error_data) return false;
    
    // Account for millis() rollover (happens every ~49 days)
    unsigned long now = millis();
    unsigned long elapsed;
    
    if (now >= last_update) {
        elapsed = now - last_update;
    } else {
        // Rollover occurred
        elapsed = (0xFFFFFFFF - last_update) + now + 1;
    }
    
    return elapsed > ERROR_TIMEOUT;
}

void ErrorDataManager::updateWarningIcon() {
    // CRITICAL: Only update display if it exists (prevents crash on headless boards)
    if (!hasDisplay()) {
        return;
    }
    
    // Show warning icon if ANY error is active
    bool should_show_warning = hasAnyActiveError();
    
    if (should_show_warning) {
        Serial.println("ErrorDataManager: Showing warning icon (MVHR error detected)");
        
        // Make icon visible
        lv_obj_clear_flag(GUI_Image__screen__image, LV_OBJ_FLAG_HIDDEN);
    } else {
        Serial.println("ErrorDataManager: Hiding warning icon (no MVHR errors)");
        
        // Hide icon (filter_data.cpp will manage it for filter warnings)
        // NOTE: Filter manager will show it again if filter needs replacement
        lv_obj_add_flag(GUI_Image__screen__image, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Request display refresh for icon visibility change
    GUI_request_display_refresh();
    lv_obj_invalidate(GUI_Image__screen__image);
}

} // namespace comfoair