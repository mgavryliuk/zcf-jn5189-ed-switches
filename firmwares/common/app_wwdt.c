#include "app_wwdt.h"

#include <jendefs.h>

#include "JN5189.h"
#include "dbg.h"
#include "fsl_clock.h"
#include "fsl_wwdt.h"

void APP_WWDT_Init(void) {
    DBG_vPrintf(TRACE_WWDT, "WWDT: Setup WWDT called\n");
    uint32_t wdtClockFrequency;
    uint32_t wdtFreq;
    wwdt_config_t config;
    wdtClockFrequency = CLOCK_GetFreq(kCLOCK_WdtOsc);
    wdtFreq = wdtClockFrequency >> 4;

    WWDT_GetDefaultConfig(&config);
    config.timeoutValue = wdtFreq * 1;
    config.warningValue = 512;
    config.windowValue = wdtFreq * 1;
    config.enableWatchdogReset = true;
    config.clockFreq_Hz = wdtClockFrequency;
    WWDT_Init(WWDT, &config);
    WWDT_Refresh(WWDT);
}
