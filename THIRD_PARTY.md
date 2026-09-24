# Third-party tools and references

VVVC is an independent ESP32 application. The serial commands and SC-01 speech
engine in this repository are maintained as VVVC code.

## Runtime dependencies

- **Arduino-ESP32**, supplied by PlatformIO's Espressif32 platform. The project
  pins platform `espressif32@7.0.1` in `platformio.ini` and uses the Arduino
  framework under its upstream license.
- **arduino-audio-driver v0.1.3** by Phil Schatzmann, fetched from its tagged
  upstream repository by PlatformIO for the optional ES8388 Audio-Kit output.
  The library includes its upstream GPLv3 license and notices in the PlatformIO
  dependency tree. VVVC's GPIO PWM output does not require this library.
- **ESP Web Tools 10.4.0**, used by the browser installer. The release bundle
  is copied into the generated `gh-pages` site under
  `vendor/esp-web-tools/` together with its upstream `LICENSE` file. The
  browser installer uses the project's documented manifest and Web Serial
  integration.

The application does not require Wi-Fi, a filesystem image, or a separate speech
library beyond the code compiled in `lib/sc01/`.

## Speech data and algorithms

- The SC-01 phone names, oscillator tables and synthesis coefficients in
  `lib/sc01/` are the project's implementation of the public SC-01 phoneme
  model. They are used to generate the PCM sent to the two audio outputs.
- The English converter's dictionary entries are transcribed from the 1981
  *Votrax SC-01 Phonetic Speech Dictionary*. The project records the source
  dictionary and unresolved transcription entries in its own data files.
- The fallback letter-to-sound rules are based on Elovitz, Johnson, McHugh and
  Shore, *Automatic Translation of English Text to Phonetics by Means of
  Letter-to-Sound Rules*, NRL Report 7948 (1976), and John A. Wasser's 1985
  public-domain C implementation:
  <https://www.tuhs.org/Usenet/comp.sources.unix/1985-April/005246.html>.
- The IPA-to-Votrax mapping is cross-checked against Greg Kennedy's public
  transcription:
  <https://github.com/greg-kennedy/p5-NRL-TextToPhoneme>.

The Wizard of Wor phrase catalog under `tools/data/` is optional host-side
input for the learning commands. It is not linked into the firmware and is not
needed for normal text or phoneme operation.

## Build-only tools

The reproducible build uses PlatformIO, esptool (provided by PlatformIO),
Python, and the Python packages listed in `tools/requirements.txt` and
`tools/requirements-docs.txt`. ReportLab is used only to generate the PDF guide;
it is not part of the firmware. These tools remain under their own upstream
licenses and are not embedded in the device image.

## Project documents

The wiring guide cites the Commodore VIC-20 manuals, the Espressif ESP32
datasheets, the Ai-Thinker Audio-Kit documentation, and the SC-01 dictionary as
technical references. Those documents remain the property of their respective
copyright holders. VVVC includes redrawn diagrams and explanatory text rather
than copied pages.

This file describes external dependencies and references only. See the license
files distributed by each dependency for its complete terms.
