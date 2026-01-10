#ifndef APP_COMMON_H_
#define APP_COMMON_H_

#include "dbg.h"

#ifdef DEBUG_COMMON
#define TRACE_COMMON TRUE
#else
#define TRACE_COMMON FALSE
#endif

#define COMMON_DBG(...) DBG_vPrintf(TRACE_COMMON, "[COMMON] " __VA_ARGS__)

#endif /* APP_COMMON_H_ */
