# Phase 4: Testing Infrastructure — Research

**Researched:** 2026-04-11 (re-research)
**Domain:** PlatformIO Native Unit Testing (Unity framework) para firmware ESP8266
**Confidence:** HIGH

## Summary

O objetivo é estabelecer `pio test -e native` executando no desktop (macOS, Apple Clang 17) sem hardware. As funções-alvo estão em `main.cpp` (`calculateStatusBitmap()` L729-743, `checkThresholdAlert()` L238-277) e `EEPROMFunction.h` (L16-51 string read/write, L69-112 pending queue). Todas dependem de globals definidos em `config.h` e tipos Arduino (`String`, `EEPROM`).

O desafio central: **desacoplamento** — extrair lógica pura para `src/logic.h`, criar stubs mínimos em `test/stubs/`, e isolar o pending queue do Firebase.

**Primary recommendation:** `src/logic.h` com `calculateStatusBitmap()` e `evaluateThreshold()` (nova função pura). Stubs mínimos para `String` e `EEPROM`. Guard `#ifdef NATIVE_TEST` em `EEPROMFunction.h` para excluir deps Firebase.

<phase_requirements>

## Phase Requirements

| ID      | Description                                                                                | Research Support                                                                                                                                                                                              |
| ------- | ------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| TEST-01 | Ambiente de teste nativo PlatformIO configurado com framework Unity                        | `[env:native]` em platformio.ini com `platform = native`, `test_framework = unity`, `build_src_filter = -<*>`. Unity 2.6.1 auto-managed. Native platform 1.2.1 instalado.                                     |
| TEST-02 | Testes unitários para calculateStatusBitmap(), checkThresholdAlert(), encode/decode EEPROM | Funções puras extraídas para `src/logic.h`. checkThresholdAlert() decomposta em `evaluateThreshold()` (retorna int). EEPROM string functions testadas via stub. Pending queue testável com guard NATIVE_TEST. |

</phase_requirements>

## Project Constraints (from CLAUDE.md)

- **Linguagem:** C++ (Arduino framework)
- **Platform:** PlatformIO
- **Hardware alvo:** ESP-12E (ESP8266), ~80KB RAM
- **Naming:** `camelCase()` para funções, `UPPER_SNAKE` para constantes
- **Headers:** `.h` com `#pragma once`
- **Indentação:** 2 espaços, estilo Allman
- **Build existente:** `pio run -e nodemcuv2` sem warnings

## Codebase Analysis — Functions Under Test

### calculateStatusBitmap() — src/main.cpp L729-743

```cpp
uint8_t calculateStatusBitmap()
{
  uint8_t st = 0x01; // always online (bit 0)
  if (sensorError)
    st |= 0x04; // error reading (bit 2)
  bool highAlert = (thresholdMode == "both" || thresholdMode == "upperOnly") && temperature > higherTemp;
  bool lowAlert = (thresholdMode == "both" || thresholdMode == "lowerOnly") && temperature < lowerTemp;
  if (highAlert || lowAlert)
    st |= 0x10; // alert (bit 4)
  return st;
}
```

**Dependencies:** `sensorError` (bool), `thresholdMode` (String), `temperature` (float), `higherTemp` (float), `lowerTemp` (float). Todas globals de `config.h`. **Pura** — sem side effects.

### checkThresholdAlert() — src/main.cpp L238-277

```cpp
void checkThresholdAlert()
{
  if (sensorError) { countMessageSending = 0; return; }
  if (thresholdMode == "none") { countMessageSending = 0; return; }
  bool overHigh = (thresholdMode == "both" || thresholdMode == "upperOnly") && temperature > higherTemp;
  bool underLow = (thresholdMode == "both" || thresholdMode == "lowerOnly") && temperature < lowerTemp;
  if (overHigh) { countMessageSending++; startAlarm(); }
  else if (underLow) { countMessageSending++; startAlarm(); }
  else { countMessageSending = 0; }
}
```

**Dependencies:** Mesmos globals + `startAlarm()` (de alarm.h) + `Serial.println()`. **Impura** — chama startAlarm(). Solução: extrair `evaluateThreshold()` como função pura.

### readStringFromEEPROM / writeStringToEEPROM — src/EEPROMFunction.h L16-51

**Dependencies:** `EEPROM.read()`, `EEPROM.write()`, `EEPROM.commit()`, `EEPROM_SIZE` const. **Pure I/O** — testável com EEPROM stub RAM-backed.

### Pending Queue — src/EEPROMFunction.h L58-152

**Dependencies:** `EEPROM`, `PendingReading` struct, `PENDING_QUEUE_*` defines, `FirebaseData fbdo` (apenas em `drainOnePendingReading`), `Serial.printf`.

`enqueuePendingReading()` + `loadPendingQueue()` são testáveis. `drainOnePendingReading()` depende de Firebase — excluído.

### Global Variables (from config.h) Needed by Tests

| Variable               | Type      | Default  | Used By           |
| ---------------------- | --------- | -------- | ----------------- |
| `sensorError`          | `bool`    | `true`   | bitmap, threshold |
| `temperature`          | `float`   | `-127.0` | bitmap, threshold |
| `higherTemp`           | `float`   | `80.0`   | bitmap, threshold |
| `lowerTemp`            | `float`   | `-80.0`  | bitmap, threshold |
| `thresholdMode`        | `String`  | `"both"` | bitmap, threshold |
| `countMessageSending`  | `int`     | `0`      | threshold         |
| `PENDING_QUEUE_ADDR`   | `#define` | `380`    | pending queue     |
| `PENDING_QUEUE_MAGIC`  | `#define` | `0xA5`   | pending queue     |
| `PENDING_QUEUE_MAXLEN` | `#define` | `16`     | pending queue     |

## Standard Stack

### Core

| Library                    | Version | Purpose                       | Why Standard                               |
| -------------------------- | ------- | ----------------------------- | ------------------------------------------ |
| Unity (ThrowTheSwitch)     | 2.6.1   | Framework de teste C/C++      | Built-in PlatformIO, leve, padrão embedded |
| PlatformIO Native Platform | 1.2.1   | Compilação e execução desktop | Plataforma oficial para testes nativos     |

### Alternatives Considered

| Instead of    | Could Use   | Tradeoff                                                    |
| ------------- | ----------- | ----------------------------------------------------------- |
| Unity         | GoogleTest  | Mais poderoso mas overkill para lógica pura simples         |
| Stubs manuais | ArduinoFake | Pesado, APIs desnecessárias — só precisamos String + EEPROM |

## Architecture Patterns

### Pattern 1: Extração de Lógica Pura para Header

Mover `calculateStatusBitmap()` e nova `evaluateThreshold()` para `src/logic.h`. O header declara `extern` para as globals e provê as funções `inline`.

**logic.h não inclui config.h** — evita cascade de deps Arduino. O test file define as globals, main.cpp já as tem via config.h.

### Pattern 2: Stubs Arduino Mínimos

`test/stubs/Arduino.h`:

- `String` class wrapping `std::string` com `==`, `!=`, `length()`, `operator[]`, `c_str()`
- `SerialStub` com métodos no-op
- Tipos: `uint8_t`, `uint32_t` via `<cstdint>`
- `min()` macro (usado por EEPROMFunction.h)
- `<cstring>` include (memmove usado pelo pending queue)

`test/stubs/EEPROM.h`:

- `EEPROMClass` com `uint8_t _data[512]` RAM array
- `read()`, `write()`, `put()`, `get()`, `begin()`, `commit()`, `clear()`

### Pattern 3: platformio.ini `[env:native]`

```ini
[env:native]
platform = native
test_framework = unity
build_src_filter = -<*>
build_flags =
  -I test/stubs
  -std=c++11
  -D NATIVE_TEST
```

- `build_src_filter = -<*>` — não compila src/
- `-I test/stubs` — stubs no include path
- `-D NATIVE_TEST` — guard macro para conditional compilation
- `-std=c++11` — para std::string

### Pattern 4: NATIVE_TEST Guard em EEPROMFunction.h

```cpp
#pragma once
#ifndef NATIVE_TEST
#include <EEPROM.h>
#include "config.h"
#else
#include "EEPROM.h"
// Define apenas o necessário para compilar
const int EEPROM_SIZE = 512;
#define PENDING_QUEUE_ADDR    380
#define PENDING_QUEUE_MAGIC   0xA5
#define PENDING_QUEUE_MAXLEN  16
#endif
```

Isso permite incluir `EEPROMFunction.h` nos testes sem puxar config.h + Firebase. A seção do Firebase (`extern FirebaseData fbdo`, `drainOnePendingReading()`) também fica dentro de `#ifndef NATIVE_TEST`.

### Pattern 5: evaluateThreshold() — Nova Função Pura

```cpp
// Returns: 0 = within limits, 1 = overHigh, -1 = underLow, -2 = sensorError, -3 = disabled
inline int evaluateThreshold()
{
  if (sensorError) return -2;
  if (thresholdMode == "none") return -3;
  bool overHigh = (thresholdMode == "both" || thresholdMode == "upperOnly") && temperature > higherTemp;
  bool underLow = (thresholdMode == "both" || thresholdMode == "lowerOnly") && temperature < lowerTemp;
  if (overHigh) return 1;
  if (underLow) return -1;
  return 0;
}
```

`checkThresholdAlert()` em main.cpp usa `evaluateThreshold()` + reage (startAlarm, countMessageSending).

### Recommended Test Structure

```
test/
├── stubs/
│   ├── Arduino.h         # String + Serial + tipos básicos
│   └── EEPROM.h          # EEPROMClass RAM-backed
└── test_native/
    └── test_logic.cpp    # Todos os testes
```

## Anti-Patterns to Avoid

- **Incluir main.cpp nos testes** — puxa WiFi, Firebase, OLED. Impossível compilar nativo.
- **Stubs completos do Arduino** — YAGNI. Só o que as funções testadas usam.
- **Globals não resetados entre testes** — setUp() obrigatório com valores default.
- **EEPROM stub com estado sujo** — `EEPROM.clear()` no setUp().
- **config.h incluído nos testes** — puxa D2, D1 e defines de pinos inexistentes no desktop.

## Common Pitfalls

### Pitfall 1: build_src_filter esquecido

PlatformIO tenta compilar src/main.cpp para native → cascade de erros (ESP8266WiFi.h not found).
**Fix:** `build_src_filter = -<*>` no `[env:native]`.

### Pitfall 2: String stub incompleto

`thresholdMode == "both"` requer `bool operator==(const char*)`.
**Fix:** Implementar exatamente os operadores usados.

### Pitfall 3: Globals não resetados entre testes

Unity roda testes sequencialmente no mesmo processo.
**Fix:** `setUp()` reseta todas as globals.

### Pitfall 4: EEPROMFunction.h inclui config.h (L3)

Cascade de erros sobre D2, D5, FirebaseData.
**Fix:** Guard `#ifndef NATIVE_TEST` ou definir macros necessárias antes do include.

### Pitfall 5: min() macro conflito

`min(count, (uint8_t)PENDING_QUEUE_MAXLEN)` em EEPROMFunction.h usa min macro do Arduino.
**Fix:** Definir `#define min(a,b) ((a)<(b)?(a):(b))` no stub Arduino.h.

### Pitfall 6: memmove/memcpy/memset sem include

Pending queue usa `memmove()`. No native, precisa `<cstring>`.
**Fix:** `#include <cstring>` no stub Arduino.h.

## Don't Hand-Roll

| Problem        | Don't Build               | Use Instead                      |
| -------------- | ------------------------- | -------------------------------- |
| Test framework | Custom assert macros      | Unity (PlatformIO built-in)      |
| Arduino String | Re-implementação completa | Stub mínimo wrapping std::string |
| EEPROM mock    | File-backed persistence   | RAM array 512 bytes              |

## Test Coverage Plan

### calculateStatusBitmap() — 6 testes

| Test                 | sensorError | thresholdMode | temperature | Expected |
| -------------------- | ----------- | ------------- | ----------- | -------- |
| Normal (online only) | false       | "both"        | 25.0        | 0x01     |
| Sensor error         | true        | "both"        | 25.0        | 0x05     |
| High alert           | false       | "both"        | 90.0        | 0x11     |
| Low alert            | false       | "both"        | -90.0       | 0x11     |
| Error + alert        | true        | "both"        | 90.0        | 0x15     |
| Mode none            | false       | "none"        | 90.0        | 0x01     |

### evaluateThreshold() — 7 testes

| Test                  | sensorError | thresholdMode | temperature | Expected |
| --------------------- | ----------- | ------------- | ----------- | -------- |
| Within limits (both)  | false       | "both"        | 25.0        | 0        |
| Over high (both)      | false       | "both"        | 90.0        | 1        |
| Under low (both)      | false       | "both"        | -90.0       | -1       |
| Over high (upperOnly) | false       | "upperOnly"   | 90.0        | 1        |
| Under low (lowerOnly) | false       | "lowerOnly"   | -90.0       | -1       |
| Sensor error          | true        | "both"        | 25.0        | -2       |
| Mode none             | false       | "none"        | 90.0        | -3       |

### EEPROM String — 3 testes

| Test                 | Input      | Expected   |
| -------------------- | ---------- | ---------- |
| Round-trip normal    | "hello123" | "hello123" |
| Round-trip empty     | ""         | ""         |
| Virgin EEPROM (0xFF) | —          | ""         |

### Pending Queue — 3 testes

| Test                 | Action           | Expected                     |
| -------------------- | ---------------- | ---------------------------- |
| Enqueue + load       | enqueue 3, load  | 3 entries recovered          |
| Ring-buffer overflow | enqueue MAXLEN+1 | oldest dropped, len = MAXLEN |
| Virgin EEPROM        | load from 0xFF   | len = 0                      |

**Total: 19 testes**

## Validation Architecture

### Test Framework

| Property           | Value                                            |
| ------------------ | ------------------------------------------------ |
| Framework          | Unity 2.6.1 (via PlatformIO built-in)            |
| Config file        | `platformio.ini` → `[env:native]` (a ser criado) |
| Quick run command  | `pio test -e native`                             |
| Full suite command | `pio test -e native -v`                          |

### Phase Requirements → Test Map

| Req ID   | Behavior                                   | Test Type | Automated Command                   | File Exists? |
| -------- | ------------------------------------------ | --------- | ----------------------------------- | ------------ |
| TEST-01  | Ambiente native executa sem erros          | smoke     | `pio test -e native`                | ❌ Wave 0    |
| TEST-02a | calculateStatusBitmap() — all alarm states | unit      | `pio test -e native -f test_native` | ❌ Wave 0    |
| TEST-02b | evaluateThreshold() — 4+ modes             | unit      | `pio test -e native -f test_native` | ❌ Wave 0    |
| TEST-02c | EEPROM string encode/decode round-trip     | unit      | `pio test -e native -f test_native` | ❌ Wave 0    |
| TEST-02d | Pending queue enqueue/load/overflow        | unit      | `pio test -e native -f test_native` | ❌ Wave 0    |

### Sampling Rate

- **Per task commit:** `pio test -e native`
- **Per wave merge:** `pio test -e native -v`
- **Phase gate:** Full suite green before verify-work

### Wave 0 Gaps

- [ ] `[env:native]` section no platformio.ini
- [ ] `test/stubs/Arduino.h` — stub mínimo com String class
- [ ] `test/stubs/EEPROM.h` — stub RAM-backed EEPROMClass
- [ ] `src/logic.h` — funções puras extraídas de main.cpp
- [ ] `test/test_native/test_logic.cpp` — testes unitários

## Environment Availability

| Dependency        | Available        | Version |
| ----------------- | ---------------- | ------- |
| PlatformIO CLI    | ✓                | 6.1.19  |
| Native platform   | ✓                | 1.2.1   |
| Apple Clang (C++) | ✓                | 17.0.0  |
| Unity framework   | ✓ (auto-managed) | 2.6.1   |

**Missing dependencies:** Nenhum.

## Open Questions

1. **EEPROMFunction.h include strategy:** Guard `#ifndef NATIVE_TEST` é a opção mais simples. Alternativa: header separado para string functions. Recomendação: guard (menos invasivo).

2. **evaluateThreshold() vs stub de startAlarm():** Extrair evaluateThreshold() é mais limpo — testa a decisão sem mock de side effects. checkThresholdAlert() em main.cpp passa a usar evaluateThreshold().

## Sources

- PlatformIO Docs — Unity Framework, Test Hierarchy
- Codebase: `src/main.cpp` (L238-277, L729-743), `src/EEPROMFunction.h` (L16-51, L58-152), `src/config.h`
- PlatformIO CLI: `pio platform list` confirms native 1.2.1
