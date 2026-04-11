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

// --- UI Constants ---
#define STATUS_BAR_H 12
#define CONTENT_Y STATUS_BAR_H

// --- UI State ---
enum DisplayMode { DMODE_NORMAL, DMODE_ALARM, DMODE_MODAL };
DisplayMode displayMode = DMODE_NORMAL;

enum AlarmState { ALARM_OFF, ALARM_SOUNDING };
AlarmState alarmState = ALARM_OFF;
unsigned long alarmStartMs = 0;
unsigned long alarmToneMs = 0;
unsigned int alarmFreq = 2000;
bool alarmToneOn = false;

float prevTemperature = -127.0;
bool firebaseConnected = false;

unsigned long s1PressStart = 0;
unsigned long s2PressStart = 0;
bool s1Handled = false;
bool s2Handled = false;

void openConfigPortal();

// ====================================================================
// FRAME CALLBACKS (drawn by OLEDDisplayUi)
// ====================================================================

/** Frame 0: Temperature with trend arrow and threshold bar. */
void frameTemperature(OLEDDisplay *d, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  d->setTextAlignment(TEXT_ALIGN_CENTER);
  d->setFont(ArialMT_Plain_10);
  d->drawString(64 + x, CONTENT_Y + y, DEVICE_NAME);

  if (temperature == -127.0 || temperature == 888.0)
  {
    d->setFont(ArialMT_Plain_16);
    d->drawString(64 + x, 30 + y, "Syncing...");
    return;
  }

  String tempStr = String(temperature, 1);
  d->setFont(ArialMT_Plain_24);
  d->setTextAlignment(TEXT_ALIGN_RIGHT);
  d->drawString(95 + x, 24 + y, tempStr);

  d->setFont(ArialMT_Plain_10);
  d->setTextAlignment(TEXT_ALIGN_LEFT);
  d->drawString(97 + x, 24 + y, "\xB0" "C");

  if (prevTemperature != -127.0 && prevTemperature != 888.0)
  {
    float diff = temperature - prevTemperature;
    const uint8_t *arrow = icon_arrow_right;
    if (diff > 0.3)
      arrow = icon_arrow_up;
    else if (diff < -0.3)
      arrow = icon_arrow_down;
    d->drawXbm(4 + x, 30 + y, ICON_SIZE, ICON_SIZE, arrow);
  }

  if (thresholdMode != "none")
  {
    float range = higherTemp - lowerTemp;
    if (range > 0)
    {
      int bx = 14, bw = 100, by = 55;
      d->drawRect(bx + x, by + y, bw, 6);
      float pos = constrain((temperature - lowerTemp) / range, 0.0f, 1.0f);
      int mx = bx + (int)(pos * (bw - 4));
      d->fillRect(mx + x, by + y, 4, 6);
      d->setFont(ArialMT_Plain_10);
      d->setTextAlignment(TEXT_ALIGN_LEFT);
      d->drawString(bx + x - 12, by + y - 2, String((int)lowerTemp));
      d->setTextAlignment(TEXT_ALIGN_RIGHT);
      d->drawString(bx + bw + x + 14, by + y - 2, String((int)higherTemp));
    }
  }
}

/** Frame 1: Device status. */
void frameStatus(OLEDDisplay *d, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  d->setTextAlignment(TEXT_ALIGN_LEFT);
  d->setFont(ArialMT_Plain_10);
  int ly = CONTENT_Y + 2;
  d->drawString(2 + x, ly + y, firebaseConnected ? "Firebase: OK" : "Firebase: --");
  ly += 13;
  d->drawString(2 + x, ly + y, "Sent: " + String(countDataSentToFireBase));
  ly += 13;
  unsigned long upSec = millis() / 1000;
  d->drawString(2 + x, ly + y,
                "Up: " + String(upSec / 3600) + "h " + String((upSec % 3600) / 60) + "m");
  ly += 13;
  d->drawString(2 + x, ly + y, "Heap: " + String(ESP.getFreeHeap()) + "B");
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
  ly += 13;
  d->drawString(2 + x, ly + y, "IP: " + ip);
  ly += 13;
  d->drawString(2 + x, ly + y, "Host: " + WiFi.hostname());
  ly += 13;
  d->drawString(2 + x, ly + y, "RSSI: " + String(WiFi.RSSI()) + " dBm");
}

/** Frame 3: Power supply / battery. */
void frameBattery(OLEDDisplay *d, OLEDDisplayUiState *state, int16_t x, int16_t y)
{
  BATTERY_LEVEL = ESP.getVcc();
  float v = BATTERY_LEVEL / 1000.0;
  d->setTextAlignment(TEXT_ALIGN_CENTER);
  d->setFont(ArialMT_Plain_10);
  d->drawString(64 + x, CONTENT_Y + 2 + y, "Power Supply");
  d->setFont(ArialMT_Plain_24);
  d->drawString(64 + x, 26 + y, String(v, 2) + " V");
  d->setFont(ArialMT_Plain_10);
  if (v > 4.5)
    d->drawString(64 + x, 52 + y, "USB Powered");
  else if (v < 3.0)
    d->drawString(64 + x, 52 + y, "LOW BATTERY!");
  else
    d->drawString(64 + x, 52 + y, "Battery OK");
}

FrameCallback uiFrames[] = {frameTemperature, frameStatus, frameWifi, frameBattery};
const uint8_t uiFrameCount = 4;

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

  if (state->currentFrame != 0 && temperature > -126.0 && temperature < 887.0)
  {
    d->setFont(ArialMT_Plain_10);
    d->setTextAlignment(TEXT_ALIGN_RIGHT);
    d->drawString(126, 0, String(temperature, 1) + "\xB0");
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
  ui.init();
  display.flipScreenVertically(); // re-apply after ui.init() resets orientation
}

// ====================================================================
// MODAL SCREENS (bypass UI framework)
// ====================================================================

void drawAlarmScreen()
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 2, "! ALARM !");
  display.drawHorizontalLine(0, 13, 128);
  display.setFont(ArialMT_Plain_24);
  display.drawString(64, 18, ALARM_MESSAGE);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 52, "Press any button to stop");
  display.display();
}

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

void drawHoldProgress(const char *action, uint8_t pct)
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 4, action);
  display.drawProgressBar(10, 22, 108, 12, pct);
  display.drawString(64, 42, pct < 100 ? "Hold..." : "Releasing...");
  display.drawString(64, 54, "Release to cancel");
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

// ====================================================================
// NON-BLOCKING ALARM
// ====================================================================

void startAlarm()
{
  alarmState = ALARM_SOUNDING;
  alarmStartMs = millis();
  alarmFreq = 2000;
  alarmToneOn = false;
  alarmToneMs = millis();
  displayMode = DMODE_ALARM;
  drawAlarmScreen();
}

bool updateAlarm()
{
  if (alarmState != ALARM_SOUNDING)
    return false;

  if (digitalRead(BUTTON_S1_PIN) == HIGH || digitalRead(BUTTON_S2_PIN) == HIGH)
  {
    noTone(BUZZER_PIN);
    alarmState = ALARM_OFF;
    displayMode = DMODE_NORMAL;
    delay(200);
    return false;
  }

  if (millis() - alarmStartMs >= 30000)
  {
    noTone(BUZZER_PIN);
    alarmState = ALARM_OFF;
    displayMode = DMODE_NORMAL;
    return false;
  }

  unsigned long el = millis() - alarmToneMs;
  if (alarmToneOn && el >= 200)
  {
    noTone(BUZZER_PIN);
    alarmToneOn = false;
    alarmToneMs = millis();
    alarmFreq = max(500u, alarmFreq - 50u);
  }
  else if (!alarmToneOn && el >= 100)
  {
    tone(BUZZER_PIN, alarmFreq, 200);
    alarmToneOn = true;
    alarmToneMs = millis();
  }

  return true;
}

// ====================================================================
// BUZZER FEEDBACK
// ====================================================================

void beepConfirm() { tone(BUZZER_PIN, 1000, 50); }

void beepSuccess()
{
  tone(BUZZER_PIN, 800, 80);
  delay(100);
  tone(BUZZER_PIN, 1200, 80);
  delay(80);
  noTone(BUZZER_PIN);
}

void beepError()
{
  for (int i = 0; i < 3; i++)
  {
    tone(BUZZER_PIN, 400, 50);
    delay(80);
    noTone(BUZZER_PIN);
    delay(40);
  }
}

void beepBoot()
{
  tone(BUZZER_PIN, 523, 80);
  delay(100);
  tone(BUZZER_PIN, 659, 80);
  delay(100);
  tone(BUZZER_PIN, 784, 120);
  delay(120);
  noTone(BUZZER_PIN);
}

// ====================================================================
// BUTTON HANDLING
// ====================================================================

void handleButtons()
{
  if (alarmState == ALARM_SOUNDING)
    return;

  bool s1 = digitalRead(BUTTON_S1_PIN) == HIGH;
  bool s2 = digitalRead(BUTTON_S2_PIN) == HIGH;

  // ---- S1 (previous frame / portal on hold) ----
  if (s1)
  {
    if (s1PressStart == 0)
    {
      s1PressStart = millis();
      s1Handled = false;
    }
    unsigned long held = millis() - s1PressStart;

    if (held >= 1000 && held < (unsigned long)BUTTON_S1_HOLD_TIME && !s1Handled)
    {
      uint8_t pct = (uint8_t)((held - 1000) * 100UL / (BUTTON_S1_HOLD_TIME - 1000));
      drawHoldProgress("Open WiFi Portal", pct);
      displayMode = DMODE_MODAL;
    }
    if (held >= (unsigned long)BUTTON_S1_HOLD_TIME && !s1Handled)
    {
      s1Handled = true;
      openConfigPortal();
    }
  }
  else
  {
    if (s1PressStart > 0 && !s1Handled)
    {
      unsigned long held = millis() - s1PressStart;
      if (held > 50 && held < 1000)
      {
        ui.previousFrame();
        beepConfirm();
      }
      displayMode = DMODE_NORMAL;
    }
    s1PressStart = 0;
    s1Handled = false;
  }

  // ---- S2 (next frame / factory reset on hold) ----
  if (s2)
  {
    if (s2PressStart == 0)
    {
      s2PressStart = millis();
      s2Handled = false;
    }
    unsigned long held = millis() - s2PressStart;

    if (held >= 1000 && held < (unsigned long)BUTTON_S2_HOLD_TIME && !s2Handled)
    {
      uint8_t pct = (uint8_t)((held - 1000) * 100UL / (BUTTON_S2_HOLD_TIME - 1000));
      drawHoldProgress("Factory Reset", pct);
      displayMode = DMODE_MODAL;
    }
    if (held >= (unsigned long)BUTTON_S2_HOLD_TIME && !s2Handled)
    {
      s2Handled = true;
      drawResetDevice();
      delay(2000);
      ESP.reset();
    }
  }
  else
  {
    if (s2PressStart > 0 && !s2Handled)
    {
      unsigned long held = millis() - s2PressStart;
      if (held > 50 && held < 1000)
      {
        ui.nextFrame();
        beepConfirm();
      }
      displayMode = DMODE_NORMAL;
    }
    s2PressStart = 0;
    s2Handled = false;
  }
}
