# LILYGO T5 4.7" E-Paper Client — Architecture Plan

## Context

We want to build a second ComfoAir remote client using a **LILYGO T5 4.7" V2.3 e-paper display with touch** (960x540, grayscale). The goal is to reuse the ventilation control UI on one half of the screen and add new content (time, weather from HA via MQTT) on the other half. This device is **remote-client only** (MQTT, no CAN), **USB-powered** (always on).

### Confirmed Hardware Specs (T5 4.7" V2.3)
- **MCU**: ESP32-S3-WROOM-1-N16R8 (same family as current project!)
- **Display**: ED047TC1 e-paper, 960x540, 16 gray levels, partial refresh support
- **Touch**: GT911 capacitive, 2-point (same controller as current project!)
- **Memory**: 16MB flash, 8MB PSRAM
- **Connectivity**: WiFi 802.11 b/g/n, BLE 5.0
- **Note**: E-paper cannot do partial refresh indefinitely — periodic full refresh required to prevent ghosting/damage

---

## Recommendation: New Repo with Shared Library (via git submodule)

### Why not mono-repo?

The current codebase has **deep hardware coupling** that makes a single-repo multi-environment approach more painful than helpful:

| Aspect | Current (Touch LCD) | T5 E-Paper |
|---|---|---|
| Display | 480x480 RGB565 LCD (ST7701, parallel RGB) | 960x540 4-bit grayscale e-paper (ED047TC1) |
| Touch | GT911 I2C (SDA=15, SCL=7) | GT911 I2C (different pins) |
| Color depth | 16-bit RGB565 | 4-bit grayscale (16 shades) |
| Refresh | 30 FPS partial | Full refresh ~2s, partial ~0.3s |
| LVGL config | Animations, shadows, gradients | Simplified styles, animations kept but slower |
| Board config | V3/V4 auto-detect, IO expander | Completely different pinout, no IO expander |
| Screen manager | Backlight PWM, dimming, sleep | No backlight, e-paper retains image |
| Assets | Color PNGs, RGB styles | Grayscale icons, high-contrast styles |

The **only reusable code** is the data/networking layer, which is a clean subset.

### Proposed Structure

```
ComfoSense-Shared/          (git submodule, shared library)
  comfoair/
    sensor_data.h/cpp        # SensorDataManager (stripped of GUI calls)
    control_manager.h/cpp    # ControlManager (stripped of GUI calls)
    filter_data.h/cpp        # FilterDataManager (stripped of GUI calls)
    error_data.h/cpp         # ErrorDataManager (stripped of GUI calls)
  mqtt/
    mqtt.h/cpp               # MQTT client wrapper
  wifi/
    wifi.h/cpp               # WiFi + WiFiManager
  time/
    time_manager.h/cpp       # NTP time sync

ComfoSense-Touch/            (existing repo, unchanged for now)
  lib/ComfoSense-Shared/     (submodule)
  src/                       (LCD-specific UI, board config, CAN, main.cpp)

ComfoSense-T5/               (new repo)
  lib/ComfoSense-Shared/     (submodule)
  src/
    main.cpp                 # T5-specific setup/loop
    board_config.h           # T5 pin definitions
    epaper_driver.cpp        # E-paper display driver + LVGL flush
    ui/                      # New LVGL UI for 960x540 split layout
    weather/                 # Weather data (new feature)
```

### Key Refactoring: Decouple Data Managers from GUI

Currently, each manager's `updateDisplay()` directly calls LVGL C functions:
- `sensor_data.cpp` → `GUI_update_sensor_display()`
- `control_manager.cpp` → `GUI_update_fan_speed_display_from_cpp()`
- `filter_data.cpp` → `GUI_update_filter_display()`

**Solution: Callback pattern.** Each manager gets a `setDisplayCallback()`:

```cpp
// In shared library
class SensorDataManager {
    std::function<void(const SensorData&)> display_callback;
public:
    void setDisplayCallback(std::function<void(const SensorData&)> cb) {
        display_callback = cb;
    }
    void updateDisplay() {
        if (display_callback) display_callback(current_data);
    }
};
```

Each project wires its own UI update functions in `main.cpp`:
```cpp
// ComfoSense-Touch main.cpp
sensorData->setDisplayCallback([](const SensorData& d) {
    GUI_update_sensor_display(d.inside_temp, d.outside_temp, ...);
});

// ComfoSense-T5 main.cpp
sensorData->setDisplayCallback([](const SensorData& d) {
    epaper_update_sensor_panel(d.inside_temp, d.outside_temp, ...);
});
```

---

## T5 E-Paper UI Architecture

### Split-Screen Layout (960x540)

```
+---------------------------+---------------------------+
|     VENTILATION PANEL     |      INFO PANEL           |
|        (~480x540)         |       (~480x540)          |
|                           |                           |
|  [Fan icon]  Speed: 2     |   14:35                   |
|                           |   Thursday, March 26      |
|  Inside:  21.5°C  45%    |                           |
|  Outside:  8.2°C  62%    |   Weather:                |
|                           |   Partly cloudy, 9°C     |
|  Filter: 142 days        |   Wind: 12 km/h NW       |
|                           |                           |
|  Mode: [Normal v]         |   [Other widgets...]     |
|                           |                           |
|  [-]  [+]  [Boost 18m]   |                           |
+---------------------------+---------------------------+
```

### Visual Style: Grayscale Adaptation of Current UI

The **same graphical elements** from the current 480x480 LCD are reused, converted to grayscale:
- Fan speed icons (fan0, fan1, fan2, fan3, fanboost) → grayscale versions of the same images
- +/- button icons → grayscale
- WiFi icon, warning icon → grayscale
- Fonts → same font files (LVGL fonts are already grayscale-compatible)
- Layout structure preserved: top status bar (time/date/filter/WiFi), center (fan icon + temps/humidity), bottom (speed +/- buttons, boost button)

The left panel essentially recreates the current 480x480 UI within ~480x540 (slightly taller — more breathing room), using 16 grayscale shades instead of RGB565 color.

### E-Paper Refresh Strategy

1. **Full refresh** on boot and every ~30 minutes (prevents ghosting/burn-in)
2. **Partial refresh** for data changes (temps, fan speed, time) — ~0.3s per update
3. **LVGL animations kept enabled** but simplified — e-paper partial refresh can handle basic transitions (opacity changes, text updates) at ~3-4 FPS. Smooth animations won't look smooth but icon visibility/hiding transitions will still work.
4. **Batched updates** — collect changes over 5-10s windows, then single partial refresh (avoids excessive refreshes that damage e-paper)
5. **Touch feedback** — brief invert of button area on press

### LVGL Configuration for E-Paper

```c
#define LV_COLOR_DEPTH 8            // 8-bit grayscale (maps to 16 e-paper gray levels)
#define LV_USE_DRAW_SW 1
#define LV_DRAW_SW_COMPLEX 1        // Keep rounded corners etc. (grayscale can handle them)
#define LV_USE_ANIMATION 1          // Keep animations (will be slow but functional)
#define LV_DEF_REFR_PERIOD 250      // 4 FPS max (partial refresh is ~0.3s)
#define LV_DPI_DEF 150              // Higher DPI for e-paper text clarity
```

### Touch on E-Paper

- Same GT911 controller and LVGL input device abstraction
- Same touch state machine (press detection, release hysteresis) from current project
- Slightly more aggressive debounce (200ms+) since display refresh is slower
- Button press/release visual feedback via partial refresh (noticeable but acceptable delay)

---

## What Gets Reused vs New

### Reused Verbatim (via shared library)
- `SensorDataManager` (with callback refactor)
- `ControlManager` (with callback refactor)
- `FilterDataManager` (with callback refactor)
- `ErrorDataManager` (with callback refactor)
- `mqtt.h/cpp` — MQTT client wrapper
- `wifi.h/cpp` — WiFi + WiFiManager
- `time_manager.h/cpp` — NTP time sync

### Reused with Grayscale Conversion
- Fan speed icons (fan0/1/2/3/boost) → converted to grayscale PNGs, same dimensions
- +/- button icons, WiFi icon, warning icon → grayscale versions
- Screen layout structure → same arrangement (status bar / center / controls), fitted into left 480x540 panel
- MQTT remote client subscription logic from main.cpp → same topics, new main.cpp
- Touch event flow → same pattern (event → ControlManager method)
- GT911 touch state machine (press/release/hysteresis) → nearly identical code

### Completely New
- E-paper display driver (ED047TC1 direct drive — LILYGO provides `epd_driver` library)
- Board configuration (T5-specific pins, no IO expander)
- LVGL flush callback for e-paper (grayscale buffer → epd_driver)
- Info panel UI (weather, extended time display)
- Weather data via MQTT (subscribe to HA weather entity topics)
- Periodic full-refresh scheduler (every ~30 min to prevent ghosting)
- Grayscale style definitions (high-contrast, no gradients/shadows)

---

## Implementation Phases

### Phase 1: Extract shared library
1. Create `ComfoSense-Shared` repo
2. Copy + refactor data managers (add display callbacks, remove `#include "ui/GUI.h"`)
3. Copy mqtt, wifi, time modules
4. Wire ComfoSense-Touch to use shared lib (verify nothing breaks)

### Phase 2: T5 basic firmware
1. Create `ComfoSense-T5` repo with PlatformIO for LILYGO T5 4.7"
2. Get e-paper driver working with LVGL (just a hello world)
3. Wire up WiFi + MQTT using shared library

### Phase 3: Ventilation panel
1. Build the left-half ventilation UI in LVGL (grayscale)
2. Create grayscale fan icons
3. Wire data managers → UI via callbacks
4. Test touch controls (fan speed +/-, boost, mode)

### Phase 4: Info panel
1. Add time/date display (right half)
2. Add weather integration
3. Implement refresh strategy (full/partial scheduling)

### Phase 5: Polish
1. Full refresh scheduling (every ~30 min) to prevent ghosting
2. Error states and connection loss handling
3. Screen layout fine-tuning and grayscale icon polish

---

## Decisions Made

- **Code sharing**: Shared git submodule (`ComfoSense-Shared`)
- **Weather**: Via MQTT from Home Assistant (subscribe to HA weather topics)
- **Power**: USB-powered (always on) — no deep sleep complexity
- **Hardware**: T5 4.7" V2.3 with touch (confirmed ESP32-S3 + GT911)
- **Visual style**: Same graphical elements (icons, layout) adapted to grayscale

## Bonus: GT911 Touch Reuse

Since both boards use GT911, the touch read callback logic from `main.cpp` (lines 219-305) can be largely reused — just different I2C pins and screen dimensions (960x540 vs 480x480). The state machine (press detection, release hysteresis, debounce) transfers directly.

---

## Verification

- Phase 1: `pio run` on ComfoSense-Touch still builds and works identically after shared lib extraction
- Phase 2: T5 displays test pattern on e-paper, connects to WiFi/MQTT
- Phase 3: Fan speed changes from touch reflected in MQTT, MQTT state updates shown on screen
- Phase 4: Time updates, weather displays correctly
- Phase 5: Device survives 24h+ without ghosting (periodic full refresh working)
