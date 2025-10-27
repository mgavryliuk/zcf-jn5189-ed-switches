#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <jendefs.h>

#include "app_battery.h"
#include "app_buttons.h"
#include "app_leds.h"

#define PDM_NETWORK_STATE_ID 0x8

typedef struct {
    const uint8_t u8BasicEndpoint;
    const uint8_t u8ZdoEndpoint;
    bool_t bIsJoined;

    LedConfig_t* const* psLedsConfigs;
    const uint8_t u8LedsAmount;

    DeviceSetupLedsConfig_t sDeviceSetupLedsConfig;

    Button_t* const* psButtons;
    uint8_t u8ButtonScanTimerID;
    const uint8_t u8ButtonsAmount;

    const uint32_t u32ButtonsInterruptMask;

    ResetMaskConfig_t sResetMaskConfig;

    DeviceBattery_t sDeviceBattery;
} DeviceConfig_t;

extern DeviceConfig_t device_config;

#endif /* DEVICE_CONFIG_H */
