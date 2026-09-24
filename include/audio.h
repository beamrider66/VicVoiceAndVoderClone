#pragma once

#include <stddef.h>
#include <stdint.h>

namespace audio {
bool begin();
// Fan out the same mono PCM to always-on GPIO PWM and the optional ES8388.
// Samples retain the source project's nominal 44.1 kHz playback clock.
bool write(const int16_t *mono, size_t frames);
// Drain queued speech normally, or discard it immediately on BREAK.
bool finish();
void silence();
bool test();
} // namespace audio
