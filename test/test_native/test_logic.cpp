#include <unity.h>
#include "Arduino.h"
#include "EEPROM.h"

// Define globals used by logic.h
bool     sensorError;
float    temperature;
float    higherTemp;
float    lowerTemp;
String   thresholdMode;
int      countMessageSending;

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

// ---- calculateStatusBitmap() tests (6 total) ----

void test_bitmap_normal(void)
{
  TEST_ASSERT_EQUAL_HEX8(0x01, calculateStatusBitmap());
}

void test_bitmap_sensor_error(void)
{
  sensorError = true;
  TEST_ASSERT_EQUAL_HEX8(0x05, calculateStatusBitmap());
}

void test_bitmap_high_alert(void)
{
  temperature = 90.0;
  TEST_ASSERT_EQUAL_HEX8(0x11, calculateStatusBitmap());
}

void test_bitmap_low_alert(void)
{
  temperature = -90.0;
  TEST_ASSERT_EQUAL_HEX8(0x11, calculateStatusBitmap());
}

void test_bitmap_sensor_error_and_alert(void)
{
  sensorError = true;
  temperature = 90.0;
  TEST_ASSERT_EQUAL_HEX8(0x15, calculateStatusBitmap());
}

void test_bitmap_mode_none_no_alert(void)
{
  temperature = 90.0;
  thresholdMode = "none";
  TEST_ASSERT_EQUAL_HEX8(0x01, calculateStatusBitmap());
}

// ---- evaluateThreshold() tests (7 total) ----

void test_threshold_within_limits(void)
{
  TEST_ASSERT_EQUAL_INT(0, evaluateThreshold());
}

void test_threshold_over_high_both(void)
{
  temperature = 90.0;
  TEST_ASSERT_EQUAL_INT(1, evaluateThreshold());
}

void test_threshold_under_low_both(void)
{
  temperature = -90.0;
  TEST_ASSERT_EQUAL_INT(-1, evaluateThreshold());
}

void test_threshold_over_high_upper_only(void)
{
  thresholdMode = "upperOnly";
  temperature = 90.0;
  TEST_ASSERT_EQUAL_INT(1, evaluateThreshold());
}

void test_threshold_under_low_lower_only(void)
{
  thresholdMode = "lowerOnly";
  temperature = -90.0;
  TEST_ASSERT_EQUAL_INT(-1, evaluateThreshold());
}

void test_threshold_sensor_error(void)
{
  sensorError = true;
  TEST_ASSERT_EQUAL_INT(-2, evaluateThreshold());
}

void test_threshold_mode_none(void)
{
  thresholdMode = "none";
  temperature = 90.0;
  TEST_ASSERT_EQUAL_INT(-3, evaluateThreshold());
}

int main(void)
{
  UNITY_BEGIN();
  // Bitmap tests
  RUN_TEST(test_bitmap_normal);
  RUN_TEST(test_bitmap_sensor_error);
  RUN_TEST(test_bitmap_high_alert);
  RUN_TEST(test_bitmap_low_alert);
  RUN_TEST(test_bitmap_sensor_error_and_alert);
  RUN_TEST(test_bitmap_mode_none_no_alert);
  // Threshold tests
  RUN_TEST(test_threshold_within_limits);
  RUN_TEST(test_threshold_over_high_both);
  RUN_TEST(test_threshold_under_low_both);
  RUN_TEST(test_threshold_over_high_upper_only);
  RUN_TEST(test_threshold_under_low_lower_only);
  RUN_TEST(test_threshold_sensor_error);
  RUN_TEST(test_threshold_mode_none);
  return UNITY_END();
}
