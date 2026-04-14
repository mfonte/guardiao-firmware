---
plan: 01-01
phase: 01-isr-safety-boot-resilience
status: complete
completed: 2026-04-11
---

## Summary

ISRWatchDog() tornado ISR-safe e callFirebase() com retry WiFi antes de reset.

## What Was Built

**src/config.h** (gitignored — alterado localmente):

- `int watchDogCount = 0` → `volatile uint8_t watchDogCount = 0`
- Adicionado: `volatile bool wdtFired = false;`

**src/main.cpp** (1 arquivo, 33 inserções / 9 remoções):

- `ISRWatchDog()`: removida `Serial.println(watchDogCount)` (ISR-unsafe), `== 80` → `>= 80`, adicionado `wdtFired = true` antes de `ESP.restart()`
- `loop()`: adicionado `Serial.printf("[WDT] loop reset, count was %u\n", watchDogCount)` antes de `watchDogCount = 0`
- `callFirebase()`: substituído `ESP.reset()` imediato por loop de 3 tentativas `WiFi.reconnect()` com `drawBootProgress("Reconnecting...", 0)` entre tentativas; fallback para `drawResetDevice() + delay(2000) + ESP.reset()` apenas se todas falharem

## Build Result

```
RAM:   [=====     ]  48.4% (used 39624 bytes from 81920 bytes)
Flash: [=======   ]  67.3% (used 702836 bytes from 1044464 bytes)
========================= [SUCCESS] Took 12.79 seconds =========================
```

## Key Files

### key-files.created

- src/main.cpp (modificado)
- src/config.h (modificado — gitignored)

## Decisions

- `wdtFired` reservada para uso futuro (não lida no loop() neste plano)
- `drawBootProgress("Reconnecting...", 0)` com progresso fixo em 0 — intencional, sem progresso real a mostrar
- `Serial.flush(); Serial.end()` mantidos antes de restart na ISR — safe para hard restart

## Self-Check: PASSED
