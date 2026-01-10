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

// enable prints for getting and freeing serial rx buffers
#ifndef TRACE_GET_FREE
#define TRACE_GET_FREE       FALSE
#endif

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include "jendefs.h"
#include <dbg.h>
#include "app_common.h"
#include "app_common_ncp.h"
#include "serial_link_ctrl.h"
#include "zps_apl.h"
#include "zps_apl_aib.h"
#include "zps_nwk_nib.h"
#include "zps_apl_aib.h"
#include "dbg.h"
#include "app_uart.h"
#include "bdb_api.h"
#include "bdb_tkd.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define SL_BLOCK_SIZE     128U


/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct {
    uint16 u16ApplicationProfileId;
    uint16 u16DeviceId;
    uint8  u8DeviceVersion;
    uint8  u8Endpoint;
    uint8  u8InClusterCount;
    uint8  u8OutClusterCount;
    uint16 *pu16InClusterList;
    uint16 *pu16OutClusterList;
    uint8 *au8InDiscoveryEnabledFlags;
    uint8 *au8OutDiscoveryEnabledFlags;
} ZPS_tsAppSimpleDescriptor;

/* define the maximum no of clusters for the get simple descriptor request */
#define MAX_SD_IN_CL_COUNT      20U
#define MAX_SD_OUT_CL_COUNT     20U
/* for every 8 Clusters we need 1 byte of flags storage */
#define MAX_SD_IN_FLAG_COUNT     ((MAX_SD_IN_CL_COUNT / 8U) + 1U)
#define MAX_SD_OUT_FLAG_COUNT    ((MAX_SD_OUT_CL_COUNT / 8U) + 1U)

/*
 * This is the minimum measured time after which the JN should
 * be up.
 */
#define WAIT_JN_READY_SLEEP_TIME_MS     178u
/*
 * Don't set this either too low or too high, you'll either just hammer
 * the JN needlessly or delay the startup of the LPC too much.
 */
#define WAIT_JN_READY_POLL_TIME_MS      30u
/****************************************************************************/
/***        Private variable declaration                                     ***/
/****************************************************************************/
PRIVATE uint16 u16LargeBufferAllocated = 0U;
PRIVATE uint16 u16SmallBufferAllocated = 0U;
PRIVATE uint16 u16MaxLargeBufferAllocated = 0U;
PRIVATE uint16 u16MaxSmallBufferAllocated = 0U;

/* flag to prevent repeated printing if all buffers exhausted */
/* cleared if pool reset or buffer allocation succeeds */
PRIVATE bool_t bPrinted = (bool_t)FALSE;

/* transmit sequence number for this side */
PRIVATE uint8 u8SLTxSeqNum = 0U;
/* the last transmit sequence received from the other side (ie their tx sequence number) */
PRIVATE uint8 u8SLRxSeqNum = 0U;
PRIVATE tsLPCSerialStats sLpcSerialStats;
/* the last seq num the Jn reported hearing */

PRIVATE uint8 u8JNRxSeqNum = 0U;

#ifdef ENABLE_UART_ACK_FROM_HOST
uint8 u8AckTestMode = 0U;
#endif

#ifdef RESET_UART
    #define FUNC_RESET(param) restartStateMachineSLCommon(TRUE)
#else
    #define FUNC_RESET(param) APP_vVerifyFatal((bool_t)param, "SERIAL RX: E_SL_MSG_STATUS_TIME_OUT", ERR_FATAL_ZIGBEE_RESTART);
#endif

PRIVATE uint32 u32GetStatusFlagsFromSend(void);

PRIVATE uint32 u32GetJNInfoSafe(uint16 u16Type);

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
PRIVATE uint8 u8SL_CalculateCRC(
                    uint16                         u16Type,
                    uint16                         u16Length,
                    uint8                         u8TxSeq,
                    uint8                         u8RxSeq,
                    uint8                         *pu8Data);
PRIVATE uint8 *pu8SL_GetRxBufferFromLargePool(void);
PRIVATE uint8 *pu8SL_GetRxBufferFromSmallPool(void);
#ifdef SL_TRANSMIT_HOOK
PUBLIC bool_t bSL_TransmitHook(
                    uint16                         u16Type,
                    uint16                         u16Length,
                    uint8                         *pu8Data,
                    void                         *pvData);
#endif // SL_TRANSMIT_HOOK

#ifdef SL_JN_RESET_HOOK
PUBLIC void vSL_JNResetHook(void);
#endif // SL_JN_RESET_HOOK

#ifdef SL_HOST_TO_COPROCESSOR_SECURE
PUBLIC void vSL_SecureTransmitLength(uint16 u16Length);
PUBLIC void vSL_SecureTransmitMessageAuth(uint16 u16Length, uint8 *pu8Data);
PUBLIC void vSL_SecureEncryptMessage(uint16 u16Length, uint8 *pu8Data);
PUBLIC bool_t bSL_SecureDecryptMessage(uint8 *pu8RxSerialBuffer, uint16 *pu16Length);
WEAK bool_t SEC_bDecryptBlock(uint8 *pu8Data, uint16 u16Length, uint8 *pu8Mic);
WEAK void SEC_vEncryptBlock(uint8 *pu8Data, uint16 u16Length, uint8 *pu8Mic);
#endif

PRIVATE void vSL_WakeJN( uint16 u16Length, bool_t bJNInDeepSleep, uint16 u16Type );

PRIVATE void vHandleRxBufferExhausted( void);
PRIVATE bool_t bIsDuplicate(uint16 u16Type, uint8 u8SeqNo);
PRIVATE void vSl_LogJnerrors(uint32 u32Flags);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/
#ifdef __IAR_SYSTEMS_ICC__
PUBLIC tsSL_Common sSLCommon = {0};
#else
PUBLIC tsSL_Common sSLCommon= {(bool_t)FALSE,(bool_t)FALSE, NULL};
#endif
static teSL_RxState eRxState = (teSL_RxState)E_STATE_RX_WAIT_START;

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
uint16 au16ApiErrors[E_SL_API_ERROR_COUNT_SIZE];
uint16 au16ApiErrorTotals[E_SL_API_ERROR_COUNT_SIZE];

tsErrorLog  asErrorLog[SIZE_JN_ERRORS] = {
        { (uint8)E_SL_MSG_STATUS_INCORRECT_PARAMETERS,  0, 0},
        { (uint8)E_SL_MSG_STATUS_UNHANDLED_COMMAND,     0, 0},
        { (uint8)E_SL_MSG_STATUS_BUSY,                  0, 0},
        { (uint8)E_SL_MSG_STATUS_STACK_ALREADY_STARTED, 0, 0},
        { (uint8)E_SL_MSG_STATUS_TIME_OUT,              0, 0},
        { (uint8)E_SL_MSG_NO_APDU_BUFFERS,              0, 0},
        { (uint8)E_SL_MSG_STATUS_UNSUPPORTED_COMMAND,   0, 0},
        { (uint8)E_SL_MSG_STATUS_PDM_BAD_PARAMS,        0, 0},
        { (uint8)E_SL_MSG_STATUS_PDM_ALREADY_DONE,      0, 0},
        { (uint8)E_SL_MSG_STATUS_PDM_UNKNOWN_VERSION,   0, 0},
        { (uint8)E_SL_MSG_STATUS_HARDWARE_FAILURE,      0, 0},
        { (uint8)E_SL_MSG_STATUS_INVALID_PARAMETER,     0, 0},
        { (uint8)E_SL_MSG_STATUS_ILLEGAL_REQUEST,       0, 0},
        { (uint8)E_SL_MSG_STATUS_C2_SUBSTITUTION,       0, 0},
        { (uint8)JN_ERROR_APDU_TOO_SMALL,               0, 0},
        { (uint8)JN_ERROR_APDU_INSTANCES_EXHAUSTED,     0, 0},
        { (uint8)JN_ERROR_NO_APDU_CONFIGURED,           0, 0},
        { (uint8)JN_ERROR_OS_MESSAGE_QUEUE_OVERRUN,     0, 0},
        { (uint8)JN_ERROR_APS_SECURITY_FAIL,            0, 0},
        { (uint8)JN_ERROR_ZDO_LINKSTATUS_FAIL,          0, 0},
        { (uint8)ZPS_APL_ZDP_E_INV_REQUESTTYPE,         0, 0},
        { (uint8)ZPS_APL_ZDP_E_DEVICE_NOT_FOUND,        0, 0},
        { (uint8)ZPS_APL_ZDP_E_NOT_ACTIVE,              0, 0},
        { (uint8)ZPS_APL_ZDP_E_NOT_SUPPORTED,           0, 0},
        { (uint8)ZPS_APL_ZDP_E_TIMEOUT,                 0, 0},
        { (uint8)ZPS_APL_ZDP_E_NO_MATCH,                0, 0},
        { (uint8)ZPS_APL_ZDP_E_NO_ENTRY,                0, 0},
        { (uint8)ZPS_APL_ZDP_E_NO_DESCRIPTOR,           0, 0},
        { (uint8)ZPS_APL_ZDP_E_INSUFFICIENT_SPACE,      0, 0},
        { (uint8)ZPS_APL_ZDP_E_NOT_PERMITTED,           0, 0},
        { (uint8)ZPS_APL_ZDP_E_TABLE_FULL,              0, 0},
        { (uint8)ZPS_APL_ZDP_E_INVALID_INDEX,           0, 0},
        { (uint8)ZPS_APL_APS_E_ASDU_TOO_LONG,           0, 0},
        { (uint8)ZPS_APL_APS_E_DEFRAG_DEFERRED,         0, 0},
        { (uint8)ZPS_APL_APS_E_DEFRAG_UNSUPPORTED,      0, 0},
        { (uint8)ZPS_APL_APS_E_ILLEGAL_REQUEST,         0, 0},
        { (uint8)ZPS_APL_APS_E_INVALID_BINDING,         0, 0},
        { (uint8)ZPS_APL_APS_E_INVALID_GROUP,           0, 0},
        { (uint8)ZPS_APL_APS_E_INVALID_PARAMETER,       0, 0},
        { (uint8)ZPS_APL_APS_E_NO_ACK,                  0, 0},
        { (uint8)ZPS_APL_APS_E_NO_BOUND_DEVICE,         0, 0},
        { (uint8)ZPS_APL_APS_E_NO_SHORT_ADDRESS,        0, 0},
        { (uint8)ZPS_APL_APS_E_NOT_SUPPORTED,           0, 0},
        { (uint8)ZPS_APL_APS_E_SECURED_LINK_KEY,        0, 0},
        { (uint8)ZPS_APL_APS_E_SECURED_NWK_KEY,         0, 0},
        { (uint8)ZPS_APL_APS_E_SECURITY_FAIL,           0, 0},
        { (uint8)ZPS_APL_APS_E_TABLE_FULL,              0, 0},
        { (uint8)ZPS_APL_APS_E_UNSECURED,               0, 0},
        { (uint8)ZPS_APL_APS_E_UNSUPPORTED_ATTRIBUTE,   0, 0},
        { (uint8)ZPS_NWK_ENUM_INVALID_PARAMETER,        0, 0},
        { (uint8)ZPS_NWK_ENUM_INVALID_REQUEST,          0, 0},
        { (uint8)ZPS_NWK_ENUM_NOT_PERMITTED,            0, 0},
        { (uint8)ZPS_NWK_ENUM_STARTUP_FAILURE,          0, 0},
        { (uint8)ZPS_NWK_ENUM_ALREADY_PRESENT,          0, 0},
        { (uint8)ZPS_NWK_ENUM_NEIGHBOR_TABLE_FULL,      0, 0},
        { (uint8)ZPS_NWK_ENUM_UNKNOWN_DEVICE,           0, 0},
        { (uint8)ZPS_NWK_ENUM_UNSUPPORTED_ATTRIBUTE,    0, 0},
        { (uint8)ZPS_NWK_ENUM_NO_NETWORKS,              0, 0},
        { (uint8)ZPS_NWK_ENUM_MAX_FRM_CTR,              0, 0},
        { (uint8)ZPS_NWK_ENUM_NO_KEY,                   0, 0},
        { (uint8)ZPS_NWK_ENUM_BAD_CCM_OUTPUT,           0, 0},
        { (uint8)ZPS_NWK_ENUM_NO_ROUTING_CAPACITY,      0, 0},
        { (uint8)ZPS_NWK_ENUM_ROUTE_DISCOVERY_FAILED,   0, 0},
        { (uint8)ZPS_NWK_ENUM_ROUTE_ERROR,              0, 0},
        { (uint8)ZPS_NWK_ENUM_BT_TABLE_FULL,            0, 0},
        { (uint8)ZPS_NWK_ENUM_FRAME_NOT_BUFFERED,       0, 0},
        { (uint8)ZPS_NWK_ENUM_FRAME_IS_BUFFERED,        0, 0},
        { (uint8)MAC_ENUM_UNSUPPORTED_SECURITY,         0, 0},
        { (uint8)MAC_ENUM_CHANNEL_ACCESS_FAILURE,       0, 0},
        { (uint8)MAC_ENUM_FRAME_TOO_LONG,               0, 0},
        { (uint8)MAC_ENUM_INVALID_HANDLE,               0, 0},
        { (uint8)MAC_ENUM_INVALID_PARAMETER,            0, 0},
        { (uint8)MAC_ENUM_NO_ACK,                       0, 0},
        { (uint8)MAC_ENUM_NO_BEACON,                    0, 0},
        { (uint8)MAC_ENUM_NO_DATA,                      0, 0},
        { (uint8)MAC_ENUM_REALIGNMENT,                  0, 0},
        { (uint8)MAC_ENUM_TRANSACTION_EXPIRED,          0, 0},
        { (uint8)MAC_ENUM_TRANSACTION_OVERFLOW,         0, 0},
        { (uint8)MAC_ENUM_UNSUPPORTED_ATTRIBUTE,        0, 0},
        { (uint8)MAC_ENUM_SCAN_IN_PROGRESS,             0, 0},
        { (uint8)0xf6,                                  0, 0}    //    MAC_ENUM_DUTY_CYCLE_EXCEEDED
};

tsErrorLog  asExtendedErrorLog[SIZE_EXT_JN_ERRORS] = {
        { (uint8)ZPS_XS_E_LOOPBACK_BAD_ENDPOINT,                    0, 0},
        { (uint8)ZPS_XS_E_SIMPLE_DESCRIPTOR_NO_OUTPUT_CLUSTER,      0, 0},
        { (uint8)ZPS_XS_E_FRAG_NEEDS_ACK,                           0, 0},
        { (uint8)ZPS_XS_E_COMMAND_MANAGER_BAD_PARAMETER,            0, 0},
        { (uint8)ZPS_XS_E_INVALID_ADDRESS,                          0, 0},
        { (uint8)ZPS_XS_E_INVALID_TX_ACK_FOR_LOCAL_EP,              0, 0},
        { (uint8)ZPS_XS_E_NO_FREE_NPDU,                             0, 0},
        { (uint8)ZPS_XS_E_NO_FREE_APDU,                             0, 0},
        { (uint8)ZPS_XS_E_NO_FREE_SIM_DATA_REQ,                     0, 0},
        { (uint8)ZPS_XS_E_NO_FREE_APS_ACK,                          0, 0},
        { (uint8)ZPS_XS_E_NO_FREE_FRAG_RECORD,                      0, 0},
        { (uint8)ZPS_XS_E_NO_FREE_MCPS_REQ,                         0, 0},
        { (uint8)ZPS_XS_E_NO_FREE_LOOPBACK,                         0, 0},
        { (uint8)ZPS_XS_E_NO_FREE_EXTENDED_ADDR,                    0, 0},
        { (uint8)ZPS_XS_E_SIMPLE_DESCRIPTOR_NOT_FOUND,              0, 0},
        { (uint8)ZPS_XS_E_BAD_PARAM_APSDE_REQ_RSP,                  0, 0},
        { (uint8)ZPS_XS_E_NO_RT_ENTRY,                              0, 0},
        { (uint8)ZPS_XS_E_NO_BTR,                                   0, 0},
        { (uint8)ZPS_XS_E_FRAME_COUNTER_ERROR,                      0, 0},
        { (uint8)ZPS_XS_E_CCM_INVALID_ERROR,                        0, 0},
        { (uint8)ZPS_XS_E_UNKNOWN_SRC_ADDR,                         0, 0},
        { (uint8)ZPS_XS_E_NO_KEY_DESCRIPTOR,                        0, 0},
        { (uint8)ZPS_XS_E_NULL_KEYDESCR,                            0, 0},
        { (uint8)ZPS_XS_E_PDUM_ERROR,                               0, 0},
        { (uint8)ZPS_XS_E_NULL_EXT_ADDR,                            0, 0},
        { (uint8)ZPS_XS_E_ENCRYPT_NULL_DESCR,                       0, 0},
        { (uint8)ZPS_XS_E_ENCRYPT_FRAME_COUNTER_FAIL,               0, 0},
        { (uint8)ZPS_XS_E_ENCRYPT_DEFAULT,                          0, 0},
        { (uint8)ZPS_XS_E_FRAME_COUNTER_EXPIRED,                    0, 0}

};

PRIVATE teJNState eJNState = JN_NOT_READY;
/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/


/********************************************************************************
  *
  * @fn PUBLIC void vShowErrorLogs
  *
  */
 /**
  *
  * @param
  *
  *
  * @brief prints the error logs without string for the error codes
  *
  * @return nobe
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vShowErrorLogs(void)
{
    uint32 i;
    for (i=0U; i< (sizeof(asErrorLog)/sizeof(tsErrorLog)); i++)
    {
        DBG_vPrintf((bool_t)TRUE, "Error Code \t0x%02X \tCount %d \tTotal %d %d\r\n",
                asErrorLog[i].u8ErrorCode,
                asErrorLog[i].u16ErrorCount,
                asErrorLog[i].u32TotalCount,
                i);
    }
    DBG_vPrintf((bool_t)TRUE, "\r\nExtended errors\r\n" );
    for (i=0U; i< (sizeof(asExtendedErrorLog)/sizeof(tsErrorLog)); i++)
    {
        DBG_vPrintf((bool_t)TRUE, "Error Code \t0X%02X \tCount %d ztTotal %d %d\r\n",
                asExtendedErrorLog[i].u8ErrorCode,
                asExtendedErrorLog[i].u16ErrorCount,
                asExtendedErrorLog[i].u32TotalCount,
                i);
    }
}


/********************************************************************************
  *
  * @fn PUBLIC void vClearJNErrorLogs
  *
  */
 /**
  *
  * @param  bool_t bAll
  *
  *
  * @brief reset the errors counts
  *
  * @return nobe
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vClearJNErrorLogs( bool_t bAll)
{
    uint32 i;
    for (i=0U; i< (sizeof(asErrorLog)/sizeof(tsErrorLog)); i++)
    {
        asErrorLog[i].u16ErrorCount = 0u;
        if (bAll == (bool_t)TRUE)
        {
            asErrorLog[i].u32TotalCount = 0u;
        }
    }
    DBG_vPrintf((bool_t)TRUE, "\r\nExtended errors\r\n" );
    for (i=0U; i< (sizeof(asExtendedErrorLog)/sizeof(tsErrorLog)); i++)
    {
        asExtendedErrorLog[i].u16ErrorCount = 0u;
        if (bAll == (bool_t)TRUE)
        {
            asExtendedErrorLog[i].u32TotalCount = 0u;
        }
    }
}

/********************************************************************************
  *
  * @fn PRIVATE void initUARTStateMachine
  *
  */
 /**
  *
  *
  * @brief Set Uart state machine to start
  *
  * @return PRIVATE void
  *
  * @note
  *
 ********************************************************************************/
PRIVATE void initUARTStateMachine(void)
{
     eRxState = (teSL_RxState)E_STATE_RX_WAIT_START;
}



/********************************************************************************
  *
  * @fn PUBLIC void restartStateMachineSLCommon
  *
  */
 /**
  *
  *
  * @brief Reinit UART API
  *
  * @return PUBLIC void
  *
  * @note
  *
 ********************************************************************************/

PUBLIC void restartStateMachineSLCommon(void)
{

    initUARTStateMachine();

    sSLCommon.bIsSlMsgGetVer = (bool_t)FALSE;
    sSLCommon.bIsWaitingStatus = (bool_t)FALSE;
    sSLCommon.pu8CurrentRxBuffer = NULL;

    restartUARTFreeRtosCommon((bool_t)TRUE);
    restartUARTRegs();
}

/********************************************************************************
  *
  * @fn PUBLIC uint8* pSL_ReadMessage
  *
  */
 /**
  *
  * @param u8Data  R Byte Read from serial link
  *
  *
  * @brief read a byte from the serial link
  *
  * @return pointer to allocated buffer when a full packet received
  * NULL when packet not fully received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 *pSL_ReadMessage(uint8 u8Data)
{
    static uint8 *pu8CRC;
    static uint16 u16Bytes, *pu16Type, *pu16Length;
    static bool_t bInEsc = (bool_t)FALSE;
    static uint8 *pu8RxSerialBuffer;

    union
    {
        uint8*  pu8;
        uint16* pu16;
    } upBuf;


    switch(u8Data)
    {
    case SL_START_CHAR:
        if(eRxState != ((teSL_RxState)E_STATE_RX_WAIT_START))
        {
             /* START CHAR received when it is not waiting for that */
            DBG_vPrintf((bool_t)TRUE, "Unexpected start char in state %d\n", eRxState);

             vSL_FreeRxBuffer(pu8RxSerialBuffer);
             if (eRxState > ((teSL_RxState)E_STATE_RX_WAIT_DATA))
             {
                 eRxState = (teSL_RxState)E_STATE_RX_WAIT_START;
                 DBG_vPrintf((bool_t)TRUE, "SERIAL RX: Start char received in wrong state");
                  break;
             }
             else
             {
                 DBG_vPrintf((bool_t)TRUE, "SERIAL RX: Unexpected start char\n");
                 eRxState = (teSL_RxState)E_STATE_RX_WAIT_START;
             }
        }
        /* Reset state machine*/
        u16Bytes = 0U;
        bInEsc = (bool_t)FALSE;
        DBG_vPrintf((bool_t)DEBUG_SL, "RX Start\n");

        /* Get the Rx buffer from small buffer pool*/
        pu8RxSerialBuffer = pu8SL_GetRxBufferFromSmallPool();
        if(pu8RxSerialBuffer == NULL)
        {
            /* No Small buffers available, try to get a large buffer */
            pu8RxSerialBuffer = pu8SL_GetRxBufferFromLargePool();
            DBG_vPrintf((bool_t)TRUE , "Allocated large buffers as no small buffer available \n");
            if(pu8RxSerialBuffer == NULL)
            {
                /* No buffer available to receive data. */
                DBG_vPrintf((bool_t)TRUE, "SERIAL RX: No Rx Serial Buffer\n");
                /* call out to allow for any buffer management actions */
                vHandleRxBufferExhausted();

                pu8RxSerialBuffer = pu8SL_GetRxBufferFromSmallPool();
                if (pu8RxSerialBuffer == NULL)
                {
                    DBG_vPrintf((bool_t)TRUE, "SERIAL RX: Still not got one\n");
                    return NULL;
                }
            }
        }
        upBuf.pu8 = (pu8RxSerialBuffer + 1U);
        pu16Type = upBuf.pu16;
        upBuf.pu8 = (pu8RxSerialBuffer + 3U);
        pu16Length = upBuf.pu16;
        pu8CRC = (pu8RxSerialBuffer + SL_MSG_RX_CRC__IDX);
        eRxState = (teSL_RxState)E_STATE_RX_WAIT_TYPEMSB;
        break;  // SL_START_CHAR

    case SL_ESC_CHAR:
        /* Escape next character*/
        bInEsc = (bool_t)TRUE;
        break; // SL_ESC_CHAR


    case SL_END_CHAR:
        /* End message*/
        if (eRxState == ((teSL_RxState)E_STATE_RX_WAIT_START))
        {
            return NULL;
        }
        DBG_vPrintf((bool_t)DEBUG_SL, "Got END\n");
        eRxState = (teSL_RxState)E_STATE_RX_WAIT_START;
        if(u16Bytes != (*pu16Length))
        {
            vSL_FreeRxBuffer(pu8RxSerialBuffer);
            sLpcSerialStats.u32SLRxFail++;
            DBG_vPrintf((bool_t)TRUE, "Got End char- Not all bytes received\n");
            return(NULL);
        }
        if((bool_t)FALSE == bSL_ValidateIncomingMessage(pu8RxSerialBuffer))
        {
#ifdef ENABLE_UART_ACK_FROM_HOST
            Send_Nack_Host_JN();
#endif
            vSL_FreeRxBuffer(pu8RxSerialBuffer);
            sLpcSerialStats.u32SLRxFail++;
            return NULL;
        }
#ifdef ENABLE_UART_ACK_FROM_HOST
        /*This part of code sends the uart acknowledgment to JN*/
        if ( ( (uint16)E_SL_MSG_STATUS_SHORT_MSG != *pu16Type ) &&
             ( (uint16)E_SL_MSG_STATUS_MSG != *pu16Type ) &&
             ( (uint16)E_SL_MSG_HOST_JN_NACK  != *pu16Type))
        {
            if (u8AckTestMode == 2U)
            {
                /* donot ack */
                u8AckTestMode--;
                vSL_FreeRxBuffer(pu8RxSerialBuffer);
                sLpcSerialStats.u32SLRxFail++;
                return NULL;
            }
            else if (u8AckTestMode == 1U)
            {
                /* send a nack */
                u8AckTestMode--;
                Send_Nack_Host_JN();
                vSL_FreeRxBuffer(pu8RxSerialBuffer);
                sLpcSerialStats.u32SLRxFail++;
                return NULL;
            }
            else
            {
                Send_Ack_Host_JN();
            }

        }
#endif//ENABLE_UART_ACK_FROM_HOST

        if ( ((uint16)E_SL_MSG_HOST_JN_NACK != *pu16Type ) &&
             ( (uint16)E_SL_MSG_HOST_JN_ACK  != *pu16Type) )
        {
            u8SLRxSeqNum = pu8RxSerialBuffer[SL_MSG_TX_SEQ_NO_IDX];
            u8JNRxSeqNum = pu8RxSerialBuffer[SL_MSG_RX_SEQ_NO_IDX];
        }



        if ((bool_t)TRUE == bIsDuplicate( *pu16Type, pu8RxSerialBuffer[SL_MSG_TX_SEQ_NO_IDX]) )
        {
            DBG_vPrintf((bool_t)TRUE, "Duplicate Type %04x seq No %d\n",
                    *pu16Type, pu8RxSerialBuffer[SL_MSG_TX_SEQ_NO_IDX]);
            vSL_FreeRxBuffer(pu8RxSerialBuffer);
            return NULL;

        }

        if ( (uint16)E_SL_MSG_EVENT_RESET  == *pu16Type )
        {
            /* align lpc stats to jn stats */
            vResetLpcSerialStats();

            /* Set JN ready state to NOT READY */
            vSetJNState(JN_NOT_READY);

            /* Set the Standard Response Period by default after a JN reset */
            vSL_SetStandardResponsePeriod();
#ifdef SL_JN_RESET_HOOK
            /* JN reset hook */
            vSL_JNResetHook();
#endif
        }
        if (( (uint16)E_SL_MSG_STATUS_MSG == *pu16Type ) ||
             ( (uint16)E_SL_MSG_STATUS_SHORT_MSG == *pu16Type ))
        {
            sLpcSerialStats.u32SLRxStatusMsg++;
        }
        else
        {
            sLpcSerialStats.u32SLRxEventMsg++;
            DBG_vPrintf((bool_t)FALSE, "Push Msg Type %04x TxSeq %d  Rx Seq %d\n",
                    *pu16Type,
                    pu8RxSerialBuffer[SL_MSG_TX_SEQ_NO_IDX],
                    pu8RxSerialBuffer[SL_MSG_RX_SEQ_NO_IDX]);
        }
        return pu8RxSerialBuffer; // SL_END_CHAR

    default:    //  switch u8Data
        if(bInEsc)
        {
            /* Unescape the character */
            u8Data ^= 0x8U;
            bInEsc = (bool_t)FALSE;
        }


        switch(eRxState)
        {

        case (teSL_RxState)E_STATE_RX_WAIT_START:
            break;

        case (teSL_RxState)E_STATE_RX_WAIT_TYPEMSB:
            *pu16Type = (uint16)u8Data << 8U;
            eRxState = (teSL_RxState)E_STATE_RX_WAIT_TYPELSB;
            break;

        case (teSL_RxState)E_STATE_RX_WAIT_TYPELSB:
            *pu16Type += (uint16)u8Data;
            DBG_vPrintf((bool_t)DEBUG_SL, "Type 0x%x\n", *pu16Type & 0xFFFFU);
            eRxState = (teSL_RxState)E_STATE_RX_WAIT_LENMSB;
            break;

        case (teSL_RxState)E_STATE_RX_WAIT_LENMSB:
            *pu16Length = (uint16)u8Data << 8U;
            eRxState = (teSL_RxState)E_STATE_RX_WAIT_LENLSB;
            break;

        case (teSL_RxState)E_STATE_RX_WAIT_LENLSB:
            *pu16Length += (uint16)u8Data;
            DBG_vPrintf((bool_t)DEBUG_SL, "Length %d\n", *pu16Length);
            if(*pu16Length > MAX_RX_LARGE_SERIAL_BUFFER_SIZE)
            {
                DBG_vPrintf((bool_t)TRUE, "Length > MaxLength\n");
                eRxState = (teSL_RxState)E_STATE_RX_WAIT_START;
                sLpcSerialStats.u32SLRxFail++;
                vSL_FreeRxBuffer(pu8RxSerialBuffer);
            }
            else
            {
                if((*pu16Length + SL_MSG_DATA_START_IDX)  > MAX_RX_SMALL_SERIAL_BUFFER_SIZE)
                {
                    uint8 *pu8RxSerialBufferTemp;

                    pu8RxSerialBufferTemp = pu8SL_GetRxBufferFromLargePool();
                    if(pu8RxSerialBufferTemp == NULL)
                    {
                        eRxState = (teSL_RxState)E_STATE_RX_WAIT_START;
                        vSL_FreeRxBuffer(pu8RxSerialBuffer);
                        DBG_vPrintf((bool_t)TRUE , "No SerialRx buffer available %d \n",(*pu16Length + SL_MSG_DATA_START_IDX) );
                        vSL_PrintRxBufferPool((bool_t)TRUE);
                        return NULL;
                    }

                    upBuf.pu8 = pu8RxSerialBufferTemp + 1;
                    *upBuf.pu16 = *pu16Type;
                    upBuf.pu8 = pu8RxSerialBufferTemp + 3;
                    *upBuf.pu16 = *pu16Length;

                    upBuf.pu8 = pu8RxSerialBufferTemp + 1;
                    pu16Type = upBuf.pu16;
                    upBuf.pu8 = pu8RxSerialBufferTemp + 3;
                    pu16Length = upBuf.pu16;
                    pu8CRC = (pu8RxSerialBufferTemp + SL_MSG_RX_CRC__IDX);
                    vSL_FreeRxBuffer(pu8RxSerialBuffer);
                    pu8RxSerialBuffer =pu8RxSerialBufferTemp;
                }
                if (*pu16Type != (uint16)E_SL_MSG_STATUS_SHORT_MSG)
                {
                    eRxState = (teSL_RxState)E_STATE_RX_WAIT_TX_SEQ_NO;
                }
                else
                {
                    eRxState = (teSL_RxState)E_STATE_RX_WAIT_CRC;
                    pu8RxSerialBuffer[SL_MSG_TX_SEQ_NO_IDX] = 0u;
                    pu8RxSerialBuffer[SL_MSG_RX_SEQ_NO_IDX] = 0u;
                }
            }
            break; // E_STATE_RX_WAIT_LENLSB

        case E_STATE_RX_WAIT_TX_SEQ_NO:
            pu8RxSerialBuffer[SL_MSG_TX_SEQ_NO_IDX] = u8Data;
            eRxState = E_STATE_RX_WAIT_RX_SEQ_NO;
            break;

        case E_STATE_RX_WAIT_RX_SEQ_NO:
            pu8RxSerialBuffer[SL_MSG_RX_SEQ_NO_IDX] = u8Data;
            eRxState = E_STATE_RX_WAIT_CRC;
            break;

        case E_STATE_RX_WAIT_CRC:
            *pu8CRC = u8Data;
            eRxState = E_STATE_RX_WAIT_DATA;
            break;

        case E_STATE_RX_WAIT_DATA:
            if(u16Bytes < *pu16Length)
            {
                DBG_vPrintf((bool_t)DEBUG_SL, "%02x ", u8Data);
                pu8RxSerialBuffer[SL_MSG_DATA_START_IDX+(uint16)u16Bytes++] = u8Data;
            }
            else {
                DBG_vPrintf((bool_t)TRUE, "Expected end char Type %04x Len %d\n",
                        *pu16Type, *pu16Length);
                eRxState = E_STATE_RX_WAIT_START;
                vSL_FreeRxBuffer(pu8RxSerialBuffer);
                sLpcSerialStats.u32SLRxFail++;
                u16Bytes = 0u;
                bInEsc = (bool_t)FALSE;
            }
            break; // E_STATE_RX_WAIT_DATA
        default:
            /* No action required */
            break;
        } // switch eRxState
        break; // default
    }

    return(NULL);
}

/********************************************************************************
  *
  * @fn PUBLIC bool_t bSL_ReadJnBootLoaderMessage
  *
  */
 /**
  *
  * @param u8Data  R data read from JN bootloader
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
PUBLIC bool_t bSL_ReadJnBootLoaderMessage(uint8 u8Data)
{
    static uint8 u8Bytes;
    static uint8 *pu8RxSerialBuffer;

    if(u8Bytes == 0U)
    {
        pu8RxSerialBuffer = &sSLCommon.au8RxLargeSerialBuffer[0][0];

        sSLCommon.pu8CurrentRxBuffer = pu8RxSerialBuffer;

        /* clear buffer */
        (void)ZBmemset(pu8RxSerialBuffer, 0, MAX_RX_LARGE_SERIAL_BUFFER_SIZE);
    }
    pu8RxSerialBuffer[u8Bytes++] = u8Data;
    if(u8Bytes == pu8RxSerialBuffer[0U] + 1U)       /*check length of received data*/
    {
        u8Bytes = 0U;
        return (bool_t)TRUE;
    }
    return (bool_t)FALSE;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SL_WriteJnBootLoaderMessage
  *
  */
 /**
  *
  * @param u16Length uint16
  * @param pu8Data uint8 *
  * @param pu8RcvdData uint8 **
  *
  * @brief Write message to the serial link
  *
  * @return uint8
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SL_WriteJnBootLoaderMessage(uint16 u16Length, uint8 *pu8Data, uint8 **pu8RcvdData)
{
    uint8 u8MsgStatus;
    uint16 u16i;

    /* Set Status for waiting for message */
    sSLCommon.bIsWaitingStatus = (bool_t)TRUE;

    /* send message out */
    for(u16i =0U; u16i< u16Length; u16i++)
    {
        SL_WRITE(*pu8Data++);
    }

    /* wait for the status */
    if((pu8SL_GetRxMessageFromSerialQueue(IGNORE_MSG_TYPE) != (void*)0U))
    {
        *pu8RcvdData = sSLCommon.pu8CurrentRxBuffer;
        u8MsgStatus = (uint8)E_SL_MSG_STATUS_SUCCESS;
    }
    else
    {
        u8MsgStatus = (uint8)E_SL_MSG_STATUS_TIME_OUT;
    }
    sSLCommon.bIsWaitingStatus = (bool_t)FALSE;

    return u8MsgStatus;
}

/********************************************************************************
  *
  * @fn PUBLIC uint8 u8SL_WriteMessage
  *
  */
 /**
  *
  * @param u16Type uint16
  * @param u16Length uint16
  * @param pu8Data uint8 *
  * @param pvData void *
  *
  * @brief Write message to the serial link
  *
  * @return uint8 (tsSL_Msg_Status->eStatus) see serial_link_ctrl.h
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 u8SL_WriteMessage(uint16 u16Type, uint16 u16Length, uint8 *pu8Data, void *pvData)
{
    uint8 u8Status;
    uint16 u16n;
    uint8 u8CRC, *pu8Temp = pvData;
    uint8 *pu8RxBuffer = NULL;
    uint16 u16SentPktType = 0x00U;
    uint16 u16ReadIdx;
    uint16 u16RxLength;
    uint8 u8i, u8RetryCount;
    uint16 u16bytesWritten=0U;
    uint8 u8Sleeps=0U;
    uint32 u32TimeSent=0U;
    uint8 u8TxSeqNo, u8RxSeqNo;
    uint32 u32TimeReceived;


    union
    {
        uint8*  pu8;
        uint16* pu16;
    }
    upBuf;

    #ifdef SL_TRANSMIT_HOOK
    if (bSL_TransmitHook(u16Type, u16Length, pu8Data, pvData))
    {
        return E_SL_MSG_STATUS_SUCCESS;
    }
#endif

    vSL_SerialLinkRtosTest();
    vSL_TakeSerialSemaphore();
    vSL_WakeJN( u16Length,  bIsJNInDeepSleep,  u16Type );

    vSL_EmptyStatusMsgQueue();

    sLpcSerialStats.u32SLTxCount++;

    if ( (u16Type != (uint16)E_SL_MSG_GET_VERSION) &&
            (u16Type != (uint16)E_SL_MSG_GET_PIB_ATTR) )
    {
        u8TxSeqNo = u8SLTxSeqNum;
        u8SLTxSeqNum++;
        u8RxSeqNo = u8SLRxSeqNum;
    }
    else
    {
        u8TxSeqNo = 0u;
        u8RxSeqNo = 0u;
    }

    u8CRC = u8SL_CalculateCRC(u16Type, u16Length, u8TxSeqNo, u8RxSeqNo, pu8Data);
    DBG_vPrintf((bool_t)DEBUG_SL, "vSL_WriteMessage(%d, %d, %02x)\n", u16Type, u16Length, u8CRC);


    for(u8RetryCount = 0U; u8RetryCount < TX_RETRY_COUNT; u8RetryCount++)
    {
        /* Don't waste time here if JN is ready or bypass for GET_VERSION */
        if (teGetJNState() == JN_NOT_READY &&
            (u16Type != (uint16)E_SL_MSG_GET_VERSION)) {
            vWaitForJNReady(JN_READY_TIME_MS);
        }

        /* Check again in case WaitForJNReady has timed out, bypass for GET_VERSION */
        if (teGetJNState() != JN_NOT_READY ||
            (u16Type == (uint16)E_SL_MSG_GET_VERSION))
        {
            /* Send start character */
            if (u8RetryCount > 0U)
            {
                sLpcSerialStats.u32SLTxRetries++;
                DBG_vPrintf((bool_t)TRUE, "Retry %d send again type %04x\n", u8RetryCount, u16Type);
            }
#ifdef ENABLE_SERIAL_LINK_FILE_LOGGING
            uint8_t rxBuffer[20];
            sprintf(rxBuffer, "\n[%d]TX-->", zbPlatGetTime());
            bSL_LoggerWrite(rxBuffer, strlen(rxBuffer));
#endif
            vSL_TxByte((bool_t)TRUE, SL_START_CHAR);

            /* Send message type */
            vSL_TxByte((bool_t)FALSE, (uint8)((uint16)(u16Type >> 8U) & 0xffU));
            vSL_TxByte((bool_t)FALSE, (uint8)((uint16)(u16Type >> 0U) & 0xffU));

            /* Send message length */
#ifdef SL_HOST_TO_COPROCESSOR_SECURE
            vSL_SecureTransmitLength(u16Length);
#else
            vSL_TxByte((bool_t)FALSE, (uint8)((uint16)(u16Length >> 8U) & 0xffU));
            vSL_TxByte((bool_t)FALSE, (uint8)((uint16)(u16Length >> 0U) & 0xffU));
#endif

            if ((u16Type != (uint16)E_SL_MSG_GET_VERSION) &&
                    (u16Type != (uint16)E_SL_MSG_GET_PIB_ATTR))
            {
                vSL_TxByte((bool_t)FALSE, u8TxSeqNo);
                vSL_TxByte((bool_t)FALSE, u8RxSeqNo);
            }

            /* Send message checksum */
            vSL_TxByte((bool_t)FALSE, u8CRC);

#ifdef SL_HOST_TO_COPROCESSOR_SECURE
            vSL_SecureEncryptMessage(u16Length, pu8Data);
#endif


            u16bytesWritten=0U;
            u8Sleeps=0U;

            for(u16n = 0U; u16n < u16Length; u16n++)
            {
                u16bytesWritten++;
                vSL_TxByte((bool_t)FALSE, pu8Data[u16n]);

                if(u16n >= SL_BLOCK_SIZE)
                {
                    /* check for multiples of block size, and delay */
                    if(((u16n % SL_BLOCK_SIZE) == 0U))
                    {
                            u8Sleeps++;
                            vSleep(2UL);
                    }
                }
            }

#ifdef SL_HOST_TO_COPROCESSOR_SECURE
            vSL_SecureTransmitMessageAuth(u16Length, pu8Data);
#endif
            /* Send end character */
            vSL_TxByte((bool_t)TRUE, SL_END_CHAR);

            u32TimeSent = zbPlatGetTime();

            /* Set Status for waiting for message */
            sSLCommon.bIsWaitingStatus = (bool_t)TRUE;

            if(u16Type == (uint16)E_SL_MSG_GET_VERSION)
            {
                sSLCommon.bIsSlMsgGetVer = (bool_t)TRUE;
            }
            pu8RxBuffer = pu8SL_GetRxMessageFromSerialQueue(u16Type);
        }

        if(pu8RxBuffer != (void*)0U)
        {
            /* collate spme response time stats */
            u32TimeReceived = zbPlatGetTime();
            uint32 u32Delta = u32TimeReceived - u32TimeSent;

            if (u32Delta < 6U)
            {
                sLpcSerialStats.u32FiveMs++;
            }
            else if (u32Delta < 10U)
            {
                sLpcSerialStats.u32TenMs++;
            }
            else if (u32Delta < 50U)
            {
                sLpcSerialStats.u32FiftyMs++;
            }
            else
            {
                sLpcSerialStats.u32Greater50ms++;
                DBG_vPrintf((bool_t)TRUE, "Pkt Type %04x Took %d\n", u16Type, u32Delta);
            }
            /* trap the slowest resonse */
            if (u32Delta > sLpcSerialStats.u32MaxDelay)
            {
                sLpcSerialStats.u32MaxDelay = u32Delta;
                sLpcSerialStats.u32PktType = (uint32)u16Type;
                DBG_vPrintf((bool_t)TRUE, "Pkt Type %04x Set New Max Response Time %d\n", u16Type, sLpcSerialStats.u32MaxDelay);
            }
            DBG_vPrintf((bool_t)DEBUG_SL, "Sl_Write Stats - Len: %d, BW: %d, SL: %d \r\n", u16Length, u16bytesWritten, u8Sleeps);
            break;
        }
        else
        {
            if (u8RetryCount == Tx_RETRIES_BEFORE_RESET)
            {
                DBG_vPrintf((bool_t)TRUE, "Resetting Zigbee Module - Len: %d, BW: %d, SL: %d, TP: %d \r\n", u16Length, u16bytesWritten, u8Sleeps, u16Type);
                APP_vNcpHostResetZigBeeModule();
                vWaitForJNReady(JN_READY_TIME_MS); /* wait for JN module to stabilize after reset */
            }
            else
            {
                DBG_vPrintf((bool_t)TRUE, "ReTry Send %04x\n", u16Type);
            }
            /* Flushing the Msg status Q, as it might hold the previous */

            DBG_vPrintf((bool_t)TRUE, "No Response to the send\n");

        }
    }     /* for retrycount  */

    if (u8RetryCount == TX_RETRY_COUNT)
    {
        sLpcSerialStats.u32SLTxFailures++;
    }

    if(u16Type == (uint16)E_SL_MSG_GET_VERSION)
    {
        sSLCommon.bIsSlMsgGetVer = (bool_t)FALSE;
    }
    if((pu8RxBuffer != (void*)0U))
    {
        u8Status = *(pu8RxBuffer + SL_MSG_RX_STATUS_IDX);
        u16SentPktType = ((uint16)*(pu8RxBuffer + SL_MSG_RSP_TYPE_MSB_IDX)) << 8U;
        u16SentPktType += (uint16)*(pu8RxBuffer + SL_MSG_RSP_TYPE_LSB_IDX);
        upBuf.pu8 = pu8RxBuffer + SL_MSG_RX_LEN_IDX;
        u16RxLength = *upBuf.pu16;

        if ((u8Status != 0U) && (u16Type != (uint16)E_SL_MSG_SERIAL_LINK_TEST_ERR_CODE))
        {
            vMonitorAllJnErrors(u8Status, (bool_t)FALSE);
        }

        if ( u8Status == (uint8)E_SL_MSG_STATUS_UNSUPPORTED_COMMAND)
        {
            DBG_vPrintf((bool_t)TRUE, "WARNING: Pkt type 0x%04x unhandled by the JN\n", u16SentPktType);
        }


        u16RxLength += SL_MSG_DATA_START_IDX;  // add the header bytes for total length
        u16ReadIdx = ~u16RxLength;             // Initialise u16ReadIdx to be != u16RxLength (artf547790)

        /* check Rxed Pkt Type */
        if(u16SentPktType == u16Type)
        {
            switch(u16SentPktType)
            {
                case (uint16)E_SL_MSG_SET_JN_ACK_TIMEOUT:
                case (uint16)E_SL_SET_ED_THRESHOLD:
                case (uint16)E_SL_MSG_SET_EXT_PANID:
                case (uint16)E_SL_MSG_STOP_EZ_SET_UP:
                case (uint16)E_SL_MSG_SET_DEVICE_PERMISSIONS:
                case (uint16)E_SL_MSG_SERIAL_LINK_SEND_TEST:
                case (uint16)E_SL_MSG_SET_END_DEVICE_PERMISSIONS:
                case (uint16)E_SL_MSG_ADD_REPLACE_LINK_KEY:
                case (uint16)E_SL_MSG_SET_TC_ADDRESS:
                case (uint16)E_SL_MSG_SET_INIT_SEC_STATE:
                case (uint16)E_SL_MSG_SET_OVERRIDE_LOCAL_MAC_ADDR:
                case (uint16)E_SL_MSG_SET_ENDPOINT_STATE:
                case (uint16)E_SL_MSG_SET_KEY_TABLE_ENTRY:
                case (uint16)E_SL_MSG_ENABLE_DISABLE_JN_POWER:
                case (uint16)E_SL_MSG_SET_CLUSTER_DISCOVERY_STATE:
                case (uint16)E_SL_MSG_SET_APS_FRAME_COUNTER:
                case (uint16)E_SL_MSG_RESET:
                case (uint16)E_SL_MSG_START_NETWORK:
                case (uint16)E_SL_MSG_START_SCAN:
                case (uint16)E_SL_MSG_SETUP_FOR_INTERPAN:
                case (uint16)E_SL_MSG_REJOIN_NETWORK:
                case (uint16)E_SL_MSG_INSECURE_REJOIN_NETWORK:
                case (uint16)E_SL_MSG_SET_NUM_EZ_SCANS:
                case (uint16)E_SL_MSG_SET_CHANNELMASK:
                case (uint16)E_SL_MSG_SET_SECURITY:
                case (uint16)E_SL_MSG_ZDO_SET_DEVICETYPE:
                case (uint16)E_SL_MSG_ERASE_PERSISTENT_DATA:
                case (uint16)E_SL_MSG_PERMIT_JOINING_REQUEST:
                case (uint16)E_SL_MSG_TEST_TYPE:
                case (uint16)E_SL_MSG_SET_POLL_CONFIG:
                case (uint16)E_SL_MSG_SET_END_DEVICE_TIMEOUT:
                case (uint16)E_SL_MSG_STOP_POLL_CYCLE:
                case (uint16)E_SL_MSG_SET_STAY_AWAKE_FLAG:
                case (uint16)E_SL_MSG_SET_MAX_MAC_FAIL_COUNT:
                case (uint16)E_SL_MSG_ADD_ADDRESS_MAP_ENTRY:
                case (uint16)E_SL_MSG_REQUEST_KEY_REQ:
                case (uint16)E_SL_MSG_SET_CHANNEL:
                case (uint16)E_SL_MSG_SET_TX_POWER:
                case (uint16)E_SL_MSG_REMOVE_LINK_KEY:
                case (uint16)E_SL_MSG_SEARCH_TRUST_CENTER:
                case (uint16)E_SL_MSG_UPDATE_DEFAULT_LINK_KEY:
                case (uint16)E_SL_MSG_TRANSPORT_NETWORK_KEY:
                case (uint16)E_SL_MSG_CLEAR_NETWORK_KEY:
                case (uint16)E_SL_MSG_SET_NETWORK_KEY:
                case (uint16)E_SL_MSG_MAC_SET_FILTER_STORAGE:
                case (uint16)E_SL_MSG_MAC_FILTER_ADD_ACCEPT:
                case (uint16)E_SL_MSG_CLEAR_SEC_MAT_SET_ENTRY:
                case (uint16)E_SL_MSG_SET_JN_INTERNAL_ATTENUATOR:
                case (uint16)E_SL_MSG_CHANGE_EXT_PANID:
                case (uint16)E_SL_MSG_CHANGE_PANID:
                case (uint16)E_SL_MSG_DISCOVER_NETWORKS:
                case (uint16)E_SL_MSG_START_ED_SCAN:
                case (uint16)E_SL_MSG_SET_RESTORE_POINT:
                case (uint16)E_SL_MSG_SET_MAC_ADDR_TABLE:
                case (uint16)E_SL_MSG_SET_ED_TIMEOUT_ON_PARENT:
                case (uint16)E_SL_MSG_SET_SUB_GHZ_ANTENNA:
                case (uint16)E_SL_MSG_SET_NWK_FRAME_COUNT:
                case (uint16)E_SL_MSG_START_ROUTER:
                case (uint16)E_SL_MSG_ENABLE_DISABLE_DUAL_MAC:
                case (uint16)E_SL_MSG_PDM_CONVERT:
                case (uint16)E_SL_MSG_SERIAL_LINK_HALT_JN:
                case (uint16)E_SL_MSG_SERIAL_LINK_ROLLBACK_TEST:
                case (uint16)E_SL_MSG_ORPHAN_REJOIN_NWK:
                case (uint16)E_SL_MSG_SERIAL_LINK_EXHAUST_APDU:
                case (uint16)E_SL_MSG_SERIAL_LINK_STACK_POLL_RATE:
                case (uint16)E_SL_MSG_BLOCK_MDS:
                case (uint16)E_SL_MSG_SERIAL_LINK_RESET_OL_MODULE:
                case (uint16)E_SL_MSG_SERIAL_LINK_SET_LEAVE_DECIDER:
                case (uint16)E_SL_MSG_SERIAL_LINK_EXHAUST_REQ_DESC:
                case (uint16)E_SL_MSG_SERIAL_LINK_TOGGLE_OL_RESET:
                case (uint16)E_SL_MSG_SERIAL_LINK_SET_RXONIDLE:
                case (uint16)E_SL_MSG_SERIAL_LINK_SLEEP_OL:
                case (uint16)E_SL_MSG_SERIAL_LINK_WAKE_OL:
                case (uint16)E_SL_MSG_READ_OL_PARAMS:
                case (uint16)E_SL_MSG_SERIAL_SEQ_TEST:
                case (uint16)E_SL_MSG_SERIAL_LINK_TEST_ERR_CODE:
                case (uint16)E_SL_MSG_SERIAL_LINK_RESET_BIND_SERVER:
                case (uint16)E_SL_MSG_SERIAL_LINK_SET_COMPLIANCE_LIMITS:
                case (uint16)E_SL_MSG_SERIAL_LINK_ENABLE_BOUND_DEVICES:
                case (uint16)E_SL_MSG_SERIAL_LINK_SET_BOUND_DEVICE:
                case (uint16)E_SL_MSG_SERIAL_LINK_CLONE_ZED:
                case (uint16)E_SL_MSG_SERIAL_LINK_SET_MANUFACTURER_CODE:
                case (uint16)E_SL_MSG_SET_REPROVISSION_DATA:
                case (uint16)E_SL_MSG_INSECURE_REJOIN_SHORT_PAN:
                case (uint16)E_SL_MSG__REJOIN_WITH_CHANNEL_MASK:
                case (uint16)E_SL_MSG_SET_SECURITY_TIMEOUT:
                case (uint16)E_SL_MSG_SEND_ZED_TIMEOUT:
                case (uint16)E_SL_MSG_SET_PARENT_TIMEOUT:
                case (uint16)E_SL_MSG_SET_PANID_CNFL_RSVL:
                case (uint16)E_SL_MSG_NETWORK_CHANGE_ADDRESS:
                case (uint16)E_SL_MSG_ZDO_REMOVE_DEVICE_REQ:
                case (uint16)E_SL_MSG_ZDO_REMOVE_DEVICE_FROM_TABLES:
                case (uint16)E_SL_MSG_SEND_DATA_REQUEST:
                case (uint16)E_SL_MSG_SET_USE_INSTALL_CODE:
                case (uint16)E_SL_MSG_SET_UPDATE_ID:
                case (uint16)E_SL_MSG_NWK_SET_DEVICETYPE:
                case (uint16)E_SL_SET_KEY_SEQ_NUMBER:
                case (uint16)E_SL_MSG_SET_NWK_ADDR:
                case (uint16)E_SL_MSG_SET_MAC_CAPABILITY:
                case (uint16)E_SL_MSG_SET_NWK_STATE_ACTIVE:
                case (uint16)E_SL_MSG_SET_DEPTH:
                case (uint16)E_SL_MSG_ADD_REPLACE_INSTALL_CODES:
                case (uint16)E_SL_MSG_SET_TC_LOCKDOWN_OVERRIDE:
                case (uint16)E_SL_MSG_ADD_TRSP_KEY_DECIDER_TABLE_ENTRY:
                case (uint16)E_SL_MSG_REMOVE_TRSP_KEY_DECIDER_TABLE_ENTRY:
                case (uint16)E_SL_MSG_SET_TRSP_KEY_DECIDER_TABLE_POLICY:
                case (uint16)E_SL_MSG_CLEAR_TRSP_KEY_DECIDER_TABLE:
                case (uint16)E_SL_MSG_NWK_CLEAR_DISC_NT:
                case (uint16)E_SL_MSG_JOIN_NETWORK:
                {
                    /* no extra data; just status seq no & type */
                    u16ReadIdx = SL_MSG_RSP_START_IDX;
                }
                break;
                case (uint16)E_SL_MSG_GET_NWK_INTERFACE_REQ:
                {
                    if(NULL != pu8Temp)
                    {
                        u16ReadIdx = SL_MSG_RSP_START_IDX;
                        *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx);
                        u16ReadIdx++;
                        *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx);
                        u16ReadIdx++;
                        *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx);
                        u16ReadIdx++;
                        (void)ZBmemcpy(pu8Temp, (pu8RxBuffer+u16ReadIdx), sizeof(uint32));
                        pu8Temp += sizeof(uint32);
                        u16ReadIdx += sizeof(uint32);
                        *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx);
                        u16ReadIdx++;
                        uint8 u8Index;
                        for (u8Index=0u; u8Index < *(pu8RxBuffer+17); u8Index++)
                        {
                            (void)ZBmemcpy(pu8Temp, (pu8RxBuffer+u16ReadIdx), sizeof(uint32));
                            u16ReadIdx += sizeof(uint32);
                            pu8Temp += sizeof(uint32);
                        }
                        *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx++);
                        *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx++);
                        (void)ZBmemcpy(pu8Temp, (pu8RxBuffer+u16ReadIdx), sizeof(uint32));
                        pu8Temp += sizeof(uint32);
                        u16ReadIdx += sizeof(uint32);
                        (void)ZBmemcpy(pu8Temp, (pu8RxBuffer+u16ReadIdx), sizeof(uint32));
                        pu8Temp += sizeof(uint32);
                        u16ReadIdx += sizeof(uint32);
                        (void)ZBmemcpy(pu8Temp, (pu8RxBuffer+u16ReadIdx), sizeof(uint32));
                        pu8Temp += sizeof(uint32);
                        u16ReadIdx += sizeof(uint32);
                        (void)ZBmemcpy(pu8Temp, (pu8RxBuffer+u16ReadIdx), sizeof(uint32));
                        pu8Temp += sizeof(uint32);
                        u16ReadIdx += sizeof(uint32);
                        (void)ZBmemcpy(pu8Temp, (pu8RxBuffer+u16ReadIdx), sizeof(uint32));
                        pu8Temp += sizeof(uint32);
                        u16ReadIdx += sizeof(uint32);
                        (void)ZBmemcpy(pu8Temp, (pu8RxBuffer+u16ReadIdx), sizeof(uint32));
                        u16ReadIdx += sizeof(uint32);
                    }
                    break;
                }
                case (uint16)E_SL_MSG_FIND_EXT_ADDR:
                {
                    if(NULL != pu8Temp)
                    {
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+7);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+6);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+5);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+4);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+3);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+2);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+1);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX);
                        u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 8U;
                    }
                    break;
                }
                case (uint16)E_SL_MSG_GET_DEVICE_PERMISSIONS:
                case (uint16)E_SL_MSG_GET_SEC_MAT_SET_SIZE:
                case (uint16)E_SL_MSG_GET_LAST_RSSI:
                case (uint16)E_SL_MSG_FRAGMENTED_PACKET_CHECK:
                {
                    u16ReadIdx = SL_MSG_RSP_START_IDX;
                    if(NULL != pu8Temp)
                    {
                        *pu8Temp = *(pu8RxBuffer+u16ReadIdx++);
                    }
                    break;
                }
                case (uint16)E_SL_MSG_GET_EXT_PANID:
                case (uint16)E_SL_MSG_GET_EXT_ADDR:
                case (uint16)E_SL_MSG_GET_TC_ADDRESS:
                case (uint16)E_SL_MSG_GET_MAPPED_IEEE_ADDR:
                case (uint16)E_SL_MSG_GET_USE_EXT_PANID:
                {
                    if(NULL != pu8Temp)
                    {
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+7);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+6);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+5);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+4);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+3);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+2);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+1);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX);
                        u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 8U;
                    }
                    break;
                }
                case (uint16)E_SL_MSG_GET_SHORT_PANID:
                case (uint16)E_SL_MSG_GET_SHORT_ADDRESS_OF_DEVICE:
                case (uint16)E_SL_MSG_GET_NWK_ADDR:
                case (uint16)E_SL_MSG_READ_JN_TEMP_VALUE:
                case (uint16)E_SL_MSG_GET_SECURITY_TIMEOUT:
                {
                    if(NULL != pu8Temp)
                    {
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+1);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX);
                        u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 2U;
                    }
                    break;
                }
                case (uint16)E_SL_MSG_GET_DEVICE_STATS:
                case (uint16)E_SL_MSG_SERIAL_LINK_GET_NWK_STATE:
                    if(NULL != pu8Temp)
                    {
                        u16ReadIdx = SL_MSG_RSP_START_IDX;
                        *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx++);
                        *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx++);

                    }
                    break;

                case (uint16)E_SL_MSG_GET_CURRENT_CHANNEL:
                case (uint16)E_SL_MSG_GET_NWK_KEY:
                {
                    u16ReadIdx = SL_MSG_RSP_START_IDX;
                    *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx++);
                    break;
                }
                case (uint16)E_SL_MSG_GET_PERMIT_JOIN:
                case (uint16)E_SL_MSG_GET_ENDPOINT_STATE:
                case (uint16)E_SL_MSG_IS_DEVICE_KEY_PRESENT:
                case (uint16)E_SL_MSG_GET_CLUSTER_DISCOVERY_STATE:
                case (uint16)E_SL_MSG_GET_USE_INSECURE_JOIN:
                case (uint16)E_SL_MSG_GET_APS_SEQ_NUM:
                case (uint16)E_SL_MSG_GET_FRAGMENTATION_SUPPORT:
                case (uint16)E_SL_MSG_GET_MAX_PAYLOAD_SIZE:
                case (uint16)E_SL_MSG_IS_COPROCESSOR_NEW_MODULE:
                case (uint16)E_SL_MSG_GET_TRSP_KEY_DECIDER_TABLE_MAX_SIZE:
                case (uint16)E_SL_MSG_GET_TRSP_KEY_DECIDER_TABLE_POLICY:
                case (uint16)E_SL_MSG_GET_GROUP_TABLE_ENTRY_ENDPOINT_ELEMENT:
                case (uint16)E_SL_MSG_GET_NUMBER_OF_NWK_DESCRIPTORS:
                case (uint16)E_SL_MSG_FIND_BIND_ENTRY_FOR_CLUSTER_ID:
                case (uint16)E_SL_MSG_SEARCH_EXT_PANID:
                case (uint16)E_SL_MSG_FORM_DISTRIBUTED_NETWORK:
                {
                    u16ReadIdx = SL_MSG_RSP_START_IDX;
                    if(NULL != pu8Temp)
                    {
                        *pu8Temp = *(pu8RxBuffer+u16ReadIdx++);
                    }
                    break;
                }

                case (uint16)E_SL_MSG_GET_BINDING_ENTRY:
                {
                    ZPS_tsAplApsmeBindingTableEntry *psEntry =
                            (ZPS_tsAplApsmeBindingTableEntry*)pvData;
                    u16ReadIdx = SL_MSG_RSP_START_IDX;

                    /* check status */
                    if((NULL != pvData) &&
                       (u8Status == ZPS_E_SUCCESS))
                    {
                        psEntry->u8DstAddrMode = *(pu8RxBuffer+u16ReadIdx++);

                        if(psEntry->u8DstAddrMode == (uint8)ZPS_E_ADDR_MODE_IEEE)
                        {
                            psEntry->uDstAddress.u64Addr = ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 56U;
                            psEntry->uDstAddress.u64Addr += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 48U;
                            psEntry->uDstAddress.u64Addr += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 40U;
                            psEntry->uDstAddress.u64Addr += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 32U;
                            psEntry->uDstAddress.u64Addr += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 24U;
                            psEntry->uDstAddress.u64Addr += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 16U;
                            psEntry->uDstAddress.u64Addr += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                            psEntry->uDstAddress.u64Addr += ((uint64)*(pu8RxBuffer+u16ReadIdx++));
                        }
                        else
                        {
                            psEntry->uDstAddress.u16Addr = ((uint16)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                            psEntry->uDstAddress.u16Addr += *(pu8RxBuffer+u16ReadIdx++);
                        }

                        psEntry->u16ClusterId = ((uint16)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                        psEntry->u16ClusterId += *(pu8RxBuffer+u16ReadIdx++);

                        psEntry->u8SourceEndpoint = *(pu8RxBuffer+u16ReadIdx++);
                        psEntry->u8DestinationEndPoint = *(pu8RxBuffer+u16ReadIdx++);
                    }

                    break;
                }
                case (uint16)E_SL_MSG_GET_KEY_TABLE_ENTRY:
                {
                    ZPS_tsAplApsKeyDescriptorEntry *psEntry = (ZPS_tsAplApsKeyDescriptorEntry*)pvData;
                    u16ReadIdx = SL_MSG_RSP_START_IDX;

                    /* check status */
                    if((NULL != pvData) && (u8Status == ZPS_E_SUCCESS))
                    {
                        psEntry->u16ExtAddrLkup = ((uint16)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                        psEntry->u16ExtAddrLkup += *(pu8RxBuffer+u16ReadIdx++);

                        (void)ZBmemcpy(psEntry->au8LinkKey,pu8RxBuffer+u16ReadIdx,ZPS_SEC_KEY_LENGTH);
                        u16ReadIdx += ZPS_SEC_KEY_LENGTH;
                        psEntry->u32OutgoingFrameCounter  = ((uint32)*(pu8RxBuffer+u16ReadIdx++)) << 24U;
                        psEntry->u32OutgoingFrameCounter += ((uint32)*(pu8RxBuffer+u16ReadIdx++)) << 16U;
                        psEntry->u32OutgoingFrameCounter += ((uint32)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                        psEntry->u32OutgoingFrameCounter += ((uint32)*(pu8RxBuffer+u16ReadIdx++));

                        /* pu32IncomingFrameCounter and  u8BitMapSecLevl (in total 5 bytes) are received in 
                        the payload but not used, so increase read index by their size to avoid mismatch warning */
                        u16ReadIdx += 5;
                    }
                    break;
                }
                case (uint16)E_SL_MSG_GET_TRSP_KEY_DECIDER_TABLE_ENTRY:
                {
#if (BDB_SET_DEFAULT_TC_POLICY == TRUE)
                    BDB_tsTrspKeyDeciderEntry *psEntry = (BDB_tsTrspKeyDeciderEntry*)pvData;
                    u16ReadIdx = SL_MSG_RSP_START_IDX;

                    /* check status */
                    if((NULL != pvData) && (u8Status == BDB_E_SUCCESS))
                    {
                        (void)ZBmemcpy(psEntry, pu8RxBuffer+u16ReadIdx,sizeof(BDB_tsTrspKeyDeciderEntry));
                        u16ReadIdx += sizeof(BDB_tsTrspKeyDeciderEntry);
                    }
#endif
                    break;
                }
                case (uint16)E_SL_MSG_GET_DEFAULT_TC_APS_LINK_KEY:
                case (uint16)E_SL_MSG_GET_DEFAULT_GLOBAL_APS_LINK_KEY:
                case (uint16)E_SL_MSG_GET_DEFAULT_DISTRIBUTED_APS_LINK_KEY:
                {
                    ZPS_tsAplApsKeyDescriptorEntry *psEntry = (ZPS_tsAplApsKeyDescriptorEntry*)pvData;
                    u16ReadIdx = SL_MSG_RSP_START_IDX;

                    /* check status */
                    if((NULL != pvData) && (u8Status == ZPS_E_SUCCESS))
                    {
                        psEntry->u16ExtAddrLkup = ((uint16)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                        psEntry->u16ExtAddrLkup += *(pu8RxBuffer+u16ReadIdx++);

                        (void)ZBmemcpy(psEntry->au8LinkKey,pu8RxBuffer+u16ReadIdx,ZPS_SEC_KEY_LENGTH);
                        u16ReadIdx += ZPS_SEC_KEY_LENGTH;
                        psEntry->u32OutgoingFrameCounter  = ((uint32)*(pu8RxBuffer+u16ReadIdx++)) << 24U;
                        psEntry->u32OutgoingFrameCounter += ((uint32)*(pu8RxBuffer+u16ReadIdx++)) << 16U;
                        psEntry->u32OutgoingFrameCounter += ((uint32)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                        psEntry->u32OutgoingFrameCounter += ((uint32)*(pu8RxBuffer+u16ReadIdx++));
                        psEntry->u8BitMapSecLevl = ((uint8)*(pu8RxBuffer+u16ReadIdx++));
                    }

                    break;
                }
                case (uint16)E_SL_MSG_GET_ADDRESS_MAP_TABLE_ENTRY:
                {
                    u16ReadIdx = SL_MSG_RSP_START_IDX;
                                      /* check status */
                    if((NULL != pvData) && ( u8Status == ZPS_E_SUCCESS))
                    {
                        (void)ZBmemcpy(pvData,pu8RxBuffer+u16ReadIdx,10UL);
                        u16ReadIdx += 10U;

                    }
                    break;
                }
                case (uint16)E_SL_MSG_GET_ROUTING_TABLE_ENTRY:
                {
                    ZPS_tsNwkRtEntry *psEntry = (ZPS_tsNwkRtEntry*)pvData;
                    u16ReadIdx = SL_MSG_RSP_START_IDX;

                    /* check status */
                    if((NULL != pvData) && (u8Status == ZPS_E_SUCCESS))
                    {
                        psEntry->u16NwkDstAddr = ((uint16)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                        psEntry->u16NwkDstAddr += *(pu8RxBuffer+u16ReadIdx++);
                        psEntry->u16NwkNxtHopAddr = ((uint16)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                        psEntry->u16NwkNxtHopAddr += *(pu8RxBuffer+u16ReadIdx++);
                        (void)ZBmemcpy((uint8*)&psEntry->uAncAttrs,pu8RxBuffer+u16ReadIdx,sizeof(psEntry->uAncAttrs));
                        u16ReadIdx += (uint16)(sizeof(psEntry->uAncAttrs));
                    }



                    break;
                }
                case (uint16)E_SL_MSG_GET_SIMPLE_DESCRIPTOR:
                {
                    ZPS_tsAppSimpleDescriptor *psEntry = (ZPS_tsAppSimpleDescriptor*)pvData;
                    static uint16 au16InClusterList[MAX_SD_IN_CL_COUNT], au16OutClusterList[MAX_SD_OUT_CL_COUNT];
                    static uint8 au8InDiscoveryEnabledFlags[MAX_SD_IN_FLAG_COUNT], au8OutDiscoveryEnabledFlags[MAX_SD_OUT_FLAG_COUNT];
                    uint8 u8FlagsByteCount = 0U, u8SkippedInput, u8SkippedOutput;

                    u16ReadIdx = SL_MSG_RSP_START_IDX;
                    if(NULL != pvData)
                    {
                        psEntry->pu16InClusterList = au16InClusterList;
                        psEntry->pu16OutClusterList = au16OutClusterList;
                        psEntry->au8InDiscoveryEnabledFlags = au8InDiscoveryEnabledFlags;
                        psEntry->au8OutDiscoveryEnabledFlags = au8OutDiscoveryEnabledFlags;

                        /* check status */
                        if(*(pu8RxBuffer+SL_MSG_RX_STATUS_IDX) == ZPS_E_SUCCESS)
                        {
                            psEntry->u16ApplicationProfileId = ((uint16)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                            psEntry->u16ApplicationProfileId += *(pu8RxBuffer+u16ReadIdx++);

                            psEntry->u16DeviceId = ((uint16)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                            psEntry->u16DeviceId += *(pu8RxBuffer+u16ReadIdx++);
                            psEntry->u8DeviceVersion = *(pu8RxBuffer+u16ReadIdx++);
                            psEntry->u8Endpoint = *(pu8RxBuffer+u16ReadIdx++);
                            psEntry->u8InClusterCount = *(pu8RxBuffer+u16ReadIdx++);
                            psEntry->u8OutClusterCount = *(pu8RxBuffer+u16ReadIdx++);

                            u8SkippedInput = 0U;
                            for(u8i=0U; u8i<psEntry->u8InClusterCount; u8i++)
                            {
                                if (u8i < MAX_SD_IN_CL_COUNT)
                                {
                                    psEntry->pu16InClusterList[u8i] = ((uint16)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                                    psEntry->pu16InClusterList[u8i] += *(pu8RxBuffer+u16ReadIdx++);
                                }
                                else
                                {
                                    /* overrun the available storage
                                     * skip the data in the rx buffer and
                                     * count skipped cluster to adjust count at the end
                                     */
                                    u16ReadIdx += sizeof(uint16);
                                    u8SkippedInput++;
                                    DBG_vPrintf((bool_t)TRUE, "Get Simple descriptor clipping input cluster list\n");
                                }
                            }
                            u8SkippedOutput = 0U;
                            for(u8i=0U; u8i<psEntry->u8OutClusterCount; u8i++)
                            {
                                if (u8i < MAX_SD_OUT_CL_COUNT)
                                {
                                    psEntry->pu16OutClusterList[u8i] = ((uint16)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                                    psEntry->pu16OutClusterList[u8i] += *(pu8RxBuffer+u16ReadIdx++);
                                }
                                else
                                {
                                    /* overrun the available storage
                                     * skip the data in the rx buffer and
                                     * count skipped cluster to adjust count at the end
                                     */
                                    u16ReadIdx += sizeof(uint16);
                                    u8SkippedOutput++;
                                    DBG_vPrintf((bool_t)TRUE, "Get Simple descriptor clipping output cluster list\n");
                                }
                            }

                            u8FlagsByteCount = (uint8)(psEntry->u8InClusterCount/8U);
                            if (0U != (psEntry->u8InClusterCount % 8U))
                            {
                                u8FlagsByteCount++;
                            }
                            for (u8i=0U; u8i<u8FlagsByteCount; u8i++)
                            {
                                if (u8i < MAX_SD_IN_FLAG_COUNT)
                                {
                                    /* only copy the flag data if we have storage for it */
                                    au8InDiscoveryEnabledFlags[u8i] = *(pu8RxBuffer+u16ReadIdx);
                                }
                                u16ReadIdx++;
                            }

                            u8FlagsByteCount = (uint8)(psEntry->u8OutClusterCount/8U);
                            if (0U != (psEntry->u8OutClusterCount % 8U))
                            {
                                u8FlagsByteCount++;
                            }
                            for (u8i=0U; u8i<u8FlagsByteCount; u8i++)
                             {
                                if (u8i < MAX_SD_OUT_FLAG_COUNT)
                                {
                                    /* only copy the flag data if we have storage for it */
                                    au8OutDiscoveryEnabledFlags[u8i] = *(pu8RxBuffer+u16ReadIdx);
                                }
                                u16ReadIdx++;
                             }

                            /* adjust cluster counts if we skipped any data */
                            psEntry->u8InClusterCount -= u8SkippedInput;
                            psEntry->u8OutClusterCount -= u8SkippedOutput;

                        }
                    }
                    break;
                }
                case (uint16)E_SL_MSG_GET_SEC_MAT_SET_ENTRY:
                {
                    ZPS_tsNwkSecMaterialSet *psEntry = (ZPS_tsNwkSecMaterialSet*)pvData;
                    u16ReadIdx = SL_MSG_RSP_START_IDX;

                    /* check status */
                    if((NULL != pvData) && (u8Status == ZPS_E_SUCCESS))
                    {
                        (void)ZBmemcpy(psEntry->au8Key, pu8RxBuffer+u16ReadIdx, ZPS_NWK_KEY_LENGTH);
                        u16ReadIdx += ZPS_NWK_KEY_LENGTH;
                        psEntry->u8KeySeqNum = *(pu8RxBuffer+u16ReadIdx++);
                        psEntry->u8KeyType = *(pu8RxBuffer+u16ReadIdx++);
                    }
                }
                break;
                case (uint16)E_SL_MSG_GET_MAC_TABLE_ENTRY:
                {
                    uint64*  pu64Address = (uint64*)pvData;
                    u16ReadIdx = SL_MSG_RSP_START_IDX;

                    /* check status */
                    if((NULL != pvData) && (u8Status == ZPS_E_SUCCESS))
                    {
                        *pu64Address = ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 56U;
                        *pu64Address += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 48U;
                        *pu64Address += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 40U;
                        *pu64Address += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 32U;
                        *pu64Address += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 24U;
                        *pu64Address += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 16U;
                        *pu64Address += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                        *pu64Address += ((uint64)*(pu8RxBuffer+u16ReadIdx++));
                    }
                }
                break;

                case (uint16)E_SL_MSG_GET_CHANNELMASK:
                {
                    if(NULL != pu8Temp)
                    {
                        /* get number of channel mask */
                        u16ReadIdx = SL_MSG_RSP_START_IDX;
                        *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx);
                        u16ReadIdx++;

                        uint8 u8Size = (*(pu8RxBuffer + SL_MSG_RSP_START_IDX) > MAX_NUM_OF_CHANNEL_MASK) ?
                                MAX_NUM_OF_CHANNEL_MASK : *(pu8RxBuffer + SL_MSG_RSP_START_IDX);
                        for (uint8 u8Index=0u; u8Index < u8Size; u8Index++)
                        {
                            (void)ZBmemcpy(pu8Temp, (pu8RxBuffer+u16ReadIdx), sizeof(uint32));
                            u16ReadIdx += sizeof(uint32);
                            pu8Temp += sizeof(uint32);
                        }
                    }
                }
                break;

                case (uint16)E_SL_MSG_GET_JN_CHIP_ID:
                case (uint16)E_SL_MSG_GET_BOOTLOADER_VERSION:
                case (uint16)E_SL_MSG_GET_CLEAR_TX_UCAST_BYTE_COUNT:
                case (uint16)E_SL_MSG_GET_CLEAR_RX_UCAST_BYTE_COUNT:
                case (uint16)E_SL_MSG_GET_CLEAR_TX_FAIL_COUNT:
                case (uint16)E_SL_MSG_GET_CLEAR_TX_RETRY_COUNT:
                case (uint16)E_SL_MSG_GET_CLEAR_TX_COUNT:
                case (uint16)E_SL_MSG_SERIAL_LINK_GET_NWK_INFC:
                case (uint16)E_SL_MSG_SERIAL_LINK_GET_OL_REALIGN:
                case (uint16)E_SL_MSG_SERIAL_LINK_GET_OL_CHANNEL:
                case (uint16)E_SL_MSG_GET_NWK_OUTGOING_FRAME_COUNT:
                case (uint16)E_SL_MSG_SERIAL_LINK_GET_STATUS_FLAGS:
                case (uint16)E_SL_MSG_GET_GROUP_TABLE_SIZE:
                case (uint16)E_SL_MSG_MAC_SET_TX_BUFFERS:
                    if(NULL != pu8Temp)
                    {
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+3);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+2);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+1);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX);
                        u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 4U;
                    }
                    break;
                case (uint16)E_SL_MSG_GET_VERSION:
                    if((u8Status != (uint8)E_SL_MSG_STATUS_BUSY) && (NULL != pu8Temp))
                    {
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+3);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+2);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+1);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+4);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+5);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+7);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+6);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+11);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+10);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+9);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+8);
                    }
                    else
                    {
                        DBG_vPrintf((bool_t)TRUE, "JN Busy message rx\r\n");
                    }
                    u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 12U;
                    sSLCommon.bIsSlMsgGetVer = (bool_t)FALSE;
                   break;
                case (uint16)E_SL_MSG_GET_APS_KEY_TABLE_SIZE:
                case (uint16)E_SL_MSG_GET_NEIGHBOR_TABLE_SIZE:
                case (uint16)E_SL_MSG_GET_ADDRESS_MAP_TABLE_SIZE:
                case (uint16)E_SL_MSG_GET_ROUTING_TABLE_SIZE:
                case (uint16)E_SL_MSG_GET_GROUP_TABLE_ENTRY_GROUP_ID:
                    if(NULL != pu8Temp)
                    {
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+1);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX);
                        u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 2U;
                    }
                    break;
                case (uint16)E_SL_MSG_GET_NWK_KEY_TABLE_ENTRY:
                    if(NULL != pvData)
                    {
                        (void)ZBmemcpy(pvData, (pu8RxBuffer+SL_MSG_RSP_START_IDX), 16UL);
                        u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 16U;
                    }
                    break;
                    /* get tsn */
                case (uint16)E_SL_MSG_UNICAST_ACK_DATA_REQ:
                case (uint16)E_SL_MSG_UNICAST_DATA_REQ:
                case (uint16)E_SL_MSG_UNICAST_IEEE_ACK_DATA_REQ:
                case (uint16)E_SL_MSG_UNICAST_IEEE_DATA_REQ:
                case (uint16)E_SL_MSG_BROADCAST_DATA_REQ:
                case (uint16)E_SL_MSG_GROUP_DATA_REQ:
                case (uint16)E_SL_MSG_BOUND_ACK_DATA_REQ:
                case (uint16)E_SL_MSG_BOUND_DATA_REQ:
                case (uint16)E_SL_MSG_MATCH_DESCRIPTOR_REQUEST:
                case (uint16)E_SL_MSG_NODE_DESCRIPTOR_REQUEST:
                case (uint16)E_SL_MSG_SIMPLE_DESCRIPTOR_REQUEST:
                case (uint16)E_SL_MSG_IEEE_ADDRESS_REQUEST:
                case (uint16)E_SL_MSG_NETWORK_ADDRESS_REQUEST:
                case (uint16)E_SL_MSG_POWER_DESCRIPTOR_REQUEST:
                case (uint16)E_SL_MSG_UNBIND:
                case (uint16)E_SL_MSG_BIND:
                case (uint16)E_SL_MSG_ACTIVE_ENDPOINT_REQUEST:
                case (uint16)E_SL_MSG_NETWORK_REMOVE_DEVICE:
                case (uint16)E_SL_MSG_ZDO_PERMIT_JOIN_REQUEST:
                case (uint16)E_SL_MSG_SEND_MGMT_NWK_UNS_ENH_UPDATE_NOTIFY:
                case (uint16)E_SL_MSG_SEND_MGMT_NWK_ENH_UPDATE_REQ:
                case (uint16)E_SL_MSG_INTERPAN_DATA_REQ:
                case (uint16)E_SL_MSG_REMOVE_REMOTE_DEVICE:
                case (uint16)E_SL_MSG_ZDO_BIND:
                case (uint16)E_SL_MSG_ZDO_UNBIND:
                case (uint16)E_SL_MSG_CHANGE_CHANNEL:
                case (uint16)E_SL_MSG_SWITCH_KEY_REQUEST:
                case (uint16)E_SL_MSG_APSDE_DATA_REQUEST:
                case (uint16)E_SL_MSG_ZPS_DEFAULT_STACK:
                case (uint16)E_SL_MSG_ZPS_SET_KEYS:
                case (uint16)E_SL_MSG_ZPS_SAVE_ALL_RECORDS:
                case (uint16)E_SL_MSG_NWK_NIB_CLEAR_TABLES:
                case (uint16)E_SL_MSG_NWK_CLEAR_MAT_SET:
                case (uint16)E_SL_MSG_RESET_DATA_STRUCTURES:
                case (uint16)E_SL_MSG_SET_IGNORE_PROFILE_CHECK:
                case (uint16)E_SL_MSG_REGISTER_INTERPAN_FILTER:
                    if(NULL != pvData)
                    {
                        *(uint8*)pvData = *(pu8RxBuffer+SL_MSG_TSN_IDX);
                    }
   #ifdef MONITOR_API_ERRORS
                          vMonitorJnErrors(u8Status, (bool_t)FALSE);
   #endif
                    u16ReadIdx = SL_MSG_RSP_START_IDX;
                    break;
                case (uint16)E_SL_MSG_GET_GLOBAL_STATS:
                    if(NULL != pu8Temp)
                    {
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+3);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+2);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+1);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX);

                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+7);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+6);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+5);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+4);

                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+11);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+10);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+9);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+8);
                        u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 12U;
                    }
                  break;
                case (uint16)E_SL_MSG_GET_RESTORE_POINT:
                     if(NULL != pvData)
                     {
                         u16ReadIdx = SL_MSG_RSP_START_IDX;
                         ZPS_tsAfRestorePointStruct *psRestore = (ZPS_tsAfRestorePointStruct*)pvData;

                         (void)ZBmemcpy( psRestore->psActvNtEntry, pu8RxBuffer+u16ReadIdx, sizeof(zps_tsPersistNeighboursTable));
                         u16ReadIdx += sizeof(zps_tsPersistNeighboursTable);

                         (void)ZBmemcpy( psRestore->psSecMatSet[0].au8Key, pu8RxBuffer+u16ReadIdx, 16UL );
                         u16ReadIdx += 16U;
                         psRestore->psSecMatSet[0].u8KeySeqNum = *(pu8RxBuffer+u16ReadIdx++);
                         psRestore->psSecMatSet[0].u8KeyType = *(pu8RxBuffer+u16ReadIdx++);

                         (void)ZBmemcpy( psRestore->psSecMatSet[1].au8Key, pu8RxBuffer+u16ReadIdx, 16UL );
                         u16ReadIdx += 16U;
                         psRestore->psSecMatSet[1].u8KeySeqNum = *(pu8RxBuffer+u16ReadIdx++);
                         psRestore->psSecMatSet[1].u8KeyType = *(pu8RxBuffer+u16ReadIdx++);

                         (void)ZBmemcpy( &psRestore->sPersist, pu8RxBuffer+u16ReadIdx, sizeof(ZPS_tsNWkNibPersist));
                         u16ReadIdx += sizeof(ZPS_tsNWkNibPersist);
                         (void)ZBmemcpy( &psRestore->u64TcAddress, pu8RxBuffer+u16ReadIdx, sizeof(uint64));
                         u16ReadIdx += sizeof(uint64);
                         (void)ZBmemcpy( &psRestore->u32OutgoingFrameCounter, pu8RxBuffer+u16ReadIdx, sizeof(uint32));
                         u16ReadIdx += sizeof(uint32);
                         psRestore->u8KeyType = *(pu8RxBuffer+u16ReadIdx++);
                     }
                    break;

                case (uint16)E_SL_MSG_GET_MAC_ADDR_TABLE:
                {
                    uint8 u8MacAddrTableSize;
                    u16ReadIdx = SL_MSG_RSP_START_IDX;
                    u8MacAddrTableSize = pu8RxBuffer[u16ReadIdx];
                     if(NULL != pvData)
                     {
                         (void)ZBmemcpy(pvData, (pu8RxBuffer+u16ReadIdx), (((uint32)sizeof(uint64) * (uint32)u8MacAddrTableSize) + 1U));
                         u16ReadIdx += (uint16)((uint16)sizeof(uint64) * (uint16)u8MacAddrTableSize) + 1U;
                     }
                }
                break;
                case (uint16)E_SL_MSG_CONVERT_ENERGY_TO_DBM:
                {
                    if(NULL != pu8Temp)
                    {
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+1);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX);
                        u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 2U;
                    }
                    break;
                }
                case (uint16)E_SL_MSG_NWK_MANAGER_STATE:
                {
                    if(NULL != pu8Temp)
                    {
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX);
                        u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 1U;
                    }
                    break;
                }
                case (uint16)E_SL_MSG_SERIAL_LINK_REQ_NEGATIVE:
                {
                    if(NULL != pvData)
                    {
                        u16ReadIdx = SL_MSG_RSP_START_IDX;
                        (void)ZBmemcpy((uint8*)pvData, pu8RxBuffer+u16ReadIdx, (uint32)u16RxLength-(uint32)u16ReadIdx);
                        *(((uint8*)pvData)+u16RxLength-u16ReadIdx) = 0U;
                        u16ReadIdx += (u16RxLength-u16ReadIdx);

                    }
                }
                break;
                case (uint16)E_SL_MSG_GET_MAC_TYPE:
                    if(NULL != pu8Temp)
                    {
                        u16ReadIdx = SL_MSG_RSP_START_IDX;
                        if ( u8Status == ZPS_E_SUCCESS )
                        {
                            *pu8Temp = *(pu8RxBuffer+u16ReadIdx++);
                        }
                    }
                    sSLCommon.bIsSlMsgGetVer = (bool_t)FALSE;
                  break;
                case (uint16)E_SL_MSG_GET_JN_PDMUSE:
                    if(NULL != pu8Temp)
                    {
                        u16ReadIdx = SL_MSG_RSP_START_IDX;
                        int i;
                        if ( u8Status == ZPS_E_SUCCESS )
                        {
                            for (i=0; i<16; i++)
                            {
                                *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx+3);
                                *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx+2);
                                *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx+1);
                                *pu8Temp++ = *(pu8RxBuffer+u16ReadIdx);
                                u16ReadIdx += 4U;

                            }
                        }
                    }
                    break;

                case (uint16)E_SL_MSG_SERIAL_LINK_COUNT_APDU:
                {
                    if(NULL != pu8Temp)
                    {
                        u16ReadIdx = SL_MSG_RSP_START_IDX;
                        if ( u8Status == ZPS_E_SUCCESS )
                        {
                            *pu8Temp = *(pu8RxBuffer+u16ReadIdx++);
                            *(pu8Temp+1) = *(pu8RxBuffer+u16ReadIdx++);
                            *(pu8Temp+2) = *(pu8RxBuffer+u16ReadIdx++);
                            *(pu8Temp+3) = *(pu8RxBuffer+u16ReadIdx++);
                        }
                    }
                }
                break;

                case (uint16)E_SL_MSG_GET_APS_FC_IEEE:
                {
                    if(NULL != pu8Temp)
                    {
                        /* the out frame counter */
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+3U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+2U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+1U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX);

                        /* the in frame counter */
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+7U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+6U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+5U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+4U);

                        u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 8U;
                    }
                }
                break;
                case (uint16)E_SL_MSG_GET_APS_SERIAL_STATS:
                {
                    u16ReadIdx = SL_MSG_RSP_START_IDX;

                    if(NULL != pu8Temp)
                    {
                        (void)ZBmemcpy( pu8Temp, (pu8RxBuffer+u16ReadIdx), (11U*sizeof(uint32)) );
                        u16ReadIdx += (11U*sizeof(uint32));
                    }
                }
                break;
                case (uint16)E_SL_MSG_GET_POLL_STATE:
                {
                    u16ReadIdx = SL_MSG_RSP_START_IDX;

                    if(NULL != pu8Temp)
                    {
                        /* the poll time interval */
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+3U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+2U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+1U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX);

                        /* the time to next poll */
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+7U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+6U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+5U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+4U);

                        /* total Polls, Polls Remaining, timer Status */
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+8U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+9U);
                        *pu8Temp++ = *(pu8RxBuffer+SL_MSG_RSP_START_IDX+10U);

                        u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 11U;
                    }
                }
                break;

                case (uint16)E_SL_MSG_GET_REPROVISSION_DATA:
                {
                    if(NULL != pvData)
                    {
                        tsReprovissionData *psRpData = (tsReprovissionData*)pvData;
                        u16ReadIdx = SL_MSG_RSP_START_IDX;
                        if ( u8Status == ZPS_E_SUCCESS )
                        {
                            (void)ZBmemcpy(psRpData->au8Key  ,pu8RxBuffer+u16ReadIdx,ZPS_SEC_KEY_LENGTH);
                            u16ReadIdx += ZPS_SEC_KEY_LENGTH;

                            psRpData->u32InFrameCtr  = ((uint32)*(pu8RxBuffer+u16ReadIdx++)) << 24U;
                            psRpData->u32InFrameCtr += ((uint32)*(pu8RxBuffer+u16ReadIdx++)) << 16U;
                            psRpData->u32InFrameCtr += ((uint32)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                            psRpData->u32InFrameCtr += ((uint32)*(pu8RxBuffer+u16ReadIdx++));

                            psRpData->u32OutFrameCtr  = ((uint32)*(pu8RxBuffer+u16ReadIdx++)) << 24U;
                            psRpData->u32OutFrameCtr += ((uint32)*(pu8RxBuffer+u16ReadIdx++)) << 16U;
                            psRpData->u32OutFrameCtr += ((uint32)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                            psRpData->u32OutFrameCtr += ((uint32)*(pu8RxBuffer+u16ReadIdx++));

                            psRpData->u8Permissions = ((uint8)*(pu8RxBuffer+u16ReadIdx++));
                        }
                    }
                }
                break;

                case (uint16)E_SL_MSG_GET_NWK_DESCRIPTOR:
                    if(NULL != pvData)
                    {
                        ZPS_tsNwkNetworkDescr *psNwkDesc = (ZPS_tsNwkNetworkDescr*)pvData;
                        u16ReadIdx = SL_MSG_RSP_START_IDX;
                        if ( u8Status == ZPS_E_SUCCESS )
                        {
                            psNwkDesc->u64ExtPanId = ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 56U;
                            psNwkDesc->u64ExtPanId += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 48U;
                            psNwkDesc->u64ExtPanId += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 40U;
                            psNwkDesc->u64ExtPanId += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 32U;
                            psNwkDesc->u64ExtPanId += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 24U;
                            psNwkDesc->u64ExtPanId += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 16U;
                            psNwkDesc->u64ExtPanId += ((uint64)*(pu8RxBuffer+u16ReadIdx++)) << 8U;
                            psNwkDesc->u64ExtPanId += ((uint64)*(pu8RxBuffer+u16ReadIdx++));

                            psNwkDesc->u8LogicalChan = ((uint8)*(pu8RxBuffer+u16ReadIdx++));
                            psNwkDesc->u8StackProfile = ((uint8)*(pu8RxBuffer+u16ReadIdx++));
                            psNwkDesc->u8ZigBeeVersion = ((uint8)*(pu8RxBuffer+u16ReadIdx++));
                            psNwkDesc->u8PermitJoining = ((uint8)*(pu8RxBuffer+u16ReadIdx++));
                            psNwkDesc->u8RouterCapacity = ((uint8)*(pu8RxBuffer+u16ReadIdx++));
                            psNwkDesc->u8EndDeviceCapacity = ((uint8)*(pu8RxBuffer+u16ReadIdx++));
#ifdef WWAH_SUPPORT
                            psNwkDesc->u8ParentCapacity = ((uint8)*(pu8RxBuffer+u16ReadIdx++));
#endif
                        }
                    }
                break;

                case (uint16)E_SL_MSG_FIND_KEY_DESCRIPTOR:
                    if(NULL != pvData)
                    {
                        u16ReadIdx = SL_MSG_RSP_START_IDX;
                        if ( u8Status == ZPS_E_SUCCESS )
                        {
                            /* Copy u32OutgoingFrameCounter */
                            (void)ZBmemcpy(pu8Temp, &pu8RxBuffer[u16ReadIdx], sizeof(uint32));
                            pu8Temp += sizeof(uint32);
                            u16ReadIdx += sizeof(uint32);

                            /* Copy u16ExtAddrLkup*/
                            (void)ZBmemcpy(pu8Temp, &pu8RxBuffer[u16ReadIdx], sizeof(uint16));
                            pu8Temp += sizeof(uint16);
                            u16ReadIdx += sizeof(uint16);
                             
                            /* Copy au8LinkKey */
                            (void)ZBmemcpy(pu8Temp, &pu8RxBuffer[u16ReadIdx], ZPS_SEC_KEY_LENGTH);
                            pu8Temp += ZPS_SEC_KEY_LENGTH;
                            u16ReadIdx += ZPS_SEC_KEY_LENGTH;

                            /* Copy u8BitMapSecLevl */
                            *pu8Temp++ = pu8RxBuffer[u16ReadIdx];
                            u16ReadIdx ++;

                            /* Copy u32Index */
                            (void)ZBmemcpy(pu8Temp, &pu8RxBuffer[u16ReadIdx], sizeof(uint32));
                            pu8Temp += sizeof(uint32);
                            u16ReadIdx += sizeof(uint32);
                        }
                    }
                break;

                case (uint16)E_SL_MSG_CONVERT_LQI_TO_RSSI_DBM:
                {
                    if(NULL != pu8Temp)
                    {
                        u16ReadIdx = SL_MSG_RSP_START_IDX;
                        if (u8Status == ZPS_E_SUCCESS)
                        {
                            *pu8Temp++ =  *(pu8RxBuffer+u16ReadIdx + 1);
                            *pu8Temp++ =  *(pu8RxBuffer+u16ReadIdx);

                            u16ReadIdx = (uint16)SL_MSG_RSP_START_IDX + 2U;
                        }
                    }
                }
                break;

                default:
                    DBG_vPrintf((bool_t)TRUE, "Unhandled Command response for %04x\n", u16SentPktType);
                    break;
            }

            if ((u16ReadIdx != u16RxLength) &&
                ( u8Status != (uint8)E_SL_MSG_STATUS_UNSUPPORTED_COMMAND) &&
                ( u8Status != (uint8)E_SL_MSG_STATUS_ILLEGAL_REQUEST) )
            {
                DBG_vPrintf((bool_t)TRUE, "WARNING: Pkt type %04x Read %d bytes Received %d bytes\n",
                        u16SentPktType, u16ReadIdx, u16RxLength);

            }

            sSLCommon.bIsWaitingStatus = (bool_t)FALSE;
            vSL_GiveSerialSemaphore();

            vSL_FreeRxBuffer(pu8RxBuffer);
            return u8Status;
        }
        else
        {
            sSLCommon.bIsWaitingStatus = (bool_t)FALSE;
            vSL_GiveSerialSemaphore();
            vSL_FreeRxBuffer(pu8RxBuffer);
            DBG_vPrintf((bool_t)TRUE, "SERIAL TX: Response type (%04x) did not mach sent type (%04x)\n", u16Type, u16SentPktType);
            return (uint8)E_SL_MSG_STATUS_INCORRECT_PARAMETERS;
        }
    }

    sSLCommon.bIsWaitingStatus = (bool_t)FALSE;
    vSL_GiveSerialSemaphore();
    if((pu8RxBuffer != (void*)0U))
    {
        /* coverity[dead_error_line] defensive coding*/
        vSL_FreeRxBuffer(pu8RxBuffer);
    }
    if(u16Type != (uint16)E_SL_MSG_GET_VERSION)
    {
        FUNC_RESET(FALSE);
    }
    return (uint8)E_SL_MSG_STATUS_TIME_OUT;
}

/********************************************************************************
  *
  * @fn PUBLIC bool_t bSL_ValidateIncomingMessage
  *
  */
 /**
  *
  * @param pu8RxSerialBuffer  R Recived data buffer
  *
  *
  * @brief Validate received message
  *
  * @return TRUE if a valid message has been received
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC bool_t bSL_ValidateIncomingMessage(uint8 *pu8RxSerialBuffer)
{
    uint8 u8CRC;
    uint8 u8CRCRxFrame=0U;
    uint16 u16Length, u16Type;
    uint8 u8TxSeqNo, u8RxSeqNo;

    union
    {
        uint8*  pu8;
        uint16* pu16;
    }
    upBuf;

    upBuf.pu8 = pu8RxSerialBuffer + 1U;
    u16Type = *upBuf.pu16;

    upBuf.pu8 = pu8RxSerialBuffer + 3U;
    u16Length = *upBuf.pu16;
    u8TxSeqNo = *(pu8RxSerialBuffer + SL_MSG_TX_SEQ_NO_IDX);
    u8RxSeqNo = *(pu8RxSerialBuffer + SL_MSG_RX_SEQ_NO_IDX);
    u8CRC = *(pu8RxSerialBuffer + SL_MSG_RX_CRC__IDX);

#ifdef SL_HOST_TO_COPROCESSOR_SECURE
    if(FALSE == bSL_SecureDecryptMessage(pu8RxSerialBuffer, &u16Length))
    {
        DBG_vPrintf((bool_t)TRUE, "Decryption FAIL\n");
        return FALSE;
    }
#endif
    u8CRCRxFrame = u8SL_CalculateCRC(u16Type, u16Length, u8TxSeqNo, u8RxSeqNo, pu8RxSerialBuffer+SL_MSG_DATA_START_IDX);

    if(u8CRC == u8CRCRxFrame )
    {
        /* CRC matches - valid packet */
        DBG_vPrintf((bool_t)DEBUG_SL, "bSL_ReadMessage(%d, %d, %02x), u8CRCRxFrame = %d\n", u16Type, u16Length, u8CRC, u8CRCRxFrame);

        if(u8CRC == SL_MSG_TYPE_APDU_PROCESSED)
        {
            pu8RxSerialBuffer[5] = 0U;
        }
        return((bool_t)TRUE);
    }

    /* Bad CRC */
    /* State Machine is reset by the callee */
    /* receive buffer is freed by the callee */

    DBG_vPrintf((bool_t)TRUE, "SERIAL RX: Bad CRC on Received Packet, resetting Rx State machine\n");

    return (bool_t)FALSE;
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_TxByte
  *
  */
 /**
  *
  * @param bSpecialCharacter  R TRUE if this byte should not be escaped
  *
  * @param u8Data  R Character to send
  *
  *
  * @brief Send, escaping if required, a byte to the serial link
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_TxByte(bool_t bSpecialCharacter, uint8 u8Data)
{
    if(!bSpecialCharacter && u8Data < 0x8U)
    {
        /* Send escape character and escape byte */
        u8Data ^= 0x8U;
        SL_WRITE(SL_ESC_CHAR);
    }
    SL_WRITE(u8Data);
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_FreeRxBuffer
  *
  */
 /**
  *
  * @param pu8RxBuffer  R Rx Buffer
  *
  *
  * @brief Release Rx buffer to pool
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_FreeRxBuffer(uint8 *pu8RxBuffer)
{
    uint8 u8i;
    bool_t bFreed = (bool_t)FALSE;

    vSL_TaskEnterCritical();
    for(u8i=0U;u8i<MAX_RX_LARGE_SERIAL_BUFFERS;u8i++)
    {
        if (pu8RxBuffer == &sSLCommon.au8RxLargeSerialBuffer[u8i][0] )
        {
            sSLCommon.au8RxLargeSerialBuffer[u8i][0] = 0U;
            if (u16SmallBufferAllocated > 0U)
            {
                u16SmallBufferAllocated--;
            }
            bFreed = (bool_t)TRUE;
            break;
        }
    }
    if (bFreed == (bool_t)FALSE)
    {
        for(u8i=0U;u8i<MAX_RX_SMALL_SERIAL_BUFFERS;u8i++)
        {
            if (pu8RxBuffer == &sSLCommon.au8RxSmallSerialBuffer[u8i][0])
            {
                sSLCommon.au8RxSmallSerialBuffer[u8i][0] = 0U;
                if (u16SmallBufferAllocated > 0U)
                {
                    u16SmallBufferAllocated--;
                }
                bFreed = (bool_t)TRUE;
                break;
            }
        }
    }
    vSL_TaskExitCritical();

    if (bFreed == (bool_t)FALSE)
    {
        DBG_vPrintf((bool_t)TRUE, "  Free failed bad address %p\n", pu8RxBuffer);
    }
    else
    {
        DBG_vPrintf((bool_t)TRACE_GET_FREE, " Free buf %p\n", pu8RxBuffer);
    }
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_ResetRxBufferPool
  *
  */
 /**
  *
  *
  * @brief Reset Rx Buffer pool
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_ResetRxBufferPool(void)
{
    uint8 u8i;
    DBG_vPrintf((bool_t)TRUE, "Reset Rx Buffer Pool\n");
    vSL_TaskEnterCritical();
    for(u8i=0U;u8i<MAX_RX_LARGE_SERIAL_BUFFERS;u8i++)
    {
        sSLCommon.au8RxLargeSerialBuffer[u8i][0] = 0U;
    }
    for(u8i=0U;u8i<MAX_RX_SMALL_SERIAL_BUFFERS;u8i++)
    {
        sSLCommon.au8RxSmallSerialBuffer[u8i][0] = 0U;
    }
    /* reset counts */
    u16SmallBufferAllocated = 0u;
    u16MaxSmallBufferAllocated = 0u;
    u16LargeBufferAllocated = 0u;
    u16MaxLargeBufferAllocated = 0u;
    vSL_TaskExitCritical();
    bPrinted = (bool_t)FALSE;
}


/********************************************************************************
  *
  * @fn PUBLIC void vSL_PrintRxBufferPool
  *
  */
 /**
  *
  * @param bPrint bool
  *
  * @brief Print Rx Buffer pool
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_PrintRxBufferPool(bool bPrint)
{
    uint8 u8i;
    uint8 u8BuffersAllocated = 0U;
    uint16 u16SubType;
    if (bPrinted == (bool_t)TRUE)
    {
        DBG_vPrintf((bool_t)TRUE, "Already Printed\n");
        return;
    }

    union
    {
        uint8*  pu8;
        uint16* pu16;
    }
    upBuf;

    vSL_TaskEnterCritical();
    DBG_vPrintf((bool_t)TRUE, "Printing Serial buffer \n");
    for(u8i=0U;u8i<MAX_RX_LARGE_SERIAL_BUFFERS;u8i++)
    {
        if(sSLCommon.au8RxLargeSerialBuffer[u8i][0] != 0u)
        {
            upBuf.pu8 = &sSLCommon.au8RxLargeSerialBuffer[u8i][1];
            u16SubType = (uint16)sSLCommon.au8RxLargeSerialBuffer[u8i][SL_MSG_RSP_TYPE_MSB_IDX] << 8U;
            u16SubType += (uint16)sSLCommon.au8RxLargeSerialBuffer[u8i][SL_MSG_RSP_TYPE_LSB_IDX];
            DBG_vPrintf((bool_t)TRUE, "larger buffer index = %d type = 0x%4x SubType %04x \n",u8i, *upBuf.pu16, u16SubType);

            u8BuffersAllocated++;
        }
    }
    if(bPrint)
    {
        DBG_vPrintf((bool_t)TRUE, "Number of Serial Large Buffers in use now = %d\n", u8BuffersAllocated);
    }

    u8BuffersAllocated = 0U;
    for(u8i=0U;u8i<MAX_RX_SMALL_SERIAL_BUFFERS;u8i++)
    {
        if(sSLCommon.au8RxSmallSerialBuffer[u8i][0] != 0u)
        {
            upBuf.pu8 = &sSLCommon.au8RxSmallSerialBuffer[u8i][1];
            u16SubType = (uint16)sSLCommon.au8RxSmallSerialBuffer[u8i][SL_MSG_RSP_TYPE_MSB_IDX] << 8U;
            u16SubType += (uint16)sSLCommon.au8RxSmallSerialBuffer[u8i][SL_MSG_RSP_TYPE_LSB_IDX];
            DBG_vPrintf((bool_t)TRUE, "Small buffer index = %d type = 0x%4x Subtype %04x\n", u8i, *upBuf.pu16, u16SubType);
            u8BuffersAllocated++;
        }
    }

    if(bPrint)
    {
        DBG_vPrintf((bool_t)TRUE, "Number of Serial Small Buffers in use now  = %d \n  ", u8BuffersAllocated);

    }
    vSL_TaskExitCritical();
    bPrinted = (bool_t)TRUE;
}
/********************************************************************************
  *
  * @fn PUBLIC void Send_Ack_Host_JN
  *
  */
 /**
  *
  *
  * @brief Sends the UART acknowledgment to JN from Host
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
#ifdef ENABLE_UART_ACK_FROM_HOST
PUBLIC void Send_Ack_Host_JN(void)
{
    uint16 u16TxLength = 0U;
    uint8 u8CRC;
    uint8 u8SeqNo = 0U;
    uint8 u8RxSeqNo = 0U;
    u8CRC = u8SL_CalculateCRC((uint16)E_SL_MSG_HOST_JN_ACK, u16TxLength, u8SeqNo, u8RxSeqNo, NULL);
     vSL_TxByte((bool_t)TRUE, SL_START_CHAR);

    /* Send message type */
    vSL_TxByte( (bool_t)FALSE, (uint8)(((uint16)E_SL_MSG_HOST_JN_ACK >> 8U) & 0xffU));
    vSL_TxByte( (bool_t)FALSE, (uint8)(((uint16)E_SL_MSG_HOST_JN_ACK >> 0U) & 0xffU));

    vSL_TxByte( (bool_t)FALSE, (uint8)((u16TxLength >> 8U) & 0xffU));
    vSL_TxByte( (bool_t)FALSE, (uint8)((u16TxLength >> 0U) & 0xffU));

    vSL_TxByte((bool_t)FALSE, u8SeqNo);
    vSL_TxByte((bool_t)FALSE, u8RxSeqNo);

    /* Send message checksum */
    vSL_TxByte((bool_t)FALSE, u8CRC);
    vSL_TxByte((bool_t)TRUE, SL_END_CHAR);
}

/********************************************************************************
  *
  * @fn PUBLIC void Send_Nack_Host_JN
  *
  */
 /**
  *
  *
  * @brief Sends the UART Nack to JN from Host
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void Send_Nack_Host_JN(void)
{
    uint16 u16TxLength = 0U;
    uint8 u8CRC;

    u8CRC = u8SL_CalculateCRC((uint16)E_SL_MSG_HOST_JN_NACK, u16TxLength, 0, 0, NULL);
     vSL_TxByte((bool_t)TRUE, SL_START_CHAR);

    /* Send message type */
    vSL_TxByte((bool_t)FALSE, (uint8)(((uint16)E_SL_MSG_HOST_JN_NACK >> 8U) & 0xffU));
    vSL_TxByte((bool_t)FALSE, (uint8)(((uint16)E_SL_MSG_HOST_JN_NACK >> 0U) & 0xffU));

    vSL_TxByte((bool_t)FALSE, (uint8)((u16TxLength >> 8U) & 0xffU));
    vSL_TxByte((bool_t)FALSE, (uint8)((u16TxLength >> 0U) & 0xffU));

    vSL_TxByte((bool_t)FALSE, 0U);
    vSL_TxByte((bool_t)FALSE, 0U);
    /* Send message checksum */
    vSL_TxByte((bool_t)FALSE, u8CRC);
    vSL_TxByte((bool_t)TRUE, SL_END_CHAR);
}
#endif
/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/********************************************************************************
  *
  * @fn PRIVATE uint8 u8SL_CalculateCRC
  *
  */
 /**
  *
  * @param u16Type uint16
  * @param u16Length uint16
  * @param u8TxSeq uint8
  * @param u8RxSeq uint8
  * @param pu8Data uint8 *
  *
  * @brief Calculate CRC of packet
  *
  * @return CRC of packet
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PRIVATE uint8 u8SL_CalculateCRC(uint16 u16Type, uint16 u16Length, uint8 u8TxSeq, uint8 u8RxSeq, uint8 *pu8Data)
{
    uint16 u16n;
    uint8 u8CRC;

    u8CRC  = (uint8)((uint8)(u16Type   >> 0U) & 0xffU);
    u8CRC ^= (uint8)((uint8)(u16Type   >> 8U) & 0xffU);
    u8CRC ^= (uint8)((uint8)(u16Length >> 0U) & 0xffU);
    u8CRC ^= (uint8)((uint8)(u16Length >> 8U) & 0xffU);
    u8CRC ^= u8TxSeq;
    u8CRC ^= u8RxSeq;

    for(u16n = 0U; u16n < u16Length; u16n++)
    {
        u8CRC ^= pu8Data[u16n];
    }

    return(u8CRC);
}



/********************************************************************************
  *
  * @fn PRIVATE uint8 * pu8SL_GetRxBufferFromLargePool
  *
  */
 /**
  *
  *
  * @brief Get Rx Buffer from pool
  *
  * @return Pointer to Rx Buffer
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PRIVATE uint8 *pu8SL_GetRxBufferFromLargePool(void)
{
    uint8 u8i;
    uint8 *pBuffer;
    vSL_TaskEnterCritical();
    for(u8i=0U;u8i<MAX_RX_LARGE_SERIAL_BUFFERS;u8i++)
    {
        /* if it is occupied then start char should be available at first index */
        if(sSLCommon.au8RxLargeSerialBuffer[u8i][0U] == 0U)
        {
            (void)ZBmemset(&sSLCommon.au8RxLargeSerialBuffer[u8i][0U], 0, MAX_RX_LARGE_SERIAL_BUFFER_SIZE);
            sSLCommon.au8RxLargeSerialBuffer[u8i][0] = SL_START_CHAR;
            pBuffer = &sSLCommon.au8RxLargeSerialBuffer[u8i][0];
            vSL_TaskExitCritical();

            /* pBuffer is always valid, so no need to check for NULL */
            u16LargeBufferAllocated++;
            if(u16MaxLargeBufferAllocated < u16LargeBufferAllocated)
            {
                u16MaxLargeBufferAllocated = u16LargeBufferAllocated;
            }
            DBG_vPrintf((bool_t)TRACE_GET_FREE, "Get lb %p", pBuffer);
            /* re allow printing of buffer pool as buffers are now being allocated */
            bPrinted = (bool_t)FALSE;
            return pBuffer;
        }
    }
#ifdef APP_ENABLE_PRINT_BUFFERS
    vSL_PrintRxBufferPool(FALSE);
#endif
    vSL_TaskExitCritical();
    return NULL;
}

/********************************************************************************
  *
  * @fn PRIVATE uint8 * pu8SL_GetRxBufferFromSmallPool
  *
  */
 /**
  *
  *
  * @brief Get Rx Buffer from pool
  *
  * @return Pointer to Rx Buffer
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PRIVATE uint8 *pu8SL_GetRxBufferFromSmallPool(void)
{
    uint8 u8i;
    uint8 *pBuffer;
    vSL_TaskEnterCritical();
    for(u8i=0U;u8i<MAX_RX_SMALL_SERIAL_BUFFERS;u8i++)
    {
        /* if it is occupied then start char should be available at first index */
        if(sSLCommon.au8RxSmallSerialBuffer[u8i][0U] == 0U)
        {
            (void)ZBmemset(&sSLCommon.au8RxSmallSerialBuffer[u8i][0] , 0x00, MAX_RX_SMALL_SERIAL_BUFFER_SIZE);
            sSLCommon.au8RxSmallSerialBuffer[u8i][0] = SL_START_CHAR;
            pBuffer = &sSLCommon.au8RxSmallSerialBuffer[u8i][0];
            vSL_TaskExitCritical();

            /* pBuffer is always valid, so no need to check for NULL */
            u16SmallBufferAllocated++;
            if(u16MaxSmallBufferAllocated < u16SmallBufferAllocated)
            {
                u16MaxSmallBufferAllocated = u16SmallBufferAllocated;
            }

            DBG_vPrintf((bool_t)TRACE_GET_FREE, "Get sb %p", pBuffer);
            /* re allow printing of buffer pool as buffers are now being allocated */
            bPrinted = (bool_t)FALSE;
            return pBuffer;
        }
    }
#ifdef APP_ENABLE_PRINT_BUFFERS
    vSL_PrintRxBufferPool(TRUE);
#endif
    vSL_TaskExitCritical();
    return NULL;
}

/********************************************************************************
  *
  * @fn PUBLIC uint32 u32SL_GetNumberOfFreeRxBuffers
  *
  */
 /**
  *
  *
  * @brief Get number of free Rx Buffer
  *
  * @return uint32
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint32 u32SL_GetNumberOfFreeRxBuffers(void)
{
    uint8 u8i;
    uint32 u32Count = 0UL;
    vSL_TaskEnterCritical();
    for(u8i=0U;u8i<MAX_RX_SMALL_SERIAL_BUFFERS;u8i++)
    {
        /* if it is occupied then start char should be available at first index */
        if(sSLCommon.au8RxSmallSerialBuffer[u8i][0U] == 0U)
        {
            u32Count++;
        }
    }
    vSL_TaskExitCritical();
    return u32Count;
}

/****************************************************************************/
/********************************************************************************
  *
  * @fn PUBLIC uint16 u16SL_GetMaxLargeBufferAllocated
  *
  */
 /**
  *
  *
  * @brief Get Maximum number of large Rx Buffer Allocated
  *
  * @return uint8 **
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 u16SL_GetMaxLargeBufferAllocated(void)
{
    return u16MaxLargeBufferAllocated;
}
/****************************************************************************/
/********************************************************************************
  *
  * @fn PUBLIC uint16 u16SL_GetMaxSmallBufferAllocated
  *
  */
 /**
  *
  *
  * @brief Get Maximum number of Small Rx Buffer Allocated
  *
  * @return uint8 **
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 u16SL_GetMaxSmallBufferAllocated(void)
{
    return u16MaxSmallBufferAllocated;
}

/****************************************************************************/
/********************************************************************************
  *
  * @fn PUBLIC uint16 u16SL_GetLargeBufferAllocated
  *
  */
 /**
  *
  *
  * @brief Get Maximum number of large Rx Buffer Allocated
  *
  * @return uint8 **
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 u16SL_GetLargeBufferAllocated(void)
{
    return u16LargeBufferAllocated;
}
/****************************************************************************/
/********************************************************************************
  *
  * @fn PUBLIC uint16 u16SL_GetSmallBufferAllocated
  *
  */
 /**
  *
  *
  * @brief Get Maximum number of Small Rx Buffer Allocated
  *
  * @return uint8 **
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint16 u16SL_GetSmallBufferAllocated(void)
{
    return u16SmallBufferAllocated;
}


/****************************************************************************/
/********************************************************************************
  *
  * @fn PRIVATE void vSL_WakeJN
  *
  */
 /**
  *
  * @param u16Length uint16
  * @param bJNInDeepSleep bool_t
  * @param u16Type uint16
  *
  * @brief Sends Wake characters to JN
  *
  * @return None **
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PRIVATE void vSL_WakeJN( uint16 u16Length, bool_t bJNInDeepSleep, uint16 u16Type )
{
}

/****************************************************************************/
/********************************************************************************
  *
  * @fn PUBLIC void vSendTestMsg
  *
  */
 /**
  *
  * @param pu8TxBuffer uint8 *
  * @param u16Length uint16
  *
  * @brief Send raw buffer over rhe seruial link
  *
  * @return None **
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSendTestMsg(uint8* pu8TxBuffer, uint16 u16Length)
{
    uint16 u16n;

    for (u16n = 0U; u16n < u16Length; u16n++)
    {
        SL_WRITE(*pu8TxBuffer++);
    }
}


#ifdef MONITOR_API_ERRORS
/********************************************************************************
  *
  * @fn PUBLIC void vMonitorJnErrors
  *
  */
 /**
  *
  * @param u8Status uint8
  * @param bExtended bool_t
  *
  * @brief Counts API and Extended erros related to trasmission and reception
  *
  * @return None **
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vMonitorJnErrors(uint8 u8Status, bool_t bExtended)
{
    uint8 u8Index = (uint8)E_SL_API_ERROR_COUNT_SIZE;
    uint8 i;
    if (bExtended == (bool_t)FALSE)
    {

        switch (u8Status)
        {
        case (uint8)E_SL_MSG_NO_APDU_BUFFERS:
            u8Index = (uint8)E_SL_API_ERROR_SL_NO_APDU_IDX;
            break;
        case (uint8)ZPS_APL_APS_E_ILLEGAL_REQUEST:
            u8Index = (uint8)E_SL_API_ERROR_SL_A3_IDX;
            break;
        case (uint8)ZPS_APL_APS_E_INVALID_PARAMETER:
            u8Index = (uint8)E_SL_API_ERROR_SL_A6_IDX;
            break;
        case (uint8)ZPS_NWK_ENUM_INVALID_REQUEST:
            u8Index = (uint8)E_SL_API_ERROR_SL_C2_IDX;
            break;
        case (uint8)ZPS_NWK_ENUM_NOT_PERMITTED:
            u8Index = (uint8)E_SL_API_ERROR_SL_C3_IDX;
            break;
        case (uint8)E_SL_MSG_STATUS_C2_SUBSTITUTION:
            u8Index = (uint8)E_SL_API_ERROR_SL_SUBST_IDX;
            break;
        default:
            /* No action required */
            break;
        }
    }
    else
    {

        switch (u8Status)
        {
        case (uint8)ZPS_XS_E_NO_FREE_NPDU:
            u8Index = (uint8)E_SL_API_ERROR_SL_EXT_NPDU_IDX;
            break;
        case (uint8)ZPS_XS_E_NO_FREE_APDU:
            u8Index = (uint8)E_SL_API_ERROR_SL_EXT_APDU_IDX;
            break;
        case (uint8)ZPS_XS_E_NO_FREE_SIM_DATA_REQ:
            u8Index = (uint8)E_SL_API_ERROR_SL_EXT_SDR_IDX;
            break;
        case (uint8)ZPS_XS_E_NO_FREE_APS_ACK:
            u8Index = (uint8)E_SL_API_ERROR_SL_EXT_APS_ACK_IDX;
            break;
        case (uint8)ZPS_XS_E_NO_FREE_FRAG_RECORD:
            u8Index = (uint8)E_SL_API_ERROR_SL_EXT_FRAG_REC_IDX;
            break;
        case (uint8)ZPS_XS_E_NO_FREE_MCPS_REQ:
            u8Index = (uint8)E_SL_API_ERROR_SL_EXT_MCPS_REC_IDX;
            break;
        case (uint8)ZPS_XS_E_NO_FREE_LOOPBACK:
            u8Index = (uint8)E_SL_API_ERROR_SL_EXT_LOOP_IDX;
            break;
        case (uint8)ZPS_XS_E_BAD_PARAM_APSDE_REQ_RSP:
            u8Index = (uint8)E_SL_API_ERROR_SL_EXT_APSDE_IDX;
            break;
        case (uint8)ZPS_XS_E_NO_RT_ENTRY:
            u8Index = (uint8)E_SL_API_ERROR_SL_EXT_RT_IDX;
            break;
        case (uint8)ZPS_XS_E_NO_BTR:
            u8Index = (uint8)E_SL_API_ERROR_SL_EXT_BTR_IDX;
            break;
        default:
            /* No action required */
            break;
        }
    }
    if (u8Index < (uint8)E_SL_API_ERROR_COUNT_SIZE)
    {
        au16ApiErrors[u8Index]++;
        au16ApiErrorTotals[u8Index]++;
        if (au16ApiErrors[u8Index] > MAX_TX_API_ERRORS)
        {
            uint32 u32Flags = u32GetStatusFlagsFromSend();
            vSl_LogJnerrors( u32Flags);
            APP_vNcpHostResetZigBeeModule();
            for (i=0U; i<(uint8)E_SL_API_ERROR_COUNT_SIZE; i++)
            {
                au16ApiErrors[i] = 0U;
            }
        }
    }
    else
    {
        if (u8Status == 0u)
        {
            for (i=0U; i<(uint8)E_SL_API_ERROR_COUNT_SIZE; i++)
            {
                au16ApiErrors[i] = 0u;
            }
        }
    }
}



/********************************************************************************
  *
  * @fn PUBLIC void vSLResetErrorCounts
  *
  */
 /**
  *
  *
  * @brief Rest the API error counts to zero
  *
  * @return PRIVATE void
  *
  * @note
  *
 ********************************************************************************/
PUBLIC void vSLResetErrorCounts(void)
{
    uint8 i;
    for (i=0U; i<(uint8)E_SL_API_ERROR_COUNT_SIZE; i++)
    {
        au16ApiErrors[i] = 0u;
        au16ApiErrorTotals[i] = 0u;
    }
}

#endif

/********************************************************************************
  *
  * @fn PUBLIC void vMonitorAllJnErrors
  *
  */
 /**
  *
  * @param u8Status uint8
  * @param bExtended bool_t
  *
  * @brief Counts errors from the Jn, api and events
  *
  * @return None **
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vMonitorAllJnErrors(uint8 u8Status, bool_t bExtended)
{
    uint32 i;
    bool_t bFound = (bool_t)FALSE;
    if (bExtended == (bool_t)FALSE)
    {
        for (i=0U; i< (sizeof(asErrorLog)/sizeof(tsErrorLog)); i++)
        {
            if ( asErrorLog[i].u8ErrorCode == u8Status)
            {
                asErrorLog[i].u16ErrorCount++;
                asErrorLog[i].u32TotalCount++;
                bFound = (bool_t)TRUE;
                break;
            }
        }
    }
    else
    {
        for (i=0U; i< (sizeof(asExtendedErrorLog)/sizeof(tsErrorLog)); i++)
        {
            if ( asExtendedErrorLog[i].u8ErrorCode == u8Status)
            {
                asExtendedErrorLog[i].u16ErrorCount++;
                asExtendedErrorLog[i].u32TotalCount++;
                bFound = (bool_t)TRUE;
                break;
            }
        }
    }

    if (bFound == (bool_t)FALSE)
    {
        DBG_vPrintf((bool_t)TRUE, "Error code %02x Extended %d not found\r\n", u8Status, bExtended);
    }
}

/********************************************************************************
  *
  * @fn PRIVATE void vHandleRxBufferExhausted
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return PRIVATE void
  *
  * @note
  *
 ********************************************************************************/
PRIVATE void vHandleRxBufferExhausted( void)
{
    /* print the buffers to help trace leaks etc and clues to the back log */
    vSL_PrintRxBufferPool( (bool_t)TRUE);

    /* Reset the buffer pool , freeing them all */
    vSL_ResetRxBufferPool();

    /* as the queues may contain some of the buffers just freed, they
     *  should be flushed to prevent overwriitng */
    /* flush the serial qu */
    vSL_FlushSerialQueue();

    vFlushAppQueue();
    vFlushZclQueue();
}

/********************************************************************************
  *
  * @fn PUBLIC void vGetLpcSerialStats
  *
  */
 /**
  *
  * @param psStats tsLPCSerialStats *
  *
  * @brief void
  *
  * @return PUBLIC void
  *
  * @note
  *
 ********************************************************************************/
PUBLIC void vGetLpcSerialStats( tsLPCSerialStats *psStats)
{
    *psStats = sLpcSerialStats;
}

/********************************************************************************
  *
  * @fn PUBLIC void vResetLpcSerialStats
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return PUBLIC void
  *
  * @note
  *
 ********************************************************************************/
PUBLIC void vResetLpcSerialStats( void)
{
    DBG_vPrintf((bool_t)FALSE, "Reset LPC STATS\n");
    (void)ZBmemset( &sLpcSerialStats, 0, sizeof(tsLPCSerialStats) );
}

/********************************************************************************
  *
  * @fn PRIVATE bool_t bIsDuplicate
  *
  */
 /**
  *
  * @param u16Type uint16
  * @param u8SeqNo uint8
  *
  * @brief void
  *
  * @return PRIVATE bool_t
  *
  * @note
  *
 ********************************************************************************/
PRIVATE bool_t bIsDuplicate(uint16 u16Type, uint8 u8SeqNo)
{
    static uint8 u8LastPushSeq = 0xffU;
    static uint16 u16LastPushType = 0xffffU;
    bool_t bRet = (bool_t)FALSE;
    if (( u16Type == (uint16)E_SL_MSG_HOST_JN_NACK) ||
            ( u16Type == (uint16)E_SL_MSG_STATUS_MSG) ||
            ( u16Type == (uint16)E_SL_MSG_STATUS_SHORT_MSG) )
    {
        /* no duplicate check on nack */
        bRet = (bool_t)FALSE;

    }
    else
    {
        if ((u16Type == u16LastPushType) && (u8SeqNo == u8LastPushSeq))
        {
            bRet = (bool_t)TRUE;
        }
        else
        {
            u8LastPushSeq = u8SeqNo;
            u16LastPushType = u16Type;
        }
    }
    return bRet;

}


/*
 * Decrement the Tx Sequence number by 1, omly to be used for testing purposes
 */
/********************************************************************************
  *
  * @fn PUBLIC void vDecrementTxseqNo
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return PUBLIC void
  *
  * @note
  *
 ********************************************************************************/
PUBLIC void vDecrementTxseqNo(void)
{
    u8SLTxSeqNum--;
}

/********************************************************************************
  *
  * @fn PRIVATE uint32 u32GetStatusFlagsFromSend
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return PUBLIC uint32
  *
  * @note
  *
 ********************************************************************************/
PRIVATE uint32 u32GetStatusFlagsFromSend(void)
{
    return u32GetJNInfoSafe((uint16)E_SL_MSG_SERIAL_LINK_GET_STATUS_FLAGS);
}


/********************************************************************************
  *
  * @fn PUBLIC void vSetJNState
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return PUBLIC uint32
  *
  * @note
  *
 ********************************************************************************/
PUBLIC void vSetJNState(teJNState eState)
{
    vSL_TaskEnterCritical();
    eJNState = eState;
    vSL_TaskExitCritical();
}

/********************************************************************************
  *
  * @fn PUBLIC void teGetJNState
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return teJNState
  *
  * @note
  *
 ********************************************************************************/
PUBLIC teJNState teGetJNState(void)
{
    teJNState eState;

    vSL_TaskEnterCritical();
    eState = eJNState;
    vSL_TaskExitCritical();

    return eState;
}

/********************************************************************************
  *
  * @fn PUBLIC bool bGetJNReady
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return PUBLIC bool
  *
  * @note
  *
 ********************************************************************************/
PUBLIC bool bGetJNReady(void)
{
    bool bIsReady = (bool_t)FALSE;
    uint8 u8JNState;
    u8JNState = (uint8)u32GetJNInfoSafe((uint16)E_SL_MSG_JN_GET_STATE);
    if ((u8JNState & JN_READY_FOR_COMMANDS) !=0U)
    {
        bIsReady = (bool_t)TRUE;
    }

    return bIsReady;
}

/********************************************************************************
  *
  * @fn PUBLIC void vWaitForJNReady
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return PUBLIC void
  *
  * @note
  *
 ********************************************************************************/
PUBLIC void vWaitForJNReady(uint16 u16JNReadyTime)
{
    int16 i16To = 0;
    uint32 u32SavedPeriod;

    /*
     * Let's wait here for the minimum measured time
     * after which the JN should be up.
     */
    vSleep(WAIT_JN_READY_SLEEP_TIME_MS);

    /*
     * If the provided wait time is less than the
     * minimum JN sleep time, then don't bother
     */
    if (u16JNReadyTime < WAIT_JN_READY_SLEEP_TIME_MS)
    {
        i16To = -1;
    }
    else
    {
        i16To = (int16)((int16)u16JNReadyTime - (int16)WAIT_JN_READY_SLEEP_TIME_MS);
    }

    u32SavedPeriod = u32SL_GetResponsePeriod();
    vSL_SetStandardResponsePeriod();

    while ((teGetJNState() == JN_NOT_READY) && (i16To >= 0))
    {
        vSleep(WAIT_JN_READY_POLL_TIME_MS);
        i16To = i16To - (int16)WAIT_JN_READY_POLL_TIME_MS;
        if (bGetJNReady() == (bool_t)TRUE) {
            vSetJNState(JN_READY);
        }
        /* Take into account the time needed for response
         * Assume full response timeout of 100ms (TX_RESPONSE_TIME_MS).
         * This is safe because in case of success the loop will return anwyay,
         * in case of failure timeout should be considered.
        */
        i16To = i16To - (int16)TX_RESPONSE_TIME_MS;
    }
    if (u32SavedPeriod == LONG_TX_RESPONSE_TIME_MS) {
        vSL_SetLongResponsePeriod();
    }

    if ((teGetJNState() == JN_NOT_READY) && (i16To <= 0))
    {
        DBG_vPrintf((bool_t)TRUE, "JN failed to be ready after %d msec\n",
            u16JNReadyTime);
    }
}

/********************************************************************************
  *
  * @fn PRIVATE uint32 u32GetJNInfoSafe
  *
  */
 /**
  *
  *
  * @brief  Special command send to get the JN resource flags that can be
  *         called from within the standard send function, avoiding recursion
  *
  *
  * @return PRIVATE uint32
  *
  * @note
  *
 ********************************************************************************/
PRIVATE uint32 u32GetJNInfoSafe(uint16 u16Type)
{
    uint16 u16Length = 0U;

    uint8 u8CRC = u8SL_CalculateCRC(u16Type, u16Length, u8SLTxSeqNum, u8SLRxSeqNum, NULL);

    vSL_TxByte((bool_t)TRUE, SL_START_CHAR);

    /* Send message type */
    vSL_TxByte((bool_t)FALSE, (uint8)((uint16)(u16Type >> 8U) & 0xffU));
    vSL_TxByte((bool_t)FALSE, (uint8)((uint16)(u16Type >> 0U) & 0xffU));

    /* Send message length */
#ifdef SL_HOST_TO_COPROCESSOR_SECURE
    vSL_SecureTransmitLength(u16Length);
#else
    vSL_TxByte((bool_t)FALSE, (uint8)((uint16)(u16Length >> 8U) & 0xffU));
    vSL_TxByte((bool_t)FALSE, (uint8)((uint16)(u16Length >> 0U) & 0xffU));
#endif

    vSL_TxByte((bool_t)FALSE, u8SLTxSeqNum);
    u8SLTxSeqNum++;
    vSL_TxByte((bool_t)FALSE, u8SLRxSeqNum);

    /* Send message checksum */
    vSL_TxByte((bool_t)FALSE, u8CRC);

    vSL_TxByte((bool_t)TRUE, SL_END_CHAR);

    uint8 *pu8RxBuffer;
    sSLCommon.bIsWaitingStatus = (bool_t)TRUE;
    pu8RxBuffer = pu8SL_GetRxMessageFromSerialQueue(u16Type);
    sSLCommon.bIsWaitingStatus = (bool_t)FALSE;

     if(pu8RxBuffer != NULL)
     {
         uint8 u8Status = *(pu8RxBuffer + SL_MSG_RX_STATUS_IDX);
         uint32 u32Flags = 0U;
         uint16 u16SentPktType = ((uint16)*(pu8RxBuffer + SL_MSG_RSP_TYPE_MSB_IDX)) << 8U;
         u16SentPktType += (uint16)*(pu8RxBuffer + SL_MSG_RSP_TYPE_LSB_IDX);

         if ((u16SentPktType == (uint16)u16Type) && (u8Status == 0u))
         {
             /* got successful response to correct message */
             u32Flags = (uint32)*(pu8RxBuffer+SL_MSG_RSP_START_IDX+3);
             u32Flags |= ((uint32)*(pu8RxBuffer+SL_MSG_RSP_START_IDX+2) << 8U);
             u32Flags |= ((uint32)*(pu8RxBuffer+SL_MSG_RSP_START_IDX+1) << 16U);
             u32Flags |= ((uint32)*(pu8RxBuffer+SL_MSG_RSP_START_IDX+0) << 24U);
         }
         /* let the buffer go */
         vSL_FreeRxBuffer(pu8RxBuffer);
         return u32Flags;
     }

    return 0u;
}

/********************************************************************************
  *
  * @fn PRIVATE void vSl_LogJnerrors
  *
  */
 /**
  *
  *
  * @brief void
  *
  * @return PUBLIC uint32
  *
  * @note
  *
 ********************************************************************************/
PRIVATE void vSl_LogJnerrors(uint32 u32Flags)
{
}

#ifdef SL_HOST_TO_COPROCESSOR_SECURE
/********************************************************************************
  *
  * @fn PUBLIC void vSL_SecureTransmitLength
  *
  */
 /**
  *
  * @param u16Length uint16
  *
  *
  * @return void
  *
 ********************************************************************************/
PUBLIC void vSL_SecureTransmitLength(uint16 u16Length)
{
    if (u16Length)
    {
        vSL_TxByte(FALSE, (uint8)((uint16)((u16Length + SL_SECURED_MSG_MIC_SIZE) >> 8U) & 0xffU));
        vSL_TxByte(FALSE, (uint8)((uint16)((u16Length + SL_SECURED_MSG_MIC_SIZE) >> 0U) & 0xffU));
    }
    else
    {
        vSL_TxByte(FALSE, (uint8)((uint16)(u16Length >> 8U) & 0xffU));
        vSL_TxByte(FALSE, (uint8)((uint16)(u16Length >> 0U) & 0xffU));
    }
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_SecureTransmitMessageAuth
  *
  */
 /**
  *
  * @param u16Length uint16
  * @param pu8Data uint8 *
  *
  *
  * @return void
  *
 ********************************************************************************/
PUBLIC void vSL_SecureTransmitMessageAuth(uint16 u16Length, uint8 *pu8Data)
{
    if (u16Length)
    {
        /* Send message authentication */
        for (uint8 i = 0; i < SL_SECURED_MSG_MIC_SIZE; i++)
        {
            vSL_TxByte(FALSE, pu8Data[u16Length+i]);
        }
    }
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_SecureEncryptMessage
  *
  */
 /**
  *
  * @param u16Length uint16
  * @param pu8Data uint8 *
  *
  *
  * @return void
  *
 ********************************************************************************/
PUBLIC void vSL_SecureEncryptMessage(uint16 u16Length, uint8 *pu8Data)
{
    if (u16Length)
    {
        SEC_vEncryptBlock(pu8Data, u16Length, pu8Data + u16Length);
    }
}

/********************************************************************************
  *
  * @fn PUBLIC bool_t bSL_SecureDecryptMessage
  *
  */
 /**
  *
  * @param pu16Length uint16 *
  * @param pu8Data uint8 *
  *
  *
  * @return void
  *
 ********************************************************************************/
PUBLIC bool_t bSL_SecureDecryptMessage(uint8 *pu8RxSerialBuffer, uint16 *pu16Length)
{
    if (*pu16Length > SL_SECURED_MSG_MIC_SIZE)
    {
        uint8 *pu8Data = pu8RxSerialBuffer + SL_MSG_DATA_START_IDX;

        /* Encrypted message from Serial Link contains data & MIC */
        *pu16Length -= SL_SECURED_MSG_MIC_SIZE;

        if (SEC_bDecryptBlock(pu8Data, *pu16Length, pu8Data+(*pu16Length)) == (bool_t)FALSE)
        {
            // Bad MIC is a fatal error as the coprocessor and Host are now out of sync
            // Need to watchdog and reboot
            if(!sSLCommon.bIsSlMsgGetVer)
                APP_vVerifyFatal(FALSE, "Decryption Error on serial link", ERR_FATAL_ZIGBEE_RESTART);
            return FALSE;
        }
        /* Update length in message */
        memcpy(pu8RxSerialBuffer + SL_MSG_RX_LEN_IDX, pu16Length, sizeof(uint16));
    }

    return TRUE;
}

/********************************************************************************
  *
  * @fn PUBLIC bool_t SEC_bDecryptBlock
  *
  */
 /**
  *
  * @param pu8Data uint8 *
  * @param u16Length uint16
  * @param pu8Mic uint8 *
  *
  * @brief generates random number as integer
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
WEAK bool_t SEC_bDecryptBlock(uint8 *pu8Data, uint16 u16Length, uint8 *pu8Mic)
{
    CRYPTO_tsAesBlock key,nonce;
    const uint8 au8key[16] = {0x0, 0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    const uint8 au8nonce[16] = {0xa0, 0xa1, 0xa2, 0xa3,0xa4, 0xa5, 0xa6, 0xa7,0x03,0x02,0x01,0x00,0x06,0,0,0};
    const uint8 au8Auth[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    bool_t bReturn = FALSE;

    ZBmemcpy(key.au8, au8key, 16);
    ZBmemset(&nonce, 0, sizeof(CRYPTO_tsAesBlock));
    ZBmemcpy(&nonce.au8[1], au8nonce, 15);

    bReturn = zbPlatCryptoAesSetKey((CRYPTO_tsReg128*)&key);
    if(bReturn)
    {
        zbPlatCryptoAesCcmStar(
            FALSE, /* TRUE=Encrypt / FALSE=Decrypt */
            SL_SECURED_MSG_MIC_SIZE, /* Required number of checksum bytes */
            sizeof(au8Auth), /* Length of authentication data in bytes */
            u16Length, /* Length of input data in bytes */
            &nonce, /* A pointer to the 128bit nonce data */
            (uint8*)au8Auth, /* Authentication data */
            pu8Data, /* Input and output data */
            pu8Mic, /* Checksum (MIC) */
            &bReturn); /* Authenticated flag */
        return bReturn;
    }
    else
    {
        DBG_vPrintf(TRUE, "Fail to set zbPlatCryptoAesSetKey %d\n", bReturn);
        return bReturn;
    }
}

/********************************************************************************
  *
  * @fn PUBLIC void SEC_vEncryptBlock
  *
  */
 /**
  *
  * @param pu8Data uint8 *
  * @param u16Length uint16
  * @param pu8Mic uint8 *
  *
  * @brief Encrypt the block
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
WEAK void SEC_vEncryptBlock(uint8 *pu8Data, uint16 u16Length, uint8 *pu8Mic)
{
    CRYPTO_tsAesBlock key,nonce;
    const uint8 au8key[16] = {0x00, 0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff};
    const uint8 au8nonce[16] = {0xa0, 0xa1, 0xa2, 0xa3,0xa4, 0xa5, 0xa6, 0xa7,0x03,0x02,0x01,0x00,0x06,0,0,0};
    const uint8 au8Auth[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    bool_t bReturn = FALSE;

    ZBmemcpy(key.au8, au8key, 16);
    ZBmemset(&nonce, 0, sizeof(CRYPTO_tsAesBlock));
    ZBmemcpy(&nonce.au8[1], au8nonce, 15);

    bReturn = zbPlatCryptoAesSetKey((CRYPTO_tsReg128*)&key);
    if(bReturn)
    {
        zbPlatCryptoAesCcmStar(
            TRUE, /* TRUE=Encrypt / FALSE=Decrypt */
            SL_SECURED_MSG_MIC_SIZE, /* Required number of checksum bytes */
            sizeof(au8Auth), /* Length of authentication data in bytes */
            u16Length, /* Length of input data in bytes */
            &nonce, /* A pointer to the 128bit nonce data */
            (uint8*)au8Auth, /* Authentication data */
            pu8Data, /* Input and output data */
            pu8Mic, /* Checksum (MIC) */
            NULL); /* Authenticated flag */
    }
    else
    {
        DBG_vPrintf(TRUE, "Fail to set zbPlatCryptoAesSetKey %d\n", bReturn);
    }
}

#endif

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
