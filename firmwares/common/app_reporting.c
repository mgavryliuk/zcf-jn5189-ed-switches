#include "app_reporting.h"

#include "PowerConfiguration.h"
#include "device_config.h"
#include "zcl.h"
#include "zcl_options.h"

tsReports asDefaultReports[ZCL_NUMBER_OF_REPORTS] = {
    {
        GENERAL_CLUSTER_ID_POWER_CONFIGURATION,
        FALSE,
        {
            .u8DirectionIsReceived = 0,
            .eAttributeDataType = E_ZCL_UINT8,
            .u16AttributeEnum = E_CLD_PWRCFG_ATTR_ID_BATTERY_VOLTAGE,
            .u16MinimumReportingInterval = ZCL_SYSTEM_MIN_REPORT_INTERVAL,
            .u16MaximumReportingInterval = ZCL_SYSTEM_MAX_REPORT_INTERVAL,
            .u16TimeoutPeriodField = 0,
            .uAttributeReportableChange = {1},
        },
    },
    {
        GENERAL_CLUSTER_ID_POWER_CONFIGURATION,
        FALSE,
        {
            .u8DirectionIsReceived = 0,
            .eAttributeDataType = E_ZCL_UINT8,
            .u16AttributeEnum = E_CLD_PWRCFG_ATTR_ID_BATTERY_PERCENTAGE_REMAINING,
            .u16MinimumReportingInterval = ZCL_SYSTEM_MIN_REPORT_INTERVAL,
            .u16MaximumReportingInterval = ZCL_SYSTEM_MAX_REPORT_INTERVAL,
            .u16TimeoutPeriodField = 0,
            .uAttributeReportableChange = {1},
        },
    },
};

PUBLIC void REPORTING_MakeSupportedAttributesReportable(void) {
    uint16 u16AttributeEnum;
    uint16 u16ClusterId;
    bool_t bManufacturerSpecific;
    teZCL_Status eStatus;
    int i;

    tsZCL_AttributeReportingConfigurationRecord* psAttributeReportingConfigurationRecord;

    for (i = 0; i < ZCL_NUMBER_OF_REPORTS; i++) {
        u16AttributeEnum = asDefaultReports[i].sAttributeReportingConfigurationRecord.u16AttributeEnum;
        u16ClusterId = asDefaultReports[i].u16ClusterID;
        bManufacturerSpecific = asDefaultReports[i].bManufacturerSpecific;
        psAttributeReportingConfigurationRecord = &(asDefaultReports[i].sAttributeReportingConfigurationRecord);
        REPORTING_DBG("REPORT: Cluster %04x Attribute %04x Min %d Max %d IntV %d Direct %d Change %d ManuSpecific %d\n", u16ClusterId,
                      u16AttributeEnum, asDefaultReports[i].sAttributeReportingConfigurationRecord.u16MinimumReportingInterval,
                      asDefaultReports[i].sAttributeReportingConfigurationRecord.u16MaximumReportingInterval,
                      asDefaultReports[i].sAttributeReportingConfigurationRecord.u16TimeoutPeriodField,
                      asDefaultReports[i].sAttributeReportingConfigurationRecord.u8DirectionIsReceived,
                      asDefaultReports[i].sAttributeReportingConfigurationRecord.uAttributeReportableChange.zint8ReportableChange,
                      bManufacturerSpecific);

        REPORTING_DBG("Configuring attribute %04x to be reportable\n", u16AttributeEnum);
        eStatus = eZCL_SetReportableFlag(device_config.u8BasicEndpoint, u16ClusterId, TRUE, FALSE, u16AttributeEnum);
        REPORTING_DBG("eZCL_SetReportableFlag status: %d\n", eStatus);
        eStatus = eZCL_CreateLocalReport(device_config.u8BasicEndpoint, u16ClusterId, bManufacturerSpecific, TRUE,
                                         psAttributeReportingConfigurationRecord);
        REPORTING_DBG("REPORT: eZCL_CreateLocalReport failed with status %d\n", eStatus);
    }
}
