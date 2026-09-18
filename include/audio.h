#pragma once

#include <stddef.h>
#include <stdint.h>

namespace audio {
bool begin();
// Samples retain the source project's 44.1 kHz Audio-Kit playback clock.
bool write(const int16_t *mono, size_t frames);
// Drain queued speech normally, or discard it immediately on BREAK.
bool finish();
void silence();
bool test();
} // namespace audio
