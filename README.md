# guardiao-firmware

Embedded firmware for the **Guardião IoT** platform devices. Responsible for reading sensors, triggering local actuators, and communicating with Firebase Realtime Database.

---

## Supported Hardware

| Version | MCU               | Sensors          | Communication    |
| ------- | ----------------- | ---------------- | ---------------- |
| A       | ESP-12E           | DS18B20 (temp)   | Wi-Fi HTTP       |
| B       | ESP32-S/S3        | DS18B20 + extras | Wi-Fi HTTP/MQTT  |
| C       | ESP32-S/S3 + LoRa | DS18B20 + extras | Wi-Fi + LoRa     |

---

## Prerequisites

- [PlatformIO](https://platformio.org/) or Arduino IDE 2.x
- Python 3.8+ (for PlatformIO)
- `Firebase ESP Client` or `FirebaseESP8266` library
- `DallasTemperature` + `OneWire` libraries

---

## Setup

1. Clone the repository:

```bash
git clone https://github.com/mfonte/guardiao-firmware.git
cd guardiao-firmware
```

2. Copy the configuration template:

```bash
cp src/config.example.h src/config.h
```

3. Edit `src/config.h` with your credentials and local parameters:

```cpp
#define API_KEY "YOUR_FIREBASE_API_KEY"
#define DATABASE_URL "https://your-project-default-rtdb.firebaseio.com/"
```

> `src/config.h` is local to your environment and must not be committed.

4. Build and upload:

```bash
# PlatformIO
pio run --target upload

# Arduino IDE
# Select the correct board and upload normally
```

---

## Project Structure

```
guardiao-firmware/
    ├── src/
    │   ├── main.cpp          # Main application code
    │   ├── drawOled.h        # OLED display rendering functions
    │   ├── EEPROMFunction.h  # EEPROM read/write helpers
    │   ├── OTAUpdate.h       # OTA update setup
    │   ├── config.example.h  # Configuration template
    │   └── config.h          # Local configuration (git-ignored)
    ├── lib/                  # Local libraries
    ├── include/
    │   └── README
    ├── platformio.ini        # PlatformIO configuration
    ├── .gitignore
    ├── CHANGELOG.md
    └── README.md
```

---

## Data Flow

```
DS18B20 → ESP → Firebase Realtime DB → App / Web
```

- Configurable reading interval (default: 5 minutes, adjustable via app)
- JSON payload: `{ "temperature": 25.4, "timestamp": 1234567890, "deviceName": "my-sensor" }`
- Protocol v2: LDID-based device identity, boot messages, periodic heartbeats

---

## Development

Follow the workflow defined in [`../DEVELOPMENT.md`](../DEVELOPMENT.md).

```bash
# Start Claude Code
claude
```

---

## Current Version

See [CHANGELOG.md](CHANGELOG.md) for version history.

---

## License

Proprietary — Guardião IoT Platform
