#include "app_resources.h"

#include <jendefs.h>

#include "ZQueue.h"
#include "ZTimer.h"
#include "app_events.h"
#include "dbg.h"

static ZTIMER_tsTimer asTimers[APP_ZTIMER_STORAGE];

tszQueue APP_msgAppEvents;

static void APP_vInitTimers(void);
static void APP_vInitQueues(void);

void APP_Resources_Init(void) {
    DBG_vPrintf(TRACE_APP_RESOURCES, "APP_RESOURCES: APP_vInitResources called\n");
    APP_vInitTimers();
    APP_vInitQueues();
    DBG_vPrintf(TRACE_APP_RESOURCES, "APP_RESOURCES: APP_vInitResources finished\n");
}

static void APP_vInitTimers(void) {
    DBG_vPrintf(TRACE_APP_RESOURCES, "APP_RESOURCES: APP_vInitTimers called. Timers amount: %d\n", APP_ZTIMER_STORAGE);
    ZTIMER_eInit(asTimers, sizeof(asTimers) / sizeof(ZTIMER_tsTimer));
    LEDS_Timers_Init();
    BUTTONS_Timers_Init();
    DBG_vPrintf(TRACE_APP_RESOURCES, "APP_RESOURCES: APP_vInitTimers finished\n");
}

static void APP_vInitQueues(void) {
    DBG_vPrintf(TRACE_APP_RESOURCES, "APP_RESOURCES: APP_vInitQueues called\n");
    ZQ_vQueueCreate(&APP_msgAppEvents, APP_QUEUE_SIZE, sizeof(APP_tsEvent), NULL);
    DBG_vPrintf(TRACE_APP_RESOURCES, "APP_RESOURCES: APP_vInitQueues finished\n");
}
