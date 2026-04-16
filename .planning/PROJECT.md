# Guardião Firmware

## What This Is

Embedded firmware for the Guardião IoT temperature monitoring platform. Runs on ESP8266 NodeMCU v2, reads DS18B20 temperature sensors, displays data on a 128×64 OLED with 6-screen UI, and pushes readings to Firebase Realtime Database. Includes threshold-based alarms, WiFi provisioning via captive portal, OTA updates, and Protocol v2 with persistent device identity (LDID), boot messages, heartbeats, and crash-resilient networking (exponential backoff, EEPROM pending queue, ISR-safe watchdog).

## Core Value

The device must reliably read temperature and push it to Firebase without crashing, even under adverse network conditions. A monitoring device that needs monitoring is worthless.

## Requirements

### Validated

- ✓ DS18B20 temperature reading with error sentinel detection — existing
- ✓ Firebase RTDB push with temperature + timestamp + datalogger — existing
- ✓ 6-screen OLED UI (Temperature, Threshold, Clock, Status, WiFi, Battery) — existing
- ✓ Status bar overlay with WiFi/Firebase/bell icons and context-sensitive right side — existing
- ✓ WiFiManager captive portal for zero-config provisioning — existing
- ✓ Threshold alarm with configurable upper/lower/both/none modes — existing
- ✓ EEPROM persistence for credentials and device name — existing
- ✓ ArduinoOTA for network firmware updates — existing
- ✓ Protocol v2: LDID-based device identity, boot messages, periodic heartbeats — existing
- ✓ Button navigation (S1=prev/portal-hold, S2=next/reset-hold) — existing
- ✓ Non-blocking main loop with millis() timers — existing
- ✓ Software watchdog via Ticker ISR — existing
- ✓ Remote config pull from Firebase (thresholds, scheduled intervals) — existing
- ✓ ISR-safe watchdog (remove Serial from ISR) — v1.1 Phase 1
- ✓ WiFi reconnection before hard reset — v1.1 Phase 1
- ✓ Firebase auth retry with max attempts and error display — v1.1 Phase 1
- ✓ User-visible OLED error screens on boot failures — v1.1 Phase 1
- ✓ Exponential backoff on Firebase failure; EEPROM pending queue for offline resilience — v1.1 Phase 2
- ✓ Separate config-fetch interval from data-send interval (60 min vs send interval) — v1.1 Phase 2
- ✓ WiFi disconnection shown on OLED with reconnecting modal — v1.1 Phase 2
- ✓ Module decomposition: drawOled.h split into rendering, buttons, alarm, buzzer — v1.1 Phase 3
- ✓ Replace magic float sentinels with bool sensorError flag — v1.1 Phase 3
- ✓ Reduce Arduino String fragmentation (snprintf + char[] for hot-paths) — v1.1 Phase 3
- ✓ Native unit tests for pure logic (Unity/PlatformIO native, 19 tests) — v1.1 Phase 4

### Active

_(None — all v1.1 requirements validated)_

### Out of Scope

- ESP32 migration (Version B) — future hardware, different framework concerns
- LoRa support (Version C) — requires new PCB and radio stack
- MQTT protocol — Phase 1 uses HTTP/Firebase REST; MQTT is future consideration
- Multi-sensor support — current hardware has single DS18B20
- Firebase API key rotation — requires Firebase Console action by user, not firmware change
- Git history pruning — operational task, not firmware development

## Context

- **Hardware:** ESP8266 NodeMCU v2, 80 MHz, ~80 KB RAM, ~1 MB Flash. Memory is tight.
- **Build:** PlatformIO with Arduino framework. v1.1 final build: RAM 53.5%, Flash 68.7%.
- **Branch:** `main` — v1.1 milestone fully merged and tagged.
- **Local libs:** `lib/esp8266-oled-ssd1306/` and `lib/WiFiManager/` are vendored copies without version tracking.
- **Config:** `src/config.h` is gitignored; `src/config.example.h` is the committed template.
- **Known issue:** Firebase API key `AIzaSyB...` exists in git history — user must rotate in Firebase Console.
- **Codebase map:** `.planning/codebase/` has 7 documents from 2026-04-11 analysis (pre-v1.1 decomposition).
- **WiFi ring buffer:** EEPROM addr 510, stores last 3 networks (magic 0xB3), auto-promotes on success. Added post-v1.1.
- **EEPROM layout:** 0=EMAIL(100B), 100=PASS(100B), 200=DEVICE_NAME(100B), 300=LDID(80B), 380=PENDING_QUEUE(130B), 510=WIFI_CREDS(296B). Total: 1024B.

## Constraints

- **RAM:** ~80 KB total, ~53% used post-v1.1 — every String allocation matters; prefer stack-allocated char arrays
- **Flash:** ~1 MB, 68% used — room for growth but not unlimited
- **ISR safety:** ESP8266 ISR context cannot call Serial, WiFi, or heap-allocating functions
- **Single thread:** Arduino event loop — all operations must be non-blocking in `loop()`
- **Backward compatible:** Changes must not break existing Firebase RTDB structure or WiFiManager provisioning flow
- **Zero downtime config:** Devices may be deployed in the field; OTA must keep working

## Key Decisions

| Decision                           | Rationale                                                               | Outcome                                         |
| ---------------------------------- | ----------------------------------------------------------------------- | ----------------------------------------------- |
| Header-only modules (.h)           | Arduino framework convention; PlatformIO compiles `src/` as single unit | ✓ Working — drawOled.h split into 4 focused headers in v1.1 |
| Global variables in config.h       | Simple shared state for single-unit compilation                         | ⚠️ Revisit — fragile, violates C++ convention   |
| snprintf + char[] for hot-paths    | Eliminate Arduino String heap fragmentation                             | ✓ Good — 6 call sites migrated in v1.1          |
| ADC_MODE(ADC_VCC) for battery      | Measures supply voltage directly                                        | ✓ Good — works on NodeMCU USB and battery       |
| Protocol v2 LDID in EEPROM         | Device identity survives renames                                        | ✓ Good — stable RTDB node key                   |
| OLEDDisplayUi for frame management | Multi-screen with transitions and indicators                            | ✓ Good — proven pattern                         |
| UTF-8 degree symbol (\xC2\xB0)     | Single-byte \xB0 caused rendering issues                                | ✓ Good — correct UTF-8                          |
| EEPROM pending queue (addr 380)    | Survive Firebase outages without losing readings                        | ✓ Good — 16-entry ring buffer, magic 0xA5       |
| WiFi credential ring buffer (addr 510) | Auto-reconnect to known networks on boot                           | ✓ Good — 3 slots (98B each), promoted on success |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd-transition`):

1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd-complete-milestone`):

1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---

_Last updated: 2026-04-14 after v1.1 Production Hardening milestone completion_
