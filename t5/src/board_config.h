#ifndef T5_BOARD_CONFIG_H
#define T5_BOARD_CONFIG_H

// =============================================================================
// LILYGO T5 4.7" V2.3 (ESP32-S3) Pin Definitions
// Source: https://github.com/Xinyuan-LilyGO/LilyGo-EPD47 (esp32s3 branch)
// =============================================================================

// E-paper display
// Managed internally by epdiy driver — no manual pin config needed

// Touch (GT911 capacitive, I2C)
#define TOUCH_SDA       18
#define TOUCH_SCL       17
#define TOUCH_INT       47

// Button
#define BUTTON_PIN      21

// Battery ADC
#define BATT_ADC_PIN    14

// SD Card (optional)
#define SD_MISO_PIN     16
#define SD_MOSI_PIN     15
#define SD_SCLK_PIN     11
#define SD_CS_PIN       42

// Display dimensions (ED047TC1 e-paper)
#define EPD_WIDTH       960
#define EPD_HEIGHT      540

#endif // T5_BOARD_CONFIG_H
