# Testing Patterns

**Analysis Date:** 2026-04-11

## Test Framework

**Runner:** None configured
- `test/` directory exists with only PlatformIO placeholder `README`
- `platformio.ini` has no `test_framework` or `test_filter` entries
- No unit or integration test files exist anywhere in the repository

**Run Commands:**
```bash
pio run                        # Build only — verify compilation and zero warnings
pio run --target upload        # Flash to hardware
pio device monitor --baud 115200  # Serial output inspection
```

## Current Testing Approach

All testing is **hardware-in-the-loop (HIL)** on NodeMCU v2:
1. Modify code in `src/`
2. `pio run` — verify zero compiler warnings/errors
3. Flash: `pio run --target upload`
4. Observe OLED display output and `Serial` monitor at 115200 baud
5. Report visual/behavioral issues and iterate

## What Lacks Test Coverage

Zero automated tests. High-risk untested areas:

- `readTemperature()` — sentinel mapping (`-127.0` and `85.0` → `888.0`)
- `checkThresholdAlert()` — all four `thresholdMode` values (`both`, `upperOnly`, `lowerOnly`, `none`)
- `readStringFromEEPROM()` / `writeStringToEEPROM()` — length-prefix encoding, truncation
- `generateOrLoadLDID()` — idempotency (same LDID on repeated calls)
- `calculateStatusBitmap()` — bit field correctness for all combinations
- Frame centering math in `frameTemperature()` — `getStringWidth()` groupX calculation
- `updateAlarm()` — all state machine transitions (`ALARM_OFF` ↔ `ALARM_SOUNDING`)
- `handleButtons()` — hold-timer logic and threshold crossings

## If Adding Tests

PlatformIO supports native (host-machine) unit tests. Pure C++ logic that doesn't touch hardware can run on host.

**Add to `platformio.ini`:**
```ini
[env:native]
platform = native
test_framework = unity
```

**Example testable targets (no hardware needed):**
- `calculateStatusBitmap()` — pure bit logic
- `checkThresholdAlert()` threshold evaluation — mock `startAlarm()` and `temperature` global
- EEPROM string encode/decode — mock EEPROM with array

**Recommended framework:** Unity (built into PlatformIO native test runner)

**Test file placement:**
- `test/test_main.cpp` for native tests
- One `TEST_ASSERT_*` per behavior, grouped in `RUN_TEST()` blocks

---

*Testing analysis: 2026-04-11*
