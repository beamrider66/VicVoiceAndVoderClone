#include "audio.h"
#include "config.h"
#include "phrase_bank.h"
#include "vic_serial.h"
#include "votrax_reciter.h"
#include "votrax_synth.h"
#include <Arduino.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

HardwareSerial *HardwareSerial::ports[3] = {};
HardwareSerial Serial(0);
TestEsp ESP;
uint32_t testMillis = 0;
static size_t audioFrames = 0;
static int toneTests = 0;
static bool audioAvailable = true;
static int audioFinishes = 0;
static int audioStops = 0;
static std::string injectDuringAudio;

namespace audio {
bool begin() { return audioAvailable; }
bool finish() {
  ++audioFinishes;
  return audioAvailable;
}
void silence() { ++audioStops; }
bool test() {
  ++toneTests;
  return audioAvailable;
}
bool write(const int16_t *, size_t frames) {
  if (!audioAvailable)
    return false;
  audioFrames += frames;
  if (!injectDuringAudio.empty()) {
    HardwareSerial::ports[2]->feed(injectDuringAudio);
    injectDuringAudio.clear();
  }
  return true;
}
} // namespace audio

void setup();
void loop();

static HardwareSerial &vic() { return *HardwareSerial::ports[2]; }
static void usb(const char *line) {
  Serial.feed(std::string(line) + '\n');
  loop();
}
static void sendVic(const std::string &bytes) {
  vic().feed(bytes);
  vic_serial::poll();
}

static std::string expectedPhones(const char *text) {
  char tokens[4096], compact[2048];
  assert(votrax::textToSc01(text, tokens, sizeof(tokens)));
  assert(votrax::sc01TokensToTypeNTalk(tokens, compact, sizeof(compact)));
  return std::string(compact) + '\r';
}

static void enablePsend() {
  vic_serial::begin();
  sendVic("\x1b\x14\x1b\x11\r"); // Echo off, PSEND on.
  vic().tx.clear();
  audioFrames = 0;
}

static void testLearning() {
  using namespace phrase_bank;
  auto usbCommand = [](const std::string &line) {
    Serial.tx.clear();
    vic().tx.clear();
    audioFrames = 0;
    usb(line.c_str());
  };
  auto vicCommand = [](const std::string &line) {
    Serial.tx.clear();
    vic().tx.clear();
    audioFrames = 0;
    sendVic(line + '\r');
  };
  vic_serial::begin();
  sendVic("\x1b\x14\r");
  usbCommand("-slots");
  assert(Serial.tx == "SLOTS 0\r\nEND\r\n" && vic().tx.empty());
  usbCommand("-learn 1 text HELLO WORLD");
  assert(Serial.tx == "OK 1\r\n" && vic().tx.empty() && audioFrames == 0);
  assert(get(1) && get(1)->count == 8);
  const Slot original = *get(1);
  vicCommand("-play 1");
  assert(vic().tx == "OK 1\r\n" && Serial.tx.empty() && audioFrames > 0);
  const size_t helloFrames = audioFrames;

  // Commands accept PETSCII letters; compact codes are kept byte-for-byte.
  std::string petscii = "-LEARN 2 PHONEMES I3_AH";
  for (char &c : petscii)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c | 0x80);
  vicCommand(petscii);
  assert(vic().tx == "OK 2\r\n" && get(2)->phones[0] == (0x24 | 0xc0));
  usbCommand("-play 2");
  assert(Serial.tx == "OK 2\r\n" && vic().tx.empty() && audioFrames > 0);
  vicCommand("-LeArN 3 CoMpAcT \xde\\{j~?");
  assert(vic().tx == "OK 3\r\n" && get(3)->count == 4);
  const uint8_t codes[] = {0x1c, 0x3b, 0x2a, 0x3e};
  assert(memcmp(get(3)->phones, codes, sizeof(codes)) == 0);
  usbCommand("-learn 4 compact \xcf\x80@\x7f?");
  assert(Serial.tx == "OK 4\r\n" && get(4)->count == 2);
  assert(get(4)->phones[0] == 0 && get(4)->phones[1] == 63);

  vicCommand("-slots");
  assert(vic().tx == "SLOTS 4\r\nSLOT 1 8\r\nSLOT 2 1\r\nSLOT 3 4\r\nSLOT 4 2\r\nEND\r\n");
  for (const char *bad :
       {"-play 0", "-play 81", "-play -1", "-play 1X", "-play 9999999999999999999999999999",
        "-forget", "-learn 1", "-learn 1 other HELLO", "-learn 1 text", "-learn 1 phonemes NOPE",
        "-learn 1 compact ~A!B?", "-learn 1 compact ~ABC", "-clear extra", "-forget 1 extra",
        "-play 1 extra", "-slots extra"}) {
    usbCommand(bad);
    assert(Serial.tx.find("ERROR") == 0 && audioFrames == 0);
    assert(get(1)->count == original.count &&
           memcmp(get(1)->phones, original.phones, original.count) == 0);
  }

  // A full, valid compact command can use all 256 phones on the VIC connection.
  vicCommand("-learn 32 compact ~" + std::string(config::phrasePhoneLimit, '@') + '?');
  assert(vic().tx == "OK 32\r\n" && get(32)->count == config::phrasePhoneLimit);
  vicCommand("-learn 1 compact ~" + std::string(config::phrasePhoneLimit + 1, '@') + '?');
  assert(vic().tx == "ERROR 1 PHRASE TOO LONG\r\n" && get(1)->count == original.count);
  usbCommand("-learn 1 text " + std::string(254, 'A'));
  assert(Serial.tx.find("line too long") != std::string::npos && get(1)->count == original.count);

  // Oversized / unfinished commands cannot commit a valid prefix or speak their tail.
  std::string tooLong = "-learn 1 phonemes ";
  for (int i = 0; i < 300; ++i)
    tooLong += "AH ";
  vicCommand(tooLong);
  assert(vic().tx.find("ERROR INCOMPLETE") == 0 && audioFrames == 0);
  assert(get(1)->count == original.count);
  vic().tx.clear();
  sendVic(
      "-learn 1 text GOODBYE\x1b"); // A trailing partial escape must not leak into the next line.
  testMillis += config::vicTimeoutMs + 1;
  vic_serial::poll();
  assert(vic().tx.find("ERROR INCOMPLETE") == 0 && get(1)->count == original.count);
  sendVic("\r");
  vicCommand("-play 1");
  assert(vic().tx == "OK 1\r\n" && audioFrames == helloFrames);

  injectDuringAudio = tooLong + "\r-learn 5 text HELLO\r";
  usbCommand("-play 1");
  assert(Serial.tx == "OK 1\r\n");
  assert(vic().tx.find("ERROR RECEIVE OVERFLOW") == 0 &&
         vic().tx.find("OK 5\r\n") != std::string::npos);
  assert(get(1)->count == original.count && get(5));

  // Explicit playback still speaks with PSEND enabled, and supports BREAK.
  sendVic("\x1b\x11\x1b\x15\r");
  vicCommand("-learn 6 text hello");
  assert(vic().tx == "OK 6\r\n" && get(6)->count == 4);
  vicCommand("-play 6");
  assert(vic().tx == "OK 6\r\n" && audioFrames > 0);
  injectDuringAudio = std::string(1, '\0');
  vicCommand("-play 1");
  assert(vic().tx == "ERROR 1 PLAYBACK INTERRUPTED\r\n" && audioFrames == 32);
  assert(get(1)->count == original.count);

  usbCommand("-learn 1 phonemes G EH T");
  assert(Serial.tx == "OK 1\r\n" && get(1)->count == 3);
  vicCommand("-forget 1");
  assert(vic().tx == "OK 1\r\n" && !get(1));
  usbCommand("-play 1");
  assert(Serial.tx == "ERROR 1 EMPTY SLOT\r\n" && audioFrames == 0);
  usbCommand("-forget 1");
  assert(Serial.tx == "ERROR 1 EMPTY SLOT\r\n");
  vicCommand("-clear");
  assert(vic().tx == "OK CLEAR\r\n");
  usbCommand("-slots");
  assert(Serial.tx == "SLOTS 0\r\nEND\r\n");

  usbCommand("-learn 1 text HELLO");
  usbCommand("-learn 32 text WORLD");
  const int restarts = ESP.restarts;
  sendVic("\x1b\x18\r");
  assert(ESP.restarts == restarts + 1);
  setup(); // Host restart simulation: execute the firmware's real boot path.
  for (size_t i = 1; i <= config::phraseSlotCount; ++i)
    assert(!get(i));
  usbCommand("-slots");
  assert(Serial.tx == "SLOTS 0\r\nEND\r\n");
}

static void testWizardCatalog(const char *path) {
  std::ifstream input(path);
  assert(input.is_open());
  phrase_bank::clear();
  std::string line;
  size_t count = 0;
  while (std::getline(input, line)) {
    auto separator = line.find('\t');
    assert(separator != std::string::npos);
    size_t id = std::stoul(line.substr(0, separator));
    std::string phones = line.substr(separator + 1);
    std::string command = "-learn " + std::to_string(id) + " phonemes " + phones;
    assert(command.size() <= config::usbLineSize);
    Serial.tx.clear();
    usb(command.c_str());
    assert(Serial.tx == "OK " + std::to_string(id) + "\r\n");
    const auto *slot = phrase_bank::get(id);
    assert(slot);
    std::istringstream tokens(phones);
    std::string token;
    size_t index = 0;
    while (tokens >> token) {
      uint8_t code;
      assert(votrax::Sc01ApproxSynth::packToken(token, code));
      assert(index < slot->count && slot->phones[index++] == code);
    }
    assert(index == slot->count);
    ++count;
  }
  assert(count == 75);
  Serial.tx.clear();
  usb("-slots");
  assert(Serial.tx.find("SLOTS 75\r\n") == 0);
  // A VIC client can recall the host-loaded phrase numbers, including the last.
  vic().tx.clear();
  sendVic("\x1b\x14\r");
  vic().tx.clear();
  sendVic("-play 75\r");
  assert(vic().tx == "OK 75\r\n");
  std::cout << "PASS: all 75 Wizard of Wor phrases learned with original inflection\n";
}

static void testSharedCommands() {
  vic_serial::begin();
  sendVic("\x1b\x14\x1b\x11\x1b\x15\x1b\x17\rSET TTY LO\r");
  usb("-learn 76 text HELLO WORLD");
  const phrase_bank::Slot saved = *phrase_bank::get(76);
  auto request = [](bool fromVic, const std::string &line) {
    Serial.tx.clear();
    vic().tx.clear();
    audioFrames = 0;
    if (fromVic)
      sendVic(line + '\r');
    else
      usb(line.c_str());
    assert((fromVic ? Serial.tx : vic().tx).empty());
    return fromVic ? vic().tx : Serial.tx;
  };

  // Compare the real USB and VIC parsers, including errors and exact PCM length.
  for (const char *command : {"-help", "-tone", "-demo", "-phonemes G EH T R EH D Y PA1",
                              "-phonemes I3_AH", "-phonemes NOPE", "-phonemes", "-unknown",
                              "-demo extra", "-help extra", "-tone extra", "  -PhOnEmEs\tAH  "}) {
    const int beforeUsbTones = toneTests;
    const auto expected = request(false, command);
    const size_t expectedFrames = audioFrames;
    const int expectedTones = toneTests - beforeUsbTones;
    const int beforeVicTones = toneTests;
    assert(request(true, command) == expected);
    assert(audioFrames == expectedFrames && toneTests - beforeVicTones == expectedTones);
    assert(!expected.empty());
  }
  assert(vic().baud == 1200);
  assert(phrase_bank::get(76)->count == saved.count);
  assert(memcmp(phrase_bank::get(76)->phones, saved.phones, saved.count) == 0);

  // PETSCII uppercase command names and token payloads use the same dispatcher.
  const std::string phoneReply = request(false, "-phonemes I3_AH");
  const size_t phoneFrames = audioFrames;
  std::string petscii = "-PHONEMES I3_AH";
  for (char &c : petscii)
    if (c >= 'A' && c <= 'Z')
      c = static_cast<char>(c | 0x80);
  assert(request(true, petscii) == phoneReply && audioFrames == phoneFrames);
  assert(request(true, "-DeMo").find("Demo complete.\r\n") != std::string::npos);

  // The demo and explicit phonemes preserve PSEND, CAPS, echo and the timer.
  vic().tx.clear();
  audioFrames = 0;
  sendVic("abc");
  testMillis += config::vicTimeoutMs + 1;
  vic_serial::poll();
  assert(vic().tx.empty());
  sendVic("\r");
  assert(vic().tx == expectedPhones("A B C") && audioFrames == 0);

  // No dash command runs from a timed-out or overfull prefix.
  vic_serial::begin();
  sendVic("\x1b\x14\r");
  vic().tx.clear();
  const int beforePartial = toneTests;
  sendVic("-demo");
  testMillis += config::vicTimeoutMs + 1;
  vic_serial::poll();
  assert(vic().tx.find("ERROR INCOMPLETE") == 0 && toneTests == beforePartial);
  sendVic("\r");
  const auto overflow = request(true, "-demo" + std::string(config::vicRxBufferSize, ' '));
  assert(overflow.find("ERROR INCOMPLETE") == 0 && toneTests == beforePartial);
  assert(request(true, "-tone") == "Tone complete.\r\n");

  // A command received during playback waits; it cannot nest another utterance.
  injectDuringAudio = "-tone\r";
  const int beforeQueuedTone = toneTests;
  assert(request(true, "-phonemes AH") == "OK PHONEMES\r\nTone complete.\r\n");
  assert(toneTests == beforeQueuedTone + 1);

  // Errors reach the VIC even when PSEND/echo are off and no USB terminal exists.
  audioAvailable = false;
  assert(request(true, "-tone") == "ERROR: audio test failed.\r\n");
  const auto failedDemo = request(true, "-demo");
  assert(failedDemo.find("ERROR: demo audio test failed.") != std::string::npos);
  assert(failedDemo.find("Demo complete") == std::string::npos);
  assert(request(true, "-phonemes AH") == "ERROR: speech interrupted or audio unavailable.\r\n");
  audioAvailable = true;
  phrase_bank::clear();
  std::cout << "PASS: shared USB/VIC commands, PETSCII, replies, demo preservation and errors\n";
}

int main(int argc, char **argv) {
  setup();
  assert(vic().baud == 2400 && Serial.baud == 115200 && toneTests == 1);

  sendVic("HELLO\r");
  assert(vic().tx == "hello\r" && audioFrames > 0);
  assert(audioFinishes == 1 && audioStops == 0);
  enablePsend();
  sendVic("HELLO WORLD\r");
  assert(vic().tx == expectedPhones("HELLO WORLD") && audioFrames == 0);

  vic().tx.clear();
  sendVic("~\\{j~?\r");
  assert(vic().tx == "\\{j~\r" && audioFrames == 0);

  vic().tx.clear();
  sendVic("HELLO ~\\{j~? WORLD\n");
  std::string hello = expectedPhones("HELLO");
  hello.pop_back();
  assert(vic().tx == hello + "\\{j~" + expectedPhones("WORLD") && audioFrames == 0);

  vic().tx.clear();
  sendVic("\x1bY\r"); // Incomplete cursor coordinates: the CR is not a terminator.
  testMillis += config::vicTimeoutMs + 1;
  vic_serial::poll();
  assert(vic().tx.empty());
  sendVic("\nHELLO\r");
  assert(vic().tx == expectedPhones("HELLO"));

  vic().tx.clear();
  sendVic("\x1b\x15\r"); // CAPS spells PETSCII lowercase bytes as uppercase letters.
  vic().tx.clear();
  sendVic("abc\r");
  assert(vic().tx == expectedPhones("A B C"));

  // Baud commands work alongside CAPS and PSEND, without becoming phonemes.
  vic().tx.clear();
  sendVic("set tty lo\r");
  assert(vic().baud == 1200 && vic().lastFlushBaud == 2400 && vic().tx.empty());
  sendVic("set tty hi\n");
  assert(vic().baud == 2400 && vic().lastFlushBaud == 1200 && vic().tx.empty());
  sendVic("~\\{j~?\r");
  assert(vic().tx == "\\{j~\r" && audioFrames == 0);

  enablePsend();
  sendVic("HELLO");
  assert(vic().tx.empty());
  testMillis += config::vicTimeoutMs + 1;
  vic_serial::poll();
  assert(vic().tx == expectedPhones("HELLO"));
  sendVic("\x1b\x17\r");
  vic().tx.clear();
  sendVic("WORLD");
  testMillis += config::vicTimeoutMs + 1;
  vic_serial::poll();
  assert(vic().tx.empty());
  sendVic("\r");
  assert(vic().tx == expectedPhones("WORLD"));

  vic_serial::begin();
  vic().tx.clear();
  audioFrames = 0;
  sendVic("SET TTY LO\r");
  assert(vic().baud == 1200 && audioFrames == 0 && vic().tx == "set tty lo\r");
  assert(vic().lastFlushBaud == 2400); // Echo completes before the baud change.
  sendVic("\x1b\x14\r");               // Echo off remains available at the lower baud rate.
  vic().tx.clear();
  sendVic("set tty hi\n");
  assert(vic().baud == 2400 && audioFrames == 0 && vic().tx.empty());
  sendVic("HELLO WORLD\r\n");
  assert(vic().tx.empty() && audioFrames > 0);
  const size_t beforePhones = audioFrames;
  sendVic("~\\{j~?\r");
  assert(vic().tx.empty() && audioFrames > beforePhones);

  sendVic("SET TTY LO\r");
  const size_t beforeDemo = audioFrames;
  usb("-demo");
  assert(toneTests == 2 && audioFrames > beforeDemo);
  assert(vic().baud == 1200 && vic().tx.empty());
  sendVic("HELLO\r");
  assert(vic().tx.empty()); // Demo preserves the current baud and echo settings.

  const size_t beforeRemovedCommands = audioFrames;
  for (const char *command : {"-mode", "-mode vic-voice", "-mode vic-voder", "-mode demo"}) {
    Serial.tx.clear();
    usb(command);
    assert(Serial.tx.find("unknown command") != std::string::npos);
  }
  assert(audioFrames == beforeRemovedCommands && toneTests == 2 && vic().baud == 1200);

  // Interrupt a phrase while more serial data arrives; later input still plays.
  audioFrames = 0;
  sendVic("WORLD\r");
  const size_t worldFrames = audioFrames;
  audioFrames = 0;
  injectDuringAudio = std::string(1, '\0') + "WORLD\r";
  const int finishesBeforeBreak = audioFinishes;
  const int stopsBeforeBreak = audioStops;
  sendVic("HELLO\r");
  assert(audioFrames == worldFrames + 32);
  assert(audioFinishes == finishesBeforeBreak + 1 && audioStops == stopsBeforeBreak + 1);
  assert(Serial.tx.find("BREAK/NUL") != std::string::npos);

  audioFrames = 0;
  usb("-phonemes NOTAPHONE");
  assert(audioFrames == 0);
  Serial.feed(std::string(254, 'A') + "\n");
  loop();
  assert(audioFrames == 0 && Serial.tx.find("line too long") != std::string::npos);
  usb("HELLO");
  assert(audioFrames > 0);

  testSharedCommands();
  testLearning();
  // Codec failure skips the boot tones but leaves learning and diagnostics usable.
  audioAvailable = false;
  const int tonesBeforeFailure = toneTests;
  Serial.tx.clear();
  setup();
  assert(toneTests == tonesBeforeFailure);
  assert(Serial.tx.find("Audio unavailable") != std::string::npos);
  const size_t framesBeforeFailure = audioFrames;
  Serial.tx.clear();
  usb("-demo");
  assert(Serial.tx.find("Demo complete") == std::string::npos);
  Serial.tx.clear();
  usb("-learn 1 text HELLO");
  assert(Serial.tx == "OK 1\r\n");
  Serial.tx.clear();
  usb("-play 1");
  assert(Serial.tx == "ERROR 1 PLAYBACK INTERRUPTED\r\n");
  assert(audioFrames == framesBeforeFailure);
  audioAvailable = true;
  Serial.tx.clear();
  usb("-play 1");
  assert(Serial.tx == "OK 1\r\n" && audioFrames > framesBeforeFailure);
  assert(argc == 2);
  testWizardCatalog(argv[1]);
  std::cout << "PASS: combined text/SC-01/controls/baud interface, framing/timer/BREAK, standalone "
               "demo, cross-port learning/replay/errors/overflow/reset, audio failure handling\n";
}
