/*
 * Board Configuration - Centralized board detection and pin definitions
 * AUTO-DETECTS which Waveshare ESP32-S3 board is being used
 * 
 * This header must be included BEFORE any other hardware initialization
 * Usage: #include "board_config.h" at the top of main.cpp
 */

#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <Arduino.h>
#include <Wire.h>

// Board type enum
enum BoardType {
    BOARD_UNKNOWN = 0,
    BOARD_TOUCH_LCD = 1,
    BOARD_RS485_CAN = 2
};

// I2C addresses for detection
#define GT911_I2C_ADDR_L  0x5D  // GT911 touch controller
#define TCA9554_I2C_ADDR  0x20  // IO expander

// Global board type variable (set once during detection)
extern BoardType g_board_type;

// Board detection function - call this FIRST in setup()
inline BoardType detectBoard() {
    Serial.println("🔍 AUTO-DETECTING BOARD TYPE...");
    
    // Test 1: Try Touch LCD board I2C pins (SDA=15, SCL=7)
    Wire.begin(15, 7);
    delay(50);
    
    // Look for GT911 touch controller (only on Touch LCD)
    Wire.beginTransmission(GT911_I2C_ADDR_L);
    uint8_t gt911_error = Wire.endTransmission();
    
    // Look for TCA9554 IO expander
    Wire.beginTransmission(TCA9554_I2C_ADDR);
    uint8_t tca_lcd_error = Wire.endTransmission();
    
    Wire.end();
    delay(10);
    
    if (gt911_error == 0) {
        // GT911 found - definitely Touch LCD board
        Serial.println("✅ ESP32-S3-Touch-LCD-4.0 detected");
        return BOARD_TOUCH_LCD;
    }
    
    // Test 2: Try RS485-CAN board I2C pins (SDA=39, SCL=38)
    Wire.begin(39, 38);
    delay(50);
    
    Wire.beginTransmission(TCA9554_I2C_ADDR);
    uint8_t tca_rs485_error = Wire.endTransmission();
    
    Wire.end();
    delay(10);
    
    if (tca_rs485_error == 0) {
        // TCA9554 found on RS485 pins - RS485-CAN board
        Serial.println("✅ ESP32-S3-RS485-CAN detected");
        return BOARD_RS485_CAN;
    }
    
    // Fallback: If TCA was found on Touch LCD pins but no GT911
    if (tca_lcd_error == 0) {
        Serial.println("⚠️  TCA9554 found on GPIO15/7, assuming Touch LCD board");
        return BOARD_TOUCH_LCD;
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
    if (g_board_type == BOARD_TOUCH_LCD) {
        Serial.println("   Board: ESP32-S3-Touch-LCD-4.0");
        Serial.println("   I2C:   SDA=GPIO15, SCL=GPIO7, INT=GPIO16");
        Serial.println("   CAN:   TX=GPIO6, RX=GPIO0");
        Serial.println("   Display: 480x480 with GT911 touch");
        Serial.println("   GPIO17/18/21: Used by RGB panel");
    } else if (g_board_type == BOARD_RS485_CAN) {
        Serial.println("   Board: ESP32-S3-RS485-CAN");
        Serial.println("   I2C:   SDA=GPIO39, SCL=GPIO38, INT=GPIO40");
        Serial.println("   CAN:   TX=GPIO15, RX=GPIO16");
        Serial.println("   Display: None (headless/bridge mode)");
        Serial.println("   GPIO17/18/21: Secured LOW (RS485 transceiver inactive)");
    }
    Serial.println("");
}

// =============================================================================
// PIN DEFINITIONS - Conditional based on detected board
// =============================================================================

// CAN Bus pins
inline gpio_num_t getCAN_TX() {
    return (g_board_type == BOARD_TOUCH_LCD) ? GPIO_NUM_6 : GPIO_NUM_15;
}

inline gpio_num_t getCAN_RX() {
    return (g_board_type == BOARD_TOUCH_LCD) ? GPIO_NUM_0 : GPIO_NUM_16;
}

// I2C pins for TCA9554 expander and touch controller
inline int getI2C_SDA() {
    return (g_board_type == BOARD_TOUCH_LCD) ? 15 : 39;
}

inline int getI2C_SCL() {
    return (g_board_type == BOARD_TOUCH_LCD) ? 7 : 38;
}

inline int getTouch_INT() {
    return (g_board_type == BOARD_TOUCH_LCD) ? 16 : 40;
}

inline int getTouch_RST() {
    return -1;  // Touch reset controlled via TCA9554 EXIO1, not direct GPIO
}

// TCA9554 address (same on both boards)
inline uint8_t getTCA9554_ADDR() {
    return 0x20;
}

// TCA9554 register definitions (same on both boards)
#define EXIO_TP_RST    1    // Touch reset
#define EXIO_BL_EN     2    // Backlight enable
#define EXIO_LCD_RST   3    // LCD reset
#define TCA_REG_OUTPUT 0x01
#define TCA_REG_CONFIG 0x03

// Display availability
inline bool hasDisplay() {
    return (g_board_type == BOARD_TOUCH_LCD);
}

// =============================================================================
// COMPATIBILITY MACROS for existing code
// These allow your existing main.cpp to work without changes
// =============================================================================

#define EXPA_I2C_SDA   getI2C_SDA()
#define EXPA_I2C_SCL   getI2C_SCL()
#define TCA9554_ADDR   getTCA9554_ADDR()

#define TOUCH_SDA      getI2C_SDA()
#define TOUCH_SCL      getI2C_SCL()
#define TOUCH_INT      getTouch_INT()
#define TOUCH_RST      getTouch_RST()

#define CAN_TX_PIN     getCAN_TX()
#define CAN_RX_PIN     getCAN_RX()

#endif // BOARD_CONFIG_H