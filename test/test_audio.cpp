#include "audio_outputs.h"
#include <Wire.h>
namespace audio = codec_audio;
TestWire Wire;
#include "config.h"
#include <Arduino.h>
#include <AudioBoard.h>
#include <cassert>
#include <driver/i2s.h>
#include <iostream>
#include <vector>

HardwareSerial *HardwareSerial::ports[3] = {};
HardwareSerial Serial(0);
namespace audio_driver {
CodecState testCodec;
int AudioDriverES8388 = 0;
Logger AudioDriverLogger;
} // namespace audio_driver
using audio_driver::testCodec;

static i2s_config_t settings;
static i2s_pin_config_t pins;
static bool installed = false;
static int installResult = ESP_OK, pinResult = ESP_OK, zeroResult = ESP_OK;
static int writeResult = ESP_OK, clearCalls = 0;
static size_t maxWrite = 256;
static std::vector<uint8_t> output;

int i2s_driver_install(i2s_port_t port, const i2s_config_t *config, int, void *) {
  assert(port == I2S_NUM_0 && !installed);
  assert(testCodec.muted && !testCodec.powered);
  settings = *config;
  installed = installResult == ESP_OK;
  return installResult;
}
int i2s_driver_uninstall(i2s_port_t) {
  assert(installed);
  installed = false;
  return ESP_OK;
}
int i2s_set_pin(i2s_port_t, const i2s_pin_config_t *value) {
  assert(installed);
  pins = *value;
  return pinResult;
}
int i2s_zero_dma_buffer(i2s_port_t) {
  assert(installed);
  ++clearCalls;
  return zeroResult;
}
int i2s_write(i2s_port_t, const void *data, size_t bytes, size_t *written, TickType_t wait) {
  assert(installed && !testCodec.muted && testCodec.powered && wait > 0);
  *written = writeResult == ESP_OK ? std::min(bytes, maxWrite) : 0;
  auto start = static_cast<const uint8_t *>(data);
  output.insert(output.end(), start, start + *written);
  return writeResult;
}

static std::vector<int16_t> samples() {
  assert(output.size() % sizeof(int16_t) == 0);
  std::vector<int16_t> values(output.size() / sizeof(int16_t));
  memcpy(values.data(), output.data(), output.size());
  return values;
}

int main() {
  Wire.present = false;
  assert(!audio::begin() && !installed);
  Wire.present = true;
  testCodec.beginOk = false;
  assert(!audio::begin() && !installed && !testCodec.powered);
  assert(!audio::write(nullptr, 0) && output.empty());
  testCodec.beginOk = true;
  installResult = ESP_FAIL;
  assert(!audio::begin() && !installed && testCodec.muted);
  installResult = ESP_OK;
  pinResult = ESP_FAIL;
  assert(!audio::begin() && !installed && !testCodec.powered);
  pinResult = ESP_OK;
  zeroResult = ESP_FAIL;
  assert(!audio::begin() && !installed && !testCodec.powered);
  zeroResult = ESP_OK;
  assert(audio::begin() && installed && !testCodec.muted && testCodec.powered);
  assert(audio::begin()); // Already initialized: do not install the driver again.
  assert(settings.sample_rate == 44100 && settings.bits_per_sample == 16);
  assert(settings.mode == (I2S_MODE_MASTER | I2S_MODE_TX));
  assert(settings.channel_format == I2S_CHANNEL_FMT_RIGHT_LEFT);
  assert(settings.communication_format == I2S_COMM_FORMAT_STAND_I2S);
  assert(settings.tx_desc_auto_clear);
  assert(testCodec.config.output_device == audio_driver::DAC_OUTPUT_ALL);
  assert(testCodec.config.i2s.bits == 16 && testCodec.config.i2s.rate == 44100);
  assert(testCodec.config.i2s.channels == 2 && !testCodec.config.sd_active);
  assert(testCodec.volume == 48 && testCodec.pins.pa == 21);
  assert(testCodec.pins.scl == 32 && testCodec.pins.sda == 33);
  assert(pins.mck_io_num == 0 && pins.bck_io_num == 27 && pins.ws_io_num == 25);
  assert(pins.data_out_num == 26 && pins.data_in_num == I2S_PIN_NO_CHANGE);

  // Full signed 16-bit amplitude survives block boundaries and short writes.
  std::vector<int16_t> mono = {-32768, -16384, -1, 0, 1, 1234, 32767};
  for (int i = 0; i < 140; ++i)
    mono.push_back(static_cast<int16_t>(i * 431 - 30000));
  maxWrite = 7;
  assert(audio::write(mono.data(), mono.size()));
  auto pcm = samples();
  assert(pcm.size() == mono.size() * 2);
  for (size_t i = 0; i < mono.size(); ++i)
    assert(pcm[2 * i] == mono[i] && pcm[2 * i + 1] == mono[i]);
  maxWrite = 256;
  const int clearsBeforeFinish = clearCalls;
  assert(audio::finish() && clearCalls == clearsBeforeFinish);
  pcm = samples();
  assert(pcm.size() > mono.size() * 2 + settings.dma_buf_count * settings.dma_buf_len * 2);
  for (size_t i = mono.size() * 2; i < pcm.size(); ++i)
    assert(pcm[i] == 0);
  audio::silence();
  assert(clearCalls == clearsBeforeFinish + 1);

  // A broken/stalled driver fails promptly and disables the speaker amplifier.
  writeResult = ESP_FAIL;
  assert(!audio::write(mono.data(), mono.size()));
  assert(!installed && testCodec.muted && !testCodec.powered);
  writeResult = ESP_OK;
  assert(audio::begin());
  maxWrite = 0;
  assert(!audio::write(mono.data(), mono.size()) && !installed);
  assert(!audio::finish());
  std::cout << "DAC audio checks passed.\n";
}
