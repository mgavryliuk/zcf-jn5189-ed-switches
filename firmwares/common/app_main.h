#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "app_polling.h"
#include "dbg.h"
#include "device_definitions.h"

#ifdef DEBUG_APP_MAIN
#define TRACE_APP_MAIN TRUE
#else
#define TRACE_APP_MAIN FALSE
#endif

#define APP_MAIN_DBG(...) DBG_vPrintf(TRACE_APP_MAIN, "[APP MAIN] " __VA_ARGS__)

// #define MAXIMUM_TIME_TO_SLEEP_SEC (60 * 60)
#define MAXIMUM_TIME_TO_SLEEP_SEC (10 * 1)
// leds amount + 1 network config leds + 1 button pollings + 1 ZCL tick + 1 POLLING + BDB Timers
#define ZTIMER_STORAGE (LEDS_AMOUNT + 1 + 1 + 1 + 1 + BDB_ZTIMER_STORAGE)

#endif /* APP_MAIN_H */
