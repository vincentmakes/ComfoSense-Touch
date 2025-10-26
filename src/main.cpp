#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "TouchDrvGT911.hpp"

// Configuration
#include "secrets.h"  // CRITICAL: Must include for MQTT_ENABLED and NTM_* defines

// Your app modules
#include "wifi/wifi.h"
#include "comfoair/comfoair.h"
#include "comfoair/sensor_data.h"
#include "comfoair/filter_data.h"
#include "comfoair/control_manager.h"
#include "comfoair/error_data.h"  // ← NEW: Error data manager
#include "screen/screen_manager.h"  // NEW: Screen manager for NTM
#include "mqtt/mqtt.h"
#include "ota/ota.h"

#include "time/time_manager.h"
#include "lvgl.h"
#include "ui/GUI.h"


// Add counter at top of main.cpp
static uint32_t touch_read_counter = 0;
static uint32_t event_fire_counter = 0;

// Display configuration
#define SCREEN_WIDTH   480
#define SCREEN_HEIGHT  480

// I2C Expander TCA9554 for backlight and reset control
#define EXPA_I2C_SDA   15
#define EXPA_I2C_SCL   7
#define TCA9554_ADDR   0x20

#define EXIO_TP_RST    1    // Touch reset
#define EXIO_BL_EN     2    // Backlight enable
#define EXIO_LCD_RST   3    // LCD reset

#define TCA_REG_OUTPUT 0x01
#define TCA_REG_CONFIG 0x03

// Touch controller GT911
#define TOUCH_SDA  15
#define TOUCH_SCL  7
#define TOUCH_INT  16
#define TOUCH_RST  -1  // Controlled via TCA9554 EXIO1

// GT911 touch controller
TouchDrvGT911 touch;

// Arduino_GFX setup for Waveshare ESP32-S3 Touch LCD 4.0"
Arduino_DataBus *bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED /* DC */, 42 /* CS */,
    2 /* SCK */, 1 /* MOSI */, GFX_NOT_DEFINED /* MISO */);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
    40 /* DE */, 39 /* VSYNC */, 38 /* HSYNC */, 41 /* PCLK */,
    46 /* R0 */, 3 /* R1 */, 8 /* R2 */, 18 /* R3 */, 17 /* R4 */,
    14 /* G0 */, 13 /* G1 */, 12 /* G2 */, 11 /* G3 */, 10 /* G4 */, 9 /* G5 */,
    5 /* B0 */, 45 /* B1 */, 48 /* B2 */, 47 /* B3 */, 21 /* B4 */,
    1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
    1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);

Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
    480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
    bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

// LVGL display buffers
static lv_color_t *lv_buf1 = nullptr;
static lv_color_t *lv_buf2 = nullptr;
static const size_t lv_buf_pixels = SCREEN_WIDTH * 60;

// Your subsystems
comfoair::ComfoAir *comfo = nullptr;
comfoair::WiFi *wifi = nullptr;
comfoair::MQTT *mqtt = nullptr;
comfoair::OTA *ota = nullptr;
comfoair::TimeManager *timeMgr = nullptr;
comfoair::SensorDataManager *sensorData = nullptr;
comfoair::FilterDataManager *filterData = nullptr;
comfoair::ControlManager *controlMgr = nullptr;
comfoair::ErrorDataManager *errorData = nullptr;  // ← NEW: Error data manager
comfoair::ScreenManager *screenMgr = nullptr;  // NEW: Screen manager

// Track current backlight state for TCA9554
static uint8_t current_tca_state = 0x0E;  // Default: all high (backlight on, LCD on, touch on)

// C interface functions for GUI to call control manager
extern "C" {
  void control_manager_increase_fan_speed() {
    if (controlMgr) controlMgr->increaseFanSpeed();
  }
  
  void control_manager_decrease_fan_speed() {
    if (controlMgr) controlMgr->decreaseFanSpeed();
  }
  
  void control_manager_activate_boost() {
    if (controlMgr) controlMgr->activateBoost();
  }
  
  void control_manager_set_temp_profile(uint8_t profile) {
    if (controlMgr) controlMgr->setTempProfile(profile);
  }
}

// Global touch input device for manual polling
static lv_indev_t *global_touch_indev = nullptr;

// TCA9554 I2C expander control
static void tca_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

// NEW: Backlight control function for ScreenManager
static void control_backlight(bool on) {
  if (on) {
    // Turn backlight on (EXIO2 = bit 2)
    current_tca_state |= (1 << (EXIO_BL_EN - 1));
    Serial.println("[Backlight] ON");
  } else {
    // Turn backlight off (EXIO2 = bit 2)
    current_tca_state &= ~(1 << (EXIO_BL_EN - 1));
    Serial.println("[Backlight] OFF");
  }
  tca_write(TCA_REG_OUTPUT, current_tca_state);
}

static void init_expander() {
  Wire.begin(EXPA_I2C_SDA, EXPA_I2C_SCL, 400000);
  delay(50);
  
  // Configure EXIO1-5 as outputs
  // Bit pattern: 1=input, 0=output
  // We need EXIO1(bit0), EXIO2(bit1), EXIO3(bit2), EXIO5(bit4) as outputs
  // 0xE1 = 0b11100001 = EXIO1,2,3,5 as outputs, EXIO4,6,7,8 as inputs
  tca_write(TCA_REG_CONFIG, 0xE1);
  
  // Power-on sequence
  tca_write(TCA_REG_OUTPUT, 0x00);  // All low - reset INCLUDING touch
  delay(50);
  tca_write(TCA_REG_OUTPUT, 0x08);  // LCD reset high
  delay(50);
  tca_write(TCA_REG_OUTPUT, 0x0C);  // Backlight on
  current_tca_state = 0x0C;  // Track state
  delay(50);
  tca_write(TCA_REG_OUTPUT, 0x0E);  // Touch reset high - ENABLE TOUCH
  current_tca_state = 0x0E;  // Track state
  delay(150);  // Delay for GT911 to initialize
}

// LVGL flush callback
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  lv_display_flush_ready(disp);
}

static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  static int16_t last_x = 0;
  static int16_t last_y = 0;
  static bool touch_active = false;
  static unsigned long last_touch_seen = 0;
  static unsigned long touch_start_time = 0;
  
  // ⚡ OPTIMIZED TIMING - Faster response!
  static const unsigned long RELEASE_DELAY_MS = 20;      // Reduced from 30ms
  static const unsigned long MIN_PRESS_DURATION_MS = 50; // Reduced from 80ms
  
  int16_t x[5], y[5];
  uint8_t touched = touch.getPoint(x, y, 5);
  bool finger_detected_now = (touched > 0);
  unsigned long now = millis();
  
  // Update last seen time whenever we detect touch
  if (finger_detected_now) {
    last_touch_seen = now;
    last_x = x[0];
    last_y = y[0];
    
    // NEW: Notify screen manager of touch (for NTM wake)
    if (screenMgr) {
      screenMgr->onTouchDetected();
    }
  }
  
  unsigned long time_since_touch = now - last_touch_seen;
  
  // ============================================================================
  // STATE MACHINE WITH FAST HYSTERESIS
  // ============================================================================
  
  if (finger_detected_now && !touch_active) {
    // ────────────────────────────────────────────────────────────────────────
    // NEW PRESS - Report immediately!
    // ────────────────────────────────────────────────────────────────────────
    
    touch_active = true;
    touch_start_time = now;
    
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = last_x;
    data->point.y = last_y;
    
    Serial.printf("🟢 [%lu] PRESS START at (%d, %d)\n", now, last_x, last_y);
  }
  else if (touch_active && time_since_touch < RELEASE_DELAY_MS) {
    // ────────────────────────────────────────────────────────────────────────
    // PRESS HELD - Keep reporting pressed
    // ────────────────────────────────────────────────────────────────────────
    
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = last_x;
    data->point.y = last_y;
  }
  else if (touch_active && time_since_touch >= RELEASE_DELAY_MS) {
    // ────────────────────────────────────────────────────────────────────────
    // PRESS RELEASED - Faster release detection!
    // ────────────────────────────────────────────────────────────────────────
    
    unsigned long press_duration = now - touch_start_time;
    
    // âœ… ALWAYS release regardless of duration - let LVGL handle debouncing
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = last_x;
    data->point.y = last_y;
    
    Serial.printf("🔴 [%lu] PRESS RELEASED after %lums\n", now, press_duration);
    
    touch_active = false;
  }
  else {
    // ────────────────────────────────────────────────────────────────────────
    // NO TOUCH
    // ────────────────────────────────────────────────────────────────────────
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // ============================================================================
  // REMOTE CLIENT MODE CHECK
  // ============================================================================
  #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
    Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
    Serial.println("║     REMOTE CLIENT MODE ENABLED                            ║");
    Serial.println("║     - No CAN bus initialization                           ║");
    Serial.println("║     - All data via MQTT from bridge device                ║");
    Serial.println("║     - Commands sent via MQTT                              ║");
    Serial.println("║     - Time sync disabled                                  ║");
    Serial.println("╚═══════════════════════════════════════════════════════════╝\n");
  #else
    Serial.println("\n╔═══════════════════════════════════════════════════════════╗");
    Serial.println("║     NORMAL MODE - Direct CAN Communication                ║");
    Serial.println("╚═══════════════════════════════════════════════════════════╝\n");
  #endif
  
  Serial.println("=== ESP32-S3 Touch LCD 4.0\" MVHR Controller ===");
  Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());
  
  setCpuFrequencyMhz(240);
  
  if (psramFound()) {
    Serial.printf("PSRAM: %d KB\n", ESP.getPsramSize() / 1024);
  }
  
  // 1. Initialize I2C expander (backlight, LCD reset)
  init_expander();
  
  // 2. Initialize display
  if (!gfx->begin()) {
    Serial.println("ERROR: Display init failed!");
    while(1) delay(1000);
  }
  gfx->fillScreen(BLACK);
  
  // 3. Initialize LVGL
  lv_init();
  
  lv_buf1 = (lv_color_t*)heap_caps_malloc(lv_buf_pixels * sizeof(lv_color_t),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  lv_buf2 = (lv_color_t*)heap_caps_malloc(lv_buf_pixels * sizeof(lv_color_t),
                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  
  if (!lv_buf1 || !lv_buf2) {
    Serial.println("ERROR: Failed to allocate LVGL buffers!");
    while(1) delay(1000);
  }
  
  lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_buffers(disp, lv_buf1, lv_buf2,
                         lv_buf_pixels * sizeof(lv_color_t),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, lvgl_flush_cb);
  
  // 4. Initialize touch controller
  Serial.println("Initializing touch...");
  
  pinMode(TOUCH_INT, INPUT);
  touch.setPins(-1, TOUCH_INT);
  
  if (touch.begin(Wire, GT911_SLAVE_ADDRESS_L, TOUCH_SDA, TOUCH_SCL)) {
    Serial.println("Touch initialized successfully");
    
    // ✅ Keep setMaxTouchPoint(5) - this is fine
    touch.setMaxTouchPoint(5);
    
    Serial.println("Touch configuration:");
    Serial.println("  - Max touch points: 5");
    Serial.println("  - Debounce: 150ms (in callback)");
    Serial.println("  - Polling: 5ms (200Hz)");
  } else {
    Serial.println("ERROR: Touch initialization failed");
  }
  
  // 5. Load GUI
  Serial.println("Loading GUI...");
  GUI_init();
  Serial.println("GUI loaded");
  
  // Initialize GUI displays (moved to GUI_Events.c)
  GUI_init_fan_speed_display();
  GUI_init_dropdown_styling();
  
  // Force initial render
  lv_timer_handler();
  delay(100);
  
  // 6. Register touch input device AFTER GUI is loaded
  Serial.println("Registering touch input device...");
  lv_display_t *disp_touch = lv_display_get_default();
  
  lv_indev_t *touch_indev = lv_indev_create();
  lv_indev_set_type(touch_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(touch_indev, lvgl_touch_read_cb);
  lv_indev_set_display(touch_indev, disp_touch);
  
  global_touch_indev = touch_indev;
  Serial.println("Touch registered with LVGL");
  
  // Force a few LVGL updates
  for (int i = 0; i < 5; i++) {
    lv_timer_handler();
    delay(10);
  }

  // 7. Initialize subsystems
  Serial.println("\nInitializing subsystems...");
  
  wifi = new comfoair::WiFi();
  
  // ============================================================================
  // MQTT CONFIGURATION
  // ============================================================================
  #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
    // In remote client mode, MQTT is MANDATORY
    mqtt = new comfoair::MQTT();
    Serial.println("MQTT client enabled (REQUIRED for Remote Client Mode)");
  #else
    // In normal mode, MQTT is optional
    #ifdef MQTT_ENABLED
      #if MQTT_ENABLED
        mqtt = new comfoair::MQTT();
        Serial.println("MQTT client enabled");
      #else
        mqtt = nullptr;
        Serial.println("MQTT client disabled (MQTT_ENABLED = false)");
      #endif
    #else
      mqtt = nullptr;
      Serial.println("MQTT client disabled (MQTT_ENABLED not defined)");
    #endif
  #endif
  
  comfo = new comfoair::ComfoAir();
  ota = new comfoair::OTA();
  timeMgr = new comfoair::TimeManager();
  
  sensorData = new comfoair::SensorDataManager();
  filterData = new comfoair::FilterDataManager();
  controlMgr = new comfoair::ControlManager();
  errorData = new comfoair::ErrorDataManager();  // ← NEW: Create error data manager
  screenMgr = new comfoair::ScreenManager();  // NEW: Screen manager
  
  // Link managers to CAN handler
  comfo->setSensorDataManager(sensorData);
  comfo->setFilterDataManager(filterData);
  comfo->setControlManager(controlMgr);
  comfo->setErrorDataManager(errorData);  // ← NEW: Link error data manager
  
  // ============================================================================
  // TIME MANAGER LINKING
  // ============================================================================
  #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
    // In remote client mode: NTP only, no device time sync
    // TimeManager is still created and used for NTP time display
    // But we don't link it to ComfoAir (no CAN time sync)
    Serial.println("TimeManager: NTP mode only (no device sync in Remote Client Mode)");
  #else
    // In normal mode: NTP + device time sync via CAN
    // CRITICAL: Link TimeManager to ComfoAir for time sync
    timeMgr->setComfoAir(comfo);
    comfo->setTimeManager(timeMgr);
  #endif
  
  // Link ComfoAir to control manager (for sending commands)
  controlMgr->setComfoAir(comfo);
  
  // Link MQTT to control manager for remote client mode
  #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
    if (mqtt) {
      controlMgr->setMQTT(mqtt);
      Serial.println("ControlManager: MQTT linked for remote command sending ✓");
    }
  #endif
  
  // Initialize managers
  sensorData->setup();
  filterData->setup();
  controlMgr->setup();
  errorData->setup();  // ← NEW: Initialize error data manager
  

  // NEW: Initialize screen manager with backlight control AND TCA write function for dimming
  screenMgr->begin(control_backlight, tca_write);

 
  
  // Start WiFi connection (non-blocking, max 20 sec)
  wifi->setup();
  
  // Only initialize network-dependent services if WiFi connected
  if (wifi->isConnected()) {
    // MQTT setup (required in remote client mode, optional otherwise)
    if (mqtt) {
      mqtt->setup();
      Serial.println("MQTT ready");
      
      // ========================================================================
      // MQTT DATA SUBSCRIPTIONS (Remote Client Mode)
      // ========================================================================
      #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
        Serial.println("Setting up MQTT subscriptions for sensor data...");
        
        // Subscribe to sensor data from bridge
        mqtt->subscribeTo(MQTT_PREFIX "/extract_air_temp", [](char const * _1, uint8_t const * _2, int _3) {
          if (sensorData) {
            float temp = atof((char*)_2);
            sensorData->updateInsideTemp(temp);
            Serial.printf("MQTT: Inside temp = %.1f°C\n", temp);
          }
        });
        
        mqtt->subscribeTo(MQTT_PREFIX "/outdoor_air_temp", [](char const * _1, uint8_t const * _2, int _3) {
          if (sensorData) {
            float temp = atof((char*)_2);
            sensorData->updateOutsideTemp(temp);
            Serial.printf("MQTT: Outside temp = %.1f°C\n", temp);
          }
        });
        
        mqtt->subscribeTo(MQTT_PREFIX "/extract_air_humidity", [](char const * _1, uint8_t const * _2, int _3) {
          if (sensorData) {
            float humidity = atof((char*)_2);
            sensorData->updateInsideHumidity(humidity);
            Serial.printf("MQTT: Inside humidity = %.1f%%\n", humidity);
          }
        });
        
        mqtt->subscribeTo(MQTT_PREFIX "/outdoor_air_humidity", [](char const * _1, uint8_t const * _2, int _3) {
          if (sensorData) {
            float humidity = atof((char*)_2);
            sensorData->updateOutsideHumidity(humidity);
            Serial.printf("MQTT: Outside humidity = %.1f%%\n", humidity);
          }
        });
        
        mqtt->subscribeTo(MQTT_PREFIX "/remaining_days_filter_replacement", [](char const * _1, uint8_t const * _2, int _3) {
          if (filterData) {
            int days = atoi((char*)_2);
            filterData->updateFilterDays(days);
            Serial.printf("MQTT: Filter days = %d\n", days);
          }
        });
        
        mqtt->subscribeTo(MQTT_PREFIX "/fan_speed", [](char const * _1, uint8_t const * _2, int _3) {
          if (controlMgr) {
            int speed = atoi((char*)_2);
            controlMgr->updateFanSpeedFromCAN(speed);
            Serial.printf("MQTT: Fan speed = %d\n", speed);
          }
        });
        
        mqtt->subscribeTo(MQTT_PREFIX "/temp_profile", [](char const * _1, uint8_t const * _2, int _3) {
          if (controlMgr) {
            uint8_t profile = 0;
            if (strcmp((char*)_2, "cold") == 0) profile = 1;
            else if (strcmp((char*)_2, "warm") == 0) profile = 2;
            controlMgr->updateTempProfileFromCAN(profile);
            Serial.printf("MQTT: Temp profile = %s (%d)\n", (char*)_2, profile);
          }
        });
        
        // Subscribe to error/alarm messages
        mqtt->subscribeTo(MQTT_PREFIX "/error_overheating", [](char const * _1, uint8_t const * _2, int _3) {
          if (errorData) {
            bool active = (strcmp((char*)_2, "ACTIVE") == 0);
            errorData->updateErrorOverheating(active);
          }
        });
        
        mqtt->subscribeTo(MQTT_PREFIX "/alarm_filter", [](char const * _1, uint8_t const * _2, int _3) {
          if (errorData) {
            bool active = (strcmp((char*)_2, "REPLACE") == 0 || strcmp((char*)_2, "ACTIVE") == 0);
            errorData->updateAlarmFilter(active);
          }
        });
        
        Serial.println("✓ MQTT sensor data subscriptions complete");
      #endif
      // ========================================================================
    }
    
    // ============================================================================
    // CAN BUS INITIALIZATION (only in non-remote client mode)
    // ============================================================================
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
      Serial.println("⚠️  CAN bus initialization SKIPPED (Remote Client Mode)");
    #else
      // Initialize CAN bus with MQTT subscriptions if available
      if (mqtt) {
        Serial.println("MQTT ready - initializing CAN bus with MQTT subscriptions");
      } else {
        Serial.println("MQTT disabled - initializing CAN bus without MQTT subscriptions");
      }
      comfo->setup();
    #endif
    
    ota->setup();
    
    // TimeManager setup - always needed for NTP time display
    // In remote client mode: NTP only (no device time sync)
    // In normal mode: NTP + device time sync via CAN
    if (timeMgr) {
      timeMgr->setup();  // This will sync NTP time (and device time if not in remote mode)
    }
    
  } else {
    Serial.println("No WiFi connection");
    
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
      Serial.println("⚠️  CRITICAL: Remote Client Mode requires WiFi connection!");
      Serial.println("⚠️  Device will not function without MQTT connectivity");
    #else
      Serial.println("Initializing CAN bus without MQTT subscriptions");
      comfo->setup();
    #endif
  }

  Serial.println("\n=== System Ready ===");
  Serial.printf("Free heap: %d KB\n", ESP.getFreeHeap() / 1024);
  Serial.printf("Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);

}

// ============================================================================
// MAIN LOOP - OPTIMIZED FOR TOUCH RESPONSIVENESS + NTM
// ============================================================================

void loop() {
  static unsigned long last_touch_read = 0;
  static unsigned long last_can_process = 0;
  
  // ✅ PRIORITY 1: LVGL timer handler (MUST be called frequently)
  lv_timer_handler();
  
  // ✅ PRIORITY 2: Process display refreshes (instant for button presses)
  GUI_process_display_refresh();
  
  // ✅ PRIORITY 3: Touch polling every 5ms (CRITICAL for responsiveness)
  unsigned long now = millis();
  if (now - last_touch_read >= 5) {  // ← 5ms = 200Hz polling
    if (global_touch_indev) {
      lv_indev_read(global_touch_indev);
    }
    last_touch_read = now;
  }
  
  // ✅ NEW: Screen manager loop (handles NTM state transitions)
  if (screenMgr) screenMgr->loop();
  
  // ============================================================================
  // CAN PROCESSING (only in non-remote client mode)
  // ============================================================================
  #if !defined(REMOTE_CLIENT_MODE) || !REMOTE_CLIENT_MODE
    // ✅ PRIORITY 4: CAN processing throttled to 10ms (prevent flooding)
    if (now - last_can_process >= 10) {
      if (comfo) comfo->loop();
      last_can_process = now;
    }
  #endif
  
  // ✅ PRIORITY 5: Manager loops (these handle batched updates)
  if (sensorData) sensorData->loop();      // Batches display updates to 5 sec
  if (filterData) filterData->loop();
  if (controlMgr) controlMgr->loop(); 
  if (errorData) errorData->loop();        // ← NEW: Error data manager loop
  
  // ✅ PRIORITY 6: Network services (lower priority)
  if (wifi) wifi->loop();
  
  // Only process network services if WiFi is connected
  if (wifi && wifi->isConnected()) {
    // MQTT loop (critical in remote client mode)
    if (mqtt) mqtt->loop();
    
    if (ota) ota->loop();
    
    // TimeManager loop - always needed for time display updates
    if (timeMgr) timeMgr->loop();
  }
  
  // Small delay to allow other tasks
  delay(1);


}