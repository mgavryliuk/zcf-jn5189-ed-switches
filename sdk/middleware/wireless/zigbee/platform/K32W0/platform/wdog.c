/*
* Copyright 2023 NXP
* All rights reserved.
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#include <fsl_wwdt.h>
#include <rom_pmc.h>
#include <dbg.h>

#ifndef WDT_CLK_FREQ
#define WDT_CLK_FREQ CLOCK_GetFreq(kCLOCK_WdtOsc)
#endif

/* Interrupt priorities */
#ifndef APP_BASE_INTERRUPT_PRIORITY
#define APP_BASE_INTERRUPT_PRIORITY (5)
#endif

#ifndef APP_WATCHDOG_PRIOIRTY
#define APP_WATCHDOG_PRIOIRTY       (1)
#endif

#ifndef WDT_GRACE_PERIODS
#define WDT_GRACE_PERIODS           (6)
#endif

#ifndef WDOG_WARN_VALUE
#define WDOG_WARN_VALUE             (512)
#endif

#ifndef WDOG_RESETS_DEVICE
#define WDOG_RESETS_DEVICE              (1)
#endif

static volatile uint8_t wdt_update_count = 0;

extern void ExceptionUnwindStack(uint32_t * pt);

static int (*wdog_int_pr_fp)(void *);
static int (*wdog_int_ep_fp)(void *);
static int (*wdog_reset_fp)(void *);

WEAK void ExceptionUnwindStack(uint32_t * pt)
{
    extern void *_vStackTop;

    PRINTF("\r\n stack dump:\r\n");
    while ((uint32)pt < (uint32)&_vStackTop )
    {
        PRINTF("0x%x  0x%x \r\n",pt, *pt);
        pt++;
        WWDT_Refresh(WWDT);
    }
    PRINTF("BBC_SM_STATE 0x400b107c = 0x%x\r\n",(*(volatile uint32 *)0x400b107c));
    PRINTF("BBC_TXCTL    0x400b11c0 = 0x%x\r\n",(*(volatile uint32 *)0x400b11c0));
    PRINTF("BBC_RXCTL    0x400b12c0 = 0x%x\r\n",(*(volatile uint32 *)0x400b12c0));
    PRINTF("PHY_MCTRL    0x400b0018 = 0x%x\r\n",(*(volatile uint32 *)0x400b0018));
#if defined(WDOG_RESETS_DEVICE) && (WDOG_RESETS_DEVICE == 1)
    PRINTF("APP: Watchdog RESETS device !\n");
#else
    PRINTF("APP: Device Halted !\n");
    RESET_PeripheralReset(kWWDT_RST_SHIFT_RSTn);
    WWDT_Deinit(WWDT);
#endif
    while (1);
}

static int zbWdogIntDefaultPrologue(void *p)
{
    uint32_t wdtStatus = WWDT_GetStatusFlags(WWDT);

    (void)p;

    /* The chip should reset before this happens. For this interrupt to occur,
     * it means that the WD timeout flag has been set but the reset has not occurred  */
    if (wdtStatus & kWWDT_TimeoutFlag)
    {
        /* A watchdog feed didn't occur prior to window timeout */
        /* Stop WDT */
        WWDT_Disable(WWDT);
        WWDT_ClearStatusFlags(WWDT, kWWDT_TimeoutFlag);
        DBG_vPrintf(TRUE, "Timeout Flag\r\n");
    }

    /* Handle warning interrupt */
    if (wdtStatus & kWWDT_WarningFlag)
    {
        /* A watchdog feed didn't occur prior to warning timeout */
        WWDT_ClearStatusFlags(WWDT, kWWDT_WarningFlag);
        if (wdt_update_count < WDT_GRACE_PERIODS)
        {
            /*
             * Feed only for the first WDT_GRACE_PERIODS warnings then allow
             * for a WDT reset to occur */
            wdt_update_count++;
            WWDT_Refresh(WWDT);
            DBG_vPrintf(TRUE,"Watchdog warning flag %d\r\n", wdt_update_count);
        }
        else
        {
            /* reset the count here */
            wdt_update_count = 0;
            ExceptionUnwindStack((uint32_t *) __get_MSP());
        }
    }
    return 0;
}

void zbPlatWdogKick()
{
    /* Kick the watchdog */
    WWDT_Refresh(WWDT);
    wdt_update_count = 0;
}

void zbPlatWdogInit()
{
    wwdt_config_t config;
    uint32_t wdtFreq;

    /* The WDT divides the input frequency into it by 4 */
    wdtFreq = WDT_CLK_FREQ >> 2 ;
    NVIC_EnableIRQ(WDT_BOD_IRQn);
    WWDT_GetDefaultConfig(&config);
    /* Replace default config values where required */
    /*
    * Set watchdog feed time constant to approximately 1s - 8 warnings
    * Set watchdog warning time to 512 ticks after feed time constant
    * Set watchdog window time to 1s
    */
    config.timeoutValue = wdtFreq * 1;
    config.warningValue = WDOG_WARN_VALUE;
    config.windowValue = wdtFreq * 1;
    /* Configure WWDT to reset on timeout */
    config.enableWatchdogReset = true;
    config.clockFreq_Hz = WDT_CLK_FREQ;
    WWDT_Init(WWDT, &config);
    /* First feed starts the watchdog */
    WWDT_Refresh(WWDT);

    NVIC_SetPriority(WDT_BOD_IRQn, APP_WATCHDOG_PRIOIRTY);
}

void zbPlatWdogDeInit()
{
    /* turn off watchdog timer whilst programming flash */
    /* Enable the WWDT clock. If it is already enabled, it doesn't hurt */
    CLOCK_EnableClock(kCLOCK_WdtOsc);
    RESET_PeripheralReset(kWWDT_RST_SHIFT_RSTn);
    WWDT_Deinit(WWDT);
}

void System_IRQHandler(void)
{
    if (wdog_int_pr_fp != NULL)
    {
        /* If the user registered its own WDOG handler, execute it */
        wdog_int_pr_fp(NULL);
    }
    else
    {
        /*
         * Otherwise, execute the default one which dumps stack &
         * resets/halts the CPU
        */
        zbWdogIntDefaultPrologue(NULL);
    }

    /*
     * We may have additional processing that's required by the user in its app,
     * especially if the WDOG INT is shared with other interrupts i.e. BOD.
     * So allow here to do some more processing. Of course, if the WDOG triggers
     * a reset or an infinite loop above,  then this will never get executed.
     *
     */
    if (wdog_int_ep_fp != NULL)
    {
        wdog_int_ep_fp(NULL);
    }
}

void zbPlatWdogIntRegisterEpilogue(int (*fp)(void *))
{
    if (fp != NULL)
    {
        wdog_int_ep_fp = fp;
    }
}

void zbPlatWdogIntRegisterPrologue(int (*fp)(void *))
{
    if (fp != NULL)
    {
        wdog_int_pr_fp = fp;
    }
}

void zbPlatWdogRegisterResetCheckCallback(int (*fp)(void *))
{
    if (fp != NULL)
    {
        wdog_reset_fp = fp;
    }
}

void zbPlatWdogResetCheckSource(void)
{
    /*
     * PMC->RESETCAUSE is not reliable, it will indicate a volatile state.
     * The solution is, thanks to AlexM to use the ROM API for getting the
     * actual reset source
     */
    uint32_t pmc_reset = pmc_reset_get_cause();
    if ((pmc_reset & PMC_RESETCAUSE_WDTRESET_MASK) ==
        PMC_RESETCAUSE_WDTRESET_MASK)
    {
        zbPlatWdogDeInit();
        if (wdog_reset_fp != NULL)
        {
            wdog_reset_fp(NULL);
        }
    }
}

