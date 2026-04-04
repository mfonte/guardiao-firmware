# guardiao-firmware

Firmware embarcado para os dispositivos da plataforma **Guardião IoT**. Responsável pela leitura de sensores, acionamento de atuadores locais e comunicação com o Firebase Realtime Database.

---

## Hardware Suportado

| Versão | MCU | Sensores | Comunicação |
|--------|-----|----------|-------------|
| A | ESP-12E | DS18B20 (temp) | Wi-Fi HTTP |
| B | ESP32-S/S3 | DS18B20 + extras | Wi-Fi HTTP/MQTT |
| C | ESP32-S/S3 + LoRa | DS18B20 + extras | Wi-Fi + LoRa |

---

## Pré-requisitos

- [PlatformIO](https://platformio.org/) ou Arduino IDE 2.x
- Python 3.8+ (para PlatformIO)
- Biblioteca `Firebase ESP Client` ou `FirebaseESP8266`
- Biblioteca `DallasTemperature` + `OneWire`

---

## Configuração

1. Clone o repositório:
```bash
git clone https://github.com/seu-usuario/guardiao-firmware.git
cd guardiao-firmware
```

2. Copie o arquivo de configuração:
```bash
cp include/config.example.h include/config.h
```

3. Edite `include/config.h` com suas credenciais:
```cpp
#define WIFI_SSID "sua-rede"
#define WIFI_PASSWORD "sua-senha"
#define FIREBASE_HOST "seu-projeto.firebaseio.com"
#define FIREBASE_AUTH "seu-token"
#define DEVICE_ID "id-unico-do-dispositivo"
```

4. Compile e faça upload:
```bash
# PlatformIO
pio run --target upload

# Arduino IDE
# Selecione a placa correta e faça upload normalmente
```

---

## Estrutura do Projeto

```
guardiao-firmware/
    ├── src/
    │   └── main.cpp          # Código principal
    ├── lib/                  # Bibliotecas locais
    ├── include/
    │   ├── config.example.h  # Template de configuração
    │   └── config.h          # Suas credenciais (não comitar)
    ├── platformio.ini        # Configuração PlatformIO
    ├── .gitignore
    ├── CHANGELOG.md
    └── README.md
```

---

## Fluxo de Dados

```
DS18B20 → ESP → Firebase Realtime DB → App / Web
```

- Leitura a cada 30 segundos (configurável)
- Payload JSON: `{ "temperatura": 25.4, "timestamp": 1234567890, "device_id": "abc123" }`

---

## Desenvolvimento

Siga o fluxo definido em [`../DEVELOPMENT.md`](../DEVELOPMENT.md).

```bash
# Instalar GSD
npx get-shit-done-cc --claude --local

# Instalar SDD
npx cc-sdd@latest --claude --lang pt

# Iniciar Claude Code
claude
```

---

## Versão Atual

Veja [CHANGELOG.md](CHANGELOG.md) para histórico de versões.

---

## Licença

Proprietary — Guardião IoT Platform
