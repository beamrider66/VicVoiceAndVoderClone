# VVVC — Vic Voice and Voder Clone

Speech firmware for **ESP32 boards**, with always-on GPIO PWM audio and an
optional **ES8388 Audio-Kit 16-bit I2S DAC and speaker amplifier**. The same
SC-01 speech and startup tones feed both outputs without selecting a mode.
An absent codec does not prevent a normal DevKit from starting and speaking
through its PWM pin. GPIO audio needs an external filter and amplifier.

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

The combined interface provides VIC-Voice-style controls and **SC-01 compact
phone numbering**, plus the basic text and
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

On Windows, double-click [`build_and_flash.bat`](build_and_flash.bat) to build
and flash **COM3**. The window stays open to show the result. From a terminal:

```bat
build_and_flash.bat COM3
build_and_flash.bat COM7
build_and_flash.bat --build-only
```

Use the board's USB-to-UART port, and close the serial console first. The script
finds PlatformIO in its usual VS Code installation or on PATH, builds the
`esp32dev` environment, and uploads only if the build succeeds. It works from
any current directory. Flashing resets the board and clears learned RAM phrases;
run the Wizard loader again if needed. `--build-only` leaves the board alone.

Open this folder in VS Code with PlatformIO, or run:

```sh
pio run
pio run -t upload
pio device monitor
```

The default build environment is `esp32dev`. The Espressif32 platform is pinned
to `7.0.1`, the pinned build version. PlatformIO
automatically downloads Phil Schatzmann's `arduino-audio-driver` at **v0.1.3**,
the same codec library version used by the firmware. No filesystem image, Wi-Fi
configuration or separately downloaded speech library is required.

The available builds use 4 MB flash layouts and the board's USB-to-UART port:

| Environment | Chip family | PWM audio GPIO | VIC RX / TX | Codec output |
| --- | --- | --- | --- | --- |
| `esp32dev` | Original ESP32 (Audio-Kit or DevKit) | 22 | 18 / 5, UART2 | ES8388 if detected |
| `esp32c3` | ESP32-C3 | 4 | 6 / 7, UART1 | None |
| `esp32s2` | ESP32-S2 | 4 | 6 / 7, UART1 | None |
| `esp32s3` | ESP32-S3 | 4 | 6 / 7, UART1 | None |

For example, `pio run -e esp32c3 -t upload --upload-port COM7`. The batch file
continues to build the original ESP32 target. Each chip family needs its own
binary; PWM does not make one binary run on every ESP32. These four families
are build-checked with the pinned Arduino 2.0.17 toolchain. C2/C6/H2/P4 and
other families are not supported by these builds. Boards need at least 4 MB
flash and the chosen pins exposed and unused. Native USB-only boards need a
USB-to-UART adapter for the console with these profiles.

The PWM pin can be changed with `-DVVVC_PWM_PIN=number` in the environment's
`build_flags`. Preserve existing flags. Avoid flash, PSRAM, USB, boot-strapping,
UART, onboard peripheral and codec pins. The classic ESP32 build probes I2C
on GPIO33/32; keep these free even on a board without the codec. Do not press
any board button connected to an output. Check the specific board schematic.

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

## Browser installer

The installer is published at <https://beamrider66.github.io/VicVoiceAndVoderClone/>.
The public project repository holds firmware source on `main` and generated
website files on `gh-pages`. To rebuild and publish an update:

```powershell
powershell -File tools/publish_pages.ps1
```

GitHub Pages serves the root of the project's `gh-pages` branch over HTTPS.
The script requires Python, PlatformIO, Git and authenticated push access to
`beamrider66/VicVoiceAndVoderClone`. It builds locally and pushes the static files;
GitHub then deploys them automatically.

The older QNAP copy is at <http://www.fox-ts.co.uk/vvvc/> in
`\\qnap\qweb\vvvc`. **Direct browser flashing requires HTTPS** (with a trusted
certificate), or a local server on `http://localhost`. The HTTP page provides
a ZIP download: extract it, run `start-local.bat` with Python 3 installed, then
open `http://localhost:8765` in desktop Chrome or Edge.

Rebuild and publish from the repository root:

```powershell
python tools/build_web.py --publish '\\qnap\qweb\vvvc'
```

Omit `--publish` to generate only `build/web`. The script builds the current
firmware, merges the four flash images with DIO/40 MHz/4 MB settings, packages
ESP Web Tools 10.4.0 locally (verified against its npm SHA-512), and creates a
manifest, firmware checksum, guide and portable ZIP. No CDN is needed at runtime.
Publishing verifies copied files and retains older firmware images. Source page
files are in `web/`; generated files are ignored under `build/`.

The installer selects ESP32, C3, S2 or S3 firmware by chip family. It cannot
identify board wiring; users must check the pins above. Installation resets learned RAM
slots. This packaging process does not flash a connected device.

## Wiring

Connect the speaker to the Audio-Kit's **labelled speaker terminals**. The
headphone output is enabled as well. This uses the documented ES8388
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
sample values and the documented Audio-Kit playback timing. Startup volume
is **48%**, adjustable with `audioVolumePercent` in
[`include/config.h`](include/config.h). GPIO25 carries the codec's digital
word clock, not analog audio. Audio-Kit buttons, SD and audio capture are unused.

### Always-on GPIO audio

GPIO22 on the original ESP32 (GPIO4 on the C3/S2/S3 profiles) carries the same
mono PCM as the codec, converted to **8-bit PWM with a 156.25 kHz carrier**.
It continues to run when no codec is installed; there is no fallback mode or
audio selection command. The codec retains the full 16-bit samples. PWM volume
is independent of the codec's 48% volume setting; adjust the external amplifier.
The two outputs have independent buffering/clocks and are not phase-aligned.

Use this starting circuit into a high-impedance amplifier input (47 kOhm or
greater). Values may need adjustment for the amplifier and desired bandwidth:

```text
PWM GPIO -- 1k --+-- 4.7k --+-- 1uF film -- amplifier audio input
                |         |
               22nF      4.7nF
                |         |
ESP32 GND ------+---------+-------------- amplifier signal ground
```

The RC stages suppress the PWM carrier; the film capacitor blocks the roughly
1.65 V idle DC level. Keep wires short and start at low amplifier volume. Use
an amplifier to drive the speaker: **do not connect a speaker or headphones
directly to the GPIO**, and do not connect the GPIO to the Audio-Kit speaker
terminals. The pin is a 0–3.3 V PWM signal, not a built-in DAC output or a
digital interface for an external DAC. During idle/after BREAK it rests at
50% duty; the startup beep, `-tone`, `-demo` and every speech source use it.

PWM uses LEDC low-speed channel/timer 0 and hardware timer 0. Its ring buffer
is in internal RAM; the sample ISR does no allocation or logging. The timer
rate is about 44101.43 samples/s (33 ppm above the nominal 44100 Hz). A stopped
sample timer times out rather than hanging speech. A failed audio backend is
disabled while the other continues. This new GPIO path has host and build
checks but still needs physical filter, waveform and simultaneous audio tests.
The existing PDF covers the earlier Audio-Kit wiring, not this new GPIO circuit.

The VIC interface retains its existing wiring:

| Connection | ESP32 pin |
| --- | --- |
| VIC user-port M, transmitted data, through a 5 V to 3.3 V divider | GPIO 18, UART2 RX |
| VIC user-port B and C, received data / receive interrupt | GPIO 5, UART2 TX, connected to both |
| VIC user-port A or N, signal ground | GND |

Use the documented interface circuitry. For the RX divider, the firmware
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
with a 100 ms gap) through PWM and any detected ES8388, then prints help. To check speech too, send:

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

The VVVC PETSCII letter conversion, cursor-sequence consumption and
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
| `src/audio.cpp` | Fan-out to both outputs and startup/test tones |
| `src/audio_codec.cpp` | Optional ES8388/I2S 16-bit output and speaker amplifier |
| `src/audio_pwm.cpp`, `src/audio_pwm_hal.c` | Always-on buffered PWM and interrupt-safe duty updates |
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

English conversion uses a standalone, malloc-free **text-to-phoneme converter**
built on a 1,288-entry transcription
of the Votrax SC-01 Phonetic Dictionary, a small exact-word override table, the
public-domain NRL letter-to-sound rules and the NRL/Votrax IPA-to-SC-01 mapping
(with number expansion) and bounded output. See [`THIRD_PARTY.md`](THIRD_PARTY.md)
for provenance.
