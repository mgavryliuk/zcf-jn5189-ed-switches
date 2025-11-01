#ifndef APP_LEDS_H
#define APP_LEDS_H

#include "dbg.h"
#include "device_definitions.h"

#ifdef DEBUG_LEDS
#define TRACE_LEDS TRUE
#else
#define TRACE_LEDS FALSE
#endif

#define LEDS_DBG(...) DBG_vPrintf(TRACE_LEDS, "[LEDS] " __VA_ARGS__)

#define LEDS_BLINK_INTERVAL ZTIMER_TIME_MSEC(100)
#define LEDS_CONTINOUS_BLINK_INTERVAL ZTIMER_TIME_MSEC(150)

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
void LEDS_Blink(LedConfig_t* psConfig);
void LEDS_TurnOff(LedConfig_t* psConfig);
void LEDS_BlinkDuringNetworkSetup_Start(void* pvParam);
void LEDS_BlinkDuringNetworkSetup_Stop(void);
void LEDS_ButtonBlinkCallback(void* ctx);

#endif /* APP_LEDS_H */
