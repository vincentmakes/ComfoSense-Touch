#include "ui_ventilation.h"
#include "images/t5_images.h"
#include <Arduino.h>

// =============================================================================
// Layout constants — left half of 960x540 e-paper
// =============================================================================
static const int32_t PANEL_WIDTH = 470;
static const int32_t PANEL_HEIGHT = 540;
static const int32_t PANEL_MARGIN = 5;

// E-paper grayscale colors
static const lv_color_t COLOR_BLACK = lv_color_make(0, 0, 0);
static const lv_color_t COLOR_WHITE = lv_color_make(255, 255, 255);
static const lv_color_t COLOR_DARK_GRAY = lv_color_make(60, 60, 60);
static const lv_color_t COLOR_MID_GRAY = lv_color_make(128, 128, 128);
static const lv_color_t COLOR_LIGHT_GRAY = lv_color_make(200, 200, 200);

// =============================================================================
// Widget references
// =============================================================================

// Top status bar (warning only — time/date/WiFi moved to info panel)
static lv_obj_t* lbl_warning = nullptr;

// Center — fan speed display (images from original LCD, converted to grayscale)
static lv_obj_t* img_fan0 = nullptr;
static lv_obj_t* img_fan1 = nullptr;
static lv_obj_t* img_fan2 = nullptr;
static lv_obj_t* img_fan3 = nullptr;
static lv_obj_t* img_fanboost = nullptr;
static lv_obj_t* lbl_boost_timer = nullptr;    // "18 min" during boost

// Center — sensor readings
static lv_obj_t* lbl_inside_temp = nullptr;
static lv_obj_t* lbl_inside_hum = nullptr;
static lv_obj_t* lbl_outside_temp = nullptr;
static lv_obj_t* lbl_outside_hum = nullptr;
static lv_obj_t* lbl_inside_label = nullptr;
static lv_obj_t* lbl_outside_label = nullptr;

// Center — filter
static lv_obj_t* lbl_filter = nullptr;

// Bottom — controls
static lv_obj_t* btn_speed_minus = nullptr;
static lv_obj_t* btn_speed_plus = nullptr;
static lv_obj_t* btn_boost = nullptr;
static lv_obj_t* dropdown_mode = nullptr;

// Callbacks
static FanSpeedChangeCallback fan_speed_cb = nullptr;
static BoostCallback boost_cb = nullptr;
static TempProfileCallback temp_profile_cb = nullptr;

// =============================================================================
// Event handlers
// =============================================================================
static void on_speed_minus_clicked(lv_event_t* e) {
    if (fan_speed_cb) fan_speed_cb(false);
}

static void on_speed_plus_clicked(lv_event_t* e) {
    if (fan_speed_cb) fan_speed_cb(true);
}

static void on_boost_clicked(lv_event_t* e) {
    if (boost_cb) boost_cb();
}

static void on_mode_changed(lv_event_t* e) {
    lv_obj_t* dropdown = (lv_obj_t*)lv_event_get_target(e);
    uint32_t sel = lv_dropdown_get_selected(dropdown);
    if (temp_profile_cb) temp_profile_cb((uint8_t)sel);
}

// =============================================================================
// Helpers — create styled label
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
// Initialize ventilation panel
// =============================================================================
void ui_ventilation_init(lv_obj_t* parent) {

    // --- Left panel container ---
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_set_size(panel, PANEL_WIDTH, PANEL_HEIGHT);
    lv_obj_set_style_bg_color(panel, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, COLOR_MID_GRAY, 0);
    lv_obj_set_style_border_side(panel, LV_BORDER_SIDE_RIGHT, 0);
    lv_obj_set_style_pad_all(panel, PANEL_MARGIN, 0);
    lv_obj_set_style_radius(panel, 0, 0);

    // =====================================================================
    // TOP STATUS BAR (warning icon only — time/date/WiFi on info panel)
    // =====================================================================
    lv_obj_t* top_bar = lv_obj_create(panel);
    lv_obj_remove_flag(top_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(top_bar, PANEL_WIDTH - 2 * PANEL_MARGIN, 40);
    lv_obj_set_align(top_bar, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_0, 0);
    lv_obj_set_style_border_width(top_bar, 1, 0);
    lv_obj_set_style_border_color(top_bar, COLOR_LIGHT_GRAY, 0);
    lv_obj_set_style_border_side(top_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(top_bar, 4, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);

    // Warning icon (hidden by default) — grayscale image
    lbl_warning = lv_img_create(top_bar);
    lv_img_set_src(lbl_warning, &warning_gray);
    lv_obj_set_align(lbl_warning, LV_ALIGN_RIGHT_MID);
    lv_obj_add_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);

    // =====================================================================
    // CENTER — FAN SPEED DISPLAY
    // =====================================================================
    lv_obj_t* center = lv_obj_create(panel);
    lv_obj_remove_flag(center, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(center, PANEL_WIDTH - 2 * PANEL_MARGIN, 280);
    lv_obj_set_align(center, LV_ALIGN_CENTER);
    lv_obj_set_pos(center, 0, -15);
    lv_obj_set_style_bg_opa(center, LV_OPA_0, 0);
    lv_obj_set_style_border_width(center, 0, 0);
    lv_obj_set_style_pad_all(center, 0, 0);

    // Fan speed images (same images as LCD version, converted to grayscale)
    // All stacked at same position, only one visible at a time
    auto create_fan_img = [&](lv_obj_t* parent, const lv_image_dsc_t* src) -> lv_obj_t* {
        lv_obj_t* img = lv_img_create(parent);
        lv_img_set_src(img, src);
        lv_obj_set_align(img, LV_ALIGN_TOP_MID);
        lv_obj_set_pos(img, 0, 5);
        lv_obj_set_style_img_opa(img, LV_OPA_0, 0); // Hidden by default
        return img;
    };

    img_fan0 = create_fan_img(center, &fan0_gray);
    img_fan1 = create_fan_img(center, &fan1_gray);
    img_fan2 = create_fan_img(center, &fan2_gray);
    img_fan3 = create_fan_img(center, &fan3_gray);
    img_fanboost = create_fan_img(center, &fanboost_gray);

    // Show fan2 by default
    lv_obj_set_style_img_opa(img_fan2, LV_OPA_COVER, 0);

    // Boost timer label (overlaid on fan image, hidden by default)
    lbl_boost_timer = create_label(center, "", &lv_font_montserrat_14, COLOR_BLACK);
    lv_obj_set_align(lbl_boost_timer, LV_ALIGN_TOP_MID);
    lv_obj_set_pos(lbl_boost_timer, -90, 15);
    lv_obj_add_flag(lbl_boost_timer, LV_OBJ_FLAG_HIDDEN);

    // --- Sensor readings ---
    // Inside label
    lbl_inside_label = create_label(center, "INSIDE", &lv_font_montserrat_12, COLOR_MID_GRAY);
    lv_obj_set_align(lbl_inside_label, LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(lbl_inside_label, 15, 20);

    // Inside temp
    lbl_inside_temp = create_label(center, "-- C", &lv_font_montserrat_28, COLOR_BLACK);
    lv_obj_set_align(lbl_inside_temp, LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(lbl_inside_temp, 10, 45);

    // Inside humidity
    lbl_inside_hum = create_label(center, "--%", &lv_font_montserrat_18, COLOR_DARK_GRAY);
    lv_obj_set_align(lbl_inside_hum, LV_ALIGN_LEFT_MID);
    lv_obj_set_pos(lbl_inside_hum, 15, 75);

    // Outside label
    lbl_outside_label = create_label(center, "OUTSIDE", &lv_font_montserrat_12, COLOR_MID_GRAY);
    lv_obj_set_align(lbl_outside_label, LV_ALIGN_RIGHT_MID);
    lv_obj_set_pos(lbl_outside_label, -15, 20);

    // Outside temp
    lbl_outside_temp = create_label(center, "-- C", &lv_font_montserrat_28, COLOR_BLACK);
    lv_obj_set_align(lbl_outside_temp, LV_ALIGN_RIGHT_MID);
    lv_obj_set_pos(lbl_outside_temp, -10, 45);

    // Outside humidity
    lbl_outside_hum = create_label(center, "--%", &lv_font_montserrat_18, COLOR_DARK_GRAY);
    lv_obj_set_align(lbl_outside_hum, LV_ALIGN_RIGHT_MID);
    lv_obj_set_pos(lbl_outside_hum, -15, 75);

    // Filter info (bottom of center)
    lbl_filter = create_label(center, "Filter: -- days", &lv_font_montserrat_14, COLOR_DARK_GRAY);
    lv_obj_set_align(lbl_filter, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_pos(lbl_filter, 0, -5);

    // =====================================================================
    // BOTTOM — CONTROL BUTTONS
    // =====================================================================
    lv_obj_t* bottom_bar = lv_obj_create(panel);
    lv_obj_remove_flag(bottom_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(bottom_bar, PANEL_WIDTH - 2 * PANEL_MARGIN, 120);
    lv_obj_set_align(bottom_bar, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_bg_opa(bottom_bar, LV_OPA_0, 0);
    lv_obj_set_style_border_width(bottom_bar, 1, 0);
    lv_obj_set_style_border_color(bottom_bar, COLOR_LIGHT_GRAY, 0);
    lv_obj_set_style_border_side(bottom_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(bottom_bar, 8, 0);
    lv_obj_set_style_radius(bottom_bar, 0, 0);

    // Speed minus button
    btn_speed_minus = lv_button_create(bottom_bar);
    lv_obj_set_size(btn_speed_minus, 120, 55);
    lv_obj_set_align(btn_speed_minus, LV_ALIGN_TOP_LEFT);
    lv_obj_set_style_bg_color(btn_speed_minus, COLOR_LIGHT_GRAY, 0);
    lv_obj_set_style_bg_color(btn_speed_minus, COLOR_MID_GRAY, LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_speed_minus, 6, 0);
    lv_obj_add_event_cb(btn_speed_minus, on_speed_minus_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t* img_minus = lv_img_create(btn_speed_minus);
    lv_img_set_src(img_minus, &minus_gray);
    lv_obj_set_align(img_minus, LV_ALIGN_CENTER);

    // Speed plus button
    btn_speed_plus = lv_button_create(bottom_bar);
    lv_obj_set_size(btn_speed_plus, 120, 55);
    lv_obj_set_align(btn_speed_plus, LV_ALIGN_TOP_MID);
    lv_obj_set_style_bg_color(btn_speed_plus, COLOR_LIGHT_GRAY, 0);
    lv_obj_set_style_bg_color(btn_speed_plus, COLOR_MID_GRAY, LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_speed_plus, 6, 0);
    lv_obj_add_event_cb(btn_speed_plus, on_speed_plus_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t* img_plus = lv_img_create(btn_speed_plus);
    lv_img_set_src(img_plus, &plus_gray);
    lv_obj_set_align(img_plus, LV_ALIGN_CENTER);

    // Boost button
    btn_boost = lv_button_create(bottom_bar);
    lv_obj_set_size(btn_boost, 120, 55);
    lv_obj_set_align(btn_boost, LV_ALIGN_TOP_RIGHT);
    lv_obj_set_style_bg_color(btn_boost, COLOR_DARK_GRAY, 0);
    lv_obj_set_style_bg_color(btn_boost, COLOR_BLACK, LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn_boost, 6, 0);
    lv_obj_add_event_cb(btn_boost, on_boost_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_boost = lv_label_create(btn_boost);
    lv_label_set_text(lbl_boost, "BOOST");
    lv_obj_set_style_text_font(lbl_boost, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_boost, COLOR_WHITE, 0);
    lv_obj_set_align(lbl_boost, LV_ALIGN_CENTER);

    // Mode dropdown
    dropdown_mode = lv_dropdown_create(bottom_bar);
    lv_dropdown_set_options(dropdown_mode, "NORMAL\nCOOLING\nHEATING");
    lv_obj_set_size(dropdown_mode, 140, 40);
    lv_obj_set_align(dropdown_mode, LV_ALIGN_BOTTOM_LEFT);
    lv_obj_set_pos(dropdown_mode, 0, 0);
    lv_obj_set_style_text_font(dropdown_mode, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(dropdown_mode, COLOR_BLACK, 0);
    lv_obj_set_style_bg_color(dropdown_mode, COLOR_WHITE, 0);
    lv_obj_set_style_border_color(dropdown_mode, COLOR_MID_GRAY, 0);
    lv_obj_set_style_border_width(dropdown_mode, 1, 0);
    lv_obj_set_style_radius(dropdown_mode, 4, 0);
    lv_obj_add_event_cb(dropdown_mode, on_mode_changed, LV_EVENT_VALUE_CHANGED, NULL);

    Serial.println("UI: Ventilation panel initialized");
}

// =============================================================================
// Display update functions
// =============================================================================

void ui_ventilation_update_sensors(const comfoair::SensorData& data) {
    char buf[16];

    snprintf(buf, sizeof(buf), "%.1f C", data.inside_temp);
    lv_label_set_text(lbl_inside_temp, buf);

    snprintf(buf, sizeof(buf), "%.0f%%", data.inside_humidity);
    lv_label_set_text(lbl_inside_hum, buf);

    snprintf(buf, sizeof(buf), "%.1f C", data.outside_temp);
    lv_label_set_text(lbl_outside_temp, buf);

    snprintf(buf, sizeof(buf), "%.0f%%", data.outside_humidity);
    lv_label_set_text(lbl_outside_hum, buf);
}

void ui_ventilation_update_fan_speed(uint8_t speed, bool boost) {
    // Hide all fan images first
    lv_obj_set_style_img_opa(img_fan0, LV_OPA_0, 0);
    lv_obj_set_style_img_opa(img_fan1, LV_OPA_0, 0);
    lv_obj_set_style_img_opa(img_fan2, LV_OPA_0, 0);
    lv_obj_set_style_img_opa(img_fan3, LV_OPA_0, 0);
    lv_obj_set_style_img_opa(img_fanboost, LV_OPA_0, 0);

    // Show the appropriate image
    lv_obj_t* active = nullptr;
    if (boost) {
        active = img_fanboost;
    } else {
        switch (speed) {
            case 0: active = img_fan0; break;
            case 1: active = img_fan1; break;
            case 2: active = img_fan2; break;
            case 3: active = img_fan3; break;
        }
    }

    if (active) {
        lv_obj_set_style_img_opa(active, LV_OPA_COVER, 0);
    }
}

void ui_ventilation_update_temp_profile(uint8_t profile) {
    if (dropdown_mode && profile <= 2) {
        lv_dropdown_set_selected(dropdown_mode, profile);
    }
}

void ui_ventilation_update_boost_timer(int minutes_remaining) {
    if (minutes_remaining > 0) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%d min", minutes_remaining);
        lv_label_set_text(lbl_boost_timer, buf);
        lv_obj_clear_flag(lbl_boost_timer, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lbl_boost_timer, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_ventilation_update_filter(int days_remaining, bool has_data) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Filter: %d days", days_remaining);
    lv_label_set_text(lbl_filter, buf);
}

void ui_ventilation_update_warning(bool show_warning) {
    if (show_warning) {
        lv_obj_clear_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lbl_warning, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_ventilation_update_wifi(bool connected) {
    // WiFi display moved to info panel — no-op here
    (void)connected;
}

void ui_ventilation_update_time(const char* time_str, const char* date_str) {
    // Time/date display moved to info panel — no-op here
    (void)time_str;
    (void)date_str;
}

// =============================================================================
// Callback setters
// =============================================================================

void ui_ventilation_set_fan_speed_callback(FanSpeedChangeCallback cb) {
    fan_speed_cb = cb;
}

void ui_ventilation_set_boost_callback(BoostCallback cb) {
    boost_cb = cb;
}

void ui_ventilation_set_temp_profile_callback(TempProfileCallback cb) {
    temp_profile_cb = cb;
}
