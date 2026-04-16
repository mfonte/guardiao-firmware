#include <unity.h>
#include <cstring>
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
#include "EEPROMFunction.h"

void setUp(void)
{
  // Reset logic globals
  sensorError = false;
  temperature = 25.0;
  higherTemp = 80.0;
  lowerTemp = -80.0;
  thresholdMode = "both";
  countMessageSending = 0;
  // Reset EEPROM stub
  EEPROM.clear();
  // Reset pending queue state
  pendingQueueLen = 0;
  memset(pendingQueue, 0, sizeof(pendingQueue));
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

// ---- EEPROM string round-trip tests (3 total) ----

void test_eeprom_string_round_trip(void)
{
  String original("hello123");
  writeStringToEEPROM(0, original);
  String recovered = readStringFromEEPROM(0);
  TEST_ASSERT_EQUAL_STRING("hello123", recovered.c_str());
}

void test_eeprom_string_empty(void)
{
  String original("");
  writeStringToEEPROM(0, original);
  String recovered = readStringFromEEPROM(0);
  TEST_ASSERT_EQUAL_STRING("", recovered.c_str());
}

void test_eeprom_virgin_returns_empty(void)
{
  // EEPROM starts as 0xFF (virgin) -- readStringFromEEPROM should return ""
  String recovered = readStringFromEEPROM(0);
  TEST_ASSERT_EQUAL_STRING("", recovered.c_str());
}

// ---- Pending queue tests (3 total) ----

void test_pending_queue_enqueue_and_load(void)
{
  // Enqueue 3 readings
  enqueuePendingReading(25.5, 1000);
  enqueuePendingReading(26.0, 2000);
  enqueuePendingReading(27.5, 3000);
  // Reset in-memory state
  pendingQueueLen = 0;
  memset(pendingQueue, 0, sizeof(pendingQueue));
  // Load from EEPROM
  loadPendingQueue();
  TEST_ASSERT_EQUAL_UINT8(3, pendingQueueLen);
  TEST_ASSERT_EQUAL_FLOAT(25.5, pendingQueue[0].temperature);
  TEST_ASSERT_EQUAL_UINT32(1000, pendingQueue[0].timestamp);
  TEST_ASSERT_EQUAL_FLOAT(27.5, pendingQueue[2].temperature);
  TEST_ASSERT_EQUAL_UINT32(3000, pendingQueue[2].timestamp);
}

void test_pending_queue_overflow_drops_oldest(void)
{
  // Fill queue to MAXLEN
  for (int i = 0; i < PENDING_QUEUE_MAXLEN; i++)
  {
    enqueuePendingReading((float)(20 + i), (uint32_t)(1000 + i));
  }
  TEST_ASSERT_EQUAL_UINT8(PENDING_QUEUE_MAXLEN, pendingQueueLen);
  // One more -- should drop oldest
  enqueuePendingReading(99.0, 9999);
  TEST_ASSERT_EQUAL_UINT8(PENDING_QUEUE_MAXLEN, pendingQueueLen);
  // First entry should now be the second original (oldest dropped)
  TEST_ASSERT_EQUAL_UINT32(1001, pendingQueue[0].timestamp);
  // Last entry should be the new one
  TEST_ASSERT_EQUAL_FLOAT(99.0, pendingQueue[PENDING_QUEUE_MAXLEN - 1].temperature);
}

void test_pending_queue_virgin_eeprom_loads_empty(void)
{
  // EEPROM is 0xFF -- loadPendingQueue should set len=0
  loadPendingQueue();
  TEST_ASSERT_EQUAL_UINT8(0, pendingQueueLen);
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
  // EEPROM string tests
  RUN_TEST(test_eeprom_string_round_trip);
  RUN_TEST(test_eeprom_string_empty);
  RUN_TEST(test_eeprom_virgin_returns_empty);
  // Pending queue tests
  RUN_TEST(test_pending_queue_enqueue_and_load);
  RUN_TEST(test_pending_queue_overflow_drops_oldest);
  RUN_TEST(test_pending_queue_virgin_eeprom_loads_empty);
  return UNITY_END();
}
