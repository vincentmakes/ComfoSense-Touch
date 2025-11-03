# ComfoSense Touch - ESP32-S3 Touch Display Controller

**Repository**: https://github.com/vincentmakes/ComfoSense-Touch

## Project Overview

A professional touch screen controller for ComfoAir ventilation systems using an ESP32-S3 with a 4" RGB LCD display (480x480) running LVGL 9.1.0. The device provides local control via a capacitive touch interface and integrates with home automation systems via MQTT.

## Hardware Platform

### Board: Waveshare ESP32-S3-Touch-LCD-4.0
- **MCU**: ESP32-S3-WROOM-1-N16R8 (16MB Flash, 8MB PSRAM)
- **Display**: 480x480 RGB LCD via ESP32 RGB panel interface
- **Touch**: GT911 capacitive touch controller (I2C)
- **I2C Expander**: TCA9554 for backlight/reset control
- **CAN Interface**: TJA1051T CAN transceiver
- **Power**: SW6124 USB-C PD with battery management
- **Schematic**: See `Schematics_ESP32S3TouchLCD4_V3_0.pdf`

### Pin Configuration
```
Display RGB Interface:
  R: IO46, 3, 8, 18, 17
  G: IO14, 13, 12, 11, 10, 9
  B: IO5, 45, 48, 47, 21
  
Display Control:
  DE: IO40, VSYNC: IO39, HSYNC: IO38, PCLK: IO41
  CS: IO42, SCK: IO2, MOSI: IO1

Touch (GT911 via I2C):
  SDA: IO15, SCL: IO7, INT: IO16
  Reset controlled via TCA9554 EXIO1

CAN Bus:
  TX: IO4, RX: IO5

TCA9554 I2C Expander:
  SDA: IO15, SCL: IO7
  EXIO1: Touch Reset
  EXIO2: Backlight Enable
  EXIO3: LCD Reset
```

## System Architecture

### Operating Modes

#### 1. Normal Mode (Default)
- Direct CAN bus communication with ComfoAir MVHR system
- MQTT integration optional (for Home Assistant, etc.)
- Full bi-directional control and monitoring
- Time synchronization with MVHR device

#### 2. Remote Client Mode
- **Enable in `secrets.h`**: `#define REMOTE_CLIENT_MODE true`
- No CAN bus initialization - device acts as MQTT-only client
- All data received from separate bridge device via MQTT
- Commands sent via MQTT (not directly to CAN)
- Useful for secondary display panels or network-isolated installations
- MQTT becomes mandatory in this mode

### Key Software Components

#### Display & GUI (LVGL 9.1.0)
- **GUI Generation**: SquareLine Studio 1.5.1
- **Screen Resolution**: 480x480 pixels
- **Update Strategy**: Dual-buffer with manual touch polling
- **Design System**: Custom cyan/yellow theme (see Design Tokens below)

#### Touch System
- **Controller**: GT911 capacitive touch (I2C)
- **Driver**: TouchDrvGT911 library
- **Polling Rate**: 200Hz (5ms intervals) - critical for responsiveness
- **Initialization Order**: MUST register touch AFTER GUI initialization
- **Reference Code**: `DemoCode_03_Drawing_points.ino` (working touch example)

#### CAN Communication (Normal Mode Only)
- **Protocol**: ComfoAir proprietary CAN protocol (50 kbps)
- **Implementation**: ESP32 native TWAI (TwoWire Automotive Interface)
- **Message Types**: Command, Request, Status, Time sync
- **Processing**: Throttled to 10ms intervals to prevent flooding

#### Network Services
- **WiFi**: Non-blocking connection (max 20 sec timeout)
- **MQTT**: Optional in Normal Mode, Mandatory in Remote Client Mode
- **OTA**: Over-the-air firmware updates
- **NTP**: Time synchronization for display

## Critical Code Patterns & Known Issues

### ✅ Display Update Patterns (CRITICAL)

The project uses two different update strategies depending on the UI element type:

#### Strategy 1: Image Updates (Opacity Changes)
```c
// For fan speed images - works with simple invalidation
lv_obj_set_style_opa(GUI_Image__screen__fanspeed2, LV_OPA_COVER, 0);
lv_obj_invalidate(active_image);
lv_obj_invalidate(parent);
// NO need for GUI_request_display_refresh()
```

#### Strategy 5: Label Text Updates (THE WORKING SOLUTION)
```c
// For time/date labels and any text that must update without user interaction
lv_label_set_text(GUI_Label__screen__time, new_text);

// CRITICAL: Force immediate refresh BEFORE invalidation
GUI_request_display_refresh();  // Calls lv_refr_now()

lv_obj_invalidate(GUI_Label__screen__time);
```

**Why the difference?**
- Image opacity changes are lightweight style properties
- Label text changes require text reflow and recalculation
- Without forced refresh, labels only update on next touch event
- `GUI_request_display_refresh()` must be called from main loop (NOT interrupts)

#### Button Update Pattern (Instant Feedback)
```c
// Update display FIRST for instant visual feedback
GUI_update_fan_speed_display_from_cpp(speed, boost);

// THEN send CAN command in background
sendFanSpeedCommand(...);
```

### ✅ Touch Initialization Sequence (CRITICAL)

**Correct Order** (as implemented in `main.cpp`):
```c
1. init_expander();                    // TCA9554 I2C expander
2. gfx->begin();                       // Display
3. lv_init();                          // LVGL core
4. lv_display_create();                // LVGL display
5. touch.begin(Wire, ...);             // Touch controller
6. GUI_init();                         // Load GUI
7. GUI_init_fan_speed_display();       // Initialize display state
8. GUI_init_dropdown_styling();        // Apply custom styles
9. lv_indev_create();                  // Register touch with LVGL
10. lv_indev_set_read_cb(...);        // Set touch callback

// Main loop MUST include manual touch polling:
if (now - last_touch_read >= 5) {      // 5ms = 200Hz
    lv_indev_read(global_touch_indev);
}
```

**Why this matters:**
- Touch MUST be registered AFTER GUI is loaded
- Manual polling at 200Hz is REQUIRED for responsive touch
- Reference: `DemoCode_03_Drawing_points.ino` shows working GT911 usage

### ✅ Debouncing System

**Per-Button Aggressive Debouncing** (prevents double-presses):
```c
// Each button tracks its own last press time
static unsigned long last_plus_press = 0;
static unsigned long last_minus_press = 0;
static unsigned long last_boost_press = 0;

// 500ms debounce per button (optimized to 50ms in latest code)
static const unsigned long BUTTON_DEBOUNCE_MS = 50;

static bool is_too_soon(unsigned long* last_press_time) {
    unsigned long now = millis();
    if (now - *last_press_time < BUTTON_DEBOUNCE_MS) {
        return true;  // Block this press
    }
    *last_press_time = now;
    return false;
}
```

**Key Benefits:**
- Prevents duplicate CAN commands from rapid touch events
- Each button independent (can press + then - rapidly)
- Logs blocked presses for debugging
- See `DEBOUNCE_FIX_FINAL.md` for complete analysis

### ✅ Main Loop Structure (Optimized for Touch Responsiveness)

```c
void loop() {
    // PRIORITY 1: LVGL timer handler (call frequently)
    lv_timer_handler();
    
    // PRIORITY 2: Process display refreshes (instant button feedback)
    GUI_process_display_refresh();
    
    // PRIORITY 3: Touch polling every 5ms (CRITICAL)
    if (millis() - last_touch_read >= 5) {
        lv_indev_read(global_touch_indev);
        last_touch_read = millis();
    }
    
    // PRIORITY 4: Screen manager (handles state transitions)
    if (screenMgr) screenMgr->loop();
    
    // PRIORITY 5: CAN processing (throttled to 10ms, normal mode only)
    #if !REMOTE_CLIENT_MODE
        if (millis() - last_can_process >= 10) {
            if (comfo) comfo->loop();
        }
    #endif
    
    // PRIORITY 6: Data managers (batched updates every 5 sec)
    if (sensorData) sensorData->loop();
    if (filterData) filterData->loop();
    if (controlMgr) controlMgr->loop();
    if (errorData) errorData->loop();
    
    // PRIORITY 7: Network services (lower priority)
    if (wifi) wifi->loop();
    if (wifi->isConnected()) {
        if (mqtt) mqtt->loop();
        if (ota) ota->loop();
        if (timeMgr) timeMgr->loop();  // Updates time labels every second
    }
    
    delay(1);
}
```

## Project File Structure

```
ComfoSense-Touch/
├── src/
│   ├── main.cpp                          # Hardware init, main loop, touch polling
│   │
│   ├── ui/                               # LVGL GUI (generated by SquareLine Studio)
│   │   ├── GUI.h                         # Main GUI declarations
│   │   ├── GUI.c                         # GUI initialization
│   │   ├── GUI_Variables.c               # Widget declarations
│   │   ├── lvgl_compat.h                 # LVGL 9.1 compatibility layer
│   │   ├── ui_helpers.h/c                # SquareLine helper functions
│   │   ├── Behaviour/
│   │   │   └── GUI_Events.c              # Event handlers (buttons, dropdown)
│   │   ├── Content/
│   │   │   ├── GUI_Animations.c          # Animations (if any)
│   │   │   └── Styles/
│   │   │       └── GUI_GlobalStyles.c    # Color definitions, fonts
│   │   └── Screens/
│   │       └── screen.c                  # Screen layout, widget creation
│   │
│   ├── comfoair/                         # CAN bus communication
│   │   ├── comfoair.h/cpp                # Main CAN handler
│   │   ├── message.h/cpp                 # Message encode/decode
│   │   ├── commands.h                    # Command definitions
│   │   ├── twai_wrapper.h/cpp            # ESP32 TWAI driver wrapper
│   │   ├── CanAddress.h/cpp              # CAN address definitions
│   │   ├── sensor_data.h/cpp             # Temperature, humidity, CO2 data
│   │   ├── filter_data.h/cpp             # Filter status management
│   │   ├── control_manager.h/cpp         # Fan speed, boost, temp profile control
│   │   └── error_data.h/cpp              # Error and alarm handling
│   │
│   ├── screen/                           # Screen state management
│   │   └── screen_manager.h/cpp          # NTM (No-Touch Mode) handling
│   │
│   ├── time/                             # Time synchronization
│   │   └── time_manager.h/cpp            # NTP sync, display updates
│   │
│   ├── mqtt/                             # MQTT integration
│   │   └── mqtt.h/cpp                    # MQTT client, subscriptions
│   │
│   ├── wifi/                             # WiFi management
│   │   └── wifi.h/cpp                    # Connection handling
│   │
│   └── ota/                              # Over-the-air updates
│       └── ota.h/cpp                     # OTA update handling
│
├── include/
│   └── secrets.h                         # Configuration (WiFi, MQTT, modes)
│
├── platformio.ini                        # PlatformIO build configuration
│
└── reference/
    ├── DemoCode_03_Drawing_points.ino    # Working GT911 touch example
    ├── PROJECT_MEMORY_UPDATED.md         # Complete project memory/history
    └── DEBOUNCE_FIX_FINAL.md            # Debounce solution documentation
```

## Configuration

### Required: `secrets.h` (copy from `secrets_template.h`)

```cpp
// WiFi Configuration
#define WIFI_SSID   "your-ssid"
#define WIFI_PASS   "your-password"

// MQTT Configuration (optional in Normal Mode, mandatory in Remote Client Mode)
#define MQTT_ENABLED true
#define MQTT_HOST   "192.168.1.100"
#define MQTT_PORT   1883
#define MQTT_USER   "your-mqtt-user"
#define MQTT_PASS   "your-mqtt-pass"
#define MQTT_PREFIX "comfoair"

// Operating Mode
#define REMOTE_CLIENT_MODE false    // true = MQTT-only, false = CAN + optional MQTT

// Timezone (POSIX format for NTP)
#define NTM_TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"  // Central European Time
```

### PlatformIO Configuration

```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_build.partitions = huge_app.csv

build_flags = 
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DBOARD_HAS_PSRAM
    
lib_deps = 
    lvgl/lvgl@^9.1.0
    moononournation/GFX Library for Arduino@^1.4.7
    knolleary/PubSubClient@^2.8
    # TouchDrvGT911 (included in src/)
```

## Design System

### Color Tokens
```c
Primary:    #00C7FF  (Cyan)     - Buttons, dropdown, accents
Accent:     #FFC802  (Yellow)   - Boost button, selected items
Background: #000000  (Black)    - Main background
Panel:      #191919  (Dark)     - Panels, containers
Panel Alt:  #002138  (Navy)     - Dropdown list background
Text:       #FFFFFF  (White)    - Primary text
Text Dark:  #000000  (Black)    - Text on accent colors
```

### Typography
- **Large Bold**: `largebold_1` - Buttons, dropdown text
- **Regular**: `font_1` - Labels, body text

## Manager System

The project uses a manager-based architecture to handle different subsystems:

### SensorDataManager (`sensor_data.h/cpp`)
- Receives temperature, humidity, CO2, RPM data from CAN
- Updates display labels (batched every 5 seconds)
- Provides data to MQTT for publishing

### FilterDataManager (`filter_data.h/cpp`)
- Tracks filter status and remaining time
- Updates filter warnings on display
- Provides filter data to MQTT

### ControlManager (`control_manager.h/cpp`)
- Handles button presses (fan speed, boost, temp profile)
- Sends CAN commands OR MQTT commands (depending on mode)
- Manages boost timer countdown
- Updates fan speed display using Strategy 5 pattern
- Debounces button presses per-button

### ErrorDataManager (`error_data.h/cpp`)
- Receives error and alarm messages
- Updates error indicators on display
- Provides error status to MQTT

### ScreenManager (`screen_manager.h/cpp`)
- Handles NTM (No-Touch Mode) - auto screen dim after inactivity
- Controls backlight brightness
- Manages touch wake-up

### TimeManager (`time_manager.h/cpp`)
- Syncs time via NTP on startup
- Updates time/date labels every second using Strategy 5
- Syncs time with MVHR device (Normal Mode only)

## Building and Flashing

```bash
# Install PlatformIO
pip install platformio

# Clone repository
git clone https://github.com/vincentmakes/ComfoSense-Touch
cd ComfoSense-Touch

# Create secrets.h from template
cp include/secrets_template.h include/secrets.h
# Edit secrets.h with your configuration

# Build project
pio run

# Upload to ESP32-S3
pio run -t upload

# Monitor serial output
pio device monitor
```

## Serial Output & Debugging

Expected serial output shows:
- Touch polling counter (validates 200Hz rate)
- Button press events with debounce status
- CAN message decode (in Normal Mode)
- MQTT subscriptions and publishes
- Display update confirmations
- NTP time sync status

```
Touch read counter: 1234567 (validates manual polling is working)
BSpeedPlus button clicked! (debounce OK)
GUI: Updating fan speed display - speed=2, boost=0
GUI: Fan speed display updated (Strategy 5 - INSTANT)
ControlManager: Sending CAN command: ventilation_level_2
1F034051 00 84 15 01 02 00 00 00    <- Only ONE CAN frame per press!
```

## Known Working Features

✅ Touch input - buttons respond correctly  
✅ Event handlers execute properly  
✅ State tracking (fan speed updates)  
✅ WiFi connection  
✅ NTP time synchronization  
✅ CAN communication (Normal Mode)  
✅ MQTT client  
✅ Image updates - fan speed images change on screen  
✅ Label text updates - time/date display correctly using Strategy 5  
✅ Per-button debouncing prevents double commands  
✅ Instant button feedback with background CAN transmission  
✅ Remote Client Mode for MQTT-only operation  

## Critical Development Notes

### What NOT to Change
1. **Touch polling frequency**: 5ms (200Hz) is required for responsiveness
2. **Touch initialization order**: Must register AFTER GUI_init()
3. **Manual touch polling**: `lv_indev_read()` must be called in main loop
4. **Strategy 5 pattern**: Required for label text updates
5. **Per-button debounce timestamps**: Each button needs independent tracking

### Watchdog Safety
- `lv_refr_now()` is safe from main loop via `GUI_request_display_refresh()`
- NEVER call `lv_refr_now()` from interrupt handlers or CAN callbacks
- Keep CAN processing fast to avoid blocking touch polling

### C/C++ Interface
- GUI is C code (SquareLine Studio generated)
- Managers are C++ classes
- Use `extern "C"` wrappers for callbacks from GUI to C++

```cpp
extern "C" {
    void control_manager_increase_fan_speed();
    void GUI_update_fan_speed_display_from_cpp(uint8_t speed, bool boost);
}
```

## Testing Checklist

When making changes, verify:
- [ ] Buttons respond within 50ms
- [ ] No duplicate CAN commands (check serial)
- [ ] Time/date updates every second
- [ ] Touch polling counter increments steadily
- [ ] No watchdog timeouts
- [ ] Display doesn't freeze
- [ ] CAN RX rate shown every 10 seconds (Normal Mode)
- [ ] MQTT subscriptions work (check with mosquitto_sub)

## Common Issues & Solutions

### Issue: Touch Not Working
**Solution**: Check initialization order. Touch must be registered AFTER GUI_init(). Verify manual polling in loop.

### Issue: Labels Don't Update
**Solution**: Use Strategy 5 pattern - call `GUI_request_display_refresh()` before `lv_obj_invalidate()`.

### Issue: Duplicate CAN Commands
**Solution**: Increase debounce time or verify per-button tracking is working. Check serial for "BLOCKED" messages.

### Issue: Sluggish Button Response
**Solution**: Verify display update happens BEFORE CAN send in control_manager.cpp.

### Issue: Watchdog Timeout
**Solution**: Never call `lv_refr_now()` from interrupts. Keep CAN callbacks fast.

### Issue: Remote Client Mode Not Working
**Solution**: Verify `REMOTE_CLIENT_MODE true` in secrets.h and MQTT broker is reachable. Check serial for MQTT subscriptions.

## References

- **Hardware Docs**: `Schematics_ESP32S3TouchLCD4_V3_0.pdf`
- **Touch Example**: `DemoCode_03_Drawing_points.ino`
- **Complete History**: `PROJECT_MEMORY_UPDATED.md`
- **Debounce Solution**: `DEBOUNCE_FIX_FINAL.md`
- **LVGL Docs**: https://docs.lvgl.io/9.1/
- **ESP32-S3 Docs**: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/

## Contributing

When modifying the code:
1. Test touch responsiveness thoroughly
2. Verify no watchdog timeouts occur
3. Check serial output for duplicate commands
4. Update this documentation if adding new features
5. Follow existing patterns (especially Strategy 5 for labels)
6. Maintain per-button debounce timestamps

---

**Last Updated**: Based on latest project state with working touch, instant button feedback, and Strategy 5 label updates.
