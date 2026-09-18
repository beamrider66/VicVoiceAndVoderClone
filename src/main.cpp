#include <Arduino.h>
#include <votrax_reciter.h>

#include "audio.h"
#include "config.h"
#include "line_buffer.h"
#include "phrase_bank.h"
#include "serial_commands.h"
#include "speech.h"
#include "vic_serial.h"

namespace {
LineBuffer<config::usbLineSize> usbLine;
char compactTokens[votrax::SC01_TEXT_BUFFER_SIZE];

void processLine(const char *input) {
  String line(input);
  line.trim();
  if (line.isEmpty()) {
    return;
  }
  if (serial_commands::handle(line.c_str(), Serial))
    return;
  size_t prefix = votrax::phonemePrefixLength(line.c_str());
  if (prefix) {
    if (votrax::typeNTalkPhonemesToSc01(line.c_str() + prefix, compactTokens,
                                        sizeof(compactTokens))) {
      speech::speakPhonemes(compactTokens);
    } else {
      Serial.println("ERROR: invalid compact phoneme input.");
    }
  } else {
    speech::speakText(line.c_str());
  }
}
} // namespace

void setup() {
  phrase_bank::clear();
  Serial.setRxBufferSize(2048);
  Serial.begin(config::usbBaud);
  bool audioReady = audio::begin();
  vic_serial::begin();
  delay(250);
  if (audioReady) {
    Serial.println("Playing startup audio test...");
    audioReady = audio::test();
  }
  Serial.println();
  serial_commands::printHelp(Serial);
  Serial.println(audioReady ? "Ready. Send -demo to check the audio and speech."
                            : "Audio unavailable. Check the ES8388 board and reset.");
}

void loop() {
  vic_serial::poll();
  while (Serial.available() > 0) {
    auto result = usbLine.append(static_cast<char>(Serial.read()));
    if (result == decltype(usbLine)::Result::Ready) {
      processLine(usbLine.line());
    } else if (result == decltype(usbLine)::Result::Overflow) {
      Serial.println("ERROR: USB line too long; discarded (maximum 253 bytes).");
    }
    vic_serial::poll();
  }
  delay(1);
}
