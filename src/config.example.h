#pragma once
// Local firmware configuration template.
// Copy to `src/config.h` and fill in with real environment values.

// EEPROM address map for per-device persistent data
const int EMAIL_ADDR      = 0;   // Email string start address
const int PASS_ADDR       = 100; // Password string start address
const int DEVICE_NAME_ADDR = 200; // Device name string start address
const int LDID_ADDR       = 300; // Logical Device ID string start address

// ---- Runtime state (populated at boot from EEPROM / WiFi / Firebase) ----
String DEVICE_UUID;   // ESP chip ID (unique per board)
String DEVICE_NAME;   // Human-readable device name (stored in EEPROM)
String DEVICE_LDID;   // Logical Device ID — immutable once generated
String USER_EMAIL;    // Firebase auth email (stored in EEPROM)
String USER_PASSWORD; // Firebase auth password (stored in EEPROM)
String newEmail;      // Staging variable used by WiFiManager save callback
String newPassword;
String newDeviceName;
String WIFI_SSID;     // Connected SSID (filled after WiFi connects)
String STA_NAME;      // Hostname / STA label used during portal setup
String HOST_NAME;     // OTA hostname ("guardiao-<chipId>")

// ---- Firmware metadata ----
#define FIRMWARE_VERSION    "0.1.0"
#define HEARTBEAT_INTERVAL  300000L  // Heartbeat interval: 5 minutes (ms)

// ---- Firebase RTDB credentials (replace with real values in config.h) ----
#define API_KEY      "YOUR_FIREBASE_API_KEY"
#define DATABASE_URL "https://your-project-default-rtdb.firebaseio.com/"

// ---- Hardware pin assignments ----
#define SDA            D2  // OLED SDA
#define SCL            D1  // OLED SCL
#define ONE_WIRE_BUS   D5  // DS18B20 data wire
#define BUZZER_PIN     D6  // Passive buzzer (GPIO12, PWM-capable, no boot restriction)
#define BUTTON_S1_PIN  D8  // S1: short = prev frame, hold 5s = WiFi portal
#define BUTTON_S2_PIN  D7  // S2: short = next frame, hold 10s = factory reset

// ---- Timing constants ----
#define GET_DATA_INTERVAL   5000L   // Local sensor read period (ms)
#define BUTTON_S1_HOLD_TIME 5000    // S1 hold duration to open captive portal (ms)
#define BUTTON_S2_HOLD_TIME 10000   // S2 hold duration to factory-reset (ms)

// ---- Alarm ----
#define ALARM_MESSAGE "ALARM ON"  // Text shown on OLED during alarm

// ---- Misc ----
#define MIN_VALID_TIMESTAMP 1704067200L  // 2024-01-01 00:00:00 UTC — NTP sanity floor

// ---- Firebase RTDB path fragments (relative to the device node root) ----
String deviceUUIDPath          = "/deviceUUID";
String deviceNamePath          = "/data/deviceName";
String tempPath                = "/data/temperature";
String timestampPath           = "/data/timestamp";
String statusPath              = "/data/deviceStatus";
String dataloggerTempPath      = "/temperature";
String dataloggerTimestampPath = "/timestamp";
String configPath              = "/config";

// ---- Threshold runtime variables (overwritten by Firebase on each sync) ----
float  higherTemp    = 80.0;
float  lowerTemp     = -80.0;
// thresholdMode values: "both" | "upperOnly" | "lowerOnly" | "none"
String thresholdMode = "both";

// ---- Scheduled readings (overwritten by Firebase on each sync) ----
// intervalMinutes == 0 means "not configured — use default 5 min"
int scheduledIntervalMinutes = 0;
int scheduledStartHour       = 0;

// ---- Firebase send interval (ms, updated dynamically from remote config) ----
unsigned long timerDelay = 300000L;  // default 5 minutes

// ---- Runtime counters / state ----
unsigned long timestamp            = 0;   // Current epoch time from NTP
int           watchDogCount        = 0;
int           countMessageSending  = 0;
int           countDataSentToFireBase = 0;

// ---- Timer bookkeeping ----
unsigned long sendDataPrevMillis  = 0;
unsigned long lastHeartbeatMillis = 0;
uint8_t       resetReason         = 1;  // 1=power, 2=watchdog, 3=OTA, 4=manual

// ---- Sensor / ADC ----
float    temperature  = -127.0;  // Last DS18B20 reading (888.0 = sensor error)
uint32_t BATTERY_LEVEL = 0;      // Raw VCC reading from ESP.getVcc() (mV * 1000)
