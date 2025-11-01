#include "app_leds.h"

#include "ZTimer.h"
#include "device_config.h"
#include "fsl_gpio.h"
#include "fsl_iocon.h"

static void LEDS_BlinkCallback(void* pvParam);

void LEDS_Hardware_Init(void) {
    gpio_pin_config_t led_pin_config = {
        .pinDirection = kGPIO_DigitalOutput,
        .outputLogic = 1U,
    };

    for (uint8_t i = 0; i < device_config.u8LedsAmount; i++) {
        LedConfig_t* led_config = device_config.psLedsConfigs[i];
        LEDS_DBG("LED(%u). Configuring Hardware...\n", led_config->u32DioPin);
        IOCON_PinMuxSet(IOCON, 0, led_config->u32DioPin, IOCON_FUNC0 | IOCON_MODE_INACT | IOCON_DIGITAL_EN);
        GPIO_PinInit(GPIO, 0, led_config->u32DioPin, &led_pin_config);
    }
}

void LEDS_Timers_Init(void) {
    LEDS_DBG("Configuring Timers...\n");
    for (uint8_t i = 0; i < device_config.u8LedsAmount; i++) {
        LedConfig_t* led = device_config.psLedsConfigs[i];
        ZTIMER_eOpen(&led->u8TimerID, LEDS_BlinkCallback, led, ZTIMER_FLAG_PREVENT_SLEEP);
        LEDS_DBG("LED(%u). Blink turn off Timer ID - %d\n", led->u32DioPin, led->u8TimerID);
    }
    ZTIMER_eOpen(&device_config.sDeviceSetupLedsConfig.u8TimerID, LEDS_BlinkDuringNetworkSetup_Start, NULL, ZTIMER_FLAG_PREVENT_SLEEP);
    LEDS_DBG("LEDS_Init finished. Blink during setup Timer ID - %d\n", device_config.sDeviceSetupLedsConfig.u8TimerID);
}

void LEDS_Blink(LedConfig_t* led_config) {
    ZTIMER_teState state = ZTIMER_eGetState(device_config.sDeviceSetupLedsConfig.u8TimerID);
    if (state == E_ZTIMER_STATE_RUNNING) {
        LEDS_DBG("Blink during setup in progress. Do nothing...");
        return;
    }

    LEDS_DBG("LED(%u). Blink called\n", led_config->u32DioPin);
    GPIO_PortClear(GPIO, 0, 1U << led_config->u32DioPin);
    LEDS_DBG("LED(%u). Starting turn off timer %d\n", led_config->u32DioPin, led_config->u8TimerID);
    ZTIMER_teStatus status = ZTIMER_eStart(led_config->u8TimerID, LEDS_BLINK_INTERVAL);
    LEDS_DBG("LED(%u). Start turn off timer %d status: %d\n", led_config->u32DioPin, led_config->u8TimerID, status);
}

void LEDS_TurnOff(LedConfig_t* led_config) {
    LEDS_DBG("LED(%u). Turn Off called\n", led_config->u32DioPin);
    GPIO_PortSet(GPIO, 0, 1U << led_config->u32DioPin);
}

void LEDS_BlinkDuringNetworkSetup_Start(void* pvParam) {
    if (device_config.sDeviceSetupLedsConfig.u8State) {
        LEDS_DBG("Blink during setup. Turning OFF\n");
        GPIO_PortSet(GPIO, 0, device_config.sDeviceSetupLedsConfig.u32Mask);
        device_config.sDeviceSetupLedsConfig.u8State = FALSE;
        ZTIMER_eStart(device_config.sDeviceSetupLedsConfig.u8TimerID, LEDS_CONTINOUS_BLINK_INTERVAL);
    } else {
        LEDS_DBG("Blink during setup. Turning ON\n");
        GPIO_PortClear(GPIO, 0, device_config.sDeviceSetupLedsConfig.u32Mask);
        device_config.sDeviceSetupLedsConfig.u8State = TRUE;
        ZTIMER_eStart(device_config.sDeviceSetupLedsConfig.u8TimerID, LEDS_CONTINOUS_BLINK_INTERVAL);
    }
}

void LEDS_BlinkDuringNetworkSetup_Stop(void) {
    LEDS_DBG("Blink during setup. Stopping...\n");
    GPIO_PortSet(GPIO, 0, device_config.sDeviceSetupLedsConfig.u32Mask);
    device_config.sDeviceSetupLedsConfig.u8State = FALSE;
    ZTIMER_eStop(device_config.sDeviceSetupLedsConfig.u8TimerID);
}

void LEDS_ButtonBlinkCallback(void* ctx) {
    LedConfig_t* led = (LedConfig_t*)ctx;
    LEDS_DBG("LED(%u). Button blink callback called...\n", led->u32DioPin);
    LEDS_Blink(led);
}

static void LEDS_BlinkCallback(void* pvParam) {
    LedConfig_t* led_config = (LedConfig_t*)pvParam;
    LEDS_DBG("LED(%u). Blink turn off callback called\n", led_config->u32DioPin);
    LEDS_TurnOff(led_config);
}
