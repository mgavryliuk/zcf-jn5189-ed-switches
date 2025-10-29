#include "app_battery.h"

#include <device_config.h>

#include "JN5189.h"
#include "PowerConfiguration.h"
#include "fsl_adc.h"
#include "fsl_power.h"
#include "zcl_customcommand.h"

static void BATTERY_ConfigureADC(void);
static void BATTERY_DisableADC(void);
static void BATTERY_UpdateCluster(void);

void BATTERY_UpdateStatus(void) {
    BATTERY_ConfigureADC();
    ADC_DoSoftwareTriggerConvSeqA(ADC0);

    adc_result_info_t adcResultInfoStruct;
    while (!ADC_GetChannelConversionResult(ADC0, VBAT_ADC_CHANNEL, &adcResultInfoStruct)) {
    }

    DeviceBattery_t deviceBattery = device_config.sDeviceBattery;
    uint16_t voltage_mV = BATTERY_CalcVoltage(adcResultInfoStruct.result);
    deviceBattery.voltage_mV = voltage_mV;
    deviceBattery.percent = BATTERY_CalcPercent(voltage_mV);
    BAT_DBG("adcResultInfoStruct.result: %d\r\n", adcResultInfoStruct.result);
    BAT_DBG("Voltage (mV): %u (%d%%)\n", voltage_mV, deviceBattery.percent);

    BATTERY_DisableADC();
    BATTERY_UpdateCluster();
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

static void BATTERY_UpdateCluster(void) {
    BAT_DBG("Updating PowerConfiguration cluster!\n");
    DeviceBattery_t deviceBattery = device_config.sDeviceBattery;

    tsZCL_ClusterInstance* psZCL_ClusterInstance;
    teZCL_Status eStatus =
        eZCL_SearchForClusterEntry(device_config.u8BasicEndpoint, GENERAL_CLUSTER_ID_POWER_CONFIGURATION, TRUE, &psZCL_ClusterInstance);
    BAT_DBG("Search for cluster entry %d in endpoint %d status: %d\n", GENERAL_CLUSTER_ID_POWER_CONFIGURATION,
            device_config.u8BasicEndpoint, eStatus);

    ((tsCLD_PowerConfiguration*)psZCL_ClusterInstance->pvEndPointSharedStructPtr)->u8BatteryVoltage =
        (uint8)(deviceBattery.voltage_mV / 100);
    ((tsCLD_PowerConfiguration*)psZCL_ClusterInstance->pvEndPointSharedStructPtr)->u8BatteryPercentageRemaining = deviceBattery.percent * 2;
    BAT_DBG("PowerConfiguration cluster sucessfully updated!\n");
}
