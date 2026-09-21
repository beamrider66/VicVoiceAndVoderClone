#include "votrax_reciter.h"

#include "english_reciter.h"

#include <ctype.h>
#include <string.h>

namespace votrax {
namespace {
/* SC-01 code order from the Type 'N Talk Appendix B. TNT represents code
   0 as ASCII 0x40, code 1 as 0x41, and so on through 0x7f. */
static const char *kTypeNTalkPhones[] = {
    "EH3", "EH2", "EH1", "PA0", "DT",  "A1", "A2",  "ZH", "AH2", "I3",  "I2",  "I1",  "M",
    "N",   "B",   "V",   "CH",  "SH",  "Z",  "AW1", "NG", "AH1", "OO1", "OO",  "L",   "K",
    "J",   "H",   "G",   "F",   "D",   "S",  "A",   "AY", "Y1",  "UH3", "AH",  "P",   "O",
    "I",   "U",   "Y",   "T",   "R",   "E",  "W",   "AE", "AE1", "AW2", "UH2", "UH1", "UH",
    "O2",  "O1",  "IU",  "U1",  "THV", "TH", "ER",  "EH", "E1",  "AW",  "PA1", "STOP"};

bool appendToken(char *output, size_t outputSize, size_t &used, const char *token) {
  size_t tokenLength = strlen(token);
  size_t separator = (used > 0) ? 1 : 0;
  if ((used + separator + tokenLength + 1) > outputSize) {
    return (false);
  }

  if (separator != 0) {
    output[used++] = ' ';
  }
  memcpy(output + used, token, tokenLength);
  used += tokenLength;
  output[used] = 0;
  return (true);
}
} // namespace

size_t phonemePrefixLength(const char *text) {
  if (text == NULL) {
    return (0);
  }

  size_t offset = 0;
  while ((text[offset] == ' ') || (text[offset] == '\t')) {
    offset++;
  }

  const unsigned char *p = reinterpret_cast<const unsigned char *>(text + offset);
  if (p[0] == 0) {
    return (0);
  }
  if ((p[0] == 0x7e) || (p[0] == 0xde) || (p[0] == 0xff)) {
    return (offset + 1);
  }
  if ((p[0] == 0xcf) && (p[1] == 0x80)) {
    return (offset + 2);
  }
  return (0);
}

bool typeNTalkPhonemesToSc01(const char *phonemes, char *sc01, size_t sc01Size) {
  if ((phonemes == NULL) || (sc01 == NULL) || (sc01Size == 0)) {
    return (false);
  }

  size_t output = 0;
  sc01[0] = 0;
  for (size_t input = 0; phonemes[input] != 0; input++) {
    unsigned char code = static_cast<unsigned char>(phonemes[input]);
    if ((code < 0x40) || (code > 0x7f)) {
      continue;
    }

    if (!appendToken(sc01, sc01Size, output, kTypeNTalkPhones[code - 0x40])) {
      return (false);
    }
  }
  return (output > 0);
}

bool sc01TokensToTypeNTalk(const char *sc01, char *phonemes, size_t phonemeSize) {
  if ((sc01 == NULL) || (phonemes == NULL) || (phonemeSize == 0)) {
    return (false);
  }

  size_t input = 0;
  size_t output = 0;
  phonemes[0] = 0;

  while (sc01[input] != 0) {
    while (isspace(static_cast<unsigned char>(sc01[input]))) {
      input++;
    }
    if (sc01[input] == 0) {
      break;
    }

    size_t start = input;
    while ((sc01[input] != 0) && !isspace(static_cast<unsigned char>(sc01[input]))) {
      input++;
    }
    size_t length = input - start;

    if ((length > 3) && (sc01[start] == 'I') && (sc01[start + 1] >= '0') &&
        (sc01[start + 1] <= '3') && (sc01[start + 2] == '_')) {
      start += 3;
      length -= 3;
    }

    bool matched = false;
    for (size_t phone = 0; phone < (sizeof(kTypeNTalkPhones) / sizeof(kTypeNTalkPhones[0]));
         phone++) {
      if ((strlen(kTypeNTalkPhones[phone]) == length) &&
          (strncmp(sc01 + start, kTypeNTalkPhones[phone], length) == 0)) {
        if ((output + 1) >= phonemeSize) {
          phonemes[0] = 0;
          return (false);
        }
        phonemes[output++] = static_cast<char>(0x40 + phone);
        matched = true;
        break;
      }
    }

    if (!matched) {
      phonemes[0] = 0;
      return (false);
    }
  }

  phonemes[output] = 0;
  return (output > 0);
}

bool textToSc01(const char *text, char *sc01, size_t sc01Size) {
  if ((text == NULL) || (sc01 == NULL) || (sc01Size == 0)) {
    return (false);
  }

  size_t length = strlen(text);
  if ((length == 0) || (length > 253)) {
    sc01[0] = 0;
    return (false);
  }

  /* reciteEnglish already emits space-separated SC-01 phoneme names. */
  if (!reciteEnglish(text, sc01, sc01Size)) {
    sc01[0] = 0;
    return (false);
  }
  return (true);
}
} // namespace votrax
