#include "ui_info_panel.h"
#include <Arduino.h>

// =============================================================================
// Layout constants — right half of 960x540 e-paper
// =============================================================================
static const int32_t PANEL_X = 480;
static const int32_t PANEL_WIDTH = 480;
static const int32_t PANEL_HEIGHT = 540;
static const int32_t PANEL_MARGIN = 15;

// E-paper grayscale colors
static const lv_color_t COLOR_BLACK = lv_color_make(0, 0, 0);
static const lv_color_t COLOR_DARK_GRAY = lv_color_make(60, 60, 60);
static const lv_color_t COLOR_MID_GRAY = lv_color_make(128, 128, 128);
static const lv_color_t COLOR_LIGHT_GRAY = lv_color_make(200, 200, 200);
static const lv_color_t COLOR_WHITE = lv_color_make(255, 255, 255);

// =============================================================================
// Widget references
// =============================================================================

// Time display (large, centered)
static lv_obj_t* lbl_time_large = nullptr;
static lv_obj_t* lbl_date_full = nullptr;

// Weather section
static lv_obj_t* lbl_weather_condition = nullptr;
static lv_obj_t* lbl_weather_temp = nullptr;
static lv_obj_t* lbl_weather_humidity = nullptr;
static lv_obj_t* lbl_weather_wind = nullptr;
static lv_obj_t* lbl_weather_label = nullptr;

// =============================================================================
// Weather condition to symbol mapping
// =============================================================================
static const char* weather_symbol(const char* condition) {
    // Map HA weather conditions to simple text representations
    if (strcmp(condition, "sunny") == 0 || strcmp(condition, "clear-night") == 0)
        return "CLEAR";
    if (strcmp(condition, "partlycloudy") == 0)
        return "PARTLY CLOUDY";
    if (strcmp(condition, "cloudy") == 0)
        return "CLOUDY";
    if (strcmp(condition, "rainy") == 0)
        return "RAIN";
    if (strcmp(condition, "snowy") == 0)
        return "SNOW";
    if (strcmp(condition, "fog") == 0)
        return "FOG";
    if (strcmp(condition, "windy") == 0 || strcmp(condition, "windy-variant") == 0)
        return "WINDY";
    if (strcmp(condition, "lightning") == 0 || strcmp(condition, "lightning-rainy") == 0)
        return "STORM";
    if (strcmp(condition, "hail") == 0)
        return "HAIL";
    if (strcmp(condition, "snowy-rainy") == 0)
        return "SLEET";
    if (strcmp(condition, "pouring") == 0)
        return "HEAVY RAIN";
    return condition; // Fallback: show raw condition
}

// =============================================================================
// Helper
// =============================================================================
static lv_obj_t* create_label(lv_obj_t* parent, const char* text,
                               const lv_font_t* font, lv_color_t color) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    return lbl;
}

// =============================================================================
// Initialize info panel
// =============================================================================
void ui_info_panel_init(lv_obj_t* parent) {

    // --- Right panel container ---
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(panel, PANEL_X, 0);
    lv_obj_set_size(panel, PANEL_WIDTH, PANEL_HEIGHT);
    lv_obj_set_style_bg_color(panel, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, PANEL_MARGIN, 0);
    lv_obj_set_style_radius(panel, 0, 0);

    // =====================================================================
    // TIME SECTION (top, large centered clock)
    // =====================================================================
    lv_obj_t* time_section = lv_obj_create(panel);
    lv_obj_remove_flag(time_section, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(time_section, PANEL_WIDTH - 2 * PANEL_MARGIN, 200);
    lv_obj_set_align(time_section, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_opa(time_section, LV_OPA_0, 0);
    lv_obj_set_style_border_width(time_section, 1, 0);
    lv_obj_set_style_border_color(time_section, COLOR_LIGHT_GRAY, 0);
    lv_obj_set_style_border_side(time_section, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(time_section, 0, 0);
    lv_obj_set_style_radius(time_section, 0, 0);

    // Large time display
    lbl_time_large = create_label(time_section, "00:00",
                                   &lv_font_montserrat_48, COLOR_BLACK);
    lv_obj_set_align(lbl_time_large, LV_ALIGN_CENTER);
    lv_obj_set_pos(lbl_time_large, 0, -20);

    // Date below time
    lbl_date_full = create_label(time_section, "--",
                                  &lv_font_montserrat_18, COLOR_DARK_GRAY);
    lv_obj_set_align(lbl_date_full, LV_ALIGN_CENTER);
    lv_obj_set_pos(lbl_date_full, 0, 30);

    // =====================================================================
    // WEATHER SECTION
    // =====================================================================
    lv_obj_t* weather_section = lv_obj_create(panel);
    lv_obj_remove_flag(weather_section, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(weather_section, PANEL_WIDTH - 2 * PANEL_MARGIN, 280);
    lv_obj_set_align(weather_section, LV_ALIGN_CENTER);
    lv_obj_set_pos(weather_section, 0, 40);
    lv_obj_set_style_bg_opa(weather_section, LV_OPA_0, 0);
    lv_obj_set_style_border_width(weather_section, 0, 0);
    lv_obj_set_style_pad_all(weather_section, 10, 0);
    lv_obj_set_style_radius(weather_section, 0, 0);

    // "WEATHER" label
    lbl_weather_label = create_label(weather_section, "WEATHER",
                                      &lv_font_montserrat_14, COLOR_MID_GRAY);
    lv_obj_set_align(lbl_weather_label, LV_ALIGN_TOP_LEFT);

    // Condition (e.g., "PARTLY CLOUDY")
    lbl_weather_condition = create_label(weather_section, "--",
                                          &lv_font_montserrat_28, COLOR_BLACK);
    lv_obj_set_align(lbl_weather_condition, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(lbl_weather_condition, 0, 30);

    // Temperature
    lbl_weather_temp = create_label(weather_section, "-- C",
                                     &lv_font_montserrat_48, COLOR_BLACK);
    lv_obj_set_align(lbl_weather_temp, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(lbl_weather_temp, 0, 80);

    // Humidity
    lbl_weather_humidity = create_label(weather_section, "Humidity: --%",
                                         &lv_font_montserrat_18, COLOR_DARK_GRAY);
    lv_obj_set_align(lbl_weather_humidity, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(lbl_weather_humidity, 0, 150);

    // Wind
    lbl_weather_wind = create_label(weather_section, "Wind: -- km/h",
                                     &lv_font_montserrat_18, COLOR_DARK_GRAY);
    lv_obj_set_align(lbl_weather_wind, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(lbl_weather_wind, 0, 180);

    Serial.println("UI: Info panel initialized");
}

// =============================================================================
// Display update functions
// =============================================================================

void ui_info_panel_update_time(const char* time_str, const char* date_str) {
    lv_label_set_text(lbl_time_large, time_str);
    lv_label_set_text(lbl_date_full, date_str);
}

void ui_info_panel_update_weather(const WeatherData& data) {
    if (!data.valid) return;

    // Condition
    lv_label_set_text(lbl_weather_condition, weather_symbol(data.condition));

    // Temperature
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f C", data.temperature);
    lv_label_set_text(lbl_weather_temp, buf);

    // Humidity
    snprintf(buf, sizeof(buf), "Humidity: %.0f%%", data.humidity);
    lv_label_set_text(lbl_weather_humidity, buf);

    // Wind
    char wind_buf[32];
    snprintf(wind_buf, sizeof(wind_buf), "Wind: %.0f km/h %s",
             data.wind_speed, data.wind_bearing);
    lv_label_set_text(lbl_weather_wind, wind_buf);
}
