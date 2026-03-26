#ifndef T5_UI_INFO_PANEL_H
#define T5_UI_INFO_PANEL_H

#include <lvgl.h>

// Weather data structure (received via MQTT from HA)
struct WeatherData {
    char condition[32];     // "sunny", "cloudy", "rainy", etc.
    float temperature;      // Current outdoor temp from weather service
    float humidity;         // Current outdoor humidity
    float wind_speed;       // km/h
    char wind_bearing[4];   // "N", "NW", "SE", etc.
    bool valid;
};

// Initialize the info panel on the right half of the screen (480-959, 0-539)
void ui_info_panel_init(lv_obj_t* parent);

// Display update functions
void ui_info_panel_update_time(const char* time_str, const char* date_str);
void ui_info_panel_update_weather(const WeatherData& data);

#endif // T5_UI_INFO_PANEL_H
