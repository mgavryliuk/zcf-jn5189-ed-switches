#include "device_config.h"

#include "app_leds.h"
#include "device_definitions.h"
#include "zps_gen.h"

LedConfig left_led_config = {.u32DioPin = LED_LEFT_DIO};
LedConfig right_led_config = {.u32DioPin = LED_RIGHT_DIO};
LedConfig* const leds_configs[] = {&left_led_config, &right_led_config};
ResetLedConfig reset_led_config = {.u32Mask = RESET_LED_MASK};

const tsEndpointButtonConfig sEndpointButtonsConfigs[] = {
    {
        .u32DioPin = BTN_LEFT_DIO,
        .u32DioMask = BTN_LEFT_MASK,
        .u32LedPin = LED_LEFT_DIO,
        .bHasLed = TRUE,
        .u16Endpoint = WXKG15LM_LEFTBUTTON_ENDPOINT,
    },
    {
        .u32DioPin = BTN_RIGHT_DIO,
        .u32DioMask = BTN_RIGHT_MASK,
        .u32LedPin = LED_RIGHT_DIO,
        .bHasLed = TRUE,
        .u16Endpoint = WXKG15LM_RIGHTBUTTON_ENDPOINT,
    },
};

const tsDeviceConfig sDeviceConfig = {
    .u8BasicEndpoint = WXKG15LM_BASIC_ENDPOINT,
    .u8ZdoEndpoint = WXKG15LM_ZDO_ENDPOINT,
    .u8EndpointButtonsCount = sizeof(sEndpointButtonsConfigs) / sizeof(tsEndpointButtonConfig),
    .psEndpointButtons = sEndpointButtonsConfigs,
};
