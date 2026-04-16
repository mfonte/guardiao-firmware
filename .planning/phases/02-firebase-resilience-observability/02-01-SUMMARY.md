# Summary: Plan 02-01 — EEPROM Queue + Firebase Backoff + Config Fetch Timer

**Phase:** 02-firebase-resilience-observability  
**Status:** COMPLETE  
**Build:** SUCCESS — RAM 49.2% (40320/81920 bytes), Flash 67.5%

## What was built

### src/config.h
- `PENDING_QUEUE_ADDR 380`, `PENDING_QUEUE_MAGIC 0xA5`, `PENDING_QUEUE_MAXLEN 16` (defines)
- `CONFIG_FETCH_INTERVAL 3600000L` (60 min config fetch)
- `firebaseFailCount`, `firebaseSkipRemaining` (uint8_t backoff state)
- `configFetchPrevMillis` (unsigned long timer for config fetch)

### src/EEPROMFunction.h
- `struct PendingReading { uint32_t timestamp; float temperature; }` (8 bytes)
- `PendingReading pendingQueue[16]`, `uint8_t pendingQueueLen`
- `loadPendingQueue()` — reads queue from EEPROM on boot (magic byte validation)
- `enqueuePendingReading(float temp, uint32_t ts)` — ring buffer, EEPROM.commit()
- `drainOnePendingReading()` — sends oldest entry to RTDB, removes on success
- Added `extern FirebaseData fbdo; extern String databasePath;` for RTDB access
- Added `#include "config.h"` for PENDING_QUEUE_* defines

### src/main.cpp
- **Include order reordered**: Firebase_ESP_Client.h before EEPROMFunction.h (required for FirebaseData type)
- `sendDataToFireBase()` → changed to `bool`, removed `getDeviceConfigurations()` internal call, captures `writeOk = Firebase.RTDB.updateNode(...)`, on success: resets counters + calls `drainOnePendingReading()`, on failure: increments `firebaseFailCount`, sets `firebaseSkipRemaining = min(1 << failCount, 16)`, enqueues reading
- `callFirebase()` → backoff guard at top: if `firebaseSkipRemaining > 0`, decrements, enqueues current reading and returns early; safety cap: if `firebaseFailCount >= 5` → `drawResetDevice()` + `ESP.reset()`
- `setup()` → calls `loadPendingQueue()` immediately after `EEPROM.begin(EEPROM_SIZE)`
- `loop()` → adds `configFetchPrevMillis` timer block: calls `getDeviceConfigurations()` every 60 min (first call immediate via `|| configFetchPrevMillis == 0`)

## Requirements addressed
- REL-04: Exponential backoff on Firebase write failure (skip cycles, enqueue readings)
- REL-05: Persistent reading queue in EEPROM survives reboots; drained after successful writes

## Key decisions applied
- D-03: Backoff guard in callFirebase() (not sendDataToFireBase)
- D-04: Safety reset at firebaseFailCount >= 5
- D-09: loadPendingQueue() in setup()
- D-12: drainOnePendingReading() uses extern fbdo/databasePath
- D-15/D-16: getDeviceConfigurations() decoupled from send cycle, moved to 60 min loop timer
