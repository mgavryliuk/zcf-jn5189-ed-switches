#include "app_zcl_tick.h"

#include "ZTimer.h"
#include "zcl.h"
#include "zps_apl_af.h"

static void ZCLTick_Callback(void* ctx);

static uint8_t u8TickTimerID;

void ZCLTick_Init(void) {
    ZTIMER_eOpen(&u8TickTimerID, ZCLTick_Callback, NULL, ZTIMER_FLAG_PREVENT_SLEEP);
}

void ZCLTick_Start(void) {
    ZCL_TICK_DBG("ZCLTick_Start called\n");
    ZTIMER_eStart(u8TickTimerID, ZCL_TICK_TIME_MSEC);
}

void ZCLTick_Stop(void) {
    if (ZCLTick_IsRunning()) {
        ZCL_TICK_DBG("ZCLTick_Stop called\n");
        ZTIMER_eStop(u8TickTimerID);
    }
}

bool_t ZCLTick_IsRunning(void) {
    return ZTIMER_eGetState(u8TickTimerID) == E_ZTIMER_STATE_RUNNING;
}

static void ZCLTick_Callback(void* ctx) {
    ZCL_TICK_DBG("ZCLTick_Callback called\n");
    ZPS_tsAfEvent sStackEvent;
    tsZCL_CallBackEvent sCallBackEvent;
    sCallBackEvent.pZPSevent = &sStackEvent;
    sCallBackEvent.eEventType = E_ZCL_CBET_TIMER;
    vZCL_EventHandler(&sCallBackEvent);
    ZTIMER_eStart(u8TickTimerID, ZCL_TICK_TIME_MSEC);
}
