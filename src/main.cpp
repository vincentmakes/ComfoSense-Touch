#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "TouchDrvGT911.hpp"

#include "serial_logger.h"
#define Serial LogSerial 

// ============================================================================
// CRITICAL: Board detection MUST come first!
// ============================================================================
#include "board_config.h"  // Auto-detects Touch LCD vs RS485-CAN board

// Configuration
#include "secrets.h"  // CRITICAL: Must include for MQTT_ENABLED and NTM_* defines

// Your app modules
#include "wifi/wifi.h"
#include "comfoair/comfoair.h"
#include "comfoair/sensor_data.h"
#include "comfoair/filter_data.h"
#include "comfoair/control_manager.h"
#include "comfoair/error_data.h"
#include "screen/screen_manager.h"
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

// ============================================================================
// PIN DEFINITIONS - Now handled by board_config.h!
// The macros EXPA_I2C_SDA, EXPA_I2C_SCL, etc. are automatically configured
// based on detected board type. No more hardcoded pins!
// ============================================================================

// GT911 touch controller
TouchDrvGT911 touch;

// ============================================================================
// ============================================================================
// DISPLAY OBJECTS - Only created/initialized on Touch LCD board
// CRITICAL: These objects are NULL pointers on RS485-CAN board to prevent
// GPIO17/18/21 from being initialized by RGB panel constructor, as those
// pins are connected to RS485 transceiver hardware and must stay LOW.
// ============================================================================
Arduino_DataBus *bus = nullptr;
Arduino_ESP32RGBPanel *rgbpanel = nullptr;
Arduino_RGB_Display *gfx = nullptr;

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
comfoair::ErrorDataManager *errorData = nullptr;
comfoair::ScreenManager *screenMgr = nullptr;

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
  // Use board-detected I2C pins (EXPA_I2C_SDA and EXPA_I2C_SCL are auto-configured)
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
    // NEW PRESS - Report immediately!
    
    touch_active = true;
    touch_start_time = now;
    
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = last_x;
    data->point.y = last_y;
    
    touch_read_counter++;
    return;
  }
  
  if (touch_active && time_since_touch < RELEASE_DELAY_MS) {
    // STILL PRESSED (or recently pressed) - Continue reporting press
    
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = last_x;
    data->point.y = last_y;
    return;
  }
  
  if (touch_active && time_since_touch >= RELEASE_DELAY_MS) {
    // RELEASE CONFIRMED - Finger definitely gone
    
    unsigned long press_duration = now - touch_start_time;
    
    // Only count as valid press if held long enough (debouncing)
    if (press_duration >= MIN_PRESS_DURATION_MS) {
      event_fire_counter++;
    }
    
    touch_active = false;
    
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = last_x;
    data->point.y = last_y;
    return;
  }
  
  // DEFAULT: Not pressed
  data->state = LV_INDEV_STATE_RELEASED;
  data->point.x = last_x;
  data->point.y = last_y;
}

// ============================================================================
// SETUP - WITH AUTO BOARD DETECTION!
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║   ComfoSense-Touch - Starting...      ║");
  Serial.println("╚════════════════════════════════════════╝");
  Serial.println("");
  
  // ========================================================================
  // STEP 1: AUTO-DETECT BOARD TYPE - MUST BE FIRST!
  // This configures ALL pins (I2C, CAN, Touch) based on detected hardware
  // ========================================================================
  initBoardConfig();
  
  // ========================================================================
  // STEP 2: CONDITIONAL DISPLAY INITIALIZATION
  // Only initialize display hardware if we're on Touch LCD board
  // ========================================================================
  
  if (hasDisplay()) {
    Serial.println("\n🖥️  DISPLAY INITIALIZATION (Touch LCD Board)");
    Serial.println("════════════════════════════════════════");
    
    // Initialize I2C expander for backlight/reset control
    init_expander();
    
    
    // Create display objects NOW (after board detection, before display init)
    // This ensures GPIO17/18/21 are only configured on Touch LCD board
    Serial.println("Creating RGB panel objects...");
    bus = new Arduino_SWSPI(
        GFX_NOT_DEFINED /* DC */, 42 /* CS */,
        2 /* SCK */, 1 /* MOSI */, GFX_NOT_DEFINED /* MISO */);
    
    rgbpanel = new Arduino_ESP32RGBPanel(
        40 /* DE */, 39 /* VSYNC */, 38 /* HSYNC */, 41 /* PCLK */,
        46 /* R0 */, 3 /* R1 */, 8 /* R2 */, 18 /* R3 */, 17 /* R4 */,
        14 /* G0 */, 13 /* G1 */, 12 /* G2 */, 11 /* G3 */, 10 /* G4 */, 9 /* G5 */,
        5 /* B0 */, 45 /* B1 */, 48 /* B2 */, 47 /* B3 */, 21 /* B4 */,
        1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
        1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);
    
    gfx = new Arduino_RGB_Display(
        480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
        bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));
    Serial.println("✅ RGB panel objects created");
    
    // Initialize display
    gfx->begin();
    gfx->fillScreen(BLACK);
    Serial.println("✅ Display initialized");
    
    // Initialize touch controller with board-detected pins
    touch.setPins(-1, TOUCH_RST);  // TOUCH_RST is auto-configured
    if (!touch.begin(Wire, GT911_SLAVE_ADDRESS_L, TOUCH_SDA, TOUCH_SCL)) {
      Serial.println("❌ GT911 touch initialization failed!");
      while (1) delay(1000);
    }
    Serial.println("✅ GT911 touch controller initialized");
    
    // Allocate LVGL buffers in PSRAM
    lv_buf1 = (lv_color_t *)heap_caps_malloc(lv_buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    lv_buf2 = (lv_color_t *)heap_caps_malloc(lv_buf_pixels * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    
    if (!lv_buf1 || !lv_buf2) {
      Serial.println("❌ Failed to allocate LVGL buffers in PSRAM!");
      while (1) delay(1000);
    }
    Serial.println("✅ LVGL buffers allocated in PSRAM");
    
    // Initialize LVGL
    lv_init();
    
    // Create LVGL display
    lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);
    lv_display_set_buffers(disp, lv_buf1, lv_buf2, lv_buf_pixels * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // Create LVGL touch input device
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, lvgl_touch_read_cb);
    global_touch_indev = indev;
    
    Serial.println("✅ LVGL initialized");
    
    // Initialize GUI
    GUI_init();
    Serial.println("✅ GUI loaded");
    
  } else {
    Serial.println("\n⊘  NO DISPLAY (RS485-CAN Board - Bridge Mode)");
    Serial.println("════════════════════════════════════════");
    Serial.println("Running in headless mode - display initialization skipped");
    
    // CRITICAL: Re-initialize I2C on correct pins for RS485-CAN board
    // The board detection left I2C in Wire.end() state, which can interfere with WiFi
    Serial.println("Re-initializing I2C on RS485-CAN pins...");
    Wire.begin(EXPA_I2C_SDA, EXPA_I2C_SCL, 400000);
    Serial.printf("✅ I2C re-initialized: SDA=GPIO%d, SCL=GPIO%d\n", EXPA_I2C_SDA, EXPA_I2C_SCL);
    Serial.println("");
  }
  
  // ========================================================================
  // STEP 3: INITIALIZE SUBSYSTEMS (common to both boards)
  // ========================================================================
  
  Serial.println("\n🔧 SUBSYSTEM INITIALIZATION");
  Serial.println("════════════════════════════════════════");
  
  // Create subsystem instances
  comfo = new comfoair::ComfoAir();
  wifi = new comfoair::WiFi();
  ota = new comfoair::OTA();
  timeMgr = new comfoair::TimeManager();
  sensorData = new comfoair::SensorDataManager();
  filterData = new comfoair::FilterDataManager();
  controlMgr = new comfoair::ControlManager();
  errorData = new comfoair::ErrorDataManager();
  screenMgr = new comfoair::ScreenManager();
  
  // Initialize MQTT if enabled
  #if MQTT_ENABLED
    mqtt = new comfoair::MQTT();
  #endif
  
  // ========================================================================
  // TIME MANAGER CONFIGURATION (Remote Client vs Normal Mode)
  // ========================================================================
  #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
    // In remote client mode: NTP only (no device time sync via CAN)
    Serial.println("⚠️  REMOTE CLIENT MODE: NTP time only (no CAN time sync)");
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
      Serial.println("ControlManager: MQTT linked for remote command sending");
    }
  #endif
  
  // Initialize managers
  // Note: In headless bridge mode, we still need sensorData for MQTT data storage,
  // but it must not call any LVGL display functions
  
  #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
    // Remote client mode: Needs sensorData/filterData for MQTT data, but no display
    Serial.println("📊 Initializing data managers for MQTT (no display updates)");
    // TODO: These managers need a hasDisplay() check in their code to skip LVGL calls
    // For now, skip them to prevent crashes - data will come via MQTT directly
    Serial.println("⚠️  Manager setup skipped - will receive data via MQTT subscriptions");
  #else
    // Normal mode: Full manager initialization with display updates
    if (hasDisplay()) {
      sensorData->setup();
      filterData->setup();
      errorData->setup();
      Serial.println("✅ Display-dependent managers initialized");
    } else {
      Serial.println("⚠️  Display-dependent managers skipped (headless mode)");
    }
  #endif
  
  // Control manager works in both modes (sends commands, doesn't update display directly)
  controlMgr->setup();
  
  // NEW: Initialize screen manager with backlight control (only if display present)
  if (hasDisplay()) {
    screenMgr->begin(control_backlight, tca_write);
  } else {
    // Still initialize screen manager but with null callbacks (no hardware control)
    screenMgr->begin(nullptr, nullptr);
  }
  
  // Start WiFi connection (non-blocking, max 20 sec)
  Serial.println("\n📡 STARTING WIFI CONNECTION");
  #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
    Serial.println("   Mode: REMOTE_CLIENT_MODE (Bridge)");
  #else
    Serial.println("   Mode: NORMAL_MODE");
  #endif
  
  // Small delay to let peripherals settle after I2C init
  // Critical for reliability - I2C and WiFi share system resources
  delay(200);  // 200ms ensures stable peripheral state
  
  Serial.println("   Calling wifi->setup()...");
  wifi->setup();
  Serial.printf("   WiFi setup complete. Connected: %s\n", wifi->isConnected() ? "YES" : "NO");
  Serial.println("");
  
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
        
        Serial.println("MQTT sensor data subscriptions complete");
      #endif
      // ========================================================================
    }
    
    // ============================================================================
    // CAN BUS INITIALIZATION (only in non-remote client mode)
    // ============================================================================
    #if defined(REMOTE_CLIENT_MODE) && REMOTE_CLIENT_MODE
      Serial.println("CAN bus initialization SKIPPED (Remote Client Mode)");
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
      Serial.println("CRITICAL: Remote Client Mode requires WiFi connection!");
      Serial.println("Device will not function without MQTT connectivity");
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
  
  // ============================================================================
  // CONDITIONAL DISPLAY UPDATES (only on Touch LCD board)
  // ============================================================================
  if (hasDisplay()) {
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
  }
  
  // ============================================================================
  // CAN PROCESSING (only in non-remote client mode)
  // ============================================================================
  #if !defined(REMOTE_CLIENT_MODE) || !REMOTE_CLIENT_MODE
    // ✅ PRIORITY 4: CAN processing throttled to 10ms (prevent flooding)
    unsigned long now = millis();
    if (now - last_can_process >= 10) {
      if (comfo) comfo->loop();
      last_can_process = now;
    }
  #endif
  
  // ✅ PRIORITY 5: Manager loops (these handle batched updates)
  // Display-dependent managers only run if display exists
  if (hasDisplay()) {
    if (sensorData) sensorData->loop();      // Batches display updates to 5 sec
    if (filterData) filterData->loop();
    if (errorData) errorData->loop();
  }
  if (controlMgr) controlMgr->loop();  // Works in both modes
  
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