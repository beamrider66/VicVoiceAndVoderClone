#include "votrax_synth.h"
#include "votrax_rom_tables.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace votrax {
namespace {
static const int VOTRAX_FP_FRAC = 15;
static const size_t PHONE_COUNT = 64;

static const int32_t kGlottalWave[] = {
    0, -18724, 32768, 28086, 23405, 18724, 14043, 9362, 4681,
};

/* SC-01A phone parameter table decoded from the public MAME-derived ROM table.
   The stop field carries the raw SC-01 closure bit. */
static const PhoneParams kPhones[PHONE_COUNT] = {
    /* name  va vd fa cld fc f1 f2 f2q f3 dur pause stop voiced noise */
    {"EH3", 15, 1, 0, 7, 0, 9, 8, 0, 11, 19, false, false, true, false},
    {"EH2", 15, 2, 0, 7, 0, 9, 8, 0, 11, 23, false, false, true, false},
    {"EH1", 15, 2, 0, 6, 0, 9, 8, 0, 11, 38, false, false, true, false},
    {"PA0", 0, 3, 0, 3, 0, 7, 9, 0, 12, 15, true, false, false, false},
    {"DT", 0, 13, 8, 6, 15, 4, 6, 0, 12, 15, false, true, false, true},
    {"A1", 15, 1, 0, 6, 0, 6, 11, 0, 11, 23, false, false, true, false},
    {"A2", 15, 1, 0, 5, 0, 6, 11, 0, 11, 33, false, false, true, false},
    {"ZH", 1, 7, 8, 7, 15, 2, 11, 1, 14, 29, false, false, true, true},
    {"AH2", 9, 2, 0, 6, 0, 15, 3, 0, 11, 23, false, false, true, false},
    {"I3", 15, 2, 0, 4, 0, 5, 10, 0, 12, 18, false, false, true, false},
    {"I2", 15, 2, 0, 5, 0, 5, 10, 0, 12, 26, false, false, true, false},
    {"I1", 15, 2, 0, 5, 0, 5, 10, 0, 12, 38, false, false, true, false},
    {"M", 10, 5, 0, 6, 0, 1, 3, 15, 9, 33, false, false, true, false},
    {"N", 12, 3, 0, 6, 0, 1, 8, 15, 13, 26, false, false, true, false},
    {"B", 15, 6, 0, 5, 0, 1, 3, 0, 12, 23, false, true, true, false},
    {"V", 1, 7, 6, 7, 15, 2, 3, 1, 9, 23, false, false, true, true},
    {"CH", 0, 7, 9, 7, 15, 2, 11, 1, 14, 23, false, false, false, true},
    {"SH", 0, 7, 9, 3, 15, 2, 11, 1, 14, 38, false, false, false, true},
    {"Z", 1, 6, 11, 5, 0, 2, 3, 4, 13, 23, false, false, true, true},
    {"AW1", 11, 1, 0, 3, 0, 13, 2, 0, 10, 46, false, false, true, false},
    {"NG", 12, 2, 0, 3, 0, 2, 12, 12, 11, 38, false, false, true, false},
    {"AH1", 9, 2, 0, 4, 0, 15, 3, 0, 11, 46, false, false, true, false},
    {"OO1", 15, 1, 0, 5, 0, 8, 2, 0, 10, 33, false, false, true, false},
    {"OO", 15, 1, 0, 4, 0, 8, 2, 0, 10, 58, false, false, true, false},
    {"L", 10, 3, 0, 7, 0, 6, 2, 0, 15, 33, false, false, true, false},
    {"K", 0, 4, 4, 1, 15, 3, 10, 0, 8, 26, false, true, false, true},
    {"J", 1, 10, 7, 3, 15, 1, 10, 1, 14, 15, false, false, true, true},
    {"H", 0, 7, 1, 3, 15, 5, 8, 0, 9, 23, false, false, false, true},
    {"G", 15, 7, 0, 6, 0, 2, 10, 0, 8, 23, false, true, true, false},
    {"F", 0, 5, 4, 2, 15, 4, 3, 4, 9, 33, false, false, false, true},
    {"D", 15, 8, 0, 8, 0, 1, 9, 0, 14, 18, false, true, true, false},
    {"S", 0, 8, 15, 2, 0, 4, 7, 0, 12, 29, false, false, false, true},
    {"A", 15, 2, 0, 4, 0, 6, 11, 0, 11, 58, false, false, true, false},
    {"AY", 15, 1, 0, 7, 0, 3, 14, 0, 14, 21, false, false, true, false},
    {"Y1", 8, 1, 0, 6, 0, 1, 13, 0, 13, 26, false, false, true, false},
    {"UH3", 14, 4, 0, 7, 0, 12, 3, 0, 11, 15, false, false, true, false},
    {"AH", 9, 2, 0, 4, 0, 15, 3, 0, 11, 76, false, false, true, false},
    {"P", 0, 5, 6, 2, 15, 4, 2, 1, 8, 33, false, true, false, true},
    {"O", 15, 2, 0, 4, 0, 7, 1, 0, 11, 58, false, false, true, false},
    {"I", 15, 2, 0, 4, 0, 5, 10, 0, 12, 58, false, false, true, false},
    {"U", 15, 2, 0, 4, 0, 3, 1, 0, 10, 58, false, false, true, false},
    {"Y", 12, 1, 0, 6, 0, 2, 14, 0, 13, 33, false, false, true, false},
    {"T", 0, 7, 15, 2, 0, 4, 6, 0, 12, 23, false, true, false, true},
    {"R", 12, 4, 0, 7, 0, 6, 4, 0, 3, 29, false, false, true, false},
    {"E", 15, 1, 0, 3, 0, 2, 14, 0, 14, 58, false, false, true, false},
    {"W", 15, 4, 0, 9, 0, 3, 0, 0, 9, 26, false, false, true, false},
    {"AE", 11, 1, 0, 3, 0, 13, 9, 0, 11, 58, false, false, true, false},
    {"AE1", 11, 1, 0, 3, 0, 13, 9, 0, 11, 33, false, false, true, false},
    {"AW2", 11, 1, 0, 6, 0, 13, 2, 0, 10, 29, false, false, true, false},
    {"UH2", 14, 4, 0, 7, 0, 12, 3, 0, 11, 23, false, false, true, false},
    {"UH1", 14, 3, 0, 6, 0, 12, 3, 0, 11, 33, false, false, true, false},
    {"UH", 14, 3, 0, 5, 0, 12, 3, 0, 11, 58, false, false, true, false},
    {"O2", 15, 1, 0, 5, 0, 7, 1, 0, 11, 26, false, false, true, false},
    {"O1", 15, 2, 0, 4, 0, 7, 1, 0, 10, 38, false, false, true, false},
    {"IU", 15, 4, 0, 9, 0, 5, 4, 0, 8, 19, false, false, true, false},
    {"U1", 15, 2, 0, 5, 0, 3, 1, 0, 10, 29, false, false, true, false},
    {"THV", 1, 6, 1, 4, 0, 3, 7, 0, 12, 26, false, false, true, true},
    {"TH", 0, 8, 3, 1, 0, 5, 8, 0, 10, 23, false, false, false, true},
    {"ER", 15, 1, 0, 4, 0, 6, 4, 0, 3, 46, false, false, true, false},
    {"EH", 15, 2, 0, 5, 0, 9, 8, 0, 11, 58, false, false, true, false},
    {"E1", 15, 2, 0, 5, 0, 2, 14, 0, 14, 38, false, false, true, false},
    {"AW", 11, 3, 0, 5, 0, 13, 2, 0, 10, 76, false, false, true, false},
    {"PA1", 0, 4, 0, 4, 0, 7, 9, 0, 12, 58, true, false, false, false},
    {"STOP", 0, 1, 0, 1, 0, 7, 9, 4, 12, 15, false, true, false, false},
};

struct PhoneAlias {
  const char *alias;
  const char *target;
};

static const PhoneAlias kAliases[] = {
    {"PA", "PA0"}, {"IY", "E"},  {"IH", "I"},   {"AA", "AH"}, {"AO", "O"},
    {"UW", "U"},   {"AX", "UH"}, {"OW", "O1"},  {"OY", "O1"}, {"EY", "A"},
    {"HH", "H"},   {"JH", "J"},  {"DH", "THV"}, {"BUL", "B"}, {"WOR", "O"},
};

template <typename T> T clampValue(T value, T low, T high) {
  if (value < low) {
    return (low);
  }
  if (value > high) {
    return (high);
  }
  return (value);
}

std::string normalizeToken(const std::string &token) {
  std::string normalized;
  normalized.reserve(token.length());
  for (size_t i = 0; i < token.length(); i++) {
    unsigned char c = static_cast<unsigned char>(token[i]);
    if (!std::isspace(c)) {
      normalized.push_back(static_cast<char>(std::toupper(c)));
    }
  }
  return (normalized);
}

bool lookupPhoneIdExact(const std::string &token, uint8_t &phone) {
  for (size_t i = 0; i < PHONE_COUNT; i++) {
    if (token == kPhones[i].name) {
      phone = static_cast<uint8_t>(i);
      return (true);
    }
  }

  for (size_t i = 0; i < (sizeof(kAliases) / sizeof(kAliases[0])); i++) {
    if (token == kAliases[i].alias) {
      return (lookupPhoneIdExact(kAliases[i].target, phone));
    }
  }

  return (false);
}

bool decodeToken(const std::string &raw, uint8_t &phone, uint8_t &inflection) {
  std::string token = normalizeToken(raw);
  inflection = 0;

  if ((token.length() > 3) && (token[0] == 'I') && (token[2] == '_') && (token[1] >= '0') &&
      (token[1] <= '3')) {
    inflection = static_cast<uint8_t>(token[1] - '0');
    token.erase(0, 3);
  }

  if (lookupPhoneIdExact(token, phone)) {
    return (true);
  }

  while (!token.empty() && std::isdigit(static_cast<unsigned char>(token[token.length() - 1]))) {
    token.erase(token.length() - 1);
  }

  return (!token.empty() && lookupPhoneIdExact(token, phone));
}

int32_t fpScale15(int32_t value, uint8_t volume) {
  return (static_cast<int32_t>((static_cast<int64_t>(value) * volume * 2185) >> VOTRAX_FP_FRAC));
}

int32_t fpScale7(int32_t value, uint8_t closure) {
  return (static_cast<int32_t>((static_cast<int64_t>(value) * closure * 4681) >> VOTRAX_FP_FRAC));
}

template <size_t N> void shiftHistory(int32_t (&history)[N], int32_t value) {
  for (size_t i = N - 1; i > 0; i--) {
    history[i] = history[i - 1];
  }
  history[0] = value;
}

template <size_t XN, size_t YN, size_t RN>
int32_t applyFilter(const int32_t (&xHistory)[XN], const int32_t (&yHistory)[YN],
                    const int32_t (&rom)[RN], uint16_t base, int fracA, int fracB) {
  int64_t accA = 0;
  for (size_t i = 0; i < XN; i++) {
    accA += static_cast<int64_t>(xHistory[i]) * rom[base + 1 + i];
  }

  int64_t accB = 0;
  for (size_t i = 0; i < (YN - 1); i++) {
    accB += static_cast<int64_t>(yHistory[i]) * rom[base + 5 + i];
  }

  return (static_cast<int32_t>((accA >> fracA) - (accB >> fracB)));
}
} // namespace

const PhoneParams *Sc01ApproxSynth::lookupPhone(const std::string &token) {
  uint8_t phone = 0;
  uint8_t inflection = 0;
  if (!decodeToken(token, phone, inflection)) {
    return (nullptr);
  }
  return (&kPhones[phone]);
}

void Sc01ApproxSynth::begin() { reset(); }

void Sc01ApproxSynth::reset() {
  queue_.clear();

  inflection_ = 0;
  phone_ = 0x3F;
  arState_ = true;
  sampleTick_ = false;

  romDuration_ = 0;
  romVd_ = 0;
  romCld_ = 0;
  romFa_ = 0;
  romFc_ = 0;
  romVa_ = 0;
  romF1_ = 0;
  romF2_ = 0;
  romF2q_ = 0;
  romF3_ = 0;
  romClosure_ = false;
  romPause_ = false;

  curFa_ = 0;
  curFc_ = 0;
  curVa_ = 0;
  curF1_ = 0;
  curF2_ = 0;
  curF2q_ = 0;
  curF3_ = 0;

  filtFa_ = 0;
  filtFc_ = 0;
  filtVa_ = 0;
  filtF1_ = 0;
  filtF2_ = 0;
  filtF2q_ = 0;
  filtF3_ = 0;

  phonetick_ = 0;
  ticks_ = 0;
  pitch_ = 0;
  closure_ = 0;
  updateCounter_ = 0;
  curClosure_ = true;
  noise_ = 0;
  curNoise_ = false;

  f1Addr_ = 0;
  f2vAddr_ = 0;
  f3Addr_ = 0;

  clearHistories();
  phoneCommit();
  filtersCommit(true);
}

void Sc01ApproxSynth::clearHistories() {
  std::memset(voice1_, 0, sizeof(voice1_));
  std::memset(voice2_, 0, sizeof(voice2_));
  std::memset(voice3_, 0, sizeof(voice3_));
  std::memset(noise1_, 0, sizeof(noise1_));
  std::memset(noise2_, 0, sizeof(noise2_));
  std::memset(noise3_, 0, sizeof(noise3_));
  std::memset(noise4_, 0, sizeof(noise4_));
  std::memset(vn1_, 0, sizeof(vn1_));
  std::memset(vn2_, 0, sizeof(vn2_));
  std::memset(vn3_, 0, sizeof(vn3_));
  std::memset(vn4_, 0, sizeof(vn4_));
  std::memset(vn5_, 0, sizeof(vn5_));
  std::memset(vn6_, 0, sizeof(vn6_));
}

void Sc01ApproxSynth::enqueueToken(const std::string &token) {
  QueuedPhone parsed;
  if (parseToken(token, parsed)) {
    queue_.push_back(parsed);
  }
}

bool Sc01ApproxSynth::packToken(const std::string &token, uint8_t &code) {
  uint8_t phone, inflection;
  if (!decodeToken(token, phone, inflection)) {
    return false;
  }
  code = phone | (inflection << 6);
  return true;
}

void Sc01ApproxSynth::enqueuePackedPhone(uint8_t code) {
  QueuedPhone parsed;
  parsed.phone = code & 0x3f;
  parsed.inflection = code >> 6;
  queue_.push_back(parsed);
}

void Sc01ApproxSynth::say(const std::string &phonemeSequence) {
  std::string token;
  for (size_t i = 0; i < phonemeSequence.length(); i++) {
    char c = phonemeSequence[i];
    if (std::isspace(static_cast<unsigned char>(c))) {
      flushToken(token);
    } else {
      token.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
  }
  flushToken(token);
}

void Sc01ApproxSynth::flushToken(std::string &token) {
  if (!token.empty()) {
    enqueueToken(token);
    token.clear();
  }
}

bool Sc01ApproxSynth::parseToken(const std::string &token, QueuedPhone &parsed) const {
  return (decodeToken(token, parsed.phone, parsed.inflection));
}

bool Sc01ApproxSynth::busy() const { return (!arState_ || !queue_.empty()); }

int16_t Sc01ApproxSynth::interpolate(int16_t current, uint8_t target) {
  return (static_cast<int16_t>(current - (current >> 3) + (static_cast<int16_t>(target) << 1)));
}

void Sc01ApproxSynth::loadNextPhone() {
  if (!arState_ || queue_.empty()) {
    return;
  }

  QueuedPhone next = queue_.front();
  queue_.pop_front();
  writePhone(next.phone, next.inflection);
}

void Sc01ApproxSynth::writePhone(uint8_t phone, uint8_t inflection) {
  inflection_ = inflection & 0x03;
  phone_ = phone & 0x3F;
  arState_ = false;
  phoneCommit();
}

void Sc01ApproxSynth::phoneCommit() {
  phonetick_ = 0;
  ticks_ = 0;

  const PhoneParams &phone = kPhones[phone_ & 0x3F];
  romF1_ = phone.f1;
  romVa_ = phone.va;
  romF2_ = phone.f2;
  romFc_ = phone.fc;
  romF2q_ = phone.f2q;
  romF3_ = phone.f3;
  romFa_ = phone.fa;
  romCld_ = phone.cld;
  romVd_ = phone.vd;
  romClosure_ = phone.stop;
  romDuration_ = phone.dur;
  romPause_ = phone.pause;

  if (romCld_ == 0) {
    curClosure_ = romClosure_;
  }
}

void Sc01ApproxSynth::chipUpdate() {
  if (ticks_ != 0x10) {
    phonetick_++;
    if (phonetick_ == ((static_cast<uint16_t>(romDuration_) << 2) | 1)) {
      phonetick_ = 0;
      ticks_++;
      if (ticks_ == romCld_) {
        curClosure_ = romClosure_;
      }
    }
  } else if (!arState_) {
    arState_ = true;
  }

  updateCounter_++;
  if (updateCounter_ == 0x30) {
    updateCounter_ = 0;
  }

  const bool tick625 = ((updateCounter_ & 0x0F) == 0);
  const bool tick208 = (updateCounter_ == 0x28);

  if (tick208 && (!romPause_ || !(filtFa_ || filtVa_))) {
    curFc_ = interpolate(curFc_, romFc_);
    curF1_ = interpolate(curF1_, romF1_);
    curF2_ = interpolate(curF2_, romF2_);
    curF2q_ = interpolate(curF2q_, romF2q_);
    curF3_ = interpolate(curF3_, romF3_);
  }

  if (tick625) {
    if (ticks_ >= romVd_) {
      curFa_ = interpolate(curFa_, romFa_);
    }
    if (ticks_ >= romCld_) {
      curVa_ = interpolate(curVa_, romVa_);
    }
  }

  if ((!curClosure_) && (filtFa_ || filtVa_)) {
    closure_ = 0;
  } else if (closure_ != (7 << 2)) {
    closure_++;
  }

  pitch_ = static_cast<uint8_t>((pitch_ + 1) & 0xFF);
  const uint16_t pitchReset =
      static_cast<uint16_t>((0xE0 ^ (inflection_ << 5) ^ (filtF1_ << 1)) + 2);
  if (pitch_ == pitchReset) {
    pitch_ = 0;
  }

  if ((pitch_ & 0xF9) == 0x08) {
    filtersCommit(false);
  }

  const bool inp = curNoise_ && (noise_ != 0x7FFF);
  noise_ = static_cast<uint16_t>(((noise_ << 1) & 0x7FFE) | (inp ? 1 : 0));
  curNoise_ = ((((noise_ >> 14) ^ (noise_ >> 13)) & 1) == 0);
}

void Sc01ApproxSynth::filtersCommit(bool force) {
  (void)force;

  filtFa_ = static_cast<uint8_t>(curFa_ >> 4);
  filtFc_ = static_cast<uint8_t>(curFc_ >> 4);
  filtVa_ = static_cast<uint8_t>(curVa_ >> 4);

  filtF1_ = static_cast<uint8_t>(curF1_ >> 4);
  f1Addr_ = static_cast<uint16_t>(filtF1_ << 3);

  filtF2_ = static_cast<uint8_t>(curF2_ >> 3);
  filtF2q_ = static_cast<uint8_t>(curF2q_ >> 4);
  f2vAddr_ = static_cast<uint16_t>(((filtF2q_ << 5) | filtF2_) << 3);

  filtF3_ = static_cast<uint8_t>(curF3_ >> 4);
  f3Addr_ = static_cast<uint16_t>(filtF3_ << 3);
}

int32_t Sc01ApproxSynth::analogCalc() {
  int32_t voice = (pitch_ >= (9 << 3)) ? 0 : kGlottalWave[pitch_ >> 3];

  voice = fpScale15(voice, filtVa_);
  shiftHistory(voice1_, voice);

  voice = applyFilter(voice1_, voice2_, f1_rom, f1Addr_, F1_FP_FRAC_A, F1_FP_FRAC_B);
  shiftHistory(voice2_, voice);

  voice = applyFilter(voice2_, voice3_, f2v_rom, f2vAddr_, F2V_FP_FRAC_A, F2V_FP_FRAC_B);
  shiftHistory(voice3_, voice);

  int32_t noiseSignal = ((pitch_ & 0x40) && curNoise_) ? 16384 : -16384;
  noiseSignal = fpScale15(noiseSignal, filtFa_);
  shiftHistory(noise1_, noiseSignal);

  noiseSignal = applyFilter(noise1_, noise2_, fn_rom, 0, FN_FP_FRAC_A, FN_FP_FRAC_B);
  shiftHistory(noise2_, noiseSignal);

  const int32_t noise2 = fpScale15(noiseSignal, filtFc_);
  shiftHistory(noise3_, noise2);

  const int32_t noise3 = 0;
  shiftHistory(noise4_, noise3);

  int32_t mixed = voice + noise3;
  shiftHistory(vn1_, mixed);

  mixed = applyFilter(vn1_, vn2_, f3_rom, f3Addr_, F3_FP_FRAC_A, F3_FP_FRAC_B);
  shiftHistory(vn2_, mixed);

  const uint8_t noiseScale = static_cast<uint8_t>(5 + (15 ^ filtFc_));
  mixed += static_cast<int32_t>((static_cast<int64_t>(noiseSignal) * noiseScale * 1638) >>
                                VOTRAX_FP_FRAC);
  shiftHistory(vn3_, mixed);

  mixed = applyFilter(vn3_, vn4_, f4_rom, 0, F4_FP_FRAC_A, F4_FP_FRAC_B);
  shiftHistory(vn4_, mixed);

  mixed = fpScale7(mixed, static_cast<uint8_t>(7 ^ (closure_ >> 2)));
  shiftHistory(vn5_, mixed);

  mixed = applyFilter(vn5_, vn6_, fx_rom, 0, FX_FP_FRAC_A, FX_FP_FRAC_B);
  shiftHistory(vn6_, mixed);

  return (mixed);
}

int16_t Sc01ApproxSynth::renderOneSample() {
  sampleTick_ = !sampleTick_;
  if (sampleTick_) {
    chipUpdate();
  }

  const int32_t analog = analogCalc();
  const int64_t scaled = (static_cast<int64_t>(analog) * 32767) / 91752;
  return (static_cast<int16_t>(clampValue<int64_t>(scaled, -32768, 32767)));
}

void Sc01ApproxSynth::render(int16_t *out, size_t frames) {
  if (out == nullptr) {
    return;
  }

  for (size_t i = 0; i < frames; i++) {
    if (arState_) {
      loadNextPhone();
    }

    if (arState_) {
      out[i] = 0;
    } else {
      out[i] = renderOneSample();
    }
  }
}
} // namespace votrax
