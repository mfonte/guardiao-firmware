# Technology Stack

**Analysis Date:** 2026-04-11

## Languages

**Primary:**
- C++ (Arduino dialect) — all firmware in `src/`

## Runtime

**Environment:**
- ESP8266 (Xtensa LX106, 80 MHz, ~80 KB RAM, ~1 MB Flash)
- Target board: NodeMCU v2 (`nodemcuv2`)

**Package Manager:**
- PlatformIO (Python-based)
- Lockfile: not present; versions pinned by semver ranges in `platformio.ini`

## Frameworks

**Core:**
- Arduino framework for ESP8266 — `setup()`, `loop()`, `Arduino.h`, `EEPROM.h`, `Ticker.h`

**Display:**
- `lib/esp8266-oled-ssd1306/` (local copy) — SSD1306Wire I2C driver + OLEDDisplayUi multi-frame animation engine

**Connectivity:**
- `lib/WiFiManager/` (local copy) — captive portal for first-time WiFi credential provisioning

**OTA:**
- ArduinoOTA (ESP8266 core built-in) — in-production over-the-air firmware updates

## Key Dependencies (platformio.ini)

| Package | Version | Purpose |
|---------|---------|---------|
| `mobizt/Firebase Arduino Client Library for ESP8266 and ESP32` | `^4.4.14` | RTDB read/write, email+password auth |
| `milesburton/DallasTemperature` | `^3.11.0` | DS18B20 OneWire temperature sensor |
| `arduino-libraries/NTPClient` | `^3.2.1` | NTP epoch for clock frame + timestamp guard |
| `bblanchon/ArduinoJson` | `^7.0.4` | Deserialise Firebase config JSON |
| `Ticker` (core) | — | 1-second ISR for software watchdog |
| `EEPROM` (core) | — | 512-byte persistent credential store |

## Configuration

**Environment:**
- `src/config.h` — gitignored, device-local. Contains API key, Firebase URL, pin assignments, EEPROM address map, all runtime global variables
- `src/config.example.h` — committed template; copy to `src/config.h` before first build. Defines `TIMEZONE_OFFSET_SEC -10800L` (BRT)

**Build:**
- `platformio.ini` — single `[env:nodemcuv2]` environment; baud 115200; OTA upload commented out

## Platform Requirements

**Development:**
- PlatformIO CLI or VS Code PlatformIO extension
- Python 3.8+
- `src/config.h` created from `src/config.example.h`

**Production Hardware:**
- ESP8266 NodeMCU v2
- DS18B20 on D5 (OneWire, 12-bit resolution)
- Passive buzzer 3.3V on D6
- SSD1306 OLED 128x64 (I2C: SDA=D2, SCL=D1, addr 0x3C)
- Button S1 on D8 (INPUT_PULLUP), Button S2 on D7 (INPUT)
- ADC_MODE set to ADC_VCC for supply voltage monitoring

---

*Stack analysis: 2026-04-11*
