#pragma once
#include <stdint.h>

// Forward-declare globals (defined in config.h for production, in test file for native)
extern bool sensorError;
extern float temperature;
extern float higherTemp;
extern float lowerTemp;
extern String thresholdMode;
extern int countMessageSending;

/**
 * Builds a status bitmap for protocol v2 heartbeat messages.
 * bit 0: online, bit 2: sensor read error, bit 4: threshold alert active.
 */
inline uint8_t calculateStatusBitmap()
{
  uint8_t st = 0x01; // always online (bit 0)
  if (sensorError)
    st |= 0x04; // error reading
  bool highAlert = (thresholdMode == "both" || thresholdMode == "upperOnly") && temperature > higherTemp;
  bool lowAlert = (thresholdMode == "both" || thresholdMode == "lowerOnly") && temperature < lowerTemp;
  if (highAlert || lowAlert)
    st |= 0x10; // alert
  return st;
}

/**
 * Evaluates the current temperature against thresholds.
 * Returns: 0 = within limits, 1 = overHigh, -1 = underLow, -2 = sensorError, -3 = disabled
 */
inline int evaluateThreshold()
{
  if (sensorError)
    return -2;
  if (thresholdMode == "none")
    return -3;
  bool overHigh = (thresholdMode == "both" || thresholdMode == "upperOnly") && temperature > higherTemp;
  bool underLow = (thresholdMode == "both" || thresholdMode == "lowerOnly") && temperature < lowerTemp;
  if (overHigh)
    return 1;
  if (underLow)
    return -1;
  return 0;
}
