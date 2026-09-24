#include "vic_serial.h"
#include "config.h"
#include "serial_commands.h"
#include "speech.h"
#include <Arduino.h>
#include <votrax_reciter.h>

namespace vic_serial {
namespace {
HardwareSerial VicSerial(config::vicUart);
uint8_t vic_voice_rx[config::vicRxBufferSize];
size_t vic_voice_rx_length = 0;
uint32_t vic_voice_last_byte_ms = 0;
bool vic_voice_echo = config::vicEcho;
bool vic_voice_psend = false;
bool vic_voice_caps = false;
bool vic_voice_timer = true;
bool vic_voice_break_received = false;
bool discard_until_newline = false;
char votrax_text_buffer[votrax::SC01_TEXT_BUFFER_SIZE];
} // namespace

static bool IsVicVoiceAlpha(uint8_t value) {
  value &= 0x7f;
  return (((value >= 'A') && (value <= 'Z')) || ((value >= 'a') && (value <= 'z')));
}

static bool IsVicVoiceUpper(uint8_t value) {
  value &= 0x7f;
  return ((value >= 'A') && (value <= 'Z'));
}

static uint8_t VicVoiceTextByte(uint8_t value) {
  value &= 0x7f;
  if (IsVicVoiceAlpha(value)) {
    value ^= 0x20; /*The VIC Voice PETSCII/ASCII letter conversion.*/
  }
  return (value);
}

static void EchoVicVoiceByte(uint8_t value) { VicSerial.write(VicVoiceTextByte(value)); }

static bool IsVicVoicePhonemeMarker(uint8_t value) {
  return ((value == 0x7e) || (value == 0xde) || (value == 0xff));
}

static String NormalizeVicVoiceText(const String &raw, bool applyCaps = true) {
  String result;
  result.reserve(raw.length() + 16);

  size_t input = 0;
  while (input < raw.length()) {
    uint8_t value = static_cast<uint8_t>(raw[input]);
    uint8_t converted = VicVoiceTextByte(value);
    if (IsVicVoiceAlpha(converted)) {
      size_t start = input;
      bool all_upper = true;
      while ((input < raw.length()) && IsVicVoiceAlpha(static_cast<uint8_t>(raw[input]))) {
        if (!IsVicVoiceUpper(VicVoiceTextByte(static_cast<uint8_t>(raw[input])))) {
          all_upper = false;
        }
        input++;
      }

      bool spell = applyCaps && vic_voice_caps && all_upper && ((input - start) >= 2);
      for (size_t i = start; i < input; i++) {
        result += static_cast<char>(VicVoiceTextByte(static_cast<uint8_t>(raw[i])));
        if (spell && ((i + 1) < input)) {
          result += ' ';
        }
      }
    } else {
      result += static_cast<char>(VicVoiceTextByte(value));
      input++;
    }
  }
  return (result);
}

static void DeliverVicVoiceSc01(const char *sc01) {
  if (vic_voice_psend) {
    static char compact[config::vicRxBufferSize];
    if (votrax::sc01TokensToTypeNTalk(sc01, compact, sizeof(compact))) {
      VicSerial.write(reinterpret_cast<const uint8_t *>(compact), strlen(compact));
    } else {
      Serial.println("ERROR: VIC Voice could not encode the SC-01 PSEND stream.");
    }
  } else {
    speech::speakPhonemes(sc01, &VicSerial);
  }
}

static void DeliverVicVoiceText(const String &raw) {
  String text = NormalizeVicVoiceText(raw);
  text.trim();
  size_t offset = 0;

  /*The spelling-rule input is limited, so split a Votrax-sized line at
    whitespace while retaining the VIC Voice 768-byte receive buffer.*/
  while (offset < text.length()) {
    while ((offset < text.length()) && (text[offset] == ' ')) {
      offset++;
    }
    if (offset >= text.length()) {
      break;
    }

    size_t end = offset + 240;
    if (end >= text.length()) {
      end = text.length();
    } else {
      int split = text.lastIndexOf(' ', static_cast<unsigned int>(end));
      if (split > static_cast<int>(offset)) {
        end = static_cast<size_t>(split);
      }
    }

    String chunk =
        text.substring(static_cast<unsigned int>(offset), static_cast<unsigned int>(end));
    if (votrax::textToSc01(chunk.c_str(), votrax_text_buffer, sizeof(votrax_text_buffer))) {
      DeliverVicVoiceSc01(votrax_text_buffer);
      if (vic_voice_break_received) {
        break;
      }
    } else {
      Serial.println("ERROR: VIC Voice English-to-SC-01 conversion failed.");
    }
    offset = end;
  }
}

static void DeliverVicVoicePhonemes(const String &raw) {
  static const size_t PHONEMES_PER_CHUNK = 96;
  size_t offset = 0;

  while (offset < raw.length()) {
    size_t count = raw.length() - offset;
    if (count > PHONEMES_PER_CHUNK) {
      count = PHONEMES_PER_CHUNK;
    }

    String compact;
    compact.reserve(count);
    for (size_t i = 0; i < count; i++) {
      uint8_t code = static_cast<uint8_t>(raw[offset + i]);
      compact += static_cast<char>(0x40 | (code & 0x3f));
    }

    if (vic_voice_psend) {
      VicSerial.write(reinterpret_cast<const uint8_t *>(compact.c_str()), compact.length());
    } else if (votrax::typeNTalkPhonemesToSc01(compact.c_str(), votrax_text_buffer,
                                               sizeof(votrax_text_buffer))) {
      speech::speakPhonemes(votrax_text_buffer, &VicSerial);
      if (vic_voice_break_received) {
        break;
      }
    } else {
      Serial.println("ERROR: VIC Voice SC-01 phoneme conversion failed.");
    }
    offset += count;
  }
}

static void ProcessVicVoiceContent(const String &content) {
  String text;
  String phonemes;
  bool phonetic = false;

  for (size_t i = 0; i < content.length(); i++) {
    uint8_t value = static_cast<uint8_t>(content[i]);
    if (!phonetic && IsVicVoicePhonemeMarker(value)) {
      DeliverVicVoiceText(text);
      if (vic_voice_break_received) {
        return;
      }
      text = "";
      phonetic = true;
    } else if (phonetic && ((value & 0x7f) == '?')) {
      DeliverVicVoicePhonemes(phonemes);
      if (vic_voice_break_received) {
        return;
      }
      phonemes = "";
      phonetic = false;
    } else if (phonetic) {
      phonemes += static_cast<char>(value);
    } else {
      text += static_cast<char>(value);
    }
  }

  if (phonetic) {
    DeliverVicVoicePhonemes(phonemes);
  } else {
    DeliverVicVoiceText(text);
  }
}

static void HandleVicVoiceEscape(size_t &input, size_t processingLength) {
  if ((input + 1) >= processingLength) {
    return;
  }

  uint8_t command = vic_voice_rx[++input] & 0x7f;
  switch (command) {
  case 0x11:
    vic_voice_psend = true;
    break;
  case 0x12:
    vic_voice_psend = false;
    break;
  case 0x13:
    vic_voice_echo = true;
    break;
  case 0x14:
    vic_voice_echo = false;
    break;
  case 0x15:
    vic_voice_caps = true;
    break;
  case 0x16:
    vic_voice_caps = false;
    break;
  case 0x17:
    vic_voice_timer = false;
    break;

  case 0x18:
    Serial.println("VIC Voice reset command received.");
    delay(20);
    ESP.restart();
    break;

  case 'Y':
  case '=':
    /*VIC Voice consumes VT52/ADM-3A row and column bytes.*/
    for (size_t i = 0; (i < 2) && ((input + 1) < processingLength); i++) {
      input++;
      if (vic_voice_echo) {
        EchoVicVoiceByte(vic_voice_rx[input]);
      }
    }
    command = 0;
    break;
  }

  /*Reproduce VIC Voice's SDC/cascade response and ignore unit selection.*/
  if ((command >= 8) && (command < 0x10)) {
    command++;
  } else if (command > 0x10) {
    command = 0;
  }
  if (vic_voice_echo) {
    VicSerial.write(command);
  }
}

static void ProcessVicVoiceBuffer(size_t processingLength) {
  if (processingLength > vic_voice_rx_length) {
    processingLength = vic_voice_rx_length;
  }

  String content;
  content.reserve(processingLength);
  bool terminated = false;

  for (size_t input = 0; input < processingLength; input++) {
    uint8_t raw = vic_voice_rx[input];
    uint8_t value = raw & 0x7f;

    if (vic_voice_echo) {
      EchoVicVoiceByte(value);
    }

    if (value == 0x1b) {
      HandleVicVoiceEscape(input, processingLength);
    } else if (value < ' ' && value != '\t') {
      if ((value == 0x08) && (content.length() > 0)) {
        content.remove(content.length() - 1);
      }
      if (value == '\r' || value == '\n') {
        terminated = true;
        break;
      }
    } else {
      content += static_cast<char>(raw);
    }
  }

  size_t remaining = vic_voice_rx_length - processingLength;
  if (remaining > 0) {
    memmove(vic_voice_rx, vic_voice_rx + processingLength, remaining);
  }
  vic_voice_rx_length = remaining;

  vic_voice_break_received = false;
  if (content.isEmpty())
    return;
  if (serial_commands::recognizes(content.c_str())) {
    if (!terminated) {
      // Never commit a command truncated by timeout or receive-buffer limits.
      discard_until_newline = true;
      vic_voice_rx_length = 0; // Discard any trailing incomplete escape as well.
      VicSerial.printf("ERROR INCOMPLETE COMMAND; SEND CR OR LF\r\n");
    } else {
      // Use the original bytes: compact SC-01 codes are case-sensitive.
      serial_commands::handle(content.c_str(), VicSerial, true);
    }
    return;
  }
  // Baud commands remain available with CAPS spelling or PSEND enabled.
  String command = NormalizeVicVoiceText(content, false);
  command.trim();
  if (command.equalsIgnoreCase("SET TTY LO") || command.equalsIgnoreCase("SET TTY HI")) {
    uint32_t baud = command.equalsIgnoreCase("SET TTY LO") ? 1200 : 2400;
    VicSerial.flush(); // Finish any echo at the old baud rate before switching.
    VicSerial.updateBaudRate(baud);
    Serial.printf("VIC UART: %lu baud.\n", static_cast<unsigned long>(baud));
    return;
  }
  ProcessVicVoiceContent(content);
  if (vic_voice_psend) {
    VicSerial.write('\r');
  }
}

static size_t ReadyLength(bool flushPartial = false) {
  for (size_t input = 0; input < vic_voice_rx_length; input++) {
    uint8_t value = vic_voice_rx[input] & 0x7f;
    if (value == 0x1b) {
      // Commands and cursor coordinates can themselves contain CR bytes.
      // Preserve an incomplete escape across UART reads and timeout flushes.
      size_t start = input;
      if (input + 1 >= vic_voice_rx_length)
        return flushPartial ? start : 0;
      uint8_t command = vic_voice_rx[++input] & 0x7f;
      if (command == 'Y' || command == '=') {
        if (input + 2 >= vic_voice_rx_length)
          return flushPartial ? start : 0;
        input += 2;
      }
    } else if (value == '\r' || value == '\n') {
      return (input + 1);
    }
  }
  return (flushPartial ? vic_voice_rx_length : 0);
}

void poll() {
  while (VicSerial.available() > 0) {
    if (vic_voice_rx_length >= config::vicRxBufferSize) {
      if (!speech::active()) {
        ProcessVicVoiceBuffer(ReadyLength(true));
        continue;
      }

      // A truncated pending command must never replace a learned slot.
      vic_voice_rx_length = 0;
      discard_until_newline = true;
      VicSerial.printf("ERROR RECEIVE OVERFLOW; PENDING INPUT DISCARDED\r\n");
    }

    uint8_t value = static_cast<uint8_t>(VicSerial.read());

    /*A UART BREAK is normally delivered as a zero byte by this Arduino core.*/
    if (value == 0) {
      speech::stop();
      vic_voice_break_received = true;

      Serial.println("VIC Voice BREAK/NUL: SC-01 speech queue cleared.");
      continue;
    }

    if (discard_until_newline) {
      if ((value & 0x7f) == '\r' || (value & 0x7f) == '\n')
        discard_until_newline = false;
      continue;
    }

    vic_voice_rx[vic_voice_rx_length++] = value;
    vic_voice_last_byte_ms = millis();
  }

  if (speech::active()) {
    return;
  }

  size_t completeLine;
  while ((completeLine = ReadyLength()) > 0) {
    ProcessVicVoiceBuffer(completeLine);
  }

  if (vic_voice_timer && (vic_voice_rx_length > 0) &&
      ((millis() - vic_voice_last_byte_ms) >= config::vicTimeoutMs)) {
    size_t ready = ReadyLength(true);
    if (ready > 0)
      ProcessVicVoiceBuffer(ready);
  }
}

void begin() {
  vic_voice_rx_length = 0;
  vic_voice_last_byte_ms = millis();
  vic_voice_echo = config::vicEcho;
  vic_voice_psend = false;
  vic_voice_caps = false;
  vic_voice_timer = true;
  vic_voice_break_received = false;
  discard_until_newline = false;
  VicSerial.setRxBufferSize(config::vicRxBufferSize);
  VicSerial.begin(config::vicBaud, SERIAL_8N1, config::vicRxPin, config::vicTxPin);
}

} // namespace vic_serial
