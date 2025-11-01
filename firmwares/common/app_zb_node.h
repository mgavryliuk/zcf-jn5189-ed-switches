#ifndef APP_NODE_H
#define APP_NODE_H

#include "ZQueue.h"

#ifdef DEBUG_NODE
#define TRACE_NODE TRUE
#else
#define TRACE_NODE FALSE
#endif

#define ZB_NODE_DBG(...) DBG_vPrintf(TRACE_NODE, "[ZB NODE] " __VA_ARGS__)

#define BDB_QUEUE_SIZE 5
#define TIMER_QUEUE_SIZE 8
#define MLME_QUEQUE_SIZE 8
#define MCPS_QUEUE_SIZE 20
#define MCPS_DCFM_QUEUE_SIZE 5

#define PDM_ID_NETWORK_STATE 0x8

typedef void (*NWKSteeringStart_cb_t)(void* ctx);
typedef void (*NWKSteeringStop_cb_t)(void);

typedef struct {
    NWKSteeringStart_cb_t pfOnNWKSteeringStartCallback;
    NWKSteeringStop_cb_t pfOnNWKSteeringStopCallback;
} ZBNodeCallbacks_t;

extern tszQueue APP_msgBdbEvents;

extern tszQueue zps_msgMlmeDcfmInd;
extern tszQueue zps_msgMcpsDcfmInd;
extern tszQueue zps_msgMcpsDcfm;
extern tszQueue zps_TimeEvents;

void ZB_NODE_Init(const ZBNodeCallbacks_t* callbacks);
void ZB_NODE_OnResetCallback(void);

#endif /* APP_NODE_H */
