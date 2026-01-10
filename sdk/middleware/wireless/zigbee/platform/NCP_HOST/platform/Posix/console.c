/*
 * Copyright 2023-2024 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <assert.h>
#include <stdlib.h>

#include "app_console.h"
#include "zb_platform.h"
#include "dbg.h"

#define BAUD_VALUES 31

static uint32_t u32GetBaudRateDefineFromValue(uint32_t value);

bool zbPlatConsoleInit(void)
{
    struct termios term;

    /* Obtain terminal attributes */
    if (tcgetattr(STDIN_FILENO, &term))
    {
        DBG_vPrintf(TRACE_CONSOLE, "Failed to retrieve terminal attributes\r\n");
        return false;
    }

    /* Disable character echo and canonical input processing */
    term.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &term))
    {
        DBG_vPrintf(TRACE_CONSOLE, "Failed to set terminal attributes\r\n");
        return false;
    }

    return true;
}

bool zbPlatConsoleReceiveChar(uint8_t *pu8Data)
{
    bool bRet = FALSE;
    int iRet;
    fd_set rd;

    struct timeval tv;
    int err;

    FD_ZERO(&rd);
    FD_SET(STDIN_FILENO, &rd);

    tv.tv_sec  = 0;
    tv.tv_usec = 100;
    err = select(1, &rd, NULL, NULL, &tv);
    if ((err != 0) && (err != -1))
    {
        iRet = read(STDIN_FILENO, pu8Data, 1);
        bRet = TRUE;
        assert(iRet > 0);
        err = select(1, &rd, NULL, NULL, &tv);
    }

    return bRet;
}

bool zbPlatConsoleCanTransmit(void)
{
    return true;
}

bool zbPlatConsoleTransmit(uint8_t pu8Data)
{
    bool bRet = FALSE;
    int iRet;
    fd_set wd;

    struct timeval tv;
    int err;

    FD_ZERO(&wd);
    FD_SET(STDOUT_FILENO, &wd);

    tv.tv_sec  = 0;
    tv.tv_usec = 100;
    err = select(STDOUT_FILENO + 1, NULL, &wd, NULL, &tv);
    if ((err != 0) && (err != -1))
    {
        iRet = write(STDOUT_FILENO, &pu8Data, 1);
        bRet = TRUE;
        assert(iRet > 0);
        err = select(1, NULL, &wd, NULL, &tv);
    }

    return bRet;
}

void zbPlatConsoleSetBaudRate(uint32_t baud)
{
    struct termios term;

    uint32_t baudDefine = u32GetBaudRateDefineFromValue(baud);

    term.c_cflag = CS8 | HUPCL | CREAD | CLOCAL;

    /* Set STDIN input speed */
    tcgetattr(STDIN_FILENO, &term);
    cfsetispeed(&term, baudDefine);
    tcsetattr(STDIN_FILENO, TCSANOW, &term);

    /* Set STDOUT input speed */
    tcgetattr(STDOUT_FILENO, &term);
    cfsetospeed(&term, baudDefine);
    tcsetattr(STDOUT_FILENO, TCSANOW, &term);
}

static uint32_t u32GetBaudRateDefineFromValue(uint32_t u32BaudValue)
{
   
    uint32_t baud_defines[BAUD_VALUES] = {B0, B50, B75, B110, B134, B150, B200, B300, B600, B1200, B1800, B2400, B4800, 
                            B9600, B19200, B38400, B57600, B115200, B230400, B460800, B500000, B576000,
                            B921600, B1000000, B1152000, B1500000, B2000000, B2500000, B3000000, B3500000,
                            B4000000};

    uint32_t baud_values[BAUD_VALUES] = {0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800, 2400, 4800, 
                            9600, 19200, 38400, 57600, 115200, 230400, 460800, 500000, 576000,
                            921600, 1000000, 1152000, 1500000, 2000000, 2500000, 3000000, 3500000,
                            4000000};

    uint32_t min_dif_val = abs(u32BaudValue - baud_values[0]);
    uint8_t min_dif_index = 0;
    
    for (uint8_t i = 0; i < BAUD_VALUES; i++)
    {
        if(abs(u32BaudValue - baud_values[i]) <= min_dif_val)
        {
            min_dif_val = abs(u32BaudValue - baud_values[i]);
            min_dif_index = i;
        }
    }

    return baud_defines[min_dif_index];
}

void zbPlatConsoleDeInit(void)
{
    struct termios term;
    bool ret = false;

    /* Obtain terminal attributes */
    if (tcgetattr(STDIN_FILENO, &term))
    {
        DBG_vPrintf(TRACE_CONSOLE, "Failed to retrieve terminal attributes\r\n");
    }
    else
    {
        /* Re-enable character echo and canonical input processing */
        term.c_lflag |= (ECHO | ICANON);
        if (tcsetattr(STDIN_FILENO, TCSANOW, &term))
        {
            DBG_vPrintf(TRACE_CONSOLE, "Failed to set terminal attributes\r\n");
        }
    }
}
