/*
* Copyright 2019, 2023 NXP
* All rights reserved.
*
* SPDX-License-Identifier: BSD-3-Clause
*/


/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include "zb_platform.h"
#include "app_console.h"

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
 * NAME: APP_bConsoleReceive
 *
 * DESCRIPTION:
 * Receives a character from the underlying terminal
 *
 * PARAMETERS: Name        RW  Usage
 *             pu8Data         Received character
 * RETURNS:
 *  TRUE  if a character is available
 *  FALSE otherwise
 *
 ****************************************************************************/
bool_t APP_bConsoleReceiveChar(uint8_t* pu8Data)
{
    return zbPlatConsoleReceiveChar(pu8Data);
}

/****************************************************************************
 *
 * NAME: APP_vConsoleInitialise
 *
 * DESCRIPTION:
 * Initialize console component to receive serial commands
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
void APP_vConsoleInitialise(void)
{
    (void)zbPlatConsoleInit();
}

/****************************************************************************
 *
 * NAME: APP_bConsoleCanTransmit
 *
 * DESCRIPTION:
 * Returns whether the console is ready to transmit
 *
 * PARAMETERS: Name        RW  Usage
 * None.
 * 
 * RETURNS:
 *  TRUE  if the console is ready
 *  FALSE otherwise
 ****************************************************************************/
bool_t APP_bConsoleCanTransmit(void)
{
    return zbPlatConsoleCanTransmit();
}

/****************************************************************************
 *
 * NAME: APP_bConsoleTransmit
 *
 * DESCRIPTION:
 * Transmits a character to the underlying terminal
 *
 * PARAMETERS: Name        RW  Usage
 *             pu8Data         Character to transmit
 * RETURNS:
 *  TRUE  if a character is available
 *  FALSE otherwise
 *
 ****************************************************************************/
bool_t APP_bConsoleTransmit(uint8_t pu8Data)
{
    return zbPlatConsoleTransmit(pu8Data);
}

/****************************************************************************
 *
 * NAME: App_vConsoleSetBaudRate
 *
 * DESCRIPTION:
 * Returns whether the console is ready to transmit (always true)
 *
 * PARAMETERS: Name        RW  Usage
 *             baud        Desired baudrate for the console
 * 
 * RETURNS:
 *  None.
 *
 ****************************************************************************/
void APP_vConsoleSetBaudRate(uint32_t baud)
{
    zbPlatConsoleSetBaudRate(baud);
}

/****************************************************************************
 *
 * NAME: App_vConsoleDeinitialise
 *
 * DESCRIPTION:
 * Performs the necessary cleanup so that the console can be reused after
 * restart.
 * PARAMETERS: None
 *
 * RETURNS:
 *  None.
 *
 ****************************************************************************/
void APP_vConsoleDeinitialise(void)
{
    zbPlatConsoleDeInit();
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
