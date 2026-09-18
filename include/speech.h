#pragma once

#include <stddef.h>
#include <stdint.h>

class HardwareSerial;

namespace speech {
bool speakText(const char *text, HardwareSerial *reply = nullptr);
bool speakPhonemes(const char *phonemes, HardwareSerial *reply = nullptr);
bool speakPacked(const uint8_t *phones, size_t count);
void stop();
bool active();
} // namespace speech
