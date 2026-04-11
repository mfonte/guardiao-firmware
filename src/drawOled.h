#include <SSD1306Wire.h>
#include <OneWire.h>
#include <EEPROMFunction.h>
#include <config.h>

OneWire oneWire(ONE_WIRE_BUS);

// Initialise the OLED display on I2C address 0x3C (SDA=D2, SCL=D1)
SSD1306Wire display(0x3c, D2, D1);

/** Clears the display and redraws the border frame. */
void clearDisplay()
{
  display.clear();
  display.display();
  display.drawVerticalLine(0, 0, 63);
  display.drawVerticalLine(127, 0, 63);
  display.drawHorizontalLine(0, 0, 127);
  display.drawHorizontalLine(0, 63, 127);
}

/** Shows the WiFi SSID the device is connecting to (used during boot). */
void showWifiSSID()
{
  clearDisplay();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 20, "Connecting to network: ");
  display.drawString(64, 40, WIFI_SSID);
  display.display();
}

/** Shows the device's obtained IP address on the OLED. */
void showIP()
{
  clearDisplay();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(64, 20, "IP:");
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(64, 40, WiFi.localIP().toString());
  display.display();
}

/** Displays a "connection lost, searching for network" message. */
void drawFindingWifi()
{
  clearDisplay(); /* clear the display */
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(1, 12, "Connection lost.");
  display.drawString(1, 32, "Searching for network...");
  display.display(); /* write the buffer to the display */
}

/** Shows the connected WiFi SSID, IP address, and hostname on the OLED. */
/* This function draws the WiFi SSID and IP address on OLED Display */
void drawWifiDetail()
{
  String wifiSSID = WiFi.SSID();
  String wifiIP = WiFi.localIP().toString();
  String STA = WiFi.hostname();

  if (wifiIP == "(IP unset)")
  {
    drawFindingWifi();
  }
  else
  {
    clearDisplay(); /* clear the display */
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_16);
    display.drawString(2, 5, STA);
    display.drawString(2, 24, "AP: " + String(wifiSSID));
    display.drawString(2, 44, "IP: " + wifiIP);
    display.display(); /* write the buffer to the display */
  }
}

/** Renders the current temperature value and device name on the OLED. */
void drawTemperature()
{
  /* create more fonts at http://oleddisplay.squix.ch/ */
  if (temperature < 0)
  {
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
  }
  else
  {
    String text_temp = String(temperature);
    clearDisplay(); /* clear the display */
    display.drawCircle(81, 40, 4);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setFont(ArialMT_Plain_16);
    display.drawString(64, 12, DEVICE_NAME);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    display.setFont(ArialMT_Plain_24);
    display.drawString(16, 32, text_temp);
    display.drawString(91, 32, "C");
  }
  display.display(); /* write the buffer to the display */
}

/** Displays a "syncing" placeholder while waiting for the first valid reading. */
void drawWaitingForTemperature()
{
  String text_temp = String(temperature);
  clearDisplay(); // clear the display
  display.setColor(WHITE);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 12, DEVICE_NAME);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(5, 40, "Syncing...");
  display.display(); /* write the buffer to the display */
}

/** Displays the ALARM_MESSAGE string centred on the OLED during an alarm event. */
/* This function draws "ALARM ON" on OLED Display */
void drawAlarm()
{
  clearDisplay(); // clear the display
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 22, ALARM_MESSAGE);
  display.display(); // write the buffer to the display
}

/** Displays a restart notice with device name while the device reboots. */
void drawResetDevice()
{
  clearDisplay();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(62, 5, "Restarting...");
  display.drawString(62, 30, DEVICE_NAME);
  display.display();
}

/** Prompts the user to configure WiFi by joining the "IoTemp Setup" access point. */
void drawWifiNotConfigured()
{
  clearDisplay();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(62, 4, "Connect to WiFi");
  display.setFont(ArialMT_Plain_16);
  display.drawString(62, 22, "IoTemp Setup");
  display.setFont(ArialMT_Plain_10);
  display.drawString(62, 42, "using your phone");
  display.display();
}

/** Displays the current supply voltage (battery level) in Volts. */
void drawBatteryLevel()
{
  clearDisplay();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(62, 5, "Battery");
  display.setFont(ArialMT_Plain_24);
  display.drawString(62, 30, String(BATTERY_LEVEL) + " V");
  display.display();
}
