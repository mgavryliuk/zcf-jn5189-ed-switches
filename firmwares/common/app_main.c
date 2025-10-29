#include "app_main.h"

#include <jendefs.h>

#include "JN5189.h"
#include "PDM.h"
#include "PWR_Interface.h"
#include "ZTimer.h"
#include "app_battery.h"
#include "app_buttons.h"
#include "app_leds.h"
#include "app_zb_node.h"
#include "bdb_api.h"
#include "dbg.h"
#include "device_config.h"
#include "fsl_power.h"
#include "fsl_wwdt.h"
#include "pdum_gen.h"
#include "pwrm.h"
#include "zps_apl_af.h"
#include "zps_apl_zdo.h"
#include "zps_gen.h"

static void PreSleep(void);
static void OnWakeUp(void);
static void WakeCallBack(void);
static void EnterMainLoop(void);

extern void zps_taskZPS(void);

static PWR_tsWakeTimerEvent sWake;
static ZTIMER_tsTimer asTimers[ZTIMER_STORAGE];

void vAppRegisterPWRCallbacks(void) {
    PWR_RegisterLowPowerEnterCallback(PreSleep);
    PWR_RegisterLowPowerExitCallback(OnWakeUp);
}

void main_task(uint32_t parameter) {
    APP_MAIN_DBG("main_task called\n");
    PDM_eInitialise(1200, 63, NULL);
    APP_MAIN_DBG("PDM_eInitialise done.\n");
    (void)PWR_ChangeDeepSleepMode(PWR_E_SLEEP_OSCON_RAMON);
    PWR_Init();
    PWR_vForceRadioRetention(TRUE);
    APP_MAIN_DBG("PWR_Init done.\n");
    PDUM_vInit();
    APP_MAIN_DBG("PDUM_vInit done.\n");
    ZTIMER_eInit(asTimers, sizeof(asTimers) / sizeof(ZTIMER_tsTimer));
    APP_MAIN_DBG("ZTIMER_eInit done with amount: %d.\n", ZTIMER_STORAGE);
    ButtonCallbacks_t buttonCallbacks = {
        .pfOnPressCallback = LEDS_ButtonBlinkCallback,
    };
    BUTTONS_SW_Init(&buttonCallbacks);
    LEDS_Timers_Init();
    ZB_NODE_Init();
    // Update status also updates cluster, so we should run it after node is configured
    BATTERY_UpdateStatus();
    EnterMainLoop();
}

static void PreSleep(void) {
    DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: PreSleep called\n");
    DbgConsole_Flush();
    DbgConsole_Deinit();
    ZTIMER_vSleep();
    if (device_config.bIsJoined) {
        vAppApiSaveMacSettings();
    }
}

static void OnWakeUp(void) {
    DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: On WakeUp called\n");
    ZTIMER_vWake();
    vAppApiRestoreMacSettings();
    if (POWER_GetIoWakeStatus() & device_config.u32ButtonsInterruptMask) {
        DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: Button pressed: %08x\n", POWER_GetIoWakeStatus());
        ZTIMER_eStart(device_config.u8ButtonScanTimerID, BUTTONS_SCAN_TIME_MSEC);
    }
}

static void WakeCallBack(void) {
    DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: Wake callback called\n");
}

static void EnterMainLoop(void) {
    while (1) {
        zps_taskZPS();
        bdb_taskBDB();
        ZTIMER_vTask();
        // TODO: process app queues

        WWDT_Refresh(WWDT);

        // TODO: enter low power based on the timers
        PWR_eRemoveActivity(&sWake);
        // PWR_teStatus eStatus = PWR_eRemoveActivity(&sWake);
        // DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: PWR_eRemoveActivity status: %d\n", eStatus);
        PWR_vWakeUpConfig(device_config.u32ButtonsInterruptMask);
        PWR_eScheduleActivity(&sWake, MAXIMUM_TIME_TO_SLEEP_SEC * 1000, WakeCallBack);
        // eStatus = PWR_eScheduleActivity(&sWake, MAXIMUM_TIME_TO_SLEEP_SEC * 1000, WakeCallBack);
        // DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: PWR_eScheduleActivity status: %d\n", eStatus);
        PWR_EnterLowPower();
    }
}
