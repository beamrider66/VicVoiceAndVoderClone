#include "audio_outputs.h"
#include "config.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/ledc.h>

extern "C" void vvvc_pwm_duty(uint8_t duty);

namespace pwm_audio {
namespace {
constexpr size_t capacity = 4096;
constexpr size_t mask = capacity - 1;
constexpr ledc_mode_t speed = LEDC_LOW_SPEED_MODE;
constexpr ledc_channel_t channel = LEDC_CHANNEL_0;
// All state touched by the interrupt lives in internal RAM. The ISR neither
// allocates nor calls the (locking, flash-resident) Arduino ledcWrite API.
DRAM_ATTR uint8_t samples[capacity];
volatile uint32_t head = 0;
volatile uint32_t tail = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
hw_timer_t *sampleTimer = nullptr;
bool ready = false;

static_assert(config::audioPwmPin != config::vicRxPin &&
              config::audioPwmPin != config::vicTxPin, "PWM and VIC UART pins must differ");
#if VVVC_ES8388
static_assert(config::audioPwmPin != config::codecSclPin &&
              config::audioPwmPin != config::codecSdaPin &&
              config::audioPwmPin != config::audioMclkPin &&
              config::audioPwmPin != config::audioBclkPin &&
              config::audioPwmPin != config::audioLrclkPin &&
              config::audioPwmPin != config::audioDataPin &&
              config::audioPwmPin != config::speakerEnablePin, "PWM conflicts with codec wiring");
#endif

void IRAM_ATTR tick() {
  portENTER_CRITICAL_ISR(&mux);
  uint8_t duty = 128; // Silence is mid-rail, not zero volts.
  if (head != tail)
    duty = samples[tail++ & mask];
  vvvc_pwm_duty(duty);
  portEXIT_CRITICAL_ISR(&mux);
}

bool stalled() {
  Serial.println("ERROR: PWM sample timer stalled; reset to retry.");
  timerAlarmDisable(sampleTimer);
  silence();
  timerEnd(sampleTimer);
  sampleTimer = nullptr;
  ready = false;
  return false;
}
} // namespace

bool begin() {
  if (ready)
    return true;
  if (!GPIO_IS_VALID_OUTPUT_GPIO(config::audioPwmPin)) {
    Serial.println("ERROR: selected PWM pin is not an output GPIO.");
    return false;
  }
  ledc_timer_config_t timer = {};
  timer.speed_mode = speed;
  timer.duty_resolution = LEDC_TIMER_8_BIT;
  timer.timer_num = LEDC_TIMER_0;
  timer.freq_hz = config::audioPwmCarrierHz;
  timer.clk_cfg = LEDC_USE_APB_CLK;
  ledc_channel_config_t output = {};
  output.gpio_num = config::audioPwmPin;
  output.speed_mode = speed;
  output.channel = channel;
  output.intr_type = LEDC_INTR_DISABLE;
  output.timer_sel = LEDC_TIMER_0;
  output.duty = 128;
  if (ledc_timer_config(&timer) != ESP_OK || ledc_channel_config(&output) != ESP_OK) {
    Serial.println("ERROR: PWM output could not be initialized.");
    return false;
  }
  // APB/2 gives fine sample timing: 907 ticks at 40 MHz = 44101.43 Hz.
  // This is within 33 ppm of the codec's 44100 Hz clock.
  sampleTimer = timerBegin(0, 2, true);
  if (!sampleTimer) {
    Serial.println("ERROR: PWM sample timer could not be initialized.");
    return false;
  }
  head = tail = 0;
  timerAttachInterrupt(sampleTimer, tick, false);
  const uint32_t ticks = (getApbFrequency() / 2 + config::audioSampleRate / 2) /
                         config::audioSampleRate;
  timerAlarmWrite(sampleTimer, ticks, true);
  timerAlarmEnable(sampleTimer);
  ready = true;
  Serial.printf("PWM ready: GPIO %d, 8-bit, %lu Hz carrier; filter + amplifier required.\n",
                config::audioPwmPin, static_cast<unsigned long>(config::audioPwmCarrierHz));
  return true;
}

bool write(const int16_t *mono, size_t frames) {
  if (!ready)
    return false;
  uint32_t progress = millis();
  while (frames) {
    // Short critical sections keep the 44.1 kHz ISR responsive on both cores.
    uint8_t block[64];
    size_t count = frames > sizeof(block) ? sizeof(block) : frames;
    for (size_t i = 0; i < count; ++i)
      block[i] = static_cast<uint8_t>((static_cast<int32_t>(mono[i]) + 32768) >> 8);
    portENTER_CRITICAL(&mux);
    size_t free = capacity - static_cast<uint32_t>(head - tail);
    if (count > free)
      count = free;
    for (size_t i = 0; i < count; ++i)
      samples[head++ & mask] = block[i];
    portEXIT_CRITICAL(&mux);
    mono += count;
    frames -= count;
    if (count)
      progress = millis();
    else {
      if (millis() - progress > 250)
        return stalled();
      delay(1);
    }
  }
  return true;
}

bool finish() {
  if (!ready)
    return false;
  const uint32_t start = millis();
  for (;;) {
    portENTER_CRITICAL(&mux);
    bool empty = head == tail;
    portEXIT_CRITICAL(&mux);
    if (empty) {
      // Allow the last sample one full period, then leave a midpoint duty.
      delay(1);
      return true;
    }
    if (millis() - start > 250)
      return stalled();
    delay(1);
  }
}

void silence() {
  portENTER_CRITICAL(&mux);
  tail = head;
  vvvc_pwm_duty(128);
  portEXIT_CRITICAL(&mux);
}
} // namespace pwm_audio
