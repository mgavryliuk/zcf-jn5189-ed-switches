/*
* Copyright 2019, 2023 NXP
* All rights reserved.
*
* SPDX-License-Identifier: BSD-3-Clause
*/


/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

#include <string.h>
#include "EmbeddedTypes.h"
#include "dbg.h"
#include "zb_platform.h"
#include "app_uart.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/
/****************************************************************************
 *
 * NAME: UART_bReceiveChar
 *
 * DESCRIPTION:
 *
 * PARAMETERS: Name        RW  Usage
 *
 * RETURNS:
 *
 ****************************************************************************/
bool_t UART_bReceiveChar ( uint8_t* pu8Data )
{
    return zbPlatUartReceiveChar(pu8Data);
}

/****************************************************************************
 *
 * NAME: vUART_Init
 *
 * DESCRIPTION:
 * Initializes UART component
 *
 * PARAMETERS:      Name            RW  Usage
 *                  device              Holds platform dependent information
 *
 * RETURNS:
 * None.
 *
 * NOTES:
 * None.
 ****************************************************************************/
void UART_vInit(void *device)
{
    (void)zbPlatUartInit(device);
}

/****************************************************************************
 *
 * NAME: vUART_SetBuadRate
 *
 * DESCRIPTION:
 *
 * PARAMETERS: Name        RW  Usage
 *
 * RETURNS:
 *
 ****************************************************************************/

void UART_vSetBaudRate(uint32_t u32BaudRate)
{
    if (!zbPlatUartSetBaudRate(u32BaudRate))
    {
        DBG_vPrintf(TRACE_UART,"\r\nFailed to set UART speed ");
    }
}

/****************************************************************************
 *
 * NAME: vUART_TxChar
 *
 * DESCRIPTION:
 * Set UART RS-232 RTS line low to allow further data
 *
 ****************************************************************************/
void UART_vTxChar(uint8_t u8Char)
{
    (void)zbPlatUartTransmit(u8Char);
}

/****************************************************************************
 *
 * NAME: vUART_TxReady
 *
 * DESCRIPTION:
 * Set UART RS-232 RTS line low to allow further data
 *
 ****************************************************************************/
bool_t UART_bTxReady()
{
    return zbPlatUartCanTransmit();
}

/****************************************************************************
 *
 * NAME: UART_vSendString
 *
 * DESCRIPTION:
 *
 *
 * RETURNS:
 * void
 *
 ****************************************************************************/
void UART_vSendString(char* sMessage)
{
    uint32_t u32Counter = 0 ;
    for(u32Counter = 0; u32Counter < strlen(sMessage); u32Counter++ )
    {
    	UART_vTxChar(sMessage[u32Counter]);
    }
}

/****************************************************************************
 *
 * NAME: UART_bReceiveBuffer
 *
 * DESCRIPTION:
 * Tries to receive a chars buffer of length provided by user
 *
 * PARAMETERS:      Name            RW  Usage
 *                  u8Buffer            Buffer to store received characters
 *                  pu32BufferLen       Number of characters to receive. At return
 *                                      will hold the actual number of characters
 *                                      received
 *
 * RETURNS:
 *  TRUE  if at least one character is available
 *  FALSE otherwise
 *
 ****************************************************************************/
bool_t UART_bReceiveBuffer(uint8_t* u8Buffer, uint32_t *pu32BufferLen)
{
    return zbPlatUartReceiveBuffer(u8Buffer, pu32BufferLen);
}

/****************************************************************************
 *
 * NAME: UART_vFree
 *
 * DESCRIPTION:
 * Release allocated resources
 *
 * PARAMETERS:      Name            RW  Usage
 * None.
 *
 * RETURNS:
 * None.
 *
 * NOTES:
 * None.
 ****************************************************************************/
void UART_vFree(void)
{
    (void)zbPlatUartFree();
}
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
