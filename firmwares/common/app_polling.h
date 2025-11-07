#ifndef APP_POLLING_H
#define APP_POLLING_H

#include "ZTimer.h"
#include "dbg.h"

#ifdef DEBUG_POLLING
#define TRACE_POLLING TRUE
#else
#define TRACE_POLLING FALSE
#endif

#define POLL_DBG(...) DBG_vPrintf(TRACE_POLLING, "[POLLING] " __VA_ARGS__)
#define POLL_TIMERS_AMOUNT (1)

typedef enum {
    POLL_COMMISIONING,
    POLL_FAST,
    POLL_REGULAR,
} PollMode_e;

typedef struct {
    PollMode_e eMode;
    uint16_t u16IntervalMs;
    uint8_t u8MaxAttempts;  // 0 = infinite
    bool bPreventSleep;
} PollingConfig_t;

typedef struct {
    const PollingConfig_t* psConfig;
    uint8_t u8Attempts;
} PollingState_t;

extern const PollingConfig_t POLL_COMMISIONING_CONFIG;
extern const PollingConfig_t POLL_REGULAR_CONFIG;
extern const PollingConfig_t POLL_FAST_CONFIG;

void POLL_Init(void);
void POLL_Start(const PollingConfig_t* psConfig);
void POLL_ResetAttempts(void);
bool_t POLL_IsSleepAllowed(void);
const PollingConfig_t* POLL_GetConfig(void);

#endif
