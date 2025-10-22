#ifndef ERROR_DATA_H
#define ERROR_DATA_H

#include <Arduino.h>

namespace comfoair {

class ErrorDataManager {
public:
    ErrorDataManager();
    
    void setup();
    void loop();
    
    // Update individual error states from CAN
    void updateErrorOverheating(bool active);
    void updateErrorTempSensorPODA(bool active);
    void updateErrorPreheatLocation(bool active);
    void updateErrorExtPressureEHA(bool active);
    void updateErrorExtPressureSUP(bool active);
    void updateErrorTempControlPODA(bool active);
    void updateErrorTempControlSUP(bool active);
    void updateAlarmFilter(bool active);
    void updateWarningSystem(bool active);
    
    // Get current error state
    bool hasAnyError();
    bool hasAnyActiveError();
    
    // Get specific error states (for display/logging)
    bool getErrorOverheating() { return error_overheating; }
    bool getErrorTempSensorPODA() { return error_temp_sensor_p_oda; }
    bool getErrorPreheatLocation() { return error_preheat_location; }
    bool getErrorExtPressureEHA() { return error_ext_pressure_eha; }
    bool getErrorExtPressureSUP() { return error_ext_pressure_sup; }
    bool getErrorTempControlPODA() { return error_tempcontrol_p_oda; }
    bool getErrorTempControlSUP() { return error_tempcontrol_sup; }
    bool getAlarmFilter() { return alarm_filter; }
    bool getWarningSystem() { return warning_system; }
    
private:
    // Error state flags
    bool error_overheating;
    bool error_temp_sensor_p_oda;
    bool error_preheat_location;
    bool error_ext_pressure_eha;
    bool error_ext_pressure_sup;
    bool error_tempcontrol_p_oda;
    bool error_tempcontrol_sup;
    bool alarm_filter;
    bool warning_system;
    
    bool has_error_data;
    unsigned long last_update;
    
    // Update warning icon visibility based on error state
    void updateWarningIcon();
    
    // Check if we should clear error state (no CAN update for >1 hour)
    bool shouldClearErrors();
    
    static const unsigned long ERROR_TIMEOUT = 3600000; // 1 hour in milliseconds
};

} // namespace comfoair

#endif
