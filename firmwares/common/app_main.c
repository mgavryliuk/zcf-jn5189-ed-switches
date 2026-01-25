#include "app_main.h"

#include <jendefs.h>

#include "JN5189.h"
#include "PDM.h"
#include "PWR_Interface.h"
#include "ZTimer.h"
#include "app_battery.h"
#include "app_buttons.h"
#include "app_leds.h"
#include "app_polling.h"
#include "app_zb_node.h"
#include "app_zcl_tick.h"
#include "bdb_api.h"
#include "device_config.h"
#include "fsl_debug_console.h"
#include "fsl_power.h"
#include "fsl_wwdt.h"
#include "pdum_gen.h"
#include "zps_apl_af.h"
#include "zps_apl_zdo.h"
#include "zps_gen.h"

static void PreSleep(void);
static void OnWakeUp(void);
static void WakeCallBack(void);
static void EnterMainLoop(void);
static void vAttemptToSleep(void);

extern void zps_taskZPS(void);
extern uint8_t mLPMFlag;

static PWR_tsWakeTimerEvent sWake;
static bool_t bActivityScheduled = FALSE;
static ZTIMER_tsTimer asTimers[ZTIMER_STORAGE];

static uint8_t u8WakeCounter = 0;

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
    PWR_vForceRadioRetention(FALSE);
    APP_MAIN_DBG("PWR_Init done.\n");
    PDUM_vInit();
    APP_MAIN_DBG("PDUM_vInit done.\n");
    ZTIMER_eInit(asTimers, sizeof(asTimers) / sizeof(ZTIMER_tsTimer));
    APP_MAIN_DBG("ZTIMER_eInit done with amount: %d.\n", ZTIMER_STORAGE);
    POLL_Init();
    BUTTONS_SW_Init();
    LEDS_Timers_Init();
    ZBNodeCallbacks_t zbNodeCallbacks = {
        .pfOnNWKSteeringStartCallback = LEDS_BlinkDuringNetworkSetup_Start,
        .pfOnNWKSteeringStopCallback = LEDS_BlinkDuringNetworkSetup_Stop,
    };
    ZB_NODE_Init(&zbNodeCallbacks);
    ZTIMER_eStart(g_u8ButtonScanTimerID, BUTTONS_SCAN_TIME_MSEC);
    memset(&sWake, 0x0, sizeof(sWake));
    EnterMainLoop();
}

static void PreSleep(void) {
    APP_MAIN_DBG("PreSleep called\n");
    ZTIMER_vSleep();
    if (device_config.bIsJoined) {
        vAppApiSaveMacSettings();
    }
    LEDS_EnableClamp();
#ifdef DEBUG_APP_ENABLED
    DbgConsole_Flush();
    DbgConsole_Deinit();
#endif
}

static void OnWakeUp(void) {
    APP_MAIN_DBG("On WakeUp called\n");
    ZTIMER_vWake();
    if (device_config.bIsJoined) {
        vAppApiRestoreMacSettings();
        ZPS_eAplAfSendKeepAlive();
        ZCLTick_Start();
        POLL_Start(&POLL_REGULAR_CONFIG);

        if (POWER_GetIoWakeStatus() & g_u32ButtonsInterruptMask) {
            APP_MAIN_DBG("Wake caused by button pressed!\n");
            ZTIMER_eStart(g_u8ButtonScanTimerID, BUTTONS_SCAN_TIME_MSEC);
        }
    }
}

static void WakeCallBack(void) {
    APP_MAIN_DBG("Wake callback called\n");
    bActivityScheduled = FALSE;
    if (device_config.bIsJoined) {
        if (u8WakeCounter == 0) {
            BATTERY_UpdateStatus();
        }
        u8WakeCounter = (u8WakeCounter + 1) % BATTERY_REPORT_EVERY_X_WAKEUPS;
    }
}

static void vAttemptToSleep(void) {
    if (!POLL_IsSleepAllowed()) {
        return;
    }

    uint16_t u16Activities = mLPMFlag;

    if (POLL_IsRunning() && u16Activities > 0)
        u16Activities--;

    if (ZCLTick_IsRunning() && u16Activities > 0)
        u16Activities--;

    if (u16Activities == 0) {
        POLL_Stop();
        ZCLTick_Stop();

        PWR_vWakeUpConfig(g_u32ButtonsInterruptMask);
        if (!device_config.bIsJoined) {
            APP_MAIN_DBG("Device is not in network. Going to deep sleep\n");
            (void)PWR_ChangeDeepSleepMode(PWR_E_SLEEP_OSCOFF_RAMOFF);
            PWR_Init();
            PWR_vForceRadioRetention(FALSE);
        } else {
            if (bActivityScheduled == FALSE) {
                APP_MAIN_DBG("Going to E_AHI_SLEEP_OSCON_RAMON sleep for %d seconds\n", MAXIMUM_TIME_TO_SLEEP_SEC);
                (void)PWR_ChangeDeepSleepMode(PWR_E_SLEEP_OSCON_RAMON);
                PWR_Init();
                PWR_vForceRadioRetention(TRUE);
                PWR_teStatus u8Status = PWR_eScheduleActivity(&sWake, MAXIMUM_TIME_TO_SLEEP_SEC * 1000, WakeCallBack);
                bActivityScheduled = TRUE;
                APP_MAIN_DBG("PWRM_eScheduleActivity status: %d\n", u8Status);
            } else {
                APP_MAIN_DBG("PWRM_eScheduleActivity is already running\n");
            }
        }
    }
}

static void EnterMainLoop(void) {
    while (1) {
        zps_taskZPS();
        bdb_taskBDB();
        ZTIMER_vTask();
        WWDT_Refresh(WWDT);
        vAttemptToSleep();
        PWR_EnterLowPower();
    }
}
