// ============================================================================
// Wifi Configuration
// ============================================================================
#define WIFI_SSID   "*****"
#define WIFI_PASS   "*****"


// ============================================================================
// MQTT Configuration
// ============================================================================

// set to false to disable MQTT altogether. MQTT is used for integration with 
// home automation system such as Home Assistant
#define MQTT_ENABLED true

#define MQTT_HOST   "******"
#define MQTT_PORT   1883
#define MQTT_USER   "*****"
#define MQTT_PASS   "*****"
#define MQTT_PREFIX "comfoair"

// ============================================================================
// Remote Client Mode Configuration
// ============================================================================

// Remote Client Mode - device acts as MQTT-only client
// When enabled:
//   - CAN bus is NOT initialized (no direct MVHR communication)
//   - All data is received via MQTT from a separate bridge device
//   - Commands are sent via MQTT (not directly to CAN bus)
//   - Time sync with MVHR is disabled
//   - Device relies entirely on MQTT broker for communication
// When disabled:
//   - Normal operation with direct CAN bus communication
//   - MQTT is optional and used in parallel with CAN
#define REMOTE_CLIENT_MODE false

// NOTE: When REMOTE_CLIENT_MODE is true, MQTT_ENABLED is automatically forced to true

// ============================================================================
// Time Configuration
// ============================================================================

// Timezone configuration for NTP time display
// Use POSIX timezone format (ESP32 doesn't support IANA names like "Europe/Zurich")
//
// using Linux/Macos, one can use "cat /usr/share/zoneinfo/Europe/Zurich | strings | tail -1"
// in order to convert IANA to POSIX (replace Europe/Zurich with your IANA identifier)
//
//
// Common European timezones:
// - Central European Time (CET/CEST): "CET-1CEST,M3.5.0,M10.5.0/3"
//   (Zurich, Paris, Berlin, Rome, Madrid, etc.)
// - Western European Time (WET/WEST): "WET0WEST,M3.5.0/1,M10.5.0"
//   (London, Lisbon)
// - Eastern European Time (EET/EEST): "EET-2EEST,M3.5.0/3,M10.5.0/4"
//   (Athens, Helsinki, Bucharest)
//
// Common US timezones:
// - Eastern: "EST5EDT,M3.2.0,M11.1.0"
// - Central: "CST6CDT,M3.2.0,M11.1.0"
// - Mountain: "MST7MDT,M3.2.0,M11.1.0"
// - Pacific: "PST8PDT,M3.2.0,M11.1.0"
//
// No DST (examples):
// - "UTC0" (UTC, no DST)
// - "JST-9" (Japan Standard Time, UTC+9, no DST)
// - "IST-5:30" (India Standard Time, UTC+5:30, no DST)
//
// Format explanation: "STD offset DST,start_rule,end_rule"
// - STD: Standard time name (e.g., CET)
// - offset: Hours from UTC (negative for east, e.g., -1 means UTC+1)
// - DST: Daylight saving time name (e.g., CEST)
// - start_rule: When DST starts (M3.5.0 = last Sunday of March at 2am)
// - end_rule: When DST ends (M10.5.0/3 = last Sunday of October at 3am)
#define TIMEZONE "CET-1CEST,M3.5.0,M10.5.0/3"

// ============================================================================
// Night Time Mode (NTM) Configuration
// ============================================================================

// Customize as of when the warning sign appears before having to change the filters

#define WARNING_THRESHOLD_DAYS 21

// ============================================================================
// Night Time Mode (NTM) Configuration
// ============================================================================

// Enable/disable Night Time Mode entirely
// When disabled, screen stays on 24/7
#define NTM_ENABLED true

// Permanent Night Mode - screen is always in NTM (ignores time window)
// Useful for installations where screen should normally be off
// When true: screen off by default, touch wakes it for NTM_WAKE_DURATION_SEC
// When false: NTM only active during NTM_START_HOUR to NTM_END_HOUR window
#define NTM_PERMANENT false

// Night Time Mode window (24-hour format, 0-23)
// Screen turns off during this time window
// Example: 22 to 6 means screen off from 22:00 (10 PM) to 06:00 (6 AM)
#define NTM_START_HOUR 22
#define NTM_END_HOUR 6

// Wake duration in seconds when screen is touched during NTM
// After this time, screen automatically turns off again
// Example: 180 = 3 minutes
#define NTM_WAKE_DURATION_SEC 30

// ============================================================================
// Screen Dimming Configuration
// ============================================================================

// Enable/disable PWM brightness dimming
// Hardware: Need two 0402 SMD resistors (R40=0 and R36=91k)
// Check repo for more information
// When enabled, software PWM at 60Hz is generated via I2C writes to TCA9554
// When disabled, screen runs at full brightness
#define DIMMING_ENABLED true

// Default brightness level (0-100) 
// 0 = darkest, 100 = brightest (relatively speaking - please don't expect full range of brightness)
// Recommended: 0 for comfortable viewing
#define DIMMING_DEFAULT_BRIGHTNESS 0

