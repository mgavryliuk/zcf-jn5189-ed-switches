#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <jendefs.h>

#include "app_buttons.h"
#include "app_leds.h"

/* Maximum number of buttons supported by the firmware.
   This is defined at compile time to avoid dynamic memory allocation during
   execution */
#ifndef MAX_BUTTONS_COUNT
#define MAX_BUTTONS_COUNT (10)
#endif

#ifndef MAX_BLINK_STATES
#define MAX_BLINK_STATES (10)
#endif

typedef struct {
    const uint8_t u8BasicEndpoint;
    const uint8_t u8ZdoEndpoint;

    LedConfig* const* psLedsConfigs;
    const uint8_t u8LedsAmount;

    DeviceSetupLedsConfig sDeviceSetupLedsConfig;

    Button* const* psButtons;
    const uint8_t u8ButtonsAmount;

    const uint32_t u32ButtonsInterruptMask;

    ResetMaskConfig u32ResetMaskConfig;
} DeviceConfig;

extern DeviceConfig device_config;

#endif /* DEVICE_CONFIG_H */
