---
plan: 01-02
phase: 01-isr-safety-boot-resilience
status: complete
completed: 2026-04-11
---

## Summary

drawBootError() criada e aplicada em todos os pontos de falha silenciosa do boot.

## What Was Built

**src/drawOled.h** (inserção de 16 linhas após drawResetDevice()):

- Nova função `drawBootError(const char *reason)` com layout 3 linhas no OLED 128x64
  - Y=4: "Erro de Boot" em ArialMT_Plain_16
  - Y=26: motivo (reason) em ArialMT_Plain_10
  - Y=44: "Reiniciando..." em ArialMT_Plain_10

**src/main.cpp** (107 inserções / 14 remoções):

- Auth loop: `while ((auth.token.uid) == "")` → limitado a 30 tentativas com `authRetryCount`; ao expirar chama `drawBootError("Firebase auth falhou")` + `delay(3000)` + `ESP.restart()`
- `setupWiFiManager` failure: `ESP.restart()` silencioso → `drawBootError("WiFi config falhou")` + `delay(3000)` + `ESP.restart()`
- `startConfigPortal` timeout: `drawResetDevice() + delay(2000)` → `drawBootError("Portal timeout")` + `delay(3000)`

## Build Result

```
RAM:   [=====     ]  48.5% (used 39736 bytes from 81920 bytes)
Flash: [=======   ]  67.3% (used 703188 bytes from 1044464 bytes)
========================= [SUCCESS] Took 12.32 seconds =========================
```

## Key Files

### key-files.created

- src/drawOled.h (modificado — drawBootError adicionado na linha 365)
- src/main.cpp (modificado — 3 pontos de falha corrigidos)

## Decisions

- `delay(3000)` padronizado em todos os boot failures — tempo adequado para leitura no campo
- `reason` é `const char*` não `String` — consistência com drawBootProgress
- Restarts nas linhas 155 (getDeviceConfigurations) e 558 (initWifiManager portal end) não foram modificados — não são boot failures silenciosos

## drawBootError Call Sites in main.cpp

| Line | Call                                    | Trigger                         |
| ---- | --------------------------------------- | ------------------------------- |
| 447  | `drawBootError("WiFi config falhou")`   | setupWiFiManager retorna false  |
| 454  | `drawBootError("Portal timeout")`       | startConfigPortal retorna false |
| 506  | `drawBootError("Firebase auth falhou")` | auth.token.uid vazio após 30s   |

## Self-Check: PASSED
