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

struct PendingReading
{
  uint32_t timestamp;
  float temperature;
}; // 8 bytes — EEPROM.put()/get() safe

PendingReading pendingQueue[PENDING_QUEUE_MAXLEN];
uint8_t pendingQueueLen = 0;

void loadPendingQueue()
{
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
    Serial.printf("[Queue] Loaded %u pending reading(s) from EEPROM\n", pendingQueueLen);
  }
  else
  {
    pendingQueueLen = 0;
    Serial.println("[Queue] EEPROM virgin or invalid — starting empty queue");
  }
}

void enqueuePendingReading(float temp, uint32_t ts)
{
  if (pendingQueueLen >= PENDING_QUEUE_MAXLEN)
  {
    // Ring buffer: descartar a entrada mais antiga
    Serial.printf("[Queue] Full, dropping oldest (ts=%u)\n", pendingQueue[0].timestamp);
    memmove(pendingQueue, pendingQueue + 1, (PENDING_QUEUE_MAXLEN - 1) * sizeof(PendingReading));
    pendingQueueLen = PENDING_QUEUE_MAXLEN - 1;
  }
  pendingQueue[pendingQueueLen].temperature = temp;
  pendingQueue[pendingQueueLen].timestamp = ts;
  pendingQueueLen++;
  // Persistir na EEPROM
  EEPROM.write(PENDING_QUEUE_ADDR, PENDING_QUEUE_MAGIC);
  EEPROM.write(PENDING_QUEUE_ADDR + 1, pendingQueueLen);
  for (uint8_t i = 0; i < pendingQueueLen; i++)
  {
    EEPROM.put(PENDING_QUEUE_ADDR + 2 + i * sizeof(PendingReading), pendingQueue[i]);
  }
  EEPROM.commit();
  Serial.printf("[Queue] Enqueued ts=%u temp=%.2f (queue len=%u)\n", ts, temp, pendingQueueLen);
}

#ifndef NATIVE_TEST
void drainPendingReadings()
{
  if (pendingQueueLen == 0)
    return;

  // Batch all pending entries into a single Firebase JSON update
  FirebaseJson drainJson;
  char drainTempPath[48];
  char drainTsPath[48];

  for (uint8_t i = 0; i < pendingQueueLen; i++)
  {
    snprintf(drainTempPath, sizeof(drainTempPath), "/datalogger/%lu/temperature", (unsigned long)pendingQueue[i].timestamp);
    snprintf(drainTsPath, sizeof(drainTsPath), "/datalogger/%lu/timestamp", (unsigned long)pendingQueue[i].timestamp);
    drainJson.set(drainTempPath, pendingQueue[i].temperature);
    drainJson.set(drainTsPath, (int)pendingQueue[i].timestamp);
  }

  uint8_t toSend = pendingQueueLen;

  if (Firebase.RTDB.updateNode(&fbdo, databasePath.c_str(), &drainJson))
  {
    // All sent — clear queue and EEPROM
    pendingQueueLen = 0;
    EEPROM.write(PENDING_QUEUE_ADDR, 0x00);
    EEPROM.commit();
    Serial.printf("[Queue] Drained all %u pending reading(s)\n", toSend);
  }
  else
  {
    Serial.printf("[Queue] Batch drain failed: %s — will retry next cycle\n", fbdo.errorReason().c_str());
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
