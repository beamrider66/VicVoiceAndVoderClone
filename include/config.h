#pragma once

#include <stddef.h>
#include <stdint.h>

namespace config {
// ES8388 Audio-Kit wiring from the working source project.
constexpr int audioVolumePercent = 48;
constexpr uint32_t audioSampleRate = 44100;
constexpr int codecSclPin = 32;
constexpr int codecSdaPin = 33;
constexpr int audioMclkPin = 0;
constexpr int audioBclkPin = 27;
constexpr int audioLrclkPin = 25;
constexpr int audioDataPin = 26;
constexpr int speakerEnablePin = 21;
constexpr uint32_t usbBaud = 115200;
constexpr uint32_t vicBaud = 2400;
constexpr bool vicEcho = true;
constexpr int vicRxPin = 18;
constexpr int vicTxPin = 5;
constexpr size_t vicRxBufferSize = 768;
constexpr uint32_t vicTimeoutMs = 4000;
constexpr size_t usbLineSize = 253;
constexpr size_t phraseSlotCount = 80;
constexpr size_t phrasePhoneLimit = 256;
} // namespace config
