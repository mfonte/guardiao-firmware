# Roadmap: Guardião Firmware

## Milestones

- ✅ **v1.1 Production Hardening** — SHIPPED 2026-04-14 — [archive](.planning/milestones/v1.1-ROADMAP.md)
- 🔲 **v1.2** — TBD

<details>
<summary>✅ v1.1 Production Hardening (Phases 1–4) — SHIPPED 2026-04-14</summary>

## Overview

Milestone de endurecimento do firmware ESP8266 existente. Ponto de partida: firmware funcional (Protocol v2 completo, OLED UI, Firebase RTDB, OTA). Objetivo: tornar o firmware apto para implantação permanente em campo — sem travamentos por ISR inseguro, sem boot-loops por Firebase instável, sem degradação de heap após dias em operação. Quatro fases em sequência: segurança no loop crítico → resiliência de rede → qualidade de código → cobertura de testes.

## Phases

- [x] **Phase 1: ISR Safety + Boot Resilience** — Watchdog ISR-safe, reconexão WiFi, auth retry com OLED, telas de erro no boot
- [x] **Phase 2: Firebase Resilience + Observability** — Backoff exponencial, intervalo separado para config, status WiFi no OLED
- [x] **Phase 3: Code Quality** — Split drawOled.h, flag sensorError, snprintf em hot-paths
- [x] **Phase 4: Testing Infrastructure** — Ambiente Unity/PlatformIO, 19 testes unitários para lógica pura

## Progress

| Phase                                  | Requirements                   | Plans Complete | Status   | Completed |
| -------------------------------------- | ------------------------------ | -------------- | -------- | --------- |
| 1. ISR Safety + Boot Resilience        | REL-01, REL-02, REL-03, OBS-01 | 2/2            | Complete | ✅        |
| 2. Firebase Resilience + Observability | REL-04, REL-05, OBS-02         | 2/2            | Complete | ✅        |
| 3. Code Quality                        | CQ-01, CQ-02, CQ-03            | 2/2            | Complete | ✅        |
| 4. Testing Infrastructure              | TEST-01, TEST-02               | 2/2            | Complete | ✅        |

</details>

---

_Roadmap criado: 2026-04-11 — Atualizado: 2026-04-14 (v1.1 arquivado)_
