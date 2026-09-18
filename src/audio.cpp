#include "audio.h"
#include "config.h"
#include <Arduino.h>
#include <AudioBoard.h>
#include <driver/i2s.h>

namespace audio {
namespace {
using namespace audio_driver;
constexpr i2s_port_t port = I2S_NUM_0;
constexpr size_t blockFrames = 64;
constexpr size_t dmaBuffers = 8;
constexpr size_t dmaFrames = 128;
static_assert(config::audioSampleRate == 44100, "ES8388 configuration requires 44.1 kHz");
static_assert(config::audioVolumePercent >= 0 && config::audioVolumePercent <= 100,
              "Audio volume must be 0..100 percent");

// Same codec pins as AudioKitEs8388V1, without unused SD, buttons or input pins.
struct CodecPins : DriverPins {
  CodecPins() {
    addI2C(PinFunction::CODEC, config::codecSclPin, config::codecSdaPin);
    addI2S(PinFunction::CODEC, config::audioMclkPin, config::audioBclkPin, config::audioLrclkPin,
           config::audioDataPin, -1);
    addPin(PinFunction::PA, config::speakerEnablePin, PinLogic::Output);
  }
} codecPins;
AudioBoard codec{AudioDriverES8388, codecPins};
bool ready = false;
bool installed = false;

bool fail(const char *message) {
  Serial.println(message);
  ready = false;
  codec.setMute(true);
  codec.setPAPower(false);
  if (installed) {
    i2s_driver_uninstall(port);
    installed = false;
  }
  return false;
}

bool zeros(size_t frames) {
  const int16_t pcm[blockFrames] = {};
  while (frames) {
    size_t block = frames > blockFrames ? blockFrames : frames;
    if (!write(pcm, block))
      return false;
    frames -= block;
  }
  return true;
}

bool playTone(uint32_t frequency, uint32_t durationMs) {
  int16_t pcm[blockFrames];
  uint32_t phase = 0;
  const uint32_t step = (static_cast<uint64_t>(frequency) << 32) / config::audioSampleRate;
  size_t remaining = config::audioSampleRate * durationMs / 1000;
  while (remaining) {
    size_t block = remaining > blockFrames ? blockFrames : remaining;
    for (size_t i = 0; i < block; ++i) {
      pcm[i] = (phase & 0x80000000UL) ? 5000 : -5000;
      phase += step;
    }
    if (!write(pcm, block))
      return false;
    remaining -= block;
  }
  return true;
}
} // namespace

bool begin() {
  if (ready)
    return true;
  AudioDriverLogger.begin(Serial, AudioDriverLogLevel::Warning);
  CodecConfig settings;
  settings.input_device = ADC_INPUT_LINE1;
  settings.output_device = DAC_OUTPUT_ALL;
  settings.sd_active = false;
  settings.i2s.bits = BIT_LENGTH_16BITS;
  settings.i2s.rate = RATE_44K;
  settings.i2s.channels = CHANNELS2;
  settings.i2s.fmt = I2S_NORMAL;
  settings.i2s.mode = MODE_SLAVE;
  if (!codec.begin(settings)) {
    Serial.println("ERROR: ES8388 codec could not be initialized. Check the audio board.");
    return false;
  }
  // Keep the outputs quiet until the I2S clock and zeroed DMA buffers are ready.
  if (!codec.setMute(true) || !codec.setPAPower(false) ||
      !codec.setVolume(config::audioVolumePercent))
    return fail("ERROR: ES8388 output configuration failed.");

  i2s_config_t i2s = {};
  i2s.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  i2s.sample_rate = config::audioSampleRate;
  i2s.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2s.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  i2s.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2s.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2s.dma_buf_count = dmaBuffers;
  i2s.dma_buf_len = dmaFrames;
  i2s.tx_desc_auto_clear = true;
  if (i2s_driver_install(port, &i2s, 0, nullptr) != ESP_OK)
    return fail("ERROR: ESP32 I2S output could not be initialized.");
  installed = true;

  i2s_pin_config_t pins = {};
  pins.mck_io_num = config::audioMclkPin;
  pins.bck_io_num = config::audioBclkPin;
  pins.ws_io_num = config::audioLrclkPin;
  pins.data_out_num = config::audioDataPin;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  if (i2s_set_pin(port, &pins) != ESP_OK || i2s_zero_dma_buffer(port) != ESP_OK)
    return fail("ERROR: ESP32 I2S pin/buffer configuration failed.");
  if (!codec.setMute(false) || !codec.setPAPower(true))
    return fail("ERROR: ES8388 speaker output could not be enabled.");
  ready = true;
  Serial.printf("ES8388 ready: 44.1 kHz, 16-bit stereo, speaker/headphone volume %d%%.\n",
                config::audioVolumePercent);
  return true;
}

bool write(const int16_t *mono, size_t frames) {
  if (!ready) {
    Serial.println("ERROR: audio unavailable; check the ES8388 board and reset.");
    return false;
  }
  int16_t stereo[blockFrames * 2];
  while (frames) {
    size_t block = frames > blockFrames ? blockFrames : frames;
    for (size_t i = 0; i < block; ++i) {
      stereo[2 * i] = mono[i];
      stereo[2 * i + 1] = mono[i];
    }
    const uint8_t *next = reinterpret_cast<const uint8_t *>(stereo);
    size_t remaining = block * 2 * sizeof(int16_t);
    while (remaining) {
      size_t written = 0;
      if (i2s_write(port, next, remaining, &written, pdMS_TO_TICKS(100)) != ESP_OK || !written ||
          written > remaining)
        return fail("ERROR: I2S audio write failed; reset to retry.");
      next += written;
      remaining -= written;
    }
    mono += block;
    frames -= block;
  }
  return true;
}

bool finish() {
  // Queue more than a DMA ring of silence. Blocking writes let every preceding
  // speech sample reach the DAC before returning, without cutting off its tail.
  return zeros((dmaBuffers + 1) * dmaFrames);
}

void silence() {
  if (ready && i2s_zero_dma_buffer(port) != ESP_OK)
    fail("ERROR: I2S audio could not be stopped; reset to retry.");
}

bool test() {
  // Original startup test: 880 Hz, 100 ms gap, 440 Hz; both tones last 300 ms.
  return playTone(880, 300) && zeros(config::audioSampleRate / 10) && playTone(440, 300) &&
         finish();
}
} // namespace audio
