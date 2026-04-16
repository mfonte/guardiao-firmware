# Architecture

**Analysis Date:** 2026-04-11

## Pattern Overview

**Overall:** Arduino event-loop with header-based modular decomposition

**Key Characteristics:**
- Single `setup()` / `loop()` entry point in `src/main.cpp`
- All modules are `.h` header files included into `main.cpp` (not separate compilation units)
- Global mutable state shared across modules via `src/config.h` globals
- OLEDDisplayUi callback pattern for multi-frame display rendering
- Non-blocking: `millis()` timers instead of `delay()` in the main loop

## Layers

**Application Entry (`src/main.cpp`):**
- Purpose: Orchestrates boot sequence, WiFi, Firebase, NTP, sensor reads, OTA, watchdog
- Contains: `setup()`, `loop()`, Firebase functions, Protocol v2 functions
- Depends on: All headers in `src/`

**Display / UI (`src/drawOled.h`):**
- Purpose: All rendering — 6 OLED frames, status bar overlay, modal screens, alarm, buttons, buzzer
- Contains: `FrameCallback` functions, `overlayStatusBar()`, `handleButtons()`, alarm state machine, buzzer sequences, `initUIFramework()`
- Depends on: `config.h`, `icons.h`, `SSD1306Wire.h`, `OLEDDisplayUi.h`

**Hardware Abstraction (`src/EEPROMFunction.h`):**
- Purpose: Length-prefixed string persistence in 512-byte EEPROM
- Contains: `readStringFromEEPROM(int)`, `writeStringToEEPROM(int, String&)`

**OTA Update (`src/OTAUpdate.h`):**
- Purpose: ArduinoOTA setup with OLED progress rendering
- Contains: `OTAsetup()`

**Configuration (`src/config.h` / `src/config.example.h`):**
- Purpose: EEPROM address map, runtime global variables, hardware pin defines, Firebase credentials
- All variables are defined (not just declared) in this header — included once via `#pragma once`

**Icons (`src/icons.h`):**
- Purpose: 8x8 XBM bitmap constants in PROGMEM for status bar and UI elements
- Contains: `icon_wifi`, `icon_wifi_off`, `icon_sync_ok`, `icon_sync_err`, `icon_bell`, `icon_arrow_up`, `icon_arrow_down`, `icon_arrow_right`, `icon_arrow_left`, `icon_battery`, `icon_battery_low`

## UI Frame Architecture (OLEDDisplayUi)

**6 frames registered in order in `src/drawOled.h`:**
```cpp
FrameCallback uiFrames[] = {
  frameTemperature,  // Frame 0: large temp reading, trend arrow, device name
  frameThreshold,    // Frame 1: position bar between lowerTemp/higherTemp
  frameClock,        // Frame 2: HH:MM:SS (Plain_24) + YYYY-MM-DD (Plain_10), BRT
  frameStatus,       // Frame 3: Firebase status, send count, uptime, free heap
  frameWifi,         // Frame 4: SSID, IP, hostname, RSSI
  frameBattery,      // Frame 5: VCC voltage (Plain_24), charge label
};
```

**Overlay (always rendered above all frames):**
- `overlayStatusBar()` — 12px top bar: WiFi icon, Firebase icon, bell icon + right side: battery icon (frame 0) OR temperature reading (frames 1-5)

**Navigation:**
- Manual only — `ui.disableAutoTransition()`; `ui.setIndicatorDirection(LEFT_RIGHT)`
- S1 short press: previous frame; S2 short press: next frame
- S1 hold 5s: open WiFiManager config portal; S2 hold 10s: device reset
- Left/right nav arrows drawn at `x=2`/`x=118`, `y=55` on all 6 frames

## Data Flow

**Sensor to Cloud (every `timerDelay` ms, default 300000ms):**
1. `loop()` fires `callFirebase()` every `GET_DATA_INTERVAL` ms (5000ms)
2. `callFirebase()` calls `readTemperature()` via `DallasTemperature`
3. If Firebase ready and `timerDelay` elapsed: `sendDataToFireBase()`
4. `sendDataToFireBase()` fetches config first, then writes temp + datalogger entry
5. `checkThresholdAlert()` evaluates limits, calls `startAlarm()` if violated

**Config Pull (each send cycle):**
1. `getDeviceConfigurations()` fetches `/config` JSON
2. Deserialises response → updates `higherTemp`, `lowerTemp`, `thresholdMode`, `timerDelay`

**Boot Sequence:**
1. Serial init → LED blink → `initDisplay()` → EEPROM read credentials
2. WiFiManager auto-connect (or captive portal if credentials empty)
3. NTP begin → Watchdog Ticker → sensor init → Firebase auth
4. `generateOrLoadLDID()` → `firebaseInit()` → `getDeviceConfigurations()` → `sendBootMessage()`
5. `OTAsetup()` → `initUIFramework()` → `beepBoot()`

## Error Handling

**Strategy:** Sentinel values + Serial logging + `ESP.restart()` for unrecoverable states

**Patterns:**
- Sensor error: `tempC == -127.0 || tempC == 85.0` → return `888.0` sentinel; guard before any use
- Firebase not ready after interval → `drawResetDevice()` + `delay(4000)` + `ESP.reset()`
- Invalid timestamp (`< MIN_VALID_TIMESTAMP`) → skip RTDB write, log to Serial
- NTP not synced: `timestamp = 0` → `frameClock` shows "Syncing..."
- Watchdog: `Ticker.attach(1, ISRWatchDog)` → 80 ticks without reset → `ESP.restart()`

## Protocol v2

**LDID (Logical Device ID):** `"ldid-{chipId}-001"` stored in EEPROM at `LDID_ADDR=300`; used as RTDB node key to survive device renames.

**Boot message:** Written on each restart with UUID, firmware version, reset reason, free heap, RSSI.

**Heartbeat:** Sent every `HEARTBEAT_INTERVAL` (300000ms) with 8-bit status bitmap.

**Status bitmap (`calculateStatusBitmap()`):**
- bit 0 = online, bit 2 = sensor error, bit 4 = threshold alert active

---

*Architecture analysis: 2026-04-11*
