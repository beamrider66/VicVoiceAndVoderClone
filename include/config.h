#pragma once

#include <stddef.h>
#include <stdint.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <sdkconfig.h>
#endif

// The Audio-Kit wiring applies only to the original ESP32. Host tests use it too.
#ifndef VVVC_ES8388
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define VVVC_ES8388 0
#else
#define VVVC_ES8388 1
#endif
#endif
#ifndef VVVC_PWM_PIN
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
#define VVVC_PWM_PIN 4
#else
#define VVVC_PWM_PIN 22
#endif
#endif

namespace config {
// ES8388 Audio-Kit wiring from the working source project.
constexpr int audioVolumePercent = 48;
constexpr uint32_t audioSampleRate = 44100;
constexpr int audioPwmPin = VVVC_PWM_PIN;
constexpr uint32_t audioPwmCarrierHz = 156250;
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
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32S3)
constexpr int vicUart = 1;
constexpr int vicRxPin = 6;
constexpr int vicTxPin = 7;
#else
constexpr int vicUart = 2;
constexpr int vicRxPin = 18;
constexpr int vicTxPin = 5;
#endif
constexpr size_t vicRxBufferSize = 768;
constexpr uint32_t vicTimeoutMs = 4000;
constexpr size_t usbLineSize = 253;
constexpr size_t phraseSlotCount = 80;
constexpr size_t phrasePhoneLimit = 256;
} // namespace config
