# Codebase Structure

**Analysis Date:** 2026-04-11

## Directory Layout

```
guardiao-firmware/
├── src/                         # Application source (PlatformIO compiles all here)
│   ├── main.cpp                 # Entry: setup(), loop(), Firebase, Protocol v2
│   ├── drawOled.h               # Display subsystem: 6 frames, overlay, buttons, alarm, buzzer
│   ├── EEPROMFunction.h         # Persistent string storage helpers
│   ├── OTAUpdate.h              # ArduinoOTA setup with OLED progress
│   ├── icons.h                  # 8x8 XBM bitmaps in PROGMEM
│   ├── config.example.h         # Configuration template (committed)
│   └── config.h                 # Device-local config (gitignored — copy from example)
├── lib/                         # Local library copies (override PlatformIO registry)
│   ├── esp8266-oled-ssd1306/    # SSD1306 Wire driver + OLEDDisplayUi
│   └── WiFiManager/             # Captive portal WiFi provisioning
├── include/
│   └── README                   # PlatformIO placeholder (unused)
├── test/
│   └── README                   # No tests exist
├── .planning/
│   └── codebase/                # GSD codebase map documents
├── platformio.ini               # Build configuration
├── .gitignore
├── README.md
├── CHANGELOG.md
├── CLAUDE.md                    # AI context document
└── MANUAL.md                    # End-user operation manual (6 screens)
```

## Directory Purposes

**`src/`:**
- All application code. PlatformIO automatically compiles everything here.
- `.h` files act as implementation modules (not just declarations)
- Key files: `main.cpp` (entry), `drawOled.h` (largest, ~490 lines), `config.h` (global state)

**`lib/`:**
- Vendored local library copies. PlatformIO picks these up before checking the registry.
- Contains: `esp8266-oled-ssd1306/`, `WiFiManager/`
- Do NOT also list these in `platformio.ini lib_deps` — they are used as-is from disk

**`include/`:**
- PlatformIO public include directory — currently unused; all headers live in `src/`

**`test/`:**
- PlatformIO test directory — currently empty (only `README`). No test runner configured.

## Key File Locations

**Entry Point:**
- `src/main.cpp`: `setup()` and `loop()`, all Firebase functions, Protocol v2 boot/heartbeat

**Display (largest module):**
- `src/drawOled.h`: All 6 frame callbacks, `overlayStatusBar()`, modal screens, button handler, alarm state machine, buzzer sequences, `initUIFramework()`

**Persistent Storage:**
- `src/EEPROMFunction.h`: `readStringFromEEPROM(int)`, `writeStringToEEPROM(int, const String&)`

**Icons:**
- `src/icons.h`: `ICON_SIZE=8` + 11 XBM icon arrays in PROGMEM

**Configuration:**
- `src/config.example.h`: Safe-to-commit template with all required defines and placeholders
- `src/config.h`: Local copy with real credentials (never commit — gitignored)

**OTA:**
- `src/OTAUpdate.h`: `OTAsetup()` — call once in `setup()` after WiFi connects

## Naming Conventions

**Files:**
- `camelCase.h` for header modules (`drawOled.h`, `EEPROMFunction.h`, `OTAUpdate.h`)
- `lowercase.cpp` for source files (`main.cpp`)
- `config.example.h` pattern for committed templates

**Functions:**
- `camelCase()` for all functions (`callFirebase()`, `readTemperature()`)
- Frame callbacks: `frame{Name}()` (`frameTemperature`, `frameClock`)
- Draw modal helpers: `draw{What}()` (`drawAlarmScreen`, `drawBootProgress`)
- Buzzer: `beep{Event}()` (`beepBoot`, `beepSuccess`, `beepError`)

**Variables:**
- `UPPER_SNAKE` for `#define` constants and EEPROM addresses
- `UPPER_SNAKE` for global runtime String/int config (`DEVICE_NAME`, `USER_EMAIL`)
- `camelCase` for local and state variables (`higherTemp`, `lastVccMs`)

**Icons:**
- `icon_{name}[]` snake_case in PROGMEM (`icon_wifi`, `icon_arrow_left`, `icon_battery_low`)

## Where to Add New Code

**New display frame:**
1. Add `void frameXxx(OLEDDisplay *d, OLEDDisplayUiState *state, int16_t x, int16_t y)` in `src/drawOled.h`
2. Add to `uiFrames[]` array and increment `uiFrameCount`
3. Add nav arrows: `d->drawXbm(2+x, 55+y, ICON_SIZE, ICON_SIZE, icon_arrow_left)` and right

**New icon:**
1. Add `static const uint8_t icon_{name}[] PROGMEM = {...};` to `src/icons.h`
2. Must be 8x8 pixels (XBM format: LSB-first, row-major, 8 bytes)

**New Firebase operation:**
1. Add function in `src/main.cpp` (after existing Firebase functions, before `loop()`)
2. Add forward declaration at top of `main.cpp` if called before definition

**New configuration constant:**
1. Add `#define NAME value` to both `src/config.example.h` (with placeholder) and `src/config.h` (with real value)

**New EEPROM slot:**
1. Add `const int NEW_ADDR = N;` to `src/config.h`
2. Existing layout: `EMAIL_ADDR=0` (100B), `PASS_ADDR=100` (100B), `DEVICE_NAME_ADDR=200` (100B), `LDID_ADDR=300` (100B)
3. Next available: 400 (112 bytes remaining before 512-byte limit)

## Special Directories

**`.planning/codebase/`:**
- Purpose: GSD codebase map documents for AI planning
- Generated: Yes (by `/gsd:map-codebase`)
- Committed: Yes

**`.pio/`** (PlatformIO build output — gitignored):
- Purpose: Compiled objects, toolchain packages, build cache
- Generated: Yes
- Committed: No

---

*Structure analysis: 2026-04-11*
