/*
 * TWAI Wrapper - Replaces esp32_can library with native ESP-IDF TWAI
 * Uses centralized board_config.h for automatic pin configuration
 * Supports:
 *   - Waveshare ESP32-S3-Touch-LCD-4.0 (CAN: TX=GPIO6, RX=GPIO0)
 *   - Waveshare ESP32-S3-RS485-CAN (CAN: TX=GPIO15, RX=GPIO16)
 */

#ifndef TWAI_WRAPPER_H
#define TWAI_WRAPPER_H

#include <Arduino.h>
#include "driver/twai.h"
#include "../board_config.h"  // Centralized board detection and pin config

// CAN_FRAME structure compatible with old esp32_can library
typedef struct {
    uint32_t id;          // CAN ID
    uint8_t extended;     // Extended frame (1) or standard (0)
    uint8_t rtr;          // RTR frame
    uint8_t length;       // Data length
    union {
        uint8_t byte[8];
        uint8_t uint8[8];
        uint32_t uint32[2];
    } data;
} CAN_FRAME;

class TWAIWrapper {
private:
    bool initialized;
    gpio_num_t tx_pin;
    gpio_num_t rx_pin;
    
public:
    TWAIWrapper() : initialized(false), tx_pin(GPIO_NUM_NC), rx_pin(GPIO_NUM_NC) {}
    
    // Get detected board type (for debugging/logging)
    BoardType getBoardType() const {
        return g_board_type;
    }
    
    // Initialize TWAI driver using centralized board config
    bool begin(uint32_t baudrate) {
        // Get pins from centralized board config
        tx_pin = getCAN_TX();
        rx_pin = getCAN_RX();
        
        Serial.print("🚌 Initializing CAN bus on TX=GPIO");
        Serial.print((int)tx_pin);
        Serial.print(", RX=GPIO");
        Serial.println((int)rx_pin);
        
        // GPIO0 pull-up if used (Touch LCD board)
        if (rx_pin == GPIO_NUM_0) {
            pinMode(0, INPUT_PULLUP);
            delay(10);
        }
        
        // Select timing config based on baudrate
        twai_timing_config_t t_config;
        if (baudrate == 50000) {
            t_config = TWAI_TIMING_CONFIG_50KBITS();
        } else if (baudrate == 125000) {
            t_config = TWAI_TIMING_CONFIG_125KBITS();
        } else if (baudrate == 250000) {
            t_config = TWAI_TIMING_CONFIG_250KBITS();
        } else if (baudrate == 500000) {
            t_config = TWAI_TIMING_CONFIG_500KBITS();
        } else {
            // Default to 50k
            t_config = TWAI_TIMING_CONFIG_50KBITS();
        }
        
        // Accept all messages
        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        
        // Configure TWAI with detected pins
        twai_general_config_t g_config = {
            .mode = TWAI_MODE_NORMAL,
            .tx_io = tx_pin,
            .rx_io = rx_pin,
            .clkout_io = (gpio_num_t)TWAI_IO_UNUSED,
            .bus_off_io = (gpio_num_t)TWAI_IO_UNUSED,
            .tx_queue_len = 32,
            .rx_queue_len = 32,
            .alerts_enabled = TWAI_ALERT_ALL,
            .clkout_divider = 0
        };
        
        // Install driver
        if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
            Serial.println("❌ TWAI: Failed to install driver");
            return false;
        }
        
        // Start driver
        if (twai_start() != ESP_OK) {
            Serial.println("❌ TWAI: Failed to start driver");
            return false;
        }
        
        initialized = true;
        Serial.println("✅ TWAI Driver started successfully");
        return true;
    }
    
    // Set debugging mode (for compatibility - does nothing)
    void setDebuggingMode(bool debug) {
        // Native TWAI doesn't have this, but we keep for compatibility
    }
    
    // Watch for messages (for compatibility - does nothing as filter already set)
    void watchFor() {
        // Filter already configured in begin()
    }
    
    // Read a CAN frame (convert TWAI message to CAN_FRAME)
    // IMPROVED: Direct queue polling instead of relying solely on alerts
    bool read(CAN_FRAME &frame) {
        if (!initialized) return false;
        
        // Try direct receive first (non-blocking, 0 timeout)
        // This polls the receive queue directly regardless of alert state
        twai_message_t rx_msg;
        if (twai_receive(&rx_msg, 0) == ESP_OK) {
            // Convert TWAI message to CAN_FRAME
            frame.id = rx_msg.identifier;
            frame.extended = rx_msg.extd;
            frame.rtr = rx_msg.rtr;
            frame.length = rx_msg.data_length_code;
            memcpy(frame.data.byte, rx_msg.data, rx_msg.data_length_code);
            return true;
        }
        
        // Check for alerts (bus errors, etc.) even if no data received
        uint32_t alerts;
        if (twai_read_alerts(&alerts, 0) == ESP_OK) {
            // Handle critical alerts
            if (alerts & TWAI_ALERT_BUS_OFF) {
                Serial.println("E (Alert) TWAI: Alert 4096");
                twai_initiate_recovery();
            }
            
            if (alerts & TWAI_ALERT_TX_FAILED) {
                Serial.println("E (Alert) TWAI: Alert 1024");
            }
        }
        
        return false;
    }
    
    // Send a CAN frame (convert CAN_FRAME to TWAI message)
    bool sendFrame(CAN_FRAME &frame) {
        if (!initialized) return false;
        
        twai_message_t tx_msg;
        tx_msg.identifier = frame.id;
        tx_msg.extd = frame.extended;
        tx_msg.rtr = frame.rtr;
        tx_msg.data_length_code = frame.length;
        memcpy(tx_msg.data, frame.data.byte, frame.length);
        
        // Try to send with 5ms timeout
        if (twai_transmit(&tx_msg, pdMS_TO_TICKS(5)) == ESP_OK) {
            return true;
        }
        
        return false;
    }
};

// Global instance to replace CAN0
extern TWAIWrapper CAN0;

#endif // TWAI_WRAPPER_H