# Phase 1: ISR Safety + Boot Resilience - Context

**Gathered:** 2026-04-11
**Status:** Ready for planning
**Mode:** Autonomous (user delegated all decisions)

<domain>
## Phase Boundary

Eliminar as três causas de crash mais graves do firmware atual:

1. `ISRWatchDog()` chama `Serial.println()` — não é ISR-safe no ESP8266
2. Quando WiFi cai após boot, o código vai direto para `ESP.reset()` sem tentar reconectar
3. O loop `while (auth.token.uid == "")` em `setup()` bloqueia indefinidamente se Firebase auth falha — resulta em boot-loop silencioso sem feedback ao usuário

Esta fase NÃO inclui: backoff exponencial de envio Firebase (Fase 2), split de drawOled.h (Fase 3), testes unitários (Fase 4).

</domain>

<decisions>
## Implementation Decisions

### ISR Watchdog (REL-01)

- **D-01:** Remover `Serial.println(watchDogCount)` da função `ISRWatchDog()` completamente
- **D-02:** Adicionar `volatile bool wdtFired = false;` em config.h ou topo de main.cpp como sinal para o loop
- **D-03:** ISR apenas faz: `watchDogCount++; if (watchDogCount >= 80) { wdtFired = true; ESP.restart(); }`
- **D-04:** No `loop()`, após reset do watchdog (`watchDogCount = 0`), logar o valor anterior via `Serial.println()` — fora da ISR
- **D-05:** Manter `Serial.flush(); Serial.end();` antes de `ESP.restart()` dentro da ISR (essas são safe — não fazem heap allocation)

### WiFi Reconnect (REL-02)

- **D-06:** Na função `callFirebase()`, quando `!Firebase.ready()`, tentar `WiFi.reconnect()` antes de `ESP.reset()`
- **D-07:** Máximo 3 tentativas de reconexão, delay de 2s entre cada (`delay(2000)`) — total máximo: 6s extra antes do reset
- **D-08:** Verificar `WiFi.status() == WL_CONNECTED` após cada tentativa para sair cedo do loop se reconectou
- **D-09:** Exibir `drawBootProgress("Reconnecting...", 0)` durante as tentativas (reutilizar função existente — progresso fixo em 0)
- **D-10:** Se após 3 tentativas ainda não conectado: exibir `drawResetDevice()` + `delay(2000)` + `ESP.reset()` (comportamento atual preservado para fallback)

### Firebase Auth Retry (REL-03)

- **D-11:** Substituir `while ((auth.token.uid) == "") { delay(1000); }` por loop com contador máximo
- **D-12:** Máximo de 30 tentativas (≈ 30 segundos a 1s/tentativa) — abaixo do watchdog trigger de 80s
- **D-13:** Ao atingir o limite: chamar `drawBootError("Firebase auth falhou")` + `delay(3000)` + `ESP.restart()`
- **D-14:** Manter `Serial.print('.')` dentro do loop de retry (não é ISR — é safe em `setup()`)

### OLED Boot Error Screens (OBS-01)

- **D-15:** Criar uma única função genérica `drawBootError(const char* reason)` em `drawOled.h`
- **D-16:** Layout: título "Erro de Boot" em ArialMT_Plain_16 no topo, `reason` em ArialMT_Plain_10 no centro, "Reiniciando..." no rodapé — mesma estrutura visual de `drawResetDevice()`
- **D-17:** Usar em todos os pontos de boot failure: Firebase auth expirada, WiFiManager timeout, setupWiFiManager falhou
- **D-18:** Substituir `ESP.restart()` silenciosos em `setup()` (linhas ~426, ~433) por `drawBootError("...")` + `delay(3000)` + `ESP.restart()`

### the agent's Discretion

- Ordem exata dos parâmetros e strings de `drawBootError()` — seguir convenção de `drawBootProgress(label, progress)`
- Se usar 2 ou 3 linhas de texto no OLED para o layout do erro
- Valor exato do delay após exibir boot error antes de reiniciar (D-13, D-18 sugerem 3000ms — pode ajustar para 2000ms se preferir mais rápido)

</decisions>

<specifics>
## Specific Ideas

- O padrão `drawBootProgress(const char* label, uint8_t progress)` em `drawOled.h` linha 327 é o modelo para `drawBootError()` — mesma assinatura simples com `const char*`
- Watchdog timeout atual é 80 contagens de 1s = 80s. Firebase auth retry com 30 tentativas de 1s = 30s — deixa 50s de margem para o watchdog não interferir durante auth
- `Serial.flush(); Serial.end();` já existem no ISR antes de `ESP.restart()` — manter exatamente assim, é o padrão correto para ESP8266

</specifics>

<canonical_refs>

## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Código a modificar

- `src/main.cpp` — `ISRWatchDog()` (linha ~277), `callFirebase()` (linha ~345), `setup()` (linhas ~474-480 auth loop, ~426 e ~433 restarts)
- `src/drawOled.h` — `drawBootProgress()` (linha 327), `drawResetDevice()` (linha 351) — modelos para `drawBootError()`
- `src/config.h` — variáveis globais onde `wdtFired` pode ser adicionada

### Planning docs

- `.planning/REQUIREMENTS.md` — REL-01, REL-02, REL-03, OBS-01
- `.planning/codebase/CONCERNS.md` — seções "Watchdog ISR", "Blocking auth wait in setup()", "No WiFi reconnection after disconnect"

No external specs — requirements fully captured in decisions above.

</canonical_refs>

<code_context>

## Existing Code Insights

### Reusable Assets

- `drawBootProgress(const char* label, uint8_t progress)` — base para `drawBootError()`, mesma API
- `drawResetDevice()` — layout visual de referência para tela de erro (fonte 16, centralizado)
- `drawResetDevice()` já é chamada em `callFirebase()` antes de reset — reutilizar padrão

### Padrões Existentes

- `volatile uint8_t watchDogCount` — já é volatile, correto para ISR
- `wdtReset.attach(1, ISRWatchDog)` — Ticker ISR que chama a cada 1 segundo
- `watchDogCount = 0` em `loop()` na linha 555 — local correto para reset + log do contador
- WiFiManager com `setConfigPortalTimeout(120)` e `setConnectTimeout(60)` — padrões já definidos

### Armadilhas Conhecidas

- `Serial.println()` dentro de ISR no ESP8266 usa heap e UART de forma não-reentrante — pode corromper saída serial ou causar WDT reset em si mesmo
- `delay()` dentro do loop de retry em `setup()` é seguro (interrompe apenas tasks do sistema, não o watchdog de hardware do ESP — o software watchdog está desativado durante setup via `wdtReset.attach`) — mas verificar se `wdtReset.attach` ocorre antes ou depois do loop de auth
- `WiFi.reconnect()` no ESP8266 retorna imediatamente — é preciso aguardar `WiFi.status()` chegar a `WL_CONNECTED`

</code_context>

---

_Phase: 01-isr-safety-boot-resilience_
_Context gathered: 2026-04-11 via autonomous mode_
