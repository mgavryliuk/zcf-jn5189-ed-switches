/*
* Copyright 2024 NXP
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

#include <EmbeddedTypes.h>
#include "bdb_api.h"
#include "PDM.h"
#include "PDM_IDs.h"
#include "app_common.h"
#include "app_common_ncp.h"
#include "app_zcl_task.h"
#include "dbg.h"
#include "app_main.h"
#include "zigbee_config.h"
#include "pdum_gen.h"
#include "app_coordinator.h"
#include "app_ota_server.h"
#include "serial_link_ctrl.h"
#include "serial_link_cmds_ctrl.h"
/* Needed for calloc/malloc */
#include <stdlib.h>

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#ifndef TRACE_APP
#define TRACE_APP FALSE
#endif

#ifndef TRACE_APP_INIT
#define TRACE_APP_INIT FALSE
#endif

#define GREEN_POWER_ENDPOINT 242

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
static void vAppHandleAfEvent(BDB_tsZpsAfEvent *psZpsAfEvent);
static void vAppSendOnOff(void);
static void vAppSendIdentifyStop(uint16_t u16Address, uint8_t u8Endpoint);
static void vAppSendRemoteBindRequest(uint16_t u16DstAddr, uint16_t u16ClusterId, uint8_t u8DstEp);
static void APP_vBdbInit(void);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
tsNcpDeviceDesc sNcpDeviceDesc = {FACTORY_NEW, E_STARTUP, ZPS_ZDO_DEVICE_COORD};
const uint8_t gUseRtos_c = 1;

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
extern bool_t bZCLQueueFull;

/****************************************************************************/
/***        Global Variables                                              ***/
/****************************************************************************/
PUBLIC uint16 u16appPrintBufferTimeInSec = 300U;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
PUBLIC void vApp_ProcessMessage(uintptr_t uiMsg);
PUBLIC void vApp_ProcessMessageVal(uintptr_t uiMsg);
PUBLIC void APP_vHandleNwkStackEvents(ZPS_tsAfEvent *psStackEvent);

/****************************************************************************
 *
 * NAME:        pvZCL_HeapAlloc
 *
 * DESCRIPTION:
 * Allocates a block of memory from the heap.
 *
 * RETURNS:
 * Pointer to block, or NULL if the heap didn't have enough space. If block
 * was already assigned, returns original value and doesn't take anything
 * from heap.
 *
 * NOTES:
 * If buffer has already been allocated, it is not cleared. If it is a fresh
 * allocation, it is cleared on request.
 *
 ****************************************************************************/
void *pvZCL_HeapAlloc(void *pvPointer, uint32 u32BytesNeeded, bool_t bClear)
{
    if (bClear)
    {
        return calloc(1, u32BytesNeeded);
    }
    else
    {
        return malloc(u32BytesNeeded);
    }
}

/****************************************************************************
 *
 * NAME: APP_vInitialiseCoordinator
 *
 * DESCRIPTION:
 * Initialises the Coordinator application
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
void APP_vInitialiseCoordinator(void)
{
    uint16_t u16ByteRead;

    /* Restore any application data previously saved to flash */
    sNcpDeviceDesc.eState       = FACTORY_NEW;
    sNcpDeviceDesc.eNodeState   = E_STARTUP;
    sNcpDeviceDesc.u8DeviceType = ZPS_ZDO_DEVICE_COORD;

    PDM_eReadDataFromRecord(PDM_ID_APP_COORD,
                            &sNcpDeviceDesc,
                            sizeof(sNcpDeviceDesc),
                            &u16ByteRead);

    APP_ZCL_vInitialise();

    /* Initialise other software modules
     * HERE
     */
    APP_eZbModuleInitialise();
    APP_vBdbInit();
    DBG_vPrintf(TRACE_APP, "Recovered Application State %d On Network %d\r\n", 
                sNcpDeviceDesc.eNodeState, sBDB.sAttrib.bbdbNodeIsOnANetwork);
}

/****************************************************************************
 *
 * NAME: APP_vBdbCallback
 *
 * DESCRIPTION:
 * Callback from the BDB
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
void APP_vBdbCallback(BDB_tsBdbEvent *psBdbEvent)
{
    static uint8_t u8NoQueryCount;

    switch (psBdbEvent->eEventType)
    {
        case BDB_EVENT_NONE:
            break;

        case BDB_EVENT_ZPSAF:
            /* Use with BDB_tsZpsAfEvent */
            vAppHandleAfEvent(&psBdbEvent->uEventData.sZpsAfEvent);
            break;

        case BDB_EVENT_INIT_SUCCESS:
            break;

        case BDB_EVENT_NWK_FORMATION_SUCCESS:
            DBG_vPrintf(TRACE_APP, "APP-BDB: NwkFormation Success \r\n");
            sNcpDeviceDesc.eNodeState = E_RUNNING;
            PDM_eSaveRecordData(PDM_ID_APP_COORD,
                                &sNcpDeviceDesc,
                                sizeof(sNcpDeviceDesc));
            break;

        case BDB_EVENT_NWK_STEERING_SUCCESS:
            DBG_vPrintf(TRACE_APP, "APP-BDB: NwkSteering Success\r\n");
            break;

        case BDB_EVENT_NWK_FORMATION_FAILURE:
            DBG_vPrintf(TRACE_APP, "APP-BDB: Network Formation Failure\r\n");
            break;

        case BDB_EVENT_FB_HANDLE_SIMPLE_DESC_RESP_OF_TARGET:
            DBG_vPrintf(TRACE_APP,
                        "APP-BDB: F&B Simple Desc response From %04x Profle %04x Device %04x Ep %d Version %d\r\n",
                        psBdbEvent->uEventData.psFindAndBindEvent->u16TargetAddress,
                        psBdbEvent->uEventData.psFindAndBindEvent->u16ProfileId,
                        psBdbEvent->uEventData.psFindAndBindEvent->u16DeviceId,
                        psBdbEvent->uEventData.psFindAndBindEvent->u8TargetEp,
                        psBdbEvent->uEventData.psFindAndBindEvent->u8DeviceVersion);
            break;

        case BDB_EVENT_FB_CHECK_BEFORE_BINDING_CLUSTER_FOR_TARGET:
            DBG_vPrintf(TRACE_APP, "APP-BDB: Check For Binding Cluster %04x\r\n",
                        psBdbEvent->uEventData.psFindAndBindEvent->uEvent.u16ClusterId);
            break;
        case BDB_EVENT_FB_CLUSTER_BIND_CREATED_FOR_TARGET:
            DBG_vPrintf(TRACE_APP, "APP-BDB: Bind Created for cluster %04x\r\n",
                        psBdbEvent->uEventData.psFindAndBindEvent->uEvent.u16ClusterId);
            vAppSendRemoteBindRequest(psBdbEvent->uEventData.psFindAndBindEvent->u16TargetAddress,
                                      psBdbEvent->uEventData.psFindAndBindEvent->uEvent.u16ClusterId,
                                      psBdbEvent->uEventData.psFindAndBindEvent->u8TargetEp);
            break;

        case BDB_EVENT_FB_BIND_CREATED_FOR_TARGET:
            DBG_vPrintf(TRACE_APP, "APP-BDB: Bind Created for target EndPt %d\r\n",
                        psBdbEvent->uEventData.psFindAndBindEvent->u8TargetEp);
            vAppSendIdentifyStop(psBdbEvent->uEventData.psFindAndBindEvent->u16TargetAddress,
                                 psBdbEvent->uEventData.psFindAndBindEvent->u8TargetEp);

            break;

        case BDB_EVENT_FB_GROUP_ADDED_TO_TARGET:
            DBG_vPrintf(TRACE_APP, "APP-BDB: Group Added with ID %04x\r\n",
                        psBdbEvent->uEventData.psFindAndBindEvent->uEvent.u16GroupId);
            u8NoQueryCount = 0;
            break;

        case BDB_EVENT_FB_ERR_BINDING_FAILED:
            DBG_vPrintf(TRACE_APP, "APP-BDB: Binding Failed\r\n");
            break;

        case BDB_EVENT_FB_ERR_BINDING_TABLE_FULL:
            DBG_vPrintf(TRACE_APP, "APP-BDB: Binding Table Full\r\n");
            break;

        case BDB_EVENT_FB_ERR_GROUPING_FAILED:
            DBG_vPrintf(TRACE_APP, "APP-BDB: Grouping Failed\r\n");
            break;

        case BDB_EVENT_FB_NO_QUERY_RESPONSE:

            if (u8NoQueryCount >= 2)
            {
                u8NoQueryCount = 0;
                BDB_vFbExitAsInitiator();
                DBG_vPrintf(TRACE_APP, "APP-BDB: No Identify Query Response Stopping F&B\r\n");
            }
            else
            {
                u8NoQueryCount++;
                DBG_vPrintf(TRACE_APP, "APP-BDB: No Identify Query Response\r\n");
            }
            break;

        case BDB_EVENT_FB_TIMEOUT:
            DBG_vPrintf(TRACE_APP, "APP-BDB: F&B Timeout\r\n");
            break;

        default:
            DBG_vPrintf(TRACE_APP, "APP-BDB: Unhandled %d\r\n", psBdbEvent->eEventType);
            break;
    }
}

/****************************************************************************
 *
 * NAME: APP_taskCoordinator
 *
 * DESCRIPTION:
 * Main state machine
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
void APP_taskCoordinator(void)
{
    BDB_teStatus eStatus;
    APP_tsEvent sAppEvent;
    sAppEvent.eType = APP_E_EVENT_NONE;

    if (ZQ_bQueueReceive(&APP_msgAppEvents, &sAppEvent) == TRUE)
    {
        DBG_vPrintf(TRACE_APP, "APP-EVT: Event %d, NodeState=%d\r\n", sAppEvent.eType, sNcpDeviceDesc.eNodeState);

        switch (sAppEvent.eType)
        {
            case APP_E_EVENT_SERIAL_TOGGLE:
                DBG_vPrintf(TRACE_APP, "APP-EVT: Send Toggle Cmd\r\n");
                vAppSendOnOff();
                break;

            case APP_E_EVENT_SERIAL_NWK_STEER:
                eStatus = BDB_eNsStartNwkSteering();
                DBG_vPrintf(TRACE_APP, "APP-EVT: Request Nwk Steering %02x\r\n", eStatus);
                break;

            case APP_E_EVENT_SERIAL_FIND_BIND_START:
                eStatus = BDB_eFbTriggerAsInitiator(COORDINATOR_APPLICATION_ENDPOINT);
                DBG_vPrintf(TRACE_APP, "APP-EVT: Find and Bind initiate %02x\r\n", eStatus);
                break;

            case APP_E_EVENT_SERIAL_FORM_NETWORK:
                /* Not already on a network ? */
                if (FALSE == sBDB.sAttrib.bbdbNodeIsOnANetwork)
                {
                    eStatus = BDB_eNfStartNwkFormation();
                    DBG_vPrintf(TRACE_APP, "APP-EVT: Request Nwk Formation %02x\r\n", eStatus);
                }
                break;

            default:
                break;
        }
    }
}

/****************************************************************************
 *
 * NAME: vAppHandleAfEvent
 *
 * DESCRIPTION:
 * Application handler for stack events
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
static void vAppHandleAfEvent(BDB_tsZpsAfEvent *psZpsAfEvent)
{
    if (psZpsAfEvent->u8EndPoint == COORDINATOR_APPLICATION_ENDPOINT)
    {
        if ((psZpsAfEvent->sStackEvent.eType == ZPS_EVENT_APS_DATA_INDICATION) ||
            (psZpsAfEvent->sStackEvent.eType == ZPS_EVENT_APS_INTERPAN_DATA_INDICATION))
        {
            APP_ZCL_vEventHandler(&psZpsAfEvent->sStackEvent);
        }
    }
    else if (psZpsAfEvent->u8EndPoint == COORDINATOR_ZDO_ENDPOINT)
    {
        vAppHandleZdoEvents(psZpsAfEvent);
    }
    else if ((psZpsAfEvent->u8EndPoint == GREEN_POWER_ENDPOINT) ||
             (psZpsAfEvent->sStackEvent.eType == ZPS_EVENT_APS_ZGP_DATA_CONFIRM))
    {
        if ((psZpsAfEvent->sStackEvent.eType == ZPS_EVENT_APS_DATA_INDICATION) ||
            (psZpsAfEvent->sStackEvent.eType == ZPS_EVENT_APS_ZGP_DATA_INDICATION) ||
            (psZpsAfEvent->sStackEvent.eType == ZPS_EVENT_APS_ZGP_DATA_CONFIRM))
        {
            APP_ZCL_vEventHandler(&psZpsAfEvent->sStackEvent);
        }
    }

    /* free up any Apdus */
    if (psZpsAfEvent->sStackEvent.eType == ZPS_EVENT_APS_DATA_INDICATION)
    {
        PDUM_eAPduFreeAPduInstance(psZpsAfEvent->sStackEvent.uEvent.sApsDataIndEvent.hAPduInst);
    }
    else if (psZpsAfEvent->sStackEvent.eType == ZPS_EVENT_APS_INTERPAN_DATA_INDICATION)
    {
        PDUM_eAPduFreeAPduInstance(psZpsAfEvent->sStackEvent.uEvent.sApsInterPanDataIndEvent.hAPduInst);
    }
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
/****************************************************************************
 *
 * NAME: vAppHandleZdoEvents
 *
 * DESCRIPTION:
 * Application handler for stack events for end point 0 (ZDO)
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
void vAppHandleZdoEvents(BDB_tsZpsAfEvent *psZpsAfEvent)
{
    ZPS_tsAfEvent *psAfEvent = &(psZpsAfEvent->sStackEvent);

    switch (psAfEvent->eType)
    {
        case ZPS_EVENT_APS_DATA_INDICATION:
            DBG_vPrintf(
                TRACE_APP,
                "APP-ZDO: Data Indication Status %02x from %04x Src Ep %d Dst Ep %d Profile %04x Cluster %04x\r\n",
                psAfEvent->uEvent.sApsDataIndEvent.eStatus, psAfEvent->uEvent.sApsDataIndEvent.uSrcAddress.u16Addr,
                psAfEvent->uEvent.sApsDataIndEvent.u8SrcEndpoint, psAfEvent->uEvent.sApsDataIndEvent.u8DstEndpoint,
                psAfEvent->uEvent.sApsDataIndEvent.u16ProfileId, psAfEvent->uEvent.sApsDataIndEvent.u16ClusterId);
            break;

        case ZPS_EVENT_APS_DATA_CONFIRM:
            break;

        case ZPS_EVENT_APS_DATA_ACK:
            break;
            break;

        case ZPS_EVENT_NWK_STARTED:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: Network started Channel = %d\r\n", ZPS_u8AplZdoGetRadioChannel());
            break;

        case ZPS_EVENT_NWK_FAILED_TO_START:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: Network Failed To start\r\n");
            break;

        case ZPS_EVENT_NWK_NEW_NODE_HAS_JOINED:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: New Node %04x Has Joined\r\n",
                        psAfEvent->uEvent.sNwkJoinIndicationEvent.u16NwkAddr);
            break;

        case ZPS_EVENT_NWK_DISCOVERY_COMPLETE:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: Discovery Complete %02x\r\n",
                        psAfEvent->uEvent.sNwkDiscoveryEvent.eStatus);
            break;

        case ZPS_EVENT_NWK_LEAVE_INDICATION:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: Leave Indication %016lx Rejoin %d\r\n",
                    psAfEvent->uEvent.sNwkLeaveIndicationEvent.u64ExtAddr,
                    psAfEvent->uEvent.sNwkLeaveIndicationEvent.u8Rejoin);
            break;

        case ZPS_EVENT_NWK_STATUS_INDICATION:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: Network status Indication %02x addr %04x\r\n",
                        psAfEvent->uEvent.sNwkStatusIndicationEvent.u8Status,
                        psAfEvent->uEvent.sNwkStatusIndicationEvent.u16NwkAddr);
            break;

        case ZPS_EVENT_NWK_ROUTE_DISCOVERY_CONFIRM:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: Discovery Confirm\r\n");
            break;

        case ZPS_EVENT_NWK_ED_SCAN:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: Energy Detect Scan %02x\r\n",
                        psAfEvent->uEvent.sNwkEdScanConfirmEvent.u8Status);
            break;
        case ZPS_EVENT_ZDO_BIND:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: Zdo Bind event\r\n");
            break;

        case ZPS_EVENT_ZDO_UNBIND:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: Zdo Unbiind Event\r\n");
            break;

        case ZPS_EVENT_ZDO_LINK_KEY:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: Zdo Link Key Event Type %d Addr %016lx\r\n",
                        psAfEvent->uEvent.sZdoLinkKeyEvent.u8KeyType,
                        psAfEvent->uEvent.sZdoLinkKeyEvent.u64IeeeLinkAddr);
            break;

        case ZPS_EVENT_BIND_REQUEST_SERVER:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: Bind Request Server Event\r\n");
            break;

        case ZPS_EVENT_ERROR:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: AF Error Event %d\r\n", psAfEvent->uEvent.sAfErrorEvent.eError);
            break;

        case ZPS_EVENT_TC_STATUS:
            DBG_vPrintf(TRACE_APP, "APP-ZDO: Trust Center Status %02x\r\n", psAfEvent->uEvent.sApsTcEvent.u8Status);
            break;

        default:
            break;
    }
}

/****************************************************************************
 *
 * NAME: vAppSendOnOff
 *
 * DESCRIPTION:
 * Sends an On Of Togle Command to the bound devices
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
static void vAppSendOnOff(void)
{
    tsZCL_Address sDestinationAddress;
    uint8_t u8seqNo;
    teZCL_Status eStatus;

    sDestinationAddress.eAddressMode = E_ZCL_AM_BOUND_NON_BLOCKING;

    eStatus = eCLD_OnOffCommandSend(APP_u8GetDeviceEndpoint(), // Src Endpoint
                                    0,                         // Dest Endpoint (bound so do not care)
                                    &sDestinationAddress,
                                    &u8seqNo,
                                    E_CLD_ONOFF_CMD_TOGGLE);

    if (eStatus != E_ZCL_SUCCESS)
    {
        DBG_vPrintf(TRACE_APP, "Send Toggle Failed x%02x Last error %02x\r\n", 
                    eStatus, eZCL_GetLastZpsError());
    }
}

/****************************************************************************
 *
 * NAME: vAppSendIdentifyStop
 *
 * DESCRIPTION:
 * Sends an Identify stop command to the target address
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
static void vAppSendIdentifyStop(uint16_t u16Address, uint8_t u8Endpoint)
{
    uint8_t u8Seq;
    tsZCL_Address sAddress;
    tsCLD_Identify_IdentifyRequestPayload sPayload;

    sPayload.u16IdentifyTime                = 0;
    sAddress.eAddressMode                   = E_ZCL_AM_SHORT_NO_ACK;
    sAddress.uAddress.u16DestinationAddress = u16Address;

    eCLD_IdentifyCommandIdentifyRequestSend(
            APP_u8GetDeviceEndpoint(),
                            u8Endpoint,
                            &sAddress,
                            &u8Seq,
                            &sPayload);
}

/****************************************************************************
 *
 * NAME: vAppSendRemoteBindRequest
 *
 * DESCRIPTION:
 * Sends a bind request to a remote node for it to create a binding with this node
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
static void vAppSendRemoteBindRequest(uint16_t u16DstAddr, uint16_t u16ClusterId, uint8_t u8DstEp)
{
    PDUM_thAPduInstance hAPduInst;
    ZPS_tuAddress uDstAddr;
    ZPS_tsAplZdpBindUnbindReq sAplZdpBindUnbindReq;
    ZPS_teStatus eStatus;

    uDstAddr.u16Addr = u16DstAddr;

    hAPduInst = PDUM_hAPduAllocateAPduInstance(apduZDP);
    if (PDUM_INVALID_HANDLE != hAPduInst)
    {
        uint8_t u8SeqNumber;
        sAplZdpBindUnbindReq.u64SrcAddress = ZPS_u64AplZdoLookupIeeeAddr(u16DstAddr);
        sAplZdpBindUnbindReq.u8SrcEndpoint = APP_u8GetDeviceEndpoint();
        sAplZdpBindUnbindReq.u16ClusterId  = u16ClusterId;

        sAplZdpBindUnbindReq.u8DstAddrMode = E_ZCL_AM_IEEE;
        sAplZdpBindUnbindReq.uAddressField.sExtended.u64DstAddress =
            ZPS_u64NwkNibGetExtAddr(ZPS_pvAplZdoGetNwkHandle());
        sAplZdpBindUnbindReq.uAddressField.sExtended.u8DstEndPoint = u8DstEp;

        DBG_vPrintf(TRACE_APP, "Remote Bind Dst addr %04x, Ieee Dst Addr %016lx Ieee Src %016lx\r\n",
                uDstAddr.u16Addr,
                sAplZdpBindUnbindReq.uAddressField.sExtended.u64DstAddress,
                sAplZdpBindUnbindReq.u64SrcAddress);

       eStatus = ZPS_eAplZdpBindUnbindRequest(hAPduInst,
                                               uDstAddr,
                                               FALSE,
                                               &u8SeqNumber,
                                               TRUE,
                                               &sAplZdpBindUnbindReq);
        DBG_vPrintf(TRACE_APP, "Sending a remote bind request Status =%x\r\n", eStatus);
        if (eStatus)
        {
            PDUM_eAPduFreeAPduInstance(hAPduInst);
        }
    }
}

/****************************************************************************
 *
 * NAME: APP_vFactoryResetRecords
 *
 * DESCRIPTION:
 * Resets persisted data structures to factory new state
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
void APP_vFactoryResetRecords(void)
{
    /* clear out the stack */
    ZPS_vDefaultStack();
    ZPS_vSetKeys();
    ZPS_vSaveAllZpsRecords();

    DBG_vPrintf(TRUE, "Erasing PDM data on Coprocessor...");
    u8ErasePersistentData();

    /* wait for coprocessor to be ready */
    vSetJNState(JN_NOT_READY);
    vWaitForJNReady(JN_READY_TIME_MS);
    vSL_SetStandardResponsePeriod();

    /* handle NCP HOST side */
    PDM_vDeleteAllDataRecords();
    APP_vNcpHostReset();
}

/****************************************************************************
 *
 * NAME: APP_vBdbInit
 *
 * DESCRIPTION:
 * Function to initialize BDB attributes and message queue
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
static void APP_vBdbInit(void)
{
    BDB_tsInitArgs sInitArgs;

    sBDB.sAttrib.bbdbNodeIsOnANetwork = ((sNcpDeviceDesc.eNodeState >= E_RUNNING) ? (TRUE) : (FALSE));
    sInitArgs.hBdbEventsMsgQ          = &APP_msgBdbEvents;

    BDB_vInit(&sInitArgs);
    sBDB.sAttrib.u32bdbPrimaryChannelSet   = BDB_PRIMARY_CHANNEL_SET;
    sBDB.sAttrib.u32bdbSecondaryChannelSet = BDB_SECONDARY_CHANNEL_SET;
}

/****************************************************************************
 *
 * NAME: APP_eGetNodeDeviceType
 *
 * DESCRIPTION:
 * Returns the node device type
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
uint8 APP_eGetNodeDeviceType(void)
{
    return sNcpDeviceDesc.u8DeviceType;
}

/****************************************************************************
 *
 * NAME: APP_eGetCurrentApplicationState
 *
 * DESCRIPTION:
 * Returns the current state of the application
 *
 * RETURNS:
 * teNodeState
 *
 ****************************************************************************/
teNodeState APP_eGetCurrentApplicationState(void)
{
    return sNcpDeviceDesc.eNodeState;
}

/****************************************************************************
 *
 * NAME: APP_u8GetDeviceEndpoint
 *
 * DESCRIPTION:
 * Return the application endpoint
 *
 * PARAMETER: void
 *
 * RETURNS: uint8_t
 *
 ****************************************************************************/
uint8_t APP_u8GetDeviceEndpoint(void)
{
    return COORDINATOR_APPLICATION_ENDPOINT;
}

/********************************************************************************
 *
 * @fn PRIVATE void APP_vNcpMainTask
 *
 */
/**
 *
 * @param p_arg void *
 *
 * @brief Main State Machine for local Node
 *
 * @return void
 *
 * @note
 *
 * imported description
 ********************************************************************************/
void APP_vNcpMainTask(void)
{
    bool pBT;
    uintptr_t uiMsg;

    /* handle serial messages  */
    (void)vSL_CheckAndHandleSerialMsg();

    /* handle zcl messages  */
    pBT = ZQ_bQueueReceive((void *)&zclQueueHandle, (void *)&uiMsg);

    if (bZCLQueueFull == (bool_t)TRUE)
    {
        DBG_vPrintf((bool_t)TRUE, "Reading ZCL Queue After Post Fail Main Task %d\n", pBT);
        bZCLQueueFull = (bool_t)FALSE;
    }

    if (pBT == TRUE)
    {
        APP_vProcessZCLMessage(uiMsg);
    }
    /* handle application messages */
    pBT = ZQ_bQueueReceive((void *)&appQueueHandle, (void *)&uiMsg);

    if (pBT == TRUE)
    {
        if ((uiMsg & QUEUE_MSG_BY_VALUE) == QUEUE_MSG_BY_VALUE)
        {
            uintptr_t uiLocalMsg = uiMsg & (~QUEUE_MSG_BY_VALUE);
            vApp_ProcessMessageVal(uiLocalMsg);
        }
        else
        {
            vApp_ProcessMessage(uiMsg);
        }
    }
}

/********************************************************************************
 *
 * @fn PUBLIC void vApp_ProcessMessage
 *
 */
/**
 *
 * @param uiMsg uintptr_t
 *
 * @brief
 *
 * @return void
 *
 * @note
 *
 * imported description
 ********************************************************************************/
PUBLIC void vApp_ProcessMessage(uintptr_t uiMsg)
{
    ZPS_tsAfEvent sStackEvent;
    PDUM_thAPduInstance myPDUM_thAPduInstance = PDUM_INVALID_HANDLE;

    if ((SL_MSG_TYPE_NWK == *((uint8 *)uiMsg)) || (SL_MSG_TYPE_ZDP_MSG == *((uint8 *)uiMsg)))
    {
        ZPS_tsAfZdpEvent sApsZdpEvent;
        uint8 u8MsgType = *(uint8 *)uiMsg;
        (void)ZBmemset(&sStackEvent, 0x00, sizeof(ZPS_tsAfEvent));
        (void)ZBmemset(&sApsZdpEvent, 0x00, sizeof(sApsZdpEvent));
        if (SL_MSG_TYPE_NWK == u8MsgType)
        {
            vSL_HandleNwkEvent((uint8 *)uiMsg, &sStackEvent, &sApsZdpEvent);
        }
        else
        {
            vSL_HandleZDPMsgEvent((uint8 *)uiMsg, &myPDUM_thAPduInstance, &sStackEvent, &sApsZdpEvent);
        }

        /* Handle Nwk stack events */
        APP_vHandleNwkStackEvents(&sStackEvent);
        if (SL_MSG_TYPE_ZDP_MSG == u8MsgType)
        {
            /* Delete APDU allocated */
            if (myPDUM_thAPduInstance != PDUM_INVALID_HANDLE)
            {
                (void)PDUM_eAPduFreeAPduInstance(myPDUM_thAPduInstance);
            }
        }
    }
    else if (*((uint8 *)uiMsg) == SL_MSG_TYPE_APDU)
    {
        (void)ZBmemset(&sStackEvent, 0x00, sizeof(ZPS_tsAfEvent));
        PDUM_thAPduInstance tempPDUM_thAPduInstance = PDUM_INVALID_HANDLE;
        /* Process the serial buffer */
        vSL_HandleApduEvent((uint8 *)uiMsg, &tempPDUM_thAPduInstance, &sStackEvent);

        /* Delete APDU before returning */
        if (tempPDUM_thAPduInstance != PDUM_INVALID_HANDLE)
        {
            (void)PDUM_eAPduFreeAPduInstance(tempPDUM_thAPduInstance);
        }
    }
    else if (*((uint8 *)uiMsg) == (uint32)APP_MSG_TYPE_USER_CMD)
    {
        DBG_vPrintf(TRUE, "Got APP_MSG_TYPE_USER_CMD\n");
    }
    else if (*((uint8 *)uiMsg) == (uint32)APP_MSG_TYPE_USER_CMD_TX)
    {
        DBG_vPrintf(TRUE, "Got APP_MSG_TYPE_USER_CMD_TX\n");
    }
    else if (*((uint8 *)uiMsg) == SL_MSG_TYPE_NODE_PARENT)
    {
        uint16 u16ShortAddress, u16ID;
        uint64 u64LongAddress, u64ParentAddress;
        uint8 u8Status;
        vSL_HandleNodeParentIndication((uint8*)uiMsg, &u16ShortAddress, &u64LongAddress, &u64ParentAddress, &u8Status, &u16ID);
        DBG_vPrintf((bool_t)1, "Update Msg short 0x%04x 0x%016lx parent 0x%016lx, Status %d ID %d\n",
                u16ShortAddress, u64LongAddress, u64ParentAddress, u8Status, u16ID );
    }
}

/********************************************************************************
 *
 * @fn PUBLIC void vApp_ProcessMessageVal
 *
 */
/**
 *
 * @param uiMsg uintptr_t
 *
 * @brief process message received as value, not pointer
 *
 * @return PUBLIC void
 *
 * @note
 *
 ********************************************************************************/
PUBLIC void vApp_ProcessMessageVal(uintptr_t uiMsg)
{
    PDUM_thAPduInstance myPDUM_thAPduInstance;

    (void)ZBmemset(&myPDUM_thAPduInstance, 0, sizeof(myPDUM_thAPduInstance));

    if (uiMsg == SL_MSG_TYPE_JN_RESETED)
    {
        DBG_vPrintf((bool_t)TRUE, "SL_MSG_TYPE_JN_RESETED\n");
        vWaitForJNReady(JN_READY_TIME_MS);
#ifdef ENABLE_UART_ACK_FROM_HOST
        (void)vConfigureCoprocessorUartAckTimeout(JN_UART_ACK_TIMEOUT);
#endif
    }
    else if (uiMsg == SL_MSG_STACK_STARTED_RUNNING)
    {
        DBG_vPrintf((bool_t)TRUE, "ZdoStartStack In Running State (JN has restarted)\n");
    }
    else if (uiMsg == SL_MSG_TYPE_EXCEPTION)
    {
        /* When an exception occurs, the JN is supposed to reset itself after printed the stack dump
         * - If not for an unknown reason, the next message the LPC is going to send will not have no response from the
         * JN Therefore, after 4 attempts, the LPC is going to automatically reset the JN which will place it in the
         * correct state
         * - If so, the JN will be in the correct state and the next LPC message will be processed as expected
         * We still need to change the SL timeout value to be extended, to allow the JN to send the stack dump.
         * The vSL_SetStandardResponsePeriod is called upon the reception of E_SL_MSG_EVENT_RESET
         */

        vSL_SetLongResponsePeriod();
    }
}

/********************************************************************************
 *
 * @fn PUBLIC void APP_vProcessZCLMessage
 *
 */
/**
 *
 * @param uiMsg uintptr_t
 *
 * @brief process ZCL messages
 *
 * @return void
 *
 * @note
 *
 * imported description
 ********************************************************************************/
PUBLIC void APP_vProcessZCLMessage(uintptr_t uiMsg)
{
    tsZCL_CallBackEvent sCallBackEvent;
    ZPS_tsAfEvent sStackEvent;
#ifdef APP_ENABLE_PRINT_BUFFERS
    static uint16 u16appPrintBufferTimeInSecCount = 0U;
#endif

    if (*((uint8 *)uiMsg) == (uint32)APP_MSG_TYPE_ZCL_TIMER)
    {
        sCallBackEvent.eEventType = E_ZCL_CBET_TIMER;
        vLockZCLMutex();

        vZCL_EventHandler(&sCallBackEvent);
        vUnlockZCLMutex();

#ifdef APP_ENABLE_PRINT_BUFFERS
        if (u16appPrintBufferTimeInSec > 0)
        {
            u16appPrintBufferTimeInSecCount++;
            if (u16appPrintBufferTimeInSecCount == u16appPrintBufferTimeInSec)
            {
                u16appPrintBufferTimeInSecCount = 0U;
                vSL_PrintRxBufferPool(TRUE);
                PDUM_vPrintAllocatedBuffers(TRUE);
            }
        }
#endif
    }
    else if (*((uint8 *)uiMsg) == SL_MSG_TYPE_APDU)
    {
        PDUM_thAPduInstance myPDUM_thAPduInstance = PDUM_INVALID_HANDLE;
        uint8 u8EndPoint;

        /* clear StackEvent */
        (void)ZBmemset(&sStackEvent, 0x00, sizeof(ZPS_tsAfEvent));
        sCallBackEvent.eEventType = E_ZCL_CBET_ZIGBEE_EVENT;
        sCallBackEvent.pZPSevent  = &sStackEvent;

        /* Process the serial buffer */
        vSL_HandleApduEvent((uint8 *)uiMsg, &myPDUM_thAPduInstance, &sStackEvent);

        /* Before pushing the event to ZCL, construct the serial
         * payload as per stack event structure */
        if ((sStackEvent.eType == (ZPS_teAfEventType)ZPS_EVENT_APS_DATA_INDICATION) ||
            (sStackEvent.eType == (ZPS_teAfEventType)ZPS_EVENT_APS_DATA_ACK))
        {
            bool bValidEp = (bool)TRUE;
            if (sStackEvent.eType == (ZPS_teAfEventType)ZPS_EVENT_APS_DATA_INDICATION)
            {
                u8EndPoint = sStackEvent.uEvent.sApsDataIndEvent.u8DstEndpoint;
                (void)ZPS_eAplAfGetEndpointState(sStackEvent.uEvent.sApsDataIndEvent.u8DstEndpoint, &bValidEp);
            }
            else /* sStackEvent.eType ==(ZPS_teAfEventType)ZPS_EVENT_APS_DATA_ACK */
            {
                u8EndPoint = sStackEvent.uEvent.sApsDataAckEvent.u8DstEndpoint;
                (void)ZPS_eAplAfGetEndpointState(sStackEvent.uEvent.sApsDataAckEvent.u8DstEndpoint, &bValidEp);
            }

            if (bValidEp)
            {
                if (u8EndPoint == COORDINATOR_ZDO_ENDPOINT)
                {
                    vAppHandleZdoEvents((BDB_tsZpsAfEvent *)&sStackEvent);
                }
                else
                {
                    vLockZCLMutex();
                    /* post to the ZCL as Event */
                    vZCL_EventHandler(&sCallBackEvent);
                    vUnlockZCLMutex();
                }
            }
            else
            {
                if (sStackEvent.eType == (ZPS_teAfEventType)ZPS_EVENT_APS_DATA_INDICATION)
                {
                    DBG_vPrintf((bool_t)TRUE, "Data Indication for unsupported end point %d\n",
                                sStackEvent.uEvent.sApsDataIndEvent.u8DstEndpoint);
                }
                else
                {
                    DBG_vPrintf((bool_t)TRUE, "Data Ack for unspported end point %d\n",
                                sStackEvent.uEvent.sApsDataAckEvent.u8DstEndpoint);
                }
            }
        }
        if (myPDUM_thAPduInstance != PDUM_INVALID_HANDLE)
        {
            (void)PDUM_eAPduFreeAPduInstance(myPDUM_thAPduInstance);
        }
    }
    else if (*((uint8 *)uiMsg) == SL_MSG_TYPE_INTERPAN)
    {
        PDUM_thAPduInstance myPDUM_thAPduInstance = PDUM_INVALID_HANDLE;
        /* clear StackEvent */
        (void)ZBmemset(&sStackEvent, 0x00, sizeof(ZPS_tsAfEvent));
        sCallBackEvent.eEventType = E_ZCL_CBET_ZIGBEE_EVENT;
        sCallBackEvent.pZPSevent  = &sStackEvent;

        /* Process the serial buffer */
        vSL_HandleInterpanEvent((uint8 *)uiMsg, &myPDUM_thAPduInstance, &sStackEvent);

        /* Before pushing the event to ZCL, construct the serial payload as per
         * stack event structure */
        if ((sStackEvent.eType == (ZPS_teAfEventType)ZPS_EVENT_APS_INTERPAN_DATA_INDICATION) ||
            (sStackEvent.eType == (ZPS_teAfEventType)ZPS_EVENT_APS_INTERPAN_DATA_CONFIRM))
        {
            /* post to the ZCL as Event */
            vZCL_EventHandler(&sCallBackEvent);
        }
        if (myPDUM_thAPduInstance != PDUM_INVALID_HANDLE)
        {
            (void)PDUM_eAPduFreeAPduInstance(myPDUM_thAPduInstance);
        }
    }
    else
    {
        /*nodefault action required */
    }
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
