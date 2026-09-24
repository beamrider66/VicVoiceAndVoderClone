#include "audio.h"
#include "audio_outputs.h"
#include "config.h"
#include <Arduino.h>

namespace audio {
namespace {
constexpr size_t blockFrames = 64;
bool pwmReady = false;
bool codecReady = false;

bool zeros(size_t frames) {
  const int16_t pcm[blockFrames] = {};
  while (frames) {
    size_t block = frames > blockFrames ? blockFrames : frames;
    if (!write(pcm, block))
      return false;
    frames -= block;
  }
  return true;
}

bool playTone(uint32_t frequency, uint32_t durationMs) {
  int16_t pcm[blockFrames];
  uint32_t phase = 0;
  const uint32_t step = (static_cast<uint64_t>(frequency) << 32) / config::audioSampleRate;
  size_t remaining = config::audioSampleRate * durationMs / 1000;
  while (remaining) {
    size_t block = remaining > blockFrames ? blockFrames : remaining;
    for (size_t i = 0; i < block; ++i) {
      pcm[i] = (phase & 0x80000000UL) ? 5000 : -5000;
      phase += step;
    }
    if (!write(pcm, block))
      return false;
    remaining -= block;
  }
  return true;
}
} // namespace

bool begin() {
  if (!pwmReady)
    pwmReady = pwm_audio::begin();
  if (!codecReady)
    codecReady = codec_audio::begin();
  return pwmReady || codecReady;
}

bool write(const int16_t *mono, size_t frames) {
  if (!pwmReady && !codecReady)
    return false;
  // Bound each enqueue to avoid starving either independently clocked output.
  while (frames) {
    const size_t block = frames > blockFrames ? blockFrames : frames;
    if (pwmReady && !pwm_audio::write(mono, block)) {
      pwm_audio::silence();
      pwmReady = false;
      Serial.println("WARNING: GPIO audio failed; remaining output will continue.");
    }
    if (codecReady && !codec_audio::write(mono, block)) {
      codec_audio::silence();
      codecReady = false;
      Serial.println("WARNING: codec audio failed; GPIO output will continue if available.");
    }
    if (!pwmReady && !codecReady)
      return false;
    mono += block;
    frames -= block;
  }
  return true;
}

bool finish() {
  // Drain both outputs so the last phoneme is never cut off.
  if (codecReady && !codec_audio::finish())
    codecReady = false;
  if (pwmReady && !pwm_audio::finish())
    pwmReady = false;
  return pwmReady || codecReady;
}

void silence() {
  if (pwmReady)
    pwm_audio::silence();
  if (codecReady)
    codec_audio::silence();
}

bool test() {
  return playTone(880, 300) && zeros(config::audioSampleRate / 10) && playTone(440, 300) &&
         finish();
}
} // namespace audio
