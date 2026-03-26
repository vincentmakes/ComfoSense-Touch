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

## Approach: Mono-Repo with Separate PlatformIO Environments

Single repo keeps everything together. The shared library lives in `shared/` (NOT `lib/`),
which PlatformIO ignores by default — zero impact on the existing Touch LCD build.
The T5 environment explicitly references it via `lib_extra_dirs = shared`.

### Proposed Structure

```
ComfoSense-Touch/                    (single repo)
  platformio.ini                     # Existing env untouched, T5 env appended
  src/                               # Existing Touch LCD source (UNTOUCHED)
    main.cpp
    comfoair/                        # Original data managers (with direct GUI calls)
    ui/
    mqtt/
    wifi/
    ...
  src_t5/                            # T5-specific source (separate directory)
    main.cpp                         # T5 setup/loop, MQTT wiring, display callbacks
    board_config.h                   # T5 pin definitions
    epaper_driver.cpp                # E-paper display driver + LVGL flush
    ui/                              # New LVGL UI for 960x540 split layout
    weather/                         # Weather data (new feature)
  shared/ComfoSense-Shared/          # Shared library (invisible to existing build)
    library.json
    comfoair/                        # Refactored data managers (callback pattern)
      sensor_data.h/cpp
      control_manager.h/cpp
      filter_data.h/cpp
      error_data.h/cpp
    mqtt/mqtt.h/cpp                  # MQTT client (runtime config)
    wifi/wifi.h/cpp                  # WiFi (runtime config)
    time/time_manager.h/cpp          # NTP time sync
```

### Why This Works Without Disturbing Existing Code

1. **`shared/` is invisible**: PlatformIO only auto-scans `lib/` — `shared/` is ignored unless explicitly referenced via `lib_extra_dirs`
2. **`src_t5/` is invisible**: The existing `[env:esp32s3]` compiles `src/` only. The T5 env uses a custom `build_src_dir` or `build_src_filter` pointing to `src_t5/`
3. **No existing files modified**: The original `src/comfoair/*.cpp` files keep their direct LVGL calls. The shared library has its own copies with callbacks instead.
4. **`pio run` unchanged**: Default env is `esp32s3`, compiles exactly as before

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

### Phase 1: Create shared library (DONE)
1. ~~Create shared library with refactored data managers (callback pattern)~~ ✓
2. ~~Copy + refactor mqtt, wifi, time modules (runtime config)~~ ✓
3. ~~Place in `shared/ComfoSense-Shared/` (invisible to existing build)~~ ✓
4. ~~Verify existing `pio run` is unaffected~~ ✓

### Phase 2: T5 basic firmware (DONE)
1. ~~Add `[env:t5-epaper]` to `platformio.ini` (append only)~~ ✓
2. ~~Create `src/t5/main.cpp` with T5 setup~~ ✓
3. ~~Get e-paper driver (epdiy HL API) working with LVGL~~ ✓
4. ~~Wire up WiFi + MQTT using shared library~~ ✓

### Phase 3: Ventilation panel (DONE)
1. ~~Build the left-half ventilation UI in LVGL (grayscale)~~ ✓
2. ~~Convert all LCD images to grayscale (fan0-3, boost, +/-, WiFi, warning)~~ ✓
3. ~~Wire data managers → UI via callbacks~~ ✓
4. ~~Wire touch controls (fan speed +/-, boost, mode dropdown)~~ ✓

### Phase 4: Info panel (DONE)
1. ~~Add large time/date display (right half)~~ ✓
2. ~~Add weather integration (MQTT from HA: condition, temp, humidity, wind)~~ ✓
3. ~~Refresh strategy already in place (30min full refresh in epaper_driver)~~ ✓

### Phase 5: Polish
1. Full refresh scheduling (every ~30 min) to prevent ghosting
2. Error states and connection loss handling
3. Screen layout fine-tuning and grayscale icon polish

---

## Decisions Made

- **Code sharing**: Mono-repo with `shared/ComfoSense-Shared/` (invisible to existing build via PlatformIO directory isolation)
- **Weather**: Via MQTT from Home Assistant (subscribe to HA weather topics)
- **Power**: USB-powered (always on) — no deep sleep complexity
- **Hardware**: T5 4.7" V2.3 with touch (confirmed ESP32-S3 + GT911)
- **Visual style**: Same graphical elements (icons, layout) adapted to grayscale

## Bonus: GT911 Touch Reuse

Since both boards use GT911, the touch read callback logic from `main.cpp` (lines 219-305) can be largely reused — just different I2C pins and screen dimensions (960x540 vs 480x480). The state machine (press detection, release hysteresis, debounce) transfers directly.

---

## Verification

- Phase 1: `pio run` (default env `esp32s3`) still builds identically ✓
- Phase 2: `pio run -e t5-epaper` builds, T5 displays test pattern, connects to WiFi/MQTT
- Phase 3: Fan speed changes from touch reflected in MQTT, MQTT state updates shown on screen
- Phase 4: Time updates, weather displays correctly
- Phase 5: Device survives 24h+ without ghosting (periodic full refresh working)
