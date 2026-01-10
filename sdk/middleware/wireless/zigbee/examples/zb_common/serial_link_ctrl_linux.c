/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier:    BSD-3-Clause
 */

#ifdef DEBUG_SERIAL_LINK
#define DEBUG_SL            TRUE
#else
#define DEBUG_SL            FALSE
#endif

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include <pthread.h>
#include "jendefs.h"
#include <dbg.h>
#include "serial_link_ctrl.h"
#include "zps_apl.h"
#include "zps_apl_aib.h"
#include "app_uart.h"
#include "app_common_ncp.h"

#include <semaphore.h>
#include <signal.h>
#include <time.h>
#include <assert.h>
#include <fcntl.h>

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "ZQueue.h"
#include "ZTimer.h"


/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define UART_RX_BUF_SIZE (2048U)
//#define RX_DMA_TIMEOUT        (5U)
#define RX_DMA_TIMEOUT      (100U)
/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef struct
{


    tszQueue serialQueueHandle;
    sem_t serialSemaphore;
    tszQueue serialRxUartQueueHandle;
    uint16_t rxSerialRspTimerHandle;
    uint8_t mtGJNPrgTimerHandle;   // added by JB
}tsSL_FreeRtosCommon;

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/
PRIVATE void vSL_vRxSerialRspTimerCallbackFnct(void *p_arg);
PRIVATE void vSL_vMtgJNPrgTimerCallbackFnct(void *p_arg);
PRIVATE tsSL_FreeRtosCommon sSL_FreeRtosCommon;
/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/
PRIVATE volatile bool_t bSLRspTimerExp = (bool_t)FALSE;
PRIVATE volatile bool_t bMtgJNPrgTimerExp = (bool_t)FALSE;
PRIVATE volatile uint32 u32RspTimerPeriod = TX_RESPONSE_TIME_MS;

PUBLIC uint32 u32AvgRxCheck = 0u;
PUBLIC uint32 u32MaxRxCheck = 0u;
PUBLIC uint32 u32JnResettimeStart = 0u;

PRIVATE uint32 u32MaxBufferUsage = 0u;
static int loggerFd = -1;

extern uint8_t rxDmaTimerHandle;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/********************************************************************************
  *
  * @fn PUBLIC teSL_Status eSL_SerialInit
  *
  */
 /**
  *
  *
  * @brief Init the serial link
  *
  * @return teSL_Status
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC teSL_Status eSL_SerialInit(void)
{
    int ret;
    struct sigevent sevp;
    ZTIMER_teStatus status;

    (void)ZBmemset(&sSL_FreeRtosCommon, 0, sizeof(sSL_FreeRtosCommon));

    ret = sem_init(&sSL_FreeRtosCommon.serialSemaphore, 0, 1);
    APP_vVerifyFatal((bool_t)(ret != -1),
            "Semaphore Create failed", ERR_FATAL_REBOOT);

    /* Create OS Queue for waiting for receiving fully parsed serial command
     * from 68 */
    ZQ_vQueueCreate(&sSL_FreeRtosCommon.serialQueueHandle, 20UL, sizeof(uintptr_t), NULL);

    /* Create queue for receiving characters from the UART */
    ZQ_vQueueCreate(&sSL_FreeRtosCommon.serialRxUartQueueHandle, MAX_RX_SERIAL_BUFFERS, sizeof(uintptr_t), NULL);

    return E_SL_SUCCESS;
}

#ifndef MULTI_TASK_ENABLED
/********************************************************************************
  *
  * @fn PUBLIC uint8 vSL_CheckAndHandleSerialMsg
  *
  */
 /**
  *
  *
  * @brief Check if any mesg present in serial queue and handle it
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 vSL_CheckAndHandleSerialMsg(void)
{
    uintptr_t uiMsg;
    bool_t pBT = FALSE;
    pBT = ZQ_bQueueReceive(&sSL_FreeRtosCommon.serialRxUartQueueHandle,
                           (void*)&uiMsg);
    if(pBT == TRUE)
    {
        /*Call vProcessSerialCommand function*/
        vProcessIncomingSerialCommands((uint8*)uiMsg);
    }
    return (uint8)pBT;
}
/********************************************************************************
  *
  * @fn PUBLIC void vSL_CheckAndHandleSerialLogMsg
  *
  */
 /**
  *
  *
  * @brief Check if any mesg present in serial queue and handle it
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_CheckAndHandleSerialLogMsg(void)
{
    uintptr_t uiMsg;
    bool_t pBT = FALSE;
    pBT = ZQ_bQueueReceive(&sSL_FreeRtosCommon.serialRxUartQueueHandle,
                           (void*)&uiMsg);
    if(pBT == TRUE)
    {
        /*Call vProcessSerialCommand function*/
        vProcessIncomingSerialLogCommand((uint8*)uiMsg);
    }

}
#endif
/********************************************************************************
  *
  * @fn PUBLIC void vSL_TakeSerialSemaphore
  *
  */
 /**
  *
  *
  * @brief Take the serial Semaphore
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_TakeSerialSemaphore(void)
{
    //(void)xSemaphoreTake(sSL_FreeRtosCommon.serialSemaphore,  portMAX_DELAY);
    sem_wait(&sSL_FreeRtosCommon.serialSemaphore);
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_GiveSerialSemaphore
  *
  */
 /**
  *
  *
  * @brief release the serial semaphore
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_GiveSerialSemaphore(void)
{
    //(void)xSemaphoreGive(sSL_FreeRtosCommon.serialSemaphore);
    sem_post(&sSL_FreeRtosCommon.serialSemaphore);
}
/********************************************************************************
  *
  * @fn PUBLIC void restartUARTFreeRtosCommon
  *
  */
 /**
  *
  *
  * @brief ReInitialize sSL_FreeRtosCommon members for UART
  *
  * @return PUBLIC void
  *
  * @note
  *
 ********************************************************************************/
PUBLIC void restartUARTFreeRtosCommon(bool_t bGiveSLSemaphone)
{
    sSL_FreeRtosCommon.rxSerialRspTimerHandle = TX_RESPONSE_TIME_MS;

    bMtgJNPrgTimerExp =  (bool_t)FALSE;

    if(bGiveSLSemaphone)
    {
        vSL_GiveSerialSemaphore();
    }

    vSL_FlushSerialQueue();

    vSL_FlushRxUartQueue();
}

void restartUARTRegs()
{
    fprintf(stderr, "%s - not doing anything\r\n", __func__);
    //close(ncp_fd);
    //open(ncp_fd)

}

/********************************************************************************
  *
  * @fn PUBLIC uint8* pu8SL_GetRxMessageFromSerialQueue
  *
  */
 /**
  *
  * @param u16MsgTypeTransmitted uint16
  *
  * @brief Receive Message from serial link queue
  *
  * @return uint8 - pointer to received message
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC uint8 *pu8SL_GetRxMessageFromSerialQueue(uint16 u16MsgTypeTransmitted)
{
    uint8 *pu8RxBuffer;

    uint16 u16SRxPktType;

    uintptr_t uiMsg;
    bool pBT;

    sSL_FreeRtosCommon.rxSerialRspTimerHandle = u32RspTimerPeriod;

    while (sSL_FreeRtosCommon.rxSerialRspTimerHandle--)
    {
#ifndef MULTI_TASK_ENABLED
        (void)vSL_CheckAndHandleSerialMsg();
#endif
        pBT = ZQ_bQueueReceive(&sSL_FreeRtosCommon.serialQueueHandle, (void*)&uiMsg);
        if(pBT == TRUE)
        {
            pu8RxBuffer =(uint8*)uiMsg;
            uint16 u16Code = ((uint16)*(pu8RxBuffer+SL_MSG_TYPE_MSB_IDX)) << 8U;
            u16Code += *(pu8RxBuffer+SL_MSG_TYPE_LSB_IDX);
            /* 8 and 9 bits in the received message represents message type */
            u16SRxPktType = ((uint16)*(pu8RxBuffer+SL_MSG_RSP_TYPE_MSB_IDX)) << 8U;
            u16SRxPktType += *(pu8RxBuffer+SL_MSG_RSP_TYPE_LSB_IDX);
            /* check Rxed Pkt Type */

            if((u16SRxPktType == u16MsgTypeTransmitted)||(IGNORE_MSG_TYPE == u16MsgTypeTransmitted))
            {
                //(void)ZTIMER_eStop(sSL_FreeRtosCommon.rxSerialRspTimerHandle);
                sSL_FreeRtosCommon.rxSerialRspTimerHandle = 0;
                return (uint8*)uiMsg;
            }
            else if (u16Code == (uint16)E_SL_MSG_HOST_JN_NACK)
            {
                //(void)ZTIMER_eStop(sSL_FreeRtosCommon.rxSerialRspTimerHandle);
                sSL_FreeRtosCommon.rxSerialRspTimerHandle = 0;
                vSL_FreeRxBuffer((uint8*)uiMsg);
                return NULL;
            }
            else
            {
                vSL_FreeRxBuffer((uint8*)uiMsg);
                 DBG_vPrintf((bool_t)TRUE, "u16SRxPktType = 0x%04x u16MsgTypeTransmitted = 0x%04x\r\n", u16SRxPktType, u16MsgTypeTransmitted);
            }
        }
        //ZTIMER_vTask();
        vSleep(1);

        APP_vSeHostCheckRxBuffer();
    }
    return NULL;
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_PostMessageToSerialQueue
  *
  */
 /**
  *
  * @param pvMessage void *
  *
  * @brief Post message to serial queue
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_PostMessageToSerialQueue(void *pvMessage)
{
    bool_t bResult;

    bResult = ZQ_bQueueSend(&sSL_FreeRtosCommon.serialQueueHandle,
                              pvMessage);

    APP_vVerifyFatal(bResult, "Queue Send Serial Q failed", ERR_FATAL_ZIGBEE_RESTART);
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_PostMessageToSerialRxQueue
  *
  */
 /**
  *
  * @param pvMessage void *
  *
  * @brief post message to serial Rx Queue
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_PostMessageToSerialRxQueue(void *pvMessage)
{
    bool_t bResult;

    bResult = ZQ_bQueueSend(&sSL_FreeRtosCommon.serialRxUartQueueHandle,
                              pvMessage);

    /* Convert FreeRTOS result to bool_t */
    if (TRUE == bResult)
    {
        bResult = (bool_t)TRUE;
    }
    else
    {
        /* post failed free buffer */
        vSL_FreeRxBuffer( (uint8*)pvMessage);
        bResult = (bool_t)FALSE;
    }

    APP_vVerifyFatal(bResult, "Queue Send Serial Rx q failed", ERR_FATAL_ZIGBEE_RESTART);
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_EmptyStatusMsgQueue
  *
  */
 /**
  *
  *
  * @brief Empty the serial message queue of status messages, as the protocol
  * is one sent gets one response, this should be empty before sending data
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_EmptyStatusMsgQueue(void)
{
    uintptr_t uiMsg;
    bool pBT = TRUE;

    while (pBT == TRUE)
    {
        pBT = ZQ_bQueueReceive(&sSL_FreeRtosCommon.serialQueueHandle, (void*)&uiMsg);
        if(pBT == TRUE)
        {
            uint16 u16SRxPktType;
            uint8* pu8RxBuffer =(uint8*)uiMsg;
            u16SRxPktType = ((uint16)*(pu8RxBuffer+8)) << 8U;
            u16SRxPktType += *(pu8RxBuffer+9);
            DBG_vPrintf((bool_t)TRUE, "Throw away status type %04x\n", u16SRxPktType);

            vSL_FreeRxBuffer((uint8*)uiMsg);
        }
        /* stops once the queue is empty and pBT is set false */
    }
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_SerialLinkRtosTest
  *
  */
 /**
  *
  *
  * @brief
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_SerialLinkRtosTest(void)
{
    //APP_vVerifyCurrentTaskIsNotTimerDaemon();
    return;
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_FlushSerialQueue
  *
  */
 /**
  *
  *
  * @brief
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_FlushSerialQueue(void)
{
    //(void)xQueueReset(sSL_FreeRtosCommon.serialQueueHandle);
    DBG_vPrintf(TRUE,"%s not implemented\r\n", __func__);
}
/********************************************************************************
  *
  * @fn PUBLIC void vSL_FlushRxUartQueue
  *
  */
 /**
  *
  *
  * @brief
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_FlushRxUartQueue(void)
{
    //(void)xQueueReset(sSL_FreeRtosCommon.serialRxUartQueueHandle);
    DBG_vPrintf(TRUE,"%s not implemented\r\n", __func__);
}
/********************************************************************************
  *
  * @fn PUBLIC void vSL_TaskEnterCritical
  *
  */
 /**
  *
  *
  * @brief
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_TaskEnterCritical(void)
{
    //taskENTER_CRITICAL();
    //DBG_vPrintf(TRUE,"%s\r\n", __func__);
}

/********************************************************************************
  *
  * @fn PUBLIC void vSL_TaskExitCritical
  *
  */
 /**
  *
  *
  * @brief
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_TaskExitCritical(void)
{
    //taskEXIT_CRITICAL();
    //DBG_vPrintf(TRUE, "%s\r\n", __func__);
}

/********************************************************************************
  *
  * @fn PRIVATE void vSL_vRxSerialRspTimerCallbackFnct
  *
  */
 /**
  *
  * @param p_arg void *
  *
  * @brief DMA check buffer Timer callback function
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PRIVATE void vSL_vRxSerialRspTimerCallbackFnct(void *p_arg)
{
    bSLRspTimerExp = (bool_t)TRUE;
}

/********************************************************************************
  *
  * @fn PRIVATE void vSL_vMtgJNPrgTimerCallbackFnct
  *
  */
 /**
  *
  * @param p_arg void *
  *
  * @brief MantraG programming timeout call back function
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PRIVATE void vSL_vMtgJNPrgTimerCallbackFnct(void *p_arg)
{
    bMtgJNPrgTimerExp = (bool_t)TRUE;
}


/********************************************************************************
  *
  * @fn PUBLIC void vSL_SetLongResponsePeriod
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
PUBLIC void vSL_SetLongResponsePeriod( void)
{
    sSL_FreeRtosCommon.rxSerialRspTimerHandle = LONG_TX_RESPONSE_TIME_MS;
    u32RspTimerPeriod = LONG_TX_RESPONSE_TIME_MS;
}


/********************************************************************************
  *
  * @fn PUBLIC void vSL_SetStandardResponsePeriod
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
PUBLIC void vSL_SetStandardResponsePeriod( void)
{
    sSL_FreeRtosCommon.rxSerialRspTimerHandle = TX_RESPONSE_TIME_MS;
    u32RspTimerPeriod = TX_RESPONSE_TIME_MS;
}

/********************************************************************************
  *
  * @fn PUBLIC void u32SL_GetResponsePeriod
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
PUBLIC uint32 u32SL_GetResponsePeriod(void)
{
  return u32RspTimerPeriod;
}

/********************************************************************************
  *
  * @fn PUBLIC void APP_vSeHostCheckRxBuffer
  *
  */
 /**
  *
  *
  * @brief UART poll handler
  *
  * @return void
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void APP_vSeHostCheckRxBuffer(void)
{
    static uint32 u32Last = 0;
    uint32 u32Now, u32Delta;

    uint8 u8UartRxBuf[UART_RX_BUF_SIZE];
    uint32 u32RcvdLength = UART_RX_BUF_SIZE;
    uint32 i;

    u32Now = zbPlatGetTime();
    if (u32Last > 0)
    {
        u32Delta = u32Now - u32Last;
        if (u32Delta > u32MaxRxCheck)
        {
            u32MaxRxCheck = u32Delta;
            DBG_vPrintf(1, "New max process gap %d\r\n", u32MaxRxCheck);
        }
        u32AvgRxCheck = (u32AvgRxCheck + u32Delta) >> 1;
    }
    u32Last = u32Now;

    /* Try and read a Serial Link command up to UART_RX_BUF_SIZE bytes long */
    UART_bReceiveBuffer(u8UartRxBuf, &u32RcvdLength);
    if (u32RcvdLength > 0)
    {
#ifdef ENABLE_SERIAL_LINK_FILE_LOGGING
        uint8_t rxBuffer[20];
        sprintf(rxBuffer, "\n[%d]RX-->", zbPlatGetTime());
        bSL_LoggerWrite(rxBuffer, strlen(rxBuffer));
#endif
        for(i=0UL;i<u32RcvdLength;i++)
        {
            vProcessIncomingSerialBytes(u8UartRxBuf[i]);
#ifdef ENABLE_SERIAL_LINK_FILE_LOGGING
            /* Reverse logic... */
            bSL_LoggerWrite(&u8UartRxBuf[i], 1);
#endif
        }
    }
}

/********************************************************************************
  *
  * @fn PRIVATE void APP_vRxDmaTimerCallbackFnct
  *
  */
 /**
  *
  * @param p_arg void *
  *
  * @brief DMA check buffer Timer callback function
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/

PUBLIC void APP_vRxDmaTimerCallbackFnct(void *p_arg)
{
    APP_vSeHostCheckRxBuffer();

    ZTIMER_eStart(rxDmaTimerHandle, RX_DMA_TIMEOUT);
}

/********************************************************************************
  *
  * @fn PRIVATE void APP_vNcpHostResetZigBeeModule
  *
  */
 /**
  *
  *
  * @brief Host NCP tries to reset Zigbee module
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void APP_vNcpHostResetZigBeeModule(void)
{
    vSL_SetLongResponsePeriod();
    (void)u8SL_WriteMessage((uint16)E_SL_MSG_RESET, 0U, NULL,NULL);
    vSL_SetStandardResponsePeriod();
    vSetJNState(JN_NOT_READY);
}

#ifdef ENABLE_SERIAL_LINK_FILE_LOGGING
/********************************************************************************
  *
  * @fn PRIVATE void bSL_LoggerInit
  *
  */
 /**
  *
  * @param p_arg void *
  *
  * @brief Initialize logger for Serial Link commands
  *
  * @return Boolean - TRUE on success, FALSE on failure
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC bool bSL_LoggerInit(void *p_arg)
{
    if (!p_arg)
    {
        DBG_vPrintf(TRUE, "Invalid logger filename %s\n", (char *)p_arg);
        return FALSE;
    }
    if ((loggerFd = open((char *)p_arg, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR)) == -1)
    {
        DBG_vPrintf(TRUE, "Cannot create logger file %s\r\n", (char *)p_arg);
        return FALSE;
    }
    DBG_vPrintf(TRUE, "Starting logging Serial Link commands to %s\n", (char *)p_arg);

    return TRUE;
}

/********************************************************************************
  *
  * @fn PRIVATE void bSL_LoggerWrite
  *
  */
 /**
  *
  * @param pu8Buffer uint8
  * @param u32BufferLength uint32
  *
  * @brief Write Serial Link commands in logger file
  *
  * @return Boolean - TRUE on success, FALSE on failure
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC bool bSL_LoggerWrite(uint8 *pu8Buffer, uint32 u32BufferLength)
{
    int nBytes = 0;

    if (loggerFd < 0)
    {
        DBG_vPrintf(TRUE, "Logger filename was not created\n");
        return FALSE;
    }
    if (!pu8Buffer || !u32BufferLength)
    {
        DBG_vPrintf(TRUE, "Invalid input arguments\n");
        return FALSE;
    }

    if ((nBytes = write(loggerFd, pu8Buffer, u32BufferLength)) < u32BufferLength)
    {
        DBG_vPrintf(TRUE, "Failed to write %d bytes\n", u32BufferLength);
        return FALSE;
    }
}

/********************************************************************************
  *
  * @fn PRIVATE void bSL_LoggerFree
  *
  */
 /**
  *
  *
  * @brief Release resources allocated for Serial Link logger
  *
  * @return None
  *
  * @note
  *
  * imported description
 ********************************************************************************/
PUBLIC void vSL_LoggerFree(void)
{
    if (loggerFd)
    {
        close(loggerFd);
    }
    loggerFd = -1;
}
#endif
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/


