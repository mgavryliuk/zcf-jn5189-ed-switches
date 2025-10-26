#include "app_leds.h"

#include <stdint.h>

#include "ZTimer.h"
#include "dbg.h"
#include "device_config.h"
#include "fsl_gpio.h"
#include "fsl_iocon.h"

static void LED_Hardware_Init(LedConfig_t* led_config);
static void LED_BlinkCallback(void* pvParam);

void LEDS_Hardware_Init(void) {
    for (uint8_t i = 0; i < device_config.u8LedsAmount; i++) {
        LED_Hardware_Init(device_config.psLedsConfigs[i]);
    }
}

void LEDS_Timers_Init(void) {
    DBG_vPrintf(TRACE_LEDS, "LEDS: Configuring Timers...\n");
    for (uint8_t i = 0; i < device_config.u8LedsAmount; i++) {
        LedConfig_t* led = device_config.psLedsConfigs[i];
        ZTIMER_eOpen(&led->u8TimerID, LED_BlinkCallback, led, ZTIMER_FLAG_PREVENT_SLEEP);
        DBG_vPrintf(TRACE_LEDS, "LED(%u): Blink turn off Timer ID - %d\n", led->u32DioPin, led->u8TimerID);
    }
    ZTIMER_eOpen(&device_config.sDeviceSetupLedsConfig.u8TimerID, LED_BlinkDuringSetup, NULL, ZTIMER_FLAG_PREVENT_SLEEP);
    DBG_vPrintf(TRACE_LEDS, "LEDS: LEDS_Init finished. Blink during setup Timer ID - %d\n", device_config.sDeviceSetupLedsConfig.u8TimerID);
}

void LED_Blink(LedConfig_t* led_config) {
    ZTIMER_teState state = ZTIMER_eGetState(device_config.sDeviceSetupLedsConfig.u8TimerID);
    if (state == E_ZTIMER_STATE_RUNNING) {
        DBG_vPrintf(TRACE_LEDS, "LEDS: Blink during setup in progress. Do nothing...");
        return;
    }

    DBG_vPrintf(TRACE_LEDS, "LED(%u): Blink called\n", led_config->u32DioPin);
    GPIO_PortClear(GPIO, 0, 1U << led_config->u32DioPin);
    DBG_vPrintf(TRACE_LEDS, "LED(%u): Starting turn off timer %d\n", led_config->u32DioPin, led_config->u8TimerID);
    ZTIMER_teStatus status = ZTIMER_eStart(led_config->u8TimerID, LED_BLINK_INTERVAL);
    DBG_vPrintf(TRACE_LEDS, "LED(%u): Start turn off timer %d status: %d\n", led_config->u32DioPin, led_config->u8TimerID, status);
}

void LED_TurnOff(LedConfig_t* led_config) {
    DBG_vPrintf(TRACE_LEDS, "LED(%u): Turn Off called\n", led_config->u32DioPin);
    GPIO_PortSet(GPIO, 0, 1U << led_config->u32DioPin);
}

void LED_BlinkDuringSetup(void* pvParam) {
    if (device_config.sDeviceSetupLedsConfig.u8State) {
        DBG_vPrintf(TRACE_LEDS, "LEDS: Blink during setup. Turning OFF\n");
        GPIO_PortSet(GPIO, 0, device_config.sDeviceSetupLedsConfig.u32Mask);
        device_config.sDeviceSetupLedsConfig.u8State = FALSE;
        ZTIMER_eStart(device_config.sDeviceSetupLedsConfig.u8TimerID, LED_CONTINOUS_BLINK_INTERVAL);
    } else {
        DBG_vPrintf(TRACE_LEDS, "LEDS: Blink during setup. Turning ON\n");
        GPIO_PortClear(GPIO, 0, device_config.sDeviceSetupLedsConfig.u32Mask);
        device_config.sDeviceSetupLedsConfig.u8State = TRUE;
        ZTIMER_eStart(device_config.sDeviceSetupLedsConfig.u8TimerID, LED_CONTINOUS_BLINK_INTERVAL);
    }
}

void LED_BlinkDuringSetup_Stop(void) {
    DBG_vPrintf(TRACE_LEDS, "LEDS: Blink during setup. Stopping...\n");
    GPIO_PortSet(GPIO, 0, device_config.sDeviceSetupLedsConfig.u32Mask);
    device_config.sDeviceSetupLedsConfig.u8State = FALSE;
    ZTIMER_eStop(device_config.sDeviceSetupLedsConfig.u8TimerID);
}

void LED_ButtonBlinkCallback(void* ctx) {
    LedConfig_t* led = (LedConfig_t*)ctx;
    DBG_vPrintf(TRACE_LEDS, "LED(%u): Button blink callback called...\n", led->u32DioPin);
    LED_Blink(led);
}

static void LED_Hardware_Init(LedConfig_t* led_config) {
    DBG_vPrintf(TRACE_LEDS, "LED(%u): Configuring Hardware...\n", led_config->u32DioPin);
    gpio_pin_config_t led_pin_config = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic = 1U,
    };

    IOCON_PinMuxSet(IOCON, 0, led_config->u32DioPin, IOCON_FUNC0 | IOCON_MODE_INACT | IOCON_DIGITAL_EN);
    GPIO_PinInit(GPIO, 0, led_config->u32DioPin, &led_pin_config);
    DBG_vPrintf(TRACE_LEDS, "LED(%u): Configuring Hardware - finished!\n", led_config->u32DioPin);
}

static void LED_BlinkCallback(void* pvParam) {
    LedConfig_t* led_config = (LedConfig_t*)pvParam;
    DBG_vPrintf(TRACE_LEDS, "LED(%u): Blink turn off callback called\n", led_config->u32DioPin);
    LED_TurnOff(led_config);
}
