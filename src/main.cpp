#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "TouchDrvGT911.hpp"

// Your app modules
#include "wifi/wifi.h"
#include "comfoair/comfoair.h"
#include "mqtt/mqtt.h"
#include "ota/ota.h"
#include "time/time_manager.h"
#include "lvgl.h"
#include "ui/GUI.h"

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

// LVGL touch read callback
static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  static int16_t last_x = 0;
  static int16_t last_y = 0;
  
  int16_t x[5], y[5];
  uint8_t touched = touch.getPoint(x, y, 5);
  
  if (touched > 0) {
    data->state = LV_INDEV_STATE_PRESSED;
    last_x = x[0];
    last_y = y[0];
    data->point.x = last_x;
    data->point.y = last_y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
    data->point.x = last_x;
    data->point.y = last_y;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println("\n========================================");
  Serial.println("ESP32-S3 ComfoAir Control");
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
  Serial.println("Initializing touch...");
  
  pinMode(TOUCH_INT, INPUT);
  touch.setPins(-1, TOUCH_INT);
  
  if (touch.begin(Wire, GT911_SLAVE_ADDRESS_L, TOUCH_SDA, TOUCH_SCL)) {
    Serial.println("Touch initialized successfully");
    touch.setMaxTouchPoint(5);
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
  mqtt = new comfoair::MQTT();
  comfo = new comfoair::ComfoAir();
  ota = new comfoair::OTA();
  timeMgr = new comfoair::TimeManager();
  
  wifi->setup();
  mqtt->setup();
  comfo->setup();
  ota->setup();
  timeMgr->setup();  // This will sync time and do initial display update
  
  Serial.println("\n=== System Ready ===");
  Serial.printf("Free heap: %d KB\n", ESP.getFreeHeap() / 1024);
  Serial.printf("Free PSRAM: %d KB\n", ESP.getFreePsram() / 1024);
}

void loop() {
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
  if (mqtt) mqtt->loop();
  if (comfo) comfo->loop();
  if (ota) ota->loop();
  if (timeMgr) timeMgr->loop();  // This updates time display every second
  
  // Small delay to allow other tasks
  delay(1);
}