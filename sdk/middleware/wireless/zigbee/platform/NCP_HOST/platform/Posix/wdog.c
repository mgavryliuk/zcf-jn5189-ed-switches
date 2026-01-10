/*
* Copyright 2024 NXP
* All rights reserved.
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <zb_platform.h>

#ifndef WDOG_TIMEOUT_PERIOD
#define WDOG_TIMEOUT_PERIOD             (6)     /* seconds */
#endif

#ifndef WDOG_RESETS_DEVICE
#define WDOG_RESETS_DEVICE              (1)
#endif

static int (*wdog_int_pr_fp)(void *);
static int (*wdog_int_ep_fp)(void *);
static int (*wdog_reset_fp)(void *);

static void ExceptionUnwindStack(uint32_t * pt)
{

}

static int zbWdogIntDefaultPrologue(void *p)
{
    return 0;
}

void WDOG0_IRQHandler(void)
{

}

void zbPlatWdogIntRegisterEpilogue(int (*fp)(void *))
{

}

void zbPlatWdogIntRegisterPrologue(int (*fp)(void *))
{

}

void zbPlatWdogRegisterResetCheckCallback(int (*fp)(void *))
{

}

void zbPlatWdogKick(void)
{

}

void zbPlatWdogInit(void)
{

}

void zbPlatWdogDeInit(void)
{

}

void zbPlatWdogResetCheckSource(void)
{

}