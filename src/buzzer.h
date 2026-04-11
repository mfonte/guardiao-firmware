#pragma once
#include <Arduino.h>
#include <config.h>

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
