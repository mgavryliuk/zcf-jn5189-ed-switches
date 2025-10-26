#ifndef APP_LED_H
#define APP_LED_H

#include <stdint.h>

#include "device_definitions.h"

#ifdef DEBUG_LEDS
#define TRACE_LEDS TRUE
#else
#define TRACE_LEDS FALSE
#endif

#define LED_BLINK_INTERVAL ZTIMER_TIME_MSEC(100)
#define LED_CONTINOUS_BLINK_INTERVAL ZTIMER_TIME_MSEC(250)

#define LEDS_TIMERS_AMOUNT (1 + LEDS_AMOUNT)

typedef struct {
    const uint32_t u32DioPin;
    uint8_t u8TimerID;
} LedConfig_t;

typedef struct {
    const uint32_t u32Mask;
    uint8_t u8TimerID;
    uint8_t u8State;
} DeviceSetupLedsConfig_t;

void LEDS_Hardware_Init(void);
void LEDS_Timers_Init(void);
void LED_Blink(LedConfig_t* psConfig);
void LED_TurnOff(LedConfig_t* psConfig);
void LED_BlinkDuringSetup(void* pvParam);
void LED_BlinkDuringSetup_Stop(void);
void LED_ButtonBlinkCallback(void* ctx);

#endif /* APP_LED_H */
