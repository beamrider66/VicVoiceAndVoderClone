#pragma once

#include <stddef.h>

namespace votrax {
constexpr size_t SC01_TEXT_BUFFER_SIZE = 4096;
constexpr size_t ENGLISH_INPUT_LIMIT = 253;

// Translate English into SC-01 tokens. Returns false on empty input or overflow.
bool textToSc01(const char *text, char *sc01, size_t sc01Size);

// Compact Type 'N Talk codes 0x40..0x7f, in SC-01 phone order.
bool typeNTalkPhonemesToSc01(const char *phonemes, char *sc01, size_t sc01Size);
bool sc01TokensToTypeNTalk(const char *sc01, char *phonemes, size_t phonemeSize);

// PETSCII pi, UTF-8 pi or '~'; includes any preceding spaces/tabs.
size_t phonemePrefixLength(const char *text);
} // namespace votrax
