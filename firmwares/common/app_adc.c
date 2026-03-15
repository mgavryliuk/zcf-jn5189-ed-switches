#include "app_adc.h"

#include "JN5189.h"
#include "fsl_adc.h"
#include "fsl_flash.h"
#include "fsl_power.h"
#include "jendefs.h"

static void APP_ADC_LoadTempCalibrations(void);
static void APP_ADC_ConfigureADC(void);
static uint16_t APP_ADC_ReadSampleSum(uint8_t channel, uint8_t ctrl0_mode, uint8_t ctrl0_tsamp);
static void APP_ADC_DeinitADC(void);
static int32_t APP_ADC_ComputeTemperature(uint16_t adcout_vbat_lsb_sum8, uint16_t tsens_adcout_T_sum8);
static int32_t APP_ADC_ComputeNLFitCorrection(int32_t input);

static APP_ADC_TempSensorCalibration_t temp_sensor_calibration;
static adc_config_t adcConfigStruct;
static adc_conv_seq_config_t adcConvSeqConfigStruct;

static APP_ADC_VBatMeasurement_t vbat_measurements;
static APP_ADC_TempMeasurement_t temp_measurements;
static APP_ADC_CalibrationState_t calibration_load_state = APP_ADC_CALIBRATION_NOT_LOADED;

void APP_ADC_PerformMeasurements() {
    APP_ADC_LoadTempCalibrations();
    APP_ADC_ConfigureADC();

    APP_ADC_DBG("Measuring battery voltage...\n");
    ADC_EnableConvSeqA(ADC0, false);
    // test mode=0 (Normal functional mode (DIV4 mode). Input range is 0 to 3.6V, although max input voltage is affected by supply voltage
    // of the device. TSAMP = 0x14 (default) - 55 kΩ
    vbat_measurements.adc_raw_sum = APP_ADC_ReadSampleSum(ADC_VBAT_CHANNEL, ADC_VBAT_MODE, ADC_VBAT_TSAMP);
    vbat_measurements.voltage_mV = APP_ADC_CalcVoltage((vbat_measurements.adc_raw_sum >> 3));
    vbat_measurements.percentage = APP_ADC_CalcPercent(vbat_measurements.voltage_mV);

    APP_ADC_DBG("vbat_measurements.adc_raw_sum = %d\r\n", vbat_measurements.adc_raw_sum);
    APP_ADC_DBG("vbat_measurements.voltage_mV = %d\r\n", vbat_measurements.voltage_mV);
    APP_ADC_DBG("vbat_measurements.percentage = %d\r\n", vbat_measurements.percentage);
    ADC_EnableConvSeqA(ADC0, false);

    if (calibration_load_state == APP_ADC_CALIBRATION_PRESENT) {
        ADC_EnableConvSeqA(ADC0, false);
        ADC_EnableTemperatureSensor(ADC0, true);
        CLOCK_uDelay(100);
        // test mode=2 (ADC in unity gain mode. (DIV1 mode). Input range is 0 to 0.9V. Voltages above this maн damage the device.
        // TSAMP = 0x1а - 87 kΩ
        temp_measurements.is_valid = TRUE;
        temp_measurements.adc_raw_sum = APP_ADC_ReadSampleSum(ADC_TEMP_CHANNEL, ADC_TEMP_MODE, ADC_TEMP_TSAMP);
        temp_measurements.calibrated_mult128 = APP_ADC_ComputeTemperature(vbat_measurements.adc_raw_sum, temp_measurements.adc_raw_sum);
        temp_measurements.calibrated_mult2 = temp_measurements.calibrated_mult128 / 64;
        APP_ADC_DBG("temp_measurements.adc_raw_sum = %d\r\n", temp_measurements.adc_raw_sum);
        APP_ADC_DBG("temp_measurements.calibrated_mult128 = %d\r\n", temp_measurements.calibrated_mult128);
        APP_ADC_DBG("temp_measurements.calibrated_mult2 = %d\r\n", temp_measurements.calibrated_mult2);
        ADC_EnableTemperatureSensor(ADC0, false);
        ADC_EnableConvSeqA(ADC0, false);
    } else {
        temp_measurements.is_valid = FALSE;
        APP_ADC_DBG("No valid calibration available, skipping temperature measurement\n");
    }

    APP_ADC_DeinitADC();
}

APP_ADC_VBatMeasurement_t APP_ADC_GetVbatMeasurement() {
    return vbat_measurements;
}

APP_ADC_TempMeasurement_t APP_ADC_GetTempMeasurement() {
    return temp_measurements;
}

static void APP_ADC_LoadTempCalibrations(void) {
    if (calibration_load_state != APP_ADC_CALIBRATION_NOT_LOADED) {
        return;
    }

    APP_ADC_DBG("Load ADC & Temp calibration values...\n");
    uint32_t* tempsensor_trim_valid = (uint32_t*)(FLASH_CONFIG_PAGE_ADDR + TEMP_SENSOR_TRIM_VALID_OFFSET);
    uint32_t* temp_sensor = (uint32_t*)(FLASH_CONFIG_PAGE_ADDR + TEMP_SENSOR_TRIM_PARAM_OFFSET);
    uint32_t wrd = *tempsensor_trim_valid;

    bool_t valid = FIELD_VAL(wrd, FLASH_CFG_TEMPSENSOR_TRIM_VALID);
    if (!valid) {
        calibration_load_state = APP_ADC_CALIBRATION_ABSENT;
        return;
    }

    wrd = temp_sensor[3];
    temp_sensor_calibration.adcout_Tref_vbat3v3_sum8 = FIELD_VAL(wrd, FLASH_CFG_TEMP_SENSOR_ADCOUT_TREF_VBAT3V3_SUM8);
    wrd = temp_sensor[4];
    temp_sensor_calibration.nlfit_Toffset = FIELD_VAL(wrd, FLASH_CFG_TEMP_SENSOR_NLFIT_TOFFSET);
    temp_sensor_calibration.nlfit_gain = FIELD_VAL(wrd, FLASH_CFG_TEMP_SENSOR_NLFIT_GAIN);
    temp_sensor_calibration.Tref = FIELD_VAL(wrd, FLASH_CFG_TEMP_SENSOR_NLFIT_TREF);

    wrd = temp_sensor[5];
    temp_sensor_calibration.vbatsum_offset = FIELD_VAL(wrd, FLASH_CFG_TEMP_SENSOR_VBATSUM_OFFSET);
    temp_sensor_calibration.vbatsum_gain = FIELD_VAL(wrd, FLASH_CFG_TEMP_SENSOR_VBATSUM_GAIN);
    if (temp_sensor_calibration.vbatsum_gain & (1 << 9)) {
        temp_sensor_calibration.vbatsum_gain = (int16_t)(temp_sensor_calibration.vbatsum_gain | 0xFC00);
    }

    wrd = temp_sensor[6];
    temp_sensor_calibration.slopefitinv_gain = FIELD_VAL(wrd, FLASH_CFG_TEMP_SENSOR_SLOPEFITINV_GAIN);
    temp_sensor_calibration.slopefitinv_vbatgain = FIELD_VAL(wrd, FLASH_CFG_TEMP_SENSOR_SLOPEFITINV_VBATGAIN);

    APP_ADC_DBG("temp_sensor_calibration.adcout_Tref_vbat3v3_sum8 = %d\n", temp_sensor_calibration.adcout_Tref_vbat3v3_sum8);
    APP_ADC_DBG("temp_sensor_calibration.nlfit_Toffset = %d\n", temp_sensor_calibration.nlfit_Toffset);
    APP_ADC_DBG("temp_sensor_calibration.nlfit_gain = %d\n", temp_sensor_calibration.nlfit_gain);
    APP_ADC_DBG("temp_sensor_calibration.Tref = %d\n", temp_sensor_calibration.Tref);
    APP_ADC_DBG("temp_sensor_calibration.vbatsum_offset = %d\n", temp_sensor_calibration.vbatsum_offset);
    APP_ADC_DBG("temp_sensor_calibration.vbatsum_gain = %d\n", temp_sensor_calibration.vbatsum_gain);
    APP_ADC_DBG("temp_sensor_calibration.slopefitinv_gain = %d\n", temp_sensor_calibration.slopefitinv_gain);
    APP_ADC_DBG("temp_sensor_calibration.slopefitinv_vbatgain = %d\n", temp_sensor_calibration.slopefitinv_vbatgain);

    calibration_load_state = APP_ADC_CALIBRATION_PRESENT;
    return;
}

static void APP_ADC_ConfigureADC(void) {
    APP_ADC_DBG("Configuring ADC...\n");
    CLOCK_EnableClock(kCLOCK_Adc0);
    CLOCK_AttachClk(kFRO12M_to_ADC_CLK);
    CLOCK_SetClkDiv(kCLOCK_DivAdcClk, 3U, false);
    POWER_EnablePD(kPDRUNCFG_PD_LDO_ADC_EN);
    CLOCK_uDelay(10U);

    adcConfigStruct.clockMode = kADC_ClockAsynchronousMode;
    adcConfigStruct.resolution = kADC_Resolution12bit;
    adcConfigStruct.sampleTimeNumber = 0;
    adcConfigStruct.extendSampleTimeNumber = kADC_ExtendSampleTimeNotUsed;
    ADC_Init(ADC0, &adcConfigStruct);

    adcConvSeqConfigStruct.channelMask = 0;
    adcConvSeqConfigStruct.triggerMask = 0U;
    adcConvSeqConfigStruct.triggerPolarity = kADC_TriggerPolarityPositiveEdge;
    adcConvSeqConfigStruct.enableSingleStep = false;
    adcConvSeqConfigStruct.enableSyncBypass = false;
    adcConvSeqConfigStruct.interruptMode = kADC_InterruptForEachSequence;
    ADC_SetConvSeqAConfig(ADC0, &adcConvSeqConfigStruct);
    /* A problem with the ADC requires a delay after setup, see RFT 1340 */
    CLOCK_uDelay(300U);
    ADC_EnableConvSeqA(ADC0, true);
    APP_ADC_DBG("ADC configured!\n");
}

static uint16_t APP_ADC_ReadSampleSum(uint8_t channel, uint8_t ctrl0_mode, uint8_t ctrl0_tsamp) {
    uint32_t read_reg = ADC0->GPADC_CTRL0;
    APP_ADC_DBG("Configuring ADC0->GPADC_CTRL0. Current value: 0x%8x, mode: %u, tsamp: %u\n", read_reg, ctrl0_mode, ctrl0_tsamp);
    read_reg &= ~(ADC_GPADC_CTRL0_TEST_MASK | ADC_GPADC_CTRL0_GPADC_TSAMP_MASK);
    read_reg |= ADC_GPADC_CTRL0_GPADC_TSAMP(ctrl0_tsamp);
    read_reg |= ADC_GPADC_CTRL0_TEST(ctrl0_mode);
    ADC0->GPADC_CTRL0 = read_reg;
    APP_ADC_DBG("Configured ADC0->GPADC_CTRL0. New value: 0x%08x\n", ADC0->GPADC_CTRL0);

    adcConvSeqConfigStruct.channelMask = (1 << channel);
    adcConvSeqConfigStruct.triggerMask = 2U;
    adcConvSeqConfigStruct.triggerPolarity = kADC_TriggerPolarityPositiveEdge;
    adcConvSeqConfigStruct.enableSingleStep = false;
    adcConvSeqConfigStruct.enableSyncBypass = true;
    adcConvSeqConfigStruct.interruptMode = kADC_InterruptForEachConversion;
    ADC_SetConvSeqAConfig(ADC0, &adcConvSeqConfigStruct);
    ADC_EnableConvSeqA(ADC0, true);

    uint8_t count = 0;
    uint16_t estimated_sum = 0;
    adc_result_info_t adcResultInfoStruct;
    while (count < ADC_SAMPLES_COUNT) {
        ADC_DoSoftwareTriggerConvSeqA(ADC0);
        while (!ADC_GetChannelConversionResult(ADC0, channel, &adcResultInfoStruct)) {
        }
        estimated_sum += adcResultInfoStruct.result;
        count++;
    }
    return estimated_sum;
}

static int32_t APP_ADC_ComputeNLFitCorrection(int32_t input) {
    int32_t signed_T_linearfit = input >> 7;
    int32_t coefnlfit1 =
        (signed_T_linearfit - temp_sensor_calibration.nlfit_Toffset) * (signed_T_linearfit - temp_sensor_calibration.nlfit_Toffset);
    int32_t coefnlfit1_shift6 = coefnlfit1 >> 6;
    return (temp_sensor_calibration.nlfit_gain * coefnlfit1_shift6) >> 6;
}

static int32_t APP_ADC_ComputeTemperature(uint16_t adcout_vbat_lsb_sum8, uint16_t tsens_adcout_T_sum8) {
    int16_t diff_adcout_vbat_lsb_mult8 = adcout_vbat_lsb_sum8 - (temp_sensor_calibration.vbatsum_offset * ADC_SAMPLES_COUNT);
    int32_t signed_val_coef1_mult8 = (temp_sensor_calibration.slopefitinv_vbatgain * diff_adcout_vbat_lsb_mult8);
    int32_t signed_val_coef1_shift16 = signed_val_coef1_mult8 >> 16;
    int32_t signed_val_coef2_mult8 = ((1 << 18) - signed_val_coef1_shift16);
    int32_t signed_val_coef2 = ((1 << 18) - signed_val_coef1_shift16) / ADC_SAMPLES_COUNT;

    if (signed_val_coef2_mult8 & (1 << 2)) {
        if (signed_val_coef1_mult8 & (1 << 31)) {
            signed_val_coef2--;
        } else {
            signed_val_coef2++;
        }
    }

    int32_t signed_val_coef3_shift16 = (temp_sensor_calibration.slopefitinv_gain * signed_val_coef2) >> 16;
    int32_t signed_val_coef4 = (tsens_adcout_T_sum8 - temp_sensor_calibration.adcout_Tref_vbat3v3_sum8) * signed_val_coef3_shift16;
    int32_t signed_val_coef4_shift13 = signed_val_coef4 >> 13;

    int32_t T_linearfit_soft_mult128 = signed_val_coef4_shift13 + temp_sensor_calibration.Tref;
    int32_t Test_Vbatsumcorr_soft_mult128 = ((temp_sensor_calibration.vbatsum_gain * diff_adcout_vbat_lsb_mult8) / ADC_SAMPLES_COUNT) >> 9;
    int32_t nlfit_corr_tlinearfit_soft_mult128 = APP_ADC_ComputeNLFitCorrection(T_linearfit_soft_mult128);
    int32_t nlfit_corr_tref_soft_mult128 = APP_ADC_ComputeNLFitCorrection(temp_sensor_calibration.Tref);

    int32_t Test_final_soft_mult128 =
        T_linearfit_soft_mult128 + nlfit_corr_tlinearfit_soft_mult128 - nlfit_corr_tref_soft_mult128 - Test_Vbatsumcorr_soft_mult128;
    return Test_final_soft_mult128;
}

static void APP_ADC_DeinitADC(void) {
    APP_ADC_DBG("Deinitializing ADC\n");
    CLOCK_AttachClk(kNONE_to_ADC_CLK);
    POWER_DisablePD(kPDRUNCFG_PD_LDO_ADC_EN);
    CLOCK_DisableClock(kCLOCK_Adc0);
    APP_ADC_DBG("ADC deinitialized!\n");
}
