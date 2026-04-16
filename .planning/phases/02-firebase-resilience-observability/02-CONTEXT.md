# Phase 2: Firebase Resilience + Observability — Context

**Gathered:** 2026-04-11
**Status:** Ready for planning

<domain>
## Phase Boundary

Parar boot-loops causados por falhas temporárias do Firebase (backoff em vez de reset imediato), preservar leituras perdidas durante o backoff em fila persistente na EEPROM (sobrevive a reboot), separar o fetch de configurações do cycle de envio de temperatura, e tornar quedas de WiFi visíveis no display durante operação normal.

Não inclui: nova UI de configuração, changes na estrutura do RTDB, mudanças na lógica de alarme.

</domain>

<decisions>
## Implementation Decisions

### REL-04 — Backoff exponencial em falhas Firebase

- **D-01:** Usar dois contadores globais em config.h:
  - `uint8_t firebaseFailCount = 0;` — falhas consecutivas de write no RTDB
  - `uint8_t firebaseSkipRemaining = 0;` — ciclos de send a pular antes de tentar novamente
- **D-02:** Em sendDataToFireBase(), checar o retorno de Firebase.RTDB.updateNode(). Se falhar:
  - Incrementar firebaseFailCount
  - Calcular `firebaseSkipRemaining = min(1u << firebaseFailCount, 16u)` — cap em 16 ciclos (~80 min com timerDelay=5 min)
  - Chamar `enqueuePendingReading(temperature, timestamp)` para preservar a leitura nao-enviada
  - Logar: `[Firebase] Write failed (fail #N), backing off N cycles`
- **D-03:** Em callFirebase(), quando firebaseSkipRemaining > 0: decrementar, chamar `enqueuePendingReading(temperature, timestamp)`, e retornar sem enviar
- **D-04:** Quando `firebaseFailCount >= 5 && firebaseSkipRemaining == 0`: reset via drawResetDevice() + delay(2000) + ESP.reset(). Cap de seguranca final.
- **D-05:** Qualquer write bem-sucedido: zera ambos os contadores; chama `drainOnePendingReading()` para drenar a fila gradualmente.

### REL-04b — Fila persistente de leituras (solicitado pelo usuario)

Requisito: Leituras perdidas durante backoff devem ser preservadas em EEPROM e re-enviadas apos reconexao, inclusive apos reboot manual.

- **D-06:** Struct a definir em EEPROMFunction.h:
  ```cpp
  struct PendingReading {
    uint32_t timestamp;
    float    temperature;
  }; // 8 bytes por entrada
  ```
- **D-07:** Layout EEPROM da fila — defines em config.h:
  - `PENDING_QUEUE_ADDR   = 380` (margem apos LDID em addr 300+38=338)
  - `PENDING_QUEUE_MAGIC  = 0xA5` (distingue fila valida de EEPROM virgem 0xFF)
  - `PENDING_QUEUE_MAXLEN = 16`
  - Estrutura: magic(1B) + count(1B) + 16 x PendingReading(8B) = 130 bytes, termina em addr 510 — dentro dos 512B totais
- **D-08:** Variaveis RAM espelho da fila em config.h (apos os defines):
  ```cpp
  PendingReading pendingQueue[PENDING_QUEUE_MAXLEN];
  uint8_t        pendingQueueLen = 0;
  ```
- **D-09:** Funcoes a implementar em EEPROMFunction.h (localidade com demais funcoes EEPROM):
  - `void loadPendingQueue()` — chamada em setup() apos EEPROM.begin(). Le magic byte; se 0xA5, carrega entries para RAM com EEPROM.get(). Se invalido (0xFF = virgem), pendingQueueLen = 0.
  - `void enqueuePendingReading(float temp, uint32_t ts)` — adiciona entry a fila RAM e persiste na EEPROM. Se fila cheia (pendingQueueLen == PENDING_QUEUE_MAXLEN): ring buffer — descarta a mais antiga via memmove(pendingQueue, pendingQueue+1, ...), mantem as mais recentes. Escreve com EEPROM.put() + EEPROM.commit(). Loga: `[Queue] Full, dropping oldest (ts=%u)` no overflow.
  - `void drainOnePendingReading()` — se pendingQueueLen == 0, retorna. Envia pendingQueue[0] ao RTDB via Firebase.RTDB.updateNode() para o path /datalogger/{timestamp} com temp e timestamp. Se sucesso: memmove para remover [0], decrementa pendingQueueLen, atualiza count na EEPROM (ou invalida magic se vazia via EEPROM.write(PENDING_QUEUE_ADDR, 0x00) + commit). Se falha: nao remove, tenta no proximo ciclo.
- **D-10:** EEPROM write policy: usar EEPROM.put()/EEPROM.get() para structs (mais eficiente que byte-a-byte). EEPROM.commit() apenas em enqueue e drain — nunca em loop tight. Flash wear: max 16 writes/outage — seguro para campo.
- **D-11:** No overflow do ring buffer, logar `[Queue] Full, dropping oldest (ts=%u)`.
- **D-12:** drainOnePendingReading() envia UM por ciclo — evita burst de writes no Firebase apos outage longa. Dados antigos chegam em ordem cronologica ao RTDB.
- **Rationale capacidade:** Outagem leve (fail 1 = 2 ciclos): 2 leituras preservadas. Outagem severa (fail 1-4 = 30 ciclos): ring buffer mantém as 16 mais recentes = ultimos 80 min de dados. Flash wear: max 16 writes/outage, ciclos por milhar — seguro para implantacao em campo.

### REL-05 — Intervalo separado para config fetch

- **D-13:** Intervalo fixo: `#define CONFIG_FETCH_INTERVAL 3600000L` (60 min) em config.h. Nao configuravel via Firebase — seria dependencia circular.
- **D-14:** Adicionar `unsigned long configFetchPrevMillis = 0;` em config.h ao lado de lastHeartbeatMillis.
- **D-15:** Remover getDeviceConfigurations() de dentro de sendDataToFireBase(). Adicionar timer proprio em loop():
  ```cpp
  if (millis() - configFetchPrevMillis > CONFIG_FETCH_INTERVAL || configFetchPrevMillis == 0)
  {
    configFetchPrevMillis = millis();
    getDeviceConfigurations();
  }
  ```
- **D-16:** Primeira execucao permanece em setup() (~linha 522). Timer em loop() dispara apenas apos 60 min.

### OBS-02 — Status de WiFi no OLED durante runtime

- **D-17:** overlayStatusBar mantido sem alteracoes (ja mostra icon_wifi_off quando WiFi cai).
- **D-18:** Em loop(), quando WiFi.status() != WL_CONNECTED no bloco de render: chamar drawWifiReconnecting() em vez de ui.update(). Sem delay, sem bloqueio.
- **D-19:** Criar drawWifiReconnecting() em drawOled.h — layout baseado em drawBootProgress mas sem barra de progresso. Linha 1 (y=10): "Sem WiFi" em ArialMT_Plain_16. Linha 2 (y=38): "Reconectando..." em ArialMT_Plain_10.
- **D-20:** Retorno automatico ao ui.update() quando WiFi reconecta — sem acao explicita de restauracao.

### Claude's Discretion

- Onde inicializar os defines PENDING_QUEUE_* em config.h (apos o mapa EEPROM existente ou em bloco separado)
- Ordem exata dos checks em loop() (WiFi render check antes ou depois do config fetch timer)
- Nomes de constantes e variaveis seguindo padrao do arquivo (camelCase vars, SCREAMING_SNAKE defines)

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Codigo existente — ler antes de implementar

- `src/main.cpp` — callFirebase() (~linhas 337-388), sendDataToFireBase() (~linhas 294-330), loop() (~linhas 562-590), setup() EEPROM.begin() (~linha 416)
- `src/config.h` — mapa EEPROM (linhas 6-10), variaveis globais, defines de intervalo (~linhas 27-90)
- `src/EEPROMFunction.h` — EEPROM_SIZE=512, readStringFromEEPROM(), writeStringToEEPROM(), padrao de EEPROM.commit()
- `src/drawOled.h` — overlayStatusBar() (~linha 246), drawBootProgress() (~linha 327), drawBootError() (~linha 365)

### Mapa EEPROM atual (para nao sobrescrever regioes existentes)

```
Addr   0- 99: EMAIL_ADDR       (string, max 99 chars, formato: length_byte + data)
Addr 100-199: PASS_ADDR        (string, max 99 chars)
Addr 200-299: DEVICE_NAME_ADDR (string, max 99 chars)
Addr 300-337: LDID_ADDR        (UUID 36 chars + 1 length byte = 37 bytes)
Addr 338-379: livre            (margem de seguranca)
Addr 380-509: PENDING_QUEUE_ADDR (fila: 1B magic + 1B count + 16x8B = 130 bytes)
Addr 510-511: livre
```

### Requisitos

- `.planning/REQUIREMENTS.md` — REL-04, REL-05, OBS-02

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets

- `EEPROM.put()` / `EEPROM.get()` para struct PendingReading — mais eficiente e seguro que serializacao byte-a-byte
- `drawBootProgress(const char*, uint8_t)` — clonar como base de `drawWifiReconnecting()`
- `lastHeartbeatMillis` em config.h — padrao de nomenclatura para `configFetchPrevMillis`

### Established Patterns

- Timers: `millis() - prevMillis > INTERVAL` com `|| prevMillis == 0` para first-run
- Contadores globais declarados em config.h, resetados no ponto de uso
- Log prefixes existentes: `[WiFi]`, `[Auth]`, `[WDT]` — usar `[Firebase]` e `[Queue]` para novos logs
- EEPROM: nunca chamar `EEPROM.commit()` em loop tight — apenas apos writes reais

### Integration Points

- `setup()` — adicionar `loadPendingQueue()` apos `EEPROM.begin()`
- `sendDataToFireBase()` — mudar retorno para bool, chamar `enqueuePendingReading()` em falha, `drainOnePendingReading()` em sucesso
- `callFirebase()` — guard de backoff; em skip cycle chamar `enqueuePendingReading(temperature, timestamp)` antes de retornar
- `loop()` — timer CONFIG_FETCH_INTERVAL paralelo ao heartbeat + WiFi render check
- `config.h` — defines PENDING_QUEUE_*, globals firebaseFailCount, firebaseSkipRemaining, configFetchPrevMillis, pendingQueue[], pendingQueueLen
- `EEPROMFunction.h` — struct PendingReading, loadPendingQueue(), enqueuePendingReading(), drainOnePendingReading()

</code_context>

<specifics>
## Specific Ideas

- Magic byte 0xA5 distingue fila valida de EEPROM virgem (0xFF) sem overhead de CRC
- drainOnePendingReading() envia UM por ciclo — dados antigos chegam em ordem cronologica ao RTDB, sem burst
- Ring buffer ao overflow preserva dados mais recentes — comportamento correto para temperatura em campo
- A fila no RTDB usa o mesmo path /datalogger/{timestamp} que sendDataToFireBase() — backfill transparente para o app

</specifics>

<deferred>
## Deferred Ideas

- Status bar com "retry em Xmin" countdown — scope de Fase 3/4
- Config fetch interval configuravel via Firebase — dependencia circular, adiar indefinidamente
- Envio em batch de toda a fila apos reconexao — avaliacao futura

</deferred>

---

*Phase: 02-firebase-resilience-observability*
*Context gathered: 2026-04-11*
