#include "serial_commands.h"
#include "audio.h"
#include "config.h"
#include "phrase_commands.h"
#include "speech.h"
#include <Arduino.h>
#include <cctype>
#include <string>

namespace serial_commands {
namespace {
void skipSpace(const char *&p) {
  while (isspace(static_cast<unsigned char>(*p)))
    ++p;
}

void runDemo(HardwareSerial &reply) {
  reply.printf("Demo: two tones, HELLO WORLD. THIS IS VIC VOICE., then GET READY.\r\n");
  if (!audio::test()) {
    reply.printf("ERROR: demo audio test failed.\r\n");
  } else if (speech::speakText("HELLO WORLD. THIS IS VIC VOICE.", &reply) &&
             speech::speakPhonemes("G EH T R EH D Y PA1", &reply)) {
    reply.printf("Demo complete.\r\n");
  }
}
} // namespace

void printHelp(HardwareSerial &reply) {
  reply.printf("VVVC - Vic Voice and Voder Clone\r\n");
  reply.printf("Both ports accept all commands below.\r\n");
  reply.printf("English text + Enter: speak using Votrax SC-01\r\n");
  reply.printf("-phonemes G EH T R EH D Y PA1: speak named SC-01 phones\r\n");
  reply.printf("~<compact bytes>?: speak Type 'N Talk phones (0x40..0x7f)\r\n");
  reply.printf("-demo: audio tones, English speech, then GET READY phonemes\r\n");
  reply.printf("-tone: audio test only; -help: show this help\r\n");
  reply.printf("-learn N text|phonemes|compact INPUT, -play N, -forget N, -slots, -clear\r\n");
  reply.printf("RAM phrase bank: %u slots, %u phones each; cleared on reset.\r\n",
               static_cast<unsigned>(config::phraseSlotCount),
               static_cast<unsigned>(config::phrasePhoneLimit));
  reply.printf("VIC input also accepts escape controls and SET TTY LO/HI.\r\n");
  reply.printf("Audio: always-on 8-bit PWM on GPIO %d; external filter + amplifier required.\r\n",
               config::audioPwmPin);
#if VVVC_ES8388
  reply.printf("ES8388 Audio-Kit: also outputs 16-bit I2S to speaker/headphones when detected.\r\n");
#endif
  reply.printf("VIC UART default %lu baud 8N1, RX %d, TX %d.\r\n",
               static_cast<unsigned long>(config::vicBaud), config::vicRxPin, config::vicTxPin);
}

bool recognizes(const char *line) {
  if (!line)
    return false;
  skipSpace(line);
  return *line == '-';
}

bool handle(const char *line, HardwareSerial &reply, bool petscii) {
  if (!recognizes(line))
    return false;
  if (phrase_commands::handle(line, reply, petscii))
    return true; // Preserve compact learning payloads byte-for-byte.

  skipSpace(line);
  std::string command;
  while (*line && !isspace(static_cast<unsigned char>(*line))) {
    unsigned char c = static_cast<unsigned char>(*line++);
    if (petscii)
      c &= 0x7f;
    command += static_cast<char>(toupper(c));
  }
  skipSpace(line);
  if (command == "-PHONEMES") {
    if (!*line) {
      reply.printf("ERROR: use -phonemes followed by SC-01 tokens.\r\n");
      return true;
    }
    std::string phones(line);
    if (petscii)
      for (char &c : phones)
        c = static_cast<char>(static_cast<unsigned char>(c) & 0x7f);
    if (speech::speakPhonemes(phones.c_str(), &reply))
      reply.printf("OK PHONEMES\r\n");
  } else if (command == "-HELP" || command == "-TONE" || command == "-DEMO") {
    if (*line) {
      reply.printf("ERROR UNEXPECTED ARGUMENTS\r\n");
    } else if (command == "-HELP") {
      printHelp(reply);
    } else if (command == "-TONE") {
      reply.printf(audio::test() ? "Tone complete.\r\n" : "ERROR: audio test failed.\r\n");
    } else {
      runDemo(reply);
    }
  } else {
    reply.printf("ERROR: unknown command. Use -help.\r\n");
  }
  return true;
}
} // namespace serial_commands
