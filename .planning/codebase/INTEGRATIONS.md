# External Integrations

**Analysis Date:** 2026-04-11

## Firebase Realtime Database

**Purpose:** Primary cloud backend — stores device data, config, datalogger
- SDK: `mobizt/Firebase Arduino Client Library for ESP8266 and ESP32 ^4.4.14`
- Auth: email + password (`auth.user.email` / `auth.user.password` from EEPROM)
- API key: `API_KEY` define in `src/config.h` (see Security in CONCERNS.md)
- Database URL: `DATABASE_URL` define in `src/config.h`
- RTDB path: `/UsersData/{uid}/devices/{LDID}/`

**Operations performed:**
- `Firebase.RTDB.updateNode()` — write temperature reading, device status, device name
- `Firebase.RTDB.getJSON()` — fetch device config (thresholds, scheduled intervals)
- Protocol v2 `sendBootMessage()` / `sendHeartbeat()` — write to dedicated boot/heartbeat nodes

**Config structure fetched (`/config`):**
```json
{
  "_threshold": { "higherTemp": float, "lowerTemp": float, "thresholdMode": "both|upperOnly|lowerOnly|none" },
  "scheduledReadings": { "intervalMinutes": int, "startHour": int }
}
```

**Datalogger path:** `/UsersData/{uid}/devices/{LDID}/datalogger/{timestamp}/`

## NTP Time Service

**Purpose:** Epoch timestamp for reading records + clock display frame
- Client: `arduino-libraries/NTPClient ^3.2.1`
- Server: `pool.ntp.org`
- Timezone offset: `TIMEZONE_OFFSET_SEC -10800L` (BRT = UTC-3, applied in `frameClock()`)
- Guard: `MIN_VALID_TIMESTAMP 1704067200L` — timestamps below this are rejected as invalid
- Usage: `timeClient.update()` + `timeClient.getEpochTime()` in `getTime()`

## WiFiManager (Captive Portal)

**Purpose:** Zero-config WiFi + credential provisioning on first boot
- Library: `lib/WiFiManager/` (local copy)
- AP SSID: `"Guardiao-" + String(ESP.getChipId() & 0xFFFF, HEX)` — unique 4-digit hex suffix per device
- Portal timeout: 30s (`autoConnect`) or 120s (`startConfigPortal` on first boot)
- Custom parameters: `Email`, `Password`, `DeviceName` (alphanumeric IDs, no spaces)
- Credentials saved to EEPROM via `setSaveConfigCallback`

## ArduinoOTA

**Purpose:** Over-the-air firmware update during development / deployment
- Hostname: `"guardiao-" + DEVICE_UUID` (set in `OTAsetup()` via `src/OTAUpdate.h`)
- Callbacks: draw OTA progress bar on OLED, reset watchdog count
- Upload port (commented out in `platformio.ini`): `upload_protocol = espota`

## ESP.getVcc() — ADC Battery Monitor

**Purpose:** Supply voltage reading shown in battery frame and status bar icon
- Mode: `ADC_MODE(ADC_VCC)` set globally (precludes analogRead on A0)
- Throttled: reads at most once per 1000ms (`lastVccMs`) to prevent flicker
- Thresholds: `< 3.0V` low battery icon/warning, `> 4.5V` USB powered label

## Monitoring & Observability

**Error Tracking:** None — errors printed to Serial at 115200 baud
**Logs:** `Serial.println("Entering/Leaving functionName")` pattern in every function

## CI/CD & Deployment

**Hosting:** None — direct firmware upload via USB or OTA
**CI Pipeline:** None

## Required Configuration (src/config.h)

```cpp
#define API_KEY "..."           // Firebase project API key
#define DATABASE_URL "..."      // Firebase RTDB URL
#define TIMEZONE_OFFSET_SEC -10800L
#define MIN_VALID_TIMESTAMP 1704067200L
// Plus: pin assignments, EEPROM addresses, timing constants
```

---

*Integration audit: 2026-04-11*
