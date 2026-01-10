/*
 * Copyright 2023 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <stdbool.h>
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>

#include <errno.h>
#include <string.h>

#include "app_uart.h"
#include "zb_platform.h"
#include "dbg.h"

/* FD for comms with NCP */
static int ncp_fd = 0;

bool zbPlatUartInit(void *device)
{
    ncp_fd = open((char *)device, O_RDWR | O_NOCTTY | O_NONBLOCK | O_CLOEXEC);
    if (ncp_fd < 0)
    {
        DBG_vPrintf(TRUE, "Failed to open device %s: %s\r\n", (char *)device, strerror(errno));
        assert(0);
    }
    return true;
}

bool zbPlatUartSetBaudRate(uint32_t baud)
{
    struct termios tios;
    bool result = true;
    int ret     = 0;

    /* Obtain the current terminal attributes */
    tcgetattr(ncp_fd, &tios);
    cfmakeraw(&tios);

    /* Configure the baudrate */
    tios.c_cflag = CS8 | HUPCL | CREAD | CLOCAL;
    ret = cfsetspeed(&tios, SERIAL_BAUD_RATE);
    if (ret != 0)
    {
        DBG_vPrintf(TRACE_UART, "Failed to configure baudrate\r\n");
        return false;
    }

    /* Set the new options for the serial terminal */
    ret = tcsetattr(ncp_fd, TCSANOW, &tios);
    if (ret != 0)
    {
        DBG_vPrintf(TRACE_UART, "Failed to set terminal attributes\r\n");
        return false;
    }

    ret = tcflush(ncp_fd, TCIOFLUSH);
    if (ret != 0)
    {
        DBG_vPrintf(TRACE_UART, "Failed to flush\r\n");
        return false;
    }
    return result;
}

bool zbPlatUartCanTransmit(void)
{
    return true;
}

bool zbPlatUartTransmit(uint8_t u8Char)
{
    int ret;

    do
    {
        ret = write(ncp_fd, &u8Char, 1);
    } while (ret != 1);

    return true;
}

bool zbPlatUartReceiveChar(uint8_t *ch)
{
    bool result = false;
    struct timeval timeout;
    fd_set readfds;
    uint8_t u8Char;
    int err, iRet;

    /* Clear file descriptors set & add NCP FD to it */
    FD_ZERO(&readfds);
    FD_SET(ncp_fd, &readfds);

    /* Configure timeout interval */
    timeout.tv_sec  = 0;
    timeout.tv_usec = 100;

    /* Monitor read file descriptors set until NCP FD has data available */
    err = select(ncp_fd + 1, &readfds, NULL, NULL, &timeout);
    if ((err != 0) && (err != -1))
    {
        if (FD_ISSET(ncp_fd, &readfds))
        {
            /* Read only 1 character */
            iRet = read(ncp_fd, &u8Char, 1);
            assert(iRet > 0);
            *ch    = u8Char;
            result = true;
        }
    }
    return result;
}

bool zbPlatUartReceiveBuffer(uint8_t *buffer, uint32_t *length)
{
    bool result = false;
    struct timeval timeout;
    fd_set readfds;
    int err, bytes = 0;

    if (!length)
    {
        DBG_vPrintf(TRACE_UART, "Invalid pointer to store length\r\n");
        return false;
    }

    /* Clear file descriptors set & add NCP FD to it */
    FD_ZERO(&readfds);
    FD_SET(ncp_fd, &readfds);

    /* Configure timeout interval */
    timeout.tv_sec  = 0;
    timeout.tv_usec = 100;

    /* Monitor read file descriptors set until NCP FD has data available */
    err = select(ncp_fd + 1, &readfds, NULL, NULL, &timeout);
    if ((err != 0) && (err != -1))
    {
        if (FD_ISSET(ncp_fd, &readfds))
        {
            bytes = read(ncp_fd, buffer, *length);
            assert(bytes > 0);
            result = true;
        }
    }
    /* Update with actual number of bytes read */
    *length = bytes;
    return result;
}

void zbPlatUartFree(void)
{
    close(ncp_fd);
}
