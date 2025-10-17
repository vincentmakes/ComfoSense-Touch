/*
 * TWAI Wrapper - Replaces esp32_can library with native ESP-IDF TWAI
 * For Waveshare ESP32-S3-Touch-LCD-4
 */

#ifndef TWAI_WRAPPER_H
#define TWAI_WRAPPER_H

#include <Arduino.h>
#include "driver/twai.h"

// GPIO pins for Waveshare board
#define TX_GPIO_NUM GPIO_NUM_6
#define RX_GPIO_NUM GPIO_NUM_0

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
    
public:
    TWAIWrapper() : initialized(false) {}
    
    // Initialize TWAI driver
    bool begin(uint32_t baudrate) {
        // GPIO0 pull-up (strapping pin fix)
        pinMode(0, INPUT_PULLUP);
        delay(10);
        
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
        
        // General configuration
      //  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
      //      TX_GPIO_NUM, 
      //      RX_GPIO_NUM, 
      //      TWAI_MODE_NORMAL
      //  );
      twai_general_config_t g_config = {
    .mode = TWAI_MODE_NORMAL,
    .tx_io = TX_GPIO_NUM,
    .rx_io = RX_GPIO_NUM,
    .clkout_io = (gpio_num_t)TWAI_IO_UNUSED,
    .bus_off_io = (gpio_num_t)TWAI_IO_UNUSED,
    .tx_queue_len = 32,
    .rx_queue_len = 32,
    .alerts_enabled = TWAI_ALERT_RX_DATA | TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | TWAI_ALERT_BUS_OFF,
    .clkout_divider = 0
};
        
        // Enable all alerts
        g_config.alerts_enabled = TWAI_ALERT_ALL;
        
        // Install driver
        if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
            Serial.println("TWAI: Failed to install driver");
            return false;
        }
        
        // Start driver
        if (twai_start() != ESP_OK) {
            Serial.println("TWAI: Failed to start driver");
            return false;
        }
        
        initialized = true;
        Serial.println("TWAI Driver started");
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
    bool read(CAN_FRAME &frame) {
        if (!initialized) return false;
        
        uint32_t alerts;
        if (twai_read_alerts(&alerts, 0) == ESP_OK) {
            // Handle alerts
            if (alerts & TWAI_ALERT_BUS_OFF) {
                Serial.println("E (Alert) TWAI: Alert 4096");
                twai_initiate_recovery();
                return false;
            }
            
            if (alerts & TWAI_ALERT_TX_FAILED) {
                Serial.println("E (Alert) TWAI: Alert 1024");
            }
            
            // Check for RX data
            if (alerts & TWAI_ALERT_RX_DATA) {
                twai_message_t rx_msg;
                if (twai_receive(&rx_msg, 0) == ESP_OK) {
                    // Convert TWAI message to CAN_FRAME
                    frame.id = rx_msg.identifier;
                    frame.extended = rx_msg.extd;
                    frame.rtr = rx_msg.rtr;
                    frame.length = rx_msg.data_length_code;
                    memcpy(frame.data.byte, rx_msg.data, rx_msg.data_length_code);
                    
                    Serial.println("Alerts reconfigured");  // For compatibility with your logs
                    return true;
                }
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
        
        // Try to send with 1 second timeout
        if (twai_transmit(&tx_msg, pdMS_TO_TICKS(5)) == ESP_OK) {
            return true;
        }
        
        return false;
    }
};

// Global instance to replace CAN0
extern TWAIWrapper CAN0;

#endif // TWAI_WRAPPER_H
