#ifndef APP_WWDT_H
#define APP_WWDT_H

#ifdef DEBUG_APP_WWDT
#define TRACE_WWDT TRUE
#else
#define TRACE_WWDT FALSE
#endif

void APP_WWDT_Init(void);

#endif /* APP_WWDT_H */
