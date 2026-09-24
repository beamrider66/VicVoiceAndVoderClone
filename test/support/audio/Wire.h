#pragma once
struct TestWire {
  bool present = true;
  bool begin(int, int) { return true; }
  void setTimeOut(int) {}
  void beginTransmission(int) {}
  int endTransmission() { return present ? 0 : 2; }
  void end() {}
};
extern TestWire Wire;
