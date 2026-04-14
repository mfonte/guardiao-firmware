# Coding Conventions

**Analysis Date:** 2026-04-11

## Naming Patterns

**Files:**
- Header modules: `camelCase.h` — `drawOled.h`, `EEPROMFunction.h`, `OTAUpdate.h`, `icons.h`
- Main source: `main.cpp`
- Config template: `config.example.h`

**Functions:**
- `camelCase()` for all — `callFirebase()`, `readTemperature()`, `checkThresholdAlert()`
- Frame callbacks: `frame{Name}()` — `frameTemperature()`, `frameClock()`, `frameBattery()`
- Draw modal screens: `draw{What}()` — `drawAlarmScreen()`, `drawBootProgress()`, `drawResetDevice()`
- Buzzer: `beep{Event}()` — `beepBoot()`, `beepSuccess()`, `beepError()`, `beepConfirm()`

**Variables:**
- Compile-time constants: `UPPER_SNAKE` — `GET_DATA_INTERVAL`, `BUTTON_S1_HOLD_TIME`, `ICON_SIZE`
- Global runtime config: `UPPER_SNAKE` — `DEVICE_NAME`, `USER_EMAIL`, `DEVICE_UUID`
- Local / state variables: `camelCase` — `higherTemp`, `lastVccMs`, `watchDogCount`
- Enum values: `UPPER_SNAKE` — `DMODE_NORMAL`, `ALARM_OFF`, `ALARM_SOUNDING`
- EEPROM address constants: `UPPER_SNAKE` — `EMAIL_ADDR`, `LDID_ADDR`

**Icons (PROGMEM):**
- `icon_{name}[]` snake_case — `icon_wifi`, `icon_arrow_left`, `icon_battery_low`

## Code Style

**Formatting:**
- No formatter config detected (no `.clang-format`)
- Brace style: Allman — opening brace on new line
- Indentation: 2 spaces

**Header guards:**
- Use `#pragma once` everywhere (not `#ifndef` guards)

## Import Organization

**Order in `main.cpp`:**
1. `<Arduino.h>` (platform)
2. Platform-conditional WiFi (`<ESP8266WiFi.h>` or `<WiFi.h>`)
3. Third-party library headers (`<WiFiManager.h>`, `<DallasTemperature.h>`)
4. Local headers via angle brackets (`<EEPROMFunction.h>`, `<drawOled.h>`) — PlatformIO resolves from `src/`
5. Firebase addon relative paths (`"addons/TokenHelper.h"`)

## Documentation Style

All non-trivial functions have a JSDoc-style block comment:
```cpp
/**
 * Brief description.
 * Extra context if needed.
 * @param paramName  Description.
 * @return Description of return value.
 */
```

Section dividers used in large files:
```cpp
// ====================================================================
// SECTION NAME
// ====================================================================
```

## Error Handling

**Patterns:**
- Sensor errors: return sentinel float (`888.0` = error, `-127.0` = disconnected)
- Guard before use: `if (temperature == 888.0 || temperature == -127.0) return;`
- Firebase errors: early return after `!Firebase.ready()` check with `Serial.println()`
- Critical failures: `Serial.flush(); Serial.end(); ESP.restart();`

## Logging

**Framework:** Arduino `Serial` (no structured logging library)

**Patterns:**
- Every non-trivial function: `Serial.println("Entering functionName")` / `Serial.println("Leaving functionName")`
- Errors: `Serial.println("Error: description.")`
- Values: `Serial.printf("Key: %.1f\n", value)` or `Serial.print()` + `Serial.println()`

## Memory Conventions

- All strings use Arduino `String` class — no `std::string`
- Icon bitmaps: `static const uint8_t[] PROGMEM` — stored in flash, read-only
- String literals in print/draw: wrap in `F()` macro where possible — `Serial.print(F("literal"))`
- EEPROM reads at boot only; writes only when config changes

## Display Code Conventions

**Frame callback signature (always):**
```cpp
void frameXxx(OLEDDisplay *d, OLEDDisplayUiState *state, int16_t x, int16_t y)
```

**Always add offset to coordinates** (enables slide animation):
```cpp
d->drawString(64 + x, 20 + y, text);
d->drawXbm(2 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_left);
```

**Font hierarchy:**
- Section titles / device name: `ArialMT_Plain_16`
- Primary value: `ArialMT_Plain_24`
- Secondary data, labels: `ArialMT_Plain_10`

**Horizontal centering (for variable-width content):**
```cpp
d->setFont(ArialMT_Plain_24);
int numW = d->getStringWidth(valueStr);
d->setFont(ArialMT_Plain_10);
int unitW = d->getStringWidth(unitStr);
int groupX = 64 - (numW + gap + unitW) / 2;
```

**Degree symbol — always use UTF-8 two-byte sequence:**
```cpp
d->drawString(x, y, "\xC2\xB0" "C");  // correct
// NOT: "\xB0C" — single byte is invalid UTF-8
```

**Nav arrows on all frames (copy exactly):**
```cpp
d->drawXbm(2 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_left);
d->drawXbm(118 + x, 55 + y, ICON_SIZE, ICON_SIZE, icon_arrow_right);
```

---

*Convention analysis: 2026-04-11*
