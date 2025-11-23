#ifndef ZCL_TICK_H
#define ZCL_TICK_H

#include "ZTimer.h"
#include "dbg.h"
#include "jendefs.h"

#ifdef DEBUG_ZCL_TICK
#define TRACE_ZCL_TICK TRUE
#else
#define TRACE_ZCL_TICK FALSE
#endif

#define ZCL_TICK_DBG(...) DBG_vPrintf(TRACE_ZCL_TICK, "[ZCL TICK] " __VA_ARGS__)

#define ZCL_TICK_TIME_MSEC ZTIMER_TIME_MSEC(1000)

void ZCLTick_Init(void);
void ZCLTick_Start(void);
void ZCLTick_Stop(void);
bool_t ZCLTick_IsRunning(void);

#endif /* ZCL_TICK_H */
