// IDF 4.4's low-level LEDC headers are C-only. Keep the tiny ISR register
// operation here; no driver locks or flash-resident calls occur per sample.
#include <stdint.h>
#include <esp_attr.h>
#include <hal/ledc_ll.h>
#include <soc/ledc_struct.h>

void IRAM_ATTR vvvc_pwm_duty(uint8_t duty) {
  ledc_ll_set_duty_int_part(&LEDC, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
  ledc_ll_set_duty_start(&LEDC, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, true);
  ledc_ll_ls_channel_update(&LEDC, LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
