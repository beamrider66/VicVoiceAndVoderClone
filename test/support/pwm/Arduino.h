#pragma once
#include "../Arduino.h"
#define IRAM_ATTR
#define DRAM_ATTR
#define portMUX_INITIALIZER_UNLOCKED 0
using portMUX_TYPE = int;
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define portENTER_CRITICAL_ISR(x) ((void)(x))
#define portEXIT_CRITICAL_ISR(x) ((void)(x))
struct hw_timer_t {};
hw_timer_t *timerBegin(int, int, bool);
void timerAttachInterrupt(hw_timer_t *, void (*)(), bool);
void timerAlarmWrite(hw_timer_t *, uint32_t, bool);
void timerAlarmEnable(hw_timer_t *);
void timerAlarmDisable(hw_timer_t *);
void timerEnd(hw_timer_t *);
inline uint32_t getApbFrequency() { return 80000000; }
