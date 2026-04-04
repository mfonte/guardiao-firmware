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
