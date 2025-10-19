#include "app_leds.h"

#include <stdint.h>

#include "ZTimer.h"
#include "dbg.h"
#include "device_config.h"
#include "device_definitions.h"
#include "fsl_gpio.h"
#include "fsl_iocon.h"

static void LED_Hardware_Init(LedConfig* led_config);
static void LED_BlinkCallback(void* pvParam);

void LEDS_Hardware_Init() {
    for (uint8_t i = 0; i < LEDS_AMOUNT; i++) {
        LED_Hardware_Init(leds_configs[i]);
    }
}

void LEDS_Timers_Init() {
    DBG_vPrintf(TRACE_LEDS, "LEDS: Configuring Timers...\n");
    for (uint8_t i = 0; i < LEDS_AMOUNT; i++) {
        ZTIMER_eOpen(&leds_configs[i]->u8TimerID, LED_BlinkCallback, leds_configs[i], ZTIMER_FLAG_PREVENT_SLEEP);
        DBG_vPrintf(TRACE_LEDS, "LED(%d): Blink turn off Timer ID - %d\n", leds_configs[i]->u32DioPin, leds_configs[i]->u8TimerID);
    }
    ZTIMER_eOpen(&reset_led_config.u8TimerID, LED_BlinkDuringSetup, NULL, ZTIMER_FLAG_PREVENT_SLEEP);
    DBG_vPrintf(TRACE_LEDS, "LEDS: LEDS_Init finished. Blink during setup Timer ID - %d\n", reset_led_config.u8TimerID);
}

void LED_Blink(LedConfig* led_config) {
    ZTIMER_teState state = ZTIMER_eGetState(reset_led_config.u8TimerID);
    if (state == E_ZTIMER_STATE_RUNNING) {
        DBG_vPrintf(TRACE_LEDS, "LEDS: Blink during setup in progress. Do nothing...");
        return;
    }

    DBG_vPrintf(TRACE_LEDS, "LED(%d): Blink called\n", led_config->u32DioPin);
    GPIO_PortClear(GPIO, 0, 1U << led_config->u32DioPin);
    DBG_vPrintf(TRACE_LEDS, "LED(%d): Starting turn off timer %d\n", led_config->u32DioPin, led_config->u8TimerID);
    ZTIMER_teStatus status = ZTIMER_eStart(led_config->u8TimerID, LED_BLINK_INTERVAL);
    DBG_vPrintf(TRACE_LEDS, "LED(%d): Start turn off timer %d status: %d\n", led_config->u32DioPin, led_config->u8TimerID, status);
}

void LED_TurnOff(LedConfig* led_config) {
    DBG_vPrintf(TRACE_LEDS, "LED(%d): Turn Off called\n", led_config->u32DioPin);
    GPIO_PortSet(GPIO, 0, 1U << led_config->u32DioPin);
}

void LED_BlinkDuringSetup(void* pvParam) {
    if (reset_led_config.u8State) {
        DBG_vPrintf(TRACE_LEDS, "LEDS: Blink during setup. Turning OFF\n");
        GPIO_PortSet(GPIO, 0, reset_led_config.u32Mask);
        reset_led_config.u8State = FALSE;
        ZTIMER_eStart(reset_led_config.u8TimerID, LED_CONTINOUS_BLINK_INTERVAL);
    } else {
        DBG_vPrintf(TRACE_LEDS, "LEDS: Blink during setup. Turning ON\n");
        GPIO_PortClear(GPIO, 0, reset_led_config.u32Mask);
        reset_led_config.u8State = TRUE;
        ZTIMER_eStart(reset_led_config.u8TimerID, LED_CONTINOUS_BLINK_INTERVAL);
    }
}

void LED_BlinkDuringSetup_Stop() {
    DBG_vPrintf(TRACE_LEDS, "LEDS: Blink during setup. Stopping...\n");
    GPIO_PortSet(GPIO, 0, reset_led_config.u32Mask);
    reset_led_config.u8State = FALSE;
    ZTIMER_eStop(reset_led_config.u8TimerID);
}

static void LED_Hardware_Init(LedConfig* led_config) {
    DBG_vPrintf(TRACE_LEDS, "LED(%d): Configuring Hardware...\n", led_config->u32DioPin);
    gpio_pin_config_t led_pin_config = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic = 1U,
    };

    IOCON_PinMuxSet(IOCON, 0, led_config->u32DioPin, IOCON_FUNC0 | IOCON_MODE_INACT | IOCON_DIGITAL_EN);
    GPIO_PinInit(GPIO, 0, led_config->u32DioPin, &led_pin_config);
    DBG_vPrintf(TRACE_LEDS, "LED(%d): Configuring Hardware - finished!\n", led_config->u32DioPin);
}

static void LED_BlinkCallback(void* pvParam) {
    LedConfig* led_config = (LedConfig*)pvParam;
    DBG_vPrintf(TRACE_LEDS, "LED(%d): Blink turn off callback called\n", led_config->u32DioPin);
    LED_TurnOff(led_config);
}