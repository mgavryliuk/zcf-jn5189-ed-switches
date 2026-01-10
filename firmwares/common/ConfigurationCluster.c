#include "ConfigurationCluster.h"

#include "PDM.h"
#include "zcl.h"
#include "zcl_options.h"

const tsZCL_AttributeDefinition asCLD_ConfigurationClusterAttributeDefinitions[] = {
    {E_CLD_BUTTON_MODE_ATTR_ID_MODE_TYPE, (E_ZCL_AF_RD | E_ZCL_AF_WR | E_ZCL_AF_MS), E_ZCL_ENUM8,
     (uint32)(&((tsCLD_Configuration*)(0))->eButtonMode), 0},
    {E_CLD_PREVENT_RESET_ATTR_ID_MODE_TYPE, (E_ZCL_AF_RD | E_ZCL_AF_WR | E_ZCL_AF_MS), E_ZCL_BOOL,
     (uint32)(&((tsCLD_Configuration*)(0))->bPreventReset), 0},
};

tsZCL_ClusterDefinition sCLD_Configuration = {GENERAL_CLUSTER_ID_CONFIGURATION,
                                              TRUE,
                                              E_ZCL_SECURITY_NETWORK,
                                              (sizeof(asCLD_ConfigurationClusterAttributeDefinitions) / sizeof(tsZCL_AttributeDefinition)),
                                              (tsZCL_AttributeDefinition*)asCLD_ConfigurationClusterAttributeDefinitions,
                                              NULL};

uint8 au8ConfigurationAttributeControlBits[(sizeof(asCLD_ConfigurationClusterAttributeDefinitions) / sizeof(tsZCL_AttributeDefinition))];

static void CONFIGURATION_CLUSTER_LoadPDMRecord(tsCLD_Configuration* sConfigCluster);
static void CONFIGURATION_CLUSTER_SavePDMRecord(tsCLD_Configuration* sConfigCluster);

teZCL_Status eCLD_ConfigurationCreateConfiguration(tsZCL_ClusterInstance* psClusterInstance, bool_t bIsServer,
                                                   tsZCL_ClusterDefinition* psClusterDefinition, void* pvEndPointSharedStructPtr,
                                                   uint8* pu8AttributeControlBits) {
    vZCL_InitializeClusterInstance(psClusterInstance, bIsServer, psClusterDefinition, pvEndPointSharedStructPtr, pu8AttributeControlBits,
                                   NULL, NULL);

    CONFIGURATION_CLUSTER_LoadPDMRecord((tsCLD_Configuration*)pvEndPointSharedStructPtr);
    return E_ZCL_SUCCESS;
}

void CONFIGURATION_CLUSTER_HandleAttrsRange(tsZCL_CallBackEvent* psEvent) {
    uint16 attrId = psEvent->uMessage.sIndividualAttributeResponse.u16AttributeEnum;
    if (attrId == E_CLD_BUTTON_MODE_ATTR_ID_MODE_TYPE) {
        uint8 mode = *((uint8*)psEvent->uMessage.sIndividualAttributeResponse.pvAttributeData);
        CONFIG_CLUSTER_DBG("Validating attrId %d with value %d\n", attrId, mode);
        if ((mode < E_CLD_BUTTON_MODE_TOGGLE) || (mode > E_CLD_BUTTON_MODE_MULTISTATE_INPUT)) {
            CONFIG_CLUSTER_DBG("Invalid Mode value: %d\n", mode);
            psEvent->uMessage.sIndividualAttributeResponse.eAttributeStatus = E_ZCL_ERR_ATTRIBUTE_RANGE;
        }
    }
}

void CONFIGURATION_CLUSTER_HandleAttrsWrite(tsZCL_CallBackEvent* psEvent, tsCLD_Configuration* sConfigCluster) {
    uint16 attrId = psEvent->uMessage.sIndividualAttributeResponse.u16AttributeEnum;
    switch (psEvent->uMessage.sIndividualAttributeResponse.eAttributeStatus) {
        case E_ZCL_SUCCESS:
            if (attrId == E_CLD_BUTTON_MODE_ATTR_ID_MODE_TYPE) {
                uint8 mode = *((uint8*)psEvent->uMessage.sIndividualAttributeResponse.pvAttributeData);
                CONFIG_CLUSTER_DBG("Writing attrId %d with value %d\n", attrId, mode);
                if ((mode < E_CLD_BUTTON_MODE_TOGGLE) || (mode > E_CLD_BUTTON_MODE_MULTISTATE_INPUT)) {
                    CONFIG_CLUSTER_DBG("Invalid Mode value: %d\n", mode);
                } else {
                    sConfigCluster->eButtonMode = mode;
                    if (g_cbZCLModeChanged)
                        g_cbZCLModeChanged(mode);
                    CONFIGURATION_CLUSTER_SavePDMRecord(sConfigCluster);
                }
            } else if (attrId == E_CLD_PREVENT_RESET_ATTR_ID_MODE_TYPE) {
                bool_t bPreventReset = *((bool_t*)psEvent->uMessage.sIndividualAttributeResponse.pvAttributeData);
                CONFIG_CLUSTER_DBG("Writing attrId %d with value %d\n", attrId, bPreventReset);
                sConfigCluster->bPreventReset = bPreventReset;
                CONFIGURATION_CLUSTER_SavePDMRecord(sConfigCluster);
            }

            break;

        default:
            CONFIG_CLUSTER_DBG("Writing attrId %d failed with status %d\n", attrId,
                               psEvent->uMessage.sIndividualAttributeResponse.eAttributeStatus);
            break;
    }
}

void CONFIGURATION_CLUSTER_ResetPDMRecord(void) {
    CONFIG_CLUSTER_DBG("Removing configuration cluster's PDM Record\n");
    PDM_vDeleteDataRecord(PDM_ID_CONFIGURATION_CLUSTER);
}

static void CONFIGURATION_CLUSTER_LoadPDMRecord(tsCLD_Configuration* sConfigCluster) {
    if (sConfigCluster != NULL) {
        sConfigCluster->eButtonMode = E_CLD_BUTTON_MODE_TOGGLE;
        sConfigCluster->bPreventReset = FALSE;
        uint16 u16ByteRead;
        CONFIG_CLUSTER_DBG("Loading ConfigurationCluster attributes from PDM\n");
        PDM_eReadDataFromRecord(PDM_ID_CONFIGURATION_CLUSTER, sConfigCluster, sizeof(tsCLD_Configuration), &u16ByteRead);
        CONFIG_CLUSTER_DBG("Button Mode: %d. Prevent Reset: %d\n", sConfigCluster->eButtonMode, sConfigCluster->bPreventReset);
        if (g_cbZCLModeChanged)
            g_cbZCLModeChanged(sConfigCluster->eButtonMode);
    }
}

static void CONFIGURATION_CLUSTER_SavePDMRecord(tsCLD_Configuration* sConfigCluster) {
    CONFIG_CLUSTER_DBG("Saving ConfigurationCluster attributes to PDM\n");
    PDM_eSaveRecordData(PDM_ID_CONFIGURATION_CLUSTER, sConfigCluster, sizeof(tsCLD_Configuration));
}
