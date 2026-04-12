#include <unity.h>
#include "Arduino.h"
#include "EEPROM.h"

// Define globals used by logic.h
bool sensorError;
float temperature;
float higherTemp;
float lowerTemp;
String thresholdMode;
int countMessageSending;

// Instantiate stubs
SerialStub Serial;
EEPROMClass EEPROM;

#include "logic.h"

void setUp(void)
{
  sensorError = false;
  temperature = 25.0;
  higherTemp = 80.0;
  lowerTemp = -80.0;
  thresholdMode = "both";
  countMessageSending = 0;
}

void tearDown(void) {}

// Smoke test: normal state = online only
void test_bitmap_normal(void)
{
  TEST_ASSERT_EQUAL_HEX8(0x01, calculateStatusBitmap());
}

// Smoke test: threshold within limits = 0
void test_threshold_within_limits(void)
{
  TEST_ASSERT_EQUAL_INT(0, evaluateThreshold());
}

int main(void)
{
  UNITY_BEGIN();
  RUN_TEST(test_bitmap_normal);
  RUN_TEST(test_threshold_within_limits);
  return UNITY_END();
}
