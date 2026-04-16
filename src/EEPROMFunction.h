#pragma once
#ifndef NATIVE_TEST
#include <EEPROM.h>
#include "config.h"
#else
#include "EEPROM.h"
const int EEPROM_SIZE = 1024;
#define PENDING_QUEUE_ADDR    380
#define PENDING_QUEUE_MAGIC   0xA5
#define PENDING_QUEUE_MAXLEN  16
#endif

#ifndef NATIVE_TEST
const int EEPROM_SIZE = 1024; // MAX EEPROM SIZE
#endif

/**
 * Reads a null-terminated string previously written by writeStringToEEPROM().
 * The byte at addrOffset holds the string length; subsequent bytes hold the data.
 * Returns an empty String if the stored length is invalid.
 * @param addrOffset Starting EEPROM address.
 * @return Recovered string, or "" on error.
 */
String readStringFromEEPROM(int addrOffset)
{
  int newStrLen = EEPROM.read(addrOffset);
  if (newStrLen == 0xFF || newStrLen >= EEPROM_SIZE - addrOffset - 1)
  { // 0xFF = virgin EEPROM; also reject overflow
    return String("");
  }
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++)
  {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';

  return String(data);
}

/**
 * Writes a string to EEPROM starting at addrOffset.
 * Stores the length byte first, then each character.
 * Strings longer than the remaining EEPROM space are silently truncated.
 * Calls EEPROM.commit() automatically after writing.
 * @param addrOffset  Starting EEPROM address.
 * @param strToWrite  The string to persist.
 */
void writeStringToEEPROM(int addrOffset, const String &strToWrite)
{
  int len = strToWrite.length();
  if (len >= EEPROM_SIZE - addrOffset - 1)
  {                                     // Check if the string is too long
    len = EEPROM_SIZE - addrOffset - 2; // Truncate if necessary
  }
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++)
  {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
  EEPROM.commit();
}

// ---- Pending queue — fila persistente de leituras não enviadas ao Firebase ----

#ifndef NATIVE_TEST
// Forward declarations — definidos em main.cpp (incluído após Firebase_ESP_Client.h)
extern FirebaseData fbdo;
extern String databasePath;
#endif

#define PENDING_META_ADDR   806   // Addr 806–823: 2 header bytes + 16 source bytes
#define PENDING_META_MAGIC  0x5C

enum PendingReadingSource : uint8_t
{
  PENDING_SRC_UNKNOWN   = 0,
  PENDING_SRC_INTERVAL  = 1,
  PENDING_SRC_SCHEDULED = 2,
  PENDING_SRC_THRESHOLD = 3,
  PENDING_SRC_RECOVERY  = 4,
};

struct PendingReading
{
  uint32_t timestamp;
  float temperature;
}; // 8 bytes — EEPROM.put()/get() safe

PendingReading pendingQueue[PENDING_QUEUE_MAXLEN];
uint8_t pendingQueueSource[PENDING_QUEUE_MAXLEN];
uint8_t pendingQueueLen = 0;

const char *pendingSourceLabel(uint8_t source)
{
  switch (source)
  {
    case PENDING_SRC_INTERVAL:  return "interval";
    case PENDING_SRC_SCHEDULED: return "scheduled";
    case PENDING_SRC_THRESHOLD: return "threshold";
    case PENDING_SRC_RECOVERY:  return "recovery";
    default:                    return "unknown";
  }
}

bool isScheduledPendingSource(uint8_t source)
{
  return source == PENDING_SRC_SCHEDULED;
}

void persistPendingQueue()
{
  EEPROM.write(PENDING_QUEUE_ADDR, PENDING_QUEUE_MAGIC);
  EEPROM.write(PENDING_QUEUE_ADDR + 1, pendingQueueLen);
  for (uint8_t i = 0; i < pendingQueueLen; i++)
  {
    EEPROM.put(PENDING_QUEUE_ADDR + 2 + i * sizeof(PendingReading), pendingQueue[i]);
  }

  EEPROM.write(PENDING_META_ADDR, PENDING_META_MAGIC);
  EEPROM.write(PENDING_META_ADDR + 1, pendingQueueLen);
  for (uint8_t i = 0; i < pendingQueueLen; i++)
  {
    EEPROM.write(PENDING_META_ADDR + 2 + i, pendingQueueSource[i]);
  }

  EEPROM.commit();
}

void clearPendingQueueStorage()
{
  pendingQueueLen = 0;
  memset(pendingQueueSource, 0, sizeof(pendingQueueSource));
  EEPROM.write(PENDING_QUEUE_ADDR, 0x00);
  EEPROM.write(PENDING_META_ADDR, 0x00);
  EEPROM.commit();
}

void removePendingAt(uint8_t index)
{
  if (index >= pendingQueueLen)
    return;

  if (index + 1 < pendingQueueLen)
  {
    memmove(pendingQueue + index,
            pendingQueue + index + 1,
            (pendingQueueLen - index - 1) * sizeof(PendingReading));
    memmove(pendingQueueSource + index,
            pendingQueueSource + index + 1,
            (pendingQueueLen - index - 1) * sizeof(uint8_t));
  }

  pendingQueueLen--;
}

void loadPendingQueue()
{
  memset(pendingQueueSource, 0, sizeof(pendingQueueSource));

  uint8_t magic = EEPROM.read(PENDING_QUEUE_ADDR);
  if (magic == PENDING_QUEUE_MAGIC)
  {
    uint8_t count = 0;
    EEPROM.get(PENDING_QUEUE_ADDR + 1, count);
    pendingQueueLen = min(count, (uint8_t)PENDING_QUEUE_MAXLEN);
    for (uint8_t i = 0; i < pendingQueueLen; i++)
    {
      EEPROM.get(PENDING_QUEUE_ADDR + 2 + i * sizeof(PendingReading), pendingQueue[i]);
    }

    uint8_t metaMagic = EEPROM.read(PENDING_META_ADDR);
    uint8_t metaCount = EEPROM.read(PENDING_META_ADDR + 1);
    if (metaMagic == PENDING_META_MAGIC)
    {
      uint8_t taggedCount = min(metaCount, pendingQueueLen);
      for (uint8_t i = 0; i < taggedCount; i++)
      {
        pendingQueueSource[i] = EEPROM.read(PENDING_META_ADDR + 2 + i);
      }
      for (uint8_t i = taggedCount; i < pendingQueueLen; i++)
      {
        pendingQueueSource[i] = PENDING_SRC_INTERVAL;
      }
      Serial.printf("[Queue] Loaded %u pending reading(s) from EEPROM (%u source-tagged)\n",
                    pendingQueueLen, taggedCount);
    }
    else
    {
      for (uint8_t i = 0; i < pendingQueueLen; i++)
      {
        pendingQueueSource[i] = PENDING_SRC_INTERVAL;
      }
      Serial.printf("[Queue] Loaded %u pending reading(s) from EEPROM (legacy format, source=interval)\n",
                    pendingQueueLen);
    }
  }
  else
  {
    pendingQueueLen = 0;
    Serial.println("[Queue] EEPROM virgin or invalid — starting empty queue");
  }
}

void enqueuePendingReading(float temp, uint32_t ts, uint8_t source = PENDING_SRC_INTERVAL)
{
  if (pendingQueueLen >= PENDING_QUEUE_MAXLEN)
  {
    int dropIndex = -1;

    if (isScheduledPendingSource(source))
    {
      for (uint8_t i = 0; i < pendingQueueLen; i++)
      {
        if (!isScheduledPendingSource(pendingQueueSource[i]))
        {
          dropIndex = i;
          break;
        }
      }
      if (dropIndex < 0)
      {
        dropIndex = 0; // queue already contains only scheduled items: keep the freshest ones
      }
    }
    else
    {
      for (uint8_t i = 0; i < pendingQueueLen; i++)
      {
        if (!isScheduledPendingSource(pendingQueueSource[i]))
        {
          dropIndex = i;
          break;
        }
      }

      if (dropIndex < 0)
      {
        Serial.printf("[Queue] Full of scheduled readings, dropping new %s ts=%u\n",
                      pendingSourceLabel(source), (unsigned)ts);
        return;
      }
    }

    Serial.printf("[Queue] Full, dropping %s ts=%u to enqueue %s ts=%u\n",
                  pendingSourceLabel(pendingQueueSource[dropIndex]),
                  (unsigned)pendingQueue[dropIndex].timestamp,
                  pendingSourceLabel(source),
                  (unsigned)ts);
    removePendingAt((uint8_t)dropIndex);
  }

  pendingQueue[pendingQueueLen].temperature = temp;
  pendingQueue[pendingQueueLen].timestamp = ts;
  pendingQueueSource[pendingQueueLen] = source;
  pendingQueueLen++;
  persistPendingQueue();
  Serial.printf("[Queue] Enqueued %s ts=%u temp=%.2f (queue len=%u)\n",
                pendingSourceLabel(source), (unsigned)ts, temp, pendingQueueLen);
}

#ifndef NATIVE_TEST
void drainPendingReadings()
{
  if (pendingQueueLen == 0)
    return;

  // PATCH directly under /datalogger to avoid replacing the entire device root.
  // This preserves historical entries that are not in the pending queue.
  FirebaseJson drainJson;
  char drainTempPath[48];
  char drainTsPath[48];
  uint8_t scheduledCount = 0;

  for (uint8_t i = 0; i < pendingQueueLen; i++)
  {
    if (isScheduledPendingSource(pendingQueueSource[i]))
    {
      scheduledCount++;
    }
    snprintf(drainTempPath, sizeof(drainTempPath), "/%lu/temperature", (unsigned long)pendingQueue[i].timestamp);
    snprintf(drainTsPath, sizeof(drainTsPath), "/%lu/timestamp", (unsigned long)pendingQueue[i].timestamp);
    drainJson.set(drainTempPath, pendingQueue[i].temperature);
    drainJson.set(drainTsPath, (int)pendingQueue[i].timestamp);
  }

  uint8_t toSend = pendingQueueLen;
  String dataloggerPath = databasePath + "/datalogger";

  if (Firebase.RTDB.updateNode(&fbdo, dataloggerPath.c_str(), &drainJson))
  {
    clearPendingQueueStorage();
    Serial.printf("[Queue] Drained all %u pending reading(s) (%u scheduled priority)\n",
                  toSend, scheduledCount);
  }
  else
  {
    Serial.printf("[Queue] Batch drain failed: %s - will retry next cycle\n", fbdo.errorReason().c_str());
  }
}
#endif

// ---- WiFi credential ring buffer — stores last 3 networks, slot 0 = primary ----
#define WIFI_CREDS_ADDR  510  // Addr 510–805: 2 header bytes + 3×98 data bytes
#define WIFI_CREDS_MAGIC 0xB3 // Distinguishes valid data from virgin EEPROM (0xFF)
#define WIFI_CRED_COUNT  3    // Maximum number of stored networks

struct WifiSlot {
  char ssid[33];     // up to 32-char SSID + null terminator
  char password[65]; // up to 64-char WPA2 password + null terminator
};                   // 98 bytes — EEPROM.put()/get() safe

#ifndef NATIVE_TEST
void loadWifiCredentials(WifiSlot slots[WIFI_CRED_COUNT], uint8_t &count)
{
  uint8_t magic = EEPROM.read(WIFI_CREDS_ADDR);
  if (magic == WIFI_CREDS_MAGIC) {
    count = min((uint8_t)WIFI_CRED_COUNT, EEPROM.read(WIFI_CREDS_ADDR + 1));
    for (uint8_t i = 0; i < count; i++) {
      EEPROM.get(WIFI_CREDS_ADDR + 2 + i * sizeof(WifiSlot), slots[i]);
    }
  } else {
    count = 0;
    for (uint8_t i = 0; i < WIFI_CRED_COUNT; i++) {
      slots[i].ssid[0] = '\0';
      slots[i].password[0] = '\0';
    }
  }
}

void saveWifiCredentials(WifiSlot slots[WIFI_CRED_COUNT], uint8_t count)
{
  EEPROM.write(WIFI_CREDS_ADDR, WIFI_CREDS_MAGIC);
  EEPROM.write(WIFI_CREDS_ADDR + 1, count);
  for (uint8_t i = 0; i < WIFI_CRED_COUNT; i++) {
    EEPROM.put(WIFI_CREDS_ADDR + 2 + i * sizeof(WifiSlot), slots[i]);
  }
  EEPROM.commit();
}

// Adds a network as slot 0. If the SSID already exists, refreshes its password
// and promotes it to slot 0. Oldest entry is dropped when the ring buffer is full.
void addWifiCredential(const char* ssid, const char* password)
{
  if (!ssid || ssid[0] == '\0') return;
  WifiSlot slots[WIFI_CRED_COUNT];
  uint8_t count = 0;
  loadWifiCredentials(slots, count);

  // Check if SSID already in list — update password and promote to slot 0
  for (uint8_t i = 0; i < count; i++) {
    if (strcmp(slots[i].ssid, ssid) == 0) {
      WifiSlot tmp = slots[i];
      strlcpy(tmp.password, password, sizeof(tmp.password));
      memmove(slots + 1, slots, i * sizeof(WifiSlot));
      slots[0] = tmp;
      saveWifiCredentials(slots, count);
      Serial.printf("[WiFi] Promoted '%s' to slot 0\n", ssid);
      return;
    }
  }

  // New SSID — insert at front, drop oldest if full
  memmove(slots + 1, slots, (WIFI_CRED_COUNT - 1) * sizeof(WifiSlot));
  strlcpy(slots[0].ssid, ssid, sizeof(slots[0].ssid));
  strlcpy(slots[0].password, password, sizeof(slots[0].password));
  count = min((uint8_t)(count + 1), (uint8_t)WIFI_CRED_COUNT);
  saveWifiCredentials(slots, count);
  Serial.printf("[WiFi] Saved '%s' to slot 0 (total=%u)\n", ssid, count);
}
#endif

// ---- WiFi credential ring buffer — stores last 3 networks, slot 0 = primary ----
