#pragma once
#include <stddef.h>
#include <stdint.h>

namespace codec_audio {
bool begin();
bool write(const int16_t *mono, size_t frames);
bool finish();
void silence();
}
namespace pwm_audio {
bool begin();
bool write(const int16_t *mono, size_t frames);
bool finish();
void silence();
}
