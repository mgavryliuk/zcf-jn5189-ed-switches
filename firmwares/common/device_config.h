#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "app_buttons.h"
#include "app_leds.h"

typedef struct {
    const uint8_t u8BasicEndpoint;
    const uint8_t u8ZdoEndpoint;
    bool_t bIsJoined;
} DeviceConfig_t;

extern DeviceConfig_t device_config;

#endif /* DEVICE_CONFIG_H */
