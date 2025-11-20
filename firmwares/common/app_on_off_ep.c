#include "app_on_off_ep.h"

#include "Groups.h"
#include "MultistateInputBasic.h"
#include "OnOff.h"
#include "dbg.h"
#include "device_config.h"
#include "device_definitions.h"
#include "zps_gen.h"

static tsZHA_OnOffEndpoint tsOnOffButtonEndpoints[BUTTONS_AMOUNT];

static void OnOff_EP_ConfigureEP(uint8 u8EndPoint, tsZHA_OnOffEndpoint* tsEndpoint);
static void OnOff_EP_Callback(tsZCL_CallBackEvent* psEvent);

void OnOff_EP_Init(void) {
    int i = 0;
    for (i = 0; i < g_u8ButtonsAmount; i++) {
        OnOff_EP_ConfigureEP(g_asButtons[i].u16Endpoint, &tsOnOffButtonEndpoints[i]);
    }
}

static void OnOff_EP_ConfigureEP(uint8 u8Endpoint, tsZHA_OnOffEndpoint* tsEndpoint) {
    ON_OFF_EP_DBG("Registering endpoint: %d\n", u8Endpoint);
    tsEndpoint->sEndPoint.u8EndPointNumber = u8Endpoint;
    tsEndpoint->sEndPoint.u16ManufacturerCode = ZCL_MANUFACTURER_CODE;
    tsEndpoint->sEndPoint.u16ProfileEnum = HA_PROFILE_ID;
    tsEndpoint->sEndPoint.bIsManufacturerSpecificProfile = FALSE;
    tsEndpoint->sEndPoint.u16NumberOfClusters = sizeof(tsZHA_OnOffEndpointClusterInstances) / sizeof(tsZCL_ClusterInstance);
    tsEndpoint->sEndPoint.psClusterInstance = (tsZCL_ClusterInstance*)&tsEndpoint->sClusterInstance;
    tsEndpoint->sEndPoint.bDisableDefaultResponse = ZCL_DISABLE_DEFAULT_RESPONSES;
    tsEndpoint->sEndPoint.pCallBackFunctions = &OnOff_EP_Callback;

    teZCL_Status eZCL_Status = eCLD_OnOffCreateOnOff(&tsEndpoint->sClusterInstance.sOnOffClient, FALSE, &sCLD_OnOffClient,
                                                     &tsEndpoint->sOnOffClientCluster, &au8OnOffClientAttributeControlBits[0], NULL);
    if (eZCL_Status != E_ZCL_SUCCESS) {
        ON_OFF_EP_DBG("eCLD_OnOffCreateOnOff failed with status: %d\n", eZCL_Status);
    }

    eZCL_Status = eCLD_MultistateInputBasicCreateMultistateInputBasic(
        &tsEndpoint->sClusterInstance.sMultistateInputServer, TRUE, &sCLD_MultistateInputBasic, &tsEndpoint->sMultistateInputServerCluster,
        &au8MultistateInputBasicAttributeControlBits[0]);
    if (eZCL_Status != E_ZCL_SUCCESS) {
        ON_OFF_EP_DBG("eCLD_MultistateInputBasicCreateMultistateInputBasic failed with status: %d\n", eZCL_Status);
    }

    eZCL_Status =
        eCLD_GroupsCreateGroups(&tsEndpoint->sClusterInstance.sGroupsClient, FALSE, &sCLD_Groups, &tsEndpoint->sGroupsClientCluster,
                                &au8GroupsAttributeControlBits[0], &tsEndpoint->sGroupsClientCustomDataStructure, &tsEndpoint->sEndPoint);
    if (eZCL_Status != E_ZCL_SUCCESS) {
        ON_OFF_EP_DBG("eCLD_GroupsCreateGroups failed with status: %d\n", eZCL_Status);
    }

    eZCL_Status = eZCL_Register(&tsEndpoint->sEndPoint);
    if (eZCL_Status != E_ZCL_SUCCESS) {
        ON_OFF_EP_DBG("eZCL_Register failed with status: %d\n", eZCL_Status);
    }
}

static void OnOff_EP_Callback(tsZCL_CallBackEvent* psEvent) {
    switch (psEvent->eEventType) {
        default:
            ON_OFF_EP_DBG("Invalid event type (%d) in OnOff_EP_Callback\n", psEvent->eEventType);
            break;
    }
}
