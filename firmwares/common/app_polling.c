#include "app_polling.h"

#include "ZTimer.h"
#include "zps_apl_zdo.h"

const PollingConfig_t POLL_COMMISIONING_CONFIG = {.eMode = POLL_COMMISIONING, .u16IntervalMs = 250, .u8MaxAttempts = 50};
const PollingConfig_t POLL_FAST_CONFIG = {.eMode = POLL_FAST, .u16IntervalMs = 250, .u8MaxAttempts = 3};
const PollingConfig_t POLL_REGULAR_CONFIG = {.eMode = POLL_REGULAR, .u16IntervalMs = 1000, .u8MaxAttempts = 3};

static void POLL_Callback(void* ctx);

static uint8_t u8PollTimerID;
static PollingState_t sPollState;

void POLL_Init(void) {
    ZTIMER_eOpen(&u8PollTimerID, POLL_Callback, NULL, ZTIMER_FLAG_PREVENT_SLEEP);
}

void POLL_Start(const PollingConfig_t* psConfig) {
    sPollState.u8Attempts = 0;
    POLL_DBG("Starting polling. Mode=%d Attempt=%d Interval=%dms\n", psConfig->eMode, sPollState.u8Attempts, psConfig->u16IntervalMs);
    sPollState.psConfig = psConfig;
    ZTIMER_eStop(u8PollTimerID);
    ZTIMER_eStart(u8PollTimerID, sPollState.psConfig->u16IntervalMs);
}

void POLL_Stop(void) {
    if (sPollState.psConfig == NULL)
        return;

    POLL_DBG("Stopping polling. Mode=%d Attempt=%d Interval=%dms\n", sPollState.psConfig->eMode, sPollState.u8Attempts,
             sPollState.psConfig->u16IntervalMs);
    if (POLL_IsRunning()) {
        ZTIMER_eStop(u8PollTimerID);
    }
    sPollState.psConfig = NULL;
    sPollState.u8Attempts = 0;
}

void POLL_ResetAttempts(void) {
    if (sPollState.psConfig == NULL)
        return;

    sPollState.u8Attempts = 0;
    POLL_DBG("Restarting polling. Mode=%d Attempt=%d Interval=%dms\n", sPollState.psConfig->eMode, sPollState.u8Attempts,
             sPollState.psConfig->u16IntervalMs);
}

bool_t POLL_IsSleepAllowed(void) {
    if (sPollState.psConfig == NULL)
        return TRUE;

    else if (sPollState.psConfig->eMode == POLL_REGULAR && sPollState.u8Attempts >= sPollState.psConfig->u8MaxAttempts)
        return TRUE;

    return FALSE;
}

bool_t POLL_IsRunning(void) {
    return ZTIMER_eGetState(u8PollTimerID) == E_ZTIMER_STATE_RUNNING;
}

const PollingConfig_t* POLL_GetConfig(void) {
    return sPollState.psConfig;
}

static void POLL_Callback(void* pvParam) {
    const PollingConfig_t* psCfg = sPollState.psConfig;
    sPollState.u8Attempts++;
    if (sPollState.u8Attempts == 255) {
        sPollState.u8Attempts = psCfg->u8MaxAttempts + 1;
        POLL_DBG("Polling attempts overfloat protection. Attempts=%d Mode=%d Interval=%dms\n", sPollState.u8Attempts, psCfg->eMode,
                 psCfg->u16IntervalMs);
    }

    POLL_DBG("Polling attempt %d: Mode=%d Interval=%dms\n", sPollState.u8Attempts, psCfg->eMode, psCfg->u16IntervalMs);
    ZPS_eAplZdoPoll();

    if (psCfg->u8MaxAttempts && sPollState.u8Attempts >= psCfg->u8MaxAttempts) {
        if (psCfg->eMode == POLL_COMMISIONING) {
            sPollState.u8Attempts = 0;
            sPollState.psConfig = &POLL_FAST_CONFIG;
        } else if (psCfg->eMode == POLL_FAST) {
            sPollState.u8Attempts = 0;
            sPollState.psConfig = &POLL_REGULAR_CONFIG;
        }
    }
    ZTIMER_eStart(u8PollTimerID, sPollState.psConfig->u16IntervalMs);
}
