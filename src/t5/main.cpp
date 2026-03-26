#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <TouchDrvGT911.hpp>

#include "board_config.h"
#include "epaper_driver.h"
#include "ui_ventilation.h"
#include "ui_info_panel.h"

// Shared library includes
#include "comfoair/sensor_data.h"
#include "comfoair/control_manager.h"
#include "comfoair/filter_data.h"
#include "comfoair/error_data.h"
#include "mqtt/mqtt.h"
#include "wifi/wifi.h"
#include "time/time_manager.h"

// Forward declaration — user must create secrets_t5.h with credentials
#include "secrets_t5.h"

// =============================================================================
// Global instances
// =============================================================================
static comfoair::WiFi* wifi = nullptr;
static comfoair::MQTT* mqtt = nullptr;
static comfoair::SensorDataManager* sensorData = nullptr;
static comfoair::ControlManager* controlMgr = nullptr;
static comfoair::FilterDataManager* filterData = nullptr;
static comfoair::ErrorDataManager* errorData = nullptr;
static comfoair::TimeManager* timeMgr = nullptr;

static lv_display_t* display = nullptr;
static lv_indev_t* touch_indev = nullptr;
static TouchDrvGT911 touch;

// Weather state (updated from MQTT)
static WeatherData weather = {};


// Touch state
static int16_t last_touch_x = 0;
static int16_t last_touch_y = 0;
static bool touch_pressed = false;

// =============================================================================
// LVGL tick provider
// =============================================================================
static void lvgl_tick_cb(void) {
    lv_tick_inc(1);
}

// =============================================================================
// Touch input read callback for LVGL
// =============================================================================
static void touch_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    int16_t x, y;

    if (touch.getPoint(&x, &y, 1)) {
        // GT911 on T5 needs coordinate swap/mirror
        // touch.setSwapXY(true) and touch.setMirrorXY(false, true) handle this
        last_touch_x = x;
        last_touch_y = y;
        touch_pressed = true;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        touch_pressed = false;
        data->state = LV_INDEV_STATE_RELEASED;
    }

    data->point.x = last_touch_x;
    data->point.y = last_touch_y;
}

// =============================================================================
// Display callback wiring — connects shared library managers to LVGL UI
// =============================================================================

static void on_sensor_display_update(const comfoair::SensorData& d) {
    ui_ventilation_update_sensors(d);
}

static void on_fan_speed_display_update(uint8_t speed, bool boost) {
    ui_ventilation_update_fan_speed(speed, boost);
}

static void on_temp_profile_display_update(uint8_t profile) {
    ui_ventilation_update_temp_profile(profile);
}

static void on_boost_timer_display_update(int minutes) {
    ui_ventilation_update_boost_timer(minutes);
}

static void on_filter_display_update(int days, bool has_data) {
    ui_ventilation_update_filter(days, has_data);
}

static void on_warning_display_update(bool show) {
    ui_ventilation_update_warning(show);
}

static void on_wifi_icon_update(bool connected) {
    ui_ventilation_update_wifi(connected);
}

static void on_time_display_update(const char* time_str, const char* date_str) {
    ui_ventilation_update_time(time_str, date_str);
    ui_info_panel_update_time(time_str, date_str);
}

// =============================================================================
// MQTT topic subscription (remote client mode)
// =============================================================================
static void setup_mqtt_subscriptions() {
    // Sensor data
    mqtt->subscribeTo(MQTT_PREFIX "/extract_air_temp",
        [](char* topic, uint8_t* payload, unsigned int len) {
            sensorData->updateInsideTemp(atof((char*)payload));
        });
    mqtt->subscribeTo(MQTT_PREFIX "/outdoor_air_temp",
        [](char* topic, uint8_t* payload, unsigned int len) {
            sensorData->updateOutsideTemp(atof((char*)payload));
        });
    mqtt->subscribeTo(MQTT_PREFIX "/extract_air_humidity",
        [](char* topic, uint8_t* payload, unsigned int len) {
            sensorData->updateInsideHumidity(atof((char*)payload));
        });
    mqtt->subscribeTo(MQTT_PREFIX "/outdoor_air_humidity",
        [](char* topic, uint8_t* payload, unsigned int len) {
            sensorData->updateOutsideHumidity(atof((char*)payload));
        });

    // Control state
    mqtt->subscribeTo(MQTT_PREFIX "/fan_speed",
        [](char* topic, uint8_t* payload, unsigned int len) {
            controlMgr->updateFanSpeedFromCAN(atoi((char*)payload));
        });
    mqtt->subscribeTo(MQTT_PREFIX "/temp_profile",
        [](char* topic, uint8_t* payload, unsigned int len) {
            uint8_t profile = 0;
            const char* val = (const char*)payload;
            if (strcmp(val, "normal") == 0) profile = 0;
            else if (strcmp(val, "cool") == 0) profile = 1;
            else if (strcmp(val, "warm") == 0) profile = 2;
            controlMgr->updateTempProfileFromCAN(profile);
        });

    // Filter data
    mqtt->subscribeTo(MQTT_PREFIX "/remaining_days_filter_replacement",
        [](char* topic, uint8_t* payload, unsigned int len) {
            filterData->updateFilterDays(atoi((char*)payload));
        });

    // Error states
    mqtt->subscribeTo(MQTT_PREFIX "/error_overheating",
        [](char* topic, uint8_t* payload, unsigned int len) {
            errorData->updateErrorOverheating(atoi((char*)payload) != 0);
        });
    mqtt->subscribeTo(MQTT_PREFIX "/alarm_filter",
        [](char* topic, uint8_t* payload, unsigned int len) {
            errorData->updateAlarmFilter(atoi((char*)payload) != 0);
        });

    // Weather data from Home Assistant
    // HA publishes weather entity attributes to MQTT topics
    // Configure these topics in your HA MQTT automation
    mqtt->subscribeTo(MQTT_PREFIX "/weather/condition",
        [](char* topic, uint8_t* payload, unsigned int len) {
            strncpy(weather.condition, (char*)payload, sizeof(weather.condition) - 1);
            weather.condition[sizeof(weather.condition) - 1] = '\0';
            weather.valid = true;
            ui_info_panel_update_weather(weather);
        });
    mqtt->subscribeTo(MQTT_PREFIX "/weather/temperature",
        [](char* topic, uint8_t* payload, unsigned int len) {
            weather.temperature = atof((char*)payload);
            weather.valid = true;
            ui_info_panel_update_weather(weather);
        });
    mqtt->subscribeTo(MQTT_PREFIX "/weather/humidity",
        [](char* topic, uint8_t* payload, unsigned int len) {
            weather.humidity = atof((char*)payload);
            ui_info_panel_update_weather(weather);
        });
    mqtt->subscribeTo(MQTT_PREFIX "/weather/wind_speed",
        [](char* topic, uint8_t* payload, unsigned int len) {
            weather.wind_speed = atof((char*)payload);
            ui_info_panel_update_weather(weather);
        });
    mqtt->subscribeTo(MQTT_PREFIX "/weather/wind_bearing",
        [](char* topic, uint8_t* payload, unsigned int len) {
            strncpy(weather.wind_bearing, (char*)payload, sizeof(weather.wind_bearing) - 1);
            weather.wind_bearing[sizeof(weather.wind_bearing) - 1] = '\0';
            ui_info_panel_update_weather(weather);
        });
}

// =============================================================================
// Setup
// =============================================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n========================================");
    Serial.println("ComfoSense-T5 E-Paper Remote Client");
    Serial.println("========================================");

    // --- LVGL init ---
    lv_init();

    // Set up tick source (1ms timer)
    const esp_timer_create_args_t tick_timer_args = {
        .callback = [](void* arg) { lv_tick_inc(1); },
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick",
        .skip_unhandled_events = false,
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&tick_timer_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, 1000); // 1ms

    // --- E-paper display init ---
    display = epaper_init_display();
    if (!display) {
        Serial.println("FATAL: E-paper display init failed!");
        while (1) delay(1000);
    }

    // --- Touch init (GT911) ---
    Wire.begin(TOUCH_SDA, TOUCH_SCL);

    // Detect GT911 address (0x5D or 0x14)
    uint8_t touchAddress = 0;
    Wire.beginTransmission(0x14);
    if (Wire.endTransmission() == 0) {
        touchAddress = 0x14;
    }
    Wire.beginTransmission(0x5D);
    if (Wire.endTransmission() == 0) {
        touchAddress = 0x5D;
    }

    if (touchAddress) {
        touch.begin(Wire, touchAddress, TOUCH_SDA, TOUCH_SCL);
        touch.setMaxCoordinates(EPD_WIDTH, EPD_HEIGHT);
        touch.setSwapXY(true);
        touch.setMirrorXY(false, true);
        Serial.printf("Touch: GT911 found at 0x%02X\n", touchAddress);

        // Register LVGL input device
        touch_indev = lv_indev_create();
        lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(touch_indev, touch_read_cb);
    } else {
        Serial.println("Touch: GT911 NOT FOUND!");
    }

    // --- WiFi ---
    wifi = new comfoair::WiFi();
    wifi->configure({
        .ssid = WIFI_SSID,
        .pass = WIFI_PASS,
        .hostname = "comfosense-t5"
    });
    wifi->setWiFiIconCallback(on_wifi_icon_update);
    wifi->setup();

    // --- MQTT ---
    mqtt = new comfoair::MQTT();
    mqtt->configure({
        .host = MQTT_HOST,
        .port = MQTT_PORT,
        .user = MQTT_USER,
        .pass = MQTT_PASS
    });
    mqtt->setup();

    // --- Data managers (shared library) ---
    sensorData = new comfoair::SensorDataManager();
    sensorData->setDisplayCallback(on_sensor_display_update);
    sensorData->setup();

    controlMgr = new comfoair::ControlManager();
    controlMgr->setRemoteClientMode(true);
    controlMgr->setMqttPrefix(MQTT_PREFIX);
    controlMgr->setMQTT(mqtt);
    controlMgr->setFanSpeedDisplayCallback(on_fan_speed_display_update);
    controlMgr->setTempProfileDisplayCallback(on_temp_profile_display_update);
    controlMgr->setBoostTimerDisplayCallback(on_boost_timer_display_update);
    controlMgr->setup();

    filterData = new comfoair::FilterDataManager();
    filterData->setWarningThreshold(WARNING_THRESHOLD_DAYS);
    filterData->setDisplayCallback(on_filter_display_update);
    filterData->setWarningCallback(on_warning_display_update);
    filterData->setup();

    errorData = new comfoair::ErrorDataManager();
    errorData->setWarningCallback(on_warning_display_update);
    errorData->setup();

    // --- Time ---
    timeMgr = new comfoair::TimeManager();
    timeMgr->setTimezone(TIMEZONE);
    timeMgr->setRemoteClientMode(true);
    timeMgr->setDisplayCallback(on_time_display_update);
    timeMgr->setup();

    // --- MQTT subscriptions ---
    setup_mqtt_subscriptions();

    // --- Initialize ventilation panel UI ---
    lv_obj_t* screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_make(255, 255, 255), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    ui_ventilation_init(screen);
    ui_info_panel_init(screen);

    // Wire touch events → ControlManager
    ui_ventilation_set_fan_speed_callback([](bool increase) {
        if (increase) controlMgr->increaseFanSpeed();
        else controlMgr->decreaseFanSpeed();
    });
    ui_ventilation_set_boost_callback([]() {
        controlMgr->activateBoost();
    });
    ui_ventilation_set_temp_profile_callback([](uint8_t profile) {
        controlMgr->setTempProfile(profile);
    });

    Serial.println("========================================");
    Serial.println("Setup complete — entering main loop");
    Serial.println("========================================");
}

// =============================================================================
// Main loop
// =============================================================================
void loop() {
    // LVGL timer processing
    lv_timer_handler();

    // Touch polling (every 20ms for e-paper — no need for 5ms like LCD)
    static unsigned long last_touch_read = 0;
    unsigned long now = millis();
    if (now - last_touch_read >= 20) {
        if (touch_indev) {
            lv_indev_read(touch_indev);
        }
        last_touch_read = now;
    }

    // Manager loops (batched display updates)
    sensorData->loop();
    filterData->loop();
    errorData->loop();
    controlMgr->loop();

    // Network services
    wifi->loop();
    mqtt->loop();
    timeMgr->loop();

    // Periodic full refresh to prevent e-paper ghosting
    epaper_check_full_refresh();

    delay(1);
}
