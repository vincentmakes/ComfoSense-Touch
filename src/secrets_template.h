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
