#include <EEPROM.h>

const int EEPROM_SIZE = 512; // MAX EEPROM SIZE

String readStringFromEEPROM(int addrOffset) {
  int newStrLen = EEPROM.read(addrOffset);
  if (newStrLen < 0 || newStrLen >= EEPROM_SIZE - addrOffset - 1) { // Check for invalid length
    return String("");
  }
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++) {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';
  
  return String(data);
}

void writeStringToEEPROM(int addrOffset, const String &strToWrite) {
  int len = strToWrite.length();
  if (len >= EEPROM_SIZE - addrOffset - 1) { // Check if the string is too long
    len = EEPROM_SIZE - addrOffset - 2; // Truncate if necessary
  }
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
  }
  EEPROM.commit();
}