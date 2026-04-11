#pragma once
#include <Arduino.h>
#include <SSD1306Wire.h>
#include <config.h>

// Hardware object created in drawOled.h
extern SSD1306Wire display;

// ====================================================================
// NON-BLOCKING ALARM
// ====================================================================

unsigned long alarmStartMs = 0;
unsigned long alarmToneMs = 0;
unsigned int alarmFreq = 2000;
bool alarmToneOn = false;

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
