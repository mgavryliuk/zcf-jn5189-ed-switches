#include "app_basic_ep.h"

#include "Basic.h"
#include "Identify.h"
#include "PDM.h"
#include "PowerConfiguration.h"
#include "device_config.h"
#include "zcl.h"
#include "zcl_options.h"

static tsZHA_BasicEndpoint sBasicEndpoint;

static void BASIC_EP_Callback(tsZCL_CallBackEvent* psEvent);

void BASIC_EP_Init(void) {
    teZCL_Status eZCL_Status = eCLD_BasicCreateBasic(&sBasicEndpoint.sClusterInstance.sBasicServer, TRUE, &sCLD_Basic,
                                                     &sBasicEndpoint.sBasicServerCluster, &au8BasicClusterAttributeControlBits[0]);
    if (eZCL_Status != E_ZCL_SUCCESS) {
        BASIC_EP_DBG("eCLD_BasicCreateBasic failed with status : % d\n ", eZCL_Status);
    }

    eZCL_Status = eCLD_IdentifyCreateIdentify(&sBasicEndpoint.sClusterInstance.sIdentifyServer, TRUE, &sCLD_Identify,
                                              &sBasicEndpoint.sIdentifyServerCluster, &au8IdentifyAttributeControlBits[0],
                                              &sBasicEndpoint.sIdentifyServerCustomDataStructure);
    if (eZCL_Status != E_ZCL_SUCCESS) {
        BASIC_EP_DBG("eCLD_IdentifyCreateIdentify failed with status : % d\n ", eZCL_Status);
    }

    eZCL_Status = eCLD_PowerConfigurationCreatePowerConfiguration(&sBasicEndpoint.sClusterInstance.sPowerConfigurationServer, TRUE,
                                                                  &sCLD_PowerConfiguration, &sBasicEndpoint.sPowerConfigurationCluster,
                                                                  &au8PowerConfigurationAttributeControlBits[0]);
    if (eZCL_Status != E_ZCL_SUCCESS) {
        BASIC_EP_DBG("eCLD_PowerConfigurationCreatePowerConfiguration failed with status : % d\n ", eZCL_Status);
    }

    eZCL_Status = eCLD_ConfigurationCreateConfiguration(&sBasicEndpoint.sClusterInstance.sConfigurationServer, TRUE, &sCLD_Configuration,
                                                        &sBasicEndpoint.sConfigurationCluster, &au8ConfigurationAttributeControlBits[0]);
    BASIC_EP_DBG("Configuring Custom Button Mode cluster status: %d\n", eZCL_Status);

    sBasicEndpoint.sEndPoint.u8EndPointNumber = device_config.u8BasicEndpoint;
    sBasicEndpoint.sEndPoint.u16ManufacturerCode = ZCL_MANUFACTURER_CODE;
    sBasicEndpoint.sEndPoint.u16ProfileEnum = HA_PROFILE_ID;
    sBasicEndpoint.sEndPoint.bIsManufacturerSpecificProfile = FALSE;
    sBasicEndpoint.sEndPoint.u16NumberOfClusters = sizeof(tsZHA_BasicEndpointClusterInstances) / sizeof(tsZCL_ClusterInstance);
    sBasicEndpoint.sEndPoint.psClusterInstance = (tsZCL_ClusterInstance*)&sBasicEndpoint.sClusterInstance;
    sBasicEndpoint.sEndPoint.bDisableDefaultResponse = ZCL_DISABLE_DEFAULT_RESPONSES;
    sBasicEndpoint.sEndPoint.pCallBackFunctions = &BASIC_EP_Callback;

    eZCL_Status = eZCL_Register(&sBasicEndpoint.sEndPoint);
    if (eZCL_Status != E_ZCL_SUCCESS) {
        BASIC_EP_DBG("eZCL_Register failed with status: %d\n", eZCL_Status);
    }

    memcpy(sBasicEndpoint.sBasicServerCluster.au8ManufacturerName, CLD_BAS_MANUF_NAME_STR, CLD_BAS_MANUF_NAME_SIZE);
    memcpy(sBasicEndpoint.sBasicServerCluster.au8ModelIdentifier, CLD_BAS_MODEL_STR, CLD_BAS_MODEL_ID_SIZE);
    memcpy(sBasicEndpoint.sBasicServerCluster.au8DateCode, CLD_BAS_DATE_STR, CLD_BAS_DATE_SIZE);
    memcpy(sBasicEndpoint.sBasicServerCluster.au8SWBuildID, CLD_BAS_SW_BUILD_STR, CLD_BAS_SW_BUILD_SIZE);

    BASIC_EP_DBG("Configuring voltage attribute to be reportable\n");
    eZCL_Status = eZCL_SetReportableFlag(device_config.u8BasicEndpoint, GENERAL_CLUSTER_ID_POWER_CONFIGURATION, TRUE, FALSE,
                                         E_CLD_PWRCFG_ATTR_ID_BATTERY_VOLTAGE);
    BASIC_EP_DBG("eZCL_SetReportableFlag status: %d\n", eZCL_Status);
}

static void BASIC_EP_Callback(tsZCL_CallBackEvent* psEvent) {
    uint16 clusterId = psEvent->psClusterInstance->psClusterDefinition->u16ClusterEnum;
    switch (psEvent->eEventType) {
        case E_ZCL_CBET_CHECK_ATTRIBUTE_RANGE:
            if (clusterId == GENERAL_CLUSTER_ID_CONFIGURATION) {
                CONFIGURATION_CLUSTER_HandleAttrsRange(psEvent);
            }
            break;

        case E_ZCL_CBET_WRITE_INDIVIDUAL_ATTRIBUTE:
            if (clusterId == GENERAL_CLUSTER_ID_CONFIGURATION) {
                CONFIGURATION_CLUSTER_HandleAttrsWrite(psEvent, &sBasicEndpoint.sConfigurationCluster);
            }
            break;

        default:
            BASIC_EP_DBG("Callback event - (%d)\n", psEvent->eEventType);
            break;
    }
}
