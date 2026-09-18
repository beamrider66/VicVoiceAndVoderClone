#include "phrase_bank.h"
#include <cctype>
#include <cstring>
#include <votrax_reciter.h>
#include <votrax_synth.h>

namespace phrase_bank {
namespace {
static_assert(config::phraseSlotCount > 0 && config::phrasePhoneLimit > 0, "Empty phrase bank");
Slot slots[config::phraseSlotCount];
// The reciter has its own 4 KB working buffer on the stack; keep this one in RAM.
char converted[votrax::SC01_TEXT_BUFFER_SIZE];

bool validSlot(size_t number) { return number >= 1 && number <= config::phraseSlotCount; }

Error parseNamed(const char *input, Slot &slot) {
  while (*input) {
    while (isspace(static_cast<unsigned char>(*input)))
      ++input;
    if (!*input)
      break;
    const char *start = input;
    while (*input && !isspace(static_cast<unsigned char>(*input)))
      ++input;
    if (slot.count == config::phrasePhoneLimit)
      return Error::TooLong;
    uint8_t code;
    if (!votrax::Sc01ApproxSynth::packToken(std::string(start, input - start), code)) {
      return Error::InvalidPhoneme;
    }
    slot.phones[slot.count++] = code;
  }
  return slot.count ? Error::None : Error::EmptyInput;
}

Error parseCompact(const char *input, Slot &slot) {
  const size_t prefix = votrax::phonemePrefixLength(input);
  size_t end = strlen(input);
  while (end && isspace(static_cast<unsigned char>(input[end - 1])))
    --end;
  if (!prefix || end <= prefix || input[end - 1] != '?')
    return Error::InvalidCompact;
  for (size_t i = prefix; i + 1 < end; ++i) {
    const uint8_t code = static_cast<uint8_t>(input[i]);
    if (code < 0x40 || code > 0x7f)
      return Error::InvalidCompact;
    if (slot.count == config::phrasePhoneLimit)
      return Error::TooLong;
    slot.phones[slot.count++] = code & 0x3f;
  }
  return slot.count ? Error::None : Error::EmptyInput;
}
} // namespace

void clear() {
  for (auto &slot : slots)
    slot = Slot{};
}

Error learn(size_t number, Format format, const char *input) {
  if (!validSlot(number))
    return Error::InvalidSlot;
  if (!input || !*input)
    return Error::EmptyInput;
  Slot candidate;
  Error error;
  if (format == Format::Text) {
    if (strlen(input) > votrax::ENGLISH_INPUT_LIMIT)
      return Error::TooLong;
    if (!votrax::textToSc01(input, converted, sizeof(converted)))
      return Error::ConversionFailed;
    error = parseNamed(converted, candidate);
  } else if (format == Format::Phonemes) {
    error = parseNamed(input, candidate);
  } else {
    error = parseCompact(input, candidate);
  }
  if (error == Error::None)
    slots[number - 1] = candidate;
  return error;
}

Error forget(size_t number) {
  if (!validSlot(number))
    return Error::InvalidSlot;
  if (!slots[number - 1].count)
    return Error::EmptySlot;
  slots[number - 1] = Slot{};
  return Error::None;
}

const Slot *get(size_t number) {
  return validSlot(number) && slots[number - 1].count ? &slots[number - 1] : nullptr;
}

const char *errorText(Error error) {
  switch (error) {
  case Error::None:
    return "OK";
  case Error::InvalidSlot:
    return "INVALID SLOT";
  case Error::EmptySlot:
    return "EMPTY SLOT";
  case Error::EmptyInput:
    return "EMPTY INPUT";
  case Error::InvalidPhoneme:
    return "INVALID PHONEME";
  case Error::InvalidCompact:
    return "INVALID COMPACT BLOCK";
  case Error::TooLong:
    return "PHRASE TOO LONG";
  case Error::ConversionFailed:
    return "TEXT CONVERSION FAILED";
  }
  return "INVALID INPUT";
}
} // namespace phrase_bank
