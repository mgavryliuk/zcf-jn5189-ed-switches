#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <jendefs.h>

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

extern LedConfig* const leds_configs[];
extern ResetLedConfig reset_led_config;

typedef struct {
    const uint32_t u32DioMask;
    const uint32_t u32LedMask;
} tsControlMasks;

typedef struct {
    const uint32_t u32DioMask;
    const uint32_t u32LedMask;
    const bool_t bHasLed;
} tsResetButtonConfig;

typedef struct {
    const uint32_t u32DioPin;
    const uint32_t u32LedPin;
    const uint32_t u32DioMask;
    const uint32_t u32LedMask;
    const bool_t bHasLed;
    const uint16_t u16Endpoint;
} tsEndpointButtonConfig;

typedef struct {
    const uint8_t u8BasicEndpoint;
    const uint8_t u8ZdoEndpoint;

    const uint8_t u8EndpointButtonsCount;
    const tsEndpointButtonConfig* psEndpointButtons;
} tsDeviceConfig;

extern const tsDeviceConfig sDeviceConfig;

#endif /* DEVICE_CONFIG_H */
