#include "phrase_commands.h"
#include "phrase_bank.h"
#include "speech.h"
#include <Arduino.h>
#include <cctype>
#include <string>

namespace phrase_commands {
namespace {
enum class Command { None, Learn, Play, Forget, Slots, Clear };

void skipSpace(const char *&p) {
  while (isspace(static_cast<unsigned char>(*p)))
    ++p;
}

std::string word(const char *&p, bool petscii) {
  skipSpace(p);
  std::string result;
  while (*p && !isspace(static_cast<unsigned char>(*p))) {
    unsigned char c = static_cast<unsigned char>(*p++);
    if (petscii)
      c &= 0x7f;
    result += static_cast<char>(toupper(c));
  }
  skipSpace(p);
  return result;
}

Command command(const char *&p, bool petscii) {
  const auto name = word(p, petscii);
  if (name == "-LEARN")
    return Command::Learn;
  if (name == "-PLAY")
    return Command::Play;
  if (name == "-FORGET")
    return Command::Forget;
  if (name == "-SLOTS")
    return Command::Slots;
  if (name == "-CLEAR")
    return Command::Clear;
  return Command::None;
}

bool slotNumber(const std::string &text, size_t &number) {
  number = 0;
  if (text.empty())
    return false;
  for (char c : text) {
    if (c < '0' || c > '9')
      return false;
    const size_t digit = c - '0';
    if (number > config::phraseSlotCount / 10 ||
        (number == config::phraseSlotCount / 10 && digit > config::phraseSlotCount % 10))
      return false;
    number = number * 10 + digit;
  }
  return number > 0;
}

void result(HardwareSerial &reply, size_t number, phrase_bank::Error error) {
  if (error == phrase_bank::Error::None)
    reply.printf("OK %u\r\n", static_cast<unsigned>(number));
  else
    reply.printf("ERROR %u %s\r\n", static_cast<unsigned>(number), phrase_bank::errorText(error));
}
} // namespace

bool recognizes(const char *line, bool petscii) {
  return line && command(line, petscii) != Command::None;
}

bool handle(const char *line, HardwareSerial &reply, bool petscii) {
  if (!line)
    return false;
  Command action = command(line, petscii);
  if (action == Command::None)
    return false;

  if (action == Command::Clear || action == Command::Slots) {
    if (*line) {
      reply.printf("ERROR UNEXPECTED ARGUMENTS\r\n");
    } else if (action == Command::Clear) {
      phrase_bank::clear();
      reply.printf("OK CLEAR\r\n");
    } else {
      size_t used = 0;
      for (size_t i = 1; i <= config::phraseSlotCount; ++i)
        if (phrase_bank::get(i))
          ++used;
      reply.printf("SLOTS %u\r\n", static_cast<unsigned>(used));
      for (size_t i = 1; i <= config::phraseSlotCount; ++i) {
        const auto *slot = phrase_bank::get(i);
        if (slot)
          reply.printf("SLOT %u %u\r\n", static_cast<unsigned>(i),
                       static_cast<unsigned>(slot->count));
      }
      reply.printf("END\r\n");
    }
    return true;
  }

  size_t number;
  if (!slotNumber(word(line, petscii), number)) {
    reply.printf("ERROR SLOT MUST BE 1..%u\r\n", static_cast<unsigned>(config::phraseSlotCount));
    return true;
  }

  if (action == Command::Learn) {
    auto type = word(line, petscii);
    phrase_bank::Format format;
    if (type == "TEXT")
      format = phrase_bank::Format::Text;
    else if (type == "PHONEMES")
      format = phrase_bank::Format::Phonemes;
    else if (type == "COMPACT")
      format = phrase_bank::Format::Compact;
    else {
      reply.printf("ERROR EXPECTED TEXT, PHONEMES OR COMPACT\r\n");
      return true;
    }
    std::string input(line);
    if (petscii && format != phrase_bank::Format::Compact) {
      for (char &c : input)
        c = static_cast<char>(static_cast<uint8_t>(c) & 0x7f);
    }
    result(reply, number, phrase_bank::learn(number, format, input.c_str()));
  } else if (*line) {
    reply.printf("ERROR UNEXPECTED ARGUMENTS\r\n");
  } else if (action == Command::Forget) {
    result(reply, number, phrase_bank::forget(number));
  } else {
    const auto *slot = phrase_bank::get(number);
    if (!slot)
      result(reply, number, phrase_bank::Error::EmptySlot);
    else if (speech::speakPacked(slot->phones, slot->count))
      result(reply, number, phrase_bank::Error::None);
    else
      reply.printf("ERROR %u PLAYBACK INTERRUPTED\r\n", static_cast<unsigned>(number));
  }
  return true;
}
} // namespace phrase_commands
