/*
* Copyright 2023 NXP
* All rights reserved.
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <stdint.h>
#include <stdbool.h>

#include "board.h"
#include "zb_platform.h"

bool zbPlatConsoleInit(void)
{
    /* Pins and UART configurations ar part of BOARD init code */
    BOARD_InitDebugConsole();

    /* invoke Uart init */
    return zbPlatUartInit(NULL);
}

bool zbPlatConsoleReceiveChar(uint8_t* pu8Data)
{
    /* invoke Uart buffer receive */
    return zbPlatUartReceiveChar(pu8Data);
}

bool zbPlatConsoleCanTransmit(void)
{
	return zbPlatUartCanTransmit();
}

bool zbPlatConsoleTransmit(uint8_t pu8Data)
{
	/* invoke Uart buffer transmit */
	return zbPlatUartTransmit(pu8Data);
}

void zbPlatConsoleSetBaudRate(uint32_t baud)
{
	zbPlatUartSetBaudRate(baud);
}

void zbPlatConsoleDeInit(void)
{
}
