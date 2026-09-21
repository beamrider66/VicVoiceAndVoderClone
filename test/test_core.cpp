#include "english_reciter.h"
#include "line_buffer.h"
#include "phrase_bank.h"
#include "votrax_reciter.h"
#include "votrax_synth.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

static void testEnglish() {
  char phones[votrax::SC01_TEXT_BUFFER_SIZE];
  assert(votrax::textToSc01("HELLO WORLD", phones, sizeof(phones)));
  assert(std::string(phones) == "H EH1 UH3 L UH3 O1 U1 PA0 W UH3 ER L D");
  assert(votrax::textToSc01("hello world", phones, sizeof(phones)));
  assert(std::string(phones) == "H EH1 UH3 L UH3 O1 U1 PA0 W UH3 ER L D");

  std::string input, expected;
  for (int i = 0; i < 40; ++i) {
    input += "HELLO ";
    if (i)
      expected += " PA0 ";
    expected += "H EH1 UH3 L UH3 O1 U1";
  }
  assert(votrax::textToSc01(input.c_str(), phones, sizeof(phones)));
  assert(phones == expected); // Regression: original reciter truncated long lines.

  struct {
    char before;
    char output[4];
    char after;
  } bounded = {'A', {}, 'Z'};
  assert(!votrax::textToSc01("HELLO WORLD", bounded.output, sizeof(bounded.output)));
  assert(bounded.before == 'A' && bounded.after == 'Z' && bounded.output[0] == 0);
  assert(!reciteEnglish("HELLO", bounded.output, sizeof(bounded.output)));
  assert(bounded.before == 'A' && bounded.after == 'Z');
  assert(!votrax::textToSc01("", phones, sizeof(phones)));
  assert(!votrax::textToSc01(nullptr, phones, sizeof(phones)));
  assert(!votrax::textToSc01(std::string(254, 'A').c_str(), phones, sizeof(phones)));
  assert(votrax::textToSc01("HELLO[WORLD", phones, sizeof(phones)));
  assert(std::string(phones) == "H EH1 UH3 L UH3 O1 U1 PA0 W UH3 ER L D");
}

static void testCompactPhones() {
  std::string all;
  for (int c = 0x40; c <= 0x7f; ++c)
    all += static_cast<char>(c);
  char tokens[512], compact[65];
  assert(votrax::typeNTalkPhonemesToSc01(all.c_str(), tokens, sizeof(tokens)));
  assert(votrax::sc01TokensToTypeNTalk(tokens, compact, sizeof(compact)));
  assert(all == compact);
  assert(votrax::sc01TokensToTypeNTalk("I2_G EH T PA1", compact, sizeof(compact)));
  assert(std::string(compact) == "\\{j~");
  assert(!votrax::sc01TokensToTypeNTalk("NOTAPHONE", compact, sizeof(compact)));
  assert(!votrax::sc01TokensToTypeNTalk("G EH T", compact, 2));
  assert(votrax::phonemePrefixLength(" \t~abc") == 3);
  assert(votrax::phonemePrefixLength("\xcf\x80"
                                     "abc") == 2);
  assert(votrax::phonemePrefixLength("\xde"
                                     "abc") == 1);
  assert(votrax::phonemePrefixLength("\xff"
                                     "abc") == 1);
  assert(votrax::phonemePrefixLength("HELLO") == 0);
}

static void testSynthesis() {
  votrax::Sc01ApproxSynth synth;
  synth.begin();
  synth.say("G EH T R EH D Y PA1");
  int16_t pcm[64];
  size_t samples = 0;
  int peak = 0;
  while (synth.busy() && samples < 400000) {
    synth.render(pcm, 64);
    samples += 64;
    for (int sample : pcm)
      peak = std::max(peak, sample < 0 ? -sample : sample);
  }
  assert(!synth.busy() && samples > 1000 && peak > 100);
  synth.say("AH");
  synth.reset();
  assert(!synth.busy());
  synth.render(pcm, 64);
  for (int sample : pcm)
    assert(sample == 0);

  // Every compact phone, including pauses and STOP, must drain its queue.
  for (int c = 0x40; c <= 0x7f; ++c) {
    const char compact[] = {static_cast<char>(c), 0};
    char token[8];
    assert(votrax::typeNTalkPhonemesToSc01(compact, token, sizeof(token)));
    assert(votrax::Sc01ApproxSynth::lookupPhone(token));
    synth.begin();
    synth.say(token);
    samples = 0;
    while (synth.busy() && samples < 400000) {
      synth.render(pcm, 64);
      samples += 64;
    }
    assert(!synth.busy());
  }
}

static void testUsbLines() {
  LineBuffer<4> line;
  using Result = decltype(line)::Result;
  for (char c : std::string("TEST"))
    assert(line.append(c) == Result::Pending);
  assert(line.append('\r') == Result::Ready);
  assert(std::string(line.line()) == "TEST");
  assert(line.append('\n') == Result::Pending);
  for (char c : std::string("TOO LONG"))
    line.append(c);
  assert(line.append('\n') == Result::Overflow);
  line.append('O');
  line.append('X');
  line.append('\b');
  line.append('K');
  assert(line.append('\n') == Result::Ready && std::string(line.line()) == "OK");
  line.append('~');
  line.append(0x7f); // DEL is the compact STOP phone.
  assert(line.append('\n') == Result::Ready && line.line()[1] == 0x7f);
}

static std::vector<int16_t> renderAll(votrax::Sc01ApproxSynth &synth) {
  std::vector<int16_t> result;
  int16_t pcm[64];
  while (synth.busy() && result.size() < 400000) {
    synth.render(pcm, 64);
    result.insert(result.end(), pcm, pcm + 64);
  }
  assert(!synth.busy());
  return result;
}

static void testPhraseBank() {
  using namespace phrase_bank;
  clear();
  assert(!get(0) && !get(1) && !get(config::phraseSlotCount + 1));
  assert(learn(1, Format::Text, "HELLO WORLD") == Error::None);
  const Slot original = *get(1);
  assert(original.count == 13);
  assert(learn(1, Format::Phonemes, "NOTAPHONE") == Error::InvalidPhoneme);
  assert(learn(1, Format::Text, "") == Error::EmptyInput);
  assert(learn(1, Format::Text, std::string(254, 'A').c_str()) == Error::TooLong);
  // 253 chars of isolated digits expand far beyond the phone limit.
  std::string digits;
  for (int i = 0; i < 127; ++i) {
    if (i)
      digits += ' ';
    digits += '9';
  }
  assert(digits.size() == 253);
  assert(learn(1, Format::Text, digits.c_str()) == Error::TooLong);
  assert(learn(0, Format::Text, "HELLO") == Error::InvalidSlot);
  assert(learn(config::phraseSlotCount + 1, Format::Text, "HELLO") == Error::InvalidSlot);
  for (const char *bad : {"~A", "A?", "~A?MORE", "~A B?", "~A!B?"}) {
    assert(learn(1, Format::Compact, bad) == Error::InvalidCompact);
  }
  assert(learn(1, Format::Compact, "~?") == Error::EmptyInput);
  assert(get(1)->count == original.count &&
         memcmp(get(1)->phones, original.phones, original.count) == 0);

  const char *named = "I3_AH I1_G EH T PA1 STOP";
  assert(learn(2, Format::Phonemes, named) == Error::None);
  assert(get(2)->phones[0] == (0x24 | 0xc0));
  assert(get(2)->phones[1] == (0x1c | 0x40));
  votrax::Sc01ApproxSynth direct, packed;
  direct.begin();
  packed.begin();
  direct.say(named);
  for (size_t i = 0; i < get(2)->count; ++i)
    packed.enqueuePackedPhone(get(2)->phones[i]);
  assert(renderAll(direct) == renderAll(packed));

  // Every code survives storage, including phone zero and STOP (ASCII DEL).
  std::string compact = "~";
  for (int c = 0x40; c <= 0x7f; ++c)
    compact += static_cast<char>(c);
  compact += '?';
  assert(learn(3, Format::Compact, compact.c_str()) == Error::None);
  assert(get(3)->count == 64);
  for (size_t i = 0; i < 64; ++i)
    assert(get(3)->phones[i] == i);
  assert(learn(3, Format::Compact, "\xcf\x80@?") == Error::None);
  assert(get(3)->count == 1 && get(3)->phones[0] == 0);

  compact = "~" + std::string(config::phrasePhoneLimit, '@') + '?';
  for (size_t i = 1; i <= config::phraseSlotCount; ++i) {
    assert(learn(i, Format::Compact, compact.c_str()) == Error::None);
    assert(get(i)->count == config::phrasePhoneLimit);
  }
  compact.insert(1, "@");
  assert(learn(1, Format::Compact, compact.c_str()) == Error::TooLong);
  assert(get(1)->count == config::phrasePhoneLimit);
  assert(learn(1, Format::Phonemes, "G EH T") == Error::None && get(1)->count == 3);
  assert(forget(1) == Error::None && !get(1));
  assert(forget(1) == Error::EmptySlot && forget(0) == Error::InvalidSlot);
  clear();
  for (size_t i = 1; i <= config::phraseSlotCount; ++i)
    assert(!get(i));
}

int main() {
  testEnglish();
  testCompactPhones();
  testSynthesis();
  testUsbLines();
  testPhraseBank();
  std::cout << "PASS: English/codec/synthesis/USB framing, RAM phrase bank bounds and packed "
               "inflection\n";
}
