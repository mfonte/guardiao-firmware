#pragma once
#include <Arduino.h>
#if defined(ESP32)
#include <WiFi.h>
#elif defined(ESP8266)
#include <ESP8266WiFi.h>
#endif
#include <SSD1306Wire.h>
#include <OLEDDisplayUi.h>
#include <OneWire.h>
#include <config.h>
#include <icons.h>

// --- Hardware Objects ---
OneWire oneWire(ONE_WIRE_BUS);
SSD1306Wire display(0x3c, D2, D1);
OLEDDisplayUi ui(&display);

// --- UI State ---
#define STATUS_BAR_H 12
#define CONTENT_Y STATUS_BAR_H

float prevTemperature = 0.0;
bool  prevSensorError = true;
// NTP anchor — written by getTime(), read by frameClock() for live seconds
unsigned long ntpAnchorEpoch  = 0;
unsigned long ntpAnchorMillis = 0;
unsigned long lastVccMs = 0;
bool firebaseConnected = false;

// ====================================================================
// FRAME CALLBACKS (drawn by OLEDDisplayUi)
// ====================================================================

/** Frame 0: Temperature — large reading with trend arrow. */
void frameTemperature(OLEDDisplay *d, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  d->setTextAlignment(TEXT_ALIGN_CENTER);
  d->setFont(ArialMT_Plain_16);
  d->drawString(64 + x, CONTENT_Y + y, DEVICE_NAME);

  if (sensorError)
  {
    d->setFont(ArialMT_Plain_16);
    d->drawString(64 + x, 34 + y, "Syncing...");
    return;
  }

  String tempStr = String(temperature, 1);
  // Measure widths to center the number+unit group horizontally
  d->setFont(ArialMT_Plain_24);
  int numW = d->getStringWidth(tempStr);
  d->setFont(ArialMT_Plain_10);
  int unitW = d->getStringWidth("\xC2\xB0" "C");
  int groupX = 64 - (numW + 2 + unitW) / 2;

  d->setFont(ArialMT_Plain_24);
  d->setTextAlignment(TEXT_ALIGN_LEFT);
  d->drawString(groupX + x, 28 + y, tempStr);

  d->setFont(ArialMT_Plain_10);
  d->setTextAlignment(TEXT_ALIGN_LEFT);
  d->drawString(groupX + numW + 2 + x, 28 + y, "\xC2\xB0" "C");

  // Navigation arrows near pagination bar
  d->drawXbm(2 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_left);

  if (!prevSensorError)
  {
    float diff = temperature - prevTemperature;
    const uint8_t *arrow = icon_arrow_right;
    if (diff > 0.3)
      arrow = icon_arrow_up;
    else if (diff < -0.3)
      arrow = icon_arrow_down;
    d->drawXbm(118 + x, 55 + y, ICON_SIZE, ICON_SIZE, arrow);
  }
}

/** Frame 1: Threshold — position bar + limits + current value. Only shown when threshold is set. */
void frameThreshold(OLEDDisplay *d, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  d->setTextAlignment(TEXT_ALIGN_CENTER);
  d->setFont(ArialMT_Plain_10);
  d->drawString(64 + x, CONTENT_Y + y, "Threshold");

  if (thresholdMode == "none")
  {
    d->setFont(ArialMT_Plain_10);
    d->drawString(64 + x, 34 + y, "Not configured");
    return;
  }

  float range = higherTemp - lowerTemp;
  if (range > 0 && !sensorError)
  {
    // Large bar — tall, center of screen
    int bx = 10, bw = 108, by = 28, bh = 10;
    d->drawRect(bx + x, by + y, bw, bh);
    float pos = constrain((temperature - lowerTemp) / range, 0.0f, 1.0f);
    int mx = bx + (int)(pos * (bw - 6));
    d->fillRect(mx + x, by + y, 6, bh);

    d->setTextAlignment(TEXT_ALIGN_LEFT);
    d->drawString(bx + x, by + bh + 4 + y, String((int)lowerTemp));
    d->setTextAlignment(TEXT_ALIGN_RIGHT);
    d->drawString(bx + bw + x, by + bh + 4 + y, String((int)higherTemp));
    d->setTextAlignment(TEXT_ALIGN_CENTER);
    d->drawString(64 + x, by + bh + 4 + y, String(temperature, 1) + "\xC2\xB0" "C");
  }
  else
  {
    d->drawString(64 + x, 34 + y, "No sensor data");
  }

  // Navigation arrows
  d->drawXbm(2 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_left);
  d->drawXbm(118 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_right);
}

/** Frame 1: Device status. */
void frameStatus(OLEDDisplay *d, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  d->setTextAlignment(TEXT_ALIGN_LEFT);
  d->setFont(ArialMT_Plain_10);
  int ly = CONTENT_Y + 2;
  d->drawString(2 + x, ly + y, firebaseConnected ? "Firebase: OK" : "Firebase: --");
  ly += 10;
  d->drawString(2 + x, ly + y, "Sent: " + String(countDataSentToFireBase));
  ly += 10;
  unsigned long upSec = millis() / 1000;
  d->drawString(2 + x, ly + y,
                "Up: " + String(upSec / 3600) + "h " + String((upSec % 3600) / 60) + "m");
  ly += 10;
  d->drawString(2 + x, ly + y, "Heap: " + String(ESP.getFreeHeap()) + "B");

  // Navigation arrows
  d->drawXbm(2 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_left);
  d->drawXbm(118 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_right);
}

/** Frame 2: WiFi details. */
void frameWifi(OLEDDisplay *d, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  String ip = WiFi.localIP().toString();
  if (ip == "(IP unset)" || ip == "0.0.0.0")
  {
    d->setTextAlignment(TEXT_ALIGN_CENTER);
    d->setFont(ArialMT_Plain_16);
    d->drawString(64 + x, 30 + y, "No WiFi");
    return;
  }
  d->setTextAlignment(TEXT_ALIGN_LEFT);
  d->setFont(ArialMT_Plain_10);
  int ly = CONTENT_Y + 2;
  d->drawString(2 + x, ly + y, "SSID: " + WiFi.SSID());
  ly += 10;
  d->drawString(2 + x, ly + y, "IP: " + ip);
  ly += 10;
  d->drawString(2 + x, ly + y, "Host: " + WiFi.hostname());
  ly += 10;
  d->drawString(2 + x, ly + y, "RSSI: " + String(WiFi.RSSI()) + " dBm");

  // Navigation arrows
  d->drawXbm(2 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_left);
  d->drawXbm(118 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_right);
}

/** Frame 3: Power supply / battery. */
void frameBattery(OLEDDisplay *d, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  if (millis() - lastVccMs >= 1000) {
    BATTERY_LEVEL = ESP.getVcc();
    lastVccMs = millis();
  }
  float v = BATTERY_LEVEL / 1000.0;
  d->setTextAlignment(TEXT_ALIGN_CENTER);
  d->setFont(ArialMT_Plain_24);
  d->drawString(64 + x, 16 + y, String(v, 2) + " V");
  d->setFont(ArialMT_Plain_10);
  if (v > 4.5)
    d->drawString(64 + x, 42 + y, "USB Powered");
  else if (v < 3.0)
    d->drawString(64 + x, 42 + y, "LOW BATTERY!");
  else
    d->drawString(64 + x, 42 + y, "Battery OK");

  // Navigation arrows
  d->drawXbm(2 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_left);
  d->drawXbm(118 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_right);
}

/** Frame N: Clock — HH:MM:SS + date from NTP. */
void frameClock(OLEDDisplay *d, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  // Interpolate current time from last NTP sync using millis() delta.
  // This keeps the clock ticking every second without blocking Firebase calls.
  unsigned long epoch = (ntpAnchorEpoch > 0)
    ? ntpAnchorEpoch + (millis() - ntpAnchorMillis) / 1000UL
    : (unsigned long)timestamp;
  if (epoch <= MIN_VALID_TIMESTAMP)
  {
    d->setTextAlignment(TEXT_ALIGN_CENTER);
    d->setFont(ArialMT_Plain_16);
    d->drawString(64 + x, 30 + y, "Syncing...");
    return;
  }

  epoch += (long)TIMEZONE_OFFSET_SEC;
  time_t t = (time_t)epoch;
  struct tm *ti = gmtime(&t);

  char timeBuf[9];
  char dateBuf[24];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
  snprintf(dateBuf, sizeof(dateBuf), "%04u-%02u-%02u", (uint16_t)(ti->tm_year + 1900), (uint8_t)(ti->tm_mon + 1), (uint8_t)ti->tm_mday);

  d->setTextAlignment(TEXT_ALIGN_CENTER);
  d->setFont(ArialMT_Plain_24);
  d->drawString(64 + x, 18 + y, timeBuf);
  d->setFont(ArialMT_Plain_10);
  d->drawString(64 + x, 46 + y, dateBuf);

  // Navigation arrows
  d->drawXbm(2 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_left);
  d->drawXbm(118 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_right);
}

FrameCallback uiFrames[] = {frameTemperature, frameThreshold, frameClock, frameStatus, frameWifi, frameBattery};
const uint8_t uiFrameCount = 6;

// ====================================================================
// OVERLAY — status bar (always visible at top)
// ====================================================================

void overlayStatusBar(OLEDDisplay *d, OLEDDisplayUiState *state)
{
  d->setColor(BLACK);
  d->fillRect(0, 0, 128, STATUS_BAR_H);
  d->setColor(WHITE);
  d->drawHorizontalLine(0, STATUS_BAR_H - 1, 128);

  int ix = 2, iy = 2;

  d->drawXbm(ix, iy, ICON_SIZE, ICON_SIZE,
             WiFi.status() == WL_CONNECTED ? icon_wifi : icon_wifi_off);
  ix += 11;

  d->drawXbm(ix, iy, ICON_SIZE, ICON_SIZE,
             firebaseConnected ? icon_sync_ok : icon_sync_err);
  ix += 11;

  if (thresholdMode != "none")
  {
    d->drawXbm(ix, iy, ICON_SIZE, ICON_SIZE, icon_bell);
  }

  if (state->currentFrame == 0)
  {
    // Temperature frame: show battery state icon in top-right
    float vbat = BATTERY_LEVEL / 1000.0;
    d->drawXbm(118, 2, ICON_SIZE, ICON_SIZE,
               vbat < 3.0 ? icon_battery_low : icon_battery);
  }
  else if (!sensorError)
  {
    d->setFont(ArialMT_Plain_10);
    d->setTextAlignment(TEXT_ALIGN_RIGHT);
    d->drawString(126, 0, String(temperature, 1) + "\xC2\xB0");
  }
}

OverlayCallback uiOverlays[] = {overlayStatusBar};
const uint8_t uiOverlayCount = 1;

// ====================================================================
// DISPLAY INITIALIZATION
// ====================================================================

void initDisplay()
{
  display.init();
  display.flipScreenVertically();
}

void initUIFramework()
{
  ui.setTargetFPS(30);
  ui.disableAutoTransition();
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(uiFrames, uiFrameCount);
  ui.setOverlays(uiOverlays, uiOverlayCount);
  ui.setIndicatorPosition(BOTTOM);
  ui.setIndicatorDirection(LEFT_RIGHT);
  ui.init();
  display.flipScreenVertically(); // re-apply after ui.init() resets orientation
}

// ====================================================================
// MODAL SCREENS (bypass UI framework)
// ====================================================================

void drawBootProgress(const char *label, uint8_t progress)
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 6, "Guardiao");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 28, label);
  display.drawProgressBar(10, 44, 108, 12, progress);
  display.display();
}


void drawWifiReconnecting()
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 10, "Sem WiFi");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 38, "Reconectando...");
  display.display();
}

void drawResetDevice()
{
  display.clear();
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 5, "Restarting...");
  display.drawString(64, 30, DEVICE_NAME);
  display.display();
}

/**
 * Displays a named boot error on the OLED before restarting.
 * @param reason Short description of the failure (max ~20 chars for readability)
 */
void drawBootError(const char *reason)
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 4, "Erro de Boot");
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 26, reason);
  display.drawString(64, 44, "Reiniciando...");
  display.display();
}

void drawWifiNotConfigured(const String &ssid)
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 4, "Connect to WiFi:");
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 22, ssid);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 42, "using your phone");
  display.display();
}
