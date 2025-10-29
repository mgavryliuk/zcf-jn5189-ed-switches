#include "dbg.h"
#include "fsl_wwdt.h"

#define WWDT_ERROR_DELAY 1000000U

static volatile uint8_t wdt_update_count = 0;

void System_IRQHandler(void) {
    uint32_t wdtStatus = WWDT_GetStatusFlags(WWDT);
    /* The chip should reset before this happens. For this interrupt to occur,
     * it means that the WD timeout flag has been set but the reset has not occurred  */
    if (wdtStatus & kWWDT_TimeoutFlag) {
        /* A watchdog feed didn't occur prior to window timeout */
        /* Stop WDT */
        WWDT_Disable(WWDT);
        WWDT_ClearStatusFlags(WWDT, kWWDT_TimeoutFlag);
        DBG_vPrintf(TRUE, "Watchdog timeout flag\n");
    }

    /* Handle warning interrupt */
    if (wdtStatus & kWWDT_WarningFlag) {
        /* A watchdog feed didn't occur prior to warning timeout */
        WWDT_ClearStatusFlags(WWDT, kWWDT_WarningFlag);
        if (wdt_update_count < 7) {
            wdt_update_count++;
            WWDT_Refresh(WWDT);
            DBG_vPrintf(TRUE, "Watchdog warning flag %d\n", wdt_update_count);
        } else {
            DBG_vPrintf(TRUE, "Watchdog last warning %d\n", wdt_update_count);
        }
    }
}
