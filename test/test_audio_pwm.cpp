#include "audio_outputs.h"
#include "config.h"
#include <Arduino.h>
#include <driver/ledc.h>
#include <cassert>
#include <vector>

HardwareSerial *HardwareSerial::ports[3] = {};
HardwareSerial Serial(0);
uint32_t testMillis = 0;
hw_timer_t hardware;
void (*interrupt)() = nullptr;
bool enabled = false, advance = true, timerAvailable = true;
int ledcResult = 0;
std::vector<uint8_t> output;
extern "C" void vvvc_pwm_duty(uint8_t value) { output.push_back(value); }
int ledc_timer_config(const ledc_timer_config_t *p) {
  assert(p->freq_hz == 156250 && p->duty_resolution == 8);
  return ledcResult;
}
int ledc_channel_config(const ledc_channel_config_t *p) {
  assert(p->gpio_num == config::audioPwmPin && p->duty == 128);
  return 0;
}
hw_timer_t *timerBegin(int n, int div, bool up) {
  assert(n == 0 && div == 2 && up);
  return timerAvailable ? &hardware : nullptr;
}
void timerAttachInterrupt(hw_timer_t *, void (*fn)(), bool edge) {
  assert(!edge); interrupt = fn;
}
void timerAlarmWrite(hw_timer_t *, uint32_t ticks, bool repeat) {
  assert(ticks == 907 && repeat);
}
void timerAlarmEnable(hw_timer_t *) { enabled = true; }
void timerAlarmDisable(hw_timer_t *) { enabled = false; }
void timerEnd(hw_timer_t *) { enabled = false; }
void delay(uint32_t ms) {
  testMillis += ms;
  if (enabled && advance)
    for (uint32_t i = 0; i < ms * 44; ++i) interrupt();
}
int main() {
  assert(!pwm_audio::write(nullptr, 0));
  ledcResult = -1;
  assert(!pwm_audio::begin());
  ledcResult = 0; timerAvailable = false;
  assert(!pwm_audio::begin());
  timerAvailable = true;
  assert(pwm_audio::begin() && enabled);
  assert(pwm_audio::begin());
  interrupt(); assert(output.back() == 128); // Underflow is silence.
  output.clear();
  int16_t input[] = {-32768, -16384, -1, 0, 1, 16384, 32767};
  assert(pwm_audio::write(input, 7));
  assert(pwm_audio::finish());
  uint8_t expected[] = {0, 64, 127, 128, 128, 192, 255};
  for (size_t i = 0; i < 7; ++i) assert(output[i] == expected[i]);
  for (size_t i = 7; i < output.size(); ++i) assert(output[i] == 128);
  // Multiple wraps, a full queue and producer backpressure preserve every sample.
  std::vector<int16_t> longInput(20000);
  for (size_t i = 0; i < longInput.size(); ++i) longInput[i] = static_cast<int16_t>(i * 211);
  output.clear();
  assert(pwm_audio::write(longInput.data(), longInput.size()));
  assert(pwm_audio::finish());
  for (size_t i = 0; i < longInput.size(); ++i)
    assert(output[i] == (static_cast<int32_t>(longInput[i]) + 32768) / 256);
  assert(pwm_audio::write(input, 7));
  pwm_audio::silence();
  output.clear();
  for (int i = 0; i < 10; ++i) interrupt();
  for (auto value : output) assert(value == 128);
  // Failed interrupt never hangs a serial command indefinitely.
  advance = false;
  assert(pwm_audio::write(input, 7));
  assert(!pwm_audio::finish() && !enabled);
  assert(!pwm_audio::write(input, 7));
  assert(pwm_audio::begin());
  assert(!pwm_audio::write(longInput.data(), longInput.size()) && !enabled);
}
