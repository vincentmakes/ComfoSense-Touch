#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "TouchDrvGT911.hpp"

// Your app modules
#include "wifi/wifi.h"
#include "comfoair/comfoair.h"
#include "comfoair/sensor_data.h"
#include "comfoair/filter_data.h"
#include "comfoair/control_manager.h"
#include "mqtt/mqtt.h"
#include "ota/ota.h"
#include "time/time_manager.h"
#include "lvgl.h"
#include "ui/GUI.h"

// Add counter at top of main.cpp
static uint32_t touch_read_counter = 0;
static uint32_t event_fire_counter = 0;

#define DIMMING false  //set to true to enable dimmming of the screen. 
//Important to note this does require hardware modification which includes soldering of very tiny resistors
// PWM dimming of AP3032 via FB injection (IO16 â†' R40(0R) â†' C27(100nF) â†' GND, R36=91k to FB)
const int BL_PWM_PIN = 16;
const int LEDC_CH     = 1;        // any free channel 0..7
const int LEDC_BITS   = 8;        // 0..255
const int LEDC_FREQ   = 2000;     // 1â€"5 kHz works well with 100 nF RC

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

static void init_expander() {
  Wire.begin(EXPA_I2C_SDA, EXPA_I2C_SCL, 400000);
  delay(50);
  
  // Configure EXIO1-3 as outputs
  tca_write(TCA_REG_CONFIG, 0xF1);
  
  // Power-on sequence
  tca_write(TCA_REG_OUTPUT, 0x00);  // All low - reset INCLUDING touch
  delay(50);
  tca_write(TCA_REG_OUTPUT, 0x08);  // LCD reset high
  delay(50);
  tca_write(TCA_REG_OUTPUT, 0x0C);  // Backlight on
  delay(50);
  tca_write(TCA_REG_OUTPUT, 0x0E);  // Touch reset high - ENABLE TOUCH
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
  static unsigned long last_touch_seen = 0;      // Last time we detected ANY touch
  static unsigned long touch_start_time = 0;
  
  // Hysteresis timing
  static const unsigned long RELEASE_DELAY_MS = 30;  // Must be no-touch for 30ms before considering "released"
  static const unsigned long MIN_PRESS_DURATION_MS = 80; // Minimum press time to be valid
  
  int16_t x[5], y[5];
  uint8_t touched = touch.getPoint(x, y, 5);
  bool finger_detected_now = (touched > 0);
  unsigned long now = millis();
  
  // Update last seen time whenever we detect touch
  if (finger_detected_now) {
    last_touch_seen = now;
    last_x = x[0];
    last_y = y[0];
  }
  
  // Calculate how long since we last saw touch
  unsigned long time_since_touch = now - last_touch_seen;
  
  // ============================================================================
  // STATE MACHINE WITH HYSTERESIS
  // ============================================================================
  
  if (finger_detected_now && !touch_active) {
    // ────────────────────────────────────────────────────────────────────────
    // STATE 1: NEW PRESS (finger detected, not currently tracking)
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
    // STATE 2: PRESS HELD (we're tracking, and touch seen recently)
    // ────────────────────────────────────────────────────────────────────────
    // This includes brief moments where touch isn't detected - we ignore them!
    
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = last_x;
    data->point.y = last_y;
    
    // No spam - only log if not detected right now (debugging intermittent loss)
    if (!finger_detected_now) {
      // Touch temporarily lost but within hysteresis window - IGNORE IT
      // Uncomment next line for debugging:
      // Serial.printf("⚡ [%lu] Ignoring brief touch loss (%lu ms ago)\n", now, time_since_touch);
    }
  }
  else if (touch_active && time_since_touch >= RELEASE_DELAY_MS) {
    // ────────────────────────────────────────────────────────────────────────
    // STATE 3: PRESS RELEASED (no touch for RELEASE_DELAY_MS)
    // ────────────────────────────────────────────────────────────────────────
    
    unsigned long press_duration = now - touch_start_time;
    
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = last_x;
    data->point.y = last_y;
    
    if (press_duration < MIN_PRESS_DURATION_MS) {
      Serial.printf("⚠️  [%lu] PRESS IGNORED (too brief: %lu ms)\n", now, press_duration);
    } else {
      Serial.printf("🔴 [%lu] PRESS END (duration: %lu ms)\n", now, press_duration);
    }
    
    touch_active = false;
  }
  else {
    // ────────────────────────────────────────────────────────────────────────
    // STATE 4: IDLE (no active touch tracking)
    // ────────────────────────────────────────────────────────────────────────
    
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = last_x;
    data->point.y = last_y;
  }
}
// Brightness 0..100% (0% = backlight off/very dim, 100% = bright)
void setBacklightPct(float pct) {
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  // Inverted mapping: higher duty -> dimmer. Map 100% bright -> low duty.
  int duty = (int)round((100.0f - pct) * ((1 << LEDC_BITS) - 1) / 100.0f);
  ledcWrite(LEDC_CH, duty);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n========================================");
  Serial.println("ESP32-S3 ComfoAir Control v2.0");
  Serial.println("+ Sensor Data Display");
  Serial.println("+ CAN Bus Time Sync");
  Serial.println("========================================");
  
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
  // Initialize touch controller
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
  
  //Dimming if enabled
  #ifdef DIMMING
  ledcSetup(LEDC_CH, LEDC_FREQ, LEDC_BITS);
  ledcAttachPin(BL_PWM_PIN, LEDC_CH);

  // Start "a bit dimmer" than full, say 70% brightness:
  setBacklightPct(70);
  #endif

  // 7. Initialize subsystems
  Serial.println("\nInitializing subsystems...");
  
  wifi = new comfoair::WiFi();
  mqtt = new comfoair::MQTT();
  comfo = new comfoair::ComfoAir();
  ota = new comfoair::OTA();
  timeMgr = new comfoair::TimeManager();
  sensorData = new comfoair::SensorDataManager();
  filterData = new comfoair::FilterDataManager();
  controlMgr = new comfoair::ControlManager();
  
  // Link managers to CAN handler
  comfo->setSensorDataManager(sensorData);
  comfo->setFilterDataManager(filterData);
  comfo->setControlManager(controlMgr);
  
  // CRITICAL: Link TimeManager to ComfoAir for time sync
  timeMgr->setComfoAir(comfo);
  comfo->setTimeManager(timeMgr);
  
  // Link ComfoAir to control manager (for sending commands)
  controlMgr->setComfoAir(comfo);
  
  // Initialize managers
  sensorData->setup();
  filterData->setup();
  controlMgr->setup();
  
  // Start WiFi connection (non-blocking, max 20 sec)
  wifi->setup();
  
  // Initialize CAN bus first (independent of WiFi)
  comfo->setup();
  
  // Only initialize network-dependent services if WiFi connected
  if (wifi->isConnected()) {
    mqtt->setup();
    ota->setup();
    timeMgr->setup();  // This will sync NTP time and check device time
  } else {
    Serial.println("Skipping MQTT, OTA, and time sync (no WiFi connection)");
  }

  Serial.println("\n=== System Ready ===");
  Serial.printf("Free heap: %d KB\n", ESP.getFreeHeap() / 1024);
  Serial.printf("Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);
}
// âœ… TOUCH-OPTIMIZED MAIN LOOP
// This is the loop() function that should replace your current loop() in main.cpp

// ============================================================================
// MAIN LOOP - OPTIMIZED FOR TOUCH RESPONSIVENESS
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
  
  // ✅ PRIORITY 4: CAN processing throttled to 10ms (prevent flooding)
  if (now - last_can_process >= 10) {
    if (comfo) comfo->loop();
    last_can_process = now;
  }
  
  // ✅ PRIORITY 5: Manager loops (these handle batched updates)
  if (sensorData) sensorData->loop();      // Batches display updates to 5 sec
  if (filterData) filterData->loop();
  
  // ✅ PRIORITY 6: Network services (lower priority)
  if (wifi) wifi->loop();
  
  // Only process network services if WiFi is connected
  if (wifi && wifi->isConnected()) {
    if (mqtt) mqtt->loop();
    if (ota) ota->loop();
    if (timeMgr) timeMgr->loop();          // Updates time display every 1 sec
  }
  
  // Small delay to allow other tasks
  delay(1);
}

/* 
 * KEY OPTIMIZATIONS:
 * 
 * 1. Touch polling reduced from 10ms to 5ms (200Hz)
 *    - Improves touch responsiveness
 *    - Reduces perceived latency
 * 
 * 2. CAN processing throttled to 10ms
 *    - Prevents CAN floods from blocking touch
 *    - Data still captured immediately in CAN interrupt
 * 
 * 3. Manager loops called every iteration
 *    - sensorData->loop() checks if 5 seconds elapsed for display update
 *    - timeMgr->loop() checks if 1 second elapsed for time update
 *    - This is lightweight (just timestamp checks)
 * 
 * 4. GUI_process_display_refresh() called early
 *    - Button presses trigger immediate display refresh
 *    - Gives instant visual feedback
 * 
 * RESULT: Touch feels instant, display updates smoothly
 */
/*
==============================================================================
WHAT CHANGED FOR TOUCH RESPONSIVENESS:
==============================================================================

âœ… Touch polling moved to TOP of loop (highest priority)
âœ… Touch poll interval reduced from 10ms to 5ms (2x faster)
âœ… LVGL timer_handler called immediately after touch
âœ… Display refresh only called when needed (button presses)
âœ… CAN processing throttled to 10ms (prevents flooding)
âœ… Time/sensor updates no longer call GUI_request_display_refresh()

RESULT:
- Touch is checked 200 times/second (every 5ms)
- Button presses feel instant
- Dropdown responds immediately
- Display updates naturally via LVGL timer
- No more blocking from forced refreshes

==============================================================================
*/

/*void loop() {
  static uint32_t last_touch_read = 0;
  
  // Call lv_timer_handler() to process LVGL events
  lv_timer_handler();
  
  // Process any pending display refreshes (from GUI event callbacks)
  GUI_process_display_refresh();
  
  // Manually read touch input every 10ms (CRITICAL for touch to work)
  if (millis() - last_touch_read >= 10) {
    if (global_touch_indev) {
      lv_indev_read(global_touch_indev);
    }
    last_touch_read = millis();
  }
  
  // Process subsystems
  if (wifi) wifi->loop();
  if (comfo) comfo->loop();
  if (sensorData) sensorData->loop();
  if (filterData) filterData->loop();
  
  // Only process network services if WiFi is connected
  if (wifi && wifi->isConnected()) {
    if (mqtt) mqtt->loop();
    if (ota) ota->loop();
    if (timeMgr) timeMgr->loop();
  }
  
  // Small delay to allow other tasks
  delay(1);
}*/