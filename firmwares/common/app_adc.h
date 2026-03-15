#ifndef APP_ADC_H
#define APP_ADC_H

#include "dbg.h"
#include "jendefs.h"

#ifdef DEBUG_APP_ADC
#define TRACE_APP_ADC TRUE
#else
#define TRACE_APP_ADC FALSE
#endif

#define APP_ADC_DBG(...) DBG_vPrintf(TRACE_APP_ADC, "[APP ADC] " __VA_ARGS__)

#define TEMP_SENSOR_TRIM_VALID_OFFSET (0x8C)
#define TEMP_SENSOR_TRIM_PARAM_OFFSET (0xA0)

#define FLASH_CFG_TEMPSENSOR_TRIM_VALID_Pos 0
#define FLASH_CFG_TEMPSENSOR_TRIM_VALID_Width 1

#define FLASH_CFG_TEMP_SENSOR_ADCOUT_TREF_VBAT3V3_SUM8_Pos 0
#define FLASH_CFG_TEMP_SENSOR_ADCOUT_TREF_VBAT3V3_SUM8_Width 16

#define FLASH_CFG_TEMP_SENSOR_NLFIT_TOFFSET_Pos 0
#define FLASH_CFG_TEMP_SENSOR_NLFIT_TOFFSET_Width 8
#define FLASH_CFG_TEMP_SENSOR_NLFIT_GAIN_Pos 8
#define FLASH_CFG_TEMP_SENSOR_NLFIT_GAIN_Width 8
#define FLASH_CFG_TEMP_SENSOR_NLFIT_TREF_Pos 16
#define FLASH_CFG_TEMP_SENSOR_NLFIT_TREF_Width 16

#define FLASH_CFG_TEMP_SENSOR_VBATSUM_OFFSET_Pos 0
#define FLASH_CFG_TEMP_SENSOR_VBATSUM_OFFSET_Width 12
#define FLASH_CFG_TEMP_SENSOR_VBATSUM_GAIN_Pos 12
#define FLASH_CFG_TEMP_SENSOR_VBATSUM_GAIN_Width 10

#define FLASH_CFG_TEMP_SENSOR_SLOPEFITINV_GAIN_Pos 0
#define FLASH_CFG_TEMP_SENSOR_SLOPEFITINV_GAIN_Width 16
#define FLASH_CFG_TEMP_SENSOR_SLOPEFITINV_VBATGAIN_Pos 16
#define FLASH_CFG_TEMP_SENSOR_SLOPEFITINV_VBATGAIN_Width 16

#define FIELD_VAL(wrd, _FLD) (((wrd) >> (_FLD##_Pos)) & ((1 << (_FLD##_Width)) - 1))

typedef struct {
    int16_t adcout_Tref_vbat3v3_sum8;
    int16_t Tref;
    int8_t nlfit_Toffset;
    int8_t nlfit_gain;
    int16_t slopefitinv_gain;
    int16_t slopefitinv_vbatgain;
    uint16_t vbatsum_offset;
    int16_t vbatsum_gain;
} APP_ADC_TempSensorCalibration_t;

typedef struct {
    bool_t is_valid;
    int16_t adc_raw_sum;
    int16_t calibrated_mult128;
    int16_t calibrated_mult2;
} APP_ADC_TempMeasurement_t;

typedef struct {
    int16_t adc_raw_sum;
    int16_t voltage_mV;
    int8_t percentage;
} APP_ADC_VBatMeasurement_t;

typedef enum {
    APP_ADC_CALIBRATION_NOT_LOADED = 0,
    APP_ADC_CALIBRATION_PRESENT,
    APP_ADC_CALIBRATION_ABSENT,
} APP_ADC_CalibrationState_t;

#define ADC_MAX_VALUE ((1U << 12) - 1U)
#define ADC_SAMPLES_COUNT 8

#define ADC_TEMP_CHANNEL 7u
#define ADC_TEMP_MODE 0x2
#define ADC_TEMP_TSAMP 0x1f

#define ADC_VBAT_CHANNEL 6U
#define ADC_VBAT_MODE 0x0
#define ADC_VBAT_TSAMP 0x14

#define VREF 3600U
#define MAX_BATTERY_VOLTAGE 3200U
#define MIN_BATTERY_VOLTAGE 2200U

static inline int16_t APP_ADC_CalcVoltage(uint32_t adcValue) {
    return (int16_t)(adcValue * VREF / ADC_MAX_VALUE);
}

static inline int8_t APP_ADC_CalcPercent(int16_t voltage_mV) {
    if (voltage_mV >= MAX_BATTERY_VOLTAGE)
        return 100;
    if (voltage_mV <= MIN_BATTERY_VOLTAGE)
        return 0;

    return (int8_t)((int32_t)(voltage_mV - MIN_BATTERY_VOLTAGE) * 100 / (MAX_BATTERY_VOLTAGE - MIN_BATTERY_VOLTAGE));
}

void APP_ADC_PerformMeasurements();
APP_ADC_VBatMeasurement_t APP_ADC_GetVbatMeasurement();
APP_ADC_TempMeasurement_t APP_ADC_GetTempMeasurement();

#endif /* APP_ADC_H */
