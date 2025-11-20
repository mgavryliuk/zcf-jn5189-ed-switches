#include "device_config.h"

#include "device_definitions.h"
#include "zps_gen.h"

LedConfig_t leftLedConfig = {.u32DioPin = LED_LEFT_DIO};
LedConfig_t rightLedConfig = {.u32DioPin = LED_RIGHT_DIO};
LedConfig_t* const ledsConfigs[LEDS_AMOUNT] = {&leftLedConfig, &rightLedConfig};

// Buttons configurations
const uint8_t g_u8ButtonsPinsAmount = 2;
const uint32_t g_asButtonsPins[] = {BTN_LEFT_DIO, BTN_RIGHT_DIO};

const uint32_t g_u32ButtonsInterruptMask = BTN_INTERRUPT_MASK;
const uint8_t g_u8ButtonsAmount = BUTTONS_AMOUNT;
const Button_t g_asButtons[BUTTONS_AMOUNT] = {
    {
        .u16Endpoint = DEVBOARD_LEFTBUTTON_ENDPOINT,
        .u32DioMask = BTN_LEFT_MASK,
        .pvLedConfig = &leftLedConfig,
    },
    {
        .u16Endpoint = DEVBOARD_RIGHTBUTTON_ENDPOINT,
        .u32DioMask = BTN_RIGHT_MASK,
        .pvLedConfig = &rightLedConfig,
    },
};

const ResetButton_t g_sResetButton = {
    .u32DioMask = BTN_RESET_MASK,
};
// Buttons configurations - END

DeviceConfig_t device_config = {
    .u8BasicEndpoint = DEVBOARD_BASIC_ENDPOINT,
    .u8ZdoEndpoint = DEVBOARD_ZDO_ENDPOINT,
    .bIsJoined = FALSE,

    .psLedsConfigs = ledsConfigs,
    .u8LedsAmount = LEDS_AMOUNT,

    .sDeviceSetupLedsConfig = {.u32Mask = RESET_LED_MASK},
    .sDeviceBattery = {},
};
