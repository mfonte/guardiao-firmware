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
#include <logic.h>

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

// ntpAnchorEpoch/ntpAnchorMillis declared in drawOled.h (shared UI state)

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
void sendBootMessage();
void sendHeartbeat();
unsigned long getTime();

/**
 * Writes initial device identity and online status to Firebase RTDB.
 * Called once during setup after Firebase authentication succeeds.
 */
void firebaseInit()
{
  // Single PATCH: identity + status + boot message combined
  // ESP8266 has ~24KB heap — minimize SSL calls to avoid exhaustion
  json.clear();
  json.add(deviceUUIDPath, String(DEVICE_UUID));
  json.add(statusPath, String("online"));
  // Boot message fields (merged to avoid a second SSL call)
  json.add("boot/v", (int)2);
  json.add("boot/m", String("B"));
  json.add("boot/ts", (int)getTime());
  json.add("boot/d/uuid", DEVICE_UUID);
  json.add("boot/d/fw", String(FIRMWARE_VERSION));
  json.add("boot/d/rst", (int)resetReason);
  json.add("boot/d/heap", (int)ESP.getFreeHeap());
  json.add("boot/d/wifi", (int)WiFi.RSSI());

  yield();
  bool ok = Firebase.RTDB.updateNode(&fbdo, databasePath.c_str(), &json);
  LOG("[BOOT] fw=%s heap=%u wifi=%s rssi=%d email=%s uid=%.8s ldid=%s — PATCH %s",
      FIRMWARE_VERSION, ESP.getFreeHeap(), WIFI_SSID.c_str(), WiFi.RSSI(),
      USER_EMAIL.c_str(), uid.c_str(), DEVICE_LDID.c_str(), ok ? "ok" : fbdo.errorReason().c_str());
  json.clear();

  // Tear down SSL session completely — ESP8266 can only handle one at a time
  fbdo.stopWiFiClient();
  yield();
}

/**
 * Runs the WiFiManager portal. All WiFiManagerParameter objects are declared
 * in the same scope as WiFiManager so lambda captures and addParameter()
 * pointers remain valid during the blocking portal call.
 *
 * Capturing references to parameters declared in a called function (the old
 * setupWiFiManager pattern) caused Exception 2 (InstructionFetchProhibited)
 * when the portal rendered the network list and tried to call the callback.
 *
 * @param blocking  true  → startConfigPortal (first-time / forced reset)
 *                  false → autoConnect       (normal reconnect)
 */
void runWiFiPortal(bool blocking)
{
  STA_NAME = WiFi.hostname();
  LOG("Hostname: %s", STA_NAME.c_str());

  WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
  wifiManager.setConfigPortalTimeout(120);
  wifiManager.setConnectTimeout(60);
  wifiManager.setCleanConnect(true);

  // Parameters MUST live in the same scope as wifiManager.
  WiFiManagerParameter p_email("Email", "Enter your email", USER_EMAIL.c_str(), 40);
  WiFiManagerParameter p_password("Password", "Enter your password", USER_PASSWORD.c_str(), 40, "type=\'password\'");
  WiFiManagerParameter p_name("DeviceName", "Enter the device name", DEVICE_NAME.c_str(), 40);

  wifiManager.addParameter(&p_email);
  wifiManager.addParameter(&p_password);
  wifiManager.addParameter(&p_name);

  wifiManager.setSaveConfigCallback([&]()
  {
    String ne = p_email.getValue();
    String np = p_password.getValue();
    String nn = p_name.getValue();
    if (ne != USER_EMAIL || np != USER_PASSWORD || nn != DEVICE_NAME) {
      USER_EMAIL = ne;
      USER_PASSWORD = np;
      DEVICE_NAME = nn;
      if (DEVICE_NAME.length() > MAX_DISPLAY_NAME_LEN) DEVICE_NAME = DEVICE_NAME.substring(0, MAX_DISPLAY_NAME_LEN);
      writeStringToEEPROM(EMAIL_ADDR, USER_EMAIL);
      writeStringToEEPROM(PASS_ADDR, USER_PASSWORD);
      writeStringToEEPROM(DEVICE_NAME_ADDR, DEVICE_NAME);
      EEPROM.commit();
      LOG("Updated email, password and deviceName saved to EEPROM.");
    }
  });

  String portalSSID = "Guardiao-" + String(ESP.getChipId() & 0xFFFF, HEX);

  if (blocking) {
    // Show portal screen only when we actually open the portal
    drawWifiNotConfigured(portalSSID);
    if (!wifiManager.startConfigPortal(portalSSID.c_str())) {
      LOG("Config portal timed out — restarting");
      drawBootError("Portal timeout");
      delay(3000);
      ESP.restart();
    }
  } else {
    // autoConnect: try saved network silently first
    if (!wifiManager.autoConnect(portalSSID.c_str())) {
      LOG("WiFi connect failed — restarting");
      drawBootError("WiFi timeout");
      delay(3000);
      ESP.restart();
    }
  }
}

/**
 * Starts WiFiManager in auto-connect mode. Delegates to runWiFiPortal().
 */
void initWifiManager()
{
  runWiFiPortal(false);
}

/**
 * Returns the current UNIX epoch time obtained from the NTP client.
 * @return Epoch time as unsigned long.
 */
unsigned long getTime()
{
  timeClient.update();
  ntpAnchorEpoch  = timeClient.getEpochTime();
  ntpAnchorMillis = millis();
  return ntpAnchorEpoch;
}

/**
 * Fetches device configuration from Firebase RTDB (thresholds, scheduled readings)
 * and updates runtime variables. Restarts the device if Firebase is not ready.
 */
void getDeviceConfigurations()
{
  LOG("[CFG] Fetching device config from Firebase...");

  if (!Firebase.ready())
  {
    LOG("Error: Firebase not ready. Restarting.");
    Serial.flush();
    Serial.end();
    ESP.restart();
    return;
  }

  // Read /config (thresholds, intervals) — small JSON, no /datalogger bloat
  String fullPath = databasePath + configPath;
  if (!Firebase.RTDB.getJSON(&fbdo, fullPath))
  {
    LOG("Error: failed to fetch config from Firebase.");
    return;
  }

  if (fbdo.dataType() != "json")
  {
    LOG("Error: response data is not JSON.");
    return;
  }

  DeserializationError error = deserializeJson(doc, fbdo.jsonString());
  if (error)
  {
    LOG("JSON deserialization error: %s", error.c_str());
    return;
  }

  // --- Config section (doc IS the config object here) ---
  // Thresholds
  higherTemp = doc["_threshold"]["higherTemp"] | 80.0f;
  lowerTemp = doc["_threshold"]["lowerTemp"] | -80.0f;
  thresholdMode = doc["_threshold"]["thresholdMode"] | "both";
  LOG("[CFG] thresholds=[%.1f, %.1f] mode=%s interval=%lums name=%s",
      lowerTemp, higherTemp, thresholdMode.c_str(), timerDelay, DEVICE_NAME.c_str());

  // Scheduled readings (legacy)
  scheduledIntervalMinutes = doc["scheduledReadings"]["intervalMinutes"] | 0;
  scheduledStartHour = doc["scheduledReadings"]["startHour"] | 0;

  // sendIntervalMinutes — app-configurable temperature send interval (preferred)
  int sendInterval = doc["sendIntervalMinutes"] | 0;
  if (sendInterval > 0)
  {
    timerDelay = (unsigned long)sendInterval * 60UL * 1000UL;
    // sendInterval applied — logged below in [CFG] summary
  }
  else if (scheduledIntervalMinutes > 0)
  {
    timerDelay = (unsigned long)scheduledIntervalMinutes * 60UL * 1000UL;
  }
  else
  {
    timerDelay = 300000UL;
  }

  // --- Device name sync (read /data node — small, separate from /config) ---
  fbdo.stopWiFiClient();
  yield();
  delay(200);  // Let SSL teardown complete before new connection
  String dataNodePath = databasePath + "/data";
  if (Firebase.RTDB.getJSON(&fbdo, dataNodePath))
  {
    DeserializationError nameErr = deserializeJson(doc, fbdo.jsonString());
    if (!nameErr)
    {
      const char* remoteName = doc["displayName"] | (const char*)nullptr;
      if (remoteName && strlen(remoteName) > 0 && String(remoteName) != DEVICE_NAME)
      {
        LOG("Device name updated from app: '%s' -> '%s'", DEVICE_NAME.c_str(), remoteName);
        DEVICE_NAME = String(remoteName);
        if (DEVICE_NAME.length() > MAX_DISPLAY_NAME_LEN) DEVICE_NAME = DEVICE_NAME.substring(0, MAX_DISPLAY_NAME_LEN);
        writeStringToEEPROM(DEVICE_NAME_ADDR, DEVICE_NAME);
        EEPROM.commit();
      }
    }
  }


}

/**
 * Reads temperature from the DS18B20 sensor.
 * Sets global sensorError flag on disconnect/short-circuit (-127 or 85).
 * @return Temperature in Celsius (value undefined when sensorError is true).
 */
float readTemperature()
{
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
  return tempC;
}

/**
 * Evaluates the current temperature against configured thresholds and fires
 * sendAlarm() if a violation is detected. Respects the thresholdMode setting
 * ("both" | "upperOnly" | "lowerOnly" | "none").
 */
void checkThresholdAlert()
{
  int result = evaluateThreshold();
  if (result == -2)
  {
    LOG("[ALERT] Sensor invalid reading — alarm suppressed");
    countMessageSending = 0;
  }
  else if (result == -3)
  {
    LOG("[ALERT] thresholdMode=none — alarm disabled");
    countMessageSending = 0;
  }
  else if (result == 1)
  {
    LOG("[ALERT] HIGH temp=%.1f°C limit=%.1f°C mode=%s",
        temperature, higherTemp, thresholdMode.c_str());
    countMessageSending++;
    startAlarm();
  }
  else if (result == -1)
  {
    LOG("[ALERT] LOW  temp=%.1f°C limit=%.1f°C mode=%s",
        temperature, lowerTemp, thresholdMode.c_str());
    countMessageSending++;
    startAlarm();
  }
  else
  {
    LOG("[ALERT] temp=%.1f°C within limits [%.1f, %.1f]",
        temperature, lowerTemp, higherTemp);
    countMessageSending = 0;
  }
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
  sendDataPrevMillis = millis();
  timestamp = (int)getTime();

  if (!sensorError && timestamp > MIN_VALID_TIMESTAMP)
  {

    fbdo.stopWiFiClient();
    json.clear();
    // Use json.add (flat keys) instead of json.set (nested) so Firebase PATCH
    // does multi-path update — preserves other /data fields like displayName.
    json.add(tempPath, temperature);
    json.add(timestampPath, timestamp);
    json.add(statusPath, String("online"));
    // Datalogger: temperature (float) and timestamp (int)
    char dlTempPath[40];
    char dlTsPath[40];
    snprintf(dlTempPath, sizeof(dlTempPath), "/datalogger/%lu/temperature", (unsigned long)timestamp);
    snprintf(dlTsPath, sizeof(dlTsPath), "/datalogger/%lu/timestamp", (unsigned long)timestamp);
    json.add(String(dlTempPath), temperature);
    json.add(String(dlTsPath), (int)timestamp);

    bool writeOk = Firebase.RTDB.updateNode(&fbdo, databasePath.c_str(), &json);
    json.clear();
    fbdo.stopWiFiClient();
    yield();

    if (writeOk)
    {
      firebaseFailCount = 0;
      firebaseSkipRemaining = 0;
      LOG("[SEND] temp=%.1f°C ts=%lu heap=%u rssi=%d -> ok (fail=0)",
          temperature, (unsigned long)timestamp, ESP.getFreeHeap(), WiFi.RSSI());
      drainPendingReadings();
      checkThresholdAlert();
      countDataSentToFireBase += 1;
      return true;
    }
    else
    {
      firebaseFailCount++;
      firebaseSkipRemaining = (uint8_t)min(1u << firebaseFailCount, 16u);
      enqueuePendingReading(temperature, (uint32_t)timestamp);
      LOG("[SEND] temp=%.1f°C ts=%lu heap=%u rssi=%d -> FAILED: %s (fail=%u skip=%u)",
          temperature, (unsigned long)timestamp, ESP.getFreeHeap(), WiFi.RSSI(),
          fbdo.errorReason().c_str(), firebaseFailCount, firebaseSkipRemaining);
      countDataSentToFireBase += 1;
      return false;
    }
  }
  else
  {
    LOG("[SEND] skipped — sensorError=%d ts=%lu", (int)sensorError, (unsigned long)timestamp);
  }

  countDataSentToFireBase += 1;
  return false;
}

/**
 * Main Firebase cycle: reads temperature, updates the display, and calls
 * sendDataToFireBase() when the interval has elapsed. Resets the device
 * if Firebase is unavailable for too long.
 */
void callFirebase()
{
  // Backoff guard: pular ciclo se ainda em período de espera (per D-03)
  if (firebaseSkipRemaining > 0)
  {
    firebaseSkipRemaining--;
    float currentTemp = readTemperature();
    if (!sensorError)
    {
      uint32_t currentTs = (uint32_t)getTime();
      if (currentTs > MIN_VALID_TIMESTAMP)
      {
        enqueuePendingReading(currentTemp, currentTs);
      }
    }
    LOG("[Firebase] Backoff: %u cycles remaining", firebaseSkipRemaining);
    return;
  }

  // Final safety cap: se max failures atingido, resetar (per D-04)
  if (firebaseFailCount >= 5)
  {
    LOG("[Firebase] Max failures reached. Resetting device.");
    drawResetDevice();
    delay(2000);
    ESP.reset();
  }

  prevTemperature = temperature;
  prevSensorError = sensorError;
  temperature = readTemperature();
  firebaseConnected = Firebase.ready();

  // Guard: skip Firebase entirely if WiFi layer is down — avoids SSL crash inside lib
  if (WiFi.status() != WL_CONNECTED)
  {
    LOG("[Firebase] WiFi not connected (status=%d), skipping cycle", WiFi.status());
    firebaseFailCount++;
    firebaseSkipRemaining = (uint8_t)min(1u << firebaseFailCount, 16u);
    float currentTemp = readTemperature();
    if (!sensorError)
    {
      uint32_t currentTs = (uint32_t)getTime();
      if (currentTs > MIN_VALID_TIMESTAMP)
        enqueuePendingReading(currentTemp, currentTs);
    }
    return;
  }

  bool timerElapsed = (millis() - sendDataPrevMillis > timerDelay || sendDataPrevMillis == 0);
  bool thresholdExceeded = (alarmState == ALARM_OFF && countMessageSending == 0 &&
                            (temperature > higherTemp || temperature < lowerTemp));

  if (Firebase.ready() && (timerElapsed || thresholdExceeded))
  {
    if (thresholdExceeded && !timerElapsed)
    {
      LOG("[Alert] Threshold exceeded — forcing immediate Firebase send");
    }
    sendDataToFireBase();
  }
  else if (!Firebase.ready() && timerElapsed)
  {
    LOG("[WiFi] Firebase not ready. Attempting WiFi reconnect...");
    bool reconnected = false;
    for (int attempt = 0; attempt < 3; attempt++)
    {
      drawBootProgress("Reconnecting...", 0);
      WiFi.reconnect();
      delay(2000);
      if (WiFi.status() == WL_CONNECTED)
      {
        reconnected = true;
        LOG("[WiFi] Reconnected on attempt %d", attempt + 1);
        break;
      }
      LOG("[WiFi] Attempt %d failed, status=%d", attempt + 1, WiFi.status());
    }

    if (!reconnected)
    {
      LOG("[WiFi] All reconnect attempts failed. Resetting.");
      drawResetDevice();
      delay(2000);
      ESP.reset();
    }
    // Firebase reconecta automaticamente na proxima chamada apos WiFi recovery.
  }

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
  loadPendingQueue(); // Restaurar leituras pendentes da EEPROM (per D-09)

  USER_EMAIL = readStringFromEEPROM(EMAIL_ADDR);
  USER_PASSWORD = readStringFromEEPROM(PASS_ADDR);
  DEVICE_NAME = readStringFromEEPROM(DEVICE_NAME_ADDR);

  DEVICE_UUID = String(ESP.getChipId());
  HOST_NAME = "guardiao-" + DEVICE_UUID;

  drawBootProgress("WiFi...", 20);

  // Build unique AP SSID using last 4 hex digits of chip ID
  String portalSSID = "Guardiao-" + String(ESP.getChipId() & 0xFFFF, HEX);

  if (USER_EMAIL == "" || USER_PASSWORD == "" || DEVICE_NAME == "")
  {
    LOG("Empty credentials. Starting WiFiManager Captive Portal.");
    runWiFiPortal(true);
    // Restart after first-time setup: WiFiManager's web server fragments
    // heap heavily (~20KB). Firebase BearSSL needs a contiguous block to
    // initialize SSL for the GITKit token request. A clean boot fixes it.
    LOG("First-time config done — restarting for clean heap before Firebase init.");
    drawBootProgress("Reiniciando...", 100);
    delay(1500);
    ESP.restart();
  }
  else
  {
    initWifiManager();
  }

  WIFI_SSID = WiFi.SSID();
  drawBootProgress("NTP...", 40);

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
  fbdo.setResponseSize(2048);
  fbdo.setBSSLBufferSize(4096, 1024);     // Rx/Tx — needs 4K rx for SSL certificate chain
  config.timeout.serverResponse = 10000;  // ms — SSL handshake can be slow on ESP8266
  config.timeout.socketConnection = 8000; // ms — TCP connect timeout
  config.timeout.wifiReconnect = 5000;    // ms — WiFi reconnect window
  config.token_status_callback = tokenStatusCallback;
  config.max_token_generation_retry = 5;
  Firebase.begin(&config, &auth);

  drawBootProgress("Authenticating...", 80);

  int authRetryCount = 0;
  while ((auth.token.uid) == "" && authRetryCount < 30)
  {
    Serial.print('.');
    delay(1000);
    authRetryCount++;
  }
  if ((auth.token.uid) == "")
  {
    LOG("\n[Auth] Firebase auth timeout after 30s. Restarting.");
    drawBootError("Firebase auth falhou");
    delay(3000);
    ESP.restart();
  }
  uid = auth.token.uid.c_str();



  // Protocol v2: generate or load persistent LDID, use as RTDB device node key
  generateOrLoadLDID();
  databasePath = "/UsersData/" + uid + "/devices/" + DEVICE_LDID;

  yield();
  delay(500);  // Let SSL from token auth close cleanly before new SSL operations
  firebaseInit();
  // Config loaded inside firebaseInit (single GET for entire device node)

  // Boot message merged into firebaseInit PATCH (single SSL call)

  // Start WDT after boot sequence
  wdtReset.attach(1, ISRWatchDog);

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
  beepConfirm();
  runWiFiPortal(true);
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
    if (wifiNow != lastWifiConnected)
    {
      if (wifiNow)
      {
        LOG("[WiFi] reconnected (ssid=%s rssi=%d)", WiFi.SSID().c_str(), WiFi.RSSI());
      }
      else
      {
        LOG("[WiFi] disconnected (status=%d)", WiFi.status());
      }
      lastWifiConnected = wifiNow;
    }
    if (wifiNow)
    {
      ui.update();
    }
    else
    {
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

  if (ESP.getFreeHeap() < 15000)
  {
    LOG("[HEAP] WARNING: free heap=%u — SSL/Firebase may fail", ESP.getFreeHeap());
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
    LOG("LDID generated: %s", DEVICE_LDID.c_str());
  }
  else
  {
    LOG("LDID loaded: %s", DEVICE_LDID.c_str());
  }
}

/**
 * Sends a protocol v2 Boot message to Firebase with device metadata
 * (UUID, firmware version, reset reason, free heap, WiFi RSSI).
 */
void sendBootMessage()
{
  fbdo.stopWiFiClient();
  yield();

  json.clear();
  json.add("v", (int)2);
  json.add("m", String("B"));
  json.add("ts", (int)getTime());
  json.add("d/uuid", DEVICE_UUID);
  json.add("d/fw", String(FIRMWARE_VERSION));
  json.add("d/rst", (int)resetReason);
  json.add("d/heap", (int)ESP.getFreeHeap());
  json.add("d/wifi", (int)WiFi.RSSI());
  char bootPath[128];
  snprintf(bootPath, sizeof(bootPath), "/UsersData/%s/devices/%s/boot", uid.c_str(), DEVICE_LDID.c_str());
  LOG("Boot msg: %s", Firebase.RTDB.updateNode(&fbdo, bootPath, &json) ? "ok" : fbdo.errorReason().c_str());
  json.clear();
}

/**
 * Sends a protocol v2 Heartbeat to Firebase with the current status bitmap,
 * free heap and WiFi RSSI. Called periodically from loop().
 */
void sendHeartbeat()
{
  fbdo.stopWiFiClient();
  yield();

  json.clear();
  json.add("v", (int)2);
  json.add("m", String("H"));
  json.add("ts", (int)getTime());
  json.add("d/st", (int)calculateStatusBitmap());
  json.add("d/heap", (int)ESP.getFreeHeap());
  json.add("d/wifi", (int)WiFi.RSSI());
  char heartbeatPath[128];
  snprintf(heartbeatPath, sizeof(heartbeatPath), "/UsersData/%s/devices/%s/heartbeat", uid.c_str(), DEVICE_LDID.c_str());
  bool hbOk = Firebase.RTDB.updateNode(&fbdo, heartbeatPath, &json);
  json.clear();
  lastHeartbeatMillis = millis();
  LOG("[HB] heap=%u rssi=%d uptime=%lus status=0x%02X -> %s",
      ESP.getFreeHeap(), WiFi.RSSI(), millis() / 1000UL,
      calculateStatusBitmap(), hbOk ? "ok" : fbdo.errorReason().c_str());
}
