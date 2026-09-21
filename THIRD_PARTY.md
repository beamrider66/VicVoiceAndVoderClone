# Source provenance

VVVC was extracted from the local `SerialSpeechSynthesizerSAM` project.
The source project was left unchanged.

- `votrax_synth.cpp`, `votrax_synth.h` and `votrax_rom_tables.h` retain that
  project's SC-01 approximation and coefficient tables. The inherited phone
  table identifies its data as MAME-derived. The unused AudioOutput adapter
  and unused sample-rate selection were removed.
- `votrax_reciter.cpp` retains the source project's SC-01 mapping and compact
  Type 'N Talk codec, with the external SAM library dependency removed.
- `english_reciter.c` is an English-to-SC-01 text-to-phoneme converter adapted
  from the local reference `DevReferences/VotraxSC01/votrax_sc01_tts_dictionary`
  (`votrax_sc01_tts.c`). It is a small, deterministic, malloc-free C99 engine
  built in four stages: (1) exact SC-01 programs transcribed from the Votrax
  SC-01 Phonetic Dictionary — 1,288 word entries in `votrax_sc01_dictionary.h`
  (a sorted word→offset/length table plus a raw SC-01 phoneme-code blob, looked
  up by binary search); (2) a small hand-tuned override table for words missing
  from or ambiguous in the scanned dictionary (`IS`, `THE`, `YOU`, `YOUR`,
  `YES`, `WHO`, `HELLO`, `DO`, `DOES`, `DONE`, `DOOR`, `EIGHT`, `TO`, `TWO`,
  `THIS`, `CADET`); (3) the public-domain Naval Research Laboratory (NRL)
  English letter-to-sound rules (NRL Report 7948, 1976, via John A. Wasser's
  1985 public-domain C implementation); and (4) the NRL/Votrax IPA-to-SC-01
  mapping, including the manufacturer's "liquid L" and dictionary-style vowel
  expansions (short I `I1 I3`, short E `EH1 EH3`, short A `AE1 EH3`, long E
  `E1 Y`, long I `AH1 EH3 Y`, long U/OO `IU U1 U1`). Integer and decimal
  numbers are expanded to words, and the SC-01 `T,CH` and `D,J` ordering
  requirements are applied automatically. It is not a SAM synthesizer and not a
  pronunciation lexicon. The dictionary was transcribed from a user-supplied
  scan of the 1981 Votrax dictionary; entries with unresolved OCR glyphs are
  omitted (the NRL rules handle them), and the reference's `dictionary_audit.json`
  records the flagged lines. The port inlines the reference's default options
  (word pause, sentence pause, final STOP, rhotic R) and exposes
  `reciteEnglish()`, which writes space-separated SC-01 phoneme names (no
  trailing STOP) and reports whether the output fits.
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
- [Sebastian Macke — SAM reconstruction](https://github.com/s-macke/SAM)
- [Phil Schatzmann — Arduino Audio Driver](https://github.com/pschatzmann/arduino-audio-driver/tree/v0.1.3)
- Votrax, *Phonetic Speech Dictionary for the SC-01 Speech Synthesizer*, 1981.
- Elovitz, Johnson, McHugh & Shore, *Automatic Translation of English Text to
  Phonetics by Means of Letter-to-Sound Rules*, NRL Report 7948, 1976.
- John A. Wasser's 1985 public-domain C implementation of the NRL rules:
  https://www.tuhs.org/Usenet/comp.sources.unix/1985-April/005246.html
- Greg Kennedy's transcription of the original NRL IPA-to-Votrax rules:
  https://github.com/greg-kennedy/p5-NRL-TextToPhoneme

No new blanket license is assigned to the inherited material. The SC-01
synthesizer tables still derive from the SAM project, whose installed upstream
README carries the following notice. The text-to-phoneme converter
(`english_reciter.c`) no longer comes from that project; it is adapted from the
local `votrax_sc01_tts_dictionary` reference, whose letter-to-sound rules are
public-domain (NRL / Wasser 1985), whose IPA-to-SC-01 mapping is derived from
the public-domain Votrax dictionary and the NRL transcription, and whose
1,288-entry word table is a transcription of the 1981 Votrax SC-01 Phonetic
Dictionary.

## Upstream SAM license notice

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
