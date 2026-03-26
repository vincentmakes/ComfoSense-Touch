#ifndef T5_UI_VENTILATION_H
#define T5_UI_VENTILATION_H

#include <lvgl.h>
#include "comfoair/sensor_data.h"

// Initialize the ventilation panel on the left half of the screen (0-479, 0-539)
void ui_ventilation_init(lv_obj_t* parent);

// Display update functions (called from manager callbacks)
void ui_ventilation_update_sensors(const comfoair::SensorData& data);
void ui_ventilation_update_fan_speed(uint8_t speed, bool boost);
void ui_ventilation_update_temp_profile(uint8_t profile);
void ui_ventilation_update_boost_timer(int minutes_remaining);
void ui_ventilation_update_filter(int days_remaining, bool has_data);
void ui_ventilation_update_warning(bool show_warning);
void ui_ventilation_update_wifi(bool connected);
void ui_ventilation_update_time(const char* time_str, const char* date_str);

// Set touch event callbacks (wired to ControlManager in main.cpp)
using FanSpeedChangeCallback = void(*)(bool increase);
using BoostCallback = void(*)();
using TempProfileCallback = void(*)(uint8_t profile);

void ui_ventilation_set_fan_speed_callback(FanSpeedChangeCallback cb);
void ui_ventilation_set_boost_callback(BoostCallback cb);
void ui_ventilation_set_temp_profile_callback(TempProfileCallback cb);

#endif // T5_UI_VENTILATION_H
