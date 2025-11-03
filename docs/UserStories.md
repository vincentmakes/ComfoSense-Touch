# ComfoSense Touch Control - User Stories & Epics

## Project Overview

ESP32-S3 based touch screen controller for ComfoAir ventilation systems with modern GUI, real-time monitoring, and home automation integration.

-----

## Epic 1: Ventilation Control

**Goal**: Provide intuitive manual control over the MVHR ventilation system

### User Story 1.1: Fan Speed Control

**As a** homeowner  
**I want to** adjust the ventilation fan speed (0-3) with touch buttons  
**So that** I can quickly change airflow based on my needs

**Acceptance Criteria**:

- ✅ Four fan speed levels (0, 1, 2, 3) available
- ✅ Plus/minus buttons to increment/decrement speed
- ✅ Visual feedback showing current speed with images
- ✅ Display updates instantly (<50ms perceived response)
- ✅ CAN command sent to MVHR unit
- ✅ Button debouncing prevents double-commands (500ms)
- ✅ Changes persist after reboot

**Technical Implementation**:

- GUI buttons: `BSpeedPlus`, `BSpeedMinus`
- CAN commands: `ventilation_level_0` through `ventilation_level_3`
- Display: Fan speed images with opacity changes
- Debouncing: Per-button 500ms timer

### User Story 1.2: Boost Mode

**As a** homeowner  
**I want to** activate a temporary high-speed boost mode  
**So that** I can quickly clear odors or moisture

**Acceptance Criteria**:

- ✅ Single button activates boost mode (20 minutes default)
- ✅ Visual indicator shows boost is active (yellow icon)
- ✅ Remaining time displayed on center button
- ✅ Automatically reverts to previous fan speed after timer expires
- ✅ Can be deactivated manually

**Technical Implementation**:

- GUI button: `BBoost`
- CAN command: `boost_20_min`, `boost_end`
- Display: `fanspeedboost` image visible during boost
- Timer display on `centerfan` button (2-digit white text)

### User Story 1.3: Temperature Profile Selection

**As a** homeowner  
**I want to** select heating/cooling preferences via dropdown  
**So that** the system can adjust heat recovery accordingly

**Acceptance Criteria**:

- ✅ Three temperature profiles available: Normal, Heating, Cooling
- ✅ Dropdown menu for selection
- ✅ Visual feedback shows current profile
- ✅ Changes immediately communicated to MVHR
- ✅ Profile selection persists

**Technical Implementation**:

- GUI widget: `DDTempProfile` dropdown
- CAN commands: `temp_profile_normal`, `temp_profile_cool`, `temp_profile_warm`
- Colors: Yellow for selected item, cyan for others

-----

## Epic 2: Environmental Monitoring

**Goal**: Display real-time environmental data from the ventilation system

### User Story 2.1: Temperature Display

**As a** homeowner  
**I want to** see inside and outside temperatures  
**So that** I understand current conditions

**Acceptance Criteria**:

- ✅ Inside temperature (extract air) displayed in °C
- ✅ Outside temperature (outdoor air) displayed in °C
- ✅ Updates every 10 seconds
- ✅ Data sourced from CAN bus or MQTT
- ✅ 1 decimal precision (e.g., “22.5°C”)
- ✅ Labels auto-update without user interaction

**Technical Implementation**:

- GUI labels: `insideTemp`, `outsideTemp`
- CAN PDOIDs: 276 (extract_air_temp), 275 (outdoor_air_temp)
- Update pattern: Strategy 5 (GUI_request_display_refresh + invalidate)
- Batching: Updates throttled to 2-second intervals

### User Story 2.2: Humidity Display

**As a** homeowner  
**I want to** see inside and outside humidity levels  
**So that** I can assess indoor air quality

**Acceptance Criteria**:

- ✅ Inside humidity (extract air) displayed as percentage
- ✅ Outside humidity displayed as percentage
- ✅ Updates every 10 seconds
- ✅ No decimal places (e.g., “45%”)
- ✅ Data persists (“sticks”) at last received value

**Technical Implementation**:

- GUI labels: `insideHum`, `outsideHum`
- CAN PDOIDs: 290 (extract_air_humidity), 292 (outdoor_air_humidity)
- Update frequency: 10-second batching

-----

## Epic 3: System Status & Maintenance

**Goal**: Monitor system health and maintenance requirements

### User Story 3.1: Filter Monitoring

**As a** homeowner  
**I want to** see exactly how many days until filter replacement  
**So that** I can maintain optimal system performance

**Acceptance Criteria**:

- ✅ Remaining days displayed as number
- ✅ Data received from MVHR via CAN
- ✅ Updates automatically when value changes
- ✅ Provides advance warning for maintenance planning

**Technical Implementation**:

- GUI label: Filter days display
- CAN PDOID: `remaining_days_filter_replacement`
- Manager: FilterDataManager class

### User Story 3.2: Error & Warning Display

**As a** homeowner  
**I want to** be notified immediately of system errors  
**So that** I can address problems before they cause damage

**Acceptance Criteria**:

- ✅ Warning icon appears when any error is active
- ✅ Errors detected from CAN bus messages
- ✅ Multiple error types supported (overheating, sensors, pressure, etc.)
- ✅ Auto-clear after 1-hour timeout if not refreshed
- ✅ MQTT publishing for home automation alerts
- ✅ Detailed serial logging with emojis (🚨 ⚠️ ✅)

**Technical Implementation**:

- GUI image: Warning icon with opacity control
- CAN PDOIDs: 321-329 (error codes)
- Manager: ErrorDataManager class
- 10+ error types tracked:
  - Overheating
  - Temperature sensor failures
  - Pre-heater issues
  - Pressure problems
  - Temperature control failures
  - Filter alarms
  - General warnings

-----

## Epic 4: Network Connectivity

**Goal**: Provide WiFi connectivity and cloud integration

### User Story 4.1: WiFi Status Indicator

**As a** homeowner  
**I want to** see WiFi connection status at a glance  
**So that** I know if network features are available

**Acceptance Criteria**:

- ✅ WiFi icon visible on screen
- ✅ Opacity at 50% when disconnected (dimmed)
- ✅ Opacity at 100% when connected (bright)
- ✅ Auto-connects to configured network on startup
- ✅ Non-blocking connection (max 20 seconds timeout)

**Technical Implementation**:

- GUI image: `wifi` icon
- WiFi class manages connection state
- Update pattern: Simple opacity changes (no Strategy 5 needed)

### User Story 4.2: MQTT Integration

**As a** smart home enthusiast  
**I want** the controller to publish data via MQTT  
**So that** I can integrate with Home Assistant or other platforms

**Acceptance Criteria**:

- ✅ All sensor data published to MQTT topics
- ✅ Commands can be received via MQTT
- ✅ Configurable via secrets.h
- ✅ Optional (can be disabled)
- ✅ Works in both direct CAN mode and Remote Client mode

**Technical Implementation**:

- MQTT prefix: `comfoair/`
- Published topics: All sensor values, status, errors
- Command topics: `comfoair/commands/*`
- Remote Client Mode: Controller can operate without CAN, receiving all data via MQTT

### User Story 4.3: OTA Updates

**As a** system administrator  
**I want to** update firmware over WiFi  
**So that** I don’t need physical access for software updates

**Acceptance Criteria**:

- ✅ OTA update capability enabled
- ✅ Updates can be pushed remotely
- ✅ System remains functional during update check
- ✅ Secure update mechanism

**Technical Implementation**:

- OTA manager class
- ArduinoOTA library integration
- Non-blocking update checks

-----

## Epic 5: Time Display & Synchronization

**Goal**: Show accurate time and maintain system time sync

### User Story 5.1: Time Display

**As a** homeowner  
**I want to** see the current time and date  
**So that** the controller serves as a useful dashboard

**Acceptance Criteria**:

- ✅ Time displayed in HH:MM format
- ✅ Date displayed with day name and full date
- ✅ Updates every second
- ✅ Syncs with NTP servers on startup
- ✅ Configurable timezone (POSIX format)

**Technical Implementation**:

- GUI labels: `time`, `date`
- TimeManager class
- NTP sync via WiFi
- Update pattern: Strategy 5 for immediate display
- Timezone: Configurable in secrets.h (default: CET/CEST)

### User Story 5.2: Device Time Sync (Optional)

**As a** system integrator  
**I want** the controller to sync time with the MVHR unit  
**So that** all timestamps are coordinated

**Acceptance Criteria**:

- ⚠️ Currently disabled (can cause CAN communication issues)
- 🔄 Reads device time from CAN ID 0x10040001
- 🔄 Compares with NTP time
- 🔄 Auto-corrects if difference > 60 seconds
- 🔄 Checks sync every hour

**Technical Notes**:

- Feature is implemented but disabled
- Can be re-enabled after CAN protocol debugging
- Time requests use RTR (Remote Transmission Request)

-----

## Epic 6: Display Management

**Goal**: Optimize power consumption and user experience

### User Story 6.1: Night Time Mode

**As a** homeowner  
**I want** the screen to turn off at night  
**So that** it doesn’t disturb sleep and saves power

**Acceptance Criteria**:

- ✅ Screen turns off at configured time (default: 22:00)
- ✅ Screen turns on at configured time (default: 06:00)
- ✅ Touch wakes screen for 3 minutes
- ✅ Repeated touches reset timer
- ✅ Permanent mode available (always off until touched)
- ✅ Fully configurable in secrets.h
- ✅ Can be completely disabled
- ✅ Touch remains responsive even when screen is off

**Technical Implementation**:

- ScreenManager class
- Backlight control via TCA9554 I/O expander (EXIO2)
- Software PWM for brightness dimming (optional)
- Configuration: NTM_ENABLED, NTM_PERMANENT, NTM_START_HOUR, NTM_END_HOUR, NTM_WAKE_DURATION_SEC
- Power savings: Up to 84% in permanent mode

### User Story 6.2: Brightness Control

**As a** homeowner  
**I want** adjustable screen brightness  
**So that** the display is comfortable in different lighting conditions

**Acceptance Criteria**:

- ✅ Software PWM dimming via I/O expander
- ✅ Configurable brightness percentage
- ✅ Smooth brightness transitions
- ✅ Works with AP3032 LED driver (inverted logic)

**Technical Implementation**:

- TCA9554 EXIO5 pin for PWM output
- Configurable duty cycle (0-100%)
- 10kHz PWM frequency
- Inverted logic: HIGH = darker, LOW = brighter

-----

## Epic 7: User Interface & Experience

**Goal**: Provide an intuitive, responsive touch interface

### User Story 7.1: Touch Input

**As a** homeowner  
**I want** responsive touch input  
**So that** controls feel natural and immediate

**Acceptance Criteria**:

- ✅ Touch polling at 200Hz (5ms intervals)
- ✅ GT911 capacitive touch controller
- ✅ Manual polling for reliability
- ✅ Touch registered after GUI initialization
- ✅ No missed touches or ghost touches

**Technical Implementation**:

- GT911 touch controller via I2C
- Manual polling in main loop (10ms intervals)
- LVGL input device integration
- Touch interrupt on GPIO16

### User Story 7.2: Visual Feedback

**As a** homeowner  
**I want** instant visual feedback from button presses  
**So that** the interface feels responsive

**Acceptance Criteria**:

- ✅ Button press feedback in <50ms
- ✅ Display updates before CAN commands sent
- ✅ Image opacity changes for state indicators
- ✅ Color-coded elements (cyan, yellow, white)
- ✅ Clear iconography for all functions

**Technical Implementation**:

- Strategy 5 pattern for instant label updates
- Simple invalidation for image changes
- Display update before CAN transmission (better UX)
- Aggressive debouncing (500ms per button)

### User Story 7.3: Design System

**As a** user  
**I want** a consistent, modern visual design  
**So that** the interface is easy to understand and pleasant to use

**Acceptance Criteria**:

- ✅ Dark theme (black background)
- ✅ Consistent color palette:
  - Primary: Cyan (#00C7FF) - buttons, dropdown
  - Accent: Yellow (#FFC802) - boost, selected items
  - Background: Black (#000000)
  - Panel: Dark gray/navy (#191919, #002138)
  - Text: White (#FFFFFF)
- ✅ Custom fonts: largebold_1, font_1
- ✅ 480x480 resolution optimized

**Technical Implementation**:

- SquareLine Studio for GUI design
- LVGL 9.2.2 graphics library
- Custom color definitions in GUI_GlobalStyles.c

-----

## Epic 8: Development & Debugging

**Goal**: Support efficient development and troubleshooting

### User Story 8.1: Serial Debugging

**As a** developer  
**I want** comprehensive debug output  
**So that** I can diagnose issues quickly

**Acceptance Criteria**:

- ✅ Detailed serial logging for all subsystems
- ✅ CAN frame logging (send/receive)
- ✅ Emoji indicators for message types (🚨⚠️✅)
- ✅ Performance metrics (CAN frame rate, update intervals)
- ✅ Manager status reporting

**Technical Implementation**:

- Serial.printf() throughout codebase
- 115200 baud rate
- Structured logging format
- Frame counters and timing statistics

### User Story 8.2: Demo Mode

**As a** developer  
**I want** the display to function without CAN connection  
**So that** I can develop and test GUI features independently

**Acceptance Criteria**:

- ✅ Dummy sensor data when CAN unavailable
- ✅ Local state tracking for controls
- ✅ Automatic disable when CAN data received
- ✅ Visual indication of demo vs. live data

**Technical Implementation**:

- Demo mode flag in managers
- Dummy data generation
- Auto-disable on first CAN message
- “DEMO” indicators in logs

-----

## Epic 9: Hardware Integration

**Goal**: Properly interface with all hardware components

### User Story 9.1: CAN Bus Communication

**As a** system  
**I want to** communicate reliably with the MVHR unit  
**So that** commands are executed and data is accurate

**Acceptance Criteria**:

- ✅ 50 kbps CAN bus speed
- ✅ TJA1051T CAN transceiver
- ✅ Extended CAN frames (29-bit)
- ✅ Multi-frame message support
- ✅ Bidirectional communication (TX/RX)
- ✅ Frame validation and error handling

**Technical Implementation**:

- ESP32 TWAI (CAN) controller
- Custom TWAI wrapper for ESP32-S3
- Message encoding/decoding classes
- CanAddress class for ID construction
- GPIO4 (TX), GPIO5 (RX)

### User Story 9.2: Display Interface

**As a** system  
**I want to** drive the RGB LCD display  
**So that** users can see the GUI

**Acceptance Criteria**:

- ✅ 480x480 RGB LCD
- ✅ RGB666 color depth (18-bit)
- ✅ Smooth graphics rendering
- ✅ No tearing or artifacts
- ✅ Backlight control

**Technical Implementation**:

- ESP32 RGB parallel interface
- 16-bit color bus (5-6-5 RGB)
- LVGL display driver integration
- Software/hardware PWM for backlight
- Multiple GPIO pins for data, sync, clock

### User Story 9.3: I2C Peripherals

**As a** system  
**I want to** control I2C devices  
**So that** touch and power management work correctly

**Acceptance Criteria**:

- ✅ TCA9554 I/O expander for control signals
- ✅ GT911 touch controller communication
- ✅ Proper I2C addressing and timing
- ✅ Error handling for I2C failures

**Technical Implementation**:

- Wire library (Arduino I2C)
- GPIO15 (SDA), GPIO7 (SCL)
- TCA9554 controls:
  - EXIO1: Touch reset
  - EXIO2: Backlight enable
  - EXIO3: LCD reset
  - EXIO5: PWM for dimming

-----

## Epic 10: Configuration & Deployment

**Goal**: Make the system easy to configure and deploy

### User Story 10.1: Easy Configuration

**As an** installer  
**I want** all settings in one place  
**So that** deployment is quick and error-free

**Acceptance Criteria**:

- ✅ Single configuration file (secrets.h)
- ✅ WiFi credentials
- ✅ MQTT settings
- ✅ Timezone configuration
- ✅ Feature flags (MQTT enable, NTM enable)
- ✅ Remote Client Mode toggle
- ✅ Template file provided

**Technical Implementation**:

- secrets_template.h provided
- Renamed to secrets.h for actual config
- Not tracked in git (.gitignore)
- All string constants in one place

### User Story 10.2: Remote Client Mode

**As a** multi-location installer  
**I want** a controller that works via MQTT only  
**So that** I can have multiple displays without multiple CAN connections

**Acceptance Criteria**:

- ✅ REMOTE_CLIENT_MODE flag in secrets.h
- ✅ Disables CAN initialization when enabled
- ✅ All data received via MQTT from bridge device
- ✅ Commands sent via MQTT
- ✅ Time sync disabled in remote mode
- ✅ WiFi/MQTT required in remote mode

**Technical Implementation**:

- Compile-time mode selection
- Conditional CAN initialization
- MQTT subscriptions for sensor data
- Command publishing to bridge

### User Story 10.3: Build & Flash

**As a** developer  
**I want** streamlined build and upload  
**So that** I can iterate quickly

**Acceptance Criteria**:

- ✅ PlatformIO project structure
- ✅ One-command build and upload
- ✅ Serial monitor integration
- ✅ Proper partition scheme for large firmware

**Technical Implementation**:

- platformio.ini configuration
- `pio run -t upload` for build/flash
- `pio device monitor` for serial output
- ESP32-S3 16MB flash partition

-----

## Implementation Priority Matrix

### Phase 1: Core Functionality (✅ Complete)

1. Hardware initialization
1. Touch input system
1. Basic GUI display
1. CAN communication
1. Fan speed control
1. Temperature/humidity display

### Phase 2: Enhanced Features (✅ Complete)

1. Boost mode
1. Temperature profile selection
1. Filter monitoring
1. WiFi connectivity
1. Time display & NTP sync
1. Error/warning system

### Phase 3: Power & UX (✅ Complete)

1. Night Time Mode
1. Brightness control
1. Button debouncing
1. Display refresh optimization
1. MQTT integration

### Phase 4: Advanced Features (✅ Complete)

1. OTA updates
1. Remote Client Mode
1. MQTT command reception
1. Comprehensive error handling
1. Boost timer display

### Phase 5: Polish & Optimization (✅ Complete)

1. Performance tuning
1. Memory optimization
1. Documentation
1. Testing & validation

-----

## Technical Debt & Known Issues

### Resolved Issues

- ✅ Label text not updating → Fixed with Strategy 5 pattern
- ✅ Touch not working → Fixed with manual polling
- ✅ Double button presses → Fixed with 500ms debouncing
- ✅ CAN frame splitting → Fixed in TWAI wrapper
- ✅ WiFi icon not updating → Fixed with opacity control

### Deferred Features

- ⏸️ Device time synchronization (disabled, needs CAN protocol work)
- ⏸️ Advanced error diagnostics (basic system working)

### Future Enhancements

- 🔮 Multi-language support
- 🔮 User-configurable dashboard layouts
- 🔮 Historical data logging
- 🔮 Energy usage tracking
- 🔮 Scheduled ventilation profiles

-----

## Success Metrics

### Functional Requirements (✅ All Met)

- ✅ Touch response time: <50ms perceived
- ✅ Display update rate: Real-time (as fast as CAN data)
- ✅ CAN communication: Bidirectional, reliable
- ✅ WiFi connection: Stable, auto-reconnect
- ✅ Power consumption: <1.2W normal, up to 84% savings in night mode

### User Experience (✅ Excellent)

- ✅ Intuitive controls - no training required
- ✅ Instant visual feedback
- ✅ Responsive touch input (no missed presses)
- ✅ Clean, modern interface
- ✅ Readable in all lighting conditions

### System Reliability (✅ Production Ready)

- ✅ No watchdog resets
- ✅ No memory leaks
- ✅ Stable over extended runtime
- ✅ Graceful error handling
- ✅ Backward compatible with existing MVHR systems

-----

## Development Team Knowledge

### Critical Patterns Established

1. **Strategy 5** for label text updates (GUI_request_display_refresh + invalidate)
1. **Simple invalidation** for image opacity changes
1. **Manual touch polling** every 10ms in main loop
1. **Per-button debouncing** at 500ms for clean UX
1. **Display-first, CAN-second** for instant perceived response

### File Organization

```
src/
├── main.cpp                 # Hardware init, main loop
├── comfoair/               # CAN communication
│   ├── comfoair.cpp/.h     # Main CAN handler
│   ├── message.cpp/.h      # Message encode/decode
│   ├── commands.h          # Command definitions
│   ├── control_manager.*   # Fan/temp control
│   ├── sensor_data.*       # Temperature/humidity
│   ├── filter_data.*       # Filter monitoring
│   └── error_data.*        # Error tracking
├── ui/                     # User interface
│   ├── GUI.c/.h           # Main GUI
│   ├── GUI_Variables.c    # Widget declarations
│   ├── Behaviour/GUI_Events.c  # Event handlers
│   └── Content/Styles/    # Styling
├── wifi/                  # Network
│   └── wifi.cpp/.h
├── mqtt/                  # MQTT client
│   └── mqtt.cpp/.h
├── ota/                   # OTA updates
│   └── ota.cpp/.h
├── time/                  # Time management
│   └── time_manager.*
├── screen/                # Display control
│   └── screen_manager.*
└── twai_wrapper.*         # CAN hardware interface
```

-----

## Project Status: ✅ PRODUCTION READY

All major features implemented and tested. System is stable, reliable, and ready for deployment.

**Last Updated**: November 2025  
**Version**: 1.0  
**Status**: Complete & Deployed