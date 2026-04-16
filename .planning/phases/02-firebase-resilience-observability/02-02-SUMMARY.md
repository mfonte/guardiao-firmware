# Summary: Plan 02-02 — drawWifiReconnecting + WiFi Status Routing

**Phase:** 02-firebase-resilience-observability  
**Status:** COMPLETE (pending hardware checkpoint)  
**Build:** SUCCESS — RAM 49.3% (40352/81920 bytes), Flash 67.5%

## What was built

### src/drawOled.h
- `drawWifiReconnecting()` — modal screen shown when WiFi is down:
  - `display.clear()` + centered alignment
  - "Sem WiFi" in ArialMT_Plain_16 at y=10
  - "Reconectando..." in ArialMT_Plain_10 at y=38
  - `display.display()`
  - No progress bar (per D-19), no parameters, non-blocking

### src/main.cpp
- `loop()` display block — replaced unconditional `ui.update()` with WiFi-conditional routing:
  ```cpp
  else if (displayMode == DMODE_NORMAL) {
    if (WiFi.status() == WL_CONNECTED) {
      ui.update();
    } else {
      drawWifiReconnecting();
    }
  }
  ```
- Automatic recovery when WiFi reconnects (no explicit restore needed, per D-20)
- `overlayStatusBar()` (existing) continues to show icon_wifi_off — complementary behavior

## Requirements addressed
- OBS-02: WiFi disconnections are now visible on the OLED display

## Key decisions applied
- D-18: WiFi routing in DMODE_NORMAL branch of loop()
- D-19: No progress bar in drawWifiReconnecting()
- D-20: Automatic recovery without explicit restore flag

## Checkpoint
Human verification in hardware still required:
1. `pio run -e nodemcuv2 --target upload`
2. Disconnect WiFi → OLED should show "Sem WiFi" / "Reconectando..."
3. Reconnect WiFi → OLED should return to normal frames automatically
