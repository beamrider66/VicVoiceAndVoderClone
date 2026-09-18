#pragma once

class HardwareSerial;

namespace serial_commands {
// Every dash command is shared by USB and the VIC, including unknown commands.
bool recognizes(const char *line);
bool handle(const char *line, HardwareSerial &reply, bool petscii = false);
void printHelp(HardwareSerial &reply);
} // namespace serial_commands
