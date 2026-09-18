#pragma once
#include <cstddef>
#include <cstdint>

using i2s_port_t = int;
using i2s_mode_t = int;
using TickType_t = uint32_t;
constexpr int I2S_NUM_0 = 0, ESP_OK = 0, ESP_FAIL = -1;
constexpr int I2S_MODE_MASTER = 1, I2S_MODE_TX = 2;
constexpr int I2S_BITS_PER_SAMPLE_16BIT = 16, I2S_CHANNEL_FMT_RIGHT_LEFT = 0;
constexpr int I2S_COMM_FORMAT_STAND_I2S = 1, ESP_INTR_FLAG_LEVEL1 = 1;
constexpr int I2S_PIN_NO_CHANGE = -1;
inline TickType_t pdMS_TO_TICKS(uint32_t ms) { return ms; }
struct i2s_config_t {
  i2s_mode_t mode;
  uint32_t sample_rate;
  int bits_per_sample, channel_format, communication_format, intr_alloc_flags;
  int dma_buf_count, dma_buf_len;
  bool use_apll, tx_desc_auto_clear;
  int fixed_mclk;
};
struct i2s_pin_config_t {
  int mck_io_num, bck_io_num, ws_io_num, data_out_num, data_in_num;
};
int i2s_driver_install(i2s_port_t, const i2s_config_t *, int, void *);
int i2s_driver_uninstall(i2s_port_t);
int i2s_set_pin(i2s_port_t, const i2s_pin_config_t *);
int i2s_zero_dma_buffer(i2s_port_t);
int i2s_write(i2s_port_t, const void *, size_t, size_t *, TickType_t);
