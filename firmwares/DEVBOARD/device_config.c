#include "device_config.h"

#include "app_leds.h"
#include "device_definitions.h"
#include "zps_gen.h"

LedConfig_t leftLedConfig = {.u32DioPin = LED_LEFT_DIO};
LedConfig_t rightLedConfig = {.u32DioPin = LED_RIGHT_DIO};
LedConfig_t* const ledsConfigs[LEDS_AMOUNT] = {&leftLedConfig, &rightLedConfig};

Button_t leftButton = {
    .u16Endpoint = DEVBOARD_LEFTBUTTON_ENDPOINT,
    .u32DioPin = BTN_LEFT_DIO,
    .u32DioMask = BTN_LEFT_MASK,

    .bPressed = FALSE,
    .u16PressedCycles = 0,
    .u8Debounce = BUTTON_DEBOUNCE_MASK,
    .eState = IDLE,

    .pfOnPressCallback = LEDS_ButtonBlinkCallback,
    .pvOnPressContext = &leftLedConfig,
};
Button_t rightButton = {
    .u16Endpoint = DEVBOARD_RIGHTBUTTON_ENDPOINT,
    .u32DioPin = BTN_RIGHT_DIO,
    .u32DioMask = BTN_RIGHT_MASK,

    .bPressed = FALSE,
    .u16PressedCycles = 0,
    .u8Debounce = BUTTON_DEBOUNCE_MASK,
    .eState = IDLE,

    .pfOnPressCallback = LEDS_ButtonBlinkCallback,
    .pvOnPressContext = &rightLedConfig,
};
Button_t* const buttons[] = {&leftButton, &rightButton};

DeviceConfig_t device_config = {
    .u8BasicEndpoint = DEVBOARD_BASIC_ENDPOINT,
    .u8ZdoEndpoint = DEVBOARD_ZDO_ENDPOINT,
    .bIsJoined = FALSE,

    .psLedsConfigs = ledsConfigs,
    .u8LedsAmount = LEDS_AMOUNT,

    .sDeviceSetupLedsConfig = {.u32Mask = RESET_LED_MASK},

    .psButtons = buttons,
    .u8ButtonsAmount = BUTTONS_AMOUNT,

    .u32ButtonsInterruptMask = BTN_INTERRUPT_MASK,
    .sResetMaskConfig =
        {
            .u32DioMask = BTN_RESET_MASK,
            .bPressed = FALSE,
            .u16PressedCycles = 0,
            .u8Debounce = BUTTON_DEBOUNCE_MASK,
        },
    .sDeviceBattery =
        {
            .sConfig =
                {
                    .u16Endpoint = 1,
                },
        },
};
