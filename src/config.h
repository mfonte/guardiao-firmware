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
#define SDA D2            // Display SDA pin
#define SCL D1            // Display SCL pin
#define ONE_WIRE_BUS D5   // DS18B20 data wire
#define BUZZER_PIN D6     // Buzzer passivo — GPIO12, suporta PWM, sem restrição de boot
#define BUTTON_S1_PIN D8  // D8 => S1 On TagsApp Board - V2.0
#define BUTTON_S2_PIN D7  // D7 => S2 On TagsApp Board - V2.0
#define GET_DATA_INTERVAL 5000L        // Leitura local a cada 5 segundos
#define BUTTON_S1_HOLD_TIME 5000       // Segurar S1 por 5s abre Captive Portal
#define BUTTON_S2_HOLD_TIME 10000      // Segurar S2 por 10s reseta o device
#define ALARM_MESSAGE "ALARM ON"
#define ADC_MODE(ADC_VCC)
#define DEBUG_CODE 1  // Enable (1)/Disable (0) serial debug

// Firebase RTDB — paths relativos à raiz do device node
String deviceUUIDPath    = "/deviceUUID";          // raiz do device (não sob config/)
String deviceNamePath    = "/data/deviceName";
String tempPath          = "/data/temperature";
String timestampPath     = "/data/timestamp";
String statusPath        = "/data/deviceStatus";
String dataloggerTempPath      = "/temperature";
String dataloggerTimestampPath = "/timestamp";
String configPath        = "/config";
String thresholdPath     = "/_threshold";
String alertPath         = "/_alert";

// Variáveis de threshold (lidas do Firebase)
float higherTemp   = 80.0;
float lowerTemp    = -80.0;
String thresholdMode = "both";  // "both" | "above" | "below" | "none"

// Variáveis de medições programadas (lidas do Firebase)
bool  scheduledReadingsEnabled  = false;
int   scheduledIntervalMinutes  = 5;   // padrão: 5 minutos
int   scheduledStartHour        = 0;

// Variável de tempo para envio ao Firebase (em ms, atualizada dinamicamente)
unsigned long timerDelay = 300000L;  // padrão: 5 minutos

// Variável para guardar o epoch time atual
int timestamp = 0;
int watchDogCount = 0;

int countMessageSending = 0;
int countDataSentToFireBase = 0;

// Timer variables
unsigned long sendDataPrevMillis = 0;
unsigned long buttonS1PressedTime = 0;
unsigned long buttonS2PressedTime = 0;

// Sensor
float temperature = -127.0;
float higherTempRead = 80.0;
float lowerTempRead  = -80.0;
uint32_t BATTERY_LEVEL;
