# Milestones

## v1.1 Production Hardening (Shipped: 2026-04-14)

**Phases completed:** 4 phases, 8 plans, 8 tasks

**Key accomplishments:**

- ISRWatchDog() tornado ISR-safe e callFirebase() com retry WiFi antes de reset.
- drawBootError() criada e aplicada em todos os pontos de falha silenciosa do boot.
- EEPROM pending queue + backoff exponencial Firebase + config fetch timer separado (60min).
- drawWifiReconnecting() modal + WiFi-conditional routing no loop() via displayMode.
- Extracted buzzer, alarm, and button handling from monolithic drawOled.h into 3 focused headers, reducing drawOled.h from 584 to 357 lines of pure rendering code.
- Eliminated Arduino String heap fragmentation in all Firebase path construction, replacing with stack-allocated char[] + snprintf across 6 call sites.
- PlatformIO native test environment with Unity, Arduino/EEPROM stubs, pure logic extraction to logic.h, and 2 passing smoke tests — production build unbroken.
- 19 unit tests covering all 4 ROADMAP success criteria: bitmap states, threshold modes, EEPROM string round-trip, and pending queue round-trip — all passing in 2.68s.

---
