# VVVC — Vic Voice and Voder Clone

Speech firmware for the **ESP32 Audio-Kit with an ES8388 codec**, using the
working Audio-Kit configuration from `SerialSpeechSynthesizerSAM`. Audio uses
the board's **16-bit I2S DAC and built-in speaker amplifier**, with the same
mono signal on both channels. One local SC-01 engine supplies all speech.

## One combined interface

The VIC serial connection accepts English text, compact SC-01 phoneme blocks,
VIC-Voice escape controls and VIC-Voder `SET TTY LO` / `SET TTY HI` baud commands
together. Text and phonemes can be mixed within a line. There is no mode to
select, and the former USB `-mode` commands have been removed.

Both the VIC connection and the USB console also accept commands to learn
phrases in numbered **RAM-only slots**, then replay them by number. Slots are
shared between the connections and disappear when the ESP32 reboots or resets.

All speech uses **one Votrax SC-01 approximation**. Startup defaults are
2400 baud with echo on; escape controls can switch echo off or back on. Settings
are held in RAM and reset restores the defaults. The USB console runs at
**115200 baud**. All dash commands work on **both connections**, including
`-demo`, `-tone`, `-phonemes`, `-help` and every learning command. Replies return
to the connection that sent the command. `-demo` is a one-shot test and
preserves the serial settings and learned slots.

The combined interface retains the source project's VIC-Voice protocol
approximation and **SC-01 compact phone numbering**, plus the basic text and
baud commands documented in the [VIC-Voder user's guide](https://www.geocities.ws/cbm/vic-voder/vic-voder-manual.pdf).
It does not implement the original SP0256 phone set or reproduce VIC-Voder's
Festival voice or Linux features. Combining the controls also means command
handling and echo defaults differ from the individual original products.

## End-user guide

The combined [user and VIC-20 wiring guide](output/pdf/VVVC_User_and_VIC20_Wiring_Guide_v2.pdf)
contains USB and VIC BASIC examples, learning commands, the Wizard of Wor loader
and all 75 phrase numbers. Its wiring section labels the viewing direction and
TOP for the computer port, cable mating face and wire/solder side. It remains a
review draft until the physical VIC connection and examples have been tested.

To regenerate it from the project folder:

```sh
python -m pip install -r tools/requirements-docs.txt
python tools/build_user_guide.py
```

## Build and upload

Open this folder in VS Code with PlatformIO, or run:

```sh
pio run
pio run -t upload
pio device monitor
```

There is one build environment, `esp32dev`. The Espressif32 platform is pinned
to `7.0.1`, the version installed for the working source project. PlatformIO
automatically downloads Phil Schatzmann's `arduino-audio-driver` at **v0.1.3**,
the same codec library version as that project. No filesystem image, Wi-Fi
configuration or separately downloaded speech library is required.

`esp32dev` is the ESP32 build profile; the firmware requires the ES8388 hardware
on the Audio-Kit. A bare DevKit has no speaker output with this firmware.

If PlatformIO is not on your Windows PATH, its installed executable on this
machine can be invoked from PowerShell:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -t upload
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" device monitor
```

If several boards are connected, specify the appropriate `--upload-port COMx`
or monitor `--port COMx`. The application image is
`.pio/build/esp32dev/firmware.bin`; use PlatformIO upload to include the matching
bootloader and partition table.

## Wiring

Connect the speaker to the Audio-Kit's **labelled speaker terminals**. The
headphone output is enabled as well. This uses the source project's ES8388
Audio-Kit pin layout (`AudioKitEs8388V1` in the driver), including the required
master clock. The internal board connections are:

| Codec / amplifier connection | ESP32 pin |
| --- | --- |
| I2C SCL / SDA | GPIO 32 / 33 |
| I2S master clock (MCLK) | GPIO 0 |
| I2S bit clock (BCLK) | GPIO 27 |
| I2S word clock (LRCLK) | GPIO 25 |
| I2S data to ES8388 | GPIO 26 |
| Speaker amplifier enable | GPIO 21 |

Audio is **44.1 kHz, signed 16-bit stereo**, preserving the full SC-01 PCM
sample values and the source Audio-Kit build's playback timing. Startup volume
is **48%**, adjustable with `audioVolumePercent` in
[`include/config.h`](include/config.h). GPIO 25 now carries the codec's digital
word clock; the direct GPIO speaker output has been removed. Audio-Kit buttons,
SD and audio capture are unused. The old GPIO-buzzer simulation was removed.

The VIC interface retains its existing wiring:

| Connection | ESP32 pin |
| --- | --- |
| VIC user-port M, transmitted data, through a 5 V to 3.3 V divider | GPIO 18, UART2 RX |
| VIC user-port B and C, received data / receive interrupt | GPIO 5, UART2 TX, connected to both |
| VIC user-port A or N, signal ground | GND |

Use the existing interface circuitry. For the RX divider, the source project
uses 10 kOhm from user-port M to GPIO 18 and 18 kOhm from GPIO 18 to ground.
Connect grounds and power the ESP32 separately over USB. This is a TTL
user-port connection, not the round IEC bus or a PC's RS-232 voltage interface.
Do not connect the VIC's 5 V transmit signal directly to the ESP32.

GPIO 18 and 5 are shared with Audio-Kit keys 5 and 6; leave those keys unused
when the VIC interface is connected. Startup echo, buffer sizes, phrase-bank
limits and the line timeout can also be changed in `include/config.h`.

## First test

After upload, open the USB console at 115200 baud with a newline terminator.
Startup plays the original **880 Hz then 440 Hz** audio test (300 ms each,
with a 100 ms gap) through the DAC, then prints help. To check speech too, send:

```text
-demo
```

Expect 880 Hz and 440 Hz tones, then “HELLO WORLD. THIS IS VIC VOICE.” and
“GET READY.” The last phrase exercises direct phoneme synthesis separately
from English conversion. The console prints `Demo complete.` on success.
The demo checks the local speech/audio path; a VIC BASIC test checks the wiring
and UART separately.

If codec or I2S initialization fails, the console reports the error and skips
the startup tones. Check the board, then reset the ESP32. Learning and help
commands remain available, but playback reports failure while audio is
unavailable. Normal speech drains its final samples before returning; BREAK
clears queued audio immediately.

Commands accepted over USB or the VIC serial connection:

```text
HELLO WORLD
-phonemes G EH T R EH D Y PA1
-phonemes I0_AH I1_AH I2_AH I3_AH
-tone
-help
```

With VIC channel 1 already open at 2400 baud, the same commands can be sent with
`PRINT#1`, for example:

```basic
PRINT#1,"-DEMO"
PRINT#1,"-TONE"
PRINT#1,"-PHONEMES G EH T R EH D Y PA1"
PRINT#1,"-HELP"
```

Send one at a time and wait for completion. Demo and tone reply with
`Demo complete.` and `Tone complete.`; direct phonemes reply with
`OK PHONEMES`. Errors also return to the requesting connection. Replies use
ASCII and CR/LF. Disable VIC echo before parsing replies (see below), and
read `-help`/`-slots` output as it arrives: long replies can exceed the VIC's
small receive buffer. Explicit demo, tone and phoneme commands still play
audio with PSEND enabled. Unknown dash commands report an error on either port.

On the USB console, a leading `~`, PETSCII
pi (`0xDE` or `0xFF`) or UTF-8 pi selects **compact** Type 'N Talk bytes for that
USB line. Use `-phonemes` for readable SC-01 tokens. USB lines accept up to 253
bytes; longer lines are discarded with an error, rather than spoken partially.
USB commands run between utterances. There is no background demo loop.

## Learning and recalling phrases

There are **80 slots, numbered 1 through 80**, each holding up to **256 SC-01
phonemes**. Text is converted when learned; recall uses the saved phonemes
directly. Named phonemes retain `I0_` through `I3_` inflection prefixes. All
storage is normal RAM, using about 20 KB for the bank plus conversion workspace.
The firmware does not save phrases to flash or a filesystem.

Send these commands as complete CR- or LF-terminated lines on either port:

| Command | Action |
| --- | --- |
| `-learn 1 text HELLO WORLD` | Convert English and store it in slot 1, without speaking it |
| `-learn 2 phonemes G EH T R EH D Y PA1` | Store named SC-01 phonemes in slot 2 |
| `-learn 3 compact ~\{j~?` | Store compact codes for `G EH T PA1` in slot 3 |
| `-play 1` | Speak slot 1 |
| `-forget 1` | Delete slot 1 |
| `-slots` | List occupied slots and their phoneme counts |
| `-clear` | Delete every slot |

Commands and named phonemes are case-insensitive. Compact codes are
case-sensitive bytes, preserved exactly. In the compact example, `\` is one
literal backslash. A compact block must have an opening pi marker and a closing
`?`; its data bytes must all be `0x40` through `0x7F`. Use ASCII `~` or PETSCII pi
(`0xDE` / `0xFF`) on the VIC connection; the USB console also accepts UTF-8 pi.
The opening marker and closing `?` are not stored. Compact input uses inflection
zero; use named phonemes to specify inflection.

Relearning an occupied slot replaces it only after the complete new phrase
passes validation. Invalid or oversized input leaves the old phrase intact.
Text input is limited to 253 bytes before conversion, and the converted result
must fit in 256 phones. The USB line limit is 253 bytes **including the command**;
the VIC buffer accepts up to 768 bytes including the line terminator. The bank
limits can be changed with `phraseSlotCount` and `phrasePhoneLimit` in
`include/config.h`.

All dash commands require CR or LF. An unfinished VIC command is rejected at
the inactivity timeout or buffer limit; the remainder is discarded through
the next CR/LF. When a receive overflow occurs during playback, pending input
is discarded through the next terminator. This prevents a truncated learn
command from silently replacing a valid slot. Send one command and wait for
its reply before sending the next.

### Replies

Replies are ASCII, terminated by CR/LF, and sent to the connection that issued
the command. A successful learn, play or forget returns `OK n`, for example:

```text
OK 1
```

`-play` replies after playback completes. A BREAK/NUL interruption returns
`ERROR n PLAYBACK INTERRUPTED` and keeps the stored phrase. Empty slots, invalid
numbers and invalid content produce errors such as `ERROR 1 EMPTY SLOT`,
`ERROR SLOT MUST BE 1..80` or `ERROR 1 INVALID PHONEME`. Clear returns `OK CLEAR`.
A list reply looks like:

```text
SLOTS 2
SLOT 1 8
SLOT 2 8
END
```

`SLOTS` reports how many slots are occupied; each `SLOT` line gives the slot
number and phoneme count. An empty bank returns `SLOTS 0` followed by `END`.
The list reports stored phoneme counts, not the original English text.

VIC command replies are sent even when echo is off. For automated clients,
disable echo first with `ESC`, `0x14` and a carriage return, so echoed input
does not precede the replies. PSEND and CAPS settings do not alter learning
commands: text is learned with normal English pronunciation, and explicit
`-play` always produces speech. Both ports access the same bank, so a phrase
learned over USB can be recalled from the VIC, and vice versa.

To send the example commands from VIC BASIC, use the normal channel:

```basic
OPEN1,2,3,CHR$(10)
PRINT#1,"-LEARN 1 TEXT HELLO WORLD"
PRINT#1,"-PLAY 1"
CLOSE1
```

The ESP32's boot path clears every slot, including after the serial `ESC`,
`0x18` reset command. Resetting the VIC alone does not reset a separately
powered ESP32. Ordinary speech, baud changes and `-demo` preserve the bank.

## Loading the old Wizard of Wor phrases

[`tools/learn_wizard.py`](tools/learn_wizard.py) teaches **all 75 phrases** from
the old project's table, keeping the original phrase numbers and `I1_` / `I2_` /
`I3_` inflection tokens. The catalog is included in
[`tools/data/wizard_of_wor.json`](tools/data/wizard_of_wor.json); the old project
does not need to be installed beside VVVC. The phrases are host-side data and
are loaded into RAM through the ordinary `-learn` interface.

Upload the current 80-slot firmware, close the PlatformIO serial monitor, then
run from the project folder (replace `COM5` with the board's USB serial port):

```powershell
python -m pip install -r tools/requirements.txt
python tools/learn_wizard.py --port COM5
```

This fills slots **1 through 75**, replacing any phrases already in those
slots. Slots **76 through 80** remain available for other phrases. The script
waits for each `OK`, then verifies every loaded slot's phoneme count. It prints
the slot-to-phrase mapping and stops on errors. It does not play while learning.

Useful options:

```powershell
python tools/learn_wizard.py --list
python tools/learn_wizard.py --dry-run
python tools/learn_wizard.py --port COM5 --phrases 21 26 55 --play 21
```

`--phrases` loads only the specified original IDs, preserving their slot
numbers. `--dry-run` prints the exact learn commands without opening a port.
`--play 21` plays "Get ready, worrior." after loading and verification. Once
learned, send **`-play 21`** from either USB or the VIC connection to recall it;
all the other phrases work the same way. A bare number is still ordinary text.

The loader uses USB at 115200 baud and needs Python 3 and `pyserial`. On this
machine, PlatformIO's Python already includes `pyserial`:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" tools/learn_wizard.py --port COM5
```

Run the loader again after an ESP32 reboot/reset. Some serial monitors reset
the ESP32 when opening their port, which also clears the learned bank; using
the VIC connection or the loader's `--play` option avoids reopening USB to test.

## VIC serial input

The default is **2400 baud, 8 data bits, no parity, one stop bit**, without
hardware flow control. A simple VIC BASIC test is:

```basic
10 OPEN1,2,3,CHR$(10)
20 PRINT#1,"HELLO WORLD"
30 CLOSE1
```

The application receive buffer is 768 bytes. Long text is split at word
boundaries for conversion. Pending input is processed after
four seconds of inactivity if the sender omitted a terminator. The VIC UART
continues receiving during speech; when its finite buffers fill, pending
input may be discarded. Pace large transmissions instead of continuously
streaming faster than the device can speak.

### Text, phonemes and escape controls

CR or LF processes a line. Echo is enabled initially. A pi marker
(`0xDE`, `0xFF`, or ASCII `~`) starts a compact SC-01 block within the text;
`?` ends it. Compact bytes `0x40` through `0x7F` select the 64 SC-01 phones.
The text before and after a phoneme block is spoken normally. All USB dash
commands, including `-phonemes`, `-demo`, `-tone` and `-help`, are also accepted
on the VIC connection. Command names and named phones accept ASCII or PETSCII
letters without changing compact phoneme bytes.

| Bytes | Action |
| --- | --- |
| `ESC`, `0x11` | PSEND on: send compact phonemes back instead of speaking |
| `ESC`, `0x12` | PSEND off |
| `ESC`, `0x13` | Echo on |
| `ESC`, `0x14` | Echo off |
| `ESC`, `0x15` | CAPS spelling on |
| `ESC`, `0x16` | CAPS spelling off |
| `ESC`, `0x17` | Disable the inactivity timer until reset |
| `ESC`, `0x18` | Restart the ESP32 |

The inherited PETSCII letter conversion, cursor-sequence consumption and
single-device cascade responses are retained. There is no multi-device chain.
NUL stops the current phrase while preserving subsequently received input;
physical BREAK handling depends on the UART reporting it as NUL.

### Baud commands

These complete lines are always available on the VIC connection, including
when CAPS spelling or PSEND is enabled. They change the baud rate without
being spoken or returned as phonemes:

| Line | Action |
| --- | --- |
| `SET TTY LO` | Change the VIC UART to 1200 baud |
| `SET TTY HI` | Change it to 2400 baud |

Send the command at the **current** baud rate, then change the computer's baud
rate to match. On a VIC, use `CHR$(8)` when reopening the channel at 1200 baud,
and `CHR$(10)` at 2400. Pending echo is transmitted at the old baud rate before
the change. Baud commands preserve echo and the other settings; reset restores
2400 baud. Phoneme blocks and escape commands remain available at either speed.

## Source and checks

| Location | Purpose |
| --- | --- |
| `src/main.cpp` | Startup and USB line input |
| `src/serial_commands.cpp` | Common USB/VIC commands, demo, help and reply routing |
| `src/vic_serial.cpp` | Combined VIC text, phoneme, escape and baud interface |
| `src/phrase_bank.cpp` | Bounded RAM storage and validation of learned phrases |
| `src/phrase_commands.cpp` | Shared learn/play/list/delete commands and replies |
| `src/speech.cpp` | Shared SC-01 playback and interruption |
| `src/audio.cpp` | ES8388/I2S 16-bit output, speaker amplifier and startup/test tones |
| `lib/sc01/src/` | Portable speech engine, ROM tables and English conversion |
| `tools/learn_wizard.py`, `tools/data/` | USB loader and original Wizard of Wor phrase catalog |
| `test/`, `tools/check.py` | Host regression checks |

Run `python tools/check.py` with GCC/Clang available. The checks exercise
English conversion and bounds, all 64 compact phones, synthesis completion,
USB framing, mixed text/phonemes, VIC protocol controls, baud changes and demo
settings preservation. Shared-command checks compare both ports' output and
audio, including PETSCII, command errors, incomplete input, queued requests
and audio failures. Phrase-bank checks cover all 80 slots, size limits,
inflection, cross-port learning/replay, atomic replacement, interrupted or
truncated commands, deletion and boot-time clearing. DAC checks exercise full
16-bit sample preservation, stereo duplication, partial writes, startup tone
pitch/duration, silence, normal completion, BREAK and initialization/write
failures. These use host serial and codec/I2S adapters. Physical board testing
of the previous firmware also confirmed audible startup tones, the demo, USB text and phoneme speech,
loading all 75 Wizard phrases, and recalling a learned phrase. Electrical UART
timing and the VIC cable/BASIC examples still require the planned VIC test.
The shared demo/tone/help/phoneme commands have passed host checks. The revised
firmware was uploaded to COM3 on 2026-09-18 with flash hashes verified. USB help
confirmed the shared command set, the board reported `Demo complete.`, all 75
Wizard phrases were reloaded and verified, and slot 26 returned `OK 26` after
playback. The physical VIC serial connection remains untested.

The checks also learn all 75 Wizard phrases through the real firmware's USB
parser, compare their packed phonemes/inflections, and recall a loaded phrase
from the VIC port. Loader checks cover acknowledgements, verification, errors,
reset detection, old firmware rejection and selection of individual phrases.

The SAM voice generator, voice controls, compatibility audio wrapper,
ESP8266 targets, built-in Wizard of Wor playback, GPIO audio and unused filesystem /
network settings were removed. English conversion retains only the inherited
SAM-derived **spelling rules and matcher**, now standalone with bounded output;
there is no SAM voice library or SAM audio synthesis. See
[`THIRD_PARTY.md`](THIRD_PARTY.md) for provenance.
