#define ADC_MODE(ADC_VCC)
#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <WiFiManager.h>
#include <Wire.h>
#include <time.h>
#include <DallasTemperature.h>
#include <drawOled.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Ticker.h>
#include <SimpleTimer.h>
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

// Initialize the timer to check temperature thresholds
SimpleTimer timer;

Ticker wdtReset;
Ticker alarmInterval;

// Database main path (updated in setup with user UID)
String databasePath;

// Variable to save Firebase USER UID
String uid;

FirebaseJson json;
JsonDocument doc;

// Configurar o modo ADC para ler a tensão de alimentação
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
  json.add(deviceUUIDPath, String(DEVICE_UUID)); // raiz do device, não config/
  json.add(statusPath, String("online"));
  Serial.printf("Set json... %s\n", Firebase.RTDB.updateNode(&fbdo, databasePath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
  json.clear();
  Serial.println("Leaving firebaseInit");
}

/**
 * Persists WiFiManager form values (email, password, device name) to EEPROM.
 * Invoked by WiFiManager after the user submits the captive portal form.
 */
void saveConfigCallback()
{
  Serial.println("Saving configuration");
  USER_EMAIL = newEmail;
  USER_PASSWORD = newPassword;
  DEVICE_NAME = newDeviceName;
  writeStringToEEPROM(EMAIL_ADDR, USER_EMAIL);
  writeStringToEEPROM(PASS_ADDR, USER_PASSWORD);
  writeStringToEEPROM(DEVICE_NAME_ADDR, DEVICE_NAME);
  EEPROM.commit();
  Serial.println("Configuration saved to EEPROM.");
}

/**
 * Configures WiFiManager with custom parameters (email, password, device name)
 * read from EEPROM and registers the save-config callback.
 * @return true on success.
 */
bool setupWiFiManager(WiFiManager &wifiManager)
{
  USER_EMAIL = readStringFromEEPROM(EMAIL_ADDR);
  USER_PASSWORD = readStringFromEEPROM(PASS_ADDR);
  DEVICE_NAME = readStringFromEEPROM(DEVICE_NAME_ADDR);

  STA_NAME = WiFi.hostname();
  Serial.println(STA_NAME);

  static WiFiManagerParameter custom_email("Email", "Enter your email", USER_EMAIL.c_str(), 40);
  static WiFiManagerParameter custom_password("Password", "Enter your password", USER_PASSWORD.c_str(), 40, "type='password'");
  static WiFiManagerParameter custom_device_name("Device name", "Enter the device name", DEVICE_NAME.c_str(), 40);

  wifiManager.addParameter(&custom_email);
  wifiManager.addParameter(&custom_password);
  wifiManager.addParameter(&custom_device_name);

  wifiManager.setSaveConfigCallback([]()
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
  if (!wifiManager.autoConnect("IoTemp Setup"))
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
 * Returns 888.0 for disconnect/short-circuit sentinel values (-127 or 85).
 * @return Temperature in Celsius, or 888.0 on sensor error.
 */
float readTemperature()
{
  Serial.println("Entering readTemperature");
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if (tempC == -127.0 || tempC == 85.0)
  {
    tempC = 888.0;
  }
  Serial.println("Leaving readTemperature");
  return tempC;
}

/**
 * Plays a descending alarm tone (2000 → 500 Hz) via the passive buzzer for 30 seconds.
 * Blocks execution during playback, then restores the temperature screen.
 */
void alarmSound()
{
  Serial.println("Entering alarmSound");
  unsigned long startTime = millis();
  unsigned int freq = 2000;

  while (millis() - startTime < 30000)
  {
    tone(BUZZER_PIN, freq, 200);  // play for 200 ms
    delay(300);                   // pause 100 ms
    freq = max(500u, freq - 50u); // gradually reduce frequency
    yield();
  }
  noTone(BUZZER_PIN);
  drawTemperature();
  Serial.println("Leaving alarmSound");
}

/**
 * Triggers the alarm sequence: draws the alarm screen then plays the alarm tone.
 */
void sendAlarm()
{
  drawAlarm();
  alarmSound();
}

/**
 * Evaluates the current temperature against configured thresholds and fires
 * sendAlarm() if a violation is detected. Respects the thresholdMode setting
 * ("both" | "upperOnly" | "lowerOnly" | "none").
 */
void checkThresholdAlert()
{
  Serial.println("Entering checkThresholdAlert");

  if (temperature == 888.0 || temperature == -127.0)
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
    sendAlarm();
  }
  else if (underLow)
  {
    Serial.println("Low temperature threshold exceeded!");
    sendAlarm();
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
  if (watchDogCount == 80)
  {
    Serial.flush();
    Serial.end();
    ESP.restart();
  }
  Serial.println(watchDogCount);
}

/**
 * Refreshes device config from Firebase, reads the current timestamp, and
 * pushes temperature + datalogger entry to RTDB. Triggers threshold check
 * after a successful write.
 */
void sendDataToFireBase()
{
  Serial.println("Entering sendDataToFireBase");
  sendDataPrevMillis = millis();

  getDeviceConfigurations();

  timestamp = (int)getTime();
  Serial.print("time: ");
  Serial.println(timestamp);

  if (temperature != 888.0 && timestamp > 1712843539)
  {
    Serial.print("Temperature: ");
    Serial.println(temperature);

    drawTemperature();

    json.clear();
    json.set(deviceNamePath.c_str(), String(DEVICE_NAME));
    json.set(tempPath.c_str(), temperature);    // float, não String
    json.set(timestampPath.c_str(), timestamp); // int, não String
    json.set(statusPath.c_str(), String("online"));
    // Datalogger: temperature (float) and timestamp (int)
    json.set(("/datalogger/" + String(timestamp) + dataloggerTempPath).c_str(), temperature);
    json.set(("/datalogger/" + String(timestamp) + dataloggerTimestampPath).c_str(), timestamp);
    Serial.printf("Set json... %s\n", Firebase.RTDB.updateNode(&fbdo, databasePath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
    json.clear();

    checkThresholdAlert();
  }
  else
  {
    drawWaitingForTemperature();
    Serial.println("Invalid temperature and/or invalid timestamp.");
  }

  countDataSentToFireBase += 1;
  Serial.println("Leaving sendDataToFireBase");
}

/**
 * Main Firebase cycle: reads temperature, updates the display, and calls
 * sendDataToFireBase() when the interval has elapsed. Resets the device
 * if Firebase is unavailable for too long.
 */
void callFirebase()
{
  Serial.println("Entering callFirebase");

  temperature = readTemperature();
  drawTemperature();

  if (Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0))
  {
    sendDataToFireBase();
  }
  else if (!Firebase.ready() && (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0))
  {
    drawResetDevice();
    delay(4000);
    ESP.reset();
  }

  if (countMessageSending == 0 && temperature > higherTemp)
  {
    countMessageSending += 1;
    checkThresholdAlert();
  }
  else if (countMessageSending == 0 && temperature < lowerTemp)
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

  display.init();
  display.flipScreenVertically();

  EEPROM.begin(EEPROM_SIZE);

  USER_EMAIL = readStringFromEEPROM(EMAIL_ADDR);
  USER_PASSWORD = readStringFromEEPROM(PASS_ADDR);
  DEVICE_NAME = readStringFromEEPROM(DEVICE_NAME_ADDR);

  Serial.println("SETUP: Reading user email and password from EEPROM");
  Serial.println(USER_EMAIL);
  Serial.println(USER_PASSWORD);
  Serial.println(DEVICE_NAME);

  DEVICE_UUID = String(ESP.getChipId());
  Serial.print("ESP Chip Id: ");
  Serial.println(DEVICE_UUID);

  if (USER_EMAIL == "" || USER_PASSWORD == "" || DEVICE_NAME == "")
  {
    drawWifiNotConfigured();
    Serial.println("Empty E-mail or Password. Starting WiFiManager Captive Portal.");
    WiFiManager wifiManager;
    wifiManager.setDebugOutput(true);
    wifiManager.setConnectTimeout(60);
    wifiManager.setCleanConnect(true);
    if (!setupWiFiManager(wifiManager))
    {
      Serial.println("Failed to setup WiFi Manager");
      ESP.restart();
    }
    wifiManager.startConfigPortal("IoTemp Setup");
  }
  else
  {
    initWifiManager();
    drawWifiDetail();
  }

  WIFI_SSID = WiFi.SSID();

  timer.setInterval(GET_DATA_INTERVAL, callFirebase);

  wdtReset.attach(1, ISRWatchDog);

  timeClient.begin();

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_S1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_S2_PIN, INPUT);
  digitalWrite(BUZZER_PIN, LOW);

  sensors.begin();
  sensors.getAddress(sensorAddress, 0);
  sensors.setResolution(sensorAddress, 12);

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;

  Firebase.reconnectNetwork(true);
  fbdo.setResponseSize(4096);
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;
  Firebase.begin(&config, &auth);

  Serial.println("Getting User UID");
  while ((auth.token.uid) == "")
  {
    Serial.print('.');
    delay(1000);
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
}

/**
 * Polls S1 and S2 buttons on each loop iteration.
 * S2 short-press: shows WiFi details and refreshes config.
 * S1 short-press: shows battery voltage.
 */
void checkButtons()
{
  if (digitalRead(BUTTON_S2_PIN) == HIGH)
  {
    Serial.println("Button S2 pressed");
    drawWifiDetail();
    delay(300);
    getDeviceConfigurations();
    Serial.println(watchDogCount);
  }
  else if (digitalRead(BUTTON_S1_PIN) == HIGH)
  {
    Serial.println("Button S1 pressed");
    BATTERY_LEVEL = (ESP.getVcc() / 1.2) / 10000;
    drawBatteryLevel();
    delay(300);
    Serial.println(watchDogCount);
  }
  delay(100);
}

/**
 * Launches the WiFiManager captive portal if credentials are missing, or
 * when S1 is held for BUTTON_S1_HOLD_TIME milliseconds.
 */
void setupNewWM()
{
  if (USER_EMAIL == "" || USER_PASSWORD == "")
  {
    Serial.println("Empty E-mail or User. Restarting Captive Portal.");
    WiFiManager wifiManager;
    wifiManager.setDebugOutput(true);
    wifiManager.setConnectTimeout(60);
    wifiManager.setCleanConnect(true);
    if (!setupWiFiManager(wifiManager))
    {
      Serial.println("Failed to re-setup WiFi Manager");
      return;
    }
    STA_NAME = "IoTemp Setup - " + STA_NAME;
    drawWifiNotConfigured();
    wifiManager.startConfigPortal(STA_NAME.c_str());
  }
  if (digitalRead(BUTTON_S1_PIN) == HIGH)
  {
    if (buttonS1PressedTime == 0)
    {
      buttonS1PressedTime = millis();
    }
    else if (millis() - buttonS1PressedTime >= BUTTON_S1_HOLD_TIME)
    {
      WiFiManager wifiManager;
      wifiManager.setDebugOutput(true);
      wifiManager.setConnectTimeout(60);
      wifiManager.setCleanConnect(true);
      if (!setupWiFiManager(wifiManager))
      {
        Serial.println("Failed to re-setup WiFi Manager");
        return;
      }
      STA_NAME = "IoTemp Setup - " + STA_NAME;
      drawWifiNotConfigured();
      wifiManager.startConfigPortal(STA_NAME.c_str());
      ESP.reset();
      buttonS1PressedTime = 0;
    }
  }
  else
  {
    buttonS1PressedTime = 0;
  }
}

/**
 * Performs a hard reset when S2 is held for BUTTON_S2_HOLD_TIME milliseconds.
 */
void resetDevice()
{
  if (digitalRead(BUTTON_S2_PIN) == HIGH)
  {
    if (buttonS2PressedTime == 0)
    {
      buttonS2PressedTime = millis();
    }
    else if (millis() - buttonS2PressedTime >= BUTTON_S2_HOLD_TIME)
    {
      drawResetDevice();
      delay(2000);
      ESP.reset();
      buttonS2PressedTime = 0;
    }
  }
  else
  {
    buttonS2PressedTime = 0;
  }
}

void loop()
{
  timer.run();
  setupNewWM();
  resetDevice();
  checkButtons();

  // Protocol v2: periodic heartbeat
  if (millis() - lastHeartbeatMillis > HEARTBEAT_INTERVAL)
  {
    sendHeartbeat();
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
  if (temperature == 888.0 || temperature == -127.0)
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
  String bootPath = "/UsersData/" + uid + "/devices/" + DEVICE_LDID + "/boot";
  Serial.printf("Boot msg: %s\n", Firebase.RTDB.updateNode(&fbdo, bootPath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
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
  String heartbeatPath = "/UsersData/" + uid + "/devices/" + DEVICE_LDID + "/heartbeat";
  Firebase.RTDB.updateNode(&fbdo, heartbeatPath.c_str(), &json);
  json.clear();
  lastHeartbeatMillis = millis();
}
