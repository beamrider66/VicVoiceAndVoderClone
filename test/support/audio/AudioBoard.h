#pragma once

#include <Arduino.h>

// Host boundary for the actual audio.cpp; the ESP32 build uses the real library.
namespace audio_driver {
enum class PinFunction { CODEC, PA };
enum class PinLogic { Output };
enum class AudioDriverLogLevel { Warning };
enum {
  ADC_INPUT_LINE1 = 1,
  DAC_OUTPUT_ALL = 3,
  BIT_LENGTH_16BITS = 16,
  RATE_44K = 44100,
  CHANNELS2 = 2,
  I2S_NORMAL = 0,
  MODE_SLAVE = 0
};
struct CodecConfig {
  int input_device = 0, output_device = 0;
  bool sd_active = true;
  struct {
    int bits = 0, rate = 0, channels = 0, fmt = 0, mode = 0;
  } i2s;
};
struct DriverPins {
  int scl = -1, sda = -1, mclk = -1, bclk = -1, ws = -1;
  int dataOut = -1, dataIn = -1, pa = -1;
  void addI2C(PinFunction, int clock, int data) {
    scl = clock;
    sda = data;
  }
  void addI2S(PinFunction, int master, int bit, int word, int out, int in) {
    mclk = master;
    bclk = bit;
    ws = word;
    dataOut = out;
    dataIn = in;
  }
  void addPin(PinFunction, int pin, PinLogic) { pa = pin; }
};
struct CodecState {
  bool beginOk = true, outputOk = true, muted = true, powered = false;
  int volume = 0;
  CodecConfig config;
  DriverPins pins;
};
extern CodecState testCodec;
extern int AudioDriverES8388;
struct AudioBoard {
  AudioBoard(int &, DriverPins &pins) : pins_(pins) {}
  bool begin(CodecConfig config) {
    testCodec.config = config;
    testCodec.pins = pins_;
    return testCodec.beginOk;
  }
  bool setMute(bool enabled) {
    testCodec.muted = enabled;
    return testCodec.outputOk;
  }
  bool setPAPower(bool enabled) {
    testCodec.powered = enabled;
    return testCodec.outputOk;
  }
  bool setVolume(int volume) {
    testCodec.volume = volume;
    return testCodec.outputOk;
  }
  DriverPins &pins_;
};
struct Logger {
  void begin(HardwareSerial &, AudioDriverLogLevel) {}
};
extern Logger AudioDriverLogger;
} // namespace audio_driver
