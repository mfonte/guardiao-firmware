#pragma once
#include <EEPROM.h>
#include "config.h"

const int EEPROM_SIZE = 512; // MAX EEPROM SIZE

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

// Forward declarations — definidos em main.cpp (incluído após Firebase_ESP_Client.h)
extern FirebaseData fbdo;
extern String databasePath;

struct PendingReading {
  uint32_t timestamp;
  float    temperature;
}; // 8 bytes — EEPROM.put()/get() safe

PendingReading pendingQueue[PENDING_QUEUE_MAXLEN];
uint8_t        pendingQueueLen = 0;

void loadPendingQueue() {
  uint8_t magic = EEPROM.read(PENDING_QUEUE_ADDR);
  if (magic == PENDING_QUEUE_MAGIC) {
    uint8_t count = 0;
    EEPROM.get(PENDING_QUEUE_ADDR + 1, count);
    pendingQueueLen = min(count, (uint8_t)PENDING_QUEUE_MAXLEN);
    for (uint8_t i = 0; i < pendingQueueLen; i++) {
      EEPROM.get(PENDING_QUEUE_ADDR + 2 + i * sizeof(PendingReading), pendingQueue[i]);
    }
    Serial.printf("[Queue] Loaded %u pending reading(s) from EEPROM\n", pendingQueueLen);
  } else {
    pendingQueueLen = 0;
    Serial.println("[Queue] EEPROM virgin or invalid — starting empty queue");
  }
}

void enqueuePendingReading(float temp, uint32_t ts) {
  if (pendingQueueLen >= PENDING_QUEUE_MAXLEN) {
    // Ring buffer: descartar a entrada mais antiga
    Serial.printf("[Queue] Full, dropping oldest (ts=%u)\n", pendingQueue[0].timestamp);
    memmove(pendingQueue, pendingQueue + 1, (PENDING_QUEUE_MAXLEN - 1) * sizeof(PendingReading));
    pendingQueueLen = PENDING_QUEUE_MAXLEN - 1;
  }
  pendingQueue[pendingQueueLen].temperature = temp;
  pendingQueue[pendingQueueLen].timestamp   = ts;
  pendingQueueLen++;
  // Persistir na EEPROM
  EEPROM.write(PENDING_QUEUE_ADDR, PENDING_QUEUE_MAGIC);
  EEPROM.write(PENDING_QUEUE_ADDR + 1, pendingQueueLen);
  for (uint8_t i = 0; i < pendingQueueLen; i++) {
    EEPROM.put(PENDING_QUEUE_ADDR + 2 + i * sizeof(PendingReading), pendingQueue[i]);
  }
  EEPROM.commit();
  Serial.printf("[Queue] Enqueued ts=%u temp=%.2f (queue len=%u)\n", ts, temp, pendingQueueLen);
}

void drainOnePendingReading() {
  if (pendingQueueLen == 0) return;

  // Enviar a entrada mais antiga (index 0)
  FirebaseJson drainJson;
  String drainPath = "/datalogger/" + String(pendingQueue[0].timestamp);
  drainJson.set((drainPath + "/temperature").c_str(), pendingQueue[0].temperature);
  drainJson.set((drainPath + "/timestamp").c_str(),   (int)pendingQueue[0].timestamp);

  if (Firebase.RTDB.updateNode(&fbdo, databasePath.c_str(), &drainJson)) {
    // Sucesso: remover da fila
    memmove(pendingQueue, pendingQueue + 1, (pendingQueueLen - 1) * sizeof(PendingReading));
    pendingQueueLen--;
    if (pendingQueueLen == 0) {
      // Fila vazia: invalidar magic byte
      EEPROM.write(PENDING_QUEUE_ADDR, 0x00);
    } else {
      EEPROM.write(PENDING_QUEUE_ADDR + 1, pendingQueueLen);
    }
    EEPROM.commit();
    Serial.printf("[Queue] Drained 1 pending reading (queue len=%u)\n", pendingQueueLen);
  } else {
    Serial.printf("[Queue] Drain failed: %s — will retry next cycle\n", fbdo.errorReason().c_str());
  }
}
