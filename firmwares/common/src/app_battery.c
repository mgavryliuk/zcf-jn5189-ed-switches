#include "app_battery.h"

#include <jendefs.h>

#include "JN5189.h"
#include "fsl_adc.h"
#include "fsl_power.h"

static void BATTERY_ConfigureADC(void);
static void BATTERY_DisableADC(void);

BatteryStatus_t BATTERY_GetStatus(void) {
    BATTERY_ConfigureADC();
    ADC_DoSoftwareTriggerConvSeqA(ADC0);

    BatteryStatus_t batStatus;
    adc_result_info_t adcResultInfoStruct;
    while (!ADC_GetChannelConversionResult(ADC0, VBAT_ADC_CHANNEL, &adcResultInfoStruct)) {
    }
    batStatus.voltage_mV = BATTERY_CalcVoltage(adcResultInfoStruct.result);
    batStatus.percent = BATTERY_CalcPercent(batStatus.voltage_mV);

    BAT_DBG("adcResultInfoStruct.result: %d\r\n", adcResultInfoStruct.result);
    BAT_DBG("Voltage (mV): %u\n", batStatus.voltage_mV);
    BAT_DBG("Percentage: %d%%\n", batStatus.percent);

    BATTERY_DisableADC();
    return batStatus;
}

static void BATTERY_ConfigureADC(void) {
    BAT_DBG("[Battery] Configuring ADC...\n");
    CLOCK_EnableClock(kCLOCK_Adc0);
    POWER_EnablePD(kPDRUNCFG_PD_LDO_ADC_EN);
    CLOCK_AttachClk(kFRO12M_to_ADC_CLK);
    CLOCK_SetClkDiv(kCLOCK_DivAdcClk, 3U, false);

    adc_config_t adcConfigStruct;
    adc_conv_seq_config_t adcConvSeqConfigStruct;

    adcConfigStruct.clockMode = kADC_ClockAsynchronousMode;
    adcConfigStruct.resolution = kADC_Resolution12bit;
    adcConfigStruct.sampleTimeNumber = 0;
    adcConfigStruct.extendSampleTimeNumber = kADC_ExtendSampleTimeNotUsed;
    ADC_Init(ADC0, &adcConfigStruct);

    adcConvSeqConfigStruct.channelMask = (1U << VBAT_ADC_CHANNEL);
    adcConvSeqConfigStruct.triggerMask = 0U;
    adcConvSeqConfigStruct.triggerPolarity = kADC_TriggerPolarityPositiveEdge;
    adcConvSeqConfigStruct.enableSingleStep = false;
    adcConvSeqConfigStruct.enableSyncBypass = false;
    adcConvSeqConfigStruct.interruptMode = kADC_InterruptForEachSequence;
    ADC_SetConvSeqAConfig(ADC0, &adcConvSeqConfigStruct);
    /* A problem with the ADC requires a delay after setup, see RFT 1340 */
    CLOCK_uDelay(300U);
    ADC_EnableConvSeqA(ADC0, true);
    BAT_DBG("ADC configured!\n");
}

static void BATTERY_DisableADC(void) {
    BAT_DBG("Disabling ADC\n");
    CLOCK_AttachClk(kNONE_to_ADC_CLK);
    POWER_DisablePD(kPDRUNCFG_PD_LDO_ADC_EN);
    CLOCK_DisableClock(kCLOCK_Adc0);
    BAT_DBG("ADC disabled!\n");
}