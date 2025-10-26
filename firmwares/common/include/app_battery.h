
#ifndef APP_BATTERY_H
#define APP_BATTERY_H

#include <jendefs.h>

#include "dbg.h"

#ifdef DEBUG_APP_BATTERY
#define TRACE_BATTERY TRUE
#else
#define TRACE_BATTERY FALSE
#endif

#define BAT_DBG(...) DBG_vPrintf(TRACE_BATTERY, "[Battery] " __VA_ARGS__)

#define VBAT_ADC_CHANNEL 6U
#define VREF 3600U
#define MAX_BATTERY_VOLTAGE 3300U
#define MIN_BATTERY_VOLTAGE 2200U
#define ADC_MAX_VALUE ((1U << 12) - 1U)

typedef struct {
    uint16_t voltage_mV;
    uint8_t percent;
} BatteryStatus_t;

static inline uint16_t BATTERY_CalcVoltage(uint32_t adcValue) {
    return (uint16_t)(adcValue * VREF / ADC_MAX_VALUE);
}

static inline uint8_t BATTERY_CalcPercent(uint16_t voltage_mV) {
    if (voltage_mV >= MAX_BATTERY_VOLTAGE)
        return 100;
    if (voltage_mV <= MIN_BATTERY_VOLTAGE)
        return 0;

    return (uint8_t)((uint32_t)(voltage_mV - MIN_BATTERY_VOLTAGE) * 100 / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE));
}

BatteryStatus_t BATTERY_GetStatus(void);

#endif /* APP_BATTERY_H */
