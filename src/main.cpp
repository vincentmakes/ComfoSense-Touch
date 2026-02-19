#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "TouchDrvGT911.hpp"

#include "serial_logger.h"
#define Serial LogSerial 

// ============================================================================
// CRITICAL: Board detection MUST come first!
// ============================================================================
#include "board_config.h"  // Auto-detects Touch LCD V3/V4 vs RS485-CAN board

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

// Track current IO expander GPIO output state
// V3: maps to TCA9554 output register 0x01 (default 0x0E = backlight+LCD+touch on)
// V4: maps to CH32V003 output register 0x03 (default 0xFF = all high)
static uint8_t current_io_output_state = 0x0E;

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
static bool touch_initialized = false;

// ============================================================================
// IO EXPANDER ABSTRACTION
// Routes writes to correct I2C address based on detected board version
// ============================================================================
static void io_expander_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(getIOExpanderAddr());
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

// ============================================================================
// BACKLIGHT CONTROL — version-aware
// V3: Toggle BL_EN bit (bit 1) in TCA9554 output register
// V4: Write PWM duty to CH32V003 register 0x05
// INVERTED: 0=full brightness, 247=off (PWM pulls AP3032 FB low)
// ~80% brightness = 247 - (80*247/100) = 247 - 198 = 49
#define V4_PWM_DEFAULT_DUTY  49   // ~80% brightness (inverted scale)
// ============================================================================
static void control_backlight(bool on) {
  if (isTouchLCDv3()) {
    // V3: Toggle EXIO2/BL_EN (bit 1) in TCA9554 output register
    if (on) {
      current_io_output_state |= (1 << V3_BIT_BL_EN);
      Serial.println("[Backlight V3] ON");
    } else {
      current_io_output_state &= ~(1 << V3_BIT_BL_EN);
      Serial.println("[Backlight V3] OFF");
    }
    io_expander_write(TCA9554_REG_OUTPUT, current_io_output_state);
    
  } else if (isTouchLCDv4()) {
    // V4: Backlight is controlled via dedicated PWM register (0x05)
    // NOT a bit in the GPIO output register!
    if (on) {
      io_expander_write(CH32V003_REG_PWM, V4_PWM_DEFAULT_DUTY);  // Restore default brightness
      Serial.printf("[Backlight V4] ON (PWM=%d, inverted)\n", V4_PWM_DEFAULT_DUTY);
    } else {
      io_expander_write(CH32V003_REG_PWM, CH32V003_PWM_MAX);  // Max duty = backlight off (inverted)
      Serial.println("[Backlight V4] OFF (PWM=0)");
    }
  }
}

// ============================================================================
// IO EXPANDER INITIALIZATION — version-aware
// V3: TCA9554 reset sequence with staged power-on
// V4: CH32V003 direction config + all outputs high + PWM init
// ============================================================================
static void init_expander() {
  // Use board-detected I2C pins (EXPA_I2C_SDA and EXPA_I2C_SCL are auto-configured)
  Wire.begin(EXPA_I2C_SDA, EXPA_I2C_SCL, 400000);
  delay(50);
  
  if (isTouchLCDv3()) {
    // ---- V3: TCA9554 at 0x20 ----
    // Configure EXIO1-5 as outputs
    // Bit pattern: 1=input, 0=output
    // 0xE1 = 0b11100001 = EXIO1,2,3,5 as outputs, EXIO4,6,7,8 as inputs
    io_expander_write(TCA9554_REG_CONFIG, 0xE1);
    
    // Power-on sequence (same as before — proven working on V3)
    io_expander_write(TCA9554_REG_OUTPUT, 0x00);  // All low - reset INCLUDING touch
    delay(50);
    io_expander_write(TCA9554_REG_OUTPUT, 0x08);   // LCD reset high (bit 2... wait)
    delay(50);
    io_expander_write(TCA9554_REG_OUTPUT, 0x0C);   // + Backlight on
    current_io_output_state = 0x0C;  // Track state
    delay(50);
    io_expander_write(TCA9554_REG_OUTPUT, 0x0E);   // + Touch reset high - ENABLE TOUCH
    current_io_output_state = 0x0E;  // Track state
    delay(150);  // Delay for GT911 to initialize
    
    Serial.println("  IO Expander: TCA9554 @ 0x20 (V3)");
    Serial.printf("  Output state: 0x%02X\n", current_io_output_state);
    
  } else if (isTouchLCDv4()) {
    // ---- V4: CH32V003 at 0x24 ----
    // MUST match factory demo sequence: OUTPUT first, then DIRECTION
    // This latches output values before enabling pins as outputs,
    // ensuring TP_RST goes HIGH (releasing GT911) at the exact moment
    // the direction register enables it.
    
    // Step 1: Set output state FIRST — all HIGH except buzzer (BEE_EN LOW)
    // Setting BEE_EN LOW before direction register enables it as output
    // prevents any buzzer activation during init
    io_expander_write(CH32V003_REG_OUTPUT, 0xBF);  // 0xFF with bit 6 (BEE) OFF
    current_io_output_state = 0xBF;
    
    // Step 2: THEN set direction — which pins are outputs
    // 0x7A = EXIO1(TP_RST), EXIO3(LCD_RST), EXIO4(SDCS), EXIO5(SYS_EN), EXIO6(BEE) as outputs
    io_expander_write(CH32V003_REG_DIR, CH32V003_DIR_MASK);
    
    // Step 3: Set initial backlight brightness via hardware PWM
    io_expander_write(CH32V003_REG_PWM, V4_PWM_DEFAULT_DUTY);  // ~80% brightness (inverted)
    delay(150);  // GT911 boot time after TP_RST release
    
    Serial.println("  IO Expander: CH32V003 @ 0x24 (V4)");
    Serial.printf("  Output: 0x%02X → Direction: 0x%02X (factory sequence)\n", 
                  current_io_output_state, CH32V003_DIR_MASK);
    Serial.printf("  Backlight: Hardware PWM initialized (duty=%d, inverted)\n", V4_PWM_DEFAULT_DUTY);
  }
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
    // LVGL 9.1.0's lv_display_set_rotation rotates rendering output but does NOT
    // auto-transform touch input coordinates. Manual flip needed for V4 (180°).
    if (getDisplayRotation() == 2) {
      last_x = (SCREEN_WIDTH - 1) - x[0];
      last_y = (SCREEN_HEIGHT - 1) - y[0];
    } else {
      last_x = x[0];
      last_y = y[0];
    }
    
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
  // ========================================================================
  // EMERGENCY: Silence CH32V003 buzzer IMMEDIATELY on power-up!
  // The CH32V003 powers up with all outputs HIGH, including BEE_EN (bit 6),
  // which drives the buzzer. The factory firmware silences it instantly because
  // setup() goes straight to I2C writes. We must do the same — before Serial,
  // before delay, before ANYTHING. This adds ~5ms at boot.
  // ========================================================================
  Wire.begin(15, 7);  // Touch LCD I2C pins — same as factory
  Wire.beginTransmission(0x24);  // CH32V003 address
  Wire.write(0x02);              // Output register
  Wire.write(0xBF);              // All HIGH except bit 6 (buzzer OFF)
  Wire.endTransmission();
  Wire.beginTransmission(0x24);
  Wire.write(0x03);              // Direction register
  Wire.write(0x3A);              // Factory mask — BEE_EN as input (can't buzz)
  Wire.endTransmission();
  Wire.end();
  // If this board isn't V4, these writes are harmless (no device at 0x24).
  // ========================================================================
  
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
  // Only initialize display hardware if we're on Touch LCD board (V3 or V4)
  // ========================================================================
  
  if (hasDisplay()) {
    Serial.println("\n🖥️  DISPLAY INITIALIZATION (Touch LCD Board)");
    Serial.println("════════════════════════════════════════");
    
    if (isTouchLCDv4()) {
      // ================================================================
      // V4: MUST match factory init sequence EXACTLY
      // Factory order: CH32V003 init → touch init → display init
      // Critical: Touch BEFORE display. gfx->begin() may interfere.
      // ================================================================
      
      // Step 1: CH32V003 init — match factory registers exactly
      // Factory uses 100kHz, writes output THEN direction, mask 0x3A
      Wire.begin(EXPA_I2C_SDA, EXPA_I2C_SCL);  // 100kHz default like factory
      delay(50);
      
      // Factory sequence: reg 0x02 = 0xFF (output), reg 0x03 = 0x3A (direction)
      // BUT: We set bit 6 (BEE_EN) LOW from the start to prevent buzzer activation
      // when direction later changes to 0x7A (making BEE_EN an output)
      io_expander_write(CH32V003_REG_OUTPUT, 0xBF);  // 0xFF with bit 6 (buzzer) OFF
      current_io_output_state = 0xBF;
      io_expander_write(CH32V003_REG_DIR, 0x3A);  // Factory mask, NOT 0x7A
      
      Serial.println("  IO Expander: CH32V003 @ 0x24 (V4)");
      Serial.printf("  Output: 0xBF → Direction: 0x3A (factory + buzzer off)\n");
      
      // Step 2: Touch init — BEFORE display, matching factory
      // Factory does: Wire.begin → delay(100) → i2c_scan → setPins(-1,-1) → begin
      Wire.begin(EXPA_I2C_SDA, EXPA_I2C_SCL);  // Re-init like factory does
      delay(100);  // Factory delay
      
      Serial.println("  [V4] I2C bus scan:");
      uint8_t gt911_found_addr = 0;
      for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
          Serial.printf("    0x%02X — %s\n", addr,
            addr == 0x24 ? "CH32V003" :
            addr == 0x51 ? "PCF8563 RTC" :
            addr == 0x5D ? "GT911 (0x5D)" :
            addr == 0x14 ? "GT911 (0x14)" :
            "unknown");
          if (addr == 0x5D || addr == 0x14) gt911_found_addr = addr;
        }
      }
      
      touch.setPins(-1, -1);
      
      bool touch_ok = false;
      if (gt911_found_addr) {
        Serial.printf("  GT911 found at 0x%02X — attempting init...\n", gt911_found_addr);
        touch_ok = touch.begin(Wire, gt911_found_addr, TOUCH_SDA, TOUCH_SCL);
      }
      if (!touch_ok) {
        touch_ok = touch.begin(Wire, GT911_SLAVE_ADDRESS_L, TOUCH_SDA, TOUCH_SCL);
      }
      if (!touch_ok) {
        Serial.println("  GT911 not at 0x5D, trying 0x14...");
        touch_ok = touch.begin(Wire, GT911_I2C_ADDR_H, TOUCH_SDA, TOUCH_SCL);
      }
      
      if (touch_ok) {
        Serial.println("✅ GT911 touch controller initialized (V4)");
        touch_initialized = true;
        
        // Read current GT911 config
        uint8_t gt_a = gt911_found_addr ? gt911_found_addr : 0x5D;
        uint8_t cfg_header[6];
        Wire.beginTransmission(gt_a);
        Wire.write(0x80); Wire.write(0x47);
        Wire.endTransmission(false);
        Wire.requestFrom(gt_a, (uint8_t)6);
        for (int i = 0; i < 6 && Wire.available(); i++) cfg_header[i] = Wire.read();
        uint16_t rx = cfg_header[1]|(cfg_header[2]<<8), ry = cfg_header[3]|(cfg_header[4]<<8);
        Serial.printf("  Config: ver=0x%02X, X=%d, Y=%d, touches=%d\n", 
                      cfg_header[0], rx, ry, cfg_header[5]);
        
        // Manual reloadConfig: replicate what setMaxTouchPoint()+reloadConfig() does
        // without using SensorLib (which crashes due to stale I2C handle from Wire.end())
        // Read entire 186-byte config, patch Touch_Number + resolution, write back.
        bool need_config_update = (rx == 0 || ry == 0 || cfg_header[5] == 0);
        
        if (need_config_update) {
          Serial.println("  Config needs update — performing manual reloadConfig...");
          
          // Read full config (186 bytes: 0x8047 to 0x8100)
          uint8_t full_cfg[186];
          memset(full_cfg, 0, sizeof(full_cfg));
          for (uint16_t offset = 0; offset < 184; offset += 28) {
            uint8_t chunk = min((uint16_t)28, (uint16_t)(184 - offset));
            uint16_t reg = 0x8047 + offset;
            Wire.beginTransmission(gt_a);
            Wire.write((uint8_t)(reg >> 8));
            Wire.write((uint8_t)(reg & 0xFF));
            Wire.endTransmission(false);
            Wire.requestFrom(gt_a, chunk);
            for (uint8_t i = 0; i < chunk && Wire.available(); i++) {
              full_cfg[offset + i] = Wire.read();
            }
          }
          
          Serial.printf("  Raw config[0-9]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X\n",
                        full_cfg[0],full_cfg[1],full_cfg[2],full_cfg[3],full_cfg[4],
                        full_cfg[5],full_cfg[6],full_cfg[7],full_cfg[8],full_cfg[9]);
          
          // Patch config: set resolution and touch count
          // Offset 0 = Config_Version (0x8047)
          full_cfg[0] = cfg_header[0] + 1;  // Increment version so GT911 accepts it
          if (full_cfg[0] == 0) full_cfg[0] = 1;
          // Offset 1-2 = X_Output_Max (0x8048-49, little-endian)
          full_cfg[1] = 0xE0; full_cfg[2] = 0x01;  // 480
          // Offset 3-4 = Y_Output_Max (0x804A-4B, little-endian)
          full_cfg[3] = 0xE0; full_cfg[4] = 0x01;  // 480
          // Offset 5 = Touch_Number (0x804C)
          full_cfg[5] = 5;
          // Offset 6 = Module_Switch1 (0x804D) — set INT trigger mode
          if (full_cfg[6] == 0) full_cfg[6] = 0x0D;  // X2Y | INT rising
          
          // Calculate checksum over bytes 0-183 (config area 0x8047-0x80FE)
          uint8_t checksum = 0;
          for (int i = 0; i < 184; i++) checksum += full_cfg[i];
          checksum = (~checksum) + 1;
          full_cfg[184] = checksum;   // 0x80FF
          full_cfg[185] = 0x01;       // 0x8100 config_fresh flag
          
          // Write full config back in chunks
          for (uint16_t offset = 0; offset < 186; offset += 28) {
            uint8_t chunk = min((uint16_t)28, (uint16_t)(186 - offset));
            uint16_t reg = 0x8047 + offset;
            Wire.beginTransmission(gt_a);
            Wire.write((uint8_t)(reg >> 8));
            Wire.write((uint8_t)(reg & 0xFF));
            Wire.write(&full_cfg[offset], chunk);
            Wire.endTransmission();
          }
          
          delay(100);  // GT911 processes config
          
          // Verify config was applied
          Wire.beginTransmission(gt_a);
          Wire.write(0x80); Wire.write(0x47);
          Wire.endTransmission(false);
          Wire.requestFrom(gt_a, (uint8_t)6);
          for (int i = 0; i < 6 && Wire.available(); i++) cfg_header[i] = Wire.read();
          rx = cfg_header[1]|(cfg_header[2]<<8); ry = cfg_header[3]|(cfg_header[4]<<8);
          Serial.printf("  After update: ver=0x%02X, X=%d, Y=%d, touches=%d, cksum=0x%02X\n", 
                        cfg_header[0], rx, ry, cfg_header[5], checksum);
        }
        
        // Touch test — 3 seconds with detailed logging
        // First clear any stale status from begin()
        Wire.beginTransmission(gt_a);
        Wire.write(0x81); Wire.write(0x4E); Wire.write(0x00);
        Wire.endTransmission();
        delay(50);  // Let GT911 process
        
        Serial.println("  Touch test (3s) — tap the screen now...");
        unsigned long test_start = millis();
        bool got_touch = false;
        uint8_t last_status = 0xFF;
        int reads = 0;
        while (millis() - test_start < 3000) {
          Wire.beginTransmission(gt_a);
          Wire.write(0x81); Wire.write(0x4E);
          Wire.endTransmission(false);
          Wire.requestFrom(gt_a, (uint8_t)1);
          uint8_t status = Wire.available() ? Wire.read() : 0;
          reads++;
          if (status != last_status) {
            Serial.printf("  Status 0x814E = 0x%02X (read #%d, t=%lums)\n", 
                          status, reads, millis() - test_start);
            last_status = status;
          }
          if ((status & 0x80) && (status & 0x0F) > 0) {
            uint8_t num = status & 0x0F;
            Wire.beginTransmission(gt_a);
            Wire.write(0x81); Wire.write(0x4F);
            Wire.endTransmission(false);
            uint8_t tp[6];
            Wire.requestFrom(gt_a, (uint8_t)6);
            for (int i = 0; i < 6 && Wire.available(); i++) tp[i] = Wire.read();
            Serial.printf("  ✅ TOUCH! n=%d, X=%d, Y=%d\n", 
                          num, tp[1]|(tp[2]<<8), tp[3]|(tp[4]<<8));
            got_touch = true;
          }
          // Always clear status
          Wire.beginTransmission(gt_a);
          Wire.write(0x81); Wire.write(0x4E); Wire.write(0x00);
          Wire.endTransmission();
          if (got_touch) break;
          delay(20);
        }
        Serial.printf("  Test: %d reads, touch=%s\n", reads, got_touch ? "YES" : "NO");
      } else {
        Serial.println("⚠️  GT911 touch init failed (V4) — continuing without touch");
      }
      
      // Step 3: NOW init display (after touch is working)
      // Need 400kHz for display SPI init
      Wire.begin(EXPA_I2C_SDA, EXPA_I2C_SCL, 400000);
      
      // Set backlight and buzzer NOW (after touch init is safe)
      io_expander_write(CH32V003_REG_PWM, V4_PWM_DEFAULT_DUTY);  // Backlight on (inverted)
      // CRITICAL: Ensure buzzer bit is LOW in output BEFORE changing direction
      // to include BEE_EN as output — prevents any buzzer pulse
      current_io_output_state &= ~(1 << V4_BIT_BEE_EN);  // Buzzer off in output first
      io_expander_write(CH32V003_REG_OUTPUT, current_io_output_state);
      // Now safe to make BEE_EN an output pin
      io_expander_write(CH32V003_REG_DIR, CH32V003_DIR_MASK);  // 0x7A
      
      Serial.println("Creating RGB panel objects...");
      
    } else {
      // V3: Original sequence — init_expander then display
      init_expander();
    
      Serial.println("Creating RGB panel objects...");
    }
    
    // Create display objects (common for V3 and V4)
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
    
    // CRITICAL: Always use rotation=0 for Arduino_RGB_Display!
    // Software rotation in the RGB driver causes artifacts with LVGL partial rendering
    // (dark lines over changing elements, remnant lines after closing menus).
    // Instead, rotation is handled by LVGL's display rotation which works correctly
    // with partial rendering mode.
    gfx = new Arduino_RGB_Display(
        480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation - ALWAYS 0 */, true /* auto_flush */,
        bus, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));
    Serial.println("✅ RGB panel objects created (rotation=0, LVGL handles orientation)");
    
    // Initialize display
    gfx->begin();
    gfx->fillScreen(BLACK);
    Serial.println("✅ Display initialized");
    
    // V3 touch init (after display, same as original)
    if (isTouchLCDv3()) {
      touch.setPins(-1, 16);
      if (!touch.begin(Wire, GT911_SLAVE_ADDRESS_L, TOUCH_SDA, TOUCH_SCL)) {
        Serial.println("❌ GT911 touch initialization failed (V3)!");
        while (1) delay(1000);
      }
      Serial.println("✅ GT911 touch controller initialized (V3)");
      touch_initialized = true;
    }
    Serial.println("✅ Touch init phase complete");
    
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
    
    // V4 needs 180° rotation — handle in LVGL, not Arduino_GFX
    // LVGL rotation works correctly with partial rendering (no dark line artifacts)
    if (getDisplayRotation() == 2) {
      lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_180);
      Serial.println("✅ LVGL display rotation set to 180° (V4)");
    }
    
    // Create LVGL touch input device (only if touch init succeeded)
    if (touch_initialized) {
      lv_indev_t *indev = lv_indev_create();
      lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
      lv_indev_set_read_cb(indev, lvgl_touch_read_cb);
      global_touch_indev = indev;
      Serial.println("✅ LVGL touch input registered");
    } else {
      Serial.println("⚠️  LVGL touch input SKIPPED (touch not available)");
    }
    
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
  // STEP 3: INITIALIZE SUBSYSTEMS (common to all boards)
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
  
  // ========================================================================
  // SCREEN MANAGER — pass version-aware IO write function
  // ========================================================================
  if (hasDisplay()) {
    screenMgr->begin(control_backlight, io_expander_write);
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
  // CONDITIONAL DISPLAY UPDATES (only on Touch LCD board — V3 or V4)
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
