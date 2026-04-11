#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <WiFiManager.h>
#include <DallasTemperature.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <EEPROMFunction.h>
#include <drawOled.h>
#include <buzzer.h>
#include <alarm.h>
#include <buttons.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <Ticker.h>
#include <OTAUpdate.h>

// Define Firebase objects
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Initialize Dallas Sensor
DallasTemperature sensors(&oneWire);
DeviceAddress sensorAddress;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");

// millis-based timer for periodic callFirebase()
unsigned long lastFirebaseCallMs = 0;

Ticker wdtReset;

// Database main path (updated in setup with user UID)
String databasePath;

// Variable to save Firebase USER UID
String uid;

FirebaseJson json;
JsonDocument doc;

// Configure ADC mode to read supply voltage
ADC_MODE(ADC_VCC);

// Forward declarations — Protocol v2 functions defined after loop()
void generateOrLoadLDID();
uint8_t calculateStatusBitmap();
void sendBootMessage();
void sendHeartbeat();

/**
 * Writes initial device identity and online status to Firebase RTDB.
 * Called once during setup after Firebase authentication succeeds.
 */
void firebaseInit()
{
  Serial.println("Entering firebaseInit");
  json.clear();
  json.add(deviceNamePath, String(DEVICE_NAME));
  json.add(deviceUUIDPath, String(DEVICE_UUID)); // device root, not under config/
  json.add(statusPath, String("online"));
  Serial.printf("Set json... %s\n", Firebase.RTDB.updateNode(&fbdo, databasePath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
  json.clear();
  Serial.println("Leaving firebaseInit");
}

/**
 * Configures WiFiManager with custom parameters (email, password, device name)
 * read from EEPROM and registers the save-config callback.
 * @return true on success.
 */
bool setupWiFiManager(WiFiManager &wifiManager)
{
  STA_NAME = WiFi.hostname();
  Serial.println(STA_NAME);

  WiFiManagerParameter custom_email("Email", "Enter your email", USER_EMAIL.c_str(), 40);
  WiFiManagerParameter custom_password("Password", "Enter your password", USER_PASSWORD.c_str(), 40, "type='password'");
  WiFiManagerParameter custom_device_name("DeviceName", "Enter the device name", DEVICE_NAME.c_str(), 40);

  wifiManager.addParameter(&custom_email);
  wifiManager.addParameter(&custom_password);
  wifiManager.addParameter(&custom_device_name);

  wifiManager.setSaveConfigCallback([&custom_email, &custom_password, &custom_device_name]()
                                    {
    newEmail = custom_email.getValue();
    newPassword = custom_password.getValue();
    newDeviceName = custom_device_name.getValue();
    if (newEmail != USER_EMAIL || newPassword != USER_PASSWORD || newDeviceName != DEVICE_NAME) {
      USER_EMAIL = newEmail;
      USER_PASSWORD = newPassword;
      DEVICE_NAME = newDeviceName;
      writeStringToEEPROM(EMAIL_ADDR, USER_EMAIL);
      writeStringToEEPROM(PASS_ADDR, USER_PASSWORD);
      writeStringToEEPROM(DEVICE_NAME_ADDR, DEVICE_NAME);
      EEPROM.commit();
      Serial.println("Updated email, password and deviceName saved to EEPROM.");
    } });
  return true;
}

/**
 * Starts WiFiManager in auto-connect mode with a 30-second config portal timeout.
 * If connection fails the device continues without blocking.
 */
void initWifiManager()
{
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
  wifiManager.setConfigPortalTimeout(30);
  wifiManager.setConnectTimeout(60);
  wifiManager.setCleanConnect(true);
  if (!setupWiFiManager(wifiManager))
  {
    Serial.println("Failed to setup WiFi Manager");
    return;
  }
  String portalSSID = "Guardiao-" + String(ESP.getChipId() & 0xFFFF, HEX);
  if (!wifiManager.autoConnect(portalSSID.c_str()))
  {
    Serial.println("Failed to connect and hit timeout");
    return;
  }
}

/**
 * Returns the current UNIX epoch time obtained from the NTP client.
 * @return Epoch time as unsigned long.
 */
unsigned long getTime()
{
  timeClient.update();
  return timeClient.getEpochTime();
}

/**
 * Fetches device configuration from Firebase RTDB (thresholds, scheduled readings)
 * and updates runtime variables. Restarts the device if Firebase is not ready.
 */
void getDeviceConfigurations()
{
  Serial.println("Entering getDeviceConfigurations");

  if (!Firebase.ready())
  {
    Serial.println("Error: Firebase not ready. Restarting.");
    Serial.flush();
    Serial.end();
    ESP.restart();
    return;
  }

  String fullPath = databasePath + configPath;

  if (!Firebase.RTDB.getJSON(&fbdo, fullPath))
  {
    Serial.println("Error: failed to fetch config from Firebase.");
    return;
  }

  if (fbdo.dataType() != "json")
  {
    Serial.println("Error: response data is not JSON.");
    return;
  }

  DeserializationError error = deserializeJson(doc, fbdo.jsonString());
  if (error)
  {
    Serial.print(F("JSON deserialization error: "));
    Serial.println(error.f_str());
    return;
  }

  // --- Thresholds ---
  higherTemp = doc["_threshold"]["higherTemp"] | 80.0f;
  lowerTemp = doc["_threshold"]["lowerTemp"] | -80.0f;
  thresholdMode = doc["_threshold"]["thresholdMode"] | "both";

  Serial.printf("Threshold: lower=%.1f  higher=%.1f  mode=%s\n",
                lowerTemp, higherTemp, thresholdMode.c_str());

  // --- Scheduled readings ---
  // NOTE: isEnabled was never written by the app — derive from intervalMinutes > 0
  scheduledIntervalMinutes = doc["scheduledReadings"]["intervalMinutes"] | 0;
  scheduledStartHour = doc["scheduledReadings"]["startHour"] | 0;

  if (scheduledIntervalMinutes > 0)
  {
    timerDelay = (unsigned long)scheduledIntervalMinutes * 60UL * 1000UL;
    Serial.printf("ScheduledReadings: enabled, interval=%d min\n", scheduledIntervalMinutes);
  }
  else
  {
    timerDelay = 300000UL; // default: 5 minutes
    Serial.println("ScheduledReadings: not configured, using default 5 min");
  }

  Serial.println("Leaving getDeviceConfigurations");
}

/**
 * Reads temperature from the DS18B20 sensor.
 * Sets global sensorError flag on disconnect/short-circuit (-127 or 85).
 * @return Temperature in Celsius (value undefined when sensorError is true).
 */
float readTemperature()
{
  Serial.println("Entering readTemperature");
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if (tempC == -127.0 || tempC == 85.0)
  {
    sensorError = true;
  }
  else
  {
    sensorError = false;
  }
  Serial.println("Leaving readTemperature");
  return tempC;
}

/**
 * Evaluates the current temperature against configured thresholds and fires
 * sendAlarm() if a violation is detected. Respects the thresholdMode setting
 * ("both" | "upperOnly" | "lowerOnly" | "none").
 */
void checkThresholdAlert()
{
  Serial.println("Entering checkThresholdAlert");

  if (sensorError)
  {
    Serial.println("Sensor returned invalid reading!");
    countMessageSending = 0;
    return;
  }

  if (thresholdMode == "none")
  {
    Serial.println("ThresholdMode=none: alarm disabled.");
    countMessageSending = 0;
    return;
  }

  bool overHigh = (thresholdMode == "both" || thresholdMode == "upperOnly") && temperature > higherTemp;
  bool underLow = (thresholdMode == "both" || thresholdMode == "lowerOnly") && temperature < lowerTemp;

  if (overHigh)
  {
    Serial.println("High temperature threshold exceeded!");
    countMessageSending++;
    startAlarm();
  }
  else if (underLow)
  {
    Serial.println("Low temperature threshold exceeded!");
    countMessageSending++;
    startAlarm();
  }
  else
  {
    Serial.println("Temperature within limits.");
    countMessageSending = 0;
  }

  Serial.println("Leaving checkThresholdAlert");
}

/**
 * Watchdog ISR — increments a counter every second. Restarts the device
 * if the main loop has not reset the counter within 80 seconds.
 */
void ISRWatchDog()
{
  watchDogCount++;
  if (watchDogCount >= 80)
  {
    Serial.flush();
    Serial.end();
    wdtFired = true;
    ESP.restart();
  }
}

/**
 * Refreshes device config from Firebase, reads the current timestamp, and
 * pushes temperature + datalogger entry to RTDB. Triggers threshold check
 * after a successful write.
 */
bool sendDataToFireBase()
{
  Serial.println("Entering sendDataToFireBase");
  sendDataPrevMillis = millis();
  // getDeviceConfigurations() movido para timer próprio em loop() (per D-15/D-16)

  timestamp = (int)getTime();
  Serial.print("time: ");
  Serial.println(timestamp);

  if (!sensorError && timestamp > MIN_VALID_TIMESTAMP)
  {
    Serial.print("Temperature: ");
    Serial.println(temperature);

    json.clear();
    json.set(deviceNamePath.c_str(), String(DEVICE_NAME));
    json.set(tempPath.c_str(), temperature);    // float, not String
    json.set(timestampPath.c_str(), timestamp); // int, not String
    json.set(statusPath.c_str(), String("online"));
    // Datalogger: temperature (float) and timestamp (int)
    char dlTempPath[40];
    char dlTsPath[40];
    snprintf(dlTempPath, sizeof(dlTempPath), "/datalogger/%lu/temperature", (unsigned long)timestamp);
    snprintf(dlTsPath, sizeof(dlTsPath), "/datalogger/%lu/timestamp", (unsigned long)timestamp);
    json.set(dlTempPath, temperature);
    json.set(dlTsPath, timestamp);

    bool writeOk = Firebase.RTDB.updateNode(&fbdo, databasePath.c_str(), &json);
    Serial.printf("Set json... %s\n", writeOk ? "ok" : fbdo.errorReason().c_str());
    json.clear();

    if (writeOk) {
      firebaseFailCount     = 0;
      firebaseSkipRemaining = 0;
      drainOnePendingReading();
      checkThresholdAlert();
      countDataSentToFireBase += 1;
      Serial.println("Leaving sendDataToFireBase");
      return true;
    } else {
      firebaseFailCount++;
      firebaseSkipRemaining = (uint8_t)min(1u << firebaseFailCount, 16u);
      enqueuePendingReading(temperature, (uint32_t)timestamp);
      Serial.printf("[Firebase] Write failed (fail #%u), backing off %u cycles\n",
                    firebaseFailCount, firebaseSkipRemaining);
      countDataSentToFireBase += 1;
      Serial.println("Leaving sendDataToFireBase");
      return false;
    }
  }
  else
  {
    Serial.println("Invalid temperature and/or invalid timestamp.");
  }

  countDataSentToFireBase += 1;
  Serial.println("Leaving sendDataToFireBase");
  return false;
}

/**
 * Main Firebase cycle: reads temperature, updates the display, and calls
 * sendDataToFireBase() when the interval has elapsed. Resets the device
 * if Firebase is unavailable for too long.
 */
void callFirebase()
{
  Serial.println("Entering callFirebase");

  // Backoff guard: pular ciclo se ainda em período de espera (per D-03)
  if (firebaseSkipRemaining > 0) {
    firebaseSkipRemaining--;
    float currentTemp = readTemperature();
    if (!sensorError) {
      uint32_t currentTs = (uint32_t)getTime();
      if (currentTs > MIN_VALID_TIMESTAMP) {
        enqueuePendingReading(currentTemp, currentTs);
      }
    }
    Serial.printf("[Firebase] Backoff: %u cycles remaining\n", firebaseSkipRemaining);
    return;
  }

  // Final safety cap: se max failures atingido, resetar (per D-04)
  if (firebaseFailCount >= 5) {
    Serial.println("[Firebase] Max failures reached. Resetting device.");
    drawResetDevice();
    delay(2000);
    ESP.reset();
  }

  prevTemperature = temperature;
  prevSensorError = sensorError;
  temperature = readTemperature();
  firebaseConnected = Firebase.ready();

  // Guard: skip Firebase entirely if WiFi layer is down — avoids SSL crash inside lib
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("[Firebase] WiFi not connected (status=%d), skipping cycle\n", WiFi.status());
    firebaseFailCount++;
    firebaseSkipRemaining = (uint8_t)min(1u << firebaseFailCount, 16u);
    float currentTemp = readTemperature();
    if (!sensorError) {
      uint32_t currentTs = (uint32_t)getTime();
      if (currentTs > MIN_VALID_TIMESTAMP) enqueuePendingReading(currentTemp, currentTs);
    }
    return;
  }

  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0))
  {
    sendDataToFireBase();
  }
  else if (!Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0))
  {
    Serial.println("[WiFi] Firebase not ready. Attempting WiFi reconnect...");
    bool reconnected = false;
    for (int attempt = 0; attempt < 3; attempt++)
    {
      drawBootProgress("Reconnecting...", 0);
      WiFi.reconnect();
      delay(2000);
      if (WiFi.status() == WL_CONNECTED)
      {
        reconnected = true;
        Serial.printf("[WiFi] Reconnected on attempt %d\n", attempt + 1);
        break;
      }
      Serial.printf("[WiFi] Attempt %d failed, status=%d\n", attempt + 1, WiFi.status());
    }

    if (!reconnected)
    {
      Serial.println("[WiFi] All reconnect attempts failed. Resetting.");
      drawResetDevice();
      delay(2000);
      ESP.reset();
    }
    // Se reconectou, Firebase irá reconectar automaticamente na próxima chamada.
  }

  if (alarmState == ALARM_OFF && countMessageSending == 0 && temperature > higherTemp)
  {
    countMessageSending += 1;
    checkThresholdAlert();
  }
  else if (alarmState == ALARM_OFF && countMessageSending == 0 && temperature < lowerTemp)
  {
    countMessageSending += 1;
    checkThresholdAlert();
  }

  Serial.println("Leaving callFirebase");
}

/**
 * Blinks the built-in LED twice to signal successful power-on / boot.
 */
void boardLedInitialization()
{
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
  digitalWrite(LED_BUILTIN, HIGH);
}

void setup()
{
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  boardLedInitialization();

  // Initialize display early for boot progress screens
  initDisplay();
  drawBootProgress("Starting...", 0);

  EEPROM.begin(EEPROM_SIZE);
  loadPendingQueue();  // Restaurar leituras pendentes da EEPROM (per D-09)

  USER_EMAIL = readStringFromEEPROM(EMAIL_ADDR);
  USER_PASSWORD = readStringFromEEPROM(PASS_ADDR);
  DEVICE_NAME = readStringFromEEPROM(DEVICE_NAME_ADDR);

  Serial.println("SETUP: Reading credentials from EEPROM");
  Serial.printf("Email: %s  Device: %s\n", USER_EMAIL.c_str(), DEVICE_NAME.c_str());

  DEVICE_UUID = String(ESP.getChipId());
  HOST_NAME = "guardiao-" + DEVICE_UUID;
  Serial.print("ESP Chip Id: ");
  Serial.println(DEVICE_UUID);

  drawBootProgress("WiFi...", 20);

  // Build unique AP SSID using last 4 hex digits of chip ID
  String portalSSID = "Guardiao-" + String(ESP.getChipId() & 0xFFFF, HEX);

  if (USER_EMAIL == "" || USER_PASSWORD == "" || DEVICE_NAME == "")
  {
    drawWifiNotConfigured(portalSSID);
    Serial.println("Empty credentials. Starting WiFiManager Captive Portal.");
    WiFiManager wifiManager;
    wifiManager.setDebugOutput(true);
    wifiManager.setConfigPortalTimeout(120);
    wifiManager.setConnectTimeout(60);
    wifiManager.setCleanConnect(true);
    if (!setupWiFiManager(wifiManager))
    {
      Serial.println("[Boot] Failed to setup WiFi Manager");
      drawBootError("WiFi config falhou");
      delay(3000);
      ESP.restart();
    }
    if (!wifiManager.startConfigPortal(portalSSID.c_str()))
    {
      Serial.println("[Boot] Config portal timed out.");
      drawBootError("Portal timeout");
      delay(3000);
      ESP.restart();
    }
  }
  else
  {
    initWifiManager();
  }

  WIFI_SSID = WiFi.SSID();
  drawBootProgress("NTP...", 40);


  wdtReset.attach(1, ISRWatchDog);

  timeClient.begin();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_S1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_S2_PIN, INPUT);
  digitalWrite(BUZZER_PIN, LOW);

  sensors.begin();
  sensors.getAddress(sensorAddress, 0);
  sensors.setResolution(sensorAddress, 12);

  drawBootProgress("Firebase...", 60);

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  Firebase.reconnectNetwork(true);
  fbdo.setResponseSize(4096);
  fbdo.setBSSLBufferSize(4096, 1024);  // Rx/Tx — reduce SSL stack pressure on ESP8266
  config.timeout.serverResponse    = 8000;   // ms — abandon SSL if server silent for 8s
  config.timeout.socketConnection  = 8000;   // ms — TCP connect timeout
  config.timeout.wifiReconnect     = 5000;   // ms — WiFi reconnect window
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;
  Firebase.begin(&config, &auth);

  drawBootProgress("Authenticating...", 80);
  Serial.println("Getting User UID");
  int authRetryCount = 0;
  while ((auth.token.uid) == "" && authRetryCount < 30)
  {
    Serial.print('.');
    delay(1000);
    authRetryCount++;
  }
  if ((auth.token.uid) == "")
  {
    Serial.println("\n[Auth] Firebase auth timeout after 30s. Restarting.");
    drawBootError("Firebase auth falhou");
    delay(3000);
    ESP.restart();
  }
  uid = auth.token.uid.c_str();

  Serial.print("User UID: ");
  Serial.println(uid);

  // Protocol v2: generate or load persistent LDID, use as RTDB device node key
  generateOrLoadLDID();
  databasePath = "/UsersData/" + uid + "/devices/" + DEVICE_LDID;

  Serial.println("Calling Firebase Init.");
  firebaseInit();

  Serial.println("Loading initial device configurations.");
  getDeviceConfigurations();

  // Protocol v2: send boot message after init
  sendBootMessage();

  OTAsetup();

  drawBootProgress("Ready!", 100);
  delay(400);

  // Initialize UI framework (frames, overlays, indicators)
  initUIFramework();
  beepBoot();
}

/**
 * Opens the WiFiManager captive portal on demand (called from button hold).
 */
void openConfigPortal()
{
  String portalSSID = "Guardiao-" + String(ESP.getChipId() & 0xFFFF, HEX);
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.setConnectTimeout(60);
  wifiManager.setCleanConnect(true);
  if (!setupWiFiManager(wifiManager))
  {
    Serial.println("Failed to setup WiFi Manager");
    beepError();
    return;
  }
  drawWifiNotConfigured(portalSSID);
  beepConfirm();
  wifiManager.startConfigPortal(portalSSID.c_str());
  beepSuccess();
  ESP.reset();
}

void loop()
{
  if (millis() - lastFirebaseCallMs >= GET_DATA_INTERVAL)
  {
    lastFirebaseCallMs = millis();
    callFirebase();
  }

  handleButtons();

  if (updateAlarm())
  {
    // Alarm active — display managed by alarm screen
  }
  else if (displayMode == DMODE_NORMAL)
  {
    static bool lastWifiConnected = true;
    bool wifiNow = (WiFi.status() == WL_CONNECTED);
    if (wifiNow != lastWifiConnected) {
      if (wifiNow) {
        Serial.printf("[OLED] WiFi reconectado — voltando para ui.update() (status=%d)\n", WiFi.status());
      } else {
        Serial.printf("[OLED] WiFi desconectado — exibindo drawWifiReconnecting() (status=%d)\n", WiFi.status());
      }
      lastWifiConnected = wifiNow;
    }
    if (wifiNow) {
      ui.update();
    } else {
      drawWifiReconnecting();
    }
  }
  // DMODE_MODAL: display managed by handleButtons (hold progress)

  // Protocol v2: periodic heartbeat
  if (millis() - lastHeartbeatMillis > HEARTBEAT_INTERVAL)
  {
    sendHeartbeat();
  }

  // Config fetch separado do ciclo de temperatura (per D-15/D-16)
  if (millis() - configFetchPrevMillis > CONFIG_FETCH_INTERVAL || configFetchPrevMillis == 0)
  {
    configFetchPrevMillis = millis();
    getDeviceConfigurations();
  }

  if (watchDogCount > 0) {
    Serial.printf("[WDT] loop reset, count was %u\n", watchDogCount);
  }
  watchDogCount = 0;
  ArduinoOTA.handle();
  yield();
}

/**
 * Generates a persistent Logical Device ID (LDID) or loads an existing one
 * from EEPROM. The LDID is immutable once created and used as the RTDB
 * device node key to survive device renames.
 */
void generateOrLoadLDID()
{
  DEVICE_LDID = readStringFromEEPROM(LDID_ADDR);
  if (DEVICE_LDID == "")
  {
    DEVICE_LDID = "ldid-" + String(DEVICE_UUID) + "-001";
    writeStringToEEPROM(LDID_ADDR, DEVICE_LDID);
    Serial.println("LDID generated: " + DEVICE_LDID);
  }
  else
  {
    Serial.println("LDID loaded: " + DEVICE_LDID);
  }
}

/**
 * Builds a status bitmap for protocol v2 heartbeat messages.
 * bit 0: online, bit 2: sensor read error, bit 4: threshold alert active.
 */
uint8_t calculateStatusBitmap()
{
  uint8_t st = 0x01; // always online (bit 0)
  if (sensorError)
    st |= 0x04; // error reading
  bool highAlert = (thresholdMode == "both" || thresholdMode == "upperOnly") && temperature > higherTemp;
  bool lowAlert = (thresholdMode == "both" || thresholdMode == "lowerOnly") && temperature < lowerTemp;
  if (highAlert || lowAlert)
    st |= 0x10; // alert
  return st;
}

/**
 * Sends a protocol v2 Boot message to Firebase with device metadata
 * (UUID, firmware version, reset reason, free heap, WiFi RSSI).
 */
void sendBootMessage()
{
  json.clear();
  json.add("v", 2);
  json.add("m", "B");
  json.add("ts", (int)getTime());
  json.set("d/uuid", DEVICE_UUID);
  json.set("d/fw", FIRMWARE_VERSION);
  json.set("d/rst", resetReason);
  json.set("d/heap", ESP.getFreeHeap());
  json.set("d/wifi", WiFi.RSSI());
  char bootPath[128];
  snprintf(bootPath, sizeof(bootPath), "/UsersData/%s/devices/%s/boot", uid.c_str(), DEVICE_LDID.c_str());
  Serial.printf("Boot msg: %s\n", Firebase.RTDB.updateNode(&fbdo, bootPath, &json) ? "ok" : fbdo.errorReason().c_str());
  json.clear();
}

/**
 * Sends a protocol v2 Heartbeat to Firebase with the current status bitmap,
 * free heap and WiFi RSSI. Called periodically from loop().
 */
void sendHeartbeat()
{
  json.clear();
  json.add("v", 2);
  json.add("m", "H");
  json.add("ts", (int)getTime());
  json.set("d/st", calculateStatusBitmap());
  json.set("d/heap", ESP.getFreeHeap());
  json.set("d/wifi", WiFi.RSSI());
  char heartbeatPath[128];
  snprintf(heartbeatPath, sizeof(heartbeatPath), "/UsersData/%s/devices/%s/heartbeat", uid.c_str(), DEVICE_LDID.c_str());
  Firebase.RTDB.updateNode(&fbdo, heartbeatPath, &json);
  json.clear();
  lastHeartbeatMillis = millis();
}
