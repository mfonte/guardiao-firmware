# Changelog — guardiao-firmware

All notable changes to this project will be documented here.
Format based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versioning follows [Semantic Versioning](https://semver.org/).

---

## [1.2.1] - 2026-04-16

### Added
- Distinct send trigger logs: `INTERVAL`, `SCHEDULED`, `THRESHOLD`, `RECOVERY`
- Hold-state logs split into `INTERVAL-HOLD` and `SCHEDULED-HOLD`
- Pending EEPROM queue source tags (`interval`, `scheduled`, `threshold`, `recovery`)
- Priority retention for scheduled readings when queue is full

### Fixed
- Scheduled readings now use local timezone (`TIMEZONE_OFFSET_SEC`) for slot alignment
- `scheduledReadings.intervalMinutes/startHour` no longer throttle regular interval sends
- Schedule and interval flows now operate independently with clear telemetry traces

### Changed
- Firmware emits trigger-aware send labels (`[SEND:<origin>]`) to simplify app-side reconciliation

---

## [1.2.0] - 2026-04-16

### Fixed
- RTDB temperature history wiped on restart — datalogger now written to
  its own timestamped path (`/datalogger/{ts}`) instead of nested inside
  the device root PATCH, preventing Firebase from replacing the entire
  `/datalogger` node on each send (fixes #10)
- Alert recovery: temperature returning to safe range after a HIGH/LOW
  alert now triggers an immediate Firebase send instead of waiting the
  full 5-minute interval
- `getDeviceConfigurations()` no longer calls `ESP.restart()` when
  Firebase is transiently not ready — retries silently on next cycle

### Added
- WiFi credential ring buffer: stores last 3 networks in EEPROM
  (addr 510), auto-reconnects on boot, promotes successful network to
  slot 0 (SSID max 32 chars, password max 64 chars)
- Clock frame interpolates seconds via `millis()` delta from last NTP
  sync — clock ticks every second without blocking Firebase calls
- LED feedback: 1 blink on successful send, 3 blinks on alarm

### Changed
- `sendIntervalMinutes` deprecated — `scheduledReadings.intervalMinutes`
  is now the single source of truth for the send interval
- Structured log format: `[BOOT]`, `[SEND]`, `[ALERT]`, `[HB]`, `[CFG]`
  with real BRT timestamp when NTP is synced, uptime fallback otherwise

---

## [1.1.0] - 2026-04-14 (Production Hardening)

### Fixed
- ISR watchdog (`ISRWatchDog`) no longer calls `Serial` — only sets flag
- WiFi reconnection attempted before `ESP.reset()` in `callFirebase()`
- Firebase auth retry with max attempts and OLED error screen on failure

### Added
- Exponential backoff on Firebase failure (min 3 retries before reset)
- EEPROM pending queue: stores up to 16 readings offline, drained on
  next successful Firebase connection
- Separate config-fetch interval (60 min) from data-send interval
- `drawWifiReconnecting()` modal on WiFi disconnection in `loop()`
- `drawBootError()` shown on all critical boot failures
- Module decomposition: `drawOled.h` split into `alarm.h`, `buttons.h`,
  `buzzer.h`, `drawOled.h` (rendering only)
- `bool sensorError` flag replaces magic float sentinels (-127, 85)
- `snprintf` + `char[]` replaces `String` in all Firebase hot-paths
- PlatformIO native test environment with Unity — 19 unit tests

---

