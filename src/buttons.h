#pragma once
#include <Arduino.h>
#include <SSD1306Wire.h>
#include <OLEDDisplayUi.h>
#include <config.h>
#include <buzzer.h>

// Hardware objects created in drawOled.h
extern SSD1306Wire display;
extern OLEDDisplayUi ui;

// Forward declaration from main.cpp
void openConfigPortal();

// ====================================================================
// BUTTON STATE
// ====================================================================

unsigned long s1PressStart = 0;
unsigned long s2PressStart = 0;
bool s1Handled = false;
bool s2Handled = false;

// ====================================================================
// HOLD PROGRESS SCREEN
// ====================================================================

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
