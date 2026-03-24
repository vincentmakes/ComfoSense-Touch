# CLAUDE.md

## Project Overview
ESP32-S3 firmware for ComfoAir Q ventilation (MVHR) control via CAN bus, with touch LCD display (LVGL) and MQTT integration. Supports two modes: normal (CAN-connected bridge) and remote client (MQTT-only display).

## Build
```bash
pio run                    # Build (PlatformIO)
pio run -t upload          # Flash
pio device monitor         # Serial monitor
```

## Architecture

### Modes
- **Normal mode** (`REMOTE_CLIENT_MODE=false`): Direct CAN bus to MVHR, publishes state to MQTT, receives commands from MQTT/touch
- **Remote client mode** (`REMOTE_CLIENT_MODE=true`): No CAN, receives all data via MQTT, sends commands via MQTT

### Key Data Flow
```
Touch/HA -> ControlManager -> ComfoAir::sendCommand() -> CAN -> MVHR
MVHR -> CAN -> ComfoAir::loop() decode -> MQTT publish + ControlManager update -> Display
```

### MQTT Topics
- State: `comfoair/<name>` (e.g., `comfoair/fan_speed`) — published by bridge from CAN data
- Commands: `comfoair/commands/<command>` (e.g., `comfoair/commands/ventilation_level_2`) — received from HA

### Important: HA Echo Loop Prevention
HA's MQTT integration echoes commands back when it sees state changes. The bridge has a 2-second dedup window (`last_sent_fan_speed` / `last_fan_speed_command_time` in comfoair.cpp) that prevents these echoes from creating infinite loops. **Both touch and MQTT command paths must update these dedup variables** — otherwise HA echoes from a previous speed change can override the current one (e.g., user presses 1->2->3, HA echo for "2" arrives and reverts to 2).

### MQTT Publishing
- Always publish every decoded CAN value to MQTT (do NOT deduplicate/publish-on-change) — at QoS 0, HA can miss a single publish and never recover until the next change
- Only retain key topics: `fan_speed`, temps, humidity, `temp_profile`, filter, errors
- Retaining ALL topics floods the broker with disk writes and slows command delivery

### CAN Bus
- 50kbps, extended frames, via TWAI driver (GPIO6 TX, GPIO0 RX)
- CAN processing throttled to every 10ms in main loop
- RTR requests for slow data (filter days, operating mode, errors) every 10 minutes

## Key Files
- `src/main.cpp` — Main loop, MQTT subscriptions (client mode), manager wiring
- `src/comfoair/comfoair.cpp` — CAN processing, MQTT command subscriptions, dedup logic
- `src/comfoair/message.cpp` — CAN frame encode/decode, command definitions
- `src/comfoair/control_manager.cpp` — Fan speed/boost/temp profile, touch event handling
- `src/comfoair/commands.h` — CAN command byte arrays
- `src/mqtt/mqtt.cpp` — MQTT client wrapper (PubSubClient)
- `src/ui/` — LVGL UI (C code), touch handling
- `src/secrets.h` — WiFi/MQTT credentials (not committed)

## Common Pitfalls
- `Serial` is actually `LogSerial` (serial_logger.h) which also writes to OTA log buffer — don't remove serial logging without understanding this
- `sendCommand()` in message.cpp previously leaked `new std::vector` per call — use stack allocation instead
- MQTT socket timeout must be low (2s) to avoid blocking main loop (default PubSubClient is 15s)
- The LVGL memory pool is fixed at 128KB with no expansion — avoid unnecessary widget creation/destruction
