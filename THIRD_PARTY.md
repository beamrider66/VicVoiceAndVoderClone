# Source provenance

VVVC was extracted from the local `SerialSpeechSynthesizerSAM` project.
The source project was left unchanged.

- `votrax_synth.cpp`, `votrax_synth.h` and `votrax_rom_tables.h` retain that
  project's SC-01 approximation and coefficient tables. The inherited phone
  table identifies its data as MAME-derived. The unused AudioOutput adapter
  and unused sample-rate selection were removed.
- `votrax_reciter.cpp` retains the source project's SC-01 mapping and compact
  Type 'N Talk codec, with the external SAM library dependency removed.
- `english_reciter.c` and `english_rules.h` are adapted from `reciter.c` and
  `ReciterTabs.h` in the source project's installed **ESP8266SAM 1.1.0** library.
  Only the text rule matcher and tables are retained. They are not a SAM
  synthesizer. Changes isolate their workspace, add an explicit output
  capacity, avoid the old short-output cutoff, and remove platform/debug hooks.
- ES8388/I2S audio, the startup tones and the VIC Voice-style protocol derive from
  `source/SSSSAM/wokwi_esp32.cpp` in the source project.
- The ES8388 codec uses Phil Schatzmann's **arduino-audio-driver v0.1.3**,
  fetched by PlatformIO from its tagged upstream source. Its driver code and
  `License.txt` carry GPLv3 notices. VVVC uses the source project's codec/pin
  configuration with only audio pins registered; no SD or button inputs are
  initialized. The driver dependency retains its upstream license files.
- `tools/data/wizard_of_wor.json` preserves all 75 entries in the original
  `source/SSSSAM/wizard_phrases.cpp` table, in the same order, with its phoneme
  fragments expanded. That file attributes its SC-01 streams to the old
  Votrax Sample Project. The labels (including their original spelling) and
  inflection prefixes are retained. This optional host-side catalog is not
  linked into the ESP32 firmware; `tools/learn_wizard.py` loads it into RAM.

Upstream acknowledgements:

- [Jan Derogee — Serial Speech Synthesizer SAM](https://janderogee.com/projects/SerialSpeechSynthesisSAM/SerialSpeechSynthesisSAM.htm)
- [Earle F. Philhower — ESP8266SAM](https://github.com/earlephilhower/ESP8266SAM)
- [Sebastian Macke — SAM reconstruction](https://github.com/s-macke/SAM)
- [Phil Schatzmann — Arduino Audio Driver](https://github.com/pschatzmann/arduino-audio-driver/tree/v0.1.3)

No new blanket license is assigned to the inherited material. In particular,
the original project's description of the reciter as public domain is not
repeated here: its installed upstream README contains the following notice.

## Upstream reciter license notice

> While the ESP8266 wrapper is my own, the SAM software is a reverse-engineered version of a software published more than 34 years ago by "Don't ask Software".
>
> The company no longer exists. Any attempt to contact the original authors failed. Hence S.A.M. can be best described as Abandonware (http://en.wikipedia.org/wiki/Abandonware)
>
> As long this is the case I cannot put my code under any specific open source software license. However the software might be used under the "Fair Use" act (https://en.wikipedia.org/wiki/FAIR_USE_Act) in the USA.

## VIC-Voder protocol reference

The text interface and `SET TTY LO` / `SET TTY HI` commands follow Rick Melick's
[VIC-Voder Model NX64H user's guide](https://www.geocities.ws/cbm/vic-voder/vic-voder-manual.pdf).
VVVC contains no Festival code, voice data, Linux image or firmware from that
product. The combined serial interface uses the local SC-01 implementation for speech.
