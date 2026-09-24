#include "audio.h"
#include "audio_outputs.h"
#include <Arduino.h>
#include <cassert>
#include <vector>

HardwareSerial *HardwareSerial::ports[3] = {};
HardwareSerial Serial(0);
bool pwmOk, codecOk, pwmWriteOk = true, codecWriteOk = true;
int pwmStops = 0, codecStops = 0, pwmDrains = 0, codecDrains = 0;
std::vector<int16_t> pwm, codec;
namespace pwm_audio {
bool begin() { return pwmOk; }
bool write(const int16_t *p, size_t n) {
  assert(n <= 64);
  if (pwmWriteOk) pwm.insert(pwm.end(), p, p+n);
  return pwmWriteOk;
}
bool finish() { ++pwmDrains; return true; }
void silence() { ++pwmStops; }
}
namespace codec_audio {
bool begin() { return codecOk; }
bool write(const int16_t *p, size_t n) {
  assert(n <= 64);
  if (codecWriteOk) codec.insert(codec.end(), p, p+n);
  return codecWriteOk;
}
bool finish() { ++codecDrains; return true; }
void silence() { ++codecStops; }
}
int main(int argc, char **argv) {
  assert(argc == 2);
  const std::string mode = argv[1];
  pwmOk = mode != "none" && mode != "codec";
  codecOk = mode != "none" && mode != "pwm";
  assert(audio::begin() == (pwmOk || codecOk));
  if (mode == "none") {
    assert(!audio::test() && !audio::finish());
    return 0;
  }
  assert(audio::test());
  const auto &values = pwmOk ? pwm : codec;
  assert(values.size() == 44100 * 7 / 10);
  if (pwmOk && codecOk) assert(pwm == codec);
  for (size_t i = 13230; i < 17640; ++i) assert(values[i] == 0);
  for (int tone = 0; tone < 2; ++tone) {
    int rising = 0;
    size_t start = tone * 17640;
    for (size_t i = start; i < start + 13230; ++i) {
      assert(values[i] == 5000 || values[i] == -5000);
      if (i > start && values[i] > 0 && values[i-1] < 0) ++rising;
    }
    assert(rising >= (tone ? 131 : 263) && rising <= (tone ? 133 : 265));
  }
  assert(pwmDrains == (pwmOk ? 1 : 0));
  assert(codecDrains == (codecOk ? 1 : 0));
  pwm.clear(); codec.clear();
  if (mode == "fail-codec") codecWriteOk = false;
  if (mode == "fail-pwm") pwmWriteOk = false;
  std::vector<int16_t> input(513);
  for (size_t i = 0; i < input.size(); ++i) input[i] = static_cast<int16_t>(i * 127);
  assert(audio::write(input.data(), input.size()));
  if (pwmOk && pwmWriteOk) assert(pwm == input);
  if (codecOk && codecWriteOk) assert(codec == input);
  audio::silence();
  assert(pwmStops == (pwmOk ? 1 : 0));
  assert(codecStops == (codecOk ? 1 : 0));
}
