---
phase: 03-code-quality
plan: 02
subsystem: firmware-performance
tags: [esp8266, snprintf, heap-fragmentation, firebase]

requires:
  - phase: 03-code-quality/01
    provides: clean module separation for focused edits

provides:
  - Hot-path Firebase paths use stack-allocated char[] + snprintf
  - Warm-path (boot, heartbeat) paths also converted
  - Reduced heap fragmentation risk on ESP8266

affects: [testing]

tech-stack:
  added: []
  patterns:
    [
      "snprintf + char[] for fixed-length paths — 48 bytes for short, 128 bytes for long",
    ]

key-files:
  created: []
  modified: [src/main.cpp, src/EEPROMFunction.h]

key-decisions:
  - "databasePath kept as String — setup-only, extensively used with .c_str(), low impact"
  - "char[48] for /datalogger/ paths, char[128] for /UsersData/ paths — sizeof() always used"

patterns-established:
  - "Firebase path construction: snprintf(buf, sizeof(buf), format, ...) instead of String +"

requirements-completed: [CQ-03]

duration: ~10min
completed: 2026-04-11
---

# Plan 03-02: snprintf for Hot-Path String Building Summary

**Eliminated Arduino String heap fragmentation in all Firebase path construction, replacing with stack-allocated char[] + snprintf across 6 call sites.**

## Performance

- **Tasks:** 2 (conversion + validation)
- **Files modified:** 2

## Accomplishments

- Converted `drainOnePendingReading()` — `String drainPath` → `char[48]` + 2x snprintf
- Converted `sendBootMessage()` — `String bootPath` → `char[128]` + snprintf
- Converted `sendHeartbeat()` — `String heartbeatPath` → `char[128]` + snprintf
- `sendDataToFireBase()` already used snprintf (prior work)
- Total: 6 snprintf calls, 0 String concatenation in Firebase paths
- Build stable: RAM 49.6%, Flash 67.6% (Flash 8 bytes less)

## Task Commits

1. **Task 1+2: snprintf conversion and validation** — `b3536df` (perf)

## Files Created/Modified

- `src/main.cpp` — bootPath and heartbeatPath converted to char[] + snprintf
- `src/EEPROMFunction.h` — drainPath converted to drainTempPath/drainTsPath char[] + snprintf

## Decisions Made

- `databasePath` kept as `String` — runs once in `setup()`, used in 5+ places with `.c_str()`. Converting would touch too many call sites for minimal gain (TODO comment in plan acknowledged).

## Deviations from Plan

- Plan suggested converting `generateOrLoadLDID()` — DEVICE_LDID is a `String` global used extensively. Kept as-is since LDID generation runs once at boot.
