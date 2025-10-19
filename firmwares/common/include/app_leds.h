#ifndef APP_LED_H
#define APP_LED_H

#include <stdint.h>

#ifdef DEBUG_LEDS
#define TRACE_LEDS TRUE
#else
#define TRACE_LEDS FALSE
#endif

#define LED_BLINK_INTERVAL ZTIMER_TIME_MSEC(1000)
#define LED_CONTINOUS_BLINK_INTERVAL ZTIMER_TIME_MSEC(250)

#define LEDS_TIMERS_AMOUN (1 + LEDS_AMOUNT)

typedef struct {
    const uint32_t u32DioPin;
    uint8_t u8TimerID;
} LedConfig;

typedef struct {
    const uint32_t u32Mask;
    uint8_t u8TimerID;
    uint8_t u8State;
} ResetLedConfig;

void LEDS_Hardware_Init();
void LEDS_Timers_Init();
void LED_Blink(LedConfig* psConfig);
void LED_TurnOff(LedConfig* psConfig);
void LED_BlinkDuringSetup(void* pvParam);
void LED_BlinkDuringSetup_Stop();

#endif /* APP_LED_H */
