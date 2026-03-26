# ComfoSense-Shared

Shared library for ComfoAir ventilation (MVHR) control projects. Provides data managers, MQTT, WiFi, and time management that can be used by different display frontends.

## Usage

Add as a git submodule to your project's `lib/` directory:

```bash
git submodule add https://github.com/vincentmakes/ComfoSense-Shared.git lib/ComfoSense-Shared
```

## Architecture

All managers use a **callback pattern** for display updates. Each host project wires its own UI by setting callbacks:

```cpp
sensorData->setDisplayCallback([](const SensorData& d) {
    // Your project-specific UI update code here
});
```

### Configuration

Instead of compile-time `secrets.h` defines, all modules use runtime configuration:

```cpp
// WiFi
wifi->configure({ .ssid = "MySSID", .pass = "MyPass", .hostname = "my-device" });

// MQTT
mqtt->configure({ .host = "192.168.1.100", .port = 1883, .user = "user", .pass = "pass" });

// Time
timeMgr->setTimezone("CET-1CEST,M3.5.0,M10.5.0/3");

// Control
controlMgr->setRemoteClientMode(true);
controlMgr->setMqttPrefix("comfoair");
```

## Modules

- **comfoair/sensor_data** - Temperature/humidity data management with batched display updates
- **comfoair/control_manager** - Fan speed, boost timer, temp profile with command debouncing
- **comfoair/filter_data** - Filter replacement countdown with configurable warning threshold
- **comfoair/error_data** - MVHR error/alarm state tracking
- **mqtt/** - PubSubClient wrapper with auto-reconnect and subscription retry
- **wifi/** - WiFi management with event-based connection tracking and manual reconnect fallback
- **time/** - NTP time sync with optional device time synchronization

## Projects Using This Library

- [ComfoSense-Touch](https://github.com/vincentmakes/ComfoSense-Touch) - 480x480 LCD touch display
- ComfoSense-T5 (planned) - LILYGO T5 4.7" e-paper display
