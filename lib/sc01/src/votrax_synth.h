#ifndef _VOTRAX_SYNTH_H_
#define _VOTRAX_SYNTH_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

namespace votrax {
static const int VOTRAX_SAMPLE_RATE_HZ = 40000;

struct PhoneParams {
  const char *name;

  uint8_t va;  /* voiced amplitude      0..15 */
  uint8_t vd;  /* voiced onset delay    ticks */
  uint8_t fa;  /* fric/noise amplitude  0..15 */
  uint8_t cld; /* closure delay         ticks */
  uint8_t fc;  /* fundamental control   0..255-ish mapped */
  uint8_t f1;  /* formant 1 control */
  uint8_t f2;  /* formant 2 control */
  uint8_t f2q; /* formant 2 bandwidth / damping control */
  uint8_t f3;  /* formant 3 control */
  uint8_t dur; /* total phone duration in ticks */
  bool pause;
  bool stop; /* SC-01 closure bit */
  bool voiced;
  bool noise;
};

class Sc01ApproxSynth {
public:
  void begin();
  void reset();

  void enqueueToken(const std::string &token);
  // Packed SC-01 phone: low six bits are the phone, high two are inflection.
  void enqueuePackedPhone(uint8_t code);
  void say(const std::string &phonemeSequence);
  void render(int16_t *out, size_t frames);
  bool busy() const;

  static const PhoneParams *lookupPhone(const std::string &token);
  static bool packToken(const std::string &token, uint8_t &code);

private:
  struct QueuedPhone {
    uint8_t phone = 0;
    uint8_t inflection = 0;
  };

  void flushToken(std::string &token);
  bool parseToken(const std::string &token, QueuedPhone &parsed) const;
  void loadNextPhone();
  void writePhone(uint8_t phone, uint8_t inflection);
  void phoneCommit();
  void chipUpdate();
  void filtersCommit(bool force);
  int16_t renderOneSample();
  int32_t analogCalc();
  void clearHistories();

  static int16_t interpolate(int16_t current, uint8_t target);

  std::deque<QueuedPhone> queue_;

  uint8_t inflection_ = 0;
  uint8_t phone_ = 0x3F;
  bool arState_ = true;
  bool sampleTick_ = false;

  uint8_t romDuration_ = 0;
  uint8_t romVd_ = 0;
  uint8_t romCld_ = 0;
  uint8_t romFa_ = 0;
  uint8_t romFc_ = 0;
  uint8_t romVa_ = 0;
  uint8_t romF1_ = 0;
  uint8_t romF2_ = 0;
  uint8_t romF2q_ = 0;
  uint8_t romF3_ = 0;
  bool romClosure_ = false;
  bool romPause_ = false;

  int16_t curFa_ = 0;
  int16_t curFc_ = 0;
  int16_t curVa_ = 0;
  int16_t curF1_ = 0;
  int16_t curF2_ = 0;
  int16_t curF2q_ = 0;
  int16_t curF3_ = 0;

  uint8_t filtFa_ = 0;
  uint8_t filtFc_ = 0;
  uint8_t filtVa_ = 0;
  uint8_t filtF1_ = 0;
  uint8_t filtF2_ = 0;
  uint8_t filtF2q_ = 0;
  uint8_t filtF3_ = 0;

  uint16_t phonetick_ = 0;
  uint8_t ticks_ = 0;
  uint8_t pitch_ = 0;
  uint8_t closure_ = 0;
  uint8_t updateCounter_ = 0;
  bool curClosure_ = true;
  uint16_t noise_ = 0;
  bool curNoise_ = false;

  uint16_t f1Addr_ = 0;
  uint16_t f2vAddr_ = 0;
  uint16_t f3Addr_ = 0;

  int32_t voice1_[4] = {};
  int32_t voice2_[4] = {};
  int32_t voice3_[4] = {};
  int32_t noise1_[3] = {};
  int32_t noise2_[3] = {};
  int32_t noise3_[2] = {};
  int32_t noise4_[2] = {};
  int32_t vn1_[4] = {};
  int32_t vn2_[4] = {};
  int32_t vn3_[4] = {};
  int32_t vn4_[4] = {};
  int32_t vn5_[2] = {};
  int32_t vn6_[2] = {};
};
} // namespace votrax

#endif
