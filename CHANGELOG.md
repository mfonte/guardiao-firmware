# Changelog — guardiao-firmware

All notable changes to this project will be documented here.
Format based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Versioning follows [Semantic Versioning](https://semver.org/).

---

## [Unreleased]

### Added
- DS18B20 temperature sensor reading via OneWire
- Data push via HTTP to Firebase Realtime Database
- Automatic Wi-Fi reconnection via WiFiManager captive portal
- Passive buzzer alarm for threshold violations
- OLED display (SSD1306) for status, temperature, WiFi info
- Protocol v2: persistent LDID, boot messages, periodic heartbeats
- OTA update support via ArduinoOTA

---

<!-- Example release:
## [0.1.0] - YYYY-MM-DD

### Added
- DS18B20 temperature reading (#1)
- HTTP push to Firebase (#2)
- Automatic Wi-Fi reconnection (#3)
-->
