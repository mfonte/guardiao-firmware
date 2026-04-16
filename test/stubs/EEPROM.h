#pragma once
#include <cstdint>
#include <cstring>

class EEPROMClass
{
public:
  uint8_t _data[1024];

  EEPROMClass() { memset(_data, 0xFF, sizeof(_data)); }

  void begin(int) {}
  uint8_t read(int addr) { return _data[addr]; }
  void write(int addr, uint8_t val) { _data[addr] = val; }

  template <typename T>
  void put(int addr, const T &val)
  {
    memcpy(&_data[addr], &val, sizeof(T));
  }

  template <typename T>
  void get(int addr, T &val)
  {
    memcpy(&val, &_data[addr], sizeof(T));
  }

  void commit() {}
  void clear() { memset(_data, 0xFF, sizeof(_data)); }
};

extern EEPROMClass EEPROM;
