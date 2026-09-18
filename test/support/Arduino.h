#pragma once

// Minimal host adapter for exercising the real firmware's serial paths.
#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

class String {
public:
  String() = default;
  String(const char *text) : value_(text) {}
  String(std::string text) : value_(std::move(text)) {}
  size_t length() const { return value_.length(); }
  bool isEmpty() const { return value_.empty(); }
  void reserve(size_t size) { value_.reserve(size); }
  const char *c_str() const { return value_.c_str(); }
  char operator[](size_t index) const { return value_[index]; }
  String &operator+=(char c) {
    value_ += c;
    return *this;
  }
  bool operator==(const char *text) const { return value_ == text; }
  bool startsWith(const char *text) const { return value_.find(text) == 0; }
  void trim() {
    size_t start = value_.find_first_not_of(" \t\r\n");
    value_ = start == std::string::npos
                 ? ""
                 : value_.substr(start, value_.find_last_not_of(" \t\r\n") - start + 1);
  }
  void toLowerCase() {
    for (char &c : value_)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  bool equalsIgnoreCase(const char *text) const {
    String a = *this, b(text);
    a.toLowerCase();
    b.toLowerCase();
    return a.value_ == b.value_;
  }
  int lastIndexOf(char c, unsigned int from) const {
    size_t index = value_.rfind(c, from);
    return index == std::string::npos ? -1 : static_cast<int>(index);
  }
  String substring(unsigned int start, unsigned int end) const {
    return String(value_.substr(start, end - start));
  }
  void remove(size_t start) { value_.erase(start); }

private:
  std::string value_;
};

class HardwareSerial {
public:
  static HardwareSerial *ports[3];
  explicit HardwareSerial(int port) { ports[port] = this; }
  std::deque<uint8_t> rx;
  std::string tx;
  uint32_t baud = 0;
  uint32_t lastFlushBaud = 0;
  size_t rxCapacity = 0;
  void setRxBufferSize(size_t size) { rxCapacity = size; }
  void begin(uint32_t speed, int = 0, int = -1, int = -1) { baud = speed; }
  void updateBaudRate(uint32_t speed) { baud = speed; }
  void flush() { lastFlushBaud = baud; }
  int available() const { return static_cast<int>(rx.size()); }
  int read() {
    if (rx.empty())
      return -1;
    int c = rx.front();
    rx.pop_front();
    return c;
  }
  size_t write(uint8_t c) {
    tx += static_cast<char>(c);
    return 1;
  }
  size_t write(const uint8_t *bytes, size_t count) {
    tx.append(reinterpret_cast<const char *>(bytes), count);
    return count;
  }
  void println(const char *text = "") {
    tx += text;
    tx += '\n';
  }
  void printf(const char *format, ...) {
    char buffer[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    tx += buffer;
  }
  void feed(const std::string &bytes) {
    for (uint8_t c : bytes)
      rx.push_back(c);
  }
};

extern HardwareSerial Serial;
extern uint32_t testMillis;
inline uint32_t millis() { return testMillis; }
inline void delay(uint32_t ms) { testMillis += ms; }
constexpr int SERIAL_8N1 = 0;
struct TestEsp {
  int restarts = 0;
  void restart() { ++restarts; }
};
extern TestEsp ESP;
