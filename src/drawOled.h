#include <SSD1306Wire.h>
#include <OneWire.h>
#include <EEPROMFunction.h>
#include <config.h>

OneWire oneWire(ONE_WIRE_BUS);
// Initializing OLED display
// SSD1306  display(0x3c, SDA, SCL);
SSD1306Wire display(0x3c, D2, D1);

void clearDisplay() {
  display.clear();
  display.display();
  display.drawVerticalLine(0, 0, 63);
  display.drawVerticalLine(127, 0, 63);
  display.drawHorizontalLine(0, 0, 127);
  display.drawHorizontalLine(0, 63, 127);
}

void showWifiSSID() {
  clearDisplay();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 20, "Connecting to network: ");
  display.drawString(64, 40, WIFI_SSID);
  display.display();
}

void showIP() {
  clearDisplay();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(64, 20, "IP:");
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(64, 40, WiFi.localIP().toString());
  display.display();
}

void drawFindingWifi() {
  clearDisplay();   /* clear the display */
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(1, 12, "Conexão perdida.");
  display.drawString(1, 32, "Procurando rede...");
  display.display(); /* write the buffer to the display */
}

/* This function draws the WiFi SSID and IP address on OLED Display */
void drawWifiDetail() {
  String wifiSSID = WiFi.SSID();
  String wifiIP = WiFi.localIP().toString();
  String STA = WiFi.hostname();

  if (wifiIP == "(IP unset)") {
    drawFindingWifi();
  } else {
    clearDisplay();   /* clear the display */
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_16);
    display.drawString(2, 5, STA);
    display.drawString(2, 24, "AP: " + String(wifiSSID));
    display.drawString(2, 44, "IP: " + wifiIP);
    display.display(); /* write the buffer to the display */
  }
}

void drawTemperature() {
  /* create more fonts at http://oleddisplay.squix.ch/ */
  if (temperature < 0) {
    String text_temp = String(temperature);
    clearDisplay(); // clear the display
    display.setColor(WHITE);
    display.drawCircle(87, 40, 4);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 12, DEVICE_NAME);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_24);
    display.drawString(15, 32, text_temp);
    display.drawString(95, 32, "C");
  } else {
    String text_temp = String(temperature);
    clearDisplay();   /* clear the display */
    display.drawCircle(81, 40, 4);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 12 , DEVICE_NAME);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_24);
    display.drawString(16, 32, text_temp);
    display.drawString(91, 32, "C");
  }
  display.display(); /* write the buffer to the display */
}

void drawWaitingForTemperature() {
  String text_temp = String(temperature);
  clearDisplay();   // clear the display
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 12, DEVICE_NAME);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(5, 40, "Sincronizando...");
  display.display(); /* write the buffer to the display */
}

/* This function draws "ALARM ON" on OLED Display */
void drawAlarm() {
  clearDisplay(); // clear the display
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 22, ALARM_MESSAGE);
  display.display(); // write the buffer to the display
}

void drawResetDevice() {
  clearDisplay();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(62, 5, "Reiniciando...");
  display.drawString(62, 30, DEVICE_NAME);
  display.display();
}

void drawWifiNotConfigured() {
  clearDisplay();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(62, 4, "Conecte-se ao WiFi");
  display.setFont(ArialMT_Plain_16);
  display.drawString(62, 22, "IoTemp Setup");
  display.setFont(ArialMT_Plain_10);
  display.drawString(62, 42, "pelo celular");
  display.display();
}

void drawBatteryLevel() {
  clearDisplay();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(62, 5, "Bateria");
  display.setFont(ArialMT_Plain_24);
  display.drawString(62, 30, String(BATTERY_LEVEL) + " V");
  display.display();
}