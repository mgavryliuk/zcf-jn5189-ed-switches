#include "app_zb_node.h"

#include "ConfigurationCluster.h"
#include "MicroSpecific.h"
#include "PDM.h"
#include "app_basic_ep.h"
#include "app_on_off_ep.h"
#include "app_polling.h"
#include "bdb_api.h"
#include "device_config.h"
#include "fsl_reset.h"
#include "pdum_gen.h"
#include "zcl.h"
#include "zps_apl_af.h"

static void ZB_NODE_InitQueues(void);
static void ZB_NODE_ZCL_Init(void);
static void ZB_NODE_Configure_Reporting(void);
static void ZB_NODE_ExtendedStatusCallBack(ZPS_teExtendedStatus eExtendedStatus);
static void ZB_NODE_ZCLCallback(tsZCL_CallBackEvent* psEvent);
static void ZB_NODE_HandleAFEvent(BDB_tsZpsAfEvent* psZpsAfEvent);
static void ZB_NODE_FactoryResetRecords(void);

static ZBNodeCallbacks_t ZBNodeCallbacks;

tszQueue APP_msgBdbEvents;

void ZB_NODE_Init(const ZBNodeCallbacks_t* callbacks) {
    if (callbacks) {
        ZBNodeCallbacks = *callbacks;
    }

    ZB_NODE_InitQueues();
    ZPS_vExtendedStatusSetCallback(ZB_NODE_ExtendedStatusCallBack);

    uint16 u16ByteRead;
    PDM_eReadDataFromRecord(PDM_ID_NETWORK_STATE, &device_config.bIsJoined, sizeof(bool_t), &u16ByteRead);
    ZB_NODE_DBG("Device network state: %s\n", device_config.bIsJoined == TRUE ? "JOINED" : "NO_NETWORK");

    ZB_NODE_ZCL_Init();
    ZB_NODE_Configure_Reporting();

    ZPS_eAplAfInit();
    ZPS_bAplAfSetEndDeviceTimeout(ZED_TIMEOUT_8192_MIN);
    ZB_NODE_DBG("Zigbee Protocol Stack initialized! End devince timeout set to 8192 minutes (5 days 16 hrs 32 min)\n");

    BDB_tsInitArgs sInitArgs;
    if (device_config.bIsJoined) {
        device_config.bIsJoined = FALSE;
        sBDB.sAttrib.bbdbNodeIsOnANetwork = TRUE;
    }
    sInitArgs.hBdbEventsMsgQ = &APP_msgBdbEvents;
    BDB_vInit(&sInitArgs);
    ZB_NODE_DBG("BDB_vInit done\n");

    BDB_vStart();
    ZB_NODE_DBG("BDB_vStart done\n");
}

void APP_vBdbCallback(BDB_tsBdbEvent* psBdbEvent) {
    ZB_NODE_DBG("APP_vBdbCallback called with event type: %d\n", psBdbEvent->eEventType);
    switch (psBdbEvent->eEventType) {
        case BDB_EVENT_NONE:
            ZB_NODE_DBG("BDB_EVENT_NONE\n");
            break;

        case BDB_EVENT_ZPSAF:
            ZB_NODE_DBG("BDB_EVENT_ZPSAF\n");
            ZB_NODE_HandleAFEvent(&psBdbEvent->uEventData.sZpsAfEvent);
            break;

        case BDB_EVENT_INIT_SUCCESS:
            ZB_NODE_DBG("BDB_EVENT_INIT_SUCCESS\n");
            break;

        case BDB_EVENT_REJOIN_FAILURE:
            ZB_NODE_DBG("Device failed to re-join the network\n");
            break;

        case BDB_EVENT_NO_NETWORK:
            ZB_NODE_DBG("Device failed to find a network\n");
            if (ZBNodeCallbacks.pfOnNWKSteeringStopCallback) {
                ZBNodeCallbacks.pfOnNWKSteeringStopCallback();
            }
            break;

        case BDB_EVENT_REJOIN_SUCCESS:
            ZB_NODE_DBG("BDB_EVENT_REJOIN_SUCCESS\n");
            device_config.bIsJoined = TRUE;
            POLL_Start(&POLL_FAST_CONFIG);
            break;

        case BDB_EVENT_NWK_STEERING_SUCCESS:
            ZB_NODE_DBG("Device successfully joined to network\n");
            device_config.bIsJoined = TRUE;
            PDM_eSaveRecordData(PDM_ID_NETWORK_STATE, &device_config.bIsJoined, sizeof(bool_t));
            ZPS_vSaveAllZpsRecords();
            if (ZBNodeCallbacks.pfOnNWKSteeringStopCallback) {
                ZBNodeCallbacks.pfOnNWKSteeringStopCallback();
            }
            break;

        case BDB_EVENT_APP_START_POLLING:
            ZB_NODE_DBG("Starting polling for data\n");
            POLL_Start(&POLL_COMMISIONING_CONFIG);
            break;

        default:
            break;
    }
}

void ZB_NODE_OnResetCallback(void) {
    ZB_NODE_DBG("Reset callback called...\n");
    if (device_config.bIsJoined) {
        tsZCL_ClusterInstance* psZCL_ClusterInstance;
        teZCL_Status eStatus =
            eZCL_SearchForClusterEntry(device_config.u8BasicEndpoint, GENERAL_CLUSTER_ID_CONFIGURATION, TRUE, &psZCL_ClusterInstance);
        ZB_NODE_DBG("Search for cluster entry %d in endpoint %d status: %d\n", GENERAL_CLUSTER_ID_CONFIGURATION,
                    device_config.u8BasicEndpoint, eStatus);

        bool_t bPreventReset = ((tsCLD_Configuration*)psZCL_ClusterInstance->pvEndPointSharedStructPtr)->bPreventReset;
        if (device_config.bIsJoined && bPreventReset) {
            ZB_NODE_DBG("Reset prevention enabled. Doing nothing...\n");
            return;
        }

        if (ZPS_eAplZdoLeaveNetwork(0UL, FALSE, FALSE) != ZPS_E_SUCCESS) {
            ZB_NODE_DBG("Leave network msg failed. Force resetting device...\n");
            ZB_NODE_FactoryResetRecords();
            MICRO_DISABLE_INTERRUPTS();
            RESET_SystemReset();
        }
    } else {
        ZB_NODE_DBG("Device is not in network. Starting NWK Steering....\n");
        BDB_eNsStartNwkSteering();
        if (ZBNodeCallbacks.pfOnNWKSteeringStartCallback) {
            ZBNodeCallbacks.pfOnNWKSteeringStartCallback(NULL);
        }
    }
}

static void ZB_NODE_InitQueues(void) {
    ZB_NODE_DBG("Initializing Queues: %d\n", BDB_QUEUE_SIZE);
    ZQ_vQueueCreate(&APP_msgBdbEvents, BDB_QUEUE_SIZE, sizeof(BDB_tsZpsAfEvent), NULL);
    ZQ_vQueueCreate(&zps_msgMlmeDcfmInd, MLME_QUEQUE_SIZE, sizeof(MAC_tsMlmeVsDcfmInd), NULL);
    ZQ_vQueueCreate(&zps_msgMcpsDcfmInd, MCPS_QUEUE_SIZE, sizeof(MAC_tsMcpsVsDcfmInd), NULL);
    ZQ_vQueueCreate(&zps_msgMcpsDcfm, MCPS_DCFM_QUEUE_SIZE, sizeof(MAC_tsMcpsVsCfmData), NULL);
    ZQ_vQueueCreate(&zps_TimeEvents, TIMER_QUEUE_SIZE, sizeof(zps_tsTimeEvent), NULL);
    ZB_NODE_DBG("Queues successfully initialized\n");
}

static void ZB_NODE_ZCL_Init(void) {
    ZB_NODE_DBG("Configuring ZCL...\n");
    teZCL_Status eZCL_Status;
    eZCL_Status = eZCL_Initialise(&ZB_NODE_ZCLCallback, apduZCL);
    if (eZCL_Status != E_ZCL_SUCCESS) {
        ZB_NODE_DBG("eZCL_Status failed with status: %d\n ", eZCL_Status);
    }
    BASIC_EP_Init();
    OnOff_EP_Init();
    ZB_NODE_DBG("Configuring ZCL done\n");
}

static void ZB_NODE_Configure_Reporting(void) {
    ZB_NODE_DBG("Configuring reporting...\n");
    // TODO:
    ZB_NODE_DBG("Configuring reporting done\n");
}

static void ZB_NODE_ExtendedStatusCallBack(ZPS_teExtendedStatus eExtendedStatus) {
    ZB_NODE_DBG("ERROR: Extended status 0x%02x\r\n", eExtendedStatus);
}

static void ZB_NODE_ZCLCallback(tsZCL_CallBackEvent* psEvent) {
    ZB_NODE_DBG("ZB_NODE_ZCLCallback event type: %d\n", psEvent->eEventType);
}

static void ZB_NODE_HandleAFEvent(BDB_tsZpsAfEvent* psZpsAfEvent) {
    if (psZpsAfEvent->u8EndPoint == device_config.u8BasicEndpoint) {
        ZB_NODE_DBG("AF Callback. Basic endpoint event received\n ");
        tsZCL_CallBackEvent sCallBackEvent;
        sCallBackEvent.pZPSevent = &psZpsAfEvent->sStackEvent;
        sCallBackEvent.eEventType = E_ZCL_CBET_ZIGBEE_EVENT;
        vZCL_EventHandler(&sCallBackEvent);
    } else if (psZpsAfEvent->u8EndPoint == device_config.u8ZdoEndpoint) {
        ZB_NODE_DBG("AF Callback. ZDO endpoint event received\n");
        ZPS_tsAfEvent* psAfEvent = &(psZpsAfEvent->sStackEvent);
        switch (psAfEvent->eType) {
            case ZPS_EVENT_NWK_LEAVE_INDICATION:
                ZB_NODE_DBG("AF Callback - ZDO endpoint. Leave Indication %016llx Rejoin %d\n",
                            psAfEvent->uEvent.sNwkLeaveIndicationEvent.u64ExtAddr, psAfEvent->uEvent.sNwkLeaveIndicationEvent.u8Rejoin);
                if ((psAfEvent->uEvent.sNwkLeaveIndicationEvent.u64ExtAddr == 0UL) &&
                    (psAfEvent->uEvent.sNwkLeaveIndicationEvent.u8Rejoin == 0)) {
                    ZB_NODE_DBG("AF Callback - ZDO endpoint. Leave (no re-join) -> Reset Data Structures\n");
                    ZB_NODE_FactoryResetRecords();
                    MICRO_DISABLE_INTERRUPTS();
                    RESET_SystemReset();
                }
                break;

            case ZPS_EVENT_NWK_LEAVE_CONFIRM:
                ZB_NODE_DBG("AF Callback - ZDO endpoint. Leave Confirm status %02x Addr %016llx\n",
                            psAfEvent->uEvent.sNwkLeaveConfirmEvent.eStatus, psAfEvent->uEvent.sNwkLeaveConfirmEvent.u64ExtAddr);
                if (psAfEvent->uEvent.sNwkLeaveConfirmEvent.u64ExtAddr == 0UL) {
                    ZB_NODE_DBG("AF Callback - ZDO endpoint. Leave -> Reset Data Structures\n");
                    ZB_NODE_FactoryResetRecords();
                    MICRO_DISABLE_INTERRUPTS();
                    RESET_SystemReset();
                }
                break;

            case ZPS_EVENT_NWK_POLL_CONFIRM:
                ZB_NODE_DBG("AF Callback - ZDO endpoint. ZPS_EVENT_NWK_POLL_CONFIRM: %d\n",
                            psAfEvent->uEvent.sNwkPollConfirmEvent.u8Status);
                // Switch to fast polling or extend its time if data received
                if (psAfEvent->uEvent.sNwkPollConfirmEvent.u8Status) {
                    const PollingConfig_t* pollCfg = POLL_GetConfig();
                    if (pollCfg != &POLL_FAST_CONFIG) {
                        POLL_Start(&POLL_FAST_CONFIG);
                    } else {
                        POLL_ResetAttempts();
                    }
                }
                break;

            default:
                ZB_NODE_DBG("AF Callback - ZDO endpoint. Unhandled Event %d\n", psAfEvent->eType);
                break;
        }
    }
    if (psZpsAfEvent->sStackEvent.eType == ZPS_EVENT_APS_DATA_INDICATION) {
        ZB_NODE_DBG("AF Callback. ZPS_EVENT_APS_DATA_INDICATION. Freeing APduInstance\n");
        PDUM_eAPduFreeAPduInstance(psZpsAfEvent->sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);
    } else if (psZpsAfEvent->sStackEvent.eType == ZPS_EVENT_APS_INTERPAN_DATA_INDICATION) {
        ZB_NODE_DBG("AF Callback. ZPS_EVENT_APS_INTERPAN_DATA_INDICATION. Freeing APduInstance\n");
        PDUM_eAPduFreeAPduInstance(psZpsAfEvent->sStackEvent.uEvent.sApsInterPanDataIndEvent.hAPduInst);
    }
}

static void ZB_NODE_FactoryResetRecords(void) {
    ZB_NODE_DBG("Factory reset called\n");
    ZPS_eAplAibSetApsUseExtendedPanId(0);
    ZPS_vDefaultStack();
    ZPS_vSetKeys();

    device_config.bIsJoined = FALSE;
    PDM_eSaveRecordData(PDM_ID_NETWORK_STATE, &device_config.bIsJoined, sizeof(bool_t));
    ZPS_vSaveAllZpsRecords();

    CONFIGURATION_CLUSTER_ResetPDMRecord();
}
