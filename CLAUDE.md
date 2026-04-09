@../CLAUDE.md

# Guardião — Firmware

## Contexto deste Repositório
Firmware embarcado para os dispositivos Guardião. Subproduto da plataforma, responsável por coletar dados dos sensores e acionar atuadores locais, comunicando-se com o Firebase Realtime Database.

## Hardware Alvo

| Versão | MCU | Status |
|--------|-----|--------|
| A | ESP-12E (PCB própria) | ✅ Existente — refatorar |
| B | ESP32-S/S3 (PCB a desenvolver) | 🔲 Futuro |
| C | ESP32-S/S3 + LoRa | 🔲 Futuro |

## Stack
- **Linguagem:** C++ (Arduino framework)
- **Platform:** PlatformIO ou Arduino IDE
- **Sensor inicial:** DS18B20 (temperatura via OneWire)
- **Atuador inicial:** Buzzer passivo 3.3V
- **Comunicação:** HTTP → Firebase Realtime DB (Fase 1)

## Estrutura Esperada
```
guardiao-firmware/
    ├── src/
    │   └── main.cpp
    ├── lib/
    │   └── (bibliotecas locais)
    ├── include/
    │   └── config.h
    ├── platformio.ini
    ├── .gitignore
    ├── README.md
    └── CHANGELOG.md
```

## Variáveis de Ambiente (config.h ou secrets.h)
```cpp
#define WIFI_SSID "sua-rede"
#define WIFI_PASSWORD "sua-senha"
#define FIREBASE_HOST "seu-projeto.firebaseio.com"
#define FIREBASE_AUTH "seu-token"
#define DEVICE_ID "id-unico-do-dispositivo"
```

## Protocolo de Comunicação — Fase 1 (HTTP)
- Endpoint: Firebase Realtime Database REST API
- Método: PUT/PATCH
- Payload: JSON com leitura do sensor + timestamp + device_id
- Intervalo de leitura: configurável (padrão: 30s)

## GitHub
- Repositório: `guardiao-firmware`
- Branch principal: `main`
- Workflow: ver `../DEVELOPMENT.md`

## Checklist de Qualidade
- [ ] Compila sem warnings
- [ ] Testado no hardware real (ESP-12E)
- [ ] Reconexão Wi-Fi tratada
- [ ] Timeout HTTP tratado
- [ ] Consumo de memória verificado (ESP-12E tem ~80KB RAM)


# AI-DLC and Spec-Driven Development

Kiro-style Spec Driven Development implementation on AI-DLC (AI Development Life Cycle)

## Project Context

### Paths
- Steering: `.kiro/steering/`
- Specs: `.kiro/specs/`

### Steering vs Specification

**Steering** (`.kiro/steering/`) - Guide AI with project-wide rules and context
**Specs** (`.kiro/specs/`) - Formalize development process for individual features

### Active Specifications
- Check `.kiro/specs/` for active specifications
- Use `/kiro:spec-status [feature-name]` to check progress

## Development Guidelines
- Think in English, generate responses in Portuguese. All Markdown content written to project files (e.g., requirements.md, design.md, tasks.md, research.md, validation reports) MUST be written in the target language configured for this specification (see spec.json.language).

## Minimal Workflow
- Phase 0 (optional): `/kiro:steering`, `/kiro:steering-custom`
- Phase 1 (Specification):
  - `/kiro:spec-init "description"`
  - `/kiro:spec-requirements {feature}`
  - `/kiro:validate-gap {feature}` (optional: for existing codebase)
  - `/kiro:spec-design {feature} [-y]`
  - `/kiro:validate-design {feature}` (optional: design review)
  - `/kiro:spec-tasks {feature} [-y]`
- Phase 2 (Implementation): `/kiro:spec-impl {feature} [tasks]`
  - `/kiro:validate-impl {feature}` (optional: after implementation)
- Progress check: `/kiro:spec-status {feature}` (use anytime)

## Development Rules
- 3-phase approval workflow: Requirements → Design → Tasks → Implementation
- Human review required each phase; use `-y` only for intentional fast-track
- Keep steering current and verify alignment with `/kiro:spec-status`
- Follow the user's instructions precisely, and within that scope act autonomously: gather the necessary context and complete the requested work end-to-end in this run, asking questions only when essential information is missing or the instructions are critically ambiguous.

## Steering Configuration
- Load entire `.kiro/steering/` as project memory
- Default files: `product.md`, `tech.md`, `structure.md`
- Custom files are supported (managed via `/kiro:steering-custom`)
