#include "app_wwdt.h"

#include "fsl_clock.h"
#include "fsl_power.h"
#include "fsl_wwdt.h"

void APP_WWDT_Init(void) {
    WWDT_DEBUG("Configuring WWDT...\n");
    NVIC_EnableIRQ(WDT_BOD_IRQn);
    uint32_t wdtClockFrequency;
    uint32_t wdtFreq;
    wwdt_config_t config;
    wdtClockFrequency = CLOCK_GetFreq(kCLOCK_WdtOsc);
    wdtFreq = wdtClockFrequency >> 2;

    WWDT_GetDefaultConfig(&config);
    if (((PMC->RESETCAUSE) & PMC_RESETCAUSE_WDTRESET_MASK) == PMC_RESETCAUSE_WDTRESET_MASK) {
        RESET_PeripheralReset(kWWDT_RST_SHIFT_RSTn);
        WWDT_Deinit(WWDT);
        WWDT_DEBUG("Watchdog timer has reset device!\n");
        POWER_ClearResetCause();
        while (1) {
        }
    }

    config.timeoutValue = wdtFreq * 1;
    config.warningValue = 512;
    config.windowValue = wdtFreq * 1;
    config.enableWatchdogReset = true;
    config.clockFreq_Hz = wdtClockFrequency;
    WWDT_Init(WWDT, &config);
    WWDT_Refresh(WWDT);
    WWDT_DEBUG("WWDT Configured!\n");
}
