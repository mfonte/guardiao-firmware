# Codebase Concerns

**Analysis Date:** 2026-04-11

## Security

**Firebase API Key exposed in git history:**
- Risk: `API_KEY` was committed to history before `src/config.h` was gitignored
- Files: `src/config.h` (gitignored now), but key appears in `git log`
- Current mitigation: `src/config.h` is gitignored; `config.example.h` uses placeholder
- Required action: Rotate the Firebase API key in Firebase Console. Prune history with `git filter-repo` if repo is public.

**No input validation on WiFiManager parameters:**
- Risk: Email, password, device name accepted as-is from captive portal, written directly to EEPROM
- Files: `src/main.cpp` `setupWiFiManager()` lines ~84-106
- Impact: Overly long inputs silently truncated to `EEPROM_SIZE - addrOffset - 2`; no user feedback

## Tech Debt

**Global variables defined in header (`src/config.h`):**
- Issue: `config.h` contains variable *definitions* (not declarations) — `String DEVICE_NAME;`, `float higherTemp = 80.0;` etc. Only safe because `#pragma once` prevents multi-include.
- Files: `src/config.h` lines 13-100
- Impact: Violates C++ convention (headers declare, `.cpp` defines); fragile if convention breaks
- Fix: Move definitions to `main.cpp`, add `extern` declarations to `config.h`

**`src/drawOled.h` is a 490-line catch-all:**
- Issue: Mixes display rendering, button handling, alarm state machine, buzzer sequences, OLEDDisplayUi initialization, and modal screens in one file
- Files: `src/drawOled.h`
- Impact: Hard to navigate; any change risks breaking unrelated subsystems
- Fix: Split into `buttons.h`, `alarm.h`, `buzzer.h`, keeping `drawOled.h` for pure rendering

**Sentinel temperature values:**
- Issue: `-127.0` (disconnected) and `888.0` (error) are magic floats compared with `==`
- Files: `src/main.cpp` (`callFirebase`, `checkThresholdAlert`, `sendDataToFireBase`), `src/drawOled.h` (`frameTemperature`, `overlayStatusBar`)
- Impact: Floating-point `==` is fragile; sentinel comparison scattered across files
- Fix: Add `bool sensorError = false;` flag set by `readTemperature()`; replace all float comparisons with flag check

**Arduino `String` heap fragmentation:**
- Issue: Heavy String concatenation in `main.cpp` and `drawOled.h` (`"ldid-" + DEVICE_UUID + "-001"`, path building)
- Files: `src/main.cpp`, `src/drawOled.h`
- Impact: ESP8266 has ~80 KB RAM; String fragmentation can cause crashes after extended uptime (days)
- Fix: Use `snprintf` into fixed char arrays for known-length strings; move string literals to `F()` macro

## Fragile Areas

**Watchdog ISR calls `Serial.println()`:**
- Issue: `ISRWatchDog()` (a Ticker ISR) calls `Serial.println(watchDogCount)` — Serial is not ISR-safe on ESP8266
- Files: `src/main.cpp` `ISRWatchDog()`
- Why fragile: Can cause WDT crashes or corrupted serial output under load
- Fix: Remove `Serial.println()` from ISR; log counter value in `loop()` after reset

**Blocking auth wait in `setup()`:**
- Issue: `while ((auth.token.uid) == "") { delay(1000); }` blocks indefinitely if Firebase auth fails
- Files: `src/main.cpp` `setup()` (~line 478)
- Impact: Software watchdog (80s) will `ESP.restart()` — device may boot-loop if auth always fails
- Fix: Add max retry count; show error on display; enter WiFiManager portal on repeated failure

**`ESP.reset()` without backoff on Firebase failure:**
- Issue: `callFirebase()` resets the device after one `timerDelay` cycle without Firebase ready
- Files: `src/main.cpp` `callFirebase()`
- Impact: Aggressive restart loop if Firebase is temporarily down; wastes cloud quota
- Fix: Exponential backoff with max retry count before reset

**Firebase config fetched on every send cycle:**
- Issue: `getDeviceConfigurations()` is called inside `sendDataToFireBase()` — doubles RTDB operations per cycle
- Files: `src/main.cpp` `sendDataToFireBase()`
- Fix: Separate config-fetch interval (e.g., hourly); cache config locally between fetches

## Missing Critical Features

**No WiFi reconnection after disconnect:**
- Problem: If WiFi drops post-boot, Firebase fails, device resets after `timerDelay`. No reconnect attempt.
- Files: `src/main.cpp` `callFirebase()`
- Fix: Attempt `WiFi.reconnect()` before `ESP.reset()`

**No automated test coverage:**
- Problem: Zero tests exist (see TESTING.md)
- Risk: Any refactor of threshold evaluation, LDID generation, status bitmap has no safety net
- Priority: High

**No user-visible error on boot failure:**
- Problem: If Firebase auth fails or WiFiManager times out, device restarts silently
- Files: `src/main.cpp` `setup()`
- Fix: Show named OLED error screen before `ESP.restart()`

## EEPROM Capacity

**512-byte limit with 4 slots of 100 bytes each:**
- Layout: `EMAIL_ADDR=0`, `PASS_ADDR=100`, `DEVICE_NAME_ADDR=200`, `LDID_ADDR=300` — 112 bytes spare
- Risk: `writeStringToEEPROM` silently truncates at `EEPROM_SIZE - addrOffset - 2` bytes; emails or passwords > 99 chars are truncated without warning
- Files: `src/EEPROMFunction.h` `writeStringToEEPROM()`

## Dependencies at Risk

**Local library copies without version tracking:**
- Risk: `lib/esp8266-oled-ssd1306/` and `lib/WiFiManager/` are manual copies with no version pinning
- Impact: Security patches and bugfixes in upstream won't be applied automatically
- Migration: Remove from `lib/`, pin via `platformio.ini lib_deps` with exact version tags

---

*Concerns audit: 2026-04-11*
