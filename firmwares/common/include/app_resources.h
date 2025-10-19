#ifndef APP_RESOURCES_H
#define APP_RESOURCES_H

#include <jendefs.h>

#include "ZQueue.h"
#include "device_definitions.h"

#ifdef DEBUG_APP_RESOURCES
#define TRACE_APP_RESOURCES TRUE
#else
#define TRACE_APP_RESOURCES FALSE
#endif

#define APP_ZTIMER_STORAGE 1 + LEDS_TIMERS_AMOUN
#define APP_QUEUE_SIZE 10

#define ZCL_TICK_TIME_MSEC ZTIMER_TIME_MSEC(1000)

extern tszQueue APP_msgAppEvents;

void APP_Resources_Init(void);

#endif /* APP_RESOURCES_H */
