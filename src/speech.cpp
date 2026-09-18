#include "speech.h"
#include "audio.h"
#include "vic_serial.h"
#include <Arduino.h>
#include <votrax_reciter.h>
#include <votrax_synth.h>

namespace speech {
namespace {
votrax::Sc01ApproxSynth synth;
bool playing = false;
bool cancelled = false;
char tokens[votrax::SC01_TEXT_BUFFER_SIZE];

bool playQueuedSpeech() {
  int16_t pcm[32];
  playing = true;
  cancelled = false;
  while (synth.busy()) {
    vic_serial::poll();
    if (cancelled)
      break;
    synth.render(pcm, 32);
    if (!audio::write(pcm, 32)) {
      cancelled = true;
      synth.reset();
      break;
    }
    delay(0);
  }
  bool complete = !cancelled && audio::finish();
  playing = false;
  return complete;
}
} // namespace

bool speakPhonemes(const char *phonemes, HardwareSerial *reply) {
  if (!reply)
    reply = &Serial;
  if (!phonemes || !*phonemes || playing) {
    reply->printf("ERROR: empty phoneme input or speech busy.\r\n");
    return false;
  }
  // Validate the whole phrase before playing; a typo must not silently disappear.
  std::string token;
  for (const char *p = phonemes;; ++p) {
    if (*p && !isspace(static_cast<unsigned char>(*p))) {
      token += *p;
    } else if (!token.empty()) {
      if (!votrax::Sc01ApproxSynth::lookupPhone(token)) {
        reply->printf("ERROR: unknown SC-01 token: %s\r\n", token.c_str());
        return false;
      }
      token.clear();
    }
    if (!*p) {
      break;
    }
  }

  synth.begin();
  synth.say(phonemes);
  bool complete = playQueuedSpeech();
  if (!complete)
    reply->printf("ERROR: speech interrupted or audio unavailable.\r\n");
  return complete;
}

bool speakPacked(const uint8_t *phones, size_t count) {
  if (!phones || !count || playing)
    return false;
  synth.begin();
  for (size_t i = 0; i < count; ++i)
    synth.enqueuePackedPhone(phones[i]);
  return playQueuedSpeech();
}

bool speakText(const char *text, HardwareSerial *reply) {
  if (!reply)
    reply = &Serial;
  if (!votrax::textToSc01(text, tokens, sizeof(tokens))) {
    reply->printf("ERROR: English conversion failed (maximum 253 input bytes).\r\n");
    return false;
  }
  return speakPhonemes(tokens, reply);
}

void stop() {
  cancelled = true;
  synth.reset();
  audio::silence();
}

bool active() { return playing; }
} // namespace speech
