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
#include "addons/TokenHelper.h" // Provide the token generation process info.
#include "addons/RTDBHelper.h"  // Provide the RTDB payload printing info and other helper functions.
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

// Database main paths (to be updated in setup with the user UID)
String databasePath;
String databaseAlertPath;
String databaseThresholdPath;

// Variable to save Firebase USER UID
String uid;

FirebaseJson json;
JsonDocument doc;

// Configurar o modo ADC para ler a tensão de alimentação
ADC_MODE(ADC_VCC);

void firebaseInit()
{
  // Send readings to database
  Serial.println("Entrando no Firebase Init");
  json.clear(); // Clear previous data
  json.add(deviceNamePath, String(DEVICE_NAME));
  json.add(configPath + "/deviceUUID", String(DEVICE_UUID));
  json.add("/data/deviceStatus", "online");
  Serial.printf("Set json... %s\n", Firebase.RTDB.updateNode(&fbdo, databasePath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
  json.clear(); // Clear previous data
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
  // Read the email and password already saved in EEPROM
  USER_EMAIL = readStringFromEEPROM(EMAIL_ADDR);
  USER_PASSWORD = readStringFromEEPROM(PASS_ADDR);
  DEVICE_NAME = readStringFromEEPROM(DEVICE_NAME_ADDR);

  STA_NAME = WiFi.hostname();
  Serial.println(STA_NAME);

  // Create custom parameters for email and password.
  static WiFiManagerParameter custom_email("Email", "Digite seu email", USER_EMAIL.c_str(), 40);
  static WiFiManagerParameter custom_password("Senha", "Digite sua senha", USER_PASSWORD.c_str(), 40, "type='password'");
  static WiFiManagerParameter custom_device_name("Equipamento", "Digite o nome do equipamento", DEVICE_NAME.c_str(), 40);

  // Add the custom parameters to WiFiManager
  wifiManager.addParameter(&custom_email);
  wifiManager.addParameter(&custom_password);
  wifiManager.addParameter(&custom_device_name);

  // Set save config callback to handle updated parameters
  wifiManager.setSaveConfigCallback([]()
                                    {
    // Access and modify the static variables directly
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
  return true; // Return true if everything is setup correctly.
}

void initWifiManager()
{
  // Initialize the WiFiManager and custom parameters
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
  wifiManager.setConfigPortalTimeout(30);
  wifiManager.setConnectTimeout(60);
  wifiManager.setCleanConnect(true);
  if (!setupWiFiManager(wifiManager))
  {
    Serial.println("Failed to setup WiFi Manager");
    return; // Stop further execution if WiFiManager setup fails.
  }
  // Attempt to connect using saved settings; if it fails, start the configuration portal.
  if (!wifiManager.autoConnect("IoTemp Setup"))
  {
    Serial.println("Failed to connect and hit timeout");
    return; // Stop further execution if unable to connect.
  }
}

// Function that gets current epoch time
unsigned long getTime()
{
  timeClient.update();
  unsigned long now_udp = timeClient.getEpochTime();
  return now_udp;
}

void getDeviceConfigurations()
{
  Serial.println("Entrei getDeviceConfigurations");

  // Verificar se o Firebase está pronto
  if (!Firebase.ready())
  {
    Serial.println("Erro: Firebase não está pronto.");
    Serial.println("Erro: vou resetar 1");
    Serial.flush();
    Serial.end();
    ESP.restart();
    return;
  }

  // Construir o caminho completo do banco de dados
  String fullPath = databasePath + configPath;

  // Tentar obter os dados do banco de dados Firebase em tempo real
  if (!Firebase.RTDB.getJSON(&fbdo, fullPath))
  {
    Serial.println("Erro ao obter os dados do banco de dados Firebase.");
    Serial.println("Erro: Firebase não está pronto.");
    return;
  }

  // Verificar o tipo de dados obtido
  if (fbdo.dataType() != "json")
  {
    Serial.println("Erro: Os dados obtidos não são do tipo JSON.");
    return;
  }

  // Obter a string JSON dos dados
  String fbdoJson = fbdo.jsonString();
  // Serial.println(fbdoJson);

  // Desserializar a string JSON
  DeserializationError error = deserializeJson(doc, fbdoJson);
  if (error)
  {
    Serial.print(F("Erro ao desserializar JSON: "));
    Serial.println(error.f_str());
    return;
  }

  // Obter os limiares de temperatura do documento JSON
  higherTemp = doc["_threshold"]["higherTemp"].as<float>();
  lowerTemp = doc["_threshold"]["lowerTemp"].as<float>();

  if (higherTemp == 0.00 and lowerTemp == 0.00)
  {
    higherTemp = 80.0;
    lowerTemp = -80.0;
  }

  Serial.print("Higher Temp. Threshold: ");
  Serial.println(higherTemp);
  Serial.print("Lower Temp. Threshold: ");
  Serial.println(lowerTemp);
  Serial.println("");

  Serial.println("Saí getDeviceConfigurations");
}

// Read temperature from DS18B20 sensor
float readTemperature()
{
  Serial.println("Entrei readTemperature");
  sensors.requestTemperatures();            // Request temperature reading
  float tempC = sensors.getTempCByIndex(0); // Get temperature in Celsius

  if (tempC == -127.0 || tempC == 85.0)
  {
    tempC = 888.0;
  }
  Serial.println("Sai readTemperature");
  return tempC;
}

// This function plays an alarm sound
void alarmSound()
{
  Serial.println("Entrei no Alarm sound");
  unsigned long startTime = millis();
  unsigned long currentTime;
  unsigned long beepDuration = 3000; // Duração inicial entre beeps em milissegundos
  unsigned long previousBeepTime = millis();
  bool buzzerState = false;

  while (millis() - startTime < 30000)
  { // Execute por 30 segundos
    currentTime = millis();
    if (currentTime - previousBeepTime >= beepDuration)
    {
      previousBeepTime = currentTime;
      beepDuration = beepDuration * 0.9;                  // Reduz a duração do beep em 10%
      buzzerState = !buzzerState;                         // Alterna o estado do buzzer
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW); // Liga ou desliga o buzzer

      // Liga ou desliga o display de acordo com o estado do buzzer
      if (buzzerState)
      {
        drawAlarm();
      }
      else
      {
        drawTemperature();
      }
    }
    yield();   // Alimente o watchdog timer
    delay(10); // Adicione um pequeno delay para evitar sobrecarga
  }
  digitalWrite(BUZZER_PIN, LOW); // Garante que o buzzer esteja desligado ao fim do alarme
  drawTemperature();
  Serial.println("Saí do Alarm sound");
}

/* This function joins the alarm sound and draw the alarm on OLED Display */
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
    Serial.println("Sensor enviou leitura errada!");
    countMessageSending = 0;
  }
  else if (temperature > higherTemp)
  {
    Serial.println("Limite de Alta Temperatura Extrapolado!");
    sendAlarm();
  }
  else if (temperature < lowerTemp)
  {
    Serial.println("Limite de Baixa Temperatura Extrapolado!");
    sendAlarm();
  }
  else
  {
    Serial.println("Temperatura Dentro dos Limites!");
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
  // Send new readings to database
  sendDataPrevMillis = millis();

  getDeviceConfigurations();

  timestamp = getTime();
  Serial.print("time: ");
  Serial.println(timestamp);

  if (temperature != 888.0 && timestamp > 1712843539)
  {
    Serial.print("Temperature: ");
    Serial.println(temperature);

    drawTemperature();
    // Send readings to database
    json.clear(); // Clear previous data
    json.set(deviceNamePath.c_str(), String(DEVICE_NAME));
    json.set(tempPath.c_str(), String(temperature));
    json.set(timestampPath.c_str(), String(timestamp));
    json.set(("/data/deviceStatus"), String("online"));
    json.add(("/datalogger/" + String(timestamp) + dataloggerTempPath).c_str(), String(temperature));
    json.add(("/datalogger/" + String(timestamp) + dataloggerTimestampPath).c_str(), String(timestamp));
    Serial.printf("Set json... %s\n", Firebase.RTDB.updateNode(&fbdo, databasePath.c_str(), &json) ? "ok" : fbdo.errorReason().c_str());
    json.clear();
    checkThresholdAlert();
  }
  else
  {
    drawWaitingForTemperature();
    Serial.println("Temperatura invalida e/ou Timestamp invalido.");
  }

  countDataSentToFireBase += 1;
  Serial.println("Saí do sendDataToFireBase");
}

void callFirebase()
{
  Serial.println("Entrei callFirebase");

  temperature = readTemperature(); // Get latest sensor readings
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
  else
  {
    // Serial.println("countMessageSending: " + String(countMessageSending));
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
  // Initialize serial communication and baudrate
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  boardLedInitialization();

  // Initializing OLED display
  display.init();
  display.flipScreenVertically();

  // Initialize EEPROM with the required size.
  EEPROM.begin(EEPROM_SIZE);

  // Read the email, password, and device name from EEPROM
  USER_EMAIL = readStringFromEEPROM(EMAIL_ADDR);
  USER_PASSWORD = readStringFromEEPROM(PASS_ADDR);
  DEVICE_NAME = readStringFromEEPROM(DEVICE_NAME_ADDR);

  Serial.println("SETUP: PEGANDO USER EMAIL E PASSWORD");
  Serial.println(USER_EMAIL);
  Serial.println(USER_PASSWORD);
  Serial.println(DEVICE_NAME);

  DEVICE_UUID = ESP.getChipId();
  Serial.print("ESP Chip Id: ");
  Serial.println(DEVICE_UUID);

  Serial.print("ESP Flash Chip Id: ");
  Serial.println(ESP.getFlashChipId());

  // Check if email or password is empty or null
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
      // Handle failure (e.g., reset device, retry, etc.)
      ESP.restart();
    }
    wifiManager.startConfigPortal("IoTemp Setup");
  }
  else
  {
    // Initializing Wi-Fi
    initWifiManager();
    drawWifiDetail();
  }

  WIFI_SSID = WiFi.SSID();

  /* Initialising the timer interval to get temperature */
  timer.setInterval(GET_DATA_INTERVAL, callFirebase);

  // Initializing Watchdog Reset
  wdtReset.attach(1, ISRWatchDog);

  timeClient.begin();

  // Configuring buzzer and button pins
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_S1_PIN, INPUT_PULLUP);
  pinMode(BUTTON_S2_PIN, INPUT);

  digitalWrite(BUZZER_PIN, LOW); // Garante que o buzzer comece desligado

  // Initializing DS18B20 sensor
  sensors.begin();
  sensors.getAddress(sensorAddress, 0);
  sensors.setResolution(sensorAddress, 12); // Setting sensor resolution to 12 bits (0.0625°C)

  // Assigning the API key (required)
  config.api_key = API_KEY;
  // Assigning the user sign-in credentials
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  // Assigning the RTDB URL (required)
  config.database_url = DATABASE_URL;

  // Firebase.reconnectWiFi(true);
  Firebase.reconnectNetwork(true);
  fbdo.setResponseSize(4096);

  // Assigning the callback function for the long running token generation task
  config.token_status_callback = tokenStatusCallback; // see addons/TokenHelper.h

  // Assigning the maximum retry of token generation
  config.max_token_generation_retry = 5;

  // Initializing the library with the Firebase auth and config
  Firebase.begin(&config, &auth);

  // Getting the user UID might take a few seconds
  Serial.println("Getting User UID");
  while ((auth.token.uid) == "")
  {
    Serial.print('.');
    delay(1000);
  }
  // Printing user UID
  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.println(uid);
  Serial.print("Device Name: ");
  Serial.println(DEVICE_NAME);

  // Update Database main paths (to be updated in setup with the user UID)
  databasePath = "/UsersData/" + uid + "/devices/" + DEVICE_NAME;
  databaseAlertPath = databasePath + alertPath;
  databaseThresholdPath = databasePath + configPath + thresholdPath;

  Serial.println("Calling Firease Init.");
  firebaseInit();

  Serial.println("Configuracoes: ");
  getDeviceConfigurations();

  OTAsetup();
}

void checkButtons()
{
  // Serial.println("Entrei checkButton");
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
  // Serial.println("Sai checkButton");
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
      return; // Stop further execution if setup fails.
    }
    STA_NAME = "IoTemp Setup - " + STA_NAME;
    drawWifiNotConfigured();
    wifiManager.startConfigPortal(STA_NAME.c_str());
  }
  // Check the button state
  if (digitalRead(BUTTON_S1_PIN) == HIGH)
  {
    // Button is pressed
    if (buttonS1PressedTime == 0)
    {                                 // Check if the timer is not started yet
      buttonS1PressedTime = millis(); // Start the timer
    }
    else if (millis() - buttonS1PressedTime >= BUTTON_S1_HOLD_TIME)
    {
      // Button has been held for 5 seconds
      WiFiManager wifiManager;
      wifiManager.setDebugOutput(true);
      wifiManager.setConnectTimeout(60);
      wifiManager.setCleanConnect(true);
      if (!setupWiFiManager(wifiManager))
      {
        Serial.println("Failed to re-setup WiFi Manager");
        return; // Stop further execution if setup fails.
      }
      STA_NAME = "IoTemp Setup - " + STA_NAME;
      drawWifiNotConfigured();
      wifiManager.startConfigPortal(STA_NAME.c_str());
      ESP.reset();
      buttonS1PressedTime = 0; // Reset timer
    }
  }
  else
  {
    buttonS1PressedTime = 0; // Reset timer if the button is released
  }
}

void resetDevice()
{
  // Check the button state
  if (digitalRead(BUTTON_S2_PIN) == HIGH)
  {
    // Button is pressed
    if (buttonS2PressedTime == 0)
    {                                 // Check if the timer is not started yet
      buttonS2PressedTime = millis(); // Start the timer
    }
    else if (millis() - buttonS2PressedTime >= BUTTON_S2_HOLD_TIME)
    {
      // Button has been held for 10 seconds
      drawResetDevice();
      delay(2000);
      ESP.reset();
      buttonS2PressedTime = 0; // Reset the timer
    }
  }
  else
  {
    buttonS2PressedTime = 0; // Reset the timer if the button is released
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
