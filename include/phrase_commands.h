#pragma once

class HardwareSerial;

namespace phrase_commands {
bool recognizes(const char *line, bool petscii = false);
// Replies go to the requesting UART, independently of echo and PSEND.
bool handle(const char *line, HardwareSerial &reply, bool petscii = false);
} // namespace phrase_commands
