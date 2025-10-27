#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "app_buttons.h"
#include "app_leds.h"
#include "dbg.h"

#ifdef DEBUG_APP_MAIN
#define TRACE_APP_MAIN TRUE
#else
#define TRACE_APP_MAIN FALSE
#endif

#define APP_MAIN_DBG(...) DBG_vPrintf(TRACE_APP_MAIN, "[APP MAIN] " __VA_ARGS__)

// #define MAXIMUM_TIME_TO_SLEEP_SEC (60 * 60)
#define MAXIMUM_TIME_TO_SLEEP_SEC (10 * 1)
#define ZTIMER_STORAGE (LEDS_TIMERS_AMOUNT + BUTTONS_TIMERS_AMOUNT + BDB_ZTIMER_STORAGE)

#endif /* APP_MAIN_H */
