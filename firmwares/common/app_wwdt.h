#ifndef APP_WWDT_H
#define APP_WWDT_H

#include "dbg.h"

#ifdef DEBUG_APP_WWDT
#define TRACE_WWDT TRUE
#else
#define TRACE_WWDT FALSE
#endif

#define WWDT_DEBUG(...) DBG_vPrintf(TRACE_WWDT, "[WWDT] " __VA_ARGS__)

void APP_WWDT_Init(void);

#endif /* APP_WWDT_H */
