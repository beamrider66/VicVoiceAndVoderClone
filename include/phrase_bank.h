#pragma once

#include "config.h"

namespace phrase_bank {
enum class Format { Text, Phonemes, Compact };
enum class Error {
  None,
  InvalidSlot,
  EmptySlot,
  EmptyInput,
  InvalidPhoneme,
  InvalidCompact,
  TooLong,
  ConversionFailed
};

struct Slot {
  uint8_t phones[config::phrasePhoneLimit] = {};
  size_t count = 0;
};

// All storage is ordinary RAM. Learning is atomic: failure preserves the slot.
void clear();
Error learn(size_t number, Format format, const char *input);
Error forget(size_t number);
const Slot *get(size_t number);
const char *errorText(Error error);
} // namespace phrase_bank
