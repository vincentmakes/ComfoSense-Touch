/*
 * Board Configuration - Centralized board detection and pin definitions
 * AUTO-DETECTS which Waveshare ESP32-S3 board and version is being used
 * 
 * Supports:
 *   - ESP32-S3-Touch-LCD-4.0 V3.x  (TCA9554 IO expander @ 0x20)
 *   - ESP32-S3-Touch-LCD-4.0 V4.x  (CH32V003 RISC-V MCU @ 0x24)
 *   - ESP32-S3-RS485-CAN            (No IO expander, headless bridge)
 * 
 * This header must be included BEFORE any other hardware initialization
 * Usage: #include "board_config.h" at the top of main.cpp
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <Arduino.h>
#include <Wire.h>

// =============================================================================
// BOARD TYPE ENUM
// =============================================================================
enum BoardType {
    BOARD_UNKNOWN       = 0,
    BOARD_TOUCH_LCD_V3  = 1,   // TCA9554 IO expander at 0x20
    BOARD_TOUCH_LCD_V4  = 2,   // CH32V003 RISC-V MCU at 0x24
    BOARD_RS485_CAN     = 3    // No IO expander (headless bridge mode)
};

// =============================================================================
// I2C ADDRESSES FOR DETECTION
// =============================================================================
#define GT911_I2C_ADDR_L    0x5D  // GT911 touch controller (primary)
#define GT911_I2C_ADDR_H    0x14  // GT911 touch controller (alternate)
#define TCA9554_I2C_ADDR    0x20  // V3 IO expander
#define CH32V003_I2C_ADDR   0x24  // V4 IO expander (RISC-V MCU)

// =============================================================================
// V3: TCA9554 REGISTER MAP
// =============================================================================
#define TCA9554_REG_INPUT   0x00   // Input port (R)
#define TCA9554_REG_OUTPUT  0x01   // Output port (R/W)
#define TCA9554_REG_POLAR   0x02   // Polarity inversion
#define TCA9554_REG_CONFIG  0x03   // Configuration: 0=output, 1=input

// V3 EXIO bit positions (in TCA9554 output register)
#define V3_BIT_TP_RST       0   // EXIO1 — touch reset
#define V3_BIT_BL_EN        1   // EXIO2 — backlight enable
#define V3_BIT_LCD_RST      2   // EXIO3 — LCD reset
#define V3_BIT_SDCS         3   // EXIO4 — SD card CS
#define V3_BIT_DIMMING      4   // EXIO5 — AP3032 FB (software PWM dimming)
#define V3_BIT_BEE_EN       5   // EXIO6 — buzzer enable
#define V3_BIT_RTC_INT      6   // EXIO7 — RTC interrupt

// =============================================================================
// V4: CH32V003 REGISTER MAP
// Source: ESPhome PR #10071 (waveshare_io_ch32v003 component)
//         + factory firmware binary string analysis
// =============================================================================
#define CH32V003_REG_DIR    0x02   // Direction: 1=output, 0=input (INVERTED vs TCA!)
#define CH32V003_REG_OUTPUT 0x03   // GPIO output state
#define CH32V003_REG_INPUT  0x04   // GPIO input state
#define CH32V003_REG_PWM    0x05   // Backlight PWM duty (0–255, safe max 247)
#define CH32V003_REG_ADC    0x06   // Battery ADC (2 bytes, little-endian, 10-bit)
#define CH32V003_REG_INT    0x07   // Interrupt status

#define CH32V003_PWM_MAX    247    // Safe maximum per Waveshare/ESPhome docs

// V4 EXIO bit positions (in CH32V003 output register 0x03)
// WARNING: These are DIFFERENT from V3! Do NOT use V3 bits on V4 hardware!
#define V4_BIT_CHARGER      0   // EXIO0 — battery charger signal (input)
#define V4_BIT_TP_RST       1   // EXIO1 — touch reset
#define V4_BIT_TP_INT       2   // EXIO2 — touch interrupt (NOT backlight!)
#define V4_BIT_LCD_RST      3   // EXIO3 — LCD reset
#define V4_BIT_SDCS         4   // EXIO4 — SD card CS
#define V4_BIT_SYS_EN       5   // EXIO5 — system enable (NOT dimming!)
#define V4_BIT_BEE_EN       6   // EXIO6 — buzzer enable
#define V4_BIT_RTC_INT      7   // EXIO7 — RTC interrupt (input)

// V4 direction mask: outputs = EXIO1,3,4,5,6; inputs = EXIO0,2,7
// Bit pattern (1=output): 0b01111010 = 0x7A
#define CH32V003_DIR_MASK   0x7A

// =============================================================================
// GLOBAL STATE
// =============================================================================
extern BoardType g_board_type;

// =============================================================================
// BOARD DETECTION
// =============================================================================
inline BoardType detectBoard() {
    Serial.println("🔍 AUTO-DETECTING BOARD TYPE AND VERSION...");
    
    // Test 1: Try Touch LCD board I2C pins (SDA=15, SCL=7)
    Wire.begin(15, 7);
    delay(50);
    
    // Look for GT911 touch controller (only on Touch LCD boards)
    Wire.beginTransmission(GT911_I2C_ADDR_L);
    uint8_t gt911_error = Wire.endTransmission();
    
    // Look for CH32V003 at 0x24 (V4 IO expander)
    Wire.beginTransmission(CH32V003_I2C_ADDR);
    uint8_t ch32_error = Wire.endTransmission();
    
    // Look for TCA9554 at 0x20 (V3 IO expander)
    Wire.beginTransmission(TCA9554_I2C_ADDR);
    uint8_t tca_lcd_error = Wire.endTransmission();
    
    Wire.end();
    delay(10);
    
    // Priority: CH32V003 (V4) > TCA9554 (V3) > GT911-only fallback
    if (ch32_error == 0) {
        Serial.println("✅ ESP32-S3-Touch-LCD-4.0 V4 detected (CH32V003 @ 0x24)");
        return BOARD_TOUCH_LCD_V4;
    }
    
    if (gt911_error == 0 || tca_lcd_error == 0) {
        // GT911 or TCA9554 found on Touch LCD pins → V3
        Serial.println("✅ ESP32-S3-Touch-LCD-4.0 V3 detected (TCA9554 @ 0x20)");
        return BOARD_TOUCH_LCD_V3;
    }
    
    // Test 2: Try RS485-CAN board I2C pins (SDA=39, SCL=38)
    Wire.begin(39, 38);
    delay(50);
    
    Wire.beginTransmission(TCA9554_I2C_ADDR);
    uint8_t tca_rs485_error = Wire.endTransmission();
    
    Wire.end();
    delay(10);
    
    if (tca_rs485_error == 0) {
        Serial.println("✅ ESP32-S3-RS485-CAN detected");
        return BOARD_RS485_CAN;
    }
    
    // Last resort: Default to RS485-CAN if nothing found
    Serial.println("⚠️  No I2C devices detected, defaulting to RS485-CAN board");
    return BOARD_RS485_CAN;
}

// Initialize board detection - CALL THIS FIRST IN setup()!
inline void initBoardConfig() {
    g_board_type = detectBoard();
    
    // Handle GPIO conflicts on RS485-CAN board
    if (g_board_type == BOARD_RS485_CAN) {
        // CRITICAL: GPIO17, GPIO18, and GPIO21 are connected to RS485 transceiver
        // hardware on the RS485-CAN board. If left floating, they activate the
        // RS485 TX/RX LEDs. These pins MUST be driven LOW to keep RS485 inactive.
        // On Touch LCD board, these same pins are used by the RGB display panel.
        pinMode(17, OUTPUT);
        digitalWrite(17, LOW);
        pinMode(18, OUTPUT);
        digitalWrite(18, LOW);
        pinMode(21, OUTPUT);
        digitalWrite(21, LOW);
        Serial.println("⚠️  GPIO17, GPIO18, GPIO21 set to LOW (RS485 transceiver inactive)");
    }
    
    // Print full configuration
    Serial.println("📋 BOARD CONFIGURATION:");
    if (g_board_type == BOARD_TOUCH_LCD_V3) {
        Serial.println("   Board:     ESP32-S3-Touch-LCD-4.0 V3");
        Serial.println("   IO Exp:    TCA9554PWR @ 0x20");
        Serial.println("   I2C:       SDA=GPIO15, SCL=GPIO7, INT=GPIO16");
        Serial.println("   CAN:       TX=GPIO6, RX=GPIO0");
        Serial.println("   Display:   480x480 RGB, rotation=0");
        Serial.println("   Backlight: EXIO2 on/off + EXIO5 software PWM (needs R36+R40)");
        Serial.println("   GPIO17/18/21: Used by RGB panel");
    } else if (g_board_type == BOARD_TOUCH_LCD_V4) {
        Serial.println("   Board:     ESP32-S3-Touch-LCD-4.0 V4");
        Serial.println("   IO Exp:    CH32V003F4U6 @ 0x24");
        Serial.println("   I2C:       SDA=GPIO15, SCL=GPIO7");
        Serial.println("   CAN:       TX=GPIO6, RX=GPIO0");
        Serial.println("   Display:   480x480 RGB, rotation=2 (180°)");
        Serial.println("   Backlight: Hardware PWM via CH32V003 reg 0x05 (0-247)");
        Serial.println("   GPIO17/18/21: Used by RGB panel");
    } else if (g_board_type == BOARD_RS485_CAN) {
        Serial.println("   Board:     ESP32-S3-RS485-CAN");
        Serial.println("   I2C:       SDA=GPIO39, SCL=GPIO38, INT=GPIO40");
        Serial.println("   CAN:       TX=GPIO15, RX=GPIO16");
        Serial.println("   Display:   None (headless/bridge mode)");
        Serial.println("   GPIO17/18/21: Secured LOW (RS485 transceiver inactive)");
    }
    Serial.println("");
}

// =============================================================================
// QUERY FUNCTIONS
// =============================================================================

// Display availability — true for BOTH V3 and V4 Touch LCD boards
inline bool hasDisplay() {
    return (g_board_type == BOARD_TOUCH_LCD_V3 || 
            g_board_type == BOARD_TOUCH_LCD_V4);
}

// Version-specific queries
inline bool isTouchLCDv3() { return g_board_type == BOARD_TOUCH_LCD_V3; }
inline bool isTouchLCDv4() { return g_board_type == BOARD_TOUCH_LCD_V4; }

// V4 has hardware PWM dimming built into the CH32V003 — no software PWM needed
inline bool hasHardwarePWM()      { return isTouchLCDv4(); }
// V3 requires software PWM bit-banging via I2C (needs R36+R40 soldered)
inline bool supportsSoftwarePWM() { return isTouchLCDv3(); }

// IO expander I2C address
inline uint8_t getIOExpanderAddr() {
    return isTouchLCDv4() ? CH32V003_I2C_ADDR : TCA9554_I2C_ADDR;
}

// =============================================================================
// PIN DEFINITIONS - Conditional based on detected board
// =============================================================================

// CAN Bus pins (IDENTICAL on V3 and V4 Touch LCD boards)
inline gpio_num_t getCAN_TX() {
    return hasDisplay() ? GPIO_NUM_6 : GPIO_NUM_15;
}

inline gpio_num_t getCAN_RX() {
    return hasDisplay() ? GPIO_NUM_0 : GPIO_NUM_16;
}

// I2C pins (IDENTICAL on V3 and V4 Touch LCD boards)
inline int getI2C_SDA() {
    return hasDisplay() ? 15 : 39;
}

inline int getI2C_SCL() {
    return hasDisplay() ? 7 : 38;
}

// Touch interrupt pin
inline int getTouch_INT() {
    if (isTouchLCDv3()) return 16;   // Direct GPIO16
    if (isTouchLCDv4()) return -1;   // Via CH32V003 EXIO2, no direct GPIO
    return 40;                        // RS485-CAN board
}

inline int getTouch_RST() {
    return -1;  // Touch reset controlled via IO expander EXIO1, not direct GPIO
}

// Display rotation
inline int getDisplayRotation() {
    // V4 LCD panel is physically mounted 180° rotated vs V3
    // Confirmed by V4 factory firmware using LV_DISPLAY_ROTATION_180
    return isTouchLCDv4() ? 2 : 0;
}

// =============================================================================
// COMPATIBILITY MACROS for existing code
// These allow subsystems to use simple macro names
// =============================================================================

#define EXPA_I2C_SDA   getI2C_SDA()
#define EXPA_I2C_SCL   getI2C_SCL()

#define TOUCH_SDA      getI2C_SDA()
#define TOUCH_SCL      getI2C_SCL()
#define TOUCH_INT      getTouch_INT()
#define TOUCH_RST      getTouch_RST()

#define CAN_TX_PIN     getCAN_TX()
#define CAN_RX_PIN     getCAN_RX()

// Legacy compat — old code uses TCA9554_ADDR directly but io_expander_write() 
// now routes to the correct address. Keep this for any remaining direct refs.
#define TCA9554_ADDR   getIOExpanderAddr()

// Keep legacy register defines for V3 code paths that reference them
#define TCA_REG_OUTPUT TCA9554_REG_OUTPUT
#define TCA_REG_CONFIG TCA9554_REG_CONFIG

// Legacy EXIO defines (V3 meaning — only used in V3 code paths)
#define EXIO_TP_RST    1    // Touch reset
#define EXIO_BL_EN     2    // Backlight enable
#define EXIO_LCD_RST   3    // LCD reset

#endif // BOARD_CONFIG_H
