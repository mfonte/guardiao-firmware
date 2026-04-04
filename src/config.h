// Define the maximum sizes for the email and password data.
const int EMAIL_ADDR = 0;         // Initial address for the email
const int PASS_ADDR = 100;        // Initial address for the password
const int DEVICE_NAME_ADDR = 200; // Initial address for the device name

String DEVICE_UUID;
String DEVICE_NAME;
String USER_EMAIL;
String USER_PASSWORD;
String newEmail;
String newPassword;
String newDeviceName;
String WIFI_SSID;
String STA_NAME;
String HOST_NAME;  // OTA Configuration and Wi-Fi Ap STA

// Firebase RTDB configuration
#define API_KEY "AIzaSyBcXGVkyUs7Nj2o5y9_GP7xhMCcQQdg11U"  // Insert Firebase project API Key
#define DATABASE_URL "https://iotemp-sensor-default-rtdb.firebaseio.com/"  // Insert RTDB URL

// Board configuration
#define SDA D2 // Display SDA pin
#define SCL D1 // Display SCL Pin
#define ONE_WIRE_BUS D5  // Data wire is plugged into port 2 on the Arduino
#define BUZZER_PIN D3  // Defining buzzer and button pins
#define BUTTON_S1_PIN D8  // D8 => S1 On TagsApp Board - V2.0
#define BUTTON_S2_PIN D7  // D2 => S2 On TagsApp Board - V2.0
#define GET_DATA_INTERVAL 5000L //  Update temperature locally every 5 seconds
#define BUTTON_S1_HOLD_TIME 5000 // Start Captive Portal after holdinh S1 for 5 seconds
#define BUTTON_S2_HOLD_TIME 10000 // Reset device after holding S2 for 10 seconds
// #define HOST_NAME  "freezer.local" // OTA Configuration
#define ALARM_MESSAGE "ALARM ON"
#define ADC_MODE(ADC_VCC)
#define DEBUG_CODE 1  // Enable (1)/Disable (0) serial debug

// Database child nodes
String deviceNamePath = "/data/deviceName";
String tempPath = "/data/temperature";
String dataloggerTempPath = "/temperature";
String dataloggerTimestampPath = "/timestamp";
String timestampPath = "/data/timestamp";
String configPath = "/config";
String thresholdPath = "/_threshold";
String telegramUserIdsListPath = "/telegramUserIdsList";
String alertPath = "/_alert";
String lowerTempPath = "/lowerTemp";
String higherTempPath = "/higherTemp";
String alertStatePath = "/state";
String alertTempPath = "/temp";

// Variable to save current epoch time
int timestamp;
int watchDogCount = 0;

int countMessageSending = 0;
int countDataSentToFireBase = 0;

// Timer variables (send new readings every three minutes)
unsigned long sendDataPrevMillis = 0;
unsigned long timerDelay = 300000L; // in milesseconds
unsigned long buttonS1PressedTime = 0;
unsigned long buttonS2PressedTime = 0;

// Initilizing variables
float temperature = -127.0;
float higherTemp = 80.0;
float lowerTemp = -80.0;
uint32_t BATTERY_LEVEL;

// Telegram BOT Setup
// #define MYTZ "BRT3BRST,M10.2.0,M2.3.0"
// const char* token =  "7131086772:AAFGlQy75dvIaMZ-rytZjHgF_OespHlOU3s";
// int64_t telegramAdminUser = 1671905173;
