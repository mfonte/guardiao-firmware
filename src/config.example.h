// Local firmware configuration template.
// Copy to `src/config.h` and fill in with real environment values.

// Define the maximum sizes for the email and password data.
const int EMAIL_ADDR = 0;         // Initial address for the email
const int PASS_ADDR = 100;        // Initial address for the password
const int DEVICE_NAME_ADDR = 200; // Initial address for the device name
const int LDID_ADDR = 300;        // Initial address for LDID (Logical Device ID)
const int LDID_SIZE = 64;         // Max LDID size

String DEVICE_UUID;
String DEVICE_NAME;
String DEVICE_LDID; // Logical Device ID (persistent, immutable)
String USER_EMAIL;
String USER_PASSWORD;
String newEmail;
String newPassword;
String newDeviceName;
String WIFI_SSID;
String STA_NAME;
String HOST_NAME; // OTA Configuration and Wi-Fi Ap STA

// Firmware version
#define FIRMWARE_VERSION "0.1.0"
#define HEARTBEAT_INTERVAL 300000L // 5 minutes

// Firebase RTDB configuration
#define API_KEY "SUA_FIREBASE_API_KEY"
#define DATABASE_URL "https://seu-projeto-default-rtdb.firebaseio.com/"

// Board configuration
#define SDA D2                    // Display SDA pin
#define SCL D1                    // Display SCL pin
#define ONE_WIRE_BUS D5           // DS18B20 data wire
#define BUZZER_PIN D6             // Passive buzzer — GPIO12, supports PWM, no boot restriction
#define BUTTON_S1_PIN D8          // D8 => S1 On TagsApp Board - V2.0
#define BUTTON_S2_PIN D7          // D7 => S2 On TagsApp Board - V2.0
#define GET_DATA_INTERVAL 5000L   // Local sensor read every 5 seconds
#define BUTTON_S1_HOLD_TIME 5000  // Hold S1 for 5 s to open the Captive Portal
#define BUTTON_S2_HOLD_TIME 10000 // Hold S2 for 10 s to factory-reset the device
#define ALARM_MESSAGE "ALARM ON"
#define ADC_MODE(ADC_VCC)
#define DEBUG_CODE 1 // Enable (1)/Disable (0) serial debug

// Firebase RTDB — paths relative to the device node root
String deviceUUIDPath = "/deviceUUID"; // device root (not under config/)
String deviceNamePath = "/data/deviceName";
String tempPath = "/data/temperature";
String timestampPath = "/data/timestamp";
String statusPath = "/data/deviceStatus";
String dataloggerTempPath = "/temperature";
String dataloggerTimestampPath = "/timestamp";
String configPath = "/config";
String thresholdPath = "/_threshold";
String alertPath = "/_alert";

// Threshold variables (read from Firebase on each sync cycle)
float higherTemp = 80.0;
float lowerTemp = -80.0;
String thresholdMode = "both"; // "both" | "above" | "below" | "none"

// Scheduled readings variables (read from Firebase)
bool scheduledReadingsEnabled = false;
int scheduledIntervalMinutes = 5; // padrão: 5 minutos
int scheduledStartHour = 0;

// Firebase send interval (ms, updated dynamically from remote config)
unsigned long timerDelay = 300000L; // padrão: 5 minutos

// Variable to hold the current epoch timestamp
int timestamp = 0;
int watchDogCount = 0;

int countMessageSending = 0;
int countDataSentToFireBase = 0;

// Timer variables
unsigned long sendDataPrevMillis = 0;
unsigned long lastHeartbeatMillis = 0;
unsigned long buttonS1PressedTime = 0;
unsigned long buttonS2PressedTime = 0;
uint8_t resetReason = 1; // 1=power, 2=watchdog, 3=OTA, 4=manual

// Sensor
float temperature = -127.0;
float higherTempRead = 80.0;
float lowerTempRead = -80.0;
uint32_t BATTERY_LEVEL;
