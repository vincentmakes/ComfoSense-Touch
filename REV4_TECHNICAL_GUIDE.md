# Waveshare ESP32-S3-Touch-LCD-4.0 Rev 4 — Technical Guide

## Overview

The Rev 4 of the Waveshare ESP32-S3-Touch-LCD-4.0 introduces significant hardware changes compared to Rev 3. While the core remains the same (ESP32-S3 + 4" 480×480 RGB LCD + GT911 capacitive touch + CAN transceiver), the I/O management is entirely different. This document covers every aspect of working with the Rev 4 board: I/O expander, display, touch, backlight dimming, buzzer, and the pitfalls discovered through extensive testing.

### How to identify your board revision

Turn the board over. Rev 3 has "V3.0" silkscreened near the bottom edge. Rev 4 has "V4.0". Programmatically, the firmware detects the revision by scanning the I2C bus at startup:

| Device | Address | Board |
|--------|---------|-------|
| TCA9554 | 0x20 | Rev 3 |
| CH32V003 | 0x24 | Rev 4 |

---

## 1. I/O Expander: CH32V003 vs TCA9554

Rev 3 uses a Texas Instruments TCA9554 8-bit I/O expander. Rev 4 replaces it with a **CH32V003F4U6**, a WCH RISC-V microcontroller reprogrammed as a smart I/O expander with additional capabilities (hardware PWM, ADC, interrupt handling).

### Register Map

| Register | Address | Function | Notes |
|----------|---------|----------|-------|
| STATUS | 0x01 | Device status | Read-only |
| OUTPUT | 0x02 | GPIO output state | Read/Write |
| DIRECTION | 0x03 | Pin direction (1=output, 0=input) | **Inverted vs TCA9554!** |
| INPUT | 0x04 | GPIO input state | Read-only |
| PWM | 0x05 | Backlight PWM duty (0–255) | Safe max: 247 |
| ADC | 0x06 | Battery voltage ADC | 2 bytes, little-endian, 10-bit |
| INT | 0x07 | Interrupt status | Read-only |

> **Critical difference from TCA9554:** The direction register sense is inverted. On the TCA9554, `1` = input and `0` = output. On the CH32V003, `1` = output and `0` = input.

### EXIO Pin Map

| Bit | Pin | Function | Direction |
|-----|-----|----------|-----------|
| 0 | EXIO0 | Battery charger signal | Input |
| 1 | EXIO1 | Touch reset (TP_RST) | Output |
| 2 | EXIO2 | Touch interrupt (TP_INT) | Input |
| 3 | EXIO3 | LCD reset (LCD_RST) | Output |
| 4 | EXIO4 | SD card chip select (SDCS) | Output |
| 5 | EXIO5 | System enable (SYS_EN) | Output |
| 6 | EXIO6 | Buzzer enable (BEE_EN) | Output |
| 7 | EXIO7 | RTC interrupt | Input |

> **Important:** On Rev 3, EXIO5 is used for software PWM backlight dimming. On Rev 4, EXIO5 is the system enable pin — **not** backlight. Backlight is handled by the dedicated PWM register (0x05).

### Direction Masks

Two masks are used at different stages of initialization:

| Mask | Binary | When | Purpose |
|------|--------|------|---------|
| 0x3A | `00111010` | Factory init, early boot | TP_RST, LCD_RST, SDCS, SYS_EN as outputs. BEE_EN as **input** (safe — cannot buzz) |
| 0x7A | `01111010` | After touch init | Same + BEE_EN as output (enables buzzer control) |

### I2C Communication

The CH32V003 sits on the same I2C bus as the GT911 touch controller and the PCF8563 RTC:

| Device | Address | Function |
|--------|---------|----------|
| GT911 | 0x5D (or 0x14) | Touch controller |
| CH32V003 | 0x24 | I/O expander |
| PCF8563 | 0x51 | Real-time clock |

I2C pins: **SDA = GPIO15**, **SCL = GPIO7** (same as Rev 3).

Basic I2C write pattern:
```cpp
Wire.begin(15, 7);
Wire.beginTransmission(0x24);
Wire.write(register_address);
Wire.write(value);
Wire.endTransmission();
```

---

## 2. Startup Sequence — Buzzer Problem and Solution

### The problem

The CH32V003 powers up with all outputs HIGH (0xFF). Bit 6 (BEE_EN) being HIGH drives the buzzer continuously. The ESP32 bootloader takes 500–800ms to load firmware from flash before any user code executes. During this time the buzzer screams.

### The solution: two layers of defense

**Layer 1 — First line of `setup()`**

The very first code in `setup()` silences the buzzer via simple I2C writes — before Serial, before delay, before anything:

```cpp
void setup() {
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

    Serial.begin(115200);
    // ... rest of setup
}
```

> **Warning — keep this code simple!** The following patterns have been tested and **cause silent boot crashes** (no serial output, perpetual reboot loop):
> - `__attribute__((constructor))` global constructors — the Arduino Wire library requires the runtime to be initialized, which only happens at `setup()` entry.
> - `for` loop with retries around `Wire.beginTransmission()` — storing the `Wire.endTransmission()` return value in a local `uint8_t err` variable and looping causes a crash. Root cause unclear but reproducible. The straight-through writes without error checking work reliably.
>
> The simple, proven pattern above adds ~5ms at boot and works on every power cycle tested.

**Layer 2 — Board detection in `initBoardConfig()`**

When the CH32V003 is confirmed present via I2C probe, silence it again immediately before returning.

### Key principle

**Always set output register (0xBF) BEFORE making BEE_EN an output.** If you write direction 0x7A while output is still 0xFF, the buzzer activates for the duration between the two writes.

Correct order:
```cpp
// 1. Clear buzzer bit in output register
io_write(0x02, 0xBF);         // Output: buzzer LOW
// 2. THEN make BEE_EN an output
io_write(0x03, 0x7A);         // Direction: BEE_EN as output (now safe)
```

Wrong order:
```cpp
// ❌ WRONG — buzzer fires between these two writes!
io_write(0x03, 0x7A);         // BEE_EN becomes output while still HIGH = BUZZ
io_write(0x02, 0xBF);         // Too late — already buzzing
```

> **Note:** There is an irreducible ~200ms window during the ESP32 ROM bootloader where no user code can run. This may cause a brief "click" on power-up. A hardware fix (pull-down resistor on BEE_EN trace) would be the only way to eliminate this completely.

---

## 3. Display Initialization

### RGB Panel Pins (identical to Rev 3)

The ST7701S LCD controller and RGB panel interface use the same GPIO pins on both revisions:

```cpp
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
```

### 180° Rotation — CRITICAL

The LCD panel on Rev 4 is physically mounted 180° rotated compared to Rev 3. This must be corrected in software.

> **NEVER use `rotation=2` in `Arduino_RGB_Display`.** Software rotation in the RGB display driver conflicts with LVGL's partial rendering mode, causing dark lines over changing elements and visual remnants after closing menus.

Correct approach — use LVGL's display rotation:

```cpp
// Always create display with rotation=0
gfx = new Arduino_RGB_Display(
    480, 480, rgbpanel, 0 /* ALWAYS 0 */, true,
    bus, GFX_NOT_DEFINED, st7701_type1_init_operations,
    sizeof(st7701_type1_init_operations));

// Handle 180° rotation in LVGL (works with partial rendering)
lv_display_t *disp = lv_display_create(480, 480);
lv_display_set_buffers(disp, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_180);
```

### Touch Coordinate Inversion

LVGL 9.1.0's `lv_display_set_rotation()` rotates the rendered output but does **not** automatically transform touch input coordinates. You must manually invert them in the touch callback:

```cpp
static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    int16_t x[5], y[5];
    uint8_t touched = touch.getPoint(x, y, 5);

    if (touched > 0) {
        // Manual 180° inversion for V4
        data->point.x = 479 - x[0];
        data->point.y = 479 - y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}
```

---

## 4. GT911 Touch Controller

### The OTP Problem

Rev 3's GT911 has factory-programmed OTP (One-Time Programmable) configuration — 480×480 resolution and touch parameters are burned into the chip. It works immediately after `touch.begin()`.

**Rev 4's GT911 has NO OTP configuration.** After a hardware reset, the config registers read: version=0x00, X_resolution=0, Y_resolution=0, touch_count=0. The host must write the full configuration at every boot.

### Why `setMaxTouchPoint()` crashes

The SensorLib's `setMaxTouchPoint()` internally calls `reloadConfig()` which calls `writeBuffer()` using a cached I2C handle. The problem:

1. Board detection calls `Wire.begin()` → probe → `Wire.end()` (frees I2C handle)
2. V4 init calls `Wire.begin(15, 7)` again
3. `touch.begin()` stores a reference to Wire's internal handle
4. The stored handle pointer is now stale (freed by `Wire.end()`)
5. `setMaxTouchPoint()` → `writeBuffer()` → dereferences NULL → crash (`EXCVADDR: 0x0000000c`)

The global `Wire` object itself works fine (it was re-initialized). Only SensorLib's cached copy is stale.

### Solution: Manual Config Write via Raw I2C

Bypass SensorLib entirely and write the GT911 config registers using raw `Wire` calls:

```cpp
uint8_t gt_addr = 0x5D;  // Or 0x14, depending on INT level at boot

// 1. Read full 186-byte config (0x8047 to 0x8100)
uint8_t full_cfg[186];
memset(full_cfg, 0, sizeof(full_cfg));
for (uint16_t offset = 0; offset < 184; offset += 28) {
    uint8_t chunk = min((uint16_t)28, (uint16_t)(184 - offset));
    uint16_t reg = 0x8047 + offset;
    Wire.beginTransmission(gt_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.endTransmission(false);
    Wire.requestFrom(gt_addr, chunk);
    for (uint8_t i = 0; i < chunk && Wire.available(); i++) {
        full_cfg[offset + i] = Wire.read();
    }
}

// 2. Patch config fields
full_cfg[0] += 1;                   // Increment Config_Version (GT911 only accepts newer)
if (full_cfg[0] == 0) full_cfg[0] = 1;
full_cfg[1] = 0xE0; full_cfg[2] = 0x01;  // X_Output_Max = 480 (little-endian)
full_cfg[3] = 0xE0; full_cfg[4] = 0x01;  // Y_Output_Max = 480
full_cfg[5] = 5;                          // Touch_Number (max simultaneous touches)
if (full_cfg[6] == 0) full_cfg[6] = 0x0D; // Module_Switch1: X2Y | INT rising

// 3. Calculate checksum (bytes 0-183)
uint8_t checksum = 0;
for (int i = 0; i < 184; i++) checksum += full_cfg[i];
checksum = (~checksum) + 1;
full_cfg[184] = checksum;   // Register 0x80FF
full_cfg[185] = 0x01;       // Register 0x8100 (config_fresh flag)

// 4. Write back in 28-byte chunks
for (uint16_t offset = 0; offset < 186; offset += 28) {
    uint8_t chunk = min((uint16_t)28, (uint16_t)(186 - offset));
    uint16_t reg = 0x8047 + offset;
    Wire.beginTransmission(gt_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.write(&full_cfg[offset], chunk);
    Wire.endTransmission();
}
delay(100);  // GT911 processes the new config
```

### GT911 I2C Address

The GT911's I2C address is determined by the level of the INT pin at reset:

| INT level at reset | Address |
|-------------------|---------|
| LOW | 0x5D |
| HIGH | 0x14 |

On Rev 4, the INT level depends on the CH32V003 direction mask at the moment the GT911 boots. With the factory mask 0x3A (TP_INT as input with pull-up), the GT911 typically appears at 0x5D. The firmware scans both addresses.

### Initialization Order

The Rev 4 factory demo initializes touch **before** display. This order is critical:

```
1. Wire.begin(15, 7)             ← I2C bus
2. CH32V003 output + direction   ← IO expander (releases TP_RST)
3. delay(100)                    ← GT911 boot time
4. touch.begin()                 ← Touch controller
5. Manual config write           ← If GT911 has no OTP
6. Wire.begin(15, 7, 400000)    ← Switch to 400kHz for display
7. gfx->begin()                 ← Display init
```

On Rev 3, the order is reversed (display first, then touch). Using the wrong order causes touch initialization failures.

---

## 5. Backlight Dimming

### Architecture

Both Rev 3 and Rev 4 use the same AP3032 boost LED driver for the backlight. The difference is how the FB (feedback) pin is controlled:

| | Rev 3 | Rev 4 |
|---|---|---|
| **Method** | Software PWM via TCA9554 EXIO5 | Hardware PWM via CH32V003 register 0x05 |
| **Frequency** | 60Hz (I2C-limited) | Flicker-free (CH32V003 internal) |
| **Hardware mod** | Requires R36 (100k) + R40 (0R) | None — built in |
| **Control pin** | EXIO5 → R40 → AP3032 FB | CH32V003 PWM → R40 → AP3032 FB |

### PWM Polarity — INVERTED

The CH32V003 PWM output drives the AP3032's FB pin LOW through R40 (10kΩ on Rev 4). Pulling FB lower reduces the output voltage, which **dims** the backlight. Therefore:

| PWM Duty | FB Voltage | Brightness |
|----------|-----------|------------|
| 0 | High (FB free) | **Maximum** |
| 247 | Low (FB pulled down) | **Minimum / Off** |

This is the opposite of what you might expect. Higher duty = dimmer.

### Non-Linear Response

The AP3032's response to FB voltage is non-linear. Duty values 0–80 all produce nearly identical full brightness because the boost converter is already at maximum current. Visible dimming only starts around duty 80. The usable range is approximately:

| Brightness | PWM Duty |
|------------|----------|
| 100% (max) | 30 |
| 80% | 72 |
| 50% | 135 |
| 20% | 198 |
| 0% (min visible) | 240 |
| Screen off | 247 |

### Standalone Dimming Example

Below is a complete, self-contained Arduino sketch that demonstrates backlight dimming on the Rev 4 board. It cycles through brightness levels and can be used to test and calibrate the dimming range on your specific board.

```cpp
// ==========================================================================
// Rev 4 Backlight Dimming — Standalone Example
// Waveshare ESP32-S3-Touch-LCD-4.0 V4
//
// Demonstrates CH32V003 hardware PWM backlight control.
// Cycles through brightness levels with serial output.
//
// Hardware: CH32V003 @ I2C 0x24, register 0x05 = PWM duty
// The AP3032 FB pin is pulled low by PWM — higher duty = dimmer.
// ==========================================================================

#include <Arduino.h>
#include <Wire.h>

// CH32V003 I2C configuration
#define CH32V003_ADDR    0x24
#define REG_OUTPUT       0x02
#define REG_DIRECTION    0x03
#define REG_PWM          0x05
#define PWM_MAX          247    // Safe maximum (do not exceed)

// I2C pins for Touch LCD V4
#define I2C_SDA          15
#define I2C_SCL          7

// Dimming range (calibrated for visible response)
#define DUTY_BRIGHTEST   30     // Minimum duty = maximum brightness
#define DUTY_DIMMEST     240    // Maximum useful duty = minimum brightness

// Write a single register to the CH32V003
bool ch32_write(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(CH32V003_ADDR);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

// Convert brightness percentage (0-100) to PWM duty
uint8_t brightness_to_duty(uint8_t percent) {
    if (percent >= 100) return DUTY_BRIGHTEST;
    if (percent == 0)   return DUTY_DIMMEST;
    // Linear map across the useful range
    uint16_t range = DUTY_DIMMEST - DUTY_BRIGHTEST;
    return DUTY_DIMMEST - (uint8_t)((uint16_t)percent * range / 100);
}

// Set backlight brightness (0-100%)
void set_brightness(uint8_t percent) {
    uint8_t duty = brightness_to_duty(percent);
    ch32_write(REG_PWM, duty);
    Serial.printf("Brightness: %3d%% → duty: %3d\n", percent, duty);
}

// Turn backlight completely off
void backlight_off() {
    ch32_write(REG_PWM, PWM_MAX);
    Serial.println("Backlight OFF (duty=247)");
}

// Turn backlight on at a given brightness
void backlight_on(uint8_t percent) {
    set_brightness(percent);
}

void setup() {
    // ---- Step 0: Silence buzzer IMMEDIATELY ----
    // Keep this simple — no loops, no error checking (causes crashes on ESP32-S3)
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.beginTransmission(CH32V003_ADDR);
    Wire.write(REG_OUTPUT);
    Wire.write(0xBF);                    // Bit 6 (buzzer) LOW
    Wire.endTransmission();
    Wire.beginTransmission(CH32V003_ADDR);
    Wire.write(REG_DIRECTION);
    Wire.write(0x3A);                    // BEE_EN as input (safe)
    Wire.endTransmission();

    Serial.begin(115200);
    delay(500);
    Serial.println("\n===================================");
    Serial.println("Rev 4 Backlight Dimming Demo");
    Serial.println("===================================\n");

    // Verify CH32V003 is present
    Wire.beginTransmission(CH32V003_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("ERROR: CH32V003 not found at 0x24!");
        Serial.println("This demo requires a Rev 4 board.");
        while (1) delay(1000);
    }
    Serial.println("CH32V003 found at 0x24\n");

    // ---- Step 1: Initialize IO expander ----
    // Output: all HIGH except buzzer (bit 6)
    ch32_write(REG_OUTPUT, 0xBF);
    // Direction: TP_RST, LCD_RST, SDCS, SYS_EN, BEE_EN as outputs
    ch32_write(REG_DIRECTION, 0x7A);

    // ---- Step 2: Start at 80% brightness ----
    set_brightness(80);
    delay(2000);

    // ---- Step 3: Demo — cycle through brightness levels ----
    Serial.println("\n--- Brightness cycle demo ---\n");

    // Ramp down from 100% to 0%
    for (int pct = 100; pct >= 0; pct -= 10) {
        set_brightness(pct);
        delay(1500);
    }

    delay(1000);

    // Ramp back up from 0% to 100%
    for (int pct = 0; pct <= 100; pct += 10) {
        set_brightness(pct);
        delay(1500);
    }

    Serial.println("\n--- Demo complete ---");
    Serial.println("Backlight set to 80% (comfortable default)\n");
    set_brightness(80);
}

void loop() {
    // Interactive: send a number 0-100 via Serial Monitor to set brightness
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        int val = input.toInt();

        if (input == "off") {
            backlight_off();
        } else if (input == "on") {
            backlight_on(80);
        } else if (val >= 0 && val <= 100) {
            set_brightness(val);
        } else {
            Serial.println("Send 0-100 for brightness, 'off', or 'on'");
        }
    }
    delay(50);
}
```

### Integration with LVGL / Screen Manager

In a production LVGL application, the dimming is managed by the screen manager which calls `io_expander_write(REG_PWM, duty)` through a function pointer. The key integration points are:

```cpp
// During init — provide the IO write function
screenMgr = new ScreenManager(io_expander_write, current_io_output_state);

// In ScreenManager::setHardwareBrightness()
void ScreenManager::setHardwareBrightness(uint8_t percent) {
    uint8_t duty = DUTY_DIM - (uint16_t)percent * (DUTY_DIM - DUTY_BRIGHT) / 100;
    io_write(CH32V003_REG_PWM, duty);
}

// Screen off (Night Time Mode)
io_write(CH32V003_REG_PWM, CH32V003_PWM_MAX);  // 247 = off

// Screen on (wake from NTM)
setHardwareBrightness(current_brightness);       // Restore saved level
```

---

## 6. CAN Bus

CAN bus pins are the same on Rev 3 and Rev 4 Touch LCD boards:

| Pin | GPIO |
|-----|------|
| CAN TX | GPIO6 |
| CAN RX | GPIO0 |

Baud rate: **50 kbps** (ComfoNet standard). Use the ESP-IDF TWAI driver with increased queue lengths (32 TX, 32 RX) for reliable communication with the MVHR.

---

## 7. Pin Summary — Rev 3 vs Rev 4

| Function | Rev 3 | Rev 4 | Notes |
|----------|-------|-------|-------|
| I2C SDA | GPIO15 | GPIO15 | Same |
| I2C SCL | GPIO7 | GPIO7 | Same |
| CAN TX | GPIO6 | GPIO6 | Same |
| CAN RX | GPIO0 | GPIO0 | Same |
| IO Expander | TCA9554 @ 0x20 | CH32V003 @ 0x24 | Different chip, different registers |
| Touch INT | GPIO16 (direct) | CH32V003 EXIO2 | No direct GPIO on V4 |
| Touch RST | EXIO1 (via TCA) | EXIO1 (via CH32) | Both via IO expander |
| LCD RST | EXIO3 (via TCA) | EXIO3 (via CH32) | Both via IO expander |
| Backlight | Software PWM on EXIO5 | Hardware PWM reg 0x05 | V4 needs no HW mod |
| Buzzer | N/A | EXIO6 (BEE_EN) | V4 only |
| Display rotation | 0° (native) | 180° (LVGL corrected) | Critical difference |
| GT911 OTP | Yes (factory programmed) | No (host must configure) | Critical difference |
| RTC | N/A | PCF8563 @ 0x51 | V4 only |
| Battery ADC | N/A | CH32V003 reg 0x06 | V4 only |
| RGB panel pins | Same | Same | Identical across revisions |
| SPI bus (ST7701S) | Same | Same | CS=42, SCK=2, MOSI=1 |

---

## 8. Common Pitfalls and Lessons Learned

### Keep early I2C writes simple — no loops or error handling

At the very start of `setup()`, before Serial is initialized, certain code patterns cause silent boot crashes on the ESP32-S3. Specifically, wrapping `Wire.beginTransmission()` / `Wire.endTransmission()` in a `for` loop with a return value check (`uint8_t err = Wire.endTransmission(); if (err == 0) break;`) causes a perpetual reboot. The root cause is unclear but reproducible. Use straight-through I2C writes without error checking for the early buzzer silencing.

### `Wire.end()` corrupts SensorLib I2C handle

If you call `Wire.end()` (e.g., during board detection) and then `Wire.begin()` again, the global `Wire` object works but SensorLib's internally cached I2C handle becomes a dangling pointer. Any SensorLib function that writes to the bus (like `setMaxTouchPoint()`) will crash with `EXCVADDR: 0x0000000c`. Use raw `Wire.beginTransmission()` calls instead.

### Never use `rotation ≠ 0` in Arduino_RGB_Display

The Arduino_GFX RGB display driver performs software coordinate transformation during each partial LVGL flush. The RGB panel's DMA is simultaneously reading the PSRAM framebuffer, causing race conditions that manifest as dark lines and rendering artifacts. Always use `rotation=0` and handle orientation in LVGL with `lv_display_set_rotation()`.

### Touch before display on Rev 4

The factory demo initializes touch before calling `gfx->begin()`. The `gfx->begin()` call reconfigures SPI and may affect I2C timing. Initializing touch after display causes intermittent GT911 communication failures on Rev 4.

### GT911 address depends on boot timing

The GT911 samples its INT pin during reset to determine its I2C address. The CH32V003's direction mask at boot time affects whether INT floats high or low. With mask 0x3A, the GT911 typically settles at 0x5D. Always scan both 0x5D and 0x14.

### Config version must increment

The GT911 only accepts a config write if the Config_Version byte (0x8047) is **greater** than the current version. If you write the same version, the GT911 silently ignores it. Always read the current version first and increment by 1.

### Software PWM (Rev 3) blocks CAN bus

On Rev 3, software PWM at 60Hz through the TCA9554 requires frequent I2C writes in the main loop. If `lv_refr_now()` is called from an LVGL event callback, it blocks the main loop long enough to trigger watchdog timeouts and miss CAN bus interrupts. Use deferred refresh patterns with rate limiting (max once every 500ms).

---

## 9. Additional Rev 4 Hardware

### Real-Time Clock (PCF8563)

Rev 4 includes a PCF8563 RTC at I2C address 0x51. This can provide timekeeping when NTP is unavailable. The RTC interrupt is connected to CH32V003 EXIO7.

### Battery Connector (BAT1)

Rev 4 has a Li-Ion battery connector. Battery voltage can be read via CH32V003 register 0x06 (ADC, 2 bytes, little-endian, 10-bit resolution). The charger status is available on EXIO0 (input).

### SD Card

SD card chip select is on EXIO4. This is managed through the CH32V003 output register, same as Rev 3's TCA9554.

---

## 10. Quick Reference — Minimal Rev 4 Init Sequence

For anyone porting to the Rev 4 board, here is the minimum viable initialization:

```cpp
#include <Wire.h>

void setup() {
    // 1. Silence buzzer (FIRST THING — before Serial!)
    //    Keep this simple — no loops, no error checking (retries cause crashes)
    Wire.begin(15, 7);
    Wire.beginTransmission(0x24);
    Wire.write(0x02); Wire.write(0xBF);  // Output: buzzer OFF
    Wire.endTransmission();
    Wire.beginTransmission(0x24);
    Wire.write(0x03); Wire.write(0x3A);  // Direction: factory safe mask
    Wire.endTransmission();
    Wire.end();

    Serial.begin(115200);
    delay(100);

    // 2. Initialize touch (BEFORE display)
    // ... GT911 init with manual config write if needed

    // 3. Set backlight
    Wire.beginTransmission(0x24);
    Wire.write(0x05);  // PWM register
    Wire.write(72);    // ~80% brightness (inverted: lower = brighter)
    Wire.endTransmission();

    // 4. Enable buzzer control + full direction mask
    Wire.beginTransmission(0x24);
    Wire.write(0x02); Wire.write(0xBF);  // Ensure buzzer still OFF
    Wire.endTransmission();
    Wire.beginTransmission(0x24);
    Wire.write(0x03); Wire.write(0x7A);  // Full mask with BEE_EN as output
    Wire.endTransmission();

    // 5. Switch to 400kHz for display init
    Wire.begin(15, 7, 400000);

    // 6. Initialize display (rotation=0, LVGL handles 180°)
    // ... Arduino_RGB_Display + LVGL init
}
```

---

*Last updated: February 2026. Based on Waveshare ESP32-S3-Touch-LCD-4.0 V4.0 hardware with CH32V003F4U6 IO expander.*
