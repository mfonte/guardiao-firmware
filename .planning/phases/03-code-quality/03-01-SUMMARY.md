---
phase: 03-code-quality
plan: 01
subsystem: firmware-architecture
tags: [esp8266, arduino, header-decomposition, single-tu]

requires:
  - phase: 02-firebase-resilience-observability
    provides: stable firmware codebase with sensorError flag

provides:
  - buzzer.h — isolated buzzer feedback functions
  - alarm.h — non-blocking alarm state machine
  - buttons.h — button handling with hold-progress UI
  - drawOled.h reduced to pure rendering (~357 lines)

affects: [03-02, testing]

tech-stack:
  added: []
  patterns:
    [
      "Single-TU include ordering — config → icons → drawOled → buzzer → alarm → buttons",
    ]

key-files:
  created: [src/buzzer.h, src/alarm.h, src/buttons.h]
  modified: [src/drawOled.h, src/main.cpp]

key-decisions:
  - "Keep DisplayMode enum and displayMode in drawOled.h — alarm.h reads via single-TU visibility"
  - "New headers include their own deps (#pragma once + Arduino.h + config.h) for IDE support"
  - "drawHoldProgress moved to buttons.h (only caller is handleButtons)"

patterns-established:
  - "Module extraction pattern: extract to focused .h, maintain include order in main.cpp"

requirements-completed: [CQ-01]

duration: ~15min
completed: 2026-04-11
---

# Plan 03-01: Module Decomposition Summary

**Extracted buzzer, alarm, and button handling from monolithic drawOled.h into 3 focused headers, reducing drawOled.h from 584 to 357 lines of pure rendering code.**

## Performance

- **Tasks:** 2 (extraction + validation)
- **Files modified:** 5

## Accomplishments

- Created `buzzer.h` (40 lines) — beepConfirm, beepSuccess, beepError, beepBoot
- Created `alarm.h` (81 lines) — drawAlarmScreen, startAlarm, updateAlarm + alarm state
- Created `buttons.h` (129 lines) — handleButtons, drawHoldProgress + button state
- Reduced `drawOled.h` to 357 lines — only frame callbacks, overlays, modal screens, init functions
- Build stable: RAM 49.6%, Flash 67.6%

## Task Commits

1. **Task 1+2: Extract modules and validate** — `0faad9d` (refactor)

## Files Created/Modified

- `src/buzzer.h` — Beep functions (confirm, success, error, boot)
- `src/alarm.h` — Non-blocking alarm state machine with screen drawing
- `src/buttons.h` — S1/S2 handling with hold-progress and portal/reset actions
- `src/drawOled.h` — Reduced to rendering only (frames, overlays, modals, init)
- `src/main.cpp` — Added includes for buzzer.h, alarm.h, buttons.h

## Deviations from Plan

None — plan executed as specified.
