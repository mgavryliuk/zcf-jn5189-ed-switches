#include "app_zb_node.h"

#include "PDM.h"
#include "bdb_api.h"
#include "device_config.h"
#include "pdum_gen.h"
#include "zcl.h"
#include "zps_apl_af.h"

static void ZB_NODE_InitQueues(void);
static void ZB_NODE_ZCL_Init(void);
static void ZB_NODE_Configure_Reporting(void);
static void ZB_NODE_ExtendedStatusCallBack(ZPS_teExtendedStatus eExtendedStatus);
static void ZB_NODE_ZCLCallback(tsZCL_CallBackEvent* psEvent);

tszQueue APP_msgBdbEvents;

void ZB_NODE_Init(void) {
    ZB_NODE_InitQueues();
    ZPS_vExtendedStatusSetCallback(ZB_NODE_ExtendedStatusCallBack);

    uint16 u16ByteRead;
    PDM_eReadDataFromRecord(PDM_NETWORK_STATE_ID, &device_config.bIsJoined, sizeof(bool_t), &u16ByteRead);
    ZB_NODE_DBG("Device network state: %s\n", device_config.bIsJoined == TRUE ? "JOINED" : "NO_NETWORK");

    ZB_NODE_ZCL_Init();
    ZB_NODE_Configure_Reporting();

    ZPS_eAplAfInit();
    ZPS_bAplAfSetEndDeviceTimeout(ZED_TIMEOUT_8192_MIN);
    ZB_NODE_DBG("Zigbee Protocol Stack initialized! End devince timeout set to 8192 minutes (5 days 16 hrs 32 min)");

    BDB_tsInitArgs sInitArgs;
    if (device_config.bIsJoined) {
        device_config.bIsJoined = FALSE;
        sBDB.sAttrib.bbdbNodeIsOnANetwork = TRUE;
    }
    sInitArgs.hBdbEventsMsgQ = &APP_msgBdbEvents;
    BDB_vInit(&sInitArgs);
    ZB_NODE_DBG("BDB_vInit done");

    BDB_vStart();
    ZB_NODE_DBG("BDB_vStart done");
    // BDB_teStatus eStatus = BDB_eNsStartNwkSteering();
    // ZB_NODE_DBG("NWK Steering status: %d\n", eStatus);
}

void APP_vBdbCallback(BDB_tsBdbEvent* psBdbEvent) {
    ZB_NODE_DBG("APP_vBdbCallback called with event type: %d\n", psBdbEvent->eEventType);
    switch (psBdbEvent->eEventType) {
        case BDB_EVENT_NONE:
            ZB_NODE_DBG("BDB_EVENT_NONE\n");
            break;

        case BDB_EVENT_ZPSAF:  // Use with BDB_tsZpsAfEvent
            ZB_NODE_DBG("BDB_EVENT_ZPSAF\n");
            // vAppHandleAfEvent(&psBdbEvent->uEventData.sZpsAfEvent);
            break;

        case BDB_EVENT_INIT_SUCCESS:
            ZB_NODE_DBG("BDB_EVENT_INIT_SUCCESS\n");
            break;

        case BDB_EVENT_REJOIN_FAILURE:
            ZB_NODE_DBG("BDB_EVENT_REJOIN_FAILURE\n");
            break;

        case BDB_EVENT_NO_NETWORK:
            ZB_NODE_DBG("BDB_EVENT_NO_NETWORK\n");
            break;

        case BDB_EVENT_REJOIN_SUCCESS:
            ZB_NODE_DBG("BDB_EVENT_REJOIN_SUCCESS\n");
            // vHandleNetworkJoinAndRejoin();
            // APP_vStartPolling(POLL_FAST);
            break;

        case BDB_EVENT_NWK_STEERING_SUCCESS:
            ZB_NODE_DBG("BDB_EVENT_NWK_STEERING_SUCCESS\n");
            break;

        case BDB_EVENT_APP_START_POLLING:
            ZB_NODE_DBG("BDB_EVENT_APP_START_POLLING\n");
            // APP_vStartPolling(POLL_COMMISSIONING);
            break;

        default:
            break;
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
    // APP_vRegisterBasicEndPoint();
    // APP_vRegisterOnOffEndPoints();
    ZB_NODE_DBG("Configuring ZCL done\n");
}

static void ZB_NODE_Configure_Reporting(void) {
    ZB_NODE_DBG("Configuring reporting...\n");

    ZB_NODE_DBG("Configuring reporting done");
}

static void ZB_NODE_ExtendedStatusCallBack(ZPS_teExtendedStatus eExtendedStatus) {
    ZB_NODE_DBG("ERROR: Extended status 0x%02x\r\n", eExtendedStatus);
}

static void ZB_NODE_ZCLCallback(tsZCL_CallBackEvent* psEvent) {
    ZB_NODE_DBG("ZB_NODE_ZCLCallback event type: %d\n", psEvent->eEventType);
}
