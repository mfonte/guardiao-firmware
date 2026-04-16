---
phase: 4
slug: testing-infrastructure
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-04-11
---

# Phase 4 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property               | Value                                              |
| ---------------------- | -------------------------------------------------- |
| **Framework**          | Unity 2.6.1 (via PlatformIO built-in)              |
| **Config file**        | `platformio.ini` → `[env:native]` (Wave 0 creates) |
| **Quick run command**  | `pio test -e native`                               |
| **Full suite command** | `pio test -e native -v`                            |
| **Estimated runtime**  | ~3 seconds                                         |

---

## Sampling Rate

- **After every task commit:** Run `pio test -e native`
- **After every plan wave:** Run `pio test -e native -v`
- **Before `/gsd-verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID  | Plan | Wave | Requirement | Test Type | Automated Command                   | File Exists | Status     |
| -------- | ---- | ---- | ----------- | --------- | ----------------------------------- | ----------- | ---------- |
| 04-01-01 | 01   | 1    | TEST-01     | smoke     | `pio test -e native`                | ❌ W0       | ⬜ pending |
| 04-02-01 | 02   | 1    | TEST-02     | unit      | `pio test -e native -f test_native` | ❌ W0       | ⬜ pending |
| 04-02-02 | 02   | 1    | TEST-02     | unit      | `pio test -e native -f test_native` | ❌ W0       | ⬜ pending |
| 04-02-03 | 02   | 1    | TEST-02     | unit      | `pio test -e native -f test_native` | ❌ W0       | ⬜ pending |

_Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky_

---

## Wave 0 Requirements

- [ ] `[env:native]` section in `platformio.ini` — platform=native, test_framework=unity, build_src_filter=-<\*>
- [ ] `test/stubs/Arduino.h` — minimal String class + Serial stub
- [ ] `test/stubs/EEPROM.h` — RAM-backed EEPROMClass
- [ ] `src/logic.h` — extracted pure functions from main.cpp
- [ ] `test/test_native/test_logic.cpp` — initial test file with Unity setup

---

## Manual-Only Verifications

_All phase behaviors have automated verification._

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
