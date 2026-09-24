#pragma once
#include <stdint.h>
enum ledc_mode_t { LEDC_LOW_SPEED_MODE };
enum ledc_channel_t { LEDC_CHANNEL_0 };
enum { LEDC_TIMER_8_BIT = 8, LEDC_TIMER_0 = 0, LEDC_USE_APB_CLK = 1,
       LEDC_INTR_DISABLE = 0, ESP_OK = 0 };
struct ledc_timer_config_t {
  ledc_mode_t speed_mode;
  int duty_resolution, timer_num;
  uint32_t freq_hz;
  int clk_cfg;
};
struct ledc_channel_config_t {
  int gpio_num;
  ledc_mode_t speed_mode;
  ledc_channel_t channel;
  int intr_type, timer_sel, duty;
};
int ledc_timer_config(const ledc_timer_config_t *);
int ledc_channel_config(const ledc_channel_config_t *);
