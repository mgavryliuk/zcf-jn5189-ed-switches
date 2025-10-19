#include "app_main.h"

#include <jendefs.h>

#include "JN5189.h"
#include "PDM.h"
#include "PWR_Interface.h"
#include "ZTimer.h"
#include "app_buttons.h"
#include "app_leds.h"
#include "app_resources.h"
#include "dbg.h"
#include "device_config.h"
#include "device_definitions.h"
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
    LEDS_Timers_Init();

    PDM_eInitialise(1200, 63, NULL);
    DBG_vPrintf(TRUE, "APP_MAIN: PDM Init\n");

    PWR_ChangeDeepSleepMode(PWR_E_SLEEP_OSCON_RAMON);
    PWR_Init();
    DBG_vPrintf(TRUE, "APP_MAIN: PWR Init\n");
    // APP_Buttons_cbTimerScan(NULL);
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
    if (POWER_GetIoWakeStatus() & BTN_CTRL_MASK) {
        DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: Button pressed: %08x\n", POWER_GetIoWakeStatus());
        // ZTIMER_eStart(u8TimerButtonScan, BUTTON_SCAN_TIME_MSEC);
    }
}

static void WakeCallBack(void) {
    DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: Wake callback called\n");
    for (uint8_t i = 0; i < LEDS_AMOUNT; i++) {
        LED_Blink(leds_configs[i]);
    }
}

static void EnterMainLoop(void) {
    PWR_vWakeUpConfig(BTN_CTRL_MASK);
    for (uint8_t i = 0; i < LEDS_AMOUNT; i++) {
        LED_Blink(leds_configs[i]);
    }
    while (1) {
        PWR_teStatus eStatus = PWR_eRemoveActivity(&sWake);
        DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: PWR_eRemoveActivity status: %d\n", eStatus);
        eStatus = PWR_eScheduleActivity(&sWake, MAXIMUM_TIME_TO_SLEEP_SEC * 1000, WakeCallBack);
        DBG_vPrintf(TRACE_APP_MAIN, "APP_MAIN: PWR_eScheduleActivity status: %d\n", eStatus);
        ZTIMER_vTask();
        WWDT_Refresh(WWDT);
        PWR_EnterLowPower();
    }
}
