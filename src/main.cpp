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

void firebaseInit()
{
  Serial.println("Entrando no Firebase Init");
  json.clear();
  json.add(deviceNamePath, String(DEVICE_NAME));
  json.add(deviceUUIDPath, String(DEVICE_UUID));   // raiz do device, não config/
  json.add(statusPath, String("online"));
  Serial.printf("Set json... %s\n", Firebase.RTDB.updateNode(&fbdo, databasePath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
  json.clear();
  Serial.println("Saindo do Firebase Init");
}

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

bool setupWiFiManager(WiFiManager &wifiManager)
{
  USER_EMAIL = readStringFromEEPROM(EMAIL_ADDR);
  USER_PASSWORD = readStringFromEEPROM(PASS_ADDR);
  DEVICE_NAME = readStringFromEEPROM(DEVICE_NAME_ADDR);

  STA_NAME = WiFi.hostname();
  Serial.println(STA_NAME);

  static WiFiManagerParameter custom_email("Email", "Digite seu email", USER_EMAIL.c_str(), 40);
  static WiFiManagerParameter custom_password("Senha", "Digite sua senha", USER_PASSWORD.c_str(), 40, "type='password'");
  static WiFiManagerParameter custom_device_name("Equipamento", "Digite o nome do equipamento", DEVICE_NAME.c_str(), 40);

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
    }
  });
  return true;
}

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

unsigned long getTime()
{
  timeClient.update();
  return timeClient.getEpochTime();
}

void getDeviceConfigurations()
{
  Serial.println("Entrei getDeviceConfigurations");

  if (!Firebase.ready())
  {
    Serial.println("Erro: Firebase não está pronto. Reiniciando.");
    Serial.flush();
    Serial.end();
    ESP.restart();
    return;
  }

  String fullPath = databasePath + configPath;

  if (!Firebase.RTDB.getJSON(&fbdo, fullPath))
  {
    Serial.println("Erro ao obter configurações do Firebase.");
    return;
  }

  if (fbdo.dataType() != "json")
  {
    Serial.println("Erro: dados não são JSON.");
    return;
  }

  DeserializationError error = deserializeJson(doc, fbdo.jsonString());
  if (error)
  {
    Serial.print(F("Erro ao desserializar JSON: "));
    Serial.println(error.f_str());
    return;
  }

  // --- Thresholds ---
  higherTemp = doc["_threshold"]["higherTemp"] | 80.0f;
  lowerTemp  = doc["_threshold"]["lowerTemp"]  | -80.0f;
  thresholdMode = doc["_threshold"]["thresholdMode"] | "both";

  Serial.printf("Threshold: lower=%.1f  higher=%.1f  mode=%s\n",
                lowerTemp, higherTemp, thresholdMode.c_str());

  // --- Medições programadas ---
  scheduledReadingsEnabled = doc["scheduledReadings"]["isEnabled"] | false;
  scheduledIntervalMinutes = doc["scheduledReadings"]["intervalMinutes"] | 5;
  scheduledStartHour       = doc["scheduledReadings"]["startHour"] | 0;

  if (scheduledReadingsEnabled && scheduledIntervalMinutes > 0)
  {
    timerDelay = (unsigned long)scheduledIntervalMinutes * 60UL * 1000UL;
    Serial.printf("ScheduledReadings: habilitado, intervalo=%d min\n", scheduledIntervalMinutes);
  }
  else
  {
    timerDelay = 300000UL; // padrão 5 minutos
    Serial.println("ScheduledReadings: desabilitado, usando padrão 5 min");
  }

  Serial.println("Saí getDeviceConfigurations");
}

float readTemperature()
{
  Serial.println("Entrei readTemperature");
  sensors.requestTemperatures();
  float tempC = sensors.getTempCByIndex(0);
  if (tempC == -127.0 || tempC == 85.0)
  {
    tempC = 888.0;
  }
  Serial.println("Sai readTemperature");
  return tempC;
}

// Toca alarme sonoro com frequência decrescente (2000→500 Hz) por 30 segundos
void alarmSound()
{
  Serial.println("Entrei no Alarm sound");
  unsigned long startTime = millis();
  unsigned int freq = 2000;

  while (millis() - startTime < 30000)
  {
    tone(BUZZER_PIN, freq, 200);           // toca 200 ms
    delay(300);                             // pausa 100 ms
    freq = max(500u, freq - 50u);           // reduz frequência gradualmente
    yield();
  }
  noTone(BUZZER_PIN);
  drawTemperature();
  Serial.println("Saí do Alarm sound");
}

void sendAlarm()
{
  drawAlarm();
  alarmSound();
}

void checkThresholdAlert()
{
  Serial.println("Entrei checkThresholdAlert");

  if (temperature == 888.0 || temperature == -127.0)
  {
    Serial.println("Sensor enviou leitura inválida!");
    countMessageSending = 0;
    return;
  }

  if (thresholdMode == "none")
  {
    Serial.println("ThresholdMode=none: alarme desabilitado.");
    countMessageSending = 0;
    return;
  }

  bool overHigh = (thresholdMode == "both" || thresholdMode == "upperOnly") && temperature > higherTemp;
  bool underLow = (thresholdMode == "both" || thresholdMode == "lowerOnly") && temperature < lowerTemp;

  if (overHigh)
  {
    Serial.println("Limite de Alta Temperatura Extrapolado!");
    sendAlarm();
  }
  else if (underLow)
  {
    Serial.println("Limite de Baixa Temperatura Extrapolado!");
    sendAlarm();
  }
  else
  {
    Serial.println("Temperatura dentro dos limites.");
    countMessageSending = 0;
  }

  Serial.println("Saí checkThresholdAlert");
}

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

void sendDataToFireBase()
{
  Serial.println("Entrei sendDataToFireBase");
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
    json.set(deviceNamePath.c_str(),  String(DEVICE_NAME));
    json.set(tempPath.c_str(),        temperature);          // float, não String
    json.set(timestampPath.c_str(),   timestamp);            // int, não String
    json.set(statusPath.c_str(),      String("online"));
    // Datalogger: temperatura (float) e timestamp (int)
    json.set(("/datalogger/" + String(timestamp) + dataloggerTempPath).c_str(),      temperature);
    json.set(("/datalogger/" + String(timestamp) + dataloggerTimestampPath).c_str(), timestamp);
    Serial.printf("Set json... %s\n", Firebase.RTDB.updateNode(&fbdo, databasePath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
    json.clear();

    checkThresholdAlert();
  }
  else
  {
    drawWaitingForTemperature();
    Serial.println("Temperatura inválida e/ou Timestamp inválido.");
  }

  countDataSentToFireBase += 1;
  Serial.println("Saí do sendDataToFireBase");
}

void callFirebase()
{
  Serial.println("Entrei callFirebase");

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

  Serial.println("Saí callFirebase");
}

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

  USER_EMAIL  = readStringFromEEPROM(EMAIL_ADDR);
  USER_PASSWORD = readStringFromEEPROM(PASS_ADDR);
  DEVICE_NAME = readStringFromEEPROM(DEVICE_NAME_ADDR);

  Serial.println("SETUP: PEGANDO USER EMAIL E PASSWORD");
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

  config.api_key      = API_KEY;
  auth.user.email     = USER_EMAIL;
  auth.user.password  = USER_PASSWORD;
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

  databasePath = "/UsersData/" + uid + "/devices/" + DEVICE_NAME;

  Serial.println("Calling Firebase Init.");
  firebaseInit();

  Serial.println("Obtendo configurações iniciais.");
  getDeviceConfigurations();

  OTAsetup();
}

void checkButtons()
{
  if (digitalRead(BUTTON_S2_PIN) == HIGH)
  {
    Serial.println("Entrei botao 02");
    drawWifiDetail();
    delay(300);
    getDeviceConfigurations();
    Serial.println(watchDogCount);
  }
  else if (digitalRead(BUTTON_S1_PIN) == HIGH)
  {
    Serial.println("Entrei botao 01");
    BATTERY_LEVEL = (ESP.getVcc() / 1.2) / 10000;
    drawBatteryLevel();
    delay(300);
    Serial.println(watchDogCount);
  }
  delay(100);
}

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
  watchDogCount = 0;
  ArduinoOTA.handle();
  yield();
}

// Protocol v2: Generate LDID if not exists
void generateOrLoadLDID() {
  DEVICE_LDID = readStringFromEEPROM(LDID_ADDR);
  if (DEVICE_LDID == "") {
    DEVICE_LDID = "ldid-" + String(DEVICE_UUID) + "-001";
    writeStringToEEPROM(LDID_ADDR, DEVICE_LDID);
    Serial.println("LDID generated: " + DEVICE_LDID);
  } else {
    Serial.println("LDID loaded: " + DEVICE_LDID);
  }
}

// Protocol v2: Calculate status bitmap
uint8_t calculateStatusBitmap() {
  uint8_t st = 0x01;  // always online (bit 0)
  if (temperature == 888.0 || temperature == -127.0) st |= 0x04;  // error reading
  bool highAlert = (thresholdMode == "both" || thresholdMode == "upperOnly") && temperature > higherTemp;
  bool lowAlert = (thresholdMode == "both" || thresholdMode == "lowerOnly") && temperature < lowerTemp;
  if (highAlert || lowAlert) st |= 0x10;  // alert
  return st;
}

// Protocol v2: Send Boot message
void sendBootMessage() {
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

// Protocol v2: Send Heartbeat
void sendHeartbeat() {
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
