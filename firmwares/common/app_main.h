#ifndef APP_MAIN_H
#define APP_MAIN_H

#include "dbg.h"
#include "device_definitions.h"

#ifdef DEBUG_APP_MAIN
#define TRACE_APP_MAIN TRUE
#else
#define TRACE_APP_MAIN FALSE
#endif

#define APP_MAIN_DBG(...) DBG_vPrintf(TRACE_APP_MAIN, "[APP MAIN] " __VA_ARGS__)

// Wake Timer1 (28-bit) max ≈ 8192s, keep below limit
#define MAXIMUM_TIME_TO_SLEEP_SEC (60 * 60 * 2)  // 2h
// leds amount + 1 network config leds + 1 button pollings + 1 ZCL tick + 1 POLLING + BDB Timers + 1 WakeUp callback timer
#define ZTIMER_STORAGE (LEDS_AMOUNT + 1 + 1 + 1 + 1 + BDB_ZTIMER_STORAGE + 1)
// report battery and temperature on every 120 wake up, e.g. approx 240h (~10 days)
#define MEASURE_BATTERY_AND_TEMPERATURE_EVERY_X_WAKEUPS 120

#endif /* APP_MAIN_H */
