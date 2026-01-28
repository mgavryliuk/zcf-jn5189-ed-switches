#include "device_config.h"

#include "device_definitions.h"
#include "zps_gen.h"

const uint8_t g_asLedsPins[] = {LED_CENTER_DIO};
const size_t g_numLedsPins = sizeof(g_asLedsPins) / sizeof(g_asLedsPins[0]);

Led_t leftLedConfig = {.u32DioMask = LED_CENTER_MASK};
const Led_t* const g_asLeds[] = {&leftLedConfig};
const size_t g_numLeds = sizeof(g_asLeds) / sizeof(g_asLeds[0]);

const Led_t g_sNetworkSetupLed = {.u32DioMask = RESET_LED_MASK};

// Buttons configurations
const size_t g_numButtonsPins = BUTTONS_AMOUNT;
const uint32_t g_asButtonsPins[] = {BTN_CENTER_DIO};

const uint32_t g_u32ButtonsInterruptMask = BTN_INTERRUPT_MASK;
const size_t g_numButtons = BUTTONS_AMOUNT;
const Button_t g_asButtons[] = {{
    .u8Endpoint = WXKG14LM_CENTERBUTTON_ENDPOINT,
    .u32DioMask = BTN_CENTER_MASK,
    .pvLedConfig = &leftLedConfig,
}};

const ResetButton_t g_sResetButton = {
    .u32DioMask = BTN_RESET_MASK,
};
// Buttons configurations - END

DeviceConfig_t device_config = {
    .u8BasicEndpoint = WXKG14LM_BASIC_ENDPOINT,
    .u8ZdoEndpoint = WXKG14LM_ZDO_ENDPOINT,
    .bIsJoined = FALSE,

    .sDeviceBattery = {},
};
