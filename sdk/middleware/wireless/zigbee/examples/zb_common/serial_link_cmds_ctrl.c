/*
* Copyright 2023-2024 NXP
* All rights reserved.
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifdef DEBUG_SERIAL_LINK
#define DEBUG_SL            TRUE
#else
#define DEBUG_SL            FALSE
#endif

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include "jendefs.h"
#include "app_zcl_task.h"
#include "app_common.h"
#include "app_main.h"
#include "serial_link_ctrl.h"
#include "serial_link_cmds_ctrl.h"
#include <string.h>
#include "dbg.h"
#include "zps_apl_zdo.h"
#include "bdb_api.h"
#if (BDB_SET_DEFAULT_TC_POLICY == TRUE)
#include "bdb_tkd.h"
#endif

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#ifndef TRACE_SL
#define TRACE_SL    FALSE
#endif

#define PKT_LEN_MORE_THAN_100_BYTES    100U

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
PRIVATE uint32 u32SL_ConvBiToU32(uint8 *pu8DataPtr);
PRIVATE uint64 u64SL_ConvBiToU64(uint8 *pu8DataPtr);
PRIVATE void vSL_ConvU32ToBi(uint32 u32Val, uint8 *pu8DataPtr);
PRIVATE void vSL_ConvU64ToBi(uint64 u64Val, uint8 *pu8DataPtr);
PRIVATE uint16 u16SL_ParseApsDataIndication(
        uint8                       *pu8Msg,
        ZPS_tsAfEvent               *psStackEvent,
        PDUM_thAPduInstance         *pmyPDUM_thAPduInstance);
PRIVATE void vSetStayAwakeFlagIfNeeded(uint16 u16Length);
PRIVATE void vResetStayAwakeFlagIfNeeded(uint16 u16Length);
PRIVATE bool_t bFromU8(uint8 u8Val);
PRIVATE uint8  u8FromBool(bool_t bVal);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
void *zps_g_pvApl = NULL;
bool_t bIsJNInDeepSleep = (bool_t)FALSE;
bool_t bIsReadingZBVersion = (bool_t)FALSE;
PRIVATE uint32 u32FormationStartTime;
PRIVATE bool_t bIsInProgramMode;

#define ZERO_KEY {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

ZPS_tsAplApsKeyDescriptorEntry s_keyPairTableStorage[4] = {
        {0, 0x0000, ZERO_KEY, 0x00},
        {0, 0x0000, ZERO_KEY, 0x00},
        {0, 0x0000, ZERO_KEY, 0x00},
        {0, 0x0000, ZERO_KEY, 0x00}
};

ZPS_tsAplApsKeyDescriptorEntry  *psAplDefaultDistributedAPSLinkKey = &s_keyPairTableStorage[0];
ZPS_tsAplApsKeyDescriptorEntry  *psAplDefaultGlobalAPSLinkKey = &s_keyPairTableStorage[1];
ZPS_tsAplApsKeyDescriptorEntry  *psAplDefaultTCAPSLinkKey = &s_keyPairTableStorage[2];
ZPS_tsAplApsKeyDescriptorEntry  *psAplTCAPSLinkKey = &s_keyPairTableStorage[3];

uint8 u8NwkKey;

uint32 u32ApsChannelMask[MAX_NUM_OF_CHANNEL_MASK] =
        {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000};

uint8 u8NumberOfNetworks;
ZPS_tsNwkNetworkDescr psNwkDescriptors[ZPS_NWK_MAX_DISC_NWK_DESCRS]; 

ZPS_tsAplApsKeyDescriptorEntry sKeyDescriptorEntry = {0, 0x0000, ZERO_KEY, 0x00};

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/********************************************************************************
  *
  * @fn PUBLIC bool_t vSetIsInProgramMode
  *
  */
 /**
  *
  * @param bool_t
  *
  *
  * @brief set bIsInProgramMode to true or false
  *
  * @return none
  *
  * @note
  *
 ********************************************************************************/
PUBLIC void vSetIsInProgramMode(bool_t bSetIsInProgramMode)
{
    bIsInProgramMode = bSetIsInProgramMode;
}

/********************************************************************************
  *
  * @fn PUBLIC bool_t bGetIsInProgramMode
  *
  */
 /**
  *
  * @param None
  *
  *
  * @brief Get bIsInProgramMode value
  *
  * @return bIsInProgramMode value
  *
  * @note
  *
 ********************************************************************************/
PUBLIC bool_t bGetIsInProgramMode(void)
{
    return bIsInProgramMode;
}

/********************************************************************************
  *
  * @fn PUBLIC void vProcessIncomingSerialBytes
  *
  */
 /**
  *
  * @param u8SerialData  R Recived bytes
  *
  *
  * @brief process the incoming bytes from serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vProcessIncomingSerialBytes(uint8 u8SerialData)
{
    uint8 *pu8Msg;

    if(bGetIsInProgramMode())
    {
        if((bool_t)TRUE == bSL_ReadJnBootLoaderMessage(u8SerialData))
        {
            if(sSLCommon.bIsWaitingStatus)
            {
                pu8Msg = sSLCommon.pu8CurrentRxBuffer;
                vSL_PostMessageToSerialQueue(&pu8Msg);
            }
        }
    }
    else
    {
        pu8Msg =  pSL_ReadMessage(u8SerialData);
        if(NULL != pu8Msg)
        {
            vSL_PostMessageToSerialRxQueue(&pu8Msg);
        }
    }
}

/********************************************************************************
  *
  * @fn PUBLIC void vProcessIncomingSerialCommands
  *
  */
 /**
  *
  * @param pu8RxBuffer  R pointer to the recived command
  *
  *
  * @brief Process incoming commands from serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vProcessIncomingSerialCommands(uint8 *pu8RxBuffer)
{
    uint8 *pu8Msg;
    static uint8 u8MsgType;

    union
    {
        uint8*  pu8;
        uint16* pu16;
    } upMsg;

    /* check Serial Packet Type */
    upMsg.pu8 = pu8RxBuffer + SL_MSG_TYPE_IDX;
    uint16 u16PktType = *upMsg.pu16;

    pu8Msg = pu8RxBuffer;
    DBG_vPrintf((bool_t)DEBUG_SL, "Got complete message: Packet Type 0x%2x\r\n", u16PktType );

    /* protect serial buffer by always leaving free entries when being hit with lots of traffic */
    if (((uint16)E_SL_MSG_DATA_INDICATION == u16PktType) || ((uint16)E_SL_MSG_INTERPAN_DATA_INDICATION == u16PktType))
    {
#if (MIN_RX_SERIAL_BUFFER_TO_HANDLE_DATA_INDICATION > 0)
        if (u32SL_GetNumberOfFreeRxBuffers() < MIN_RX_SERIAL_BUFFER_TO_HANDLE_DATA_INDICATION)
        {
            DBG_vPrintf((bool_t)TRUE, "Ignoring received data due to high traffic %d\r\n",
                    u32SL_GetNumberOfFreeRxBuffers() );
            vSL_FreeRxBuffer(pu8RxBuffer);
            return;
        }
#endif
    }

    /* check Pkt Type */
    switch(u16PktType)
    {
    case (uint16)E_SL_MSG_STATUS_SHORT_MSG:
    case (uint16)E_SL_MSG_STATUS_MSG:
    case (uint16)E_SL_MSG_HOST_JN_NACK:
        if( sSLCommon.bIsWaitingStatus)
        {
            vSL_PostMessageToSerialQueue(&pu8Msg); /* buffer is freed in u8SL_WriteMessage*/
        }
        else
        {
            DBG_vPrintf((bool_t)TRUE, "Status msg received, not posted to queue\r\n");
            /* free the buffer */
            vSL_FreeRxBuffer(pu8RxBuffer);
        }
        return;

    case (uint16)E_SL_MSG_EVENT_RESET:
    {
        u8MsgType = SL_MSG_TYPE_JN_RESETED;

        /* post a message to ESI (App) task */
        APP_vPostToAppQueueWord (u8MsgType);

        vSL_FreeRxBuffer(pu8RxBuffer);
    }
    break;
    case (uint16)E_SL_MSG_EVENT_OL_RESET:
    {
        *pu8RxBuffer = SL_MSG_TYPE_OL_RESETED;
        /* post a message to ESI (App) task */

        APP_vPostToAppQueue ((void*)&pu8Msg);
    }
    break;
    case (uint16)E_SL_MSG_EVENT_OL_REALIGN:
    {
        DBG_vPrintf((bool_t)TRUE, "OL Realignment Required Detected\n");
        vSL_FreeRxBuffer(pu8RxBuffer);
    }
    break;
    case (uint16)E_SL_MSG_EVENT_OL_ASSERT:
    {
        uint8 u8Length = *(pu8RxBuffer+SL_MSG_DATA_START_IDX);
        DBG_vPrintf((bool_t)TRUE, "OL Watchdog Assert length %d\n", u8Length);
        uint8* pu8Ptr = pu8RxBuffer+SL_MSG_DATA_START_IDX+1;
        uint8 u8i;
        for (u8i=0U; u8i<u8Length; u8i++)
        {
            DBG_vPrintf((bool_t)TRUE, "%02x ", *pu8Ptr++);
        }
        DBG_vPrintf((bool_t)TRUE, "\n");
        vSL_FreeRxBuffer(pu8RxBuffer);
    }
    break;
    case (uint16)E_SL_MSG_EVENT_OL_BEACON:
    {
        uint8 u8Length = *(pu8RxBuffer+SL_MSG_DATA_START_IDX);
        uint8* pu8Ptr = pu8RxBuffer+SL_MSG_DATA_START_IDX+1;
        if (*pu8Ptr == 0xebU)
        {
            DBG_vPrintf((bool_t)TRUE, "Rejected EBR\r\n");
        }
        else if (*pu8Ptr == 0xdbU)
        {
            DBG_vPrintf((bool_t)TRUE, "LBT Fail\r\n");
        }
        else if (*pu8Ptr == 0x11U)
        {
            DBG_vPrintf((bool_t)TRUE, "Watchdog\r\n");
        }
        else if (*pu8Ptr == 0x14U)
        {
            DBG_vPrintf((bool_t)TRUE, "Soft Reset mlme\r\n");
        }
        else
        {
            DBG_vPrintf((bool_t)TRUE, "Undefined OL DBG Msg\r\n");
        }

        uint8 u8i;
        for (u8i=0U; u8i<u8Length; u8i++)
        {
            DBG_vPrintf((bool_t)TRUE, "%02x ", *pu8Ptr++);
        }
        DBG_vPrintf((bool_t)TRUE, "\r\n");
        vSL_FreeRxBuffer(pu8RxBuffer);
    }
    break;
    case (uint16)E_SL_MSG_STACK_STARTED_RUNNING:
    {
       u8MsgType = SL_MSG_STACK_STARTED_RUNNING;
       APP_vPostToAppQueueWord (u8MsgType);
       vSL_FreeRxBuffer(pu8RxBuffer);
    }
    break;
    case (uint16)E_SL_MSG_JN_PRE_SLEEP_INDICATION:
    {
        bIsJNInDeepSleep = (bool_t)TRUE;
        vSL_FreeRxBuffer(pu8RxBuffer);
    }
    break;
    case (uint16)E_SL_MSG_JN_WAKE_UP_INDICATION:
    {
        bIsJNInDeepSleep = (bool_t)FALSE;
        vSL_FreeRxBuffer(pu8RxBuffer);
    }
    break;

    case (uint16)E_SL_MSG_LOG:
    {
        DBG_vPrintf((bool_t)TRUE, "\n*** %s\r\n",pu8RxBuffer+SL_MSG_DATA_START_IDX);

        vSL_FreeRxBuffer(pu8RxBuffer);
    }
    break;

    case (uint16)E_SL_MSG_MODULE_TEST:
    {
        DBG_vPrintf((bool_t)TRUE, "%s", pu8RxBuffer+SL_MSG_DATA_START_IDX);
        vSL_FreeRxBuffer(pu8RxBuffer);
    }
    break;

    case (uint16)E_SL_MSG_MAC_POLL_FAILURE:
    {
        u8MsgType = SL_MSG_TYPE_MAC_POLL_FAILURE;

        /* post a message to ESI (App) task */
        APP_vPostToAppQueueWord (u8MsgType);
        vSL_FreeRxBuffer(pu8RxBuffer);

        break;
    }
    case (uint16)E_SL_MSG_MAC_RESTORED:
    {
        u8MsgType = SL_MSG_TYPE_MAC_RESTORED;

        /* post a message to ESI (App) task */
        APP_vPostToAppQueueWord (u8MsgType);
        vSL_FreeRxBuffer(pu8RxBuffer);

        break;
    }
    case (uint16)E_SL_MSG_EXCEPTION:
        u8MsgType = SL_MSG_TYPE_EXCEPTION;
        DBG_vPrintf((bool_t)TRUE, "Exception received %s\r\n",pu8RxBuffer+SL_MSG_DATA_START_IDX);

        /* post message to the App Task queue */
        APP_vPostToAppQueueWord (u8MsgType);
        vSL_FreeRxBuffer(pu8RxBuffer);
        break;

    case (uint16)E_SL_MSG_TRANSPORT_KEY_DECIDER:
        /* Update 1st byte that will indicates the new node parent indication */
        *pu8RxBuffer = SL_MSG_TYPE_NODE_PARENT;
        /* post message to the App Task queue */
        APP_vPostToAppQueue((void*)&pu8Msg);
        break;

    case (uint16)E_SL_MSG_JEN_OS_ERROR:
        DBG_vPrintf((bool_t)TRUE, "Jen OS Error received %d\r\n",pu8RxBuffer[SL_MSG_DATA_START_IDX]);
        vSL_FreeRxBuffer(pu8RxBuffer);
        break;

    case (uint16)E_SL_MSG_PDM_ERROR_EVENT:
        DBG_vPrintf((bool_t)TRUE, "JN PDM Error=%d, received ", pu8RxBuffer[SL_MSG_DATA_START_IDX] & 0x0fU);
        if((pu8RxBuffer[SL_MSG_DATA_START_IDX] >> 4U) == (uint8)E_SL_PDM_CMD_INIT)
        {
            DBG_vPrintf((bool_t)TRUE, "during init\r\n");
        }
        else if((pu8RxBuffer[SL_MSG_DATA_START_IDX] >> 4U) == (uint8)E_SL_PDM_CMD_SAVE)
        {
            DBG_vPrintf((bool_t)TRUE, "during save\r\n");
        }
        else if((pu8RxBuffer[SL_MSG_DATA_START_IDX] >> 4U) == (uint8)E_SL_PDM_CMD_READ)
        {
            DBG_vPrintf((bool_t)TRUE, "during read\r\n");
        }
        else
        {
            if((pu8RxBuffer[SL_MSG_DATA_START_IDX] >> 4U) == (uint8)E_SL_PDM_CMD_CALLBACK)
            {
                DBG_vPrintf((bool_t)TRUE, "through event handler callback with event number %d\r\n",
                        u32SL_ConvBiToU32(&pu8RxBuffer[SL_MSG_DATA_START_IDX + 1U]));
            }
        }
        vSL_FreeRxBuffer(pu8RxBuffer);
        break;
    case (uint16)E_SL_MSG_ZDP_DATA_INDICATION:
        /* Update 1st byte that will indicates the Network indication */
        *pu8RxBuffer = SL_MSG_TYPE_ZDP_MSG;
        /* post message to the App Task queue */

        APP_vPostToAppQueue((void*)&pu8Msg);
        break;
    case (uint16)E_SL_MSG_DATA_INDICATION:
    case (uint16)E_SL_MSG_DATA_ACK:
    case (uint16)E_SL_MSG_DATA_CONFIRM:
        /* Update 1st byte that will indicates the APS indication */
        *pu8RxBuffer = SL_MSG_TYPE_APDU;
        /* post message to the ZCL Task queue */

        /* Ensure this byte does not contain  SL_MSG_TYPE_APDU_PROCESSED (127)
         * This is used to ensure the buffer is not freed until it has been processed
         * twice, once in the zcl queue and one fro the app queue (Comms Hub only)
         * see artf762249
         */
        pu8RxBuffer[SL_MSG_TX_SEQ_NO_IDX] = 0u;

        APP_vPostToZclQueue(pu8Msg);

        /* post to App queue for diagnostic stack */
        APP_vPostToAppQueue((void*)&pu8Msg);
        break;

    case (uint16)E_SL_MSG_INTERPAN_DATA_INDICATION:
    case (uint16)E_SL_MSG_INTERPAN_DATA_CONFIRM:
        /* Update 1st byte that will indicates the Interpan data indication */
        *pu8RxBuffer = SL_MSG_TYPE_INTERPAN;

        /* post message to the ZCL Task queue */
        APP_vPostToZclQueue(pu8Msg);
        break;

    case (uint16)E_SL_MSG_NEW_NODE_HAS_JOINED:
    case (uint16)E_SL_MSG_NETWORK_JOINED_FORMED:
    case (uint16)E_SL_MSG_LEAVE_INDICATION:
    case (uint16)E_SL_MSG_MANAGEMENT_LEAVE_RESPONSE:
    case (uint16)E_SL_MSG_DEVICE_ANNOUNCE:
    case (uint16)E_SL_MSG_MATCH_DESCRIPTOR_RESPONSE:
    case (uint16)E_SL_MSG_IEEE_ADDRESS_RESPONSE:
    case (uint16)E_SL_MSG_BIND_RESPONSE:
    case (uint16)E_SL_MSG_UNBIND_RESPONSE:
    case (uint16)E_SL_MSG_ZDO_LINK_KEY_EVENT:
    case (uint16)E_SL_MSG_NODE_DESCRIPTOR_RESPONSE:
    case (uint16)E_SL_MSG_SIMPLE_DESCRIPTOR_RESPONSE:
    case (uint16)E_SL_MSG_NWK_DISCOVERY_COMPLETE:
    case (uint16)E_SL_MSG_NETWORK_ADDRESS_RESPONSE:
    case (uint16)E_SL_MSG_NWK_JOIN_TIMEOUT:
    case (uint16)E_SL_MSG_MANAGEMENT_NETWORK_UPDATE_RESPONSE:
    case (uint16)E_SL_MSG_NWK_ED_SCAN:
    case (uint16)E_SL_MSG_MGMT_NETWORK_ENH_UPDATE_RESPONSE:
    case (uint16)E_SL_MSG_MGMT_NETWORK_UNS_ENH_UPDATE_NOTIFY:
    case (uint16)E_SL_MSG_MGMT_NWK_UPDATE_REQ:
    case (uint16)E_SL_MSG_MGMT_ENHANCED_NWK_UPDATE_REQ:
    case (uint16)E_SL_MSG_NWK_STATUS_INDICATION:
    case (uint16)E_SL_MSG_ACTIVE_ENDPOINT_RESPONSE:
    case (uint16)E_SL_MSG_LEAVE_CONFIRM:
    case (uint16)E_SL_MSG_ZPS_ERROR:
    case (uint16)E_SL_MSG_SERIAL_LINK_BAD_TABLE:
    case (uint16)(E_SL_MSG_SERIAL_LINK_BIND_INDICATION):
    case (uint16)E_SL_MSG_ZPS_INTERNAL_ERROR:
    case (uint16)E_SL_MSG_ZPS_FRAME_COUNTER_HIGH:
    case (uint16)E_SL_MSG_NWK_ROUTE_DISCOVERY:
    case (uint16)E_SL_MSG_PANID_CNFL_INDICATION:
    case (uint16)E_SL_MSG_TC_STATUS:
    case (uint16)E_SL_MSG_ZDO_BIND_EVENT:

        if (u16PktType == (uint16)E_SL_MSG_NETWORK_JOINED_FORMED)
        {
            uint32 u32Stop = zbPlatGetTime();

            DBG_vPrintf((bool_t)TRUE, "Nwk formation/joining took %d MS\n", (u32Stop - u32FormationStartTime));
            vSL_SetStandardResponsePeriod();
        }

        /* Update 1st byte that will indicates the Network indication */
        *pu8RxBuffer = SL_MSG_TYPE_NWK;
        /* post message to the App Task queue */
        APP_vPostToAppQueue((void*)&pu8Msg);
        break;

    default:
        DBG_vPrintf((bool_t)DEBUG_SL, "Error: E_SL_MSG_STATUS_UNHANDLED_COMMAND\r\n");
        vSL_FreeRxBuffer(pu8RxBuffer);
        break;
    }
}

/********************************************************************************
  *
  * @fn PUBLIC void vProcessIncomingSerialLogCommand
  *
  */
 /**
  *
  * @param pu8RxBuffer  R pointer to the received command
  *
  *
  * @brief Process incoming log commands from serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vProcessIncomingSerialLogCommand(uint8 *pu8RxBuffer)
{
    if((bool_t)FALSE == bSL_ValidateIncomingMessage(pu8RxBuffer))
    {
        vSL_FreeRxBuffer(pu8RxBuffer);
        return;
    }

    union
    {
        uint8*  pu8;
        uint16* pu16;
    } upMsg;

    /* check Serial Packet Type */
    upMsg.pu8 = pu8RxBuffer + SL_MSG_TYPE_IDX;
    uint16 u16PktType = *upMsg.pu16;

    DBG_vPrintf((bool_t)DEBUG_SL, "Got complete message: Packet Type 0x%2x\r\n", u16PktType );

    /* check Pkt Type */
    switch(u16PktType)
    {
    case (uint16)E_SL_MSG_STATUS_SHORT_MSG:
            break;
    case (uint16)E_SL_MSG_STATUS_MSG:
        break;
    case (uint16)E_SL_MSG_EVENT_RESET:
        break;
    case (uint16)E_SL_MSG_JN_PRE_SLEEP_INDICATION:
        break;
    case (uint16)E_SL_MSG_JN_WAKE_UP_INDICATION:
        break;

    case (uint16)E_SL_MSG_LOG:
    {
        DBG_vPrintf((bool_t)TRUE, "\n>>> %s\r\n", pu8RxBuffer+6);
    }
    break;

    case (uint16)E_SL_MSG_EXCEPTION:
         break;

    case (uint16)E_SL_MSG_TRANSPORT_KEY_DECIDER:
        break;

    case (uint16)E_SL_MSG_JEN_OS_ERROR:
        break;

    case (uint16)E_SL_MSG_ZPS_ERROR:
        break;

    case (uint16)E_SL_MSG_PDM_ERROR_EVENT:
        break;

    case (uint16)E_SL_MSG_DATA_INDICATION:
    case (uint16)E_SL_MSG_DATA_ACK:
    case (uint16)E_SL_MSG_DATA_CONFIRM:
        break;

    case (uint16)E_SL_MSG_INTERPAN_DATA_INDICATION:
    case (uint16)E_SL_MSG_INTERPAN_DATA_CONFIRM:
        break;

    case (uint16)E_SL_MSG_NEW_NODE_HAS_JOINED:
    case (uint16)E_SL_MSG_NETWORK_JOINED_FORMED:
    case (uint16)E_SL_MSG_LEAVE_INDICATION:
    case (uint16)E_SL_MSG_DEVICE_ANNOUNCE:
    case (uint16)E_SL_MSG_MATCH_DESCRIPTOR_RESPONSE:
    case (uint16)E_SL_MSG_IEEE_ADDRESS_RESPONSE:
    case (uint16)E_SL_MSG_BIND_RESPONSE:
    case (uint16)E_SL_MSG_UNBIND_RESPONSE:
    case (uint16)E_SL_MSG_ZDO_LINK_KEY_EVENT:
    case (uint16)E_SL_MSG_NODE_DESCRIPTOR_RESPONSE:
    case (uint16)E_SL_MSG_SIMPLE_DESCRIPTOR_RESPONSE:
    case (uint16)E_SL_MSG_NWK_DISCOVERY_COMPLETE:
    case (uint16)E_SL_MSG_NETWORK_ADDRESS_RESPONSE:
    case (uint16)E_SL_MSG_NWK_JOIN_TIMEOUT:
    case (uint16)E_SL_MSG_MANAGEMENT_NETWORK_UPDATE_RESPONSE:
    case (uint16)E_SL_MSG_NWK_ED_SCAN:
    case (uint16)E_SL_MSG_MGMT_NETWORK_ENH_UPDATE_RESPONSE:
    case (uint16)E_SL_MSG_MGMT_NETWORK_UNS_ENH_UPDATE_NOTIFY:
    case (uint16)E_SL_MSG_LEAVE_CONFIRM:
    case (uint16)(E_SL_MSG_SERIAL_LINK_BIND_INDICATION):
        break;

    default:
        DBG_vPrintf((bool_t)DEBUG_SL, "Error: E_SL_MSG_STATUS_UNHANDLED_COMMAND\r\n");
        break;
    }
    vSL_FreeRxBuffer(pu8RxBuffer);
}


/********************************************************************************
  *
  * @fn PUBLIC void vSL_HandleApduEvent
  *
  */
 /**
  *
  * @param pu8RxSerialMsg uint8 *
  * @param pmyPDUM_thAPduInstance PDUM_thAPduInstance *
  * @param psStackEvent ZPS_tsAfEvent *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_HandleApduEvent(
                    uint8                       *pu8RxSerialMsg,
                    PDUM_thAPduInstance *pmyPDUM_thAPduInstance,
                    ZPS_tsAfEvent               *psStackEvent)
{
    uint16 u16PktType;
    uint16 u16PktLength;
    uint16 u16Len = 0U;
    uint8 *pu8Msg = pu8RxSerialMsg + SL_MSG_DATA_START_IDX;
    union
    {
        uint8*  pu8;
        uint16* pu16;
    } upMsg;
    uint8 u8TempStatus = 0u;
    upMsg.pu8 = pu8RxSerialMsg + SL_MSG_TYPE_IDX;
    u16PktType = *upMsg.pu16;
    upMsg.pu8 = pu8RxSerialMsg + SL_MSG_RX_LEN_IDX;
    u16PktLength = *upMsg.pu16;

    /* check Pkt type */
    if(u16PktType == (uint16)E_SL_MSG_DATA_INDICATION)
    {
        /* Update Event type */
        psStackEvent->eType =(ZPS_teAfEventType)ZPS_EVENT_APS_DATA_INDICATION;

        u16Len = u16SL_ParseApsDataIndication(pu8Msg, psStackEvent, pmyPDUM_thAPduInstance);
    }
    else
    {
        if(u16PktType == (uint16)E_SL_MSG_DATA_ACK)
        {
            /* Update Event type */
            psStackEvent->eType =(ZPS_teAfEventType)ZPS_EVENT_APS_DATA_ACK;

            /* Update Event Data Ack status */
            psStackEvent->uEvent.sApsDataAckEvent.u8Status = *(pu8Msg+u16Len++);
            u8TempStatus = psStackEvent->uEvent.sApsDataAckEvent.u8Status;

            /* Update Event Data Ind Ack Src EP */
            psStackEvent->uEvent.sApsDataAckEvent.u8SrcEndpoint = *(pu8Msg+u16Len++);

            /* Update Event Data Ind Ack Dst EP */
            psStackEvent->uEvent.sApsDataAckEvent.u8DstEndpoint = *(pu8Msg+u16Len++);

            /* Update Event Data Ind Ack Dst Addr Mode */
            psStackEvent->uEvent.sApsDataAckEvent.u8DstAddrMode = *(pu8Msg+u16Len++);

            /* Update Event Data Ind Ack Dst Addr */
            psStackEvent->uEvent.sApsDataAckEvent.u16DstAddr = ((uint16)*(pu8Msg+u16Len++)) << 8U;
            psStackEvent->uEvent.sApsDataAckEvent.u16DstAddr += *(pu8Msg+u16Len++);

            /* Update Event Data Ind Ack Seq Num */
            psStackEvent->uEvent.sApsDataAckEvent.u8SequenceNum = *(pu8Msg+u16Len++);

            /* Update Event Data Ind Ack profile Id */
            psStackEvent->uEvent.sApsDataAckEvent.u16ProfileId = ((uint16)*(pu8Msg+u16Len++)) << 8U;
            psStackEvent->uEvent.sApsDataAckEvent.u16ProfileId += *(pu8Msg+u16Len++);

            /* Update Event Data Ind Ack Cluster Id */
            psStackEvent->uEvent.sApsDataAckEvent.u16ClusterId = ((uint16)*(pu8Msg+u16Len++)) << 8U;
            psStackEvent->uEvent.sApsDataAckEvent.u16ClusterId += *(pu8Msg+u16Len++);
            if((psStackEvent->uEvent.sApsDataAckEvent.u8Status != 0U))
            {
                DBG_vPrintf((bool_t)TRUE, "psStackEvent->uEvent.sApsDataAckEvent.u8Status   %d u8SequenceNum %d u16Addr 0x%4x, Mode %d\r\n",
                        psStackEvent->uEvent.sApsDataAckEvent.u8Status ,
                        psStackEvent->uEvent.sApsDataAckEvent.u8SequenceNum ,
                        psStackEvent->uEvent.sApsDataAckEvent.u16DstAddr,
                        psStackEvent->uEvent.sApsDataAckEvent.u8DstAddrMode);
            }
        }
        else
        {
            if(u16PktType == (uint16)E_SL_MSG_DATA_CONFIRM)
            {
                /* Update Event type */
                psStackEvent->eType =(ZPS_teAfEventType)ZPS_EVENT_APS_DATA_CONFIRM;

                /* Update Event Data Confirm status */
                psStackEvent->uEvent.sApsDataConfirmEvent.u8Status = *(pu8Msg+u16Len++);
                u8TempStatus = psStackEvent->uEvent.sApsDataConfirmEvent.u8Status;


                /* Update Event Data Confirm Src EP */
                psStackEvent->uEvent.sApsDataConfirmEvent.u8SrcEndpoint = *(pu8Msg+u16Len++);

                /* Update Event Data Confirm Dst EP */
                psStackEvent->uEvent.sApsDataConfirmEvent.u8DstEndpoint = *(pu8Msg+u16Len++);

                /* Update Event Data Confirm Dst Addr Mode */
                psStackEvent->uEvent.sApsDataConfirmEvent.u8DstAddrMode = *(pu8Msg+u16Len++);

                /*check Dst Addt mode */
                if(psStackEvent->uEvent.sApsDataConfirmEvent.u8DstAddrMode == (uint8)ZPS_E_ADDR_MODE_IEEE)
                {
                    psStackEvent->uEvent.sApsDataConfirmEvent.uDstAddr.u64Addr = u64SL_ConvBiToU64(pu8Msg+u16Len);
                    u16Len += sizeof(uint64);
                }
                else
                {
                    psStackEvent->uEvent.sApsDataConfirmEvent.uDstAddr.u16Addr = ((uint16)*(pu8Msg+u16Len++)) << 8U;
                    psStackEvent->uEvent.sApsDataConfirmEvent.uDstAddr.u16Addr += *(pu8Msg+u16Len++);
                }

                /* Update Event Data Confirm Seq Num */
                psStackEvent->uEvent.sApsDataConfirmEvent.u8SequenceNum = *(pu8Msg+u16Len++);
            }
        }
    }

    if (u16PktLength != u16Len)
    {
        DBG_vPrintf((bool_t)TRUE, "WARNING: Apdu event %04x Received %d bytes Handled %d bytes\n",
                        u16PktType, u16PktLength, u16Len);
    }

    if(pu8RxSerialMsg[SL_MSG_TX_SEQ_NO_IDX] == SL_MSG_TYPE_APDU_PROCESSED)
    {
        vSL_FreeRxBuffer(pu8RxSerialMsg);
    }
    else
    {
        pu8RxSerialMsg[SL_MSG_TX_SEQ_NO_IDX] = SL_MSG_TYPE_APDU_PROCESSED;
    }

    if (u8TempStatus != 0u)
    {
        vMonitorAllJnErrors( u8TempStatus, (bool_t)FALSE);
    }
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_HandleZDPMsgEvent
  *
  */
 /**
  *
  * @param pu8RxSerialMsg uint8 *
  * @param pmyPDUM_thAPduInstance PDUM_thAPduInstance *
  * @param psStackEvent ZPS_tsAfEvent *
  * @param psApsZdpEvent ZPS_tsAfZdpEvent *
  *
  * @brief Parse ZDP message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_HandleZDPMsgEvent(
                    uint8                       *pu8RxSerialMsg,
                    PDUM_thAPduInstance         *pmyPDUM_thAPduInstance,
                    ZPS_tsAfEvent               *psStackEvent,
                    ZPS_tsAfZdpEvent            *psApsZdpEvent)
{
    uint8 *pu8Msg = pu8RxSerialMsg + SL_MSG_DATA_START_IDX;
    union
    {
        uint8*  pu8;
        uint16* pu16;
    } upMsg;
    upMsg.pu8 = pu8RxSerialMsg + SL_MSG_RX_LEN_IDX;
    uint16 u16PktLength = *upMsg.pu16;
    uint16 u16Len = 0U;

    /* Update Event type */
    psStackEvent->eType =(ZPS_teAfEventType)ZPS_EVENT_APS_DATA_INDICATION;
    u16Len = u16SL_ParseApsDataIndication(pu8Msg,psStackEvent,pmyPDUM_thAPduInstance);
    if (psStackEvent->uEvent.sApsDataIndEvent.u8DstEndpoint == 0U)
    {
            uint16 u16ClusterId = psStackEvent->uEvent.sApsDataIndEvent.u16ClusterId;
            switch (u16ClusterId)
            {
                /* do not check status for the following cases */
                case ZPS_ZDP_DEVICE_ANNCE_REQ_CLUSTER_ID:
                case ZPS_ZDP_MGMT_NWK_UPDATE_REQ_CLUSTER_ID:
                case ZPS_ZDP_MGMT_NWK_ENHANCE_UPDATE_REQ_CLUSTER_ID:
                case ZPS_ZDP_MGMT_PERMIT_JOINING_REQ_CLUSTER_ID:
                case ZPS_ZDP_MGMT_NWK_UNSOLICITED_ENHANCE_UPDATE_NOTIFY_CLUSTER_ID:
                    break;
                default:
                    /* Check status for errors */
                    if ( (bool_t)TRUE == zps_bAplZdpUnpackResponse (psStackEvent,psApsZdpEvent) )
                    {
                        if (psApsZdpEvent->uZdpData.sActiveEpRsp.u8Status != 0u)
                        {
                            vMonitorAllJnErrors(psApsZdpEvent->uZdpData.sActiveEpRsp.u8Status, (bool_t)FALSE );
                        }
                    }
                    break;
            }
    }

    if (u16PktLength != u16Len)
    {
        DBG_vPrintf((bool_t)TRUE, "WARNING: Zdp event Cluster %04x Received %d bytes Handled %d bytes\n",
                psApsZdpEvent->u16ClusterId, u16PktLength, u16Len);
    }
    vSL_FreeRxBuffer(pu8RxSerialMsg);
}
/********************************************************************************
  *
  * @fn PUBLIC void vSL_HandleNwkEvent
  *
  */
 /**
  *
  * @param pu8RxSerialMsg uint8 *
  * @param psStackEvent ZPS_tsAfEvent *
  * @param psApsZdpEvent ZPS_tsAfZdpEvent *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_HandleNwkEvent(
                    uint8                       *pu8RxSerialMsg,
                    ZPS_tsAfEvent               *psStackEvent,
                    ZPS_tsAfZdpEvent            *psApsZdpEvent)
{
    uint16 u16PktType;
    uint16 u16PktLength;
    uint16 u16Len = 0U;
    uint8 u8TempStatus = 0u;
    bool_t bExtended = (bool_t)FALSE;
    uint8 *pu8Msg = pu8RxSerialMsg + SL_MSG_DATA_START_IDX;

    union
    {
        uint8*  pu8;
        uint16* pu16;
    } upMsg;

    upMsg.pu8 = pu8RxSerialMsg + SL_MSG_TYPE_IDX;
    u16PktType = *upMsg.pu16;
    upMsg.pu8 = pu8RxSerialMsg + SL_MSG_RX_LEN_IDX;
    u16PktLength = *upMsg.pu16;

    /* check Pkt type */
    if(u16PktType == (uint16)E_SL_MSG_NEW_NODE_HAS_JOINED)
    {
        /* Update NWK Event type */
        psStackEvent->eType = ZPS_EVENT_NWK_NEW_NODE_HAS_JOINED;

        /* Update NWK Event Device Ext Addr */
        psStackEvent->uEvent.sNwkJoinIndicationEvent.u64ExtAddr = u64SL_ConvBiToU64(pu8Msg+u16Len);
        u16Len += sizeof(uint64);
        /* Update NWK Event Device NWK Addr */
        psStackEvent->uEvent.sNwkJoinIndicationEvent.u16NwkAddr = ((uint16)*(pu8Msg+u16Len++)) << 8U;
        psStackEvent->uEvent.sNwkJoinIndicationEvent.u16NwkAddr += ((uint16)*(pu8Msg+u16Len++));

        /* Update Event Data capability */
        psStackEvent->uEvent.sNwkJoinIndicationEvent.u8Capability = *(pu8Msg+u16Len++);

        /* Update Event Data Rejoin */
        psStackEvent->uEvent.sNwkJoinIndicationEvent.u8Rejoin = *(pu8Msg+u16Len++);

        /* Update Event Data Secure Re Join */
        psStackEvent->uEvent.sNwkJoinIndicationEvent.u8SecureRejoin = *(pu8Msg+u16Len++);
    }
    else if (u16PktType == (uint16)E_SL_MSG_NETWORK_JOINED_FORMED)
    {
        /* Update NWK Event type */
        uint8 u8Status;

        u8Status = *(pu8Msg+u16Len++);

        if(u8Status == 1U)
        {
            psStackEvent->eType = ZPS_EVENT_NWK_STARTED;
            psStackEvent->uEvent.sNwkFormationEvent.u8Status = *(pu8Msg+u16Len++);
            u8TempStatus = psStackEvent->uEvent.sNwkFormationEvent.u8Status;
        }
        else
        {
            if (u8Status == 0xC4U)
            {
                psStackEvent->eType = ZPS_EVENT_NWK_FAILED_TO_START;
                psStackEvent->uEvent.sNwkFormationEvent.u8Status = *(pu8Msg+u16Len++);
                u8TempStatus = psStackEvent->uEvent.sNwkFormationEvent.u8Status;
            }
            else if ((u8Status == (uint8)ZPS_EVENT_NWK_JOINED_AS_ENDDEVICE) ||
                    (u8Status == (uint8)ZPS_EVENT_NWK_JOINED_AS_ROUTER))
            {
                psStackEvent->eType = (ZPS_teAfEventType)u8Status;
                psStackEvent->uEvent.sNwkJoinedEvent.u16Addr = ((uint16)*(pu8Msg+u16Len++)) << 8U;
                psStackEvent->uEvent.sNwkJoinedEvent.u16Addr += ((uint16)*(pu8Msg+u16Len++));
                psStackEvent->uEvent.sNwkJoinedEvent.bRejoin = bFromU8(*(pu8Msg+u16Len++));
                psStackEvent->uEvent.sNwkJoinedEvent.bSecuredRejoin = bFromU8(*(pu8Msg+u16Len++));
            }
            else
            {
                if (u8Status == (uint8)ZPS_EVENT_NWK_FAILED_TO_JOIN)
                {
                    psStackEvent->eType = (ZPS_teAfEventType)u8Status;
                    psStackEvent->uEvent.sNwkJoinFailedEvent.u8Status = *(pu8Msg+u16Len++);
                    u8TempStatus = psStackEvent->uEvent.sNwkJoinFailedEvent.u8Status;
                    psStackEvent->uEvent.sNwkJoinFailedEvent.bRejoin = bFromU8(*(pu8Msg+u16Len++));
                }
            }
        }
    }
    else if (u16PktType == (uint16)E_SL_MSG_LEAVE_INDICATION)
    {
        /* Update NWK Event type */
        psStackEvent->eType = ZPS_EVENT_NWK_LEAVE_INDICATION;
        /* Update NWK Event Device Ext Addr */
        psStackEvent->uEvent.sNwkLeaveIndicationEvent.u64ExtAddr = u64SL_ConvBiToU64(pu8Msg+u16Len);
        u16Len += sizeof(uint64);

        /* Update Event Data Rejoin */
        psStackEvent->uEvent.sNwkLeaveIndicationEvent.u8Rejoin = *(pu8Msg+u16Len++);
    }
    else if (u16PktType == (uint16)E_SL_MSG_LEAVE_CONFIRM)
    {
        /* Update NWK Event type */
        psStackEvent->eType = ZPS_EVENT_NWK_LEAVE_CONFIRM;

        /* Update NWK Event Device Ext Addr */
        psStackEvent->uEvent.sNwkLeaveConfirmEvent.u64ExtAddr = u64SL_ConvBiToU64(pu8Msg+u16Len);
        u16Len += sizeof(uint64);
        psStackEvent->uEvent.sNwkLeaveConfirmEvent.bRejoin = bFromU8(*(pu8Msg+u16Len++));
        psStackEvent->uEvent.sNwkLeaveConfirmEvent.eStatus = *(pu8Msg+u16Len++);
        u8TempStatus = psStackEvent->uEvent.sNwkLeaveConfirmEvent.eStatus;
    }
    else if (u16PktType == (uint16)E_SL_MSG_MANAGEMENT_LEAVE_RESPONSE)
    {
        /* Update NWK Event type */
        psStackEvent->eType =(ZPS_teAfEventType)ZPS_EVENT_APS_DATA_INDICATION;
        psStackEvent->uEvent.sApsDataIndEvent.u8DstEndpoint = 0U;
        u8TempStatus = *(pu8Msg+u16Len++);
    }
    else if (u16PktType == (uint16)E_SL_MSG_ZDO_LINK_KEY_EVENT)
    {
        /* Update NWK Event type */
        psStackEvent->eType = ZPS_EVENT_ZDO_LINK_KEY;

        /* Update NWK Event Device Ext Addr */
        psStackEvent->uEvent.sZdoLinkKeyEvent.u64IeeeLinkAddr = u64SL_ConvBiToU64(pu8Msg+u16Len);
        u16Len += sizeof(uint64);
    }
    else if (u16PktType == (uint16)E_SL_MSG_NWK_DISCOVERY_COMPLETE)
    {
        uint8 i;
        /* Update NWK Event type */
        psStackEvent->eType = ZPS_EVENT_NWK_DISCOVERY_COMPLETE;
        /* Update NWK Discovery Event Unscanned Channels */
        psStackEvent->uEvent.sNwkDiscoveryEvent.u32UnscannedChannels = u32SL_ConvBiToU32(pu8Msg+u16Len);
        u16Len += sizeof(uint32);
        /* Update NWK Discovery Event status */
        psStackEvent->uEvent.sNwkDiscoveryEvent.eStatus = *(pu8Msg+u16Len++);
        u8TempStatus = psStackEvent->uEvent.sNwkDiscoveryEvent.eStatus;
        /* Update NWK Discovery Event Network Count */
        psStackEvent->uEvent.sNwkDiscoveryEvent.u8NetworkCount = *(pu8Msg+u16Len++);
        /* Update NWK Discovery Event Selected Network */
        psStackEvent->uEvent.sNwkDiscoveryEvent.u8SelectedNetwork = *(pu8Msg+u16Len++);

        /* Max 5 network supported */
        if (psStackEvent->uEvent.sNwkDiscoveryEvent.u8NetworkCount > APP_ZDP_MAX_NUM_NETWORK_DESCR)
        {
            psStackEvent->uEvent.sNwkDiscoveryEvent.u8NetworkCount = APP_ZDP_MAX_NUM_NETWORK_DESCR;
        }
        psStackEvent->uEvent.sNwkDiscoveryEvent.psNwkDescriptors = (ZPS_tsNwkNetworkDescr *)psApsZdpEvent->uLists.asNwkDescTable;
        /* Update NWK Discovery Event Network Descriptors */
        for(i=0U;i<psStackEvent->uEvent.sNwkDiscoveryEvent.u8NetworkCount;i++)
        {
            psStackEvent->uEvent.sNwkDiscoveryEvent.psNwkDescriptors[i].u64ExtPanId = u64SL_ConvBiToU64(pu8Msg+u16Len);
            u16Len += sizeof(uint64);
            psStackEvent->uEvent.sNwkDiscoveryEvent.psNwkDescriptors[i].u8LogicalChan = *(pu8Msg+u16Len++);
            psStackEvent->uEvent.sNwkDiscoveryEvent.psNwkDescriptors[i].u8StackProfile = *(pu8Msg+u16Len++);
            psStackEvent->uEvent.sNwkDiscoveryEvent.psNwkDescriptors[i].u8ZigBeeVersion = *(pu8Msg+u16Len++);
            psStackEvent->uEvent.sNwkDiscoveryEvent.psNwkDescriptors[i].u8PermitJoining = *(pu8Msg+u16Len++);
            psStackEvent->uEvent.sNwkDiscoveryEvent.psNwkDescriptors[i].u8RouterCapacity = *(pu8Msg+u16Len++);
            psStackEvent->uEvent.sNwkDiscoveryEvent.psNwkDescriptors[i].u8EndDeviceCapacity = *(pu8Msg+u16Len++);
        }
    }
    else if((uint16)E_SL_MSG_NWK_JOIN_TIMEOUT == u16PktType)
    {
        psStackEvent->eType = ZPS_EVENT_NWK_FAILED_TO_JOIN;
        psStackEvent->uEvent.sNwkJoinFailedEvent.u8Status = (uint8)ZPS_NWK_ENUM_NO_NETWORKS;
        u8TempStatus = (uint8)ZPS_NWK_ENUM_NO_NETWORKS;
        psStackEvent->uEvent.sNwkJoinFailedEvent.bRejoin = (bool_t)FALSE;
    }
    else if(u16PktType == (uint16)E_SL_MSG_TC_STATUS)
    {
        /* Update Event type */
        psStackEvent->eType = ZPS_EVENT_TC_STATUS;

        /* Copy TC status */
        uint8 u8TcStatus = *(pu8Msg+u16Len++);
        psStackEvent->uEvent.sApsTcEvent.u8Status = u8TcStatus;

        switch (u8TcStatus) 
        {
        case ZPS_APL_APS_E_SECURED_LINK_KEY:
        case ZPS_APL_APS_E_SECURITY_FAIL:
        {
            psStackEvent->uEvent.sApsTcEvent.uTcData.u64ExtendedAddress = u64SL_ConvBiToU64(pu8Msg+u16Len);
            u16Len += sizeof(uint64);
        }
        break;
        case ZPS_E_SUCCESS:
        {
            /* Copy outgoing frame counter */
            psAplTCAPSLinkKey->u32OutgoingFrameCounter = u32SL_ConvBiToU32(pu8Msg+u16Len);
            u16Len += (uint16)sizeof(uint32);

            /* Copy ext address lookup */
            psAplTCAPSLinkKey->u16ExtAddrLkup = ((uint16)*(pu8Msg+u16Len++))  << 8U;
            psAplTCAPSLinkKey->u16ExtAddrLkup += *(pu8Msg+u16Len++);

            /* Copy TC APS link key */
            (void)ZBmemcpy( psAplTCAPSLinkKey->au8LinkKey, pu8Msg, ZPS_SEC_KEY_LENGTH);
            u16Len += ZPS_SEC_KEY_LENGTH;

            /* Copy bitmap sec level */
            psAplTCAPSLinkKey->u8BitMapSecLevl = *(pu8Msg+u16Len++);

            psStackEvent->uEvent.sApsTcEvent.uTcData.pKeyDesc = psAplTCAPSLinkKey;
        }
        break;
        default:
            DBG_vPrintf(TRUE, "Unhandled ZPS_EVENT_TC_STATUS status %x", u8TcStatus);
            break;
        }
    }
    else if (u16PktType == (uint16)E_SL_MSG_NWK_ED_SCAN)
    {
        /* Update NWK Event type */
        psStackEvent->eType = ZPS_EVENT_NWK_ED_SCAN;

        /* copy the contents of the NWK_ED_SCAN message */
        psStackEvent->uEvent.sNwkEdScanConfirmEvent.u8Status = *(pu8Msg+u16Len++);
        u8TempStatus = psStackEvent->uEvent.sNwkEdScanConfirmEvent.u8Status;
        psStackEvent->uEvent.sNwkEdScanConfirmEvent.u8ResultListSize = *(pu8Msg+u16Len++);
        uint8 u8Index;
        for (u8Index=0u; u8Index < psStackEvent->uEvent.sNwkEdScanConfirmEvent.u8ResultListSize; u8Index++)
        {
            psStackEvent->uEvent.sNwkEdScanConfirmEvent.au8EnergyDetect[u8Index] = *(pu8Msg+u16Len++);
        }
    }
    else if(u16PktType == (uint16)E_SL_MSG_NWK_STATUS_INDICATION)
    {
        /* Update Event type */
        psStackEvent->eType = ZPS_EVENT_NWK_STATUS_INDICATION;
        /* Update Event Data  NwkAddr*/
        psStackEvent->uEvent.sNwkStatusIndicationEvent.u16NwkAddr = ((uint16)*(pu8Msg+u16Len++))  << 8U;
        psStackEvent->uEvent.sNwkStatusIndicationEvent.u16NwkAddr += *(pu8Msg+u16Len++);
        /* Update Event Data Nwk event status */
        psStackEvent->uEvent.sNwkStatusIndicationEvent.u8Status = *(pu8Msg+u16Len++);
        DBG_vPrintf((bool_t)TRUE, "Nwk status Ind addr %04x Status %02x\n",
                psStackEvent->uEvent.sNwkStatusIndicationEvent.u16NwkAddr,
                psStackEvent->uEvent.sNwkStatusIndicationEvent.u8Status);
        if (psStackEvent->uEvent.sNwkStatusIndicationEvent.u8Status == 0xaaU)
        {
            /* out of sequence test packet, kill the event from going further */
            psStackEvent->eType = ZPS_EVENT_NONE;
        }
    }
    else if(u16PktType == (uint16)E_SL_MSG_NWK_ROUTE_DISCOVERY)
    {
        /* Update Event type */
        psStackEvent->eType = ZPS_EVENT_NWK_ROUTE_DISCOVERY_CONFIRM;
        /* Update NWK Route Discovery Event DstAddress */
        psStackEvent->uEvent.sNwkRouteDiscoveryConfirmEvent.u16DstAddress = ((uint16)*(pu8Msg+u16Len++))  << 8U;
        psStackEvent->uEvent.sNwkRouteDiscoveryConfirmEvent.u16DstAddress += *(pu8Msg+u16Len++);
        /* Update NWK Route Discovery Event status */
        psStackEvent->uEvent.sNwkRouteDiscoveryConfirmEvent.u8Status = *(pu8Msg+u16Len++);
        /* Update NWK Route Discovery Event NWK status */
        psStackEvent->uEvent.sNwkRouteDiscoveryConfirmEvent.u8NwkStatus = *(pu8Msg+u16Len++);
    }
    else if(u16PktType == (uint16)E_SL_MSG_ZDO_BIND_EVENT)
    {
        /* Update Event type */
        psStackEvent->eType = ZPS_EVENT_ZDO_BIND;

        /* Copy destination address mode */
        uint8 u8DstAddrMode = *(pu8Msg+u16Len++);
        psStackEvent->uEvent.sZdoBindEvent.u8DstAddrMode = u8DstAddrMode;

        /* Copy destination address */
        if(ZPS_E_ADDR_MODE_IEEE == u8DstAddrMode)
        {
            psStackEvent->uEvent.sZdoBindEvent.uDstAddr.u64Addr = u64SL_ConvBiToU64(pu8Msg+u16Len);
            u16Len += sizeof(uint64);
        }
        else
        {
            psStackEvent->uEvent.sZdoBindEvent.uDstAddr.u16Addr = ((uint16)*(pu8Msg+u16Len++))  << 8U;
            psStackEvent->uEvent.sZdoBindEvent.uDstAddr.u16Addr += *(pu8Msg+u16Len++);
        }

        /* Copy source endpoint */
        psStackEvent->uEvent.sZdoBindEvent.u8SrcEp = *(pu8Msg+u16Len++);

        /* Copy desination endpoint */
        psStackEvent->uEvent.sZdoBindEvent.u8DstEp = *(pu8Msg+u16Len++);

    }
    else if (u16PktType == (uint16)E_SL_MSG_ZPS_ERROR )
    {
        psStackEvent->eType  = ZPS_EVENT_ERROR;

        /* Update the Event Error */
        psStackEvent->uEvent.sAfErrorEvent.eError = *(pu8Msg+u16Len++);

        /* Check which kind of error has been raised */
        if (psStackEvent->uEvent.sAfErrorEvent.eError == ZPS_ERROR_OS_MESSAGE_QUEUE_OVERRUN)
        {
            /* Update the OS Error Message */
            psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorOsMessageOverrun.hMessage = (void *)(uintptr_t)u32SL_ConvBiToU32(pu8Msg+u16Len);
            u16Len += (uint16)sizeof(uint32);
            u8TempStatus = (uint8)JN_ERROR_OS_MESSAGE_QUEUE_OVERRUN;
        }
        else if (psStackEvent->uEvent.sAfErrorEvent.eError == ZPS_ERROR_ZDO_LINKSTATUS_FAIL) {
            psStackEvent->uEvent.sAfErrorEvent.uErrorData.u64Value = u64SL_ConvBiToU64(pu8Msg+u16Len);
            u16Len += sizeof(uint64);
            u8TempStatus = (uint8)JN_ERROR_ZDO_LINKSTATUS_FAIL;
        }
        else
        {
            u8TempStatus = (uint8)psStackEvent->uEvent.sAfErrorEvent.eError + 0x30u;
            /* Update Event Error Data Profile ID */
            psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.u16ProfileId = ((uint16)*(pu8Msg+u16Len++)) << 8U;
            psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.u16ProfileId += ((uint16)*(pu8Msg+u16Len++));
            /* Update Event Error Data Cluster ID */
            psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.u16ClusterId = ((uint16)*(pu8Msg+u16Len++)) << 8U;
            psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.u16ClusterId += ((uint16)*(pu8Msg+u16Len++));
            /* Update Event Error Data Source Address Mode */
            psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.u8SrcAddrMode = *(pu8Msg+u16Len++);
            if ((uint8)ZPS_E_ADDR_MODE_IEEE == psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.u8SrcAddrMode)
            {
                /* Update Event Error Data IEEE Address */
                psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.uAddr.u64SrcAddr = u64SL_ConvBiToU64(pu8Msg+u16Len);
                u16Len += sizeof(uint64);
            }
            else
            {
                /* Update Event Error Data Short Address */
                psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.uAddr.u16SrcAddr = ((uint16)*(pu8Msg+u16Len++)) << 8U;
                psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.uAddr.u16SrcAddr += ((uint16)*(pu8Msg+u16Len++));
            }
            /* Update Event Error Data Destination Endpoint */
            psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.u8DstEndpoint = *(pu8Msg+u16Len++);
            /* Update Event Error Data Source Endpoint */
            psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.u8SrcEndpoint = *(pu8Msg+u16Len++);
            /* Update Event Error Data APDU */
            psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.hAPdu = (PDUM_thAPdu)(uintptr_t)u32SL_ConvBiToU32(pu8Msg+u16Len);
            u16Len += sizeof(uint32);
            /* Update Event Error Data Data Size */
            psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.u16DataSize = ((uint16)*(pu8Msg+u16Len++)) << 8U;
            psStackEvent->uEvent.sAfErrorEvent.uErrorData.sAfErrorApdu.u16DataSize += ((uint16)*(pu8Msg+u16Len++));
        }
    }
    else if (u16PktType == (uint16)E_SL_MSG_SERIAL_LINK_BAD_TABLE )
    {
        DBG_vPrintf((bool_t)TRUE, "BAD Table Event\n");
    }
    else if (u16PktType == (uint16)E_SL_MSG_ZPS_INTERNAL_ERROR )
    {
        u8TempStatus = *(pu8Msg+u16Len++);
        psStackEvent->eType = ZPS_EVENT_NONE;
    }
    else
    {
        /* No action required */
    }

    if (u16PktLength != u16Len)
    {
        DBG_vPrintf((bool_t)TRUE, "WARNING: Nwk event %04x Received %d bytes Handled %d bytes\n",
                u16PktType, u16PktLength, u16Len);
    }

    if (u8TempStatus != 0u)
    {
        vMonitorAllJnErrors( u8TempStatus, bExtended);
    }

    vSL_FreeRxBuffer(pu8RxSerialMsg);
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_HandleInterpanEvent
  *
  */
 /**
  *
  * @param pu8RxSerialMsg uint8 *
  * @param pmyPDUM_thAPduInstance PDUM_thAPduInstance *
  * @param psStackEvent ZPS_tsAfEvent *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_HandleInterpanEvent(
                    uint8                       *pu8RxSerialMsg,
                    PDUM_thAPduInstance         *pmyPDUM_thAPduInstance,
                    ZPS_tsAfEvent               *psStackEvent)
{
    uint16 u16PktType;
    uint16 u16PktLength;
    uint16 u16Len = 0U;
    uint8 *pu8Msg = pu8RxSerialMsg + SL_MSG_DATA_START_IDX;

    union
    {
        uint8*  pu8;
        uint16* pu16;
    }
    upMsg;

    upMsg.pu8 = pu8RxSerialMsg + SL_MSG_TYPE_IDX;
    u16PktType = *upMsg.pu16;
    upMsg.pu8 = pu8RxSerialMsg + SL_MSG_RX_LEN_IDX;
    u16PktLength = *upMsg.pu16;

    /* check Pkt type */
    if(u16PktType == (uint16)E_SL_MSG_INTERPAN_DATA_INDICATION)
    {
        uint16 u16ApduPayloadLen = 0x00U;
        uint8 *pu8Payload;

        /* Update Event type */
        psStackEvent->eType =(ZPS_teAfEventType)ZPS_EVENT_APS_INTERPAN_DATA_INDICATION;

        /* Update Event Data Ind status */
        psStackEvent->uEvent.sApsInterPanDataIndEvent.eStatus = *(pu8Msg+u16Len++);

        /* Update Event Data Profile Id */
        psStackEvent->uEvent.sApsInterPanDataIndEvent.u16ProfileId = ((uint16)*(pu8Msg+u16Len++)) << 8U;
        psStackEvent->uEvent.sApsInterPanDataIndEvent.u16ProfileId += *(pu8Msg+u16Len++);

        /* Update Event Data Cluster Id */
        psStackEvent->uEvent.sApsInterPanDataIndEvent.u16ClusterId = ((uint16)*(pu8Msg+u16Len++)) << 8U;
        psStackEvent->uEvent.sApsInterPanDataIndEvent.u16ClusterId += *(pu8Msg+u16Len++);

        /* Update Event Data Ind Dst End Point */
        psStackEvent->uEvent.sApsInterPanDataIndEvent.u8DstEndpoint = *(pu8Msg+u16Len++);

        /* Update Event Data Ind Src Addr Mode */
        psStackEvent->uEvent.sApsInterPanDataIndEvent.u8SrcAddrMode = *(pu8Msg+u16Len++);

        /* Update Event Data Ind Src Addr*/
        if(psStackEvent->uEvent.sApsInterPanDataIndEvent.u8SrcAddrMode == (uint8)ZPS_E_ADDR_MODE_IEEE)
        {
            psStackEvent->uEvent.sApsInterPanDataIndEvent.u64SrcAddress = u64SL_ConvBiToU64(pu8Msg+u16Len);
            u16Len += sizeof(uint64);
        }
        else
        {
            psStackEvent->uEvent.sApsInterPanDataIndEvent.u64SrcAddress = ((uint64)*(pu8Msg+u16Len++)) << 8U;
            psStackEvent->uEvent.sApsInterPanDataIndEvent.u64SrcAddress += *(pu8Msg+u16Len++);
        }

        /* Update Src pan Id */
        psStackEvent->uEvent.sApsInterPanDataIndEvent.u16SrcPan = ((uint16)*(pu8Msg+u16Len++)) << 8U;
        psStackEvent->uEvent.sApsInterPanDataIndEvent.u16SrcPan += *(pu8Msg+u16Len++);

        /* Update dest pan Id */
        psStackEvent->uEvent.sApsInterPanDataIndEvent.sDstAddr.u16PanId = ((uint16)*(pu8Msg+u16Len++)) << 8U;
        psStackEvent->uEvent.sApsInterPanDataIndEvent.sDstAddr.u16PanId += *(pu8Msg+u16Len++);

        /* Update Event Data Ind Dst Addr Mode */
        //psStackEvent->uEvent.sApsInterPanDataIndEvent.sDstAddr.eMode = (teInterPanAddrMode)*(pu8Msg+u16Len++);
        psStackEvent->uEvent.sApsInterPanDataIndEvent.sDstAddr.eMode = *(pu8Msg+u16Len++);

        /* Update Event Data Ind Dst Addr*/
        if(psStackEvent->uEvent.sApsInterPanDataIndEvent.sDstAddr.eMode == ZPS_E_AM_INTERPAN_IEEE)

        {
            psStackEvent->uEvent.sApsInterPanDataIndEvent.sDstAddr.uAddress.u64Addr = u64SL_ConvBiToU64(pu8Msg+u16Len);
            u16Len += sizeof(uint64);
        }
        else
        {
            psStackEvent->uEvent.sApsInterPanDataIndEvent.sDstAddr.uAddress.u16Addr = ((uint16)*(pu8Msg+u16Len++)) << 8U;
            psStackEvent->uEvent.sApsInterPanDataIndEvent.sDstAddr.uAddress.u16Addr += *(pu8Msg+u16Len++);
        }

        /* Update Event Data Ind Link Quality */
        psStackEvent->uEvent.sApsInterPanDataIndEvent.u8LinkQuality = *(pu8Msg+u16Len++);

        /* Update Payload Length */
        u16ApduPayloadLen = ((uint16)*(pu8Msg+u16Len++)) << 8U;
        u16ApduPayloadLen += *(pu8Msg+u16Len++);

        *pmyPDUM_thAPduInstance= hZCL_AllocateAPduInstance();

        /* Set hApdu Instance */
        psStackEvent->uEvent.sApsInterPanDataIndEvent.hAPduInst = *pmyPDUM_thAPduInstance;

        pu8Payload = (uint8 *)(PDUM_pvAPduInstanceGetPayload(*pmyPDUM_thAPduInstance));

        /* copy Payload */
        (void)ZBmemcpy(pu8Payload, pu8Msg+u16Len, u16ApduPayloadLen);
        u16Len += u16ApduPayloadLen;

        /* Set Payload Size */
        (void)PDUM_eAPduInstanceSetPayloadSize(*pmyPDUM_thAPduInstance, u16ApduPayloadLen);
    }
    else
    {
        if(u16PktType == (uint16)E_SL_MSG_INTERPAN_DATA_CONFIRM)
        {
            /* Update Event type */
            psStackEvent->eType =(ZPS_teAfEventType)ZPS_EVENT_APS_INTERPAN_DATA_CONFIRM;

            /* Update Event Data Confirm status */
            psStackEvent->uEvent.sApsInterPanDataConfirmEvent.u8Status = *(pu8Msg+u16Len++);

            /* Update Event Data Confirm handle */
            psStackEvent->uEvent.sApsInterPanDataConfirmEvent.u8Handle = *(pu8Msg+u16Len++);
        }
    }

    if (u16PktLength != u16Len)
    {
        DBG_vPrintf((bool_t)TRUE, "WARNING: Interpan event %04x Received %d bytes Handled %d bytes\n",
                        u16PktType, u16PktLength, u16Len);
    }
    vSL_FreeRxBuffer(pu8RxSerialMsg);
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_HandleNodeParentIndication
  *
  */
 /**
  *
  * @param pu8RxSerialMsg uint8 *
  * @param pu16ShortAddress uint16 *
  * @param pu64LongAddress uint64 *
  * @param pu64ParentAddress uint64 *
  * @param pu8Status uint8 *
  * @param pu16ID uint16 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_HandleNodeParentIndication(
                    uint8                       *pu8RxSerialMsg,
                    uint16                      *pu16ShortAddress,
                    uint64                      *pu64LongAddress,
                    uint64                      *pu64ParentAddress,
                    uint8                       *pu8Status,
                    uint16                      *pu16ID)
{
    uint16 u16Len = 0U;
    uint8 *pu8Msg = pu8RxSerialMsg + SL_MSG_DATA_START_IDX;
    union
    {
        uint8*  pu8;
        uint16* pu16;
    } upMsg;

    upMsg.pu8 = (pu8RxSerialMsg + SL_MSG_RX_LEN_IDX);
    uint16 u16PktLen = *upMsg.pu16;

    *pu16ShortAddress = ((uint16)*(pu8Msg+u16Len++)) << 8U;
    *pu16ShortAddress += *(pu8Msg+u16Len++);

    *pu64LongAddress = u64SL_ConvBiToU64(pu8Msg+u16Len);
    u16Len += sizeof(uint64);
    *pu64ParentAddress = u64SL_ConvBiToU64(pu8Msg+u16Len);
    u16Len += sizeof(uint64);

    *pu8Status = *(pu8Msg+u16Len++);

    *pu16ID = ((uint16)*(pu8Msg+u16Len++)) << 8U;
    *pu16ID += *(pu8Msg+u16Len++);
    if (u16PktLen != u16Len)
    {
        DBG_vPrintf((bool_t)1, "WARNING, Parent Ind Expected %d Got %d\n", u16PktLen, u16Len);
    }
    vSL_FreeRxBuffer(pu8RxSerialMsg);
}

/********************************************************************************
  *
  * @fn PUBLIC bool bCheckFragmentedPacket
  *
  */
 /**
  *
  *
  * @brief check TX/RX for Fragmented packet
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC bool bCheckFragmentedPacket(void)
{
    bool bCheckFragmentedPacketRet = (bool_t)TRUE;
    if(u8SL_WriteMessage((uint16)E_SL_MSG_FRAGMENTED_PACKET_CHECK, 0U, NULL, &bCheckFragmentedPacketRet) == ZPS_E_SUCCESS)
    {
        DBG_vPrintf((bool_t)DEBUG_SL, "E_SL_MSG_FRAGMENTED_PACKET_CHECK %d \r\n", bCheckFragmentedPacketRet );
    }
    return bCheckFragmentedPacketRet;
}
/********************************************************************************
  *
  * @fn PUBLIC int16 i16APP_ReadJNTempSensorVal
  *
  */
 /**
  *
  *
  * @brief Send serial command to read temperature sensor value on JN
  *
  * @return TRUE if success,
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC int16 i16APP_ReadJNTempSensorVal(void)
{

    uint16 u16ADCTempVal = 0U;
    int16 i16DeciCentigrade  = -100;

    /* Send over serial */
    if(u8SL_WriteMessage((uint16)E_SL_MSG_READ_JN_TEMP_VALUE, 0U, NULL, &u16ADCTempVal) == ZPS_E_SUCCESS)
    {
        DBG_vPrintf((bool_t)DEBUG_SL, "E_SL_MSG_READ_JN_TEMP_VALUE  %d \r\n",u16ADCTempVal );

        i16DeciCentigrade = (int16)(25 - ((((int32)u16ADCTempVal - 605) * 723) / 1024));
        /*                           ^                              ^       \______/
                                     |                              |           | - 0.7060 multiplier
                                     |                              |
                                     |                              | - Raw ADC reading in the factory, at the given temperature.
                                     | - Temperature (degrees C) in the factory

          The multiplier does two steps into one:
          1. The Converting the raw ADC to milivolts
          2. Converting the milivolts to degrees C using the sensor gain
        */
    }
    return i16DeciCentigrade;
}
/********************************************************************************
  *
  * @fn PUBLIC void vAPP_SetMaxMACPollFailCount
  *
  */
 /**
  *
  * @param u8MaxMacFailCount uint8
  *
  * @brief Sets the maximum Poll failures to report the event E_SL_MSG_MAC_POLL_FAILURE
  * the host
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vAPP_SetMaxMACPollFailCount(uint8 u8MaxMacFailCount)
{

    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SET_MAX_MAC_FAIL_COUNT, (uint16)sizeof(u8MaxMacFailCount), &u8MaxMacFailCount, NULL);

}

/********************************************************************************
  *
  * @fn PUBLIC u8GetReprovisionData
  *
  */
 /**
  *
  * @param pvZdo u64Address
  * @param psReprovission tsReprovissionData*  *
  *
  * @return 0 if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8GetReprovisionData( uint64 u64Address, tsReprovissionData *psReprovission)
{
    uint8 au8TxSerialBuffer[40];
    uint8 *pu8TxBuffer;
    uint8 u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;
    psReprovission->u64Address = u64Address;

    /* Copy IEEE Addr */
    vSL_ConvU64ToBi( u64Address, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint64);
    u16TxLength += sizeof(uint64);

    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_REPROVISSION_DATA, u16TxLength, au8TxSerialBuffer, psReprovission);

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC u8SetReprovisionData
  *
  */
 /**
  *
  * @param psReprovission tsReprovissionData*  *
  *
  * @return 0 if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SetReprovisionData( tsReprovissionData *psReprovission)
{
    uint8 au8TxSerialBuffer[40];
    uint8 *pu8TxBuffer;
    uint8 u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    (void)ZBmemcpy( pu8TxBuffer, &psReprovission->au8Key, ZPS_SEC_KEY_LENGTH);
    pu8TxBuffer += ZPS_SEC_KEY_LENGTH;
    u16TxLength += ZPS_SEC_KEY_LENGTH;

    /* Copy IEEE Addr */
    vSL_ConvU64ToBi( psReprovission->u64Address, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint64);
    u16TxLength += sizeof(uint64);

    /* Copy u32InFrameCtr */
    vSL_ConvU32ToBi( psReprovission->u32InFrameCtr, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy u32OutFrameCtr */
    vSL_ConvU32ToBi( psReprovission->u32OutFrameCtr, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy u8Permissions */
    *pu8TxBuffer = psReprovission->u8Permissions;
    u16TxLength += sizeof(uint8);

    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_REPROVISSION_DATA, u16TxLength, au8TxSerialBuffer, NULL);

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC teZCL_Status eAPP_GetKeyTableEntry
  *
  */
 /**
  *
  * @param u16Index uint16
  * @param psEntry ZPS_tsAplApsKeyDescriptorEntry *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC teZCL_Status eAPP_GetKeyTableEntry(uint16 u16Index, ZPS_tsAplApsKeyDescriptorEntry* psEntry)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Add a byte of padding */
    *pu8TxBuffer++ = 0U;
    u16TxLength += sizeof(uint8);

    /* Serialize u16Index */
    vSL_ConvU64ToBi((uint64)u16Index, pu8TxBuffer);
    u16TxLength += sizeof(uint64);

    (void)ZBmemset(psEntry, 0x00, sizeof(ZPS_tsAplApsKeyDescriptorEntry));

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_KEY_TABLE_ENTRY, u16TxLength, au8TxSerialBuffer, psEntry);
    /* Sanitize temporary variables */
    (void)ZBmemset(au8TxSerialBuffer, 0 , (uint32)u16TxLength);
    return (teZCL_Status)u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC teZCL_Status eAPP_GetKeyTableEntryByIEEEAddress
  *
  */
 /**
  *
  * @param u16Index uint16
  * @param psEntry ZPS_tsAplApsKeyDescriptorEntry *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC teZCL_Status eAPP_GetKeyTableEntryByIEEEAddress(uint64 u64IEEEAddress,
                                                       ZPS_tsAplApsKeyDescriptorEntry* psEntry)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Query Type; this uses the new API */
    *pu8TxBuffer++ = 1U;
    u16TxLength += sizeof(uint8);

    /* Serialize u64IEEEAddress */
    vSL_ConvU64ToBi(u64IEEEAddress, pu8TxBuffer);
    u16TxLength += sizeof(uint64);

    (void)ZBmemset(psEntry, 0x00, sizeof(ZPS_tsAplApsKeyDescriptorEntry));

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_KEY_TABLE_ENTRY,
                                 u16TxLength,
                                 au8TxSerialBuffer,
                                 psEntry);
    /* Sanitize temporary variables */
    (void)ZBmemset(au8TxSerialBuffer, 0 , (uint32)u16TxLength);
    return (teZCL_Status)u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint16 u16GetNeighbourTableSize
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 u16GetNeighbourTableSize(void)
{
    uint16 u16TableSize = 0U;
    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_NEIGHBOR_TABLE_SIZE, 0U, NULL, &u16TableSize);
    return u16TableSize;
}

/********************************************************************************
  *
  * @fn PUBLIC teZCL_Status eAPP_GetAddressMapTableEntry
  *
  */
 /**
  *
  * @param u16Index uint16
  * @param pu8AddressMap uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC teZCL_Status eAPP_GetAddressMapTableEntry(uint16 u16Index, uint8 *pu8AddressMap)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16Index */
    *pu8TxBuffer++ = (uint8)(u16Index >> 8U);
    *pu8TxBuffer++ = (uint8)(u16Index);
    u16TxLength += sizeof(uint16);

    (void)ZBmemset(pu8AddressMap, 0x00, 10UL);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_ADDRESS_MAP_TABLE_ENTRY, u16TxLength, au8TxSerialBuffer, pu8AddressMap);
    return (teZCL_Status)u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint16 u16GetAddressMapTableSize
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 u16GetAddressMapTableSize(void)
{
    uint16 u16TableSize = 0U;
    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_ADDRESS_MAP_TABLE_SIZE, 0U, NULL, &u16TableSize);
    return u16TableSize;
}
/********************************************************************************
  *
  * @fn PUBLIC teZCL_Status eAPP_GetRoutingTableEntry
  *
  */
 /**
  *
  * @param u16Index uint16
  * @param psEntry ZPS_tsNwkRtEntry *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC teZCL_Status eAPP_GetRoutingTableEntry(uint16 u16Index, ZPS_tsNwkRtEntry *psEntry)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16Index */
    *pu8TxBuffer++ = (uint8)(u16Index >> 8U);
    *pu8TxBuffer++ = (uint8)(u16Index);
    u16TxLength += sizeof(uint16);

    (void)ZBmemset(psEntry, 0x00, sizeof(ZPS_tsNwkRtEntry));

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_ROUTING_TABLE_ENTRY, u16TxLength, au8TxSerialBuffer, psEntry);
    return (teZCL_Status)u8Status; /*CJGTODO:casting*/
}

/********************************************************************************
  *
  * @fn PUBLIC uint16 u16GetRoutingTableSize
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 u16GetRoutingTableSize(void)
{
    uint16 u16TableSize = 0U;
    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_ROUTING_TABLE_SIZE, 0U, NULL, &u16TableSize);
    return u16TableSize;
}

/********************************************************************************
  *
  * @fn PUBLIC uint16 u16GetApsKeyTableSize
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 u16GetApsKeyTableSize(void)
{
    uint16 u16TableSize = 0U;
    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_APS_KEY_TABLE_SIZE, 0U, NULL, &u16TableSize);
    return u16TableSize;
}

/********************************************************************************
  *
  * @fn PUBLIC uint16 u16GetApsKeyTableSize
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint32 u32GetApsGroupTableSize(void)
{
    uint32 u32TableSize = 0U;
    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_GROUP_TABLE_SIZE, 0U, NULL, &u32TableSize);
    return u32TableSize;
}

/********************************************************************************
  *
  * @fn PUBLIC void vAHI_SwReset
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vAHI_SwReset(void)
{
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_RESET, 0U, NULL,NULL);

    vSetJNState(JN_NOT_READY);
}

/********************************************************************************
  *
  * @fn PUBLIC teZCL_Status eAPP_GetBinding
  *
  */
 /**
  *
  * @param u16Index uint16
  * @param psEntry ZPS_tsAplApsmeBindingTableEntry *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC teZCL_Status eAPP_GetBinding(uint16 u16Index, ZPS_tsAplApsmeBindingTableEntry* psEntry)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16Index */
    *pu8TxBuffer++ = (uint8)(u16Index >> 8U);
    *pu8TxBuffer++ = (uint8)(u16Index);
    u16TxLength += sizeof(uint16);

    (void)ZBmemset(psEntry, 0x00, sizeof(ZPS_tsAplApsmeBindingTableEntry));

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_BINDING_ENTRY, u16TxLength, au8TxSerialBuffer, psEntry);
    return (teZCL_Status)u8Status; /*CJGTODO:casting*/
}

/********************************************************************************
  *
  * @fn PUBLIC teZCL_Status eAPP_GetMacAdddress
  *
  */
 /**
  *
  * @param u16Index uint16
  * @param pu64Address uint64 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC teZCL_Status eAPP_GetMacAdddress(uint16 u16Index, uint64* pu64Address)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16Index */
    *pu8TxBuffer++ = (uint8)(u16Index >> 8U);
    *pu8TxBuffer++ = (uint8)(u16Index);
    u16TxLength += sizeof(uint16);

    *pu64Address = 0ULL;

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_MAC_TABLE_ENTRY, u16TxLength, au8TxSerialBuffer, pu64Address);
    return (teZCL_Status)u8Status; /*CJGTODO:casting*/
}

/********************************************************************************
  *
  * @fn PUBLIC void vStartJoinNetwork
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vStartJoinNetwork(void)
{
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_START_SCAN, 0U, NULL,NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SetParentTimeoutMethod
  *
  */
 /**
  *
  * @param u8Method uint8
  *
  * @brief Set the parent timeout method on th JN
  *
  * @return uint8, the status of the request
  *
  * @note
  *
  * imported description
 ********************************************************************************/
uint8 u8SetParentTimeoutMethod(uint8 u8Method)
{
    return u8SL_WriteMessage((uint16)E_SL_MSG_SET_PARENT_TIMEOUT , 1, &u8Method, NULL);
}


/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SetAPSFrameCounter
  *
  */
 /**
  *
  * @param u32FrameCounter uint32
  * @param bIsInOrOut bool_t
  * @param u64IEEEAddress uint64
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SetAPSFrameCounter(uint32 u32FrameCounter,
                                bool_t bIsInOrOut,
                                uint64 u64IEEEAddress)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    uint8 u8Status;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u32FrameCounter */
    vSL_ConvU32ToBi(u32FrameCounter, pu8TxBuffer);
    u16TxLength += sizeof(uint32);
    pu8TxBuffer += sizeof(uint32);

    /* Copy bIsInOrOut */
    *pu8TxBuffer++ = (uint8)(bIsInOrOut);
    u16TxLength += sizeof(uint8);

    /* Copy u64IEEEAddress */
    vSL_ConvU64ToBi(u64IEEEAddress, pu8TxBuffer);
    u16TxLength += sizeof(uint64);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_APS_FRAME_COUNTER, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SetNwkFrameCounter
  *
  */
 /**
  *
  * @param u32FrameCounter uint32
  * @param bIsInOrOut bool_t
  * @param u16NwkAddress uint16
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SetNwkFrameCounter(uint32 u32FrameCounter,
                                bool_t bIsInOrOut,
                                uint16 u16NwkAddress)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    uint8 u8Status;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u32FrameCounter */
    vSL_ConvU32ToBi(u32FrameCounter, pu8TxBuffer);
    u16TxLength += sizeof(uint32);
    pu8TxBuffer += sizeof(uint32);

    /* Copy bIsInOrOut */
    *pu8TxBuffer++ = (uint8)(bIsInOrOut);
    u16TxLength += sizeof(uint8);

    /* Copy Nwk address */
    *pu8TxBuffer++ = (uint8)(u16NwkAddress >> 8U);
    *pu8TxBuffer++ = (uint8)u16NwkAddress;
    u16TxLength += sizeof(uint16);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_NWK_FRAME_COUNT, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;
}
/********************************************************************************
  *
  * @fn PUBLIC void vChangeChannel
  *
  */
 /**
  *
  * @param u32ChannelNum uint32
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vChangeChannel(uint32 u32ChannelNum)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u32ChannelNum */
    vSL_ConvU32ToBi(u32ChannelNum, pu8TxBuffer);
    u16TxLength += sizeof(uint32);

    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_CHANGE_CHANNEL, u16TxLength, au8TxSerialBuffer, NULL);
    return;
}

/********************************************************************************
  *
  * @fn PUBLIC void vUpdateDefaultTCAPSLinkKey
  *
  */
 /**
  *
  * @param pu8key uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vUpdateDefaultTCAPSLinkKey(uint8 *pu8key)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy pu8key */
    (void)ZBmemcpy(pu8TxBuffer, pu8key, 16UL);
    u16TxLength += 16U;

    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_UPDATE_DEFAULT_LINK_KEY, u16TxLength, au8TxSerialBuffer, NULL);
    /* Sanitize temporary variables */
    (void)ZBmemset(au8TxSerialBuffer, 0 , (uint32)u16TxLength);
    return;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8GetVersionAndDeviceType
  *
  */
 /**
  *
  * @param pu32Version uint32 *
  * @param peDeviceType teSL_ZigBeeDeviceType *
  * @param peMacType teSL_DeviceMacType *
  * @param pu16OptionMask uint16 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8GetVersionAndDeviceType(
                    uint32                      *pu32Version,
                    teSL_ZigBeeDeviceType       *peDeviceType,
                    teSL_DeviceMacType          *peMacType,
                    uint16                      *pu16OptionMask,
                    uint32                      *pu32SDKVersion)
{
    uint8 u8Status, au8Temp[12] = {0U};

    bIsReadingZBVersion = (bool_t)TRUE;
    /* Send over serial */
    vSL_SetLongResponsePeriod();
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_VERSION, 0U, NULL, au8Temp);
    vSL_SetStandardResponsePeriod();
    if(u8Status == ZPS_E_SUCCESS)
    {
        bIsReadingZBVersion = (bool_t)FALSE;

        (void)ZBmemcpy(pu32Version, au8Temp, sizeof(uint32));

        *peDeviceType = (teSL_ZigBeeDeviceType)au8Temp[4];
        *peMacType    = (teSL_DeviceMacType)au8Temp[5];
        (void)ZBmemcpy(pu16OptionMask, &au8Temp[6], sizeof(uint16));
        (void)ZBmemcpy(pu32SDKVersion, &au8Temp[8], sizeof(uint32));
    }

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SL_GetJnPDMUse
  *
  */
 /**
  *
  * @param pu8SegmentIntex uint8
  * @param pu32JNPDMUse uint32 *
  *
  * @brief Get a chunk of 16 PDM segments wear count.
  *
  * @return PUBLIC uint8
  *
  * @note
  *
 ********************************************************************************/
PUBLIC uint8 u8SL_GetJnPDMUse(uint8 pu8SegmentIndex, uint32 *pu32JNPDMUse)
{
    uint8 u8Status;
    u8Status = (uint8)u8SL_WriteMessage((uint16)E_SL_MSG_GET_JN_PDMUSE, (uint16)sizeof(uint8), &pu8SegmentIndex, pu32JNPDMUse);
    return u8Status;
}
/********************************************************************************
  *
  * @fn PUBLIC void vSetNetworkKey
  *
  */
 /**
  *
  * @param au8Key uint8
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSetNetworkKey(uint8 au8Key[ZPS_NWK_KEY_LENGTH])
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy Key */
    (void)ZBmemcpy(pu8TxBuffer, au8Key, ZPS_NWK_KEY_LENGTH);
    pu8TxBuffer += ZPS_NWK_KEY_LENGTH;
    u16TxLength += ZPS_NWK_KEY_LENGTH;

    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SET_NETWORK_KEY, u16TxLength, au8TxSerialBuffer, NULL);
    /* Sanitize temporary variables */
    (void)ZBmemset(au8TxSerialBuffer, 0 , (uint32)u16TxLength);
}

/********************************************************************************
  *
  * @fn PUBLIC void vGetNetworkKey
  *
  */
 /**
  *
  * @param au8Key uint8
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vGetNetworkKey(uint8 au8Key[ZPS_NWK_KEY_LENGTH])
{
    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_NWK_KEY_TABLE_ENTRY, 0U, NULL, au8Key);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8ClearNetworkKey
  *
  */
 /**
  *
  * @param u8KeySeqNo uint8
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8ClearNetworkKey(uint8 u8KeySeqNo)
{
    uint8 u8Status;

    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_CLEAR_NETWORK_KEY, 1U, &u8KeySeqNo, NULL);

    return u8Status;
}



/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSetUpForInterPan
  *
  */
 /**
  *
  * @param u32ChannelMask uint32
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSetUpForInterPan(uint32 u32ChannelMask)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u32ChannelMask */
    vSL_ConvU32ToBi(u32ChannelMask, pu8TxBuffer);
    u16TxLength += sizeof(uint32);

    return u8SL_WriteMessage((uint16)E_SL_MSG_SETUP_FOR_INTERPAN, u16TxLength, au8TxSerialBuffer,NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC PHY_Enum_e eAppApiPlmeSetTxPower
  *
  */
 /**
  *
  * @param u32TxPower uint32  W Tx Power to be set
  *
  *
  * @brief Attempt to set Tx Power of JN over serial link, There are 4
  * power levels for JN5168: 0 , -9, -20, - 32
  *
  * @return Status of setting the Tx power : PHY_Enum_e
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC PHY_Enum_e eAppApiPlmeSetTxPower(uint32 u32TxPower)
{
    uint8 u8Status;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u32TxPower */
    vSL_ConvU32ToBi(u32TxPower, pu8TxBuffer);
    u16TxLength += sizeof(uint32);
    u8Status =  u8SL_WriteMessage((uint16)E_SL_MSG_SET_TX_POWER, u16TxLength, au8TxSerialBuffer,NULL);

    if((uint8)MAC_ENUM_SUCCESS != u8Status)
    {
        DBG_vPrintf((bool_t)TRUE, "eAppApiPlmeSetTxPower failed, error code %d \r\n", u8Status);
    }

    return (PHY_Enum_e)u8Status; /*CJGTODO:casting*/
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus JPT_eConvertEnergyTodBm
  *
  */
 /**
  *
  * @param pu8Energy uint8
  * @param p16EnergyIndBm int16 *
  *
  * @brief Remove link key
  *
  * @return Status of remove link key : ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus JPT_eConvertEnergyTodBm(uint8 pu8Energy, int16* p16EnergyIndBm)
{
    uint8 au8TxSerialBuffer[10], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy pu8Energy */
    *pu8TxBuffer = pu8Energy;
    u16TxLength += sizeof(uint8);

    (void)ZBmemset(p16EnergyIndBm, 0x00, sizeof(int16));

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_CONVERT_ENERGY_TO_DBM, u16TxLength, au8TxSerialBuffer, p16EnergyIndBm);
    DBG_vPrintf((bool_t)DEBUG_SL, "p16EnergyIndBm %d \n", *p16EnergyIndBm);
    return u8Status;
}


/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus u8SerialLinkDataTest
  *
  */
 /**
  *
  * @param pu8Data uint8 * Data to be sent
  *
  *
  * @brief Remove link key
  *
  * @return Status of remove link key : ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus u8SerialLinkDataTest(uint8 *pu8Data)
{
    uint8 u8Status;
    uint16 u16Length;

    (void)ZBmemcpy(&u16Length, pu8Data, sizeof(uint16));
    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_SEND_TEST, (uint16)(u16Length+2U), pu8Data, (void*)NULL);
    return u8Status;
}


/********************************************************************************
  *
  * @fn PUBLIC bool bSL_SetNumEZScans
  *
  */
 /**
  *
  * @param u8NumScans  R Number of scan attempts
  *
  *
  * @brief Set number of EZ scan attempts
  *
  * @return TRUE if successful
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC bool bSL_SetNumEZScans(uint8 u8NumScans)
{
    bool bStatus = (bool_t)FALSE;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;

    /* Copy u8NumScans */
    au8TxSerialBuffer[0] = u8NumScans;
    u16TxLength += sizeof(uint8);

    if(u8SL_WriteMessage((uint16)E_SL_MSG_SET_NUM_EZ_SCANS, u16TxLength, au8TxSerialBuffer, NULL)  == (uint8)E_SL_MSG_STATUS_SUCCESS)
    {
        bStatus = (bool_t)TRUE;
    }

    return bStatus;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8ErasePersistentData
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8ErasePersistentData(void)
{
    uint8 u8Status;
    vSL_SetLongResponsePeriod();
    u8Status =  u8SL_WriteMessage((uint16)E_SL_MSG_ERASE_PERSISTENT_DATA, 0U, NULL,NULL);
    vSL_SetStandardResponsePeriod();
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint32 MAC_eMacFilterAddAccept
  *
  */
 /**
  *
  * @param u16Addr uint16
  * @param u8LinkQuality uint8
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint32 MAC_eMacFilterAddAccept(uint16 u16Addr, uint8 u8LinkQuality)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16Addr */
    *pu8TxBuffer++ = (uint8)(u16Addr >> 8U);
    *pu8TxBuffer++ = (uint8)(u16Addr);
    u16TxLength += sizeof(uint16);

    /* Copy u8LinkQuality */
    *pu8TxBuffer++ = u8LinkQuality;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    return u8SL_WriteMessage((uint16)E_SL_MSG_MAC_FILTER_ADD_ACCEPT, u16TxLength, au8TxSerialBuffer, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8GetSecurityMaterialSetSize
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8GetSecurityMaterialSetSize(void)
{
    uint8 u8Size = 0U;
    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_SEC_MAT_SET_SIZE, 0U, NULL, &u8Size);
    return u8Size;
}

/********************************************************************************
  *
  * @fn PUBLIC teZCL_Status eGetSecurityMaterialSetEntry
  *
  */
 /**
  *
  * @param u8Index uint8
  * @param psEntry ZPS_tsNwkSecMaterialSet *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC teZCL_Status eGetSecurityMaterialSetEntry(uint8 u8Index, ZPS_tsNwkSecMaterialSet* psEntry)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8Index */
    *pu8TxBuffer++ = u8Index;
    u16TxLength += sizeof(uint8);

    (void)ZBmemset(psEntry, 0x00, sizeof(ZPS_tsNwkSecMaterialSet));

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_SEC_MAT_SET_ENTRY, u16TxLength, au8TxSerialBuffer, psEntry);
    return (teZCL_Status)u8Status; /*CJGTODO:casting*/
}

/********************************************************************************
  *
  * @fn PUBLIC teZCL_Status eClearSecurityMaterialSetEntry
  *
  */
 /**
  *
  * @param u8Index uint8
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC teZCL_Status eClearSecurityMaterialSetEntry(uint8 u8Index)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8Index */
    *pu8TxBuffer++ = u8Index;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_CLEAR_SEC_MAT_SET_ENTRY, u16TxLength, au8TxSerialBuffer, NULL);
    return (teZCL_Status)u8Status; /*CJGTODO:casting*/
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_SetJnInternalAttenuator
  *
  */
 /**
  *
  * @param bSetAttenuation  R Value to set attenuation
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_SetJnInternalAttenuator(bool_t bSetAttenuation)
{
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SET_JN_INTERNAL_ATTENUATOR, (uint16)sizeof(bSetAttenuation), &bSetAttenuation,NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SL_GetLastRssi
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SL_GetLastRssi(void)
{
    uint8 u8Rssi = 0U;

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_LAST_RSSI, 0U, NULL,&u8Rssi);
    return u8Rssi;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus vSL_GetGlobalStats
  *
  */
 /**
  *
  * @param pu32TotalSuccessfulTX uint32 *
  * @param pu32TotalFailTX uint32 *
  * @param pu32TotalRX uint32 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus vSL_GetGlobalStats(
            uint32                      *pu32TotalSuccessfulTX,
            uint32                      *pu32TotalFailTX,
            uint32                      *pu32TotalRX)
{
    uint32 u32Stats[3] = {0UL};

    ZPS_teStatus eStatus = (ZPS_teStatus)u8SL_WriteMessage((uint16)E_SL_MSG_GET_GLOBAL_STATS, 0U, NULL,u32Stats);
    if(eStatus == ZPS_E_SUCCESS)
    {
        *pu32TotalSuccessfulTX = u32Stats[0];
        *pu32TotalFailTX = u32Stats[1];
        *pu32TotalRX  = u32Stats[2];
    }
    return eStatus;
}

/********************************************************************************
  *
  * @fn PUBLIC bool_t bSL_GetDeviceStats
  *
  */
 /**
  *
  * @param u64Mac uint64
  * @param pu8LastLqi uint8 *
  * @param pu8AverageLqi uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC bool_t bSL_GetDeviceStats(
            uint64                      u64Mac,
            uint8                       *pu8LastLqi,
            uint8                       *pu8AverageLqi)
{
    uint8 au8Stats[2] = {0U};
    uint8 au8TxSerialBuffer[8], *pu8TxBuffer;
    uint16 u16TxLength = 0U;

    pu8TxBuffer = au8TxSerialBuffer;
    /* Copy IEEE Dst Addr */
    vSL_ConvU64ToBi(u64Mac, pu8TxBuffer);
    u16TxLength += sizeof(uint64);


    if(TRUE == u8SL_WriteMessage((uint16)E_SL_MSG_GET_DEVICE_STATS,u16TxLength, au8TxSerialBuffer,au8Stats))
    {
        *pu8LastLqi = au8Stats[0];
        *pu8AverageLqi = au8Stats[1];
        return (bool_t)TRUE;
    }

    return (bool_t)FALSE;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSL_ChangePanId
  *
  */
 /**
  *
  * @param u16PanId  R 16 bit PAN ID
  *
  *
  * @brief Changes short PAN id
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSL_ChangePanId(uint16 u16PanId)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16PanId */
    *pu8TxBuffer++ = (uint8)((u16PanId >> 8U) & 0xffu);
    *pu8TxBuffer = (uint8)(u16PanId & 0xffu);
    u16TxLength += sizeof(uint16);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_CHANGE_PANID, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;

}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSL_EnableDisableDualMac
  *
  */
 /**
  *
  * @param u8Endpoint uint8
  * @param u16ClusterId uint16
  * @param bOutput bool
  * @param bEnableDualMac bool_t
  *
  * @brief Enables or Disables the Dual MAC
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSL_EnableDisableDualMac(
        uint8                       u8Endpoint,
        uint16                      u16ClusterId,
        bool                        bOutput,
        bool_t                      bEnableDualMac)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8Endpoint */
    (void)ZBmemcpy(pu8TxBuffer, &u8Endpoint, sizeof(uint8));
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    /* Copy u16ClusterId */
    *pu8TxBuffer++ = (uint8)(u16ClusterId >> 8U);
    *pu8TxBuffer++ = (uint8)(u16ClusterId);
    u16TxLength += sizeof(uint16);

    /* Copy bOutput */
    (void)ZBmemcpy(pu8TxBuffer, &bOutput, sizeof(bool_t));
    pu8TxBuffer += sizeof(bool_t);
    u16TxLength += sizeof(bool_t);

    /* Copy bEnableDualMac */
    (void)ZBmemcpy(pu8TxBuffer, &bEnableDualMac, sizeof(bool_t));
    pu8TxBuffer += sizeof(bool_t);
    u16TxLength += sizeof(bool_t);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_ENABLE_DISABLE_DUAL_MAC, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;
}


/********************************************************************************
  *
  * @fn PUBLIC void vSL_GetMacAddrTable
  *
  */
 /**
  *
  * @param pu8Table uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_GetMacAddrTable(uint8 *pu8Table)
{
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_MAC_ADDR_TABLE, 0U, NULL,pu8Table);
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_SetMacAddrTable
  *
  */
 /**
  *
  * @param pu8Table uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_SetMacAddrTable(uint8 *pu8Table)
{
    uint8 u8MacAddrTableSize;
    uint16 u16TxLength = 0x00U;

    u8MacAddrTableSize = pu8Table[0];
    u16TxLength = (uint16)sizeof(uint64) * (uint16)u8MacAddrTableSize;
    u16TxLength++;
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SET_MAC_ADDR_TABLE, u16TxLength, pu8Table, NULL);
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
/********************************************************************************
  *
  * @fn PRIVATE uint16 u16SL_ParseApsDataIndication
  *
  */
 /**
  *
  * @param pu8Msg uint8 *,
  *
  * @param psStackEvent ZPS_tsAfEvent *,
  *
  * @param pmyPDUM_thAPduInstance PDUM_thAPduInstance *
  *
  *
  * @brief Parses received APS data indication from the buffer to ZPS_tsAfEvent
  *
  * @return uint16 the number of bytes unpacked from the serial buffer
  *
  * @note
  *
  * imported description
 ********************************************************************************/

PRIVATE uint16 u16SL_ParseApsDataIndication(
        uint8                       *pu8Msg,
        ZPS_tsAfEvent               *psStackEvent,
        PDUM_thAPduInstance         *pmyPDUM_thAPduInstance)
{
    uint16 u16Len = 0U;
    uint16 u16ApduPayloadLen = 0x00U;
    uint8 *pu8Payload;
    /* Update Event Data Ind status */
    psStackEvent->uEvent.sApsDataIndEvent.eStatus = *(pu8Msg+u16Len++);

    if (psStackEvent->uEvent.sApsDataIndEvent.eStatus != 0u)
    {

        vMonitorAllJnErrors(psStackEvent->uEvent.sApsDataIndEvent.eStatus, (bool_t)FALSE);
    }

    /* Update Event Data Ind Sec status */
    psStackEvent->uEvent.sApsDataIndEvent.eSecurityStatus = *(pu8Msg+u16Len++);

    /* Update Event Data Profile Id */
    psStackEvent->uEvent.sApsDataIndEvent.u16ProfileId = ((uint16)*(pu8Msg+u16Len++)) << 8U;
    psStackEvent->uEvent.sApsDataIndEvent.u16ProfileId += *(pu8Msg+u16Len++);

    /* Update Event Data Cluster Id */
    psStackEvent->uEvent.sApsDataIndEvent.u16ClusterId = ((uint16)*(pu8Msg+u16Len++)) << 8U;
    psStackEvent->uEvent.sApsDataIndEvent.u16ClusterId += *(pu8Msg+u16Len++);

    /* Update Event Data Ind Src End Point */
    psStackEvent->uEvent.sApsDataIndEvent.u8SrcEndpoint = *(pu8Msg+u16Len++);

    /* Update Event Data Ind Dst End Point */
    psStackEvent->uEvent.sApsDataIndEvent.u8DstEndpoint = *(pu8Msg+u16Len++);

    /* Update Event Data Ind Src Addr Mode */
    psStackEvent->uEvent.sApsDataIndEvent.u8SrcAddrMode = *(pu8Msg+u16Len++);

    /* Update Event Data Ind Src Addr*/
    if(psStackEvent->uEvent.sApsDataIndEvent.u8SrcAddrMode == (uint8)ZPS_E_ADDR_MODE_IEEE)
    {
        psStackEvent->uEvent.sApsDataIndEvent.uSrcAddress.u64Addr = u64SL_ConvBiToU64(pu8Msg+u16Len);
        u16Len += sizeof(uint64);
    }
    else
    {
        psStackEvent->uEvent.sApsDataIndEvent.uSrcAddress.u16Addr = ((uint16)*(pu8Msg+u16Len++)) << 8U;
        psStackEvent->uEvent.sApsDataIndEvent.uSrcAddress.u16Addr += *(pu8Msg+u16Len++);
    }

    /* Update Event Data Ind Dst Addr Mode */
    psStackEvent->uEvent.sApsDataIndEvent.u8DstAddrMode = *(pu8Msg+u16Len++);

    /* Update Event Data Ind Dst Addr*/
    if(psStackEvent->uEvent.sApsDataIndEvent.u8DstAddrMode == (uint8)ZPS_E_ADDR_MODE_IEEE)
    {
        psStackEvent->uEvent.sApsDataIndEvent.uDstAddress.u64Addr = u64SL_ConvBiToU64(pu8Msg+u16Len);
        u16Len += sizeof(uint64);
    }
    else
    {
        psStackEvent->uEvent.sApsDataIndEvent.uDstAddress.u16Addr = ((uint16)*(pu8Msg+u16Len++)) << 8U;
        psStackEvent->uEvent.sApsDataIndEvent.uDstAddress.u16Addr += *(pu8Msg+u16Len++);
    }

    /* Update Event Data Ind Link Quality */
    psStackEvent->uEvent.sApsDataIndEvent.u8LinkQuality = *(pu8Msg+u16Len++);

    /* Update Event Data Ind Rx Time */
    psStackEvent->uEvent.sApsDataIndEvent.u32RxTime = u32SL_ConvBiToU32(pu8Msg+u16Len);
    u16Len += sizeof(uint32);
    /* Update Payload Length */
    u16ApduPayloadLen = ((uint16)*(pu8Msg+u16Len++)) << 8U;
    u16ApduPayloadLen += *(pu8Msg+u16Len++);

    *pmyPDUM_thAPduInstance = hZCL_AllocateAPduInstance();
    if(*pmyPDUM_thAPduInstance == NULL)
    {
        return u16Len;
    }
    /* Set hApdu Instance */
    psStackEvent->uEvent.sApsDataIndEvent.hAPduInst = *pmyPDUM_thAPduInstance;

    pu8Payload = (uint8 *)(PDUM_pvAPduInstanceGetPayload(*pmyPDUM_thAPduInstance));

    /* copy Payload */
    (void)ZBmemcpy(pu8Payload, pu8Msg+u16Len, u16ApduPayloadLen);
    u16Len += u16ApduPayloadLen;

    /* Set Payload Size */
    (void)PDUM_eAPduInstanceSetPayloadSize(*pmyPDUM_thAPduInstance, u16ApduPayloadLen);

    return u16Len;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSetEndDeviceTimeOutTimeOnParent
  *
  */
 /**
  *
  * @param pu16NwkAddr uint16 *
  * @param pu8TimeOutConstInd uint8 *
  *
  * @brief Sets the End Device Timeout Duration on the Parent device.
  *
  * @return ZPS_teStatus
  *
  * @note macro : ATSE_DUT_CH  <br>
  * AT command : SETP  <br>
  * description: SETP,<EndDeviceShortAddr>,<TimeOutValue>: Set ED Timeout on Parent
  * <br>
  *
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSetEndDeviceTimeOutTimeOnParent(uint16* pu16NwkAddr, uint8* pu8TimeOutConstInd)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy network address of the End device */
    pu8TxBuffer[0u] = (uint8) ((*pu16NwkAddr) >> 8u);
    pu8TxBuffer[1u] = (uint8) (*pu16NwkAddr);
    u16TxLength += sizeof(uint16);

    /* Copy the TimeOut Duration Constant */
    pu8TxBuffer[2u] = (uint8) (*pu8TimeOutConstInd);
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    return u8SL_WriteMessage((uint16)E_SL_MSG_SET_ED_TIMEOUT_ON_PARENT, u16TxLength, au8TxSerialBuffer, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eConvertLqiToRssidBm
  *
  */
 /**
  *
  * @param pu8Lqi uint8 *
  * @param pu8MacId uint8 *
  *
  * @brief Converts an LQI value, according to the MAC source interface
  *
  * @return ZPS_teStatus
  *
  * @note macro : ATSE_DUT_CH  <br>
  * AT command : CLTR  <br>
  * description: CLTR,<LQI>,<MAC interface>: Get LQI
  * <br>
  *
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eConvertLqiToRssidBm(uint8* pu8Lqi, uint8* pu8MacId)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    int16 i16RssidBm;
    uint8 u8Status;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy network address of the End device */
    pu8TxBuffer[0u] = *pu8Lqi;
    pu8TxBuffer[1u] = *pu8MacId;
    u16TxLength += sizeof(uint16);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_CONVERT_LQI_TO_RSSI_DBM,
                                 u16TxLength,
                                 au8TxSerialBuffer,
                                 &i16RssidBm);
    if(u8Status == 0U)
    {
        DBG_vPrintf((bool_t)TRUE,"RSSI = %02d dBm\r\n", i16RssidBm);
    }
    else
    {
        DBG_vPrintf((bool_t)TRUE,"u8JNConvertLqitoRssidBm Err: 0x%02x\r\n", u8Status);
    }

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SerialLinkSetMacInterfaces
  *
  */
 /**
  *
  * @param bEnable2G4 bool_t
  * @param bEnableSG bool_t
  *
  * @brief Sends serial command to enable or disable a mac interface
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SerialLinkSetMacInterfaces( bool_t bEnable2G4, bool_t bEnableSG)
{
    uint8 u8Status;
    uint8 au8TxSerialBuffer[2];

    /* Copy bEnable2G4 & bEnableSG */
    au8TxSerialBuffer[0] = (uint8)bEnable2G4;
    au8TxSerialBuffer[1] = (uint8)bEnableSG;

    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_MAC_INTERFACE, 2U, au8TxSerialBuffer, NULL);
    return u8Status;
}


/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eChangeSubGHzChannel
  *
  */
 /**
  *
  * @param pu32ChanMask  R Channel Mask
  *
  *
  * @brief Sends serial command to change the operating Sub-GHz channel
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eChangeSubGHzChannel(uint32* pu32ChanMask)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy pu32ChanMask */
    vSL_ConvU32ToBi(*pu32ChanMask, pu8TxBuffer);
    u16TxLength += sizeof(uint32);

    /* Send over serial interface */
    return u8SL_WriteMessage((uint16)E_SL_MSG_CHANGE_SUB_GHZ_CHANNEL, u16TxLength, au8TxSerialBuffer, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSetNetworkInterfaceRequest
  *
  */
 /**
  *
  * @param psSetNWKIntrf ZPS_tsNwkNlmeReqSetInterface *
  *
  * @brief Set the Network Interface Request
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSetNetworkInterfaceRequest(ZPS_tsNwkNlmeReqSetInterface* psSetNWKIntrf)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    uint8 u8Index;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8InterfaceIndex */
    pu8TxBuffer[0u] = psSetNWKIntrf->u8InterfaceIndex;
    u16TxLength += sizeof(uint8);
    /* Copy bState */
    pu8TxBuffer[u16TxLength] = u8FromBool(psSetNWKIntrf->bState);
    u16TxLength += sizeof(uint8);
    /* Copy bRoutersAllowed */
    pu8TxBuffer[u16TxLength] = u8FromBool(psSetNWKIntrf->bRoutersAllowed);
    u16TxLength += sizeof(uint8);
    /* Copy u32ChanToUse */
    vSL_ConvU32ToBi(psSetNWKIntrf->u32ChannelToUse, (pu8TxBuffer + u16TxLength));
    u16TxLength += sizeof(uint32);
    /* Copy u8ChanCount */
    pu8TxBuffer[u16TxLength] = psSetNWKIntrf->sSupportedChannels.u8ChannelPageCount;
    u16TxLength += sizeof(uint8);

    for (u8Index = 0U; u8Index < psSetNWKIntrf->sSupportedChannels.u8ChannelPageCount; u8Index++)
    {
        /* Copy u32ChanMask[u8Index] */
        vSL_ConvU32ToBi(psSetNWKIntrf->sSupportedChannels.u32ChannelField[u8Index], (pu8TxBuffer + u16TxLength));
        u16TxLength += sizeof(uint32);
    }

    /* Send over serial */
    return u8SL_WriteMessage((uint16)E_SL_MSG_SET_NWK_INTERFACE_REQ, u16TxLength, au8TxSerialBuffer, NULL);
}
/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eGetNetworkInterfaceRequest
  *
  */
 /**
  *
  * @param u8InterfaceIndex uint8
  * @param psGetNWKIntrf ZPS_tsNwkNlmeCfmGetInterface *
  *
  * @brief Get the Network Interface Request.
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eGetNetworkInterfaceRequest(uint8 u8InterfaceIndex, ZPS_tsNwkNlmeCfmGetInterface* psGetNWKIntrf)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    uint8 u8Index;
    uint8 u8i;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8InterfaceIndex */
    pu8TxBuffer[0u] = u8InterfaceIndex;
    u16TxLength += sizeof(uint8);

    uint8 u8Status;
    uint8 au8Stats[60];

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_NWK_INTERFACE_REQ, u16TxLength, au8TxSerialBuffer, au8Stats);

    if(u8Status == ZPS_E_SUCCESS)
    {
        /* Get u8Status */
        psGetNWKIntrf->u8Status = au8Stats[0u];
        /* Get u8InterfaceIndex */
        psGetNWKIntrf->u8InterfaceIndex = au8Stats[1u];
        /* Get bState */
        psGetNWKIntrf->bState = bFromU8(au8Stats[2u]);
        /* Get u32ChannelInUse */
        u8Index = 3U;
        psGetNWKIntrf->u32ChannelInUse = u32SL_ConvBiToU32(au8Stats+u8Index);
        u8Index += sizeof(uint32);
        /* Get u8ChannelPageCount */
        psGetNWKIntrf->sSupportedChannels.u8ChannelPageCount = au8Stats[u8Index];
        u8Index += sizeof(uint8);
        uint8 u8Count = psGetNWKIntrf->sSupportedChannels.u8ChannelPageCount;
        for (u8i=0U; u8i<u8Count; u8i++)
        {
            /* Get u32ChannelField[0] */
            psGetNWKIntrf->sSupportedChannels.u32ChannelField[u8i] = u32SL_ConvBiToU32(au8Stats+u8Index);
            u8Index += sizeof(uint32);
        }


        /* Get bRoutersAllowed */
        psGetNWKIntrf->bRoutersAllowed = (au8Stats[u8Index++] != 0U) ? (bool_t)TRUE : (bool_t)FALSE;
        /* Get bPowerNegotiationSupported */
        psGetNWKIntrf->bPowerNegotiationSupported = bFromU8(au8Stats[u8Index++]);
        /* Get u32MacTxUcastRetry */
        psGetNWKIntrf->u32MacTxUcastAccRetry = u32SL_ConvBiToU32(au8Stats+u8Index);
        u8Index += sizeof(uint32);

        psGetNWKIntrf->u32MacTxUcastAvgRetry = u32SL_ConvBiToU32(au8Stats+u8Index);
        u8Index += sizeof(uint32);

        psGetNWKIntrf->u32MacTxUcastFail = u32SL_ConvBiToU32(au8Stats+u8Index);
        u8Index += sizeof(uint32);

        psGetNWKIntrf->u32MacTxUcast = u32SL_ConvBiToU32(au8Stats+u8Index);
        u8Index += sizeof(uint32);

        psGetNWKIntrf->u32APSTxUcastRetry = u32SL_ConvBiToU32(au8Stats+u8Index);
        u8Index += sizeof(uint32);

        psGetNWKIntrf->u32APSTxUcastFail = u32SL_ConvBiToU32(au8Stats+u8Index);
        u8Index += sizeof(uint32);
    }


    return u8Status;
}
/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSendMgmtNWKEnhancedUpdateReq
  *
  */
 /**
  *
  * @param u16DstAddr uint16
  * @param u8ScanDuration uint8
  * @param u8ScanCount uint8
  * @param u32ChanMask uint32
  *
  * @brief Send Management Network Enhanced Network Update Request.
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSendMgmtNWKEnhancedUpdateReq(uint16 u16DstAddr, uint8 u8ScanDuration, uint8 u8ScanCount, uint32 u32ChanMask)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16DstAddr */
    pu8TxBuffer[u16TxLength] = (uint8) (u16DstAddr >>8u);
    pu8TxBuffer[u16TxLength+1U] = (uint8) (u16DstAddr);
    u16TxLength += sizeof(uint16);

    /* Copy u8ScanDuration */
    pu8TxBuffer[u16TxLength] = u8ScanDuration;
    u16TxLength += sizeof(uint8);

    /* Copy u8ScanCount */
    pu8TxBuffer[u16TxLength] = u8ScanCount;
    u16TxLength += sizeof(uint8);

    /* Copy u32ChanMask */
    vSL_ConvU32ToBi(u32ChanMask, (pu8TxBuffer + u16TxLength));
    u16TxLength += sizeof(uint32);

    /* Send over serial */
    return u8SL_WriteMessage((uint16)E_SL_MSG_SEND_MGMT_NWK_ENH_UPDATE_REQ, u16TxLength, au8TxSerialBuffer, NULL);
}
/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSendMgmtNWKUnsolicitedEnhancedUpdateNotify
  *
  */
 /**
  *
  * @param u16DstAddr uint16
  * @param u32ChanInUse uint32
  * @param u16MACTxUnicastFailures uint16
  * @param u16MACTxUnicastRetries uint16
  * @param u8PeriodOfTimeForResults uint8
  *
  * @brief Send Management Network Unsolicted Enhanced Network Update Notify.
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSendMgmtNWKUnsolicitedEnhancedUpdateNotify(
        uint16 u16DstAddr, uint32 u32ChanInUse, uint16 u16MACTxUnicastTotal,
        uint16 u16MACTxUnicastFailures, uint16 u16MACTxUnicastRetries, uint8 u8PeriodOfTimeForResults)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16DstAddr */
    pu8TxBuffer[u16TxLength] = (uint8) (u16DstAddr >>8u);
    pu8TxBuffer[u16TxLength+1U] = (uint8) (u16DstAddr);
    u16TxLength += sizeof(uint16);

    /* Copy u32ChanInUse */
    vSL_ConvU32ToBi(u32ChanInUse, (pu8TxBuffer + u16TxLength));
    u16TxLength += sizeof(uint32);

    /* Copy u16MACTxUnicastTotal */
    pu8TxBuffer[u16TxLength] = (uint8) (u16MACTxUnicastTotal >>8u);
    pu8TxBuffer[u16TxLength+1U] = (uint8) (u16MACTxUnicastTotal);
    u16TxLength += sizeof(uint16);

    /* Copy u16MACTxUnicastFailures */
    pu8TxBuffer[u16TxLength] = (uint8) (u16MACTxUnicastFailures >>8u);
    pu8TxBuffer[u16TxLength+1U] = (uint8) (u16MACTxUnicastFailures);
    u16TxLength += sizeof(uint16);

    /* Copy u16MACTxUnicastRetries */
    pu8TxBuffer[u16TxLength] = (uint8) (u16MACTxUnicastRetries >>8u);
    pu8TxBuffer[u16TxLength+1U] = (uint8) (u16MACTxUnicastRetries);
    u16TxLength += sizeof(uint16);

    /* Copy u8PeriodOfTimeForResults */
    pu8TxBuffer[u16TxLength] = u8PeriodOfTimeForResults;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    return u8SL_WriteMessage((uint16)E_SL_MSG_SEND_MGMT_NWK_UNS_ENH_UPDATE_NOTIFY, u16TxLength, au8TxSerialBuffer, NULL);
}
/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSendStartEnergyDetectScan
  *
  */
 /**
  *
  * @param u32ScanChanMask uint32
  * @param u8ScanDuration uint8
  *
  * @brief Send Start Energy Detect scan.
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSendStartEnergyDetectScan(uint32 u32ScanChanMask, uint8 u8ScanDuration)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u32ScanPage */
    vSL_ConvU32ToBi(u32ScanChanMask, (pu8TxBuffer + u16TxLength));
    u16TxLength += sizeof(uint32);

    /* Copy u8ScanDuration */
    pu8TxBuffer[u16TxLength] = u8ScanDuration;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    return u8SL_WriteMessage((uint16)E_SL_MSG_START_ED_SCAN, u16TxLength, au8TxSerialBuffer, NULL);
}
/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSendGetClearNibTxUcastBytesCount
  *
  */
 /**
  *
  * @param u8GetClear uint8
  * @param u16ShortAddress uint16
  * @param pu32NibTxUcastBytesCount uint32 *
  *
  * @brief Send Get/Clear Nib Tx Unicast Bytes Count.
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSendGetClearNibTxUcastBytesCount(uint8 u8GetClear,
        uint16 u16ShortAddress, uint32* pu32NibTxUcastBytesCount)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8GetClear */
    pu8TxBuffer[u16TxLength] = u8GetClear;
    u16TxLength += sizeof(uint8);

    /* Copy u16ShortAddress */
    pu8TxBuffer[u16TxLength] = (uint8) (u16ShortAddress >>8u);
    pu8TxBuffer[u16TxLength+1U] = (uint8) (u16ShortAddress);
    u16TxLength += sizeof(uint16);

    /* Send over serial */
    return u8SL_WriteMessage((uint16)E_SL_MSG_GET_CLEAR_TX_UCAST_BYTE_COUNT, u16TxLength,
            au8TxSerialBuffer, pu32NibTxUcastBytesCount);
}
/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSendGetClearNibRxUcastBytesCount
  *
  */
 /**
  *
  * @param u8GetClear uint8
  * @param u16ShortAddress uint16
  * @param pu32NibRxUcastBytesCount uint32 *
  *
  * @brief Send Get/Clear Nib Rx Unicast Bytes Count.
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSendGetClearNibRxUcastBytesCount(uint8 u8GetClear,
        uint16 u16ShortAddress, uint32* pu32NibRxUcastBytesCount)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8GetClear */
    pu8TxBuffer[u16TxLength] = u8GetClear;
    u16TxLength += sizeof(uint8);

    /* Copy u16ShortAddress */
    pu8TxBuffer[u16TxLength] = (uint8) (u16ShortAddress >>8u);
    pu8TxBuffer[u16TxLength+1U] = (uint8) (u16ShortAddress);
    u16TxLength += sizeof(uint16);

    /* Send over serial */
    return u8SL_WriteMessage((uint16)E_SL_MSG_GET_CLEAR_RX_UCAST_BYTE_COUNT, u16TxLength,
            au8TxSerialBuffer, pu32NibRxUcastBytesCount);
}
/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSendGetClearMacIfTxFailCount
  *
  */
 /**
  *
  * @param u8GetClear uint8
  * @param u8MacId uint8
  * @param pu32TxFailCount uint32 *
  *
  * @brief Send Get/Clear Mac Interface Fail Count.
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSendGetClearMacIfTxFailCount(uint8 u8GetClear,
        uint8 u8MacId, uint32* pu32TxFailCount)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8GetClear */
    pu8TxBuffer[u16TxLength] = u8GetClear;
    u16TxLength += sizeof(uint8);

    /* Copy u8MacId */
    pu8TxBuffer[u16TxLength] = (u8MacId);
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    return u8SL_WriteMessage((uint16)E_SL_MSG_GET_CLEAR_TX_FAIL_COUNT, u16TxLength,
            au8TxSerialBuffer, pu32TxFailCount);
}
/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSendGetClearMacIfTxRetryCount
  *
  */
 /**
  *
  * @param u8GetClear uint8
  * @param u8MacId uint8
  * @param pu32TxRetryCount uint32 *
  *
  * @brief Send Get/Clear Mac Interface Retry Count.
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSendGetClearMacIfTxRetryCount(uint8 u8GetClear,
        uint8 u8MacId, uint32* pu32TxRetryCount)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8GetClear */
    pu8TxBuffer[u16TxLength] = u8GetClear;
    u16TxLength += sizeof(uint8);

    /* Copy u8MacId */
    pu8TxBuffer[u16TxLength] = (u8MacId);
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    return u8SL_WriteMessage((uint16)E_SL_MSG_GET_CLEAR_TX_RETRY_COUNT, u16TxLength,
            au8TxSerialBuffer, pu32TxRetryCount);
}

/********************************************************************************
  *
  * @fn PRIVATE uint32 u32SL_ConvBiToU32
  *
  */
 /**
  *
  * @param pu8DataPtr uint8 *
  *
  * @brief void
  *
  * @return PRIVATE uint32
  *
  * @note
  *
 ********************************************************************************/
PRIVATE uint32 u32SL_ConvBiToU32(uint8 *pu8DataPtr)
{
    return ( (((uint32)*(pu8DataPtr    )) << 24U) |
             (((uint32)*(pu8DataPtr + 1)) << 16U) |
             (((uint32)*(pu8DataPtr + 2)) <<  8U) |
             (((uint32)*(pu8DataPtr + 3))       ) );
}

/********************************************************************************
  *
  * @fn PRIVATE uint64 u64SL_ConvBiToU64
  *
  */
 /**
  *
  * @param pu8DataPtr uint8 *
  *
  * @brief void
  *
  * @return PRIVATE uint64
  *
  * @note
  *
 ********************************************************************************/
PRIVATE uint64 u64SL_ConvBiToU64(uint8 *pu8DataPtr)
{
    return ((((uint64)*(pu8DataPtr    )) << 56U) |
            (((uint64)*(pu8DataPtr + 1)) << 48U) |
            (((uint64)*(pu8DataPtr + 2)) << 40U) |
            (((uint64)*(pu8DataPtr + 3)) << 32U) |
            (((uint64)*(pu8DataPtr + 4)) << 24U) |
            (((uint64)*(pu8DataPtr + 5)) << 16U) |
            (((uint64)*(pu8DataPtr + 6)) <<  8U) |
            (((uint64)*(pu8DataPtr + 7))       ) );
}

/********************************************************************************
  *
  * @fn PRIVATE void vSL_ConvU32ToBi
  *
  */
 /**
  *
  * @param u32Val uint32
  * @param pu8DataPtr uint8 *
  *
  * @brief void
  *
  * @return PRIVATE void
  *
  * @note
  *
 ********************************************************************************/
PRIVATE void vSL_ConvU32ToBi(uint32 u32Val, uint8 *pu8DataPtr)
{
    *pu8DataPtr = (uint8)(u32Val >> 24U);
    *(pu8DataPtr + 1) = (uint8)(u32Val >> 16U);
    *(pu8DataPtr + 2) = (uint8)(u32Val >> 8U);
    *(pu8DataPtr + 3) = (uint8)u32Val;
}

/********************************************************************************
  *
  * @fn PRIVATE void vSL_ConvU64ToBi
  *
  */
 /**
  *
  * @param u64Val uint64
  * @param pu8DataPtr uint8 *
  *
  * @brief void
  *
  * @return PRIVATE void
  *
  * @note
  *
 ********************************************************************************/
PRIVATE void vSL_ConvU64ToBi(uint64 u64Val, uint8 *pu8DataPtr)
{
    *pu8DataPtr = (uint8)(u64Val >> 56U);
    *(pu8DataPtr + 1) = (uint8)(u64Val >> 48U);
    *(pu8DataPtr + 2) = (uint8)(u64Val >> 40U);
    *(pu8DataPtr + 3) = (uint8)(u64Val >> 32U);
    *(pu8DataPtr + 4) = (uint8)(u64Val >> 24U);
    *(pu8DataPtr + 5) = (uint8)(u64Val >> 16U);
    *(pu8DataPtr + 6) = (uint8)(u64Val >> 8U);
    *(pu8DataPtr + 7) = (uint8)u64Val;
}

/********************************************************************************
  *
  * @fn PRIVATE void vSetStayAwakeFlagIfNeeded
  *
  */
 /**
  *
  * @param u16Length uint16  Length of data to be transmitted to JN
  *
  *
  * @brief Sets stay awake flag on a sleepy device
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PRIVATE void vSetStayAwakeFlagIfNeeded(uint16 u16Length)
{
}
/********************************************************************************
  *
  * @fn PRIVATE void vResetStayAwakeFlagIfNeeded
  *
  */
 /**
  *
  * @param u16Length uint16  Length of data transmitted to JN
  *
  *
  * @brief Resets stay awake flag on a sleepy device
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PRIVATE void vResetStayAwakeFlagIfNeeded(uint16 u16Length)
{
}
/********************************************************************************
  *
  * @fn PUBLIC uint8 vConfigureCoprocessorUartAckTimeout
  *
  */
 /**
  *
  * @param u16Time uint16  Time in ms
  *
  *
  * @brief Sets the JN Uart ACK Timeout
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 vConfigureCoprocessorUartAckTimeout(uint16 u16Time)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8PermitDuration */
    *pu8TxBuffer++ = (uint8)(u16Time >> 8U);
    *pu8TxBuffer++ = (uint8)u16Time;

    u16TxLength += sizeof(uint16);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_JN_ACK_TIMEOUT, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;
}
/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSendGetClearMacIfTxCount
  *
  */
 /**
  *
  * @param u8GetClear uint8
  * @param u8MacId uint8
  * @param pu32TxCount uint32 *
  *
  * @brief Send Get/Clear Mac Interface Retry Count.
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus eSendGetClearMacIfTxCount(uint8 u8GetClear,
        uint8 u8MacId, uint32* pu32TxCount)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8GetClear */
    pu8TxBuffer[u16TxLength] = u8GetClear;
    u16TxLength += sizeof(uint8);

    /* Copy u8MacId */
    pu8TxBuffer[u16TxLength] = (u8MacId);
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    return u8SL_WriteMessage((uint16)E_SL_MSG_GET_CLEAR_TX_COUNT, u16TxLength,
            au8TxSerialBuffer, pu32TxCount);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SendCloneZed
  *
  */
 /**
  *
  * @param pu8NwkKey uint8 *
  * @param u64Epid uint64
  * @param u64TCAddr uint64
  * @param u8KSN uint8
  * @param u8Channel uint8
  * @param u16NwkAddr uint16
  * @param u16PanId uint16
  *
  * @brief void
  *
  * @return PUBLIC uint8
  *
  * @note
  *
 ********************************************************************************/
PUBLIC uint8 u8SendCloneZed( uint8 *pu8NwkKey,
                               uint64 u64Epid,
                               uint64 u64TCAddr,
                               uint8 u8KSN,
                               uint8 u8Channel,
                                 uint16 u16NwkAddr,
                                 uint16 u16PanId)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], u8Status;
    uint16 u16TxLength = 0x00U;
    uint8 *pu8TxBuffer;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy Network Key */
    (void)ZBmemcpy(pu8TxBuffer, pu8NwkKey, 16UL);
    u16TxLength += 16U;
    pu8TxBuffer += 16U;

    /* Copy Extended PAN ID */
    vSL_ConvU64ToBi( u64Epid, pu8TxBuffer);
    u16TxLength += sizeof(uint64);
    pu8TxBuffer += sizeof(uint64);

    /* Copy Trust Center Address */
    vSL_ConvU64ToBi( u64TCAddr, pu8TxBuffer);
    u16TxLength += sizeof(uint64);
    pu8TxBuffer += sizeof(uint64);

    /* Copy Key Sequence Number */
    *pu8TxBuffer++  = u8KSN;
    u16TxLength++;

    /* Copy Channel */
    *pu8TxBuffer++  = u8Channel;
    u16TxLength++;

    /* Copy Network Address */
    *pu8TxBuffer++ = (uint8)(u16NwkAddr >> 8U);
    *pu8TxBuffer++ = (uint8)u16NwkAddr;
    u16TxLength += sizeof(uint16);

    /* Copy PAN ID */
    *pu8TxBuffer++ = (uint8)(u16PanId >> 8U);
    *pu8TxBuffer++ = (uint8)u16PanId;
    u16TxLength += sizeof(uint16);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_CLONE_ZED, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SendPdmConversionCommand
  *
  */
 /**
  *
  * @param u8FromVersion uint8
  * @param u8ToVersion uint8
  *
  * @brief Send Get/Clear Mac Interface Retry Count.
  *
  * @return ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SendPdmConversionCommand(uint8 u8FromVersion, uint8 u8ToVersion)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], u8Status;

    /* Copy u8FromVersion & u8ToVersion */
    au8TxSerialBuffer[0] = u8FromVersion;
    au8TxSerialBuffer[1] = u8ToVersion;

    /* Set Long Response Timeout Period */
    vSL_SetLongResponsePeriod();

    /* Send over serial */
    u8Status = (uint8)u8SL_WriteMessage((uint16)E_SL_MSG_PDM_CONVERT, 2U, au8TxSerialBuffer, NULL);

    /* Reset to Short Response Timeout Period */
    vSL_SetStandardResponsePeriod();
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 eHaltJN
  *
  */
 /**
  *
  *
  * @brief Send cmd to halt ther JN
  *
  * @return Status
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 eHaltJN(void)
{
    uint8 u8Status;
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_HALT_JN, 0U, NULL, NULL);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8CountApdus
  *
  */
 /**
  *
  * @param pu8Count uint8 *
  *
  * @brief Send cmd to count free apdus in each pool
  *
  * @return S_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8CountApdus(uint8* pu8Count)
{
    return u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_COUNT_APDU, 0U, NULL, pu8Count);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8ExhaustApdus
  *
  */
 /**
  *
  *
  * @brief Send cmd to exhaust all apdus
  *
  * @return Status
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8ExhaustApdus(void)
{

    return u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_EXHAUST_APDU, 0U, NULL, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8setStackPollRate
  *
  */
 /**
  *
  * @param u16RateMs uint16
  *
  * @brief Send cmd to set poll rate for frag and aps acks
  *
  * @return status
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8setStackPollRate(uint16 u16RateMs)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16RateMs */
    *pu8TxBuffer++ = (uint8)(u16RateMs >> 8U);
    *pu8TxBuffer++ = (uint8)u16RateMs;
    u16TxLength += sizeof(uint16);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_STACK_POLL_RATE, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;
}


/********************************************************************************
  *
  * @fn PUBLIC uint32 u32GetInFrameCounter
  *
  */
 /**
  *
  * @param u16Index uint16
  *
  * @brief Send get incoming frame ctr for given index in NT
  *
  * @return u32InFrameCtr
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint32 u32GetInFrameCounter(uint16 u16Index)
{
    uint32 u32FrameCounter = 0UL;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16Index */
    *pu8TxBuffer++ = (uint8)(u16Index >> 8U);
    *pu8TxBuffer++ = (uint8)u16Index;
    u16TxLength += sizeof(uint16);

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_GET_NWK_INFC, u16TxLength, au8TxSerialBuffer, &u32FrameCounter);

    return u32FrameCounter;

}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SetLeveDecider
  *
  */
 /**
  *
  * @param u8LeaveFlags uint8
  *
  * @brief Send command to set the leave decider conditions
  *
  * @return status
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SetLeveDecider(uint8 u8LeaveFlags)
{
    return u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_SET_LEAVE_DECIDER, 1U, &u8LeaveFlags, NULL);;
}



/********************************************************************************
  *
  * @fn PUBLIC uint8 u8ExhaustDescriptors
  *
  */
 /**
  *
  *
  * @brief Send cmd to exhaust all request descriptors
  *
  * @return status
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8ExhaustDescriptors( void)
{
    return u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_EXHAUST_REQ_DESC, 0U, NULL, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SetRxOnIdleState
  *
  */
 /**
  *
  * @param u8Intf uint8
  * @param u8State uint8
  *
  * @brief Set the RxOnIdle state for interface
  *
  * @return status
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SetRxOnIdleState(uint8 u8Intf, uint8 u8State)
{
    uint8 au8TxBuffer[10];

    /* Copy Interface and the RX ON when Idle State */
    au8TxBuffer[0] = u8Intf;
    au8TxBuffer[1] = u8State;

    return u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_SET_RXONIDLE, 2U, au8TxBuffer, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8GetRxOnIdleState
  *
  */
 /**
  *
  * @param u8Intf uint8
  * @param pu8RxOnIdle uint8 *
  *
  * @brief Get the RxOnIdle state for interface
  *
  * @return status
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8GetRxOnIdleState(uint8 u8Intf, uint8 *pu8RxOnIdle)
{
    return u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_GET_RXONIDLE, 1U, &u8Intf, pu8RxOnIdle);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SleepOL
  *
  */
 /**
  *
  * @param u8Mode uint8
  * @param u32TimeMs uint32
  *
  * @brief void
  *
  * @return PUBLIC uint8
  *
  * @note
  *
 ********************************************************************************/
PUBLIC uint8 u8SleepOL(uint8 u8Mode, uint32 u32TimeMs)
{
    uint8 au8TxBuffer[10];
    uint16 u16TxLength = 0U;

    /* Copy u8Mode */
    au8TxBuffer[0] = u8Mode;
    u16TxLength += sizeof(uint8);

    /* Copy u32TimeMs */
    vSL_ConvU32ToBi(u32TimeMs, &au8TxBuffer[1] );
    u16TxLength += sizeof(uint32);

    return u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_SLEEP_OL, u16TxLength, au8TxBuffer, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8WakeOL
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return PUBLIC uint8
  *
  * @note
  *
 ********************************************************************************/
PUBLIC uint8 u8WakeOL( void)
{
    return u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_WAKE_OL, 0U, NULL, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8ReadOLParams
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return PUBLIC uint8
  *
  * @note
  *
 ********************************************************************************/
PUBLIC uint8 u8ReadOLParams( void)
{
    return u8SL_WriteMessage((uint16)E_SL_MSG_READ_OL_PARAMS, 0U, NULL, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8GetSerialStats
  *
  */
 /**
  *
  * @param psStats tsJNSerialStats *
  *
  * @brief void
  *
  * @return PUBLIC uint8
  *
  * @note
  *
 ********************************************************************************/
PUBLIC uint8 u8GetSerialStats( tsJNSerialStats *psStats)
{
    uint8 u8Status = 0U;
    uint8 au8RxBuffer[11U*sizeof(uint32)] = {0U};
    uint8 u8Index = 0U;
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_APS_SERIAL_STATS, 0U, NULL, au8RxBuffer);
    if (u8Status == 0U)
    {
        psStats->u32SLRxCount = u32SL_ConvBiToU32( &au8RxBuffer[u8Index]);
        u8Index += sizeof(uint32);
        psStats->u32SLRxFail = u32SL_ConvBiToU32( &au8RxBuffer[u8Index]);
        u8Index += sizeof(uint32);
        psStats->u32SLTxStatusMsg = u32SL_ConvBiToU32( &au8RxBuffer[u8Index]);
        u8Index += sizeof(uint32);
        psStats->u32SLTxEventMsg = u32SL_ConvBiToU32( &au8RxBuffer[u8Index]);
        u8Index += sizeof(uint32);
        psStats->u32OverwrittenRXMessage = u32SL_ConvBiToU32( &au8RxBuffer[u8Index]);
        u8Index += sizeof(uint32);
        psStats->u32SLTxRetries = u32SL_ConvBiToU32( &au8RxBuffer[u8Index]);
        u8Index += sizeof(uint32);
        psStats->u32SLTxFailures = u32SL_ConvBiToU32( &au8RxBuffer[u8Index]);
        u8Index += sizeof(uint32);
        psStats->u32SL5Ms = u32SL_ConvBiToU32( &au8RxBuffer[u8Index]);
        u8Index += sizeof(uint32);
        psStats->u32SL8Ms = u32SL_ConvBiToU32( &au8RxBuffer[u8Index]);
        u8Index += sizeof(uint32);
        psStats->u32SL10Ms = u32SL_ConvBiToU32( &au8RxBuffer[u8Index]);
        u8Index += sizeof(uint32);
        psStats->u32Greater10Ms = u32SL_ConvBiToU32( &au8RxBuffer[u8Index]);
    }
    return u8Status;
}


/********************************************************************************
  *
  * @fn PUBLIC uint8 u8GetNetworkOutFrameCounter
  *
  */
 /**
  *
  * @param pu32FrameCounter uint32 *
  *
  * @brief void
  *
  * @return PUBLIC uint8
  *
  * @note
  *
 ********************************************************************************/
PUBLIC uint8 u8GetNetworkOutFrameCounter( uint32* pu32FrameCounter)
{
    return u8SL_WriteMessage((uint16)E_SL_MSG_GET_NWK_OUTGOING_FRAME_COUNT, 0U, NULL, pu32FrameCounter);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8GetCounterForAddress
  *
  */
 /**
  *
  * @param u64Address uint64
  *
  * @brief Get and printf the In and Out frame counters for given ieee address
  *
  * @return status
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8GetCounterForAddress(uint64 u64Address)
{
uint32 u32Outgoing, u32Incoming;
uint8 au8TxSerialBuffer[10] = {0U};
uint8 au8Response[10] = {0U};
uint8 u8Status;
uint16 u16TxLength = 0U;

union
{
    uint8*  pu8;
    uint32* pu32;
} upBuf;

    /* Copy u64Address */
    vSL_ConvU64ToBi(u64Address, au8TxSerialBuffer);
    u16TxLength += (uint16)sizeof(uint64);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_APS_FC_IEEE, u16TxLength, au8TxSerialBuffer, au8Response);
    if (u8Status == 0U)
    {
        /* the address was in the key tabnle here are the frame counters  */
        upBuf.pu8 = au8Response;
        u32Outgoing = *upBuf.pu32;
        u32Incoming = *(upBuf.pu32 + 1);   /* points to au8Response[4] */
        DBG_vPrintf((bool_t)TRUE, "Device %016lx has Out FC %08x and In FC %08x\n", u64Address, u32Outgoing, u32Incoming);
    }
    else
    {
        DBG_vPrintf((bool_t)TRUE, "Device %016lx not found\n", u64Address);
    }
    return u8Status;
}


/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SerialOutOfSeqTest
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return PUBLIC uint8
  *
  * @note
  *
 ********************************************************************************/
PUBLIC uint8 u8SerialOutOfSeqTest( void)
{
    return u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_SEQ_TEST, 0U, NULL, NULL);
}


/********************************************************************************
  *
  * @fn PRIVATE bool_t bFromU8
  *
  */
 /**
  *
  * @param u8Val uint8
  *
  * @brief Convert a uint8 to a bool_t
  *
  * @return bool_t
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PRIVATE bool_t bFromU8(uint8 u8Val)
{
    bool_t bResult;

    if (u8Val == 0U)
    {
        bResult = (bool_t)FALSE;
    }
    else
    {
        bResult = (bool_t)TRUE;
    }

    return bResult;
}

/********************************************************************************
  *
  * @fn PRIVATE uint8 u8FromBool
  *
  */
 /**
  *
  * @param bVal bool_t
  *
  * @brief Convert a bool_t value to a uint8
  *
  * @return uint8
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PRIVATE uint8  u8FromBool(bool_t bVal)
{
    uint8 u8Result;

    if (bVal == (bool_t)TRUE)
    {
        u8Result = 1U;
    }
    else
    {
        u8Result = 0U;
    }

    return u8Result;
}


/********************************************************************************
  *
  * @fn PPUBLIC uint8 u8SendErrorTestcode
  *
  */
 /**
  *
  * @param u8Code uint8
  *
  * @brief sends a test api error to to the JN for it to return
  *
  * @return uint8
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SendErrorTestcode(uint8 u8Code)
{
    return u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_TEST_ERR_CODE, 1U, &u8Code, NULL);

}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8GetStatusFlags
  *
  */
 /**
  *
  * @param pu32Flags uint32 *
  *
  * @brief Gets the resource status flags from the JN
  *
  * @return uint8
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8GetStatusFlags( uint32 *pu32Flags)
{
    return u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_GET_STATUS_FLAGS, 0U, NULL, pu32Flags);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8ResetBoundServer
  *
  */
 /**
  *
  * @param
  *
  * @brief Resets the stack bind server if locked
  *
  * @return uint8
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8ResetBoundServer(void)
{
    return  u8SL_WriteMessage((uint16)E_SL_MSG_SERIAL_LINK_RESET_BIND_SERVER, 0U, NULL, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8EnableBoundDevices
  *
  */
 /**
  *
  * @param
  *
  * @brief Set removes the block in the binding table for all addresses
  *
  * @return uint8
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8EnableBoundDevices(bool_t bEnable)
{
    uint8 u8TxByte = (uint8)bEnable;
    return u8SL_WriteMessage( (uint16)E_SL_MSG_SERIAL_LINK_ENABLE_BOUND_DEVICES, 1U, &u8TxByte, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SetBoundDevice
  *
  */
 /**
  *
  * @param
  *
  * @brief allows a given address in the binding table to be blocked or enabled to send
  *
  * @return uint8
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SetBoundDevice(uint64 u64Address, bool_t bState)
{
    uint8 au8TxSerialBuffer[10], u8Status;
    uint16 u16TxLength = 0x00U;

    /* Copy u64Address */
    vSL_ConvU64ToBi( u64Address, au8TxSerialBuffer);
    u16TxLength += sizeof(uint64);

    /* Copy bState */
    au8TxSerialBuffer[u16TxLength++] = (uint8)bState;

    u8Status = u8SL_WriteMessage( (uint16)E_SL_MSG_SERIAL_LINK_SET_BOUND_DEVICE, u16TxLength, au8TxSerialBuffer, NULL);

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8GetNwkState
  *
  */
 /**
  *
  * @param
  *
  * @brief gets the Zdo and Nwk states from the JN
  *
  * @return uint8
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8GetNwkState( uint8* pu8ZdoState, uint8* pu8NwkState)
{
    uint8 u8Status;
    uint8 au8Response[2];
    u8Status = u8SL_WriteMessage( (uint16)E_SL_MSG_SERIAL_LINK_GET_NWK_STATE, 0U, NULL, au8Response);
    if (u8Status == 0u)
    {
        *pu8ZdoState = au8Response[0];
        *pu8NwkState = au8Response[1];
    }

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SetManufacturerCode
  *
  */
 /**
  *
  * @param uint16 u16ManCode
  *
  * @brief Send the command to set the Manufacturer Code of the device
  *
  * @return uint8
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SetManufacturerCode( uint16 u16ManCode )
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    uint8 u8Status;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16ManCode */
    *pu8TxBuffer++ = (uint8)( (u16ManCode >> 8U) & 0xFFU );
    *pu8TxBuffer++ = (uint8)( u16ManCode & 0xFFU );
    u16TxLength += sizeof(uint16);

    u8Status = u8SL_WriteMessage( (uint16)E_SL_MSG_SERIAL_LINK_SET_MANUFACTURER_CODE, u16TxLength, au8TxSerialBuffer, NULL );

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SetJnAuthorizationTimeout
  *
  */
 /**
  *
  * @param uint8 u8TimeOut
  *
  * @brief Set Set APS Security Timeout Period
  *
  * @return uint8
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SetJnAuthorizationTimeout(uint8 u8TimeOut)
{
    uint8 u8Status = u8SL_WriteMessage( (uint16)E_SL_MSG_SET_SECURITY_TIMEOUT, 1, &u8TimeOut, NULL );

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SendEndDeviceTimeOut
  *
  */
 /**
  *
  * @param
  *
  * @brief Requests the Jn sends its keep alive message
  *
  * @return uint8
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SendEndDeviceTimeOut( void)
{
    uint8 u8Status = u8SL_WriteMessage( (uint16)E_SL_MSG_SEND_ZED_TIMEOUT, 0, NULL, NULL );

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSL_NetworkChangeAddress
  *
  */
 /**
  *
  * @param u16NwkAddr  R  New network address
  *
  * @brief Change the network address
  *
  * @return ZPS_teStatus    SUCCESS if the command was sent across the link
  *
  * @note
  *
 ********************************************************************************/
PUBLIC ZPS_teStatus eSL_NetworkChangeAddress(uint16 u16NwkAddr)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16NwkAddr */
    *pu8TxBuffer++ = (uint8)( (u16NwkAddr >> 8U) & 0xFFU );
    *pu8TxBuffer++ = (uint8)( u16NwkAddr & 0xFFU );
    u16TxLength += sizeof(uint16);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_NETWORK_CHANGE_ADDRESS, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSL_GetJnAuthorizationTimeout
  *
  */
 /**
  *
  * @param u16NwkAddr  W  Authorization timeout
  *
  * @brief Get the APS Security timeout
  *
  * @return ZPS_teStatus    SUCCESS if the command was sent across the link
  *
  * @note
  *
 ********************************************************************************/
PUBLIC ZPS_teStatus eSL_GetJnAuthorizationTimeout(uint16 *pu16AuthorizationTimeout)
{
    uint8 u8Status;
    uint16 u16Response;

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_SECURITY_TIMEOUT, 0U, NULL, &u16Response);
    DBG_vPrintf((bool_t)TRACE_SL, "eSL_GetJnAuthorizationTimeout status %d response %d\n", u8Status, u16Response);

    if (u8Status == 0u) {
        *pu16AuthorizationTimeout = u16Response;
    }

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfUnicastAckDataReq
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param u32ClId_DstEp_SrcEp uint32
  * @param u16DestAddr uint16
  * @param u16SecMd_Radius uint16
  * @param pu8SeqNum uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfUnicastAckDataReq(
                    void                        *pvApl,
                    PDUM_thAPduInstance         hAPduInst,
                    uint32                      u32ClId_DstEp_SrcEp,
                    uint16                      u16DestAddr,
                    uint16                      u16SecMd_Radius,
                    uint8                       *pu8SeqNum)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, *pu8Payload, u8Status;
    uint16 u16Len = 0x00U;
    uint16 u16TxLength = 0x00U;

    if (0xfff8U <= u16DestAddr)
    {
        /* address cannot be a reserved or broadcast address */
        return (uint8)ZPS_APL_APS_E_INVALID_PARAMETER;
    }

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy Cluster Id, DSt Ep, Src EP */
    vSL_ConvU32ToBi(u32ClId_DstEp_SrcEp, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy Dst Addr */
    *pu8TxBuffer++ = (uint8)(u16DestAddr >> 8U);
    *pu8TxBuffer++ = (uint8)u16DestAddr;
    u16TxLength += sizeof(uint16);

    /* Copy Sec Mode and Radius */
    *pu8TxBuffer++ = (uint8)(u16SecMd_Radius >> 8U);
    *pu8TxBuffer++ = (uint8)u16SecMd_Radius;
    u16TxLength += sizeof(uint16);

    /* Copy payload size */
    u16Len = PDUM_u16APduInstanceGetPayloadSize(hAPduInst);
    *pu8TxBuffer++ = (uint8)(u16Len >> 8U);
    *pu8TxBuffer++ = (uint8)u16Len;
    u16TxLength += sizeof(uint16);
    vSetStayAwakeFlagIfNeeded(u16Len);
    /* Copy Payload */
    pu8Payload = (uint8 *)(PDUM_pvAPduInstanceGetPayload(hAPduInst));
    (void)ZBmemcpy(pu8TxBuffer, pu8Payload, (uint32)u16Len);
    u16TxLength += u16Len;

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_UNICAST_ACK_DATA_REQ, u16TxLength,
            au8TxSerialBuffer, pu8SeqNum);

    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    vResetStayAwakeFlagIfNeeded(u16Len);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfUnicastDataReq
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param u32ClId_DstEp_SrcEp uint32
  * @param u16DestAddr uint16
  * @param u16SecMd_Radius uint16
  * @param pu8SeqNum uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfUnicastDataReq(
                    void                        *pvApl,
                    PDUM_thAPduInstance         hAPduInst,
                    uint32                      u32ClId_DstEp_SrcEp,
                    uint16                      u16DestAddr,
                    uint16                      u16SecMd_Radius,
                    uint8                       *pu8SeqNum)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, *pu8Payload, u8Status;
    uint16 u16Len = 0x00U;
    uint16 u16TxLength = 0x00U;

    if (0xfff8U <= u16DestAddr)
    {
        /* address cannot be a reserved or broadcast address */
        return (uint8)ZPS_APL_APS_E_INVALID_PARAMETER;
    }

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy Cluster Id, DSt Ep, Src EP */
    vSL_ConvU32ToBi(u32ClId_DstEp_SrcEp, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy Dst Addr */
    *pu8TxBuffer++ = (uint8)(u16DestAddr >> 8U);
    *pu8TxBuffer++ = (uint8)u16DestAddr;
    u16TxLength += sizeof(uint16);

    /* Copy Sec Mode and Radius */
    *pu8TxBuffer++ = (uint8)(u16SecMd_Radius >> 8U);
    *pu8TxBuffer++ = (uint8)u16SecMd_Radius;
    u16TxLength += sizeof(uint16);

    /* Copy payload size */
    u16Len = PDUM_u16APduInstanceGetPayloadSize(hAPduInst);
    *pu8TxBuffer++ = (uint8)(u16Len >> 8U);
    *pu8TxBuffer++ = (uint8)u16Len;
    u16TxLength += sizeof(uint16);

    /* Copy Payload */
    pu8Payload = (uint8 *)(PDUM_pvAPduInstanceGetPayload(hAPduInst));
    (void)ZBmemcpy(pu8TxBuffer, pu8Payload, (uint32)u16Len);
    u16TxLength += u16Len;
    vSetStayAwakeFlagIfNeeded(u16Len);
    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_UNICAST_DATA_REQ, u16TxLength, au8TxSerialBuffer, pu8SeqNum);

    vResetStayAwakeFlagIfNeeded(u16Len);

    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfUnicastIeeeAckDataReq
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param u32ClId_DstEp_SrcEp uint32
  * @param pu64DestAddr uint64 *
  * @param u16SecMd_Radius uint16
  * @param pu8SeqNum uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfUnicastIeeeAckDataReq(
                    void                        *pvApl,
                    PDUM_thAPduInstance         hAPduInst,
                    uint32                      u32ClId_DstEp_SrcEp,
                    uint64                      *pu64DestAddr,
                    uint16                      u16SecMd_Radius,
                    uint8                       *pu8SeqNum)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, *pu8Payload, u8Status;
    uint16 u16Len = 0x00U;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy Cluster Id, DSt Ep, Src EP */
    vSL_ConvU32ToBi(u32ClId_DstEp_SrcEp, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy IEEE Dst Addr */
    vSL_ConvU64ToBi(*pu64DestAddr, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint64);
    u16TxLength += sizeof(uint64);

    /* Copy Sec Mode and Radius */
    *pu8TxBuffer++ = (uint8)(u16SecMd_Radius >> 8U);
    *pu8TxBuffer++ = (uint8)u16SecMd_Radius;
    u16TxLength += sizeof(uint16);

    /* Copy payload size */
    u16Len = PDUM_u16APduInstanceGetPayloadSize(hAPduInst);
    *pu8TxBuffer++ = (uint8)(u16Len >> 8U);
    *pu8TxBuffer++ = (uint8)u16Len;
    u16TxLength += sizeof(uint16);

    /* Copy Payload */
    pu8Payload = (uint8 *)(PDUM_pvAPduInstanceGetPayload(hAPduInst));
    (void)ZBmemcpy(pu8TxBuffer, pu8Payload, (uint32)u16Len);
    u16TxLength += u16Len;
    vSetStayAwakeFlagIfNeeded(u16Len);
    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_UNICAST_IEEE_ACK_DATA_REQ, u16TxLength,
            au8TxSerialBuffer, pu8SeqNum);
    vResetStayAwakeFlagIfNeeded(u16Len);
    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfUnicastIeeeDataReq
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param u32ClId_DstEp_SrcEp uint32
  * @param pu64DestAddr uint64 *
  * @param u16SecMd_Radius uint16
  * @param pu8SeqNum uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfUnicastIeeeDataReq(
                    void                        *pvApl,
                    PDUM_thAPduInstance         hAPduInst,
                    uint32                      u32ClId_DstEp_SrcEp,
                    uint64                      *pu64DestAddr,
                    uint16                      u16SecMd_Radius,
                    uint8                       *pu8SeqNum)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, *pu8Payload, u8Status;
    uint16 u16Len = 0x00U;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy Cluster Id, DSt Ep, Src EP */
    vSL_ConvU32ToBi(u32ClId_DstEp_SrcEp, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy IEEE Dst Addr */
    vSL_ConvU64ToBi(*pu64DestAddr, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint64);
    u16TxLength += sizeof(uint64);

    /* Copy Sec Mode and Radius */
    *pu8TxBuffer++ = (uint8)(u16SecMd_Radius >> 8U);
    *pu8TxBuffer++ = (uint8)u16SecMd_Radius;
    u16TxLength += sizeof(uint16);

    /* Copy payload size */
    u16Len = PDUM_u16APduInstanceGetPayloadSize(hAPduInst);
    *pu8TxBuffer++ = (uint8)(u16Len >> 8U);
    *pu8TxBuffer++ = (uint8)u16Len;
    u16TxLength += sizeof(uint16);

    /* Copy Payload */
    pu8Payload = (uint8 *)(PDUM_pvAPduInstanceGetPayload(hAPduInst));
    (void)ZBmemcpy(pu8TxBuffer, pu8Payload, (uint32)u16Len);
    u16TxLength += u16Len;
    vSetStayAwakeFlagIfNeeded(u16Len);
    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_UNICAST_IEEE_DATA_REQ, u16TxLength, au8TxSerialBuffer, pu8SeqNum);
    vResetStayAwakeFlagIfNeeded(u16Len);
    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfBoundAckDataReq
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param u32ClId_SrcEp uint32
  * @param u16SecMd_Radius uint16
  * @param pu8SeqNum uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfBoundAckDataReq(
                    void                        *pvApl,
                    PDUM_thAPduInstance         hAPduInst,
                    uint32                      u32ClId_SrcEp,
                    uint16                      u16SecMd_Radius,
                    uint8                       *pu8SeqNum)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, *pu8Payload, u8Status;
    uint16 u16Len = 0x00U;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy Cluster Id, Src EP */
    vSL_ConvU32ToBi(u32ClId_SrcEp, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy Sec Mode and Radius */
    *pu8TxBuffer++ = (uint8)(u16SecMd_Radius >> 8U);
    *pu8TxBuffer++ = (uint8)u16SecMd_Radius;
    u16TxLength += sizeof(uint16);

    /* Copy payload size */
    u16Len = PDUM_u16APduInstanceGetPayloadSize(hAPduInst);
    DBG_vPrintf((bool_t)TRUE, "CLust src Ep %08x secRad %04x Pl length %d\r\n",
            u32ClId_SrcEp,
            u16SecMd_Radius,
            u16Len);
    *pu8TxBuffer++ = (uint8)(u16Len >> 8U);
    *pu8TxBuffer++ = (uint8)u16Len;
    u16TxLength += sizeof(uint16);

    /* Copy Payload */
    pu8Payload = (uint8 *)(PDUM_pvAPduInstanceGetPayload(hAPduInst));
    (void)ZBmemcpy(pu8TxBuffer, pu8Payload, (uint32)u16Len);
    u16TxLength += u16Len;
    vSetStayAwakeFlagIfNeeded(u16Len);
    /* Send over serial */
    DBG_vPrintf((bool_t)TRUE, "send Bound Ack length %d\r\n", u16TxLength );
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_BOUND_ACK_DATA_REQ, u16TxLength, au8TxSerialBuffer, pu8SeqNum);
    vResetStayAwakeFlagIfNeeded(u16Len);
    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfBoundDataReq
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param u32ClId_SrcEp uint32
  * @param u16SecMd_Radius uint16
  * @param pu8SeqNum uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfBoundDataReq(
                    void                        *pvApl,
                    PDUM_thAPduInstance         hAPduInst,
                    uint32                      u32ClId_SrcEp,
                    uint16                      u16SecMd_Radius,
                    uint8                       *pu8SeqNum)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, *pu8Payload, u8Status;
    uint16 u16Len = 0x00U;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy Cluster Id, Src EP */
    vSL_ConvU32ToBi(u32ClId_SrcEp, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy Sec Mode and Radius */
    *pu8TxBuffer++ = (uint8)(u16SecMd_Radius >> 8U);
    *pu8TxBuffer++ = (uint8)u16SecMd_Radius;
    u16TxLength += sizeof(uint16);

    /* Copy payload size */
    u16Len = PDUM_u16APduInstanceGetPayloadSize(hAPduInst);
    *pu8TxBuffer++ = (uint8)(u16Len >> 8U);
    *pu8TxBuffer++ = (uint8)u16Len;
    u16TxLength += sizeof(uint16);

    /* Copy Payload */
    pu8Payload = (uint8 *)(PDUM_pvAPduInstanceGetPayload(hAPduInst));
    (void)ZBmemcpy(pu8TxBuffer, pu8Payload, (uint32)u16Len);
    u16TxLength += u16Len;
    vSetStayAwakeFlagIfNeeded(u16Len);
    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_BOUND_DATA_REQ, u16TxLength, au8TxSerialBuffer, pu8SeqNum);
    vResetStayAwakeFlagIfNeeded(u16Len);
    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfBroadcastDataReq
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param u32ClId_DstEp_SrcEp uint32
  * @param eBroadcastMode ZPS_teAplAfBroadcastMode
  * @param u16SecMd_Radius uint16
  * @param pu8SeqNum uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfBroadcastDataReq(
                    void                        *pvApl,
                    PDUM_thAPduInstance         hAPduInst,
                    uint32                      u32ClId_DstEp_SrcEp,
                    ZPS_teAplAfBroadcastMode    eBroadcastMode,
                    uint16                      u16SecMd_Radius,
                    uint8                       *pu8SeqNum)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, *pu8Payload, u8Status;
    uint16 u16Len = 0x00U;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy Cluster Id, DSt Ep, Src EP */
    vSL_ConvU32ToBi(u32ClId_DstEp_SrcEp, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy Brd Addr Mode */
    *pu8TxBuffer++ = (uint8)(eBroadcastMode);
    u16TxLength += sizeof(uint8);

    /* Copy Sec Mode and Radius */
    *pu8TxBuffer++ = (uint8)(u16SecMd_Radius >> 8U);
    *pu8TxBuffer++ = (uint8)u16SecMd_Radius;
    u16TxLength += sizeof(uint16);

    /* Copy payload size */
    u16Len = PDUM_u16APduInstanceGetPayloadSize(hAPduInst);
    *pu8TxBuffer++ = (uint8)(u16Len >> 8U);
    *pu8TxBuffer++ = (uint8)u16Len;
    u16TxLength += sizeof(uint16);

    /* Copy Payload */
    pu8Payload = (uint8 *)(PDUM_pvAPduInstanceGetPayload(hAPduInst));
    (void)ZBmemcpy(pu8TxBuffer, pu8Payload, (uint32)u16Len);
    u16TxLength += u16Len;
    vSetStayAwakeFlagIfNeeded(u16Len);
    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_BROADCAST_DATA_REQ, u16TxLength, au8TxSerialBuffer, pu8SeqNum);
    vResetStayAwakeFlagIfNeeded(u16Len);
    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfGroupDataReq
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param u32ClId_SrcEp uint32
  * @param u16DstGroupAddr uint16
  * @param u16SecMd_Radius uint16
  * @param pu8SeqNum uint8 *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfGroupDataReq(
                    void                        *pvApl,
                    PDUM_thAPduInstance         hAPduInst,
                    uint32                      u32ClId_SrcEp,
                    uint16                      u16DstGroupAddr,
                    uint16                      u16SecMd_Radius,
                    uint8                       *pu8SeqNum)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, *pu8Payload, u8Status;
    uint16 u16Len = 0x00U;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy Cluster Id, DSt Ep, Src EP */
    vSL_ConvU32ToBi(u32ClId_SrcEp, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy Group Addr */
    *pu8TxBuffer++ = (uint8)(u16DstGroupAddr >> 8U);
    *pu8TxBuffer++ = (uint8)(u16DstGroupAddr);
    u16TxLength += sizeof(uint16);

    /* Copy Sec Mode and Radius */
    *pu8TxBuffer++ = (uint8)(u16SecMd_Radius >> 8U);
    *pu8TxBuffer++ = (uint8)u16SecMd_Radius;
    u16TxLength += sizeof(uint16);

    /* Copy payload size */
    u16Len = PDUM_u16APduInstanceGetPayloadSize(hAPduInst);
    *pu8TxBuffer++ = (uint8)(u16Len >> 8U);
    *pu8TxBuffer++ = (uint8)u16Len;
    u16TxLength += sizeof(uint16);

    /* Copy Payload */
    pu8Payload = (uint8 *)(PDUM_pvAPduInstanceGetPayload(hAPduInst));
    (void)ZBmemcpy(pu8TxBuffer, pu8Payload, (uint32)u16Len);
    u16TxLength += u16Len;
    vSetStayAwakeFlagIfNeeded(u16Len);
    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GROUP_DATA_REQ, u16TxLength, au8TxSerialBuffer, pu8SeqNum);
    vResetStayAwakeFlagIfNeeded(u16Len);
    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

PUBLIC ZPS_teStatus zps_eAplAfApsdeDataReq(void *pvApl,
        PDUM_thAPduInstance hAPduInst,
        ZPS_tsAfProfileDataReq* psProfileDataReq,
        uint8 *pu8SeqNum,
        uint8 eTxOptions)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, *pu8Payload, u8Status;
    uint16 u16Len = 0x00U;
    uint16 u16TxLength = 0x00U, u16SecMd_Radius;
    uint32 u32ClId_SrcEp;
    pu8TxBuffer = au8TxSerialBuffer;

    /* Create variable for Cluster Id, DSt Ep, Src EP */
    u32ClId_SrcEp = (psProfileDataReq->u16ClusterId << 16U) | 
                    (psProfileDataReq->u8DstEp << 8U) |  
                    (psProfileDataReq->u8SrcEp);

    /* Copy Cluster Id, DSt Ep, Src EP */
    vSL_ConvU32ToBi(u32ClId_SrcEp, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy address mode */
    *pu8TxBuffer++ = psProfileDataReq->eDstAddrMode;
    u16TxLength += sizeof(uint8);
    
    /* Copy the destination address - The address which will be put into the binding table */
    if(psProfileDataReq->eDstAddrMode == ZPS_E_ADDR_MODE_IEEE)
    {
        /* Copy IEEE destination address */
        vSL_ConvU64ToBi(psProfileDataReq->uDstAddr.u64Addr, pu8TxBuffer);
        pu8TxBuffer += sizeof(uint64);
        u16TxLength += sizeof(uint64);
    }
    else
    {
        /* Copy short destination address */
        *pu8TxBuffer++ = (uint8)(psProfileDataReq->uDstAddr.u16Addr >> 8U);
        *pu8TxBuffer++ = (uint8)(psProfileDataReq->uDstAddr.u16Addr);
        u16TxLength += sizeof(uint16);
    }

    /* Copy profile id */
    *pu8TxBuffer++ = (uint8)(psProfileDataReq->u16ProfileId >> 8U);
    *pu8TxBuffer++ = (uint8)psProfileDataReq->u16ProfileId;
    u16TxLength += sizeof(uint16);

    /* Create variable for Sec Mode and Radius */
    u16SecMd_Radius = (psProfileDataReq->eSecurityMode << 8U) |
                      (psProfileDataReq->u8Radius);

    /* Copy Sec Mode and Radius */
    *pu8TxBuffer++ = (uint8)(u16SecMd_Radius >> 8U);
    *pu8TxBuffer++ = (uint8)u16SecMd_Radius;
    u16TxLength += sizeof(uint16);

    /* Copy payload size */
    u16Len = PDUM_u16APduInstanceGetPayloadSize(hAPduInst);
    *pu8TxBuffer++ = (uint8)(u16Len >> 8U);
    *pu8TxBuffer++ = (uint8)u16Len;
    u16TxLength += sizeof(uint16);

    /* Copy Payload */
    pu8Payload = (uint8 *)(PDUM_pvAPduInstanceGetPayload(hAPduInst));
    (void)ZBmemcpy(pu8TxBuffer, pu8Payload, (uint32)u16Len);
    u16TxLength += u16Len;
    vSetStayAwakeFlagIfNeeded(u16Len);
    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_APSDE_DATA_REQUEST, u16TxLength, au8TxSerialBuffer, pu8SeqNum);
    vResetStayAwakeFlagIfNeeded(u16Len);
    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

PUBLIC ZPS_teStatus zps_eAplZdpActiveEpRequest(
    void *pvApl,
    PDUM_thAPduInstance hAPduInst,
    ZPS_tuAddress uDstAddr,
    bool bExtAddr,
    uint8 *pu8SeqNumber,
    ZPS_tsAplZdpActiveEpReq *psZdpActiveEpReq)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    bool_t bIsExtAddress;
    uint8 u8Status;

    pu8TxBuffer = au8TxSerialBuffer;

    bIsExtAddress = (FALSE != bExtAddr) ? (bool_t)TRUE : (bool_t)FALSE;

    /* Copy bIsExtAddress */
    *pu8TxBuffer++ = u8FromBool(bIsExtAddress);
    u16TxLength += sizeof(uint8);

    /* Copy destination address */
    if(bIsExtAddress)
    {
        vSL_ConvU64ToBi(uDstAddr.u64Addr, pu8TxBuffer);
        pu8TxBuffer += sizeof(uint64);
        u16TxLength += sizeof(uint64);
    }
    else
    {
        *pu8TxBuffer++ = (uint8)(uDstAddr.u16Addr >> 8U);
        *pu8TxBuffer++ = (uint8)(uDstAddr.u16Addr);
        u16TxLength += sizeof(uint16);
    }

    /* Copy address of interest */
    *pu8TxBuffer++ = (uint8)(psZdpActiveEpReq->u16NwkAddrOfInterest >> 8U);
    *pu8TxBuffer++ = (uint8)psZdpActiveEpReq->u16NwkAddrOfInterest;
    u16TxLength += sizeof(uint16);

    /* Send over serial */
    u8Status = u8SL_WriteMessage( (uint16)E_SL_MSG_ACTIVE_ENDPOINT_REQUEST, u16TxLength, au8TxSerialBuffer, pu8SeqNumber);

    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

PUBLIC ZPS_teStatus zps_eAplZdoAddReplaceLinkKey (void *pvApl, uint64 u64IeeeAddr,
                                                  uint8 au8Key[ZPS_SEC_KEY_LENGTH],
                                                  ZPS_teApsLinkKeyType eKeyType)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    uint8 u8Status;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u64IEEEAddress */
    vSL_ConvU64ToBi(u64IeeeAddr, pu8TxBuffer);
    u16TxLength += sizeof(uint64);
    pu8TxBuffer += sizeof(uint64);

    /* Copy au8Key */
    (void)ZBmemcpy( pu8TxBuffer, &au8Key, ZPS_SEC_KEY_LENGTH);
    pu8TxBuffer += ZPS_SEC_KEY_LENGTH;
    u16TxLength += ZPS_SEC_KEY_LENGTH;

    /* Copy key type */
    *pu8TxBuffer++ = eKeyType;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage( (uint16)E_SL_MSG_ADD_REPLACE_LINK_KEY, u16TxLength, au8TxSerialBuffer, NULL);

    return u8Status;
}

PUBLIC ZPS_teStatus zps_eAplZdoAddReplaceInstallCodes (void *pvApl,
                                                  uint64 u64IeeeAddr,
                                                  uint8 au8Key[ZPS_SEC_KEY_LENGTH],
                                                  uint8 u8Size,
                                                  ZPS_teApsLinkKeyType eKeyType)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    uint8 u8Status;

    pu8TxBuffer = au8TxSerialBuffer;

   /* Copy u64IEEEAddress */
    vSL_ConvU64ToBi(u64IeeeAddr, pu8TxBuffer);
    u16TxLength += sizeof(uint64);
    pu8TxBuffer += sizeof(uint64);

    /* Copy au8Key */
    (void)ZBmemcpy( pu8TxBuffer, au8Key, ZPS_SEC_KEY_LENGTH);
    pu8TxBuffer += ZPS_SEC_KEY_LENGTH;
    u16TxLength += ZPS_SEC_KEY_LENGTH;

    /* Copy key type */
    *pu8TxBuffer++ = eKeyType;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage( (uint16)E_SL_MSG_ADD_REPLACE_INSTALL_CODES, u16TxLength, au8TxSerialBuffer, NULL);

    return u8Status;
}

PUBLIC void *ZPS_pvNwkSecGetNetworkKey(void *pvNwk)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;

    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_NWK_KEY, u16TxLength, au8TxSerialBuffer, &u8NwkKey);

    return &u8NwkKey;
}

PUBLIC void zps_vSetZdoDeviceType(void *pvApl,uint8 u8DeviceType)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy ZDO device type*/
    (void)ZBmemcpy(pu8TxBuffer, &u8DeviceType, sizeof(uint8));
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_ZDO_SET_DEVICETYPE, u16TxLength, au8TxSerialBuffer, NULL);

}

PUBLIC void ZPS_vNwkNibSetUpdateId(void *pvNwk, uint8 u8UpdateId)
{
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SET_UPDATE_ID, (uint16)sizeof(u8UpdateId), &u8UpdateId,NULL);
}

PUBLIC void ZPS_vNwkNibSetPanId(void *pvNwk, uint16 u16PanId)
{
    eSL_ChangePanId(u16PanId);
}

PUBLIC void ZPS_vNwkSetDeviceType(void* pvNwk, ZPS_teNwkDeviceType eNwkDeviceType)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy NWK device type*/
    (void)ZBmemcpy(pu8TxBuffer, &eNwkDeviceType, sizeof(ZPS_teNwkDeviceType));
    pu8TxBuffer += sizeof(ZPS_teNwkDeviceType);
    u16TxLength += sizeof(ZPS_teNwkDeviceType);

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_NWK_SET_DEVICETYPE, u16TxLength, au8TxSerialBuffer, NULL);

}

PUBLIC void ZPS_vNwkNibSetKeySeqNum(void *pvNwk, uint8 u8ActiveKeySeqNumber)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy active key sequence number */
    (void)ZBmemcpy(pu8TxBuffer, &u8ActiveKeySeqNumber, sizeof(uint8));
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    (void)u8SL_WriteMessage((uint16)E_SL_SET_KEY_SEQ_NUMBER, u16TxLength, au8TxSerialBuffer, NULL);    
}

PUBLIC void ZPS_vNwkNibSetNwkAddr(void *pvNwk, uint16 u16NwkAddr)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy NWK address */
    (void)ZBmemcpy(pu8TxBuffer, &u16NwkAddr, sizeof(uint16));
    pu8TxBuffer += sizeof(uint16);
    u16TxLength += sizeof(uint16);

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SET_NWK_ADDR, u16TxLength, au8TxSerialBuffer, NULL);    
}

PUBLIC void zps_vAplAfSetMacCapability( void* pvApl, uint8 u8MacCapability )
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy MAC capability */
    (void)ZBmemcpy(pu8TxBuffer, &u8MacCapability, sizeof(uint8));
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SET_MAC_CAPABILITY, u16TxLength, au8TxSerialBuffer, NULL);    
}

PUBLIC void ZPS_vSetNwkStateActive ( void    *pvNwk )
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;

    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SET_NWK_STATE_ACTIVE, u16TxLength, au8TxSerialBuffer, NULL);
}

PUBLIC void ZPS_vNwkNibSetDepth(void *pvNwk, uint8 u8Depth)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy depth */
    (void)ZBmemcpy(pu8TxBuffer, &u8Depth, sizeof(uint8));
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SET_DEPTH, u16TxLength, au8TxSerialBuffer, NULL);    
}
/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_bAplZdoTrustCenterGetDevicePermissions
  *
  */
 /**
  *
  * @param pvZdo void *
  * @param u64Addr uint64
  * @param pu8DevicePermissions ZPS_teDevicePermissions *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_bAplZdoTrustCenterGetDevicePermissions(
                    void                        *pvZdo,
                    uint64                      u64Addr,
                    ZPS_teDevicePermissions     *pu8DevicePermissions)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy IEEE Addr */
    vSL_ConvU64ToBi(u64Addr, pu8TxBuffer);
    u16TxLength += sizeof(uint64);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_DEVICE_PERMISSIONS, u16TxLength, au8TxSerialBuffer, pu8DevicePermissions);

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_bAplZdoTrustCenterSetDevicePermissions
  *
  */
 /**
  *
  * @param pvZdo void *
  * @param u64Addr uint64
  * @param u8DevicePermissions ZPS_teDevicePermissions
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_bAplZdoTrustCenterSetDevicePermissions(
                    void                        *pvZdo,
                    uint64                      u64Addr,
                    ZPS_teDevicePermissions     u8DevicePermissions)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy IEEE Addr */
    vSL_ConvU64ToBi(u64Addr, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint64);
    u16TxLength += sizeof(uint64);

    /* Copy u8DevicePermissions */
    *pu8TxBuffer++ = (uint8)u8DevicePermissions;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_DEVICE_PERMISSIONS, u16TxLength, au8TxSerialBuffer, NULL);

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint64 ZPS_u64NwkNibGetExtAddr
  *
  */
 /**
  *
  * @param pvNwk void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint64 ZPS_u64NwkNibGetExtAddr(void *pvNwk)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], u8Status;
    uint16 u16TxLength = 0x00U;
    uint64 u64IEEEAddr = 0x0uLL;

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_EXT_ADDR,
                                 u16TxLength,
                                 au8TxSerialBuffer,
                                 &u64IEEEAddr);

    /* check status */
    if(u8Status == ZPS_E_SUCCESS)
    {
        return u64IEEEAddr;
    }

    return 0x00ULL;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAibSetApsTrustCenterAddress
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u64TcAddress uint64
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAibSetApsTrustCenterAddress(void *pvApl, uint64 u64TcAddress)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy IEEE Addr */
    vSL_ConvU64ToBi(u64TcAddress, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint64);
    u16TxLength += sizeof(uint64);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_TC_ADDRESS, u16TxLength, au8TxSerialBuffer, NULL);

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_vAplSecSetInitialSecurityState
  *
  */
 /**
  *
  * @param pvApl void *
  * @param eKeyState ZPS_teZdoNwkKeyState
  * @param pu8Key uint8 *
  * @param u8KeySeqNum uint8
  * @param eKeyType ZPS_teApsLinkKeyType
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_vAplSecSetInitialSecurityState(
                    void                        *pvApl,
                    ZPS_teZdoNwkKeyState        eKeyState,
                    uint8                       *pu8Key,
                    uint8                       u8KeySeqNum,
                    ZPS_teApsLinkKeyType        eKeyType)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy eKeyState */
    *pu8TxBuffer++ = (uint8)eKeyState;
    u16TxLength += sizeof(uint8);

    /* Copy Key */
    if(pu8Key == NULL)
    {
        (void)ZBmemset(pu8TxBuffer, 0, ZPS_SEC_KEY_LENGTH);
    }
    else
    {
        (void)ZBmemcpy(pu8TxBuffer, pu8Key, ZPS_SEC_KEY_LENGTH);
    }
    pu8TxBuffer += ZPS_SEC_KEY_LENGTH;
    u16TxLength += ZPS_SEC_KEY_LENGTH;

    /* Copy u8KeySeqNum */
    *pu8TxBuffer++ = u8KeySeqNum;
    u16TxLength += sizeof(uint8);

    /* Copy eKeyType */
    *pu8TxBuffer++ = (uint8)eKeyType;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_INIT_SEC_STATE, u16TxLength, au8TxSerialBuffer, NULL);
    /* Sanitize temporary variables */
    (void)ZBmemset(au8TxSerialBuffer, 0 , (uint32)u16TxLength);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfGetEndpointState
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u8Endpoint uint8
  * @param pbEnabled bool *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfGetEndpointState(void *pvApl, uint8 u8Endpoint, bool *pbEnabled)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8Endpoint */
    *pu8TxBuffer++ = u8Endpoint;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_ENDPOINT_STATE, u16TxLength, au8TxSerialBuffer, pbEnabled);

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfSetEndpointState
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u8Endpoint uint8
  * @param bEnabled bool
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfSetEndpointState(void *pvApl, uint8 u8Endpoint, bool bEnabled)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8Endpoint */
    *pu8TxBuffer++ = u8Endpoint;
    u16TxLength += sizeof(uint8);

    /* Copy bEnabled */
    *pu8TxBuffer++ = u8FromBool((bool_t)bEnabled);
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_ENDPOINT_STATE, u16TxLength, au8TxSerialBuffer, NULL);

    return u8Status;
}


/********************************************************************************
  *
  * @fn PUBLIC uint64 ZPS_u64NwkNibGetMappedIeeeAddr
  *
  */
 /**
  *
  * @param pvNwk void *
  * @param u16Location uint16
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint64 ZPS_u64NwkNibGetMappedIeeeAddr(void* pvNwk, uint16 u16Location)
{
    uint8 *pu8TxBuffer;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    uint64 u64IeeeAddress = 0ULL;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16Location - LookUp table index */
    *pu8TxBuffer++ = (uint8)(u16Location >> 8U);
    *pu8TxBuffer++ = (uint8)(u16Location);
    u16TxLength += sizeof(uint16);
    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_MAPPED_IEEE_ADDR,
                      u16TxLength,
                      au8TxSerialBuffer,
                      &u64IeeeAddress);

    return u64IeeeAddress;
}

/********************************************************************************
  *
  * @fn PUBLIC uint64 zps_u16AplAibGetDeviceKeyPairTableSize
  *
  */
 /**
  *
  * @param pvApl void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return APS key table size
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 zps_u16AplAibGetDeviceKeyPairTableSize(void *pvApl)
{
    return u16GetApsKeyTableSize();
}

/********************************************************************************
  *
  * @fn PUBLIC uint64 zps_u16AplAibGetGroupTableSize
  *
  */
 /**
  *
  * @param pvApl void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return APS group table size
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint32 zps_u32AplAibGetGroupTableSize(void *pvApl)
{
    return u32GetApsGroupTableSize();
}

/********************************************************************************
  *
  * @fn PUBLIC uint16 zps_u16AplAibGetGroupTableEntryGroupId
  *
  */
 /**
  *
  * @param pvApl void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return group ID member of group table entry 
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 zps_u16AplAibGetGroupTableEntryGroupId(void *pvApl, uint32 u32Index)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    uint16 u16GroupId = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u32Index */
    vSL_ConvU32ToBi(u32Index, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Send over serial */
    u8SL_WriteMessage((uint16)E_SL_MSG_GET_GROUP_TABLE_ENTRY_GROUP_ID, u16TxLength, au8TxSerialBuffer, &u16GroupId);

    return u16GroupId;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 zps_u8AplAibGetGroupTableEntryEndpoint
  *
  */
 /**
  *
  * @param pvApl void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return element of endpoint member of group table entry 
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 zps_u8AplAibGetGroupTableEntryEndpoint(void *pvApl, uint32 u32Index, uint8 u8ByteOffset)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    uint8 u8Endpoint = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u32Index */
    vSL_ConvU32ToBi(u32Index, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy u8ByteOffset */
    *pu8TxBuffer++ = u8ByteOffset;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8SL_WriteMessage((uint16)E_SL_MSG_GET_GROUP_TABLE_ENTRY_ENDPOINT_ELEMENT, u16TxLength, au8TxSerialBuffer, &u8Endpoint);

    return u8Endpoint;
}
/********************************************************************************
  *
  * @fn PUBLIC uint64 zps_tsAplAibGetDeviceKeyPairTableEntry
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u16Location u16Index
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return APS key table entry at given index
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_tsAplApsKeyDescriptorEntry zps_tsAplAibGetDeviceKeyPairTableEntry(void *pvApl, uint16 u16Index)
{
    ZPS_tsAplApsKeyDescriptorEntry key;
    eAPP_GetKeyTableEntry(u16Index, &key);
    return key;
}

/********************************************************************************
  *
  * @fn PUBLIC void* zps_pvAplZdoGetNwkHandle
  *
  */
 /**
  *
  * @param pvZdo void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void *zps_pvAplZdoGetNwkHandle(void *pvZdo)
{
    return NULL;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAibSetApsUseExtendedPanId
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u64UseExtPanId uint64
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAibSetApsUseExtendedPanId(void *pvApl, uint64 u64UseExtPanId)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy pu64Address */
    vSL_ConvU64ToBi(u64UseExtPanId, pu8TxBuffer);
    u16TxLength += sizeof(uint64);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_EXT_PANID, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAibSetApsChannelMask
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u32ChannelMask uint32
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAibSetApsChannelMask(void *pvApl, uint32 u32ChannelMask)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u32ChannelMask */
    vSL_ConvU32ToBi(u32ChannelMask, pu8TxBuffer);
    u16TxLength += sizeof(uint32);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_CHANNELMASK, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdoPermitJoining
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u8PermitDuration uint8
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdoPermitJoining(void *pvApl, uint8 u8PermitDuration)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8PermitDuration */
    *pu8TxBuffer++ = u8PermitDuration;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_PERMIT_JOINING_REQUEST, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;
}
/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdoStartStack
  *
  */
 /**
  *
  * @param pvApl void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdoStartStack(void *pvApl)
{
    uint8 u8Status;
    vSL_SetLongResponsePeriod();

    u32FormationStartTime = zbPlatGetTime();

    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_START_NETWORK, 0U, NULL,NULL);
    if (u8Status != 0U)
    {
        vSL_SetStandardResponsePeriod();
    }
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdoLeaveNetwork
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u64Addr uint64
  * @param bRemoveChildren bool
  * @param bRejoin bool
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdoLeaveNetwork(void *pvApl, uint64 u64Addr, bool bRemoveChildren, bool bRejoin)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy pu64Address */
    vSL_ConvU64ToBi(u64Addr, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint64);
    u16TxLength += sizeof(uint64);

    /* Copy bRemoveChildren */
    *pu8TxBuffer++ = (uint8)bRemoveChildren;
    u16TxLength += sizeof(uint8);

    /* Copy bRejoin */
    *pu8TxBuffer++ = (uint8)bRejoin;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_REMOVE_REMOTE_DEVICE, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdpMatchDescRequest
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param uDstAddr ZPS_tuAddress
  * @param bExtAddr bool
  * @param pu8SeqNumber uint8 *
  * @param psZdpMatchDescReq ZPS_tsAplZdpMatchDescReq *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdpMatchDescRequest(
                    void                        *pvApl,
                    PDUM_thAPduInstance         hAPduInst,
                    ZPS_tuAddress               uDstAddr,
                    bool                        bExtAddr,
                    uint8                       *pu8SeqNumber,
                    ZPS_tsAplZdpMatchDescReq    *psZdpMatchDescReq)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status,i;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;
    /* Copy target short address */
    *pu8TxBuffer++ = (uint8)(uDstAddr.u16Addr >> 8U);
    *pu8TxBuffer++ = (uint8)(uDstAddr.u16Addr);
    u16TxLength += sizeof(uint16);

    /* Copy NWK Address Of Interest */
    *pu8TxBuffer++ = (uint8)(psZdpMatchDescReq->u16NwkAddrOfInterest >> 8U);
    *pu8TxBuffer++ = (uint8)(psZdpMatchDescReq->u16NwkAddrOfInterest);
    u16TxLength += sizeof(uint16);

    /* Copy profile id */
    *pu8TxBuffer++ = (uint8)(psZdpMatchDescReq->u16ProfileId >> 8U);
    *pu8TxBuffer++ = (uint8)psZdpMatchDescReq->u16ProfileId;
    u16TxLength += sizeof(uint16);

    /* Copy number of input cluster */
    *pu8TxBuffer++ = psZdpMatchDescReq->u8NumInClusters;
    u16TxLength += sizeof(uint8);

    /* Copy incluster list */
    for(i=0U;i<psZdpMatchDescReq->u8NumInClusters;i++)
    {
        *pu8TxBuffer++ = (uint8)(psZdpMatchDescReq->pu16InClusterList[i] >> 8U);
        *pu8TxBuffer++ = (uint8)psZdpMatchDescReq->pu16InClusterList[i];
    }
    u16TxLength += (uint16)psZdpMatchDescReq->u8NumInClusters * 2U;

    /* Copy number of output cluster */
    *pu8TxBuffer++ = psZdpMatchDescReq->u8NumOutClusters;
    u16TxLength += sizeof(uint8);

    /* Copy outcluster list */
    for(i=0U;i<psZdpMatchDescReq->u8NumOutClusters;i++)
    {
        *pu8TxBuffer++ = (uint8)(psZdpMatchDescReq->pu16OutClusterList[i] >> 8U);
        *pu8TxBuffer++ = (uint8)psZdpMatchDescReq->pu16OutClusterList[i];
    }
    u16TxLength += ((uint16)psZdpMatchDescReq->u8NumOutClusters) * 2U;
    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_MATCH_DESCRIPTOR_REQUEST, u16TxLength, au8TxSerialBuffer, pu8SeqNumber);

    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdpIeeeAddrRequest
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param uDstAddr ZPS_tuAddress
  * @param bExtAddr bool
  * @param pu8SeqNumber uint8 *
  * @param psZdpIeeeAddrReq ZPS_tsAplZdpIeeeAddrReq *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdpIeeeAddrRequest( void *pvApl,
                                                PDUM_thAPduInstance hAPduInst,
                                                ZPS_tuAddress uDstAddr,
                                                bool bExtAddr,
                                                uint8 *pu8SeqNumber,
                                                ZPS_tsAplZdpIeeeAddrReq *psZdpIeeeAddrReq)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy target short address */
    *pu8TxBuffer++ = (uint8)(uDstAddr.u16Addr >> 8U);
    *pu8TxBuffer++ = (uint8)(uDstAddr.u16Addr);
    u16TxLength += sizeof(uint16);

    /* Copy look up address */
    *pu8TxBuffer++ = (uint8)(psZdpIeeeAddrReq->u16NwkAddrOfInterest >> 8U);
    *pu8TxBuffer++ = (uint8)psZdpIeeeAddrReq->u16NwkAddrOfInterest;
    u16TxLength += sizeof(uint16);

    /* Copy u8RequestType */
    *pu8TxBuffer++ = psZdpIeeeAddrReq->u8RequestType;
    u16TxLength += sizeof(uint8);

    /* Copy u8StartIndex */
    *pu8TxBuffer++ = psZdpIeeeAddrReq->u8StartIndex;
    u16TxLength += sizeof(uint8);
    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_IEEE_ADDRESS_REQUEST, u16TxLength, au8TxSerialBuffer, pu8SeqNumber);

    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdpNwkAddrRequest
  *
  */
 /**
  *
  * @param pvApl void *
  *
  * @param hAPduInst PDUM_tance ,
  *
  * @param uDstAddr ZPS_tuAddress ,
  *
  * @param bExtAddr bool ,
  *
  * @param pu8SeqNumber uint8 *,
  *
  * @param psZdpNwkAddrReq ZPS_tsAplZdpNwkAddrReq *
  *
  *
  * @brief Attempt to ask for a network address the serial link
  *
  * @return status of zps_eAplZdpNwkAddrRequest
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdpNwkAddrRequest(void *pvApl,
                                              PDUM_thAPduInstance hAPduInst,
                                              ZPS_tuAddress uDstAddr,
                                              bool bExtAddr,
                                              uint8 *pu8SeqNumber,
                                               ZPS_tsAplZdpNwkAddrReq *psZdpNwkAddrReq)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy target short address */
    *pu8TxBuffer++ = (uint8)(uDstAddr.u16Addr >> 8U);
    *pu8TxBuffer++ = (uint8)(uDstAddr.u16Addr);
    u16TxLength += sizeof(uint16);

    /* Copy look up address */
    vSL_ConvU64ToBi(psZdpNwkAddrReq->u64IeeeAddr, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint64);
    u16TxLength += sizeof(uint64);


    /* Copy u8RequestType */
    *pu8TxBuffer++ = psZdpNwkAddrReq->u8RequestType;
    u16TxLength += sizeof(uint8);

    /* Copy u8StartIndex */
    *pu8TxBuffer++ = psZdpNwkAddrReq->u8StartIndex;
    u16TxLength += sizeof(uint8);


    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_NETWORK_ADDRESS_REQUEST, u16TxLength, au8TxSerialBuffer, pu8SeqNumber);

    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdpBindUnbindRequest
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param uDstAddr ZPS_tuAddress
  * @param bExtAddr bool
  * @param pu8SeqNumber uint8 *
  * @param bBindReq bool
  * @param psZdpBindReq ZPS_tsAplZdpBindUnbindReq *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdpBindUnbindRequest(void *pvApl,
                                                 PDUM_thAPduInstance hAPduInst,
                                                 ZPS_tuAddress uDstAddr,
                                                 bool bExtAddr,
                                                 uint8 *pu8SeqNumber,
                                                 bool bBindReq,
                                                 ZPS_tsAplZdpBindUnbindReq *psZdpBindReq)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy the target IEEE bind address - The address the request is going to */
    vSL_ConvU64ToBi(psZdpBindReq->u64SrcAddress, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint64);
    u16TxLength += sizeof(uint64);

    /* Copy source endpoint */
    *pu8TxBuffer++ = psZdpBindReq->u8SrcEndpoint;
    u16TxLength += sizeof(uint8);

    /* Copy cluster id */
    *pu8TxBuffer++ = (uint8)(psZdpBindReq->u16ClusterId >> 8U);
    *pu8TxBuffer++ = (uint8)(psZdpBindReq->u16ClusterId);
    u16TxLength += sizeof(uint16);

    /* Copy destination address mode */
    *pu8TxBuffer++ = psZdpBindReq->u8DstAddrMode;
    u16TxLength += sizeof(uint8);

    /* Copy the destination address - The address which will be put into the binding table */
    if(psZdpBindReq->u8DstAddrMode == 3U)
    {
        /* Copy IEEE destination address */
        vSL_ConvU64ToBi(psZdpBindReq->uAddressField.sExtended.u64DstAddress, pu8TxBuffer);
        pu8TxBuffer += sizeof(uint64);
        u16TxLength += sizeof(uint64);

        /* Copy Destination Endpoint */
        *pu8TxBuffer++ = psZdpBindReq->uAddressField.sExtended.u8DstEndPoint;
        u16TxLength += sizeof(uint8);
    }
    else
    {
        /* Copy short/group destination address */
        *pu8TxBuffer++ = (uint8)(psZdpBindReq->uAddressField.sShort.u16DstAddress >> 8U);
        *pu8TxBuffer++ = (uint8)(psZdpBindReq->uAddressField.sShort.u16DstAddress);
        u16TxLength += sizeof(uint16);
    }

    /* Send over serial */
    if(bBindReq)
    {
        u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_BIND, u16TxLength, au8TxSerialBuffer, pu8SeqNumber);
    }
    else
    {
        u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_UNBIND, u16TxLength, au8TxSerialBuffer, pu8SeqNumber);
    }
    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus APP_eAplZdoBind
  *
  */
 /**
  *
  * @param u16ClusterId uint16
  * @param u8SrcEndpoint uint8
  * @param u16DstNwkAddr uint16
  * @param u64DstIeeeAddr uint64
  * @param u8DstEndpoint uint8
  * @param u8DstAddrMode uint8
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus APP_eAplZdoBind( uint16 u16ClusterId,
                                      uint8 u8SrcEndpoint,
                                      uint16 u16DstNwkAddr,
                                      uint64 u64DstIeeeAddr,
                                      uint8 u8DstEndpoint,
                                      uint8 u8DstAddrMode)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy the target IEEE bind address - The address the request is going to */
    vSL_ConvU64ToBi(u64DstIeeeAddr, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint64);
    u16TxLength += sizeof(uint64);

    /* Copy cluster id */
    *pu8TxBuffer++ = (uint8)(u16ClusterId >> 8U);
    *pu8TxBuffer++ = (uint8)(u16ClusterId);
    u16TxLength += sizeof(uint16);

    /* Copy the destination address - The address which will be put into the binding table */
    *pu8TxBuffer++ = (uint8)(u16DstNwkAddr >> 8U);
    *pu8TxBuffer++ = (uint8)(u16DstNwkAddr);
    u16TxLength += sizeof(uint16);

    /* Copy Source Endpoint */
    *pu8TxBuffer++ = u8SrcEndpoint;
    u16TxLength += sizeof(uint8);

    /* Copy Destination Endpoint */
    *pu8TxBuffer++ = u8DstEndpoint;
    u16TxLength += sizeof(uint8);

    /* Copy Destination Address Mode */
    *pu8TxBuffer++ = u8DstAddrMode;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_ZDO_BIND, u16TxLength, au8TxSerialBuffer, NULL);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdoRequestKeyReq
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u8KeyType uint8
  * @param u64IeeePartnerAddr uint64
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdoRequestKeyReq(void *pvApl, uint8 u8KeyType, uint64 u64IeeePartnerAddr)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy key type */
    *pu8TxBuffer++ = u8KeyType;
    u16TxLength += sizeof(uint8);
    /* Copy partner address */
    vSL_ConvU64ToBi(u64IeeePartnerAddr, pu8TxBuffer);
    u16TxLength += sizeof(uint64);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_REQUEST_KEY_REQ, u16TxLength, au8TxSerialBuffer, NULL);

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdpNodeDescRequest
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param uDstAddr ZPS_tuAddress
  * @param bExtAddr bool
  * @param pu8SeqNumber uint8 *
  * @param psZdpNodeDescReq ZPS_tsAplZdpNodeDescReq *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdpNodeDescRequest( void *pvApl,
                                                PDUM_thAPduInstance hAPduInst,
                                                ZPS_tuAddress uDstAddr,
                                                bool bExtAddr,
                                                uint8 *pu8SeqNumber,
                                                ZPS_tsAplZdpNodeDescReq *psZdpNodeDescReq)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy look up address */
    *pu8TxBuffer++ = (uint8)(psZdpNodeDescReq->u16NwkAddrOfInterest >> 8U);
    *pu8TxBuffer++ = (uint8)psZdpNodeDescReq->u16NwkAddrOfInterest;
    u16TxLength += sizeof(uint16);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_NODE_DESCRIPTOR_REQUEST, u16TxLength, au8TxSerialBuffer, pu8SeqNumber);

    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfSetEndpointDiscovery
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u8Endpoint uint8
  * @param u16ClusterId uint16
  * @param bOutput bool
  * @param bDiscoverable bool
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfSetEndpointDiscovery(
                    void                        *pvApl,
                    uint8                       u8Endpoint,
                    uint16                      u16ClusterId,
                    bool                        bOutput,
                    bool                        bDiscoverable)
{
    uint8 *pu8TxBuffer, u8Status;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8Endpoint */
    (void)ZBmemcpy(pu8TxBuffer, &u8Endpoint, sizeof(uint8));
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    /* Copy u16ClusterId */
    *pu8TxBuffer++ = (uint8)(u16ClusterId >> 8U);
    *pu8TxBuffer++ = (uint8)(u16ClusterId);
    u16TxLength += sizeof(uint16);

    /* Copy bOutput */
    (void)ZBmemcpy(pu8TxBuffer, &bOutput, sizeof(bool_t));
    pu8TxBuffer += sizeof(bool_t);
    u16TxLength += sizeof(bool_t);

    /* Copy bDiscoverable */
    (void)ZBmemcpy(pu8TxBuffer, &bDiscoverable, sizeof(bool_t));
    pu8TxBuffer += sizeof(bool_t);
    u16TxLength += sizeof(bool_t);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_CLUSTER_DISCOVERY_STATE, u16TxLength, au8TxSerialBuffer, NULL);

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint64 zps_eAplAibGetApsTrustCenterAddress
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint64 zps_eAplAibGetApsTrustCenterAddress(void *pvApl)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], u8Status;
    uint16 u16TxLength = 0x00U;
    uint64 u64IEEEAddr = 0ULL;

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_TC_ADDRESS,
                                 u16TxLength,
                                 au8TxSerialBuffer,
                                 &u64IEEEAddr);

    /* check status */
    if(u8Status == ZPS_E_SUCCESS)
    {
        return u64IEEEAddr;
    }

    return 0x00ULL;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdpMgmtPermitJoiningRequest
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param uDstAddr ZPS_tuAddress
  * @param bExtAddr bool
  * @param pu8SeqNumber uint8 *
  * @param psZdpMgmtPermitJoiningReq ZPS_tsAplZdpMgmtPermitJoiningReq *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdpMgmtPermitJoiningRequest(
                    void                        *pvApl,
                    PDUM_thAPduInstance         hAPduInst,
                    ZPS_tuAddress               uDstAddr,
                    bool                        bExtAddr,
                    uint8                       *pu8SeqNumber,
                    ZPS_tsAplZdpMgmtPermitJoiningReq *psZdpMgmtPermitJoiningReq)
{
    uint8 *pu8TxBuffer, u8Status;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;
    /* Copy DstAddr */
    *pu8TxBuffer++ = (uint8)(uDstAddr.u16Addr >> 8U);
    *pu8TxBuffer++ = (uint8)(uDstAddr.u16Addr);
    u16TxLength += sizeof(uint16);

    /* Copy duration */
    (void)ZBmemcpy(pu8TxBuffer, &psZdpMgmtPermitJoiningReq->u8PermitDuration, sizeof(uint8));
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);
    /* Copy bTcSignificance */
    (void)ZBmemcpy(pu8TxBuffer, &psZdpMgmtPermitJoiningReq->bTcSignificance, sizeof(uint8));
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);
    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_ZDO_PERMIT_JOIN_REQUEST, u16TxLength, au8TxSerialBuffer, NULL);
    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfGetSimpleDescriptor
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u8Endpoint uint8
  * @param psDesc ZPS_tsAplAfSimpleDescriptor *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfGetSimpleDescriptor(
                    void                        *pvApl,
                    uint8                       u8Endpoint,
                    ZPS_tsAplAfSimpleDescriptor *psDesc)
{
    uint8 *pu8TxBuffer, u8Status;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8Endpoint */
    (void)ZBmemcpy(pu8TxBuffer, &u8Endpoint, sizeof(uint8));
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_SIMPLE_DESCRIPTOR,
                                 u16TxLength,
                                 au8TxSerialBuffer,
                                 psDesc);

    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAfInterPanDataReq
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param u32ClId_ProfId uint32
  * @param psDstAddr ZPS_tsInterPanAddress *
  * @param u8Handle uint8
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAfInterPanDataReq(
                    void                        *pvApl,
                    PDUM_thAPduInstance         hAPduInst,
                    uint32                      u32ClId_ProfId,
                    ZPS_tsInterPanAddress       *psDstAddr,
                    uint8                       u8Handle)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, *pu8Payload, u8Status;
    uint16 u16Len = 0x00U;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy Cluster Id, profile id */
    vSL_ConvU32ToBi(u32ClId_ProfId, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint32);
    u16TxLength += sizeof(uint32);

    /* Copy Dst Addr mode */
    *pu8TxBuffer++ = (uint8)psDstAddr->eMode;
    u16TxLength += sizeof(uint8);

    /* Copy pan id */
    *pu8TxBuffer++ = (uint8)(psDstAddr->u16PanId >> 8U);
    *pu8TxBuffer++ = (uint8)psDstAddr->u16PanId;
    u16TxLength += sizeof(uint16);

    if(psDstAddr->eMode == ZPS_E_AM_INTERPAN_IEEE)
    {
        /* Copy IEEE Dst Addr */
        vSL_ConvU64ToBi(psDstAddr->uAddress.u64Addr, pu8TxBuffer);
        pu8TxBuffer += sizeof(uint64);
        u16TxLength += sizeof(uint64);
    }
    else
    {
        *pu8TxBuffer++ = (uint8)(psDstAddr->uAddress.u16Addr >> 8U);
        *pu8TxBuffer++ = (uint8)psDstAddr->uAddress.u16Addr;
        u16TxLength += sizeof(uint16);
    }

    /* Copy u8Handle mode */
    *pu8TxBuffer++ = (uint8)u8Handle;
    u16TxLength += sizeof(uint8);

    /* Copy payload size */
    u16Len = PDUM_u16APduInstanceGetPayloadSize(hAPduInst);
    *pu8TxBuffer++ = (uint8)(u16Len >> 8U);
    *pu8TxBuffer++ = (uint8)u16Len;
    u16TxLength += sizeof(uint16);

    /* Copy Payload */
    pu8Payload = (uint8 *)(PDUM_pvAPduInstanceGetPayload(hAPduInst));
    (void)ZBmemcpy(pu8TxBuffer, pu8Payload, (uint32)u16Len);
    u16TxLength += u16Len;

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_INTERPAN_DATA_REQ, u16TxLength, au8TxSerialBuffer, NULL);

    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdpSimpleDescRequest
  *
  */
 /**
  *
  * @param pvApl void *
  * @param hAPduInst PDUM_thAPduInstance
  * @param uDstAddr ZPS_tuAddress
  * @param bExtAddr bool
  * @param pu8SeqNumber uint8 *
  * @param psZdpSimpleDescReq ZPS_tsAplZdpSimpleDescReq *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdpSimpleDescRequest(void *pvApl,
                                                 PDUM_thAPduInstance hAPduInst,
                                                 ZPS_tuAddress uDstAddr,
                                                 bool bExtAddr,
                                                 uint8 *pu8SeqNumber,
                                                 ZPS_tsAplZdpSimpleDescReq *psZdpSimpleDescReq)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;
    bool_t bIsExtAddress;

    pu8TxBuffer = au8TxSerialBuffer;
    bIsExtAddress = (FALSE != bExtAddr) ? (bool_t)TRUE : (bool_t)FALSE;
    /* Copy bIsExtAddress */
    *pu8TxBuffer++ = u8FromBool(bIsExtAddress);
    u16TxLength += sizeof(uint8);

    /* Copy target short address */
    if(bIsExtAddress)
    {
        vSL_ConvU64ToBi(uDstAddr.u64Addr, pu8TxBuffer);
        pu8TxBuffer += sizeof(uint64);
        u16TxLength += sizeof(uint64);
    }
    else
    {
        *pu8TxBuffer++ = (uint8)(uDstAddr.u16Addr >> 8U);
        *pu8TxBuffer++ = (uint8)(uDstAddr.u16Addr);
        u16TxLength += sizeof(uint16);
    }

    /* Copy u16NwkAddrOfInterest */
    *pu8TxBuffer++ = (uint8)(psZdpSimpleDescReq->u16NwkAddrOfInterest >> 8U);
    *pu8TxBuffer++ = (uint8)psZdpSimpleDescReq->u16NwkAddrOfInterest;
    u16TxLength += sizeof(uint16);

    /* Copy u8EndPoint */
    *pu8TxBuffer++ = psZdpSimpleDescReq->u8EndPoint;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SIMPLE_DESCRIPTOR_REQUEST, u16TxLength, au8TxSerialBuffer, pu8SeqNumber);

    (void)PDUM_eAPduFreeAPduInstance(hAPduInst);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint32* zps_pu32AplAibGetApsChannelMask
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint32_t* zps_pu32AplAibGetApsChannelMask(void *pvApl, uint8_t *u8ChannelMaskCount)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    uint8 au8Mask[21];
    uint8 u8Index = 0;

    /* Send over serial */
    u8SL_WriteMessage((uint16)E_SL_MSG_GET_CHANNELMASK, u16TxLength, au8TxSerialBuffer, &au8Mask);

    /* Get number of channel mask */
    *u8ChannelMaskCount = au8Mask[u8Index++];

    if (*u8ChannelMaskCount > MAX_NUM_OF_CHANNEL_MASK) {

        DBG_vPrintf((bool_t)DEBUG_SL, "Exceeded Channel Mask count: rcvd %d max set to %d \r\n",
                *u8ChannelMaskCount, MAX_NUM_OF_CHANNEL_MASK);
        /* Limit received number of channel masks to 5 */
        *u8ChannelMaskCount = MAX_NUM_OF_CHANNEL_MASK;
    }

    /* Fill in provided masks */
    for (int i = 0; i < *u8ChannelMaskCount; i++) {
        u32ApsChannelMask[i++] = u32SL_ConvBiToU32(au8Mask+u8Index);
        u8Index += sizeof(uint32);
    }
    return (uint32*)&u32ApsChannelMask;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdoRemoveLinkKey
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u64IeeeAddr uint64
  *
  * @brief Remove link key
  *
  * @return Status of remove link key : ZPS_teStatus
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdoRemoveLinkKey(void *pvApl, uint64 u64IeeeAddr)
{
    uint8 u8Status;
    uint8 au8TxSerialBuffer[50], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u64IeeeAddr */
    vSL_ConvU64ToBi(u64IeeeAddr, pu8TxBuffer);
    u16TxLength += sizeof(uint64);
    u8Status =  u8SL_WriteMessage((uint16)E_SL_MSG_REMOVE_LINK_KEY, u16TxLength, au8TxSerialBuffer,NULL);

    /* Sanitize temporary variables */
    (void)ZBmemset(au8TxSerialBuffer, 0 , (uint32)u16TxLength);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdoRejoinNetwork
  *
  */
 /**
  *
  * @param pvNwk void *
  * @param bWithDiscovery bool_t
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdoRejoinNetwork(void *pvNwk, bool_t bWithDiscovery, bool_t bAllChannels, bool_t bTryNextParent)
{
    uint8 au8TxBuffer[3];
    au8TxBuffer[0] = (uint8)bWithDiscovery;
    au8TxBuffer[1] = (uint8)bAllChannels;

    return u8SL_WriteMessage((uint16)E_SL_MSG_REJOIN_NETWORK, 2U, au8TxBuffer,NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplZdoDiscoverNetworks
  *
  */
 /**
  *
  * @param pvApl void *
  * @param u32ChannelMask uint32
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplZdoDiscoverNetworks(void *pvApl, uint32 u32ChannelMask, bool_t bStateActive)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u32ChannelMask */
    vSL_ConvU32ToBi(u32ChannelMask, pu8TxBuffer);
    u16TxLength += sizeof(uint32);

    /* Send over serial */
    return u8SL_WriteMessage((uint16)E_SL_MSG_DISCOVER_NETWORKS, u16TxLength, au8TxSerialBuffer, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_u64AplAibGetApsUseExtendedPanId
  *
  */
 /**
  *
  * @param pvApl void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return uint64 The u64UseExtPanId element of the AIB
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint64 zps_u64AplAibGetApsUseExtendedPanId(void *pvApl)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], u8Status;
    uint16 u16TxLength = 0x00U;
    uint64 u64extPanId = 0ULL;

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_USE_EXT_PANID,
                                 u16TxLength,
                                 au8TxSerialBuffer,
                                 &u64extPanId);
    /* check status */
    if(u8Status == ZPS_E_SUCCESS)
    {
        return u64extPanId;
    }

    return 0x00ULL;
}

/********************************************************************************
  *
  * @fn PUBLIC bool zps_bAplAibGetApsUseInsecureJoin
  *
  */
 /**
  *
  * @param pvApl  Parameter Ignored. Intended to make API same as Stack API
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return bool TRUE is insecure join is used, FALSE otherwise
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC bool zps_bAplAibGetApsUseInsecureJoin(void *pvApl)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    bool_t bUseInsecureJoin = (bool_t)FALSE;

    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_USE_INSECURE_JOIN,
                            u16TxLength,
                            au8TxSerialBuffer,
                            &bUseInsecureJoin);
    return bUseInsecureJoin;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus zps_eAplAibSetApsUseInstallCode
  *
  */
 /**
  *
  * @param pvApl  Parameter Ignored. Intended to make API same as Stack API
  * @param bUseInstallCode bool
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC ZPS_teStatus zps_eAplAibSetApsUseInstallCode(void *pvApl, bool bUseInstallCode)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy bUseInstallCode */
    *pu8TxBuffer++ = u8FromBool((bool_t)bUseInstallCode);
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_USE_INSTALL_CODE,
                                u16TxLength,
                                au8TxSerialBuffer,
                                NULL);
    return u8Status;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 zps_u8AplZdoGetRadioChannel
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return PUBLIC uint8
  *
  * @note
  *
 ********************************************************************************/
PUBLIC uint8 zps_u8AplZdoGetRadioChannel (void *pvApl)
{
    uint8 u8Channel = 0U;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_CURRENT_CHANNEL, u16TxLength, au8TxSerialBuffer, &u8Channel);
    return u8Channel;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 zps_vDefaultStack
  *
  */
 /**
  * @param pvApl void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
 ********************************************************************************/
PUBLIC void zps_vDefaultStack (void *pvApl)
{
    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_ZPS_DEFAULT_STACK, 0U, NULL, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 zps_vSaveAllZpsRecords
  *
  */
 /**
  * @param pvApl void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
 ********************************************************************************/
PUBLIC void zps_vSaveAllZpsRecords (void *pvApl)
{
    /* Send over serial */
    vSL_SetLongResponsePeriod();
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_ZPS_SAVE_ALL_RECORDS, 0U, NULL, NULL);
    vSL_SetStandardResponsePeriod();
}

/********************************************************************************
  *
 ********************************************************************************/
void zps_vAplZdoDefaultServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoZdoClientInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoDeviceAnnceServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoActiveEpServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoNwkAddrServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoIeeeAddrServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoSystemServerDiscoveryServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoNodeDescServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoPowerDescServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoMatchDescServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoSimpleDescServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoMgmtLqiServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoMgmtLeaveServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoMgmtNWKUpdateServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoBindUnbindServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoBindRequestServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoMgmtBindServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoPermitJoiningServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoMgmtRtgServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplZdoParentAnnceServerInit()
{
    fprintf(stderr, "%s\n", __func__);
}
PUBLIC void zps_vRegisterCallbackForSecondsTick(tpfTimerExpiredCallBack pfTimerExpiredCallback)
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vHandleApsdeDataFragIndNotSupported()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplTrustCenterInit()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplTrustCenterUpdateDevice()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_vAplTrustCenterRequestKey()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoZdoClient()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoDeviceAnnceServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoActiveEpServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoNwkAddrServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoIeeeAddrServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoSystemServerDiscoveryServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoNodeDescServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoPowerDescServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoMatchDescServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoSimpleDescServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoMgmtLqiServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoMgmtLeaveServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoMgmtNWKUpdateServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoBindUnbindServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoBindRequestServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoMgmtBindServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoPermitJoiningServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoMgmtRtgServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoParentAnnceServer()
{
    fprintf(stderr, "%s\n", __func__);
}
void zps_bAplZdoDefaultServer()
{
    fprintf(stderr, "%s\n", __func__);
}
PUBLIC ZPS_teStatus zps_eAplAfInit (void* pvApl, bool_t bColdInit )
{
    fprintf(stderr,"%s\n", __func__);
    return 0;
}
PUBLIC ZPS_tsNwkNib *zps_psAplZdoGetNib(void *pvApl)
{
    fprintf(stderr,"%s\n", __func__);
    return NULL;
}
PUBLIC ZPS_teStatus zps_eAfVerifyKeyReqRsp ( void* pvApl,  uint64 u64DstAddr,  uint8 u8KeyType)
{
    fprintf(stderr,"%s\n", __func__);
    return 0;
}
PUBLIC ZPS_teStatus zps_eAplZdoJoinNetwork(void *pvApl, ZPS_tsNwkNetworkDescr *psNetworkDescr)
{
    uint8 au8TxSerialBuffer[40];
    uint8 *pu8TxBuffer;
    uint8 u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy psNetworkDescr->u64ExtPanId */
    vSL_ConvU64ToBi( psNetworkDescr->u64ExtPanId, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint64);
    u16TxLength += sizeof(uint64);

    /* Copy psNetworkDescr->u8LogicalChan */
    *pu8TxBuffer = psNetworkDescr->u8LogicalChan;
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    /* Copy psNetworkDescr->u8StackProfile */
    *pu8TxBuffer = psNetworkDescr->u8StackProfile;
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    /* Copy psNetworkDescr->u8ZigBeeVersion */
    *pu8TxBuffer = psNetworkDescr->u8ZigBeeVersion;
    pu8TxBuffer += sizeof(uint8);    
    u16TxLength += sizeof(uint8);

    /* Copy psNetworkDescr->u8PermitJoining */
    *pu8TxBuffer = psNetworkDescr->u8PermitJoining;
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    /* Copy psNetworkDescr->u8RouterCapacity */
    *pu8TxBuffer = psNetworkDescr->u8RouterCapacity;
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    /* Copy psNetworkDescr->u8EndDeviceCapacity */
    *pu8TxBuffer = psNetworkDescr->u8EndDeviceCapacity;
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

#ifdef WWAH_SUPPORT
    /* Copy psNetworkDescr->u8ParentCapacity */
    *pu8TxBuffer = psNetworkDescr->u8ParentCapacity;
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);
#endif
    vSL_SetLongResponsePeriod();
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_JOIN_NETWORK, u16TxLength, au8TxSerialBuffer, NULL);
    vSL_SetStandardResponsePeriod();
    return u8Status;
}

PUBLIC ZPS_tsAplApsKeyDescriptorEntry *zps_psFindKeyDescr(void *pvApl,  uint64 u64DeviceAddr,  uint32* pu32Index)
{
    uint8 au8TxSerialBuffer[40];
    uint8 *pu8TxBuffer;
    uint8 u8Status;
    uint8 u8Index = 0;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    uint8 au8FindKeyDescResp[27];

    /* Copy u64DeviceAddr */
    vSL_ConvU64ToBi(u64DeviceAddr, pu8TxBuffer);
    pu8TxBuffer += sizeof(uint64);
    u16TxLength += sizeof(uint64);

    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_FIND_KEY_DESCRIPTOR, u16TxLength, au8TxSerialBuffer, au8FindKeyDescResp);

    if (u8Status == ZPS_E_SUCCESS)
    {
        
        /* Copy sKeyDescriptorEntry.u32OutgoingFrameCounter */
        sKeyDescriptorEntry.u32OutgoingFrameCounter = u32SL_ConvBiToU32(&au8FindKeyDescResp[u8Index]);
        u8Index += sizeof(uint32);

        /* Copy sKeyDescriptorEntry.u16ExtAddrLkup */
        sKeyDescriptorEntry.u16ExtAddrLkup = ((uint32)au8FindKeyDescResp[u8Index++] << 8U);
        sKeyDescriptorEntry.u16ExtAddrLkup += (uint32)au8FindKeyDescResp[u8Index++];
        
        /*Copy sKeyDescriptorEntry.au8LinkKey */
        memcpy(sKeyDescriptorEntry.au8LinkKey, &au8FindKeyDescResp[u8Index], ZPS_SEC_KEY_LENGTH);
        u8Index += ZPS_SEC_KEY_LENGTH;

        /* sKeyDescriptorEntry.u8BitMapSecLevl */
        sKeyDescriptorEntry.u8BitMapSecLevl = au8FindKeyDescResp[u8Index];
        u8Index += sizeof(uint8);

        /* Copy u32Index */
        *pu32Index = u32SL_ConvBiToU32(&au8FindKeyDescResp[u8Index]);
        u8Index += sizeof(uint32);
        
        return &sKeyDescriptorEntry;
    }
    else
    {
        return NULL;
    }
}

PUBLIC ZPS_teStatus zps_eAplFormDistributedNetworkRouter(
    void *pvApl ,
    ZPS_tsAftsStartParamsDistributed *psStartParms,
    bool_t bSendDevice)
{
    uint8 au8TxSerialBuffer[40];
    uint8 *pu8TxBuffer;
    uint8 u8Status,i;
    uint16 u16TxLength = 0x00U;
    bool bRandomKey = (psStartParms->pu8NwkKey == NULL)? TRUE: FALSE;

    pu8TxBuffer = au8TxSerialBuffer;

    /* copy psStartParms->u64ExtPanId */
    vSL_ConvU64ToBi(psStartParms->u64ExtPanId, pu8TxBuffer);
    u16TxLength += sizeof(uint64);
    pu8TxBuffer += sizeof(uint64);

    /* if pu8NwkKey is not null, send key, else send 0 (random key) */ 
    if(bRandomKey == FALSE)
    {
        (void)ZBmemcpy(pu8TxBuffer, psStartParms->pu8NwkKey, ZPS_SEC_KEY_LENGTH);
    }
    else
    {
        (void)ZBmemset(pu8TxBuffer, 0x0U, ZPS_SEC_KEY_LENGTH);
    }
    u16TxLength += ZPS_SEC_KEY_LENGTH;
    pu8TxBuffer += ZPS_SEC_KEY_LENGTH;

    /* copy psStartParms->u16PanId */ 
    *pu8TxBuffer++ = (uint8)(psStartParms->u16PanId >> 8U);
    *pu8TxBuffer++ = (uint8)(psStartParms->u16PanId);
    u16TxLength += sizeof(uint16);

    /* copy psStartParms->u16NwkAddr */ 
    *pu8TxBuffer++ = (uint8)(psStartParms->u16NwkAddr >> 8U);
    *pu8TxBuffer++ = (uint8)(psStartParms->u16NwkAddr);
    u16TxLength += sizeof(uint16);

    /* copy psStartParms->u8KeyIndex */ 
    *pu8TxBuffer = psStartParms->u8KeyIndex;
    u16TxLength += sizeof(uint8);
    pu8TxBuffer += sizeof(uint8);

    /* copy psStartParms->u8LogicalChannel */ 
    *pu8TxBuffer = psStartParms->u8LogicalChannel;
    u16TxLength += sizeof(uint8);
    pu8TxBuffer += sizeof(uint8);

    /* copy psStartParms->u8NwkupdateId */ 
    *pu8TxBuffer = psStartParms->u8NwkupdateId;
    u16TxLength += sizeof(uint8);
    pu8TxBuffer += sizeof(uint8);

    /* copy bSendDevice */
    *pu8TxBuffer = bSendDevice;
    u16TxLength += sizeof(uint8);
    pu8TxBuffer += sizeof(uint8);

    /* Set Long Response Timeout Period */
    vSL_SetLongResponsePeriod();
    
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_FORM_DISTRIBUTED_NETWORK, u16TxLength, au8TxSerialBuffer, NULL);

    return u8Status;
}

PUBLIC ZPS_teStatus zps_eAplZdoBind(    void   *pvApl,
        uint32 u32ClusterIdEps,
        uint8  u8DstAddrMode,
        uint16 u16DstAddr,
        uint64 u64DstIeeeAddr
)
{
    //(u16ClusterId << 16) | (u8DstEndpoint << 8) | u8SrcEndpoint, ZPS_E_ADDR_MODE_IEEE, u16DstAddr, u64DstIeeeAddr
    return APP_eAplZdoBind((u32ClusterIdEps >> 16) & 0xFFFF,
                           u32ClusterIdEps & 0xFF,
                           u16DstAddr,
                           u64DstIeeeAddr,
                           (u32ClusterIdEps >> 8) & 0xFF,
                           u8DstAddrMode);
    return 0;
}
PUBLIC ZPS_teStatus zps_eAplZdoStartRouter(void *pvApl, bool_t bDeviceAnnounce)
{
    uint8 u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_START_ROUTER, (uint16)sizeof(bDeviceAnnounce), &bDeviceAnnounce, NULL);
    return u8Status;
}

uint64 zps_u64NwkLibFromPayload(
    uint8 *pu8Buffer)
{
    fprintf(stderr,"%s\n", __func__);
    return 0;
}

ZPS_teStatus zps_eAplZdoGroupAllEndpointRemove(void *pvApl, uint8 u8DstEndpoint)
{
    fprintf(stderr,"%s\n", __func__);
    return 0;
}

ZPS_teStatus zps_eAplZdoGroupEndpointRemove(void *pvApl, uint16 u16GroupAddr, uint8 u8DstEndpoint)
{
    fprintf(stderr,"%s\n", __func__);
    return 0;
}

ZPS_teStatus zps_eAplZdoGroupEndpointAdd(void *pvApl, uint16 u16GroupAddr, uint8 u8DstEndpoint)
{
    fprintf(stderr,"%s\n", __func__);
    return 0;
}
PUBLIC  ZPS_teStatus zps_eAplAfBoundDataReqNonBlocking( void                  *pvApl,
                                               PDUM_thAPduInstance    hAPduInst,
                                               uint32                 u32ClId_SrcEp,
                                               uint16                 u16SecMd_Radius,
                                               bool                   bAckReq )
{
    return zps_eAplAfBoundDataReq(pvApl,
                                  hAPduInst,
                                  u32ClId_SrcEp,
                                  u16SecMd_Radius,
                                  NULL);
}

PUBLIC ZPS_teZdoDeviceType zps_eAplZdoGetDeviceType(void *pvApl)
{
    return APP_eGetNodeDeviceType();
}
PUBLIC ZPS_tsAplAib *zps_psAplAibGetAib(void *pvApl)
{
    fprintf(stderr,"%s\n", __func__);
    return NULL;
}
PUBLIC uint8* zps_pu8AplZdoGetVsOUI(void *pvApl)
{
    fprintf(stderr, "%s\n", __func__);
    return NULL;
}
void zps_taskZPS()
{
}

/********************************************************************************
  *
  * @fn PUBLIC void ZPS_vNwkNibSetChannel
  *
  */
 /**
  *
  * @param pvNwk void *
  * @param u8Channel uint8
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void ZPS_vNwkNibSetChannel(void *pvNwk, uint8 u8Channel)
{
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SET_CHANNEL, (uint16)sizeof(u8Channel), &u8Channel,NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint64 ZPS_u64NwkNibGetEpid
  *
  */
 /**
  *
  * @param pvNwk void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint64 ZPS_u64NwkNibGetEpid(void *pvNwk)
{
    uint8 u8Status;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    uint64 u64ExtendedPanId = 0U;

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_EXT_PANID, u16TxLength, au8TxSerialBuffer, &u64ExtendedPanId);
    /* check status */
    if(u8Status == ZPS_E_SUCCESS)
    {
        return u64ExtendedPanId;
    }

    return 0x00ULL;
}

/********************************************************************************
  *
  * @fn PUBLIC uint64 ZPS_u64NwkNibGetPid
  *
  */
 /**
  *
  * @param pvNwk void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 ZPS_u16NwkNibGetPid(void *pvNwk)
{
    uint8 u8Status;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    uint16 u16PanId = 0x00U;

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_SHORT_PANID, u16TxLength, au8TxSerialBuffer, &u16PanId);
    /* check status */
    if(u8Status == ZPS_E_SUCCESS)
    {
        return u16PanId;
    }

    return 0x00U;
}

/********************************************************************************
  *
  * @fn PUBLIC bool_t ZPS_bNwkNibAddrMapAddEntry
  *
  */
 /**
  *
  * @param pvNwk void *
  * @param u16NwkAddr uint16
  * @param u64ExtAddr uint64
  * @param bCheckNeighborTable bool_t
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC bool_t ZPS_bNwkNibAddrMapAddEntry(void *pvNwk,
                                         uint16 u16NwkAddr,
                                         uint64 u64ExtAddr,
                                         bool_t bCheckNeighborTable)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy nwk address */
    *pu8TxBuffer++ = (uint8)(u16NwkAddr >> 8U);
    *pu8TxBuffer++ = (uint8)(u16NwkAddr);
    u16TxLength += sizeof(uint16);

    /* Copy u64ExtAddr */
    vSL_ConvU64ToBi(u64ExtAddr, pu8TxBuffer);
    u16TxLength += sizeof(uint64);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_ADD_ADDRESS_MAP_ENTRY, u16TxLength, au8TxSerialBuffer, NULL);

    return ((0U != u8Status) ? (bool_t)TRUE : (bool_t)FALSE);
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus ZPS_vNwkNibSetExtPanId
  *
  */
 /**
  *
  * @param u64ExtPanId  R 64 bit extended PAN ID
  *
  *
  * @brief Sends serial link command to set extended PAN ID
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void ZPS_vNwkNibSetExtPanId(void *pvNwk, uint64 u64ExtPanId)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u64ExtPanId */
    vSL_ConvU64ToBi(u64ExtPanId, pu8TxBuffer);
    u16TxLength += sizeof(uint64);

    /* Send over serial */
    (void) u8SL_WriteMessage((uint16)E_SL_MSG_CHANGE_EXT_PANID, u16TxLength, au8TxSerialBuffer, NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC uint16 ZPS_u16NwkNibFindNwkAddr
  *
  */
 /**
  *
  * @param pvNwk void *
  * @param u64ExtAddr uint64
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 ZPS_u16NwkNibFindNwkAddr(void *pvNwk, uint64 u64ExtAddr)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], u8Status,*pu8TxBuffer;
    uint16 u16TxLength = 0U;
    uint16 u16ShortAddr = 0xffffU;

    pu8TxBuffer = au8TxSerialBuffer;
    /* Copy MAC Addr */
    vSL_ConvU64ToBi(u64ExtAddr, pu8TxBuffer);
    u16TxLength += sizeof(uint64);
    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_SHORT_ADDRESS_OF_DEVICE, u16TxLength, au8TxSerialBuffer, &u16ShortAddr);
    /* check status */
    if(u8Status == ZPS_E_SUCCESS)
    {
        return u16ShortAddr;
    }
    return ZPS_NWK_INVALID_NWK_ADDR;
}

/********************************************************************************
  *
  * @fn PUBLIC bool ZPS_vNwkGetPermitJoiningStatus
  *
  */
 /**
  *
  * @param pvNwk  Parameter Ignored. Intended to make API same as Stack API
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return Whether permit join is enabled or not
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC bool_t ZPS_vNwkGetPermitJoiningStatus(void *pvNwk)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    bool_t bPermitJoin = (bool_t)FALSE;

    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_PERMIT_JOIN, u16TxLength, au8TxSerialBuffer, &bPermitJoin);
    return bPermitJoin;
}

/********************************************************************************
  *
  * @fn PUBLIC void* ZPS_pvNwkGetHandle
  *
  */
 /**
  *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void *ZPS_pvNwkGetHandle(void)
{
    return NULL;
}

/********************************************************************************
  *
  * @fn PUBLIC uint16 ZPS_u16NwkNibGetNwkAddr
  *
  */
 /**
  *
  * @param pvNwk void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 ZPS_u16NwkNibGetNwkAddr(void *pvNwk)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U, u16NWKAddr = 0xffffU;

    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_NWK_ADDR, u16TxLength, au8TxSerialBuffer, &u16NWKAddr);

    return u16NWKAddr;
}

/********************************************************************************
  *
  * @fn PUBLIC uint64 ZPS_u64NwkNibFindExtAddr
  *
  */
 /**
  *
  * @param pvNwk void *
  * @param u16NwkAddr uint16
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint64 ZPS_u64NwkNibFindExtAddr(void *pvNwk, uint16 u16NwkAddr)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;
    uint64 u64Addr = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy NWK Addr */
    *pu8TxBuffer++ = (uint8)(u16NwkAddr >> 8U);
    *pu8TxBuffer++ = (uint8)(u16NwkAddr);
    u16TxLength += sizeof(uint16);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_FIND_EXT_ADDR, u16TxLength, au8TxSerialBuffer, &u64Addr);

    /* check status */
    if(u8Status == ZPS_E_SUCCESS)
    {
        return u64Addr;
    }

    return 0x00UL;
}

/********************************************************************************
  *
  * @fn PUBLIC ZPS_teStatus eSL_SearchExtendedPanId
  *
  */
 /**
  *
  * @param u64ExtPanId  R 64 bit extended PAN ID
  * @param u16PanId  R 16 bit PAN ID
  *
  *
  * @brief Sends serial link command to search for Ext PANID and PANID
  *
  * @return bool TRUE if found, FALSE otherwise
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC bool eSL_SearchExtendedPanId(uint64 u64ExtPanId, uint16 u16PanId)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;
    bool bRetVal = TRUE;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u64ExtPanId */
    vSL_ConvU64ToBi(u64ExtPanId, pu8TxBuffer);
    u16TxLength += sizeof(uint64);

    /* Copy u16PanId */
    pu8TxBuffer[u16TxLength++] = (u16PanId >> 8) & 0xFF;
    pu8TxBuffer[u16TxLength++] = (uint8)u16PanId;

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SEARCH_EXT_PANID, u16TxLength, au8TxSerialBuffer, &bRetVal);
    return bRetVal;
}

PUBLIC ZPS_tsAplApsKeyDescriptorEntry** ZPS_psAplDefaultDistributedAPSLinkKey(void)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], u8Status;
    ZPS_tsAplApsKeyDescriptorEntry key;
    uint16 u16TxLength = 0x00U;

    u8SL_WriteMessage((uint16)E_SL_MSG_GET_DEFAULT_DISTRIBUTED_APS_LINK_KEY,
                       u16TxLength,
                       au8TxSerialBuffer,
                       &key);

    psAplDefaultDistributedAPSLinkKey->u32OutgoingFrameCounter = key.u32OutgoingFrameCounter;
    psAplDefaultDistributedAPSLinkKey->u16ExtAddrLkup = key.u16ExtAddrLkup;
    psAplDefaultDistributedAPSLinkKey->u8BitMapSecLevl = key.u8BitMapSecLevl;

    (void)ZBmemcpy(&psAplDefaultDistributedAPSLinkKey->au8LinkKey,
                &key.au8LinkKey,
                ZPS_SEC_KEY_LENGTH);

    return &psAplDefaultDistributedAPSLinkKey;
}

PUBLIC ZPS_tsAplApsKeyDescriptorEntry** ZPS_psAplDefaultGlobalAPSLinkKey(void)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], u8Status;
    ZPS_tsAplApsKeyDescriptorEntry key;
    uint16 u16TxLength = 0x00U;

    u8SL_WriteMessage((uint16)E_SL_MSG_GET_DEFAULT_GLOBAL_APS_LINK_KEY,
                        u16TxLength,
                        au8TxSerialBuffer,
                        &key);

    psAplDefaultGlobalAPSLinkKey->u32OutgoingFrameCounter = key.u32OutgoingFrameCounter;
    psAplDefaultGlobalAPSLinkKey->u16ExtAddrLkup = key.u16ExtAddrLkup;
    psAplDefaultGlobalAPSLinkKey->u8BitMapSecLevl = key.u8BitMapSecLevl;

    (void)ZBmemcpy(&psAplDefaultGlobalAPSLinkKey->au8LinkKey,
                &key.au8LinkKey,
                ZPS_SEC_KEY_LENGTH);

    return &psAplDefaultGlobalAPSLinkKey;
}

PUBLIC ZPS_tsAplApsKeyDescriptorEntry** zps_psAplDefaultTrustCenterAPSLinkKey(void *pvApl)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], u8Status;
    ZPS_tsAplApsKeyDescriptorEntry key;
    uint16 u16TxLength = 0x00U;

    u8SL_WriteMessage((uint16)E_SL_MSG_GET_DEFAULT_TC_APS_LINK_KEY,
                        u16TxLength,
                        au8TxSerialBuffer,
                        &key);

    psAplDefaultTCAPSLinkKey->u32OutgoingFrameCounter = key.u32OutgoingFrameCounter;
    psAplDefaultTCAPSLinkKey->u16ExtAddrLkup = key.u16ExtAddrLkup;
    psAplDefaultTCAPSLinkKey->u8BitMapSecLevl = key.u8BitMapSecLevl;

    (void)ZBmemcpy(&psAplDefaultTCAPSLinkKey->au8LinkKey,
                &key.au8LinkKey,
                ZPS_SEC_KEY_LENGTH);

    return &psAplDefaultTCAPSLinkKey;

}

/********************************************************************************
  *
  * @fn PUBLIC uint8 ZPS_u8ApsGetSeqNum
  *
  */
 /**
  *
  * @param pvApl void *
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return TRUE if a complete valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 ZPS_u8ApsGetSeqNum(void *pvApl)
{
    uint8 u8Status;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    uint8 u8ApsNextSeqNum = 0U;

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_APS_SEQ_NUM, u16TxLength, au8TxSerialBuffer, &u8ApsNextSeqNum);
    if(u8Status == ZPS_E_SUCCESS)
    {
        return u8ApsNextSeqNum;
    }
    return 0;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 ZPS_vSetKeys
  *
  */
 /**
  *
  * @brief Attempt to read a complete message from the serial link
  *
  * @return None
  *
  * @note
  *
 ********************************************************************************/
PUBLIC void ZPS_vSetKeys (void)
{
    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_ZPS_SET_KEYS, 0U, NULL, NULL);
}

PUBLIC bool_t ZPS_bAplDoesDeviceSupportFragmentation(void *pvApl)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    bool bFragSup = FALSE;
   
    /* Send over serial */
    u8SL_WriteMessage((uint16)E_SL_MSG_GET_FRAGMENTATION_SUPPORT, u16TxLength, au8TxSerialBuffer, &bFragSup);
  
    return bFragSup;
}

PUBLIC uint8 ZPS_u8AplGetMaxPayloadSize(void *pvApl , uint16 u16Addr)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    uint8 u8MaxPayloadSize;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16Addr */
    (void)ZBmemcpy(pu8TxBuffer, &u16Addr, sizeof(uint16));
    pu8TxBuffer += sizeof(uint16);
    u16TxLength += sizeof(uint16);

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_GET_MAX_PAYLOAD_SIZE, u16TxLength, au8TxSerialBuffer, &u8MaxPayloadSize);

    return u8MaxPayloadSize;
}
PUBLIC uint8 ZPS_u8NwkManagerState(void)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    uint8 u8NwkState = 0x0U;
   
    /* Send over serial */
    u8SL_WriteMessage((uint16)E_SL_MSG_NWK_MANAGER_STATE, u16TxLength, au8TxSerialBuffer, &u8NwkState);
  
    return u8NwkState;
}
PUBLIC void ZPS_vNwkNibClearDiscoveryNT(void *pvNwk)
{
    /* Send over serial */
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_NWK_CLEAR_DISC_NT, 0U, NULL, NULL);
}
PUBLIC  ZPS_tsNwkNetworkDescr* ZPS_psGetNetworkDescriptors (uint8 *pu8NumberOfNetworks)
{
    /* Get number of nwk descriptors */
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    uint8 i = 0;
   
    u8SL_WriteMessage((uint16)E_SL_MSG_GET_NUMBER_OF_NWK_DESCRIPTORS, u16TxLength, au8TxSerialBuffer, &u8NumberOfNetworks);
    *pu8NumberOfNetworks = u8NumberOfNetworks;

    /* Build nwk descriptor array */
    for(i = 0; i < u8NumberOfNetworks; i++)
    {
        ZPS_tsNwkNetworkDescr tsCurrDesc;
        u8SL_WriteMessage((uint16)E_SL_MSG_GET_NWK_DESCRIPTOR, u16TxLength, au8TxSerialBuffer, &tsCurrDesc);
        *(psNwkDescriptors + i) = tsCurrDesc;
    }

    return psNwkDescriptors;
}

PUBLIC void ZPS_vTCSetCallback(void *pfTcFunc)
{
    fprintf(stderr,"%s\n", __func__);
}

PUBLIC void ZPS_vSetTCLockDownOverride (void* pvApl, bool_t u8RemoteOverride, bool_t bDisableAuthentications )
{
 uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8RemoteOverride */
    (void)ZBmemcpy(pu8TxBuffer, &u8RemoteOverride, sizeof(uint8));
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    /* Copy bDisableAuthentications */
    (void)ZBmemcpy(pu8TxBuffer, &bDisableAuthentications, sizeof(uint8));
    pu8TxBuffer += sizeof(uint8);
    u16TxLength += sizeof(uint8);

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SET_TC_LOCKDOWN_OVERRIDE, u16TxLength, au8TxSerialBuffer, NULL);
}

PUBLIC ZPS_tsNwkNib *ZPS_psNwkNibGetHandle(void *pvNwk)
{
    fprintf(stderr,"%s\n", __func__);
    return NULL;
}
bool ZPS_GetApsUseExtendedPanid(void *pvApl)
{
    fprintf(stderr,"%s\n", __func__);
    return 0;
}
PUBLIC void *ZPS_pvAplZdoGetAplHandle(void)
{
    //fprintf(stderr, "%s\n", __func__);
    return NULL;
}
PUBLIC void ZPS_vSetExtendedStatus(ZPS_teExtendedStatus eExtendedStatus)
{
	 fprintf(stderr, "%s\n", __func__);
}
void ZPS_vSecondTimerCallback()
{
    fprintf(stderr, "%s\n", __func__);
}

PUBLIC bool_t ZPS_bIsCoprocessorNewModule(void)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    bool bCoproFactoryNew = FALSE;
   
    /* Send over serial */
    u8SL_WriteMessage((uint16)E_SL_MSG_IS_COPROCESSOR_NEW_MODULE, u16TxLength, au8TxSerialBuffer, &bCoproFactoryNew);
  
    return bCoproFactoryNew;
}

#if (BDB_SET_DEFAULT_TC_POLICY == TRUE)

/****************************************************************************
 *
 * NAME: eGetTrspKeyDeciderPolicy
 *
 * DESCRIPTION:
 *  Return the policy of the transport key decider table.
 *
 * PARAMETERS:      Name            RW      Usage
 * 
 * RETURNS:
 *  BDB_eTrspKeyDeciderPolicy
 *
 ****************************************************************************/
PUBLIC BDB_eTrspKeyDeciderPolicy eGetTrspKeyDeciderPolicy()
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    BDB_eTrspKeyDeciderPolicy eTrspKeyDeciderPolicy = 0x0U;
   
    /* Send over serial */
    u8SL_WriteMessage((uint16)E_SL_MSG_GET_TRSP_KEY_DECIDER_TABLE_POLICY, u16TxLength, au8TxSerialBuffer, &eTrspKeyDeciderPolicy);
   
    return eTrspKeyDeciderPolicy;
}

/****************************************************************************
 *
 * NAME: vSetTrspKeyDeciderPolicy
 *
 * DESCRIPTION:
 *  Sets the policy of the transport key decider table to the given policy
 *  Policy modes are ignore, whitelist and blacklist. 
 *  Fails if given value is not a valid policy
 *
 * PARAMETERS:      Name            RW      Usage
 * 
 * RETURNS:
 *  BDB_teStatus
 *
 ****************************************************************************/
PUBLIC BDB_teStatus vSetTrspKeyDeciderPolicy(BDB_eTrspKeyDeciderPolicy eTrspKeyDeciderPolicy)
{
    uint8 u8Status;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;

    /* Copy policy */
    au8TxSerialBuffer[0] = eTrspKeyDeciderPolicy;
    u16TxLength += sizeof(uint8);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_SET_TRSP_KEY_DECIDER_TABLE_POLICY, u16TxLength, au8TxSerialBuffer, NULL);

    return (BDB_teStatus)u8Status;
}

/****************************************************************************
 *
 * NAME: u8GetTrspKeyDeciderTableSize
 *
 * DESCRIPTION:
 *  Returns the current number of occupied transport key decider table entries. 
 *
 * PARAMETERS:      Name            RW      Usage
 * 
 * RETURNS:
 *  uint8
 *
 ****************************************************************************/
PUBLIC uint8 u8GetTrspKeyDeciderTableMaxSize()
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE];
    uint16 u16TxLength = 0x00U;
    uint8 u8TkdSize;
   
    /* Send over serial */
    u8SL_WriteMessage((uint16)E_SL_MSG_GET_TRSP_KEY_DECIDER_TABLE_MAX_SIZE, u16TxLength, au8TxSerialBuffer, &u8TkdSize);
  
    return u8TkdSize;
}

/****************************************************************************
 *
 * NAME: vGetTrspKeyDeciderEntry
 *
 * DESCRIPTION:
 * Gets entry of the transport key decider table for the given index.
 * Fails if given index is out of bounds.
 * PARAMETERS:      Name                    RW      Usage
 *                  s_TrspKeyDeciderEntry           Entry to remove
 * RETURNS:
 *  BDB_teStatus
 *
 ****************************************************************************/
PUBLIC BDB_teStatus tsGetTrspKeyDeciderEntry(uint8 u8Index, BDB_tsTrspKeyDeciderEntry* psTrspKeyDeciderEntry)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer, u8Status;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy index */
    au8TxSerialBuffer[0] = u8Index;
    u16TxLength += sizeof(uint8);

    (void)ZBmemset(psTrspKeyDeciderEntry, 0x00, sizeof(BDB_tsTrspKeyDeciderEntry));

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_GET_TRSP_KEY_DECIDER_TABLE_ENTRY,
                                 u16TxLength,
                                 au8TxSerialBuffer,
                                 psTrspKeyDeciderEntry);

    return (BDB_teStatus)u8Status;
}

/****************************************************************************
 *
 * NAME: vAddTrspKeyDeciderEntry
 *
 * DESCRIPTION:
 *  Adds given entry to transport key decider table if not already present. 
 *  Fails if table full.
 *
 * PARAMETERS:      Name                    RW      Usage
 *                  s_TrspKeyDeciderEntry           Entry to add
 * RETURNS:
 *  BDB_teStatus
 *
 ****************************************************************************/
PUBLIC BDB_teStatus vAddTrspKeyDeciderEntry(BDB_tsTrspKeyDeciderEntry s_TrspKeyDeciderEntry)
{
    uint8 u8Status;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Serialize u64IEEEAddress */
    vSL_ConvU64ToBi((uint64)s_TrspKeyDeciderEntry, pu8TxBuffer);
    u16TxLength += sizeof(uint64);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_ADD_TRSP_KEY_DECIDER_TABLE_ENTRY,
                                 u16TxLength,
                                 au8TxSerialBuffer,
                                 NULL);

    return (BDB_teStatus)u8Status;
}

/****************************************************************************
 *
 * NAME: vRemoveTrspKeyDeciderEntry
 *
 * DESCRIPTION:
 *  Removes given entry from transport key decider table if not already missing. 
 *  Fails if table empty.
 *
 * PARAMETERS:      Name                    RW      Usage
 *                  s_TrspKeyDeciderEntry           Entry to remove
 * RETURNS:
 *  BDB_teStatus
 *
 ****************************************************************************/
PUBLIC BDB_teStatus vRemoveTrspKeyDeciderEntry(BDB_tsTrspKeyDeciderEntry s_TrspKeyDeciderEntry)
{
    uint8 u8Status;
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Serialize u64IEEEAddress */
    vSL_ConvU64ToBi((uint64)s_TrspKeyDeciderEntry, pu8TxBuffer);
    u16TxLength += sizeof(uint64);

    /* Send over serial */
    u8Status = u8SL_WriteMessage((uint16)E_SL_MSG_REMOVE_TRSP_KEY_DECIDER_TABLE_ENTRY,
                                 u16TxLength,
                                 au8TxSerialBuffer,
                                 NULL);

    return (BDB_teStatus)u8Status;
}

/****************************************************************************
 *
 * NAME: vClearTrspKeyDeciderTable
 *
 * DESCRIPTION:
 *  Zeroes out the transport key decider table.
 *
 * PARAMETERS:      Name            RW  Usage
 * 
 * RETURNS:
 *  void
 *
 ****************************************************************************/
PUBLIC void vClearTrspKeyDeciderTable()
{
    /* Send over serial */
    u8SL_WriteMessage((uint16)E_SL_MSG_CLEAR_TRSP_KEY_DECIDER_TABLE, 0U, NULL, NULL);
}

#endif

/****************************************************************************
 *
 * NAME: zps_bAplAibFindBindTableEntryForClusterId
 *
 * DESCRIPTION:
 *  Searches for bind table entry corresponding to given cluster ID. Returns 
 *  true if found, false otherwise.
 *
 * PARAMETERS:      Name                    RW      Usage
 *                  pvApl                         APL handle
 *                  u16ClusterId                  cluster ID to search for
 * RETURNS:
 *  bool
 *
 *****************************************************************************/
PUBLIC bool zps_bAplAibFindBindTableEntryForClusterId( void *pvApl, uint16 u16ClusterId )
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    bool bEntryFound = FALSE;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u16ClusterId */
    *pu8TxBuffer++ = (uint8)(u16ClusterId >> 8U);
    *pu8TxBuffer++ = (uint8)(u16ClusterId);
    u16TxLength += sizeof(uint16);

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_FIND_BIND_ENTRY_FOR_CLUSTER_ID, u16TxLength, au8TxSerialBuffer, &bEntryFound);

    return bEntryFound;
}

PUBLIC void zps_vSetIgnoreProfileCheck(void)
{
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_SET_IGNORE_PROFILE_CHECK, 0U, NULL, NULL);
}

PUBLIC void ZPS_vAplZdoRegisterInterPanFilter(void* fnPtr)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;

    uint8 u8Endpoint = ROUTER_ONOFFLIGHT_ENDPOINT;
    uint16 u16ProfileId = ZLL_PROFILE_ID;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8Endpoint */
    *pu8TxBuffer++ = u8Endpoint;
    u16TxLength += sizeof(uint8);

    /* Copy u16ProfileId */
    *pu8TxBuffer++ = (uint8)(u16ProfileId >> 8U);
    *pu8TxBuffer++ = (uint8)(u16ProfileId);
    u16TxLength += sizeof(uint16);

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_REGISTER_INTERPAN_FILTER, u16TxLength, au8TxSerialBuffer, NULL);
}

PUBLIC void ZPS_vNwkNibClearTables (void *pvNwk)
{
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_NWK_NIB_CLEAR_TABLES, 0U, NULL, NULL);
}

PUBLIC void zps_vNwkSecClearMatSet(void *psNwk)
{
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_NWK_CLEAR_MAT_SET, 0U, NULL, NULL);
}

PUBLIC uint32 ZPS_u32MacSetTxBuffers (uint8 u8MaxTxBuffers)
{
    uint8 au8TxSerialBuffer[MAX_TX_SERIAL_BUFFER_SIZE], *pu8TxBuffer;
    uint16 u16TxLength = 0x00U;
    uint32 u32Ret = 0;

    pu8TxBuffer = au8TxSerialBuffer;

    /* Copy u8MaxTxBuffers */
    *pu8TxBuffer++ = u8MaxTxBuffers;
    u16TxLength += sizeof(uint8);

    (void)u8SL_WriteMessage((uint16)E_SL_MSG_MAC_SET_TX_BUFFERS, u16TxLength, au8TxSerialBuffer, &u32Ret);

    return u32Ret;
}
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
