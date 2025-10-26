#include "device_config.h"

#include "app_leds.h"
#include "device_definitions.h"
#include "zps_gen.h"

LedConfig left_led_config = {.u32DioPin = LED_LEFT_DIO};
LedConfig right_led_config = {.u32DioPin = LED_RIGHT_DIO};
LedConfig* const leds_configs[LEDS_AMOUNT] = {&left_led_config, &right_led_config};

Button left_button = {
    .u16Endpoint = WXKG15LM_LEFTBUTTON_ENDPOINT,
    .u32DioPin = BTN_LEFT_DIO,
    .u32DioMask = BTN_LEFT_MASK,

    .bPressed = FALSE,
    .u16PressedCycles = 0,
    .u8Debounce = BTN_DEBOUNCE_MASK,
    .eState = IDLE,

    .pfOnPressCallback = LED_ButtonBlinkCallback,
    .pvOnPressContext = &left_led_config,
};
Button right_button = {
    .u16Endpoint = WXKG15LM_RIGHTBUTTON_ENDPOINT,
    .u32DioPin = BTN_RIGHT_DIO,
    .u32DioMask = BTN_RIGHT_MASK,

    .bPressed = FALSE,
    .u16PressedCycles = 0,
    .u8Debounce = BTN_DEBOUNCE_MASK,
    .eState = IDLE,

    .pfOnPressCallback = LED_ButtonBlinkCallback,
    .pvOnPressContext = &right_led_config,
};
Button* const buttons[] = {&left_button, &right_button};

DeviceConfig device_config = {
    .u8BasicEndpoint = WXKG15LM_BASIC_ENDPOINT,
    .u8ZdoEndpoint = WXKG15LM_ZDO_ENDPOINT,

    .psLedsConfigs = leds_configs,
    .u8LedsAmount = LEDS_AMOUNT,

    .sDeviceSetupLedsConfig = {.u32Mask = RESET_LED_MASK},

    .psButtons = buttons,
    .u8ButtonsAmount = BUTTONS_AMOUNT,

    .u32ButtonsInterruptMask = BTN_INTERRUPT_MASK,
    .u32ResetMaskConfig =
        {
            .u32DioMask = BTN_RESET_MASK,
            .bPressed = FALSE,
            .u16PressedCycles = 0,
            .u8Debounce = BTN_DEBOUNCE_MASK,
        },
};
