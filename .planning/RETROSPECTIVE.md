# Retrospective

## v1.1 Production Hardening (2026-04-14)

### What We Shipped

4 phases, 8 plans, 8 tasks. Firmware went from "functional but fragile" to production-ready:

- **Phase 1:** ISR-safe watchdog, WiFi pre-reset retry, Firebase auth retry with OLED error screens
- **Phase 2:** Exponential backoff, EEPROM pending queue (16 readings offline), config-fetch timer separation, WiFi reconnecting OLED modal
- **Phase 3:** drawOled.h split into 4 focused headers (584→357 lines), sensorError flag, snprintf migration
- **Phase 4:** PlatformIO native test environment, 19 unit tests — all passing in 2.68s

### What Worked

- **Header-only decomposition** — splitting drawOled.h without breaking main.cpp build proved easier than expected; Arduino single-unit compilation made it straightforward.
- **EEPROM ring structures with magic bytes** — the pattern (magic sentinel + data slots) worked cleanly for both pending queue and WiFi credential ring buffer. Reusable for future EEPROM additions.
- **Unity/PlatformIO native** — native env with Arduino stubs was more viable than expected; zero hardware required for logic tests. Good investment.
- **snprintf migration** — mechanical change with high impact; eliminated all String heap fragmentation in Firebase path-building with minimal code churn.
- **Incremental builds throughout** — checking `pio run` at each plan boundary caught integration issues early and kept RAM/Flash metrics visible.

### What Was Inefficient

- **SUMMARY.md format divergence** — phases 01/02 wrote file paths as first line instead of one-liner accomplishments. `gsd-tools milestone complete` extracted them literally, producing garbage in MILESTONES.md. Required manual cleanup.
- **Heredoc in zsh agent context** — couldn't use inline heredocs; every file write needed a Python script workaround. Added friction but became second nature.
- **Duplicate code from patch scripts** — WiFi ring buffer was applied via a Python patch script that ran twice, creating duplicate struct and function bodies. Required a dedup script to clean up.
- **GSD discuss phase overhead** — for mechanical phases (Phase 3 snprintf, Phase 4 tests) the full discuss→plan workflow felt heavy. Consider `/gsd:quick` or direct planning for clearly-scoped work.

### Key Lessons

- **EEPROM layout is now 1024B** — document the full map before any new EEPROM additions. The layout in PROJECT.md is now canonical.
- **RAM headroom consumed** — went from ~48% to ~53.5% in v1.1. Future features must budget RAM explicitly; avoid new heap allocations.
- **Test stubs are fragile** — EEPROM.h stub needed buffer expansion (512→1024) when EEPROM_SIZE changed. Stubs must track production constants.
- **Magic byte collision risk** — 0xA5 (pending queue) and 0xB3 (WiFi creds) are now reserved. Document sentinel values.

### Patterns Established

- EEPROM address map documented in PROJECT.md (canonical source)
- Ring buffer pattern: `magic byte + count + fixed-size slots` — reuse for future EEPROM structures
- Test stub pattern: `test/stubs/{Library}.h` with minimal Arduino compatibility shim
- One-liner SUMMARY.md accomplishments (first line = human-readable outcome, not file path)

### Metrics

| Metric | Before v1.1 | After v1.1 |
| ------ | ----------- | ---------- |
| RAM usage | ~48% | 53.5% |
| Flash usage | ~67% | 68.7% |
| Unit tests | 0 | 19 (all passing) |
| Max Firebase retries before reset | 1 | 3+ with backoff |
| Offline resilience | None | 16-reading EEPROM queue |
| Known networks stored | 1 (in config.h) | 3 (EEPROM ring buffer) |

### Next Milestone Candidates

- **WiFi credential UX** — show stored networks on OLED, allow deletion from button interface
- **Multi-sensor support** — OneWire bus scan for multiple DS18B20s
- **MQTT as alternative protocol** — Firebase HTTP has latency; MQTT would improve real-time monitoring
- **ESP32 migration (Version B)** — more RAM, dual-core, better BLE provisioning
- **OTA security** — currently unsigned; add basic signature verification
- **Threshold persistence across OTA** — thresholds revert on firmware update

---

_Retrospective written: 2026-04-14_
