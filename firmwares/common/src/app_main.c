#include "app_main.h"

#include <jendefs.h>

#include "JN5189.h"
#include "PDM.h"
#include "PWR_Interface.h"
#include "ZTimer.h"
#include "app_battery.h"
#include "app_buttons.h"
#include "app_leds.h"
#include "app_resources.h"
#include "dbg.h"
#include "device_config.h"
#include "fsl_power.h"
#include "fsl_wwdt.h"
#include "pwrm.h"

static void PreSleep(void);
static void OnWakeUp(void);
static void WakeCallBack(void);
static void EnterMainLoop(void);

static PWR_tsWakeTimerEvent sWake;

void vAppRegisterPWRCallbacks(void) {
    PWR_RegisterLowPowerEnterCallback(PreSleep);
    PWR_RegisterLowPowerExitCallback(OnWakeUp);
}

void main_task(uint32_t parameter) {
    DBG_vPrintf(TRUE, "APP_MAIN: main_task called\n");
    APP_Resources_Init();

    PDM_eInitialise(1200, 63, NULL);
    DBG_vPrintf(TRUE, "APP_MAIN: PDM Init\n");

    PWR_ChangeDeepSleepMode(PWR_E_SLEEP_OSCON_RAMON);
    PWR_Init();
    DBG_vPrintf(TRUE, "APP_MAIN: PWR Init\n");
    ZTIMER_eStart(device_config.u8ButtonScanTimerID, BUTTON_SCAN_TIME_MSEC);
    EnterMainLoop();
}

static void PreSleep(void) {
    DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: PreSleep called\n");
    DbgConsole_Flush();
    DbgConsole_Deinit();
    ZTIMER_vSleep();
}

static void OnWakeUp(void) {
    DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: On WakeUp called\n");
    ZTIMER_vWake();
    if (POWER_GetIoWakeStatus() & device_config.u32ButtonsInterruptMask) {
        DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: Button pressed: %08x\n", POWER_GetIoWakeStatus());
        ZTIMER_eStart(device_config.u8ButtonScanTimerID, BUTTON_SCAN_TIME_MSEC);
    }
}

static void WakeCallBack(void) {
    DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: Wake callback called\n");
}

static void EnterMainLoop(void) {
    while (1) {
        // TODO: make it later better
        PWR_eRemoveActivity(&sWake);
        // DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: PWR_eRemoveActivity status: %d\n", eStatus);
        PWR_vWakeUpConfig(device_config.u32ButtonsInterruptMask);
        PWR_eScheduleActivity(&sWake, MAXIMUM_TIME_TO_SLEEP_SEC * 1000, WakeCallBack);
        // DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: PWR_eScheduleActivity status: %d\n", eStatus);
        ZTIMER_vTask();
        WWDT_Refresh(WWDT);
        PWR_EnterLowPower();
    }
}
