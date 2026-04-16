#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <cstdio>
#include <cstdarg>

#define min(a, b) ((a) < (b) ? (a) : (b))

class String
{
  std::string _buf;

public:
  String() : _buf() {}
  String(const char *s) : _buf(s ? s : "") {}
  String(const String &o) : _buf(o._buf) {}
  String(int n) : _buf(std::to_string(n)) {}

  String &operator=(const char *s)
  {
    _buf = s ? s : "";
    return *this;
  }
  String &operator=(const String &o)
  {
    _buf = o._buf;
    return *this;
  }

  bool operator==(const char *s) const { return _buf == s; }
  bool operator==(const String &o) const { return _buf == o._buf; }
  bool operator!=(const char *s) const { return _buf != s; }
  bool operator!=(const String &o) const { return _buf != o._buf; }

  int length() const { return (int)_buf.size(); }
  char operator[](int i) const { return _buf[i]; }
  const char *c_str() const { return _buf.c_str(); }
};

struct SerialStub
{
  void println(const char *) {}
  void println(const String &) {}
  void print(const char *) {}
  void flush() {}
  void end() {}
  void printf(const char *fmt, ...)
  {
    (void)fmt;
  }
};

extern SerialStub Serial;
