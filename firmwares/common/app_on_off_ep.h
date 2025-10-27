#ifndef APP_ON_OFF_EP_H
#define APP_ON_OFF_EP_H

#include "Groups.h"
#include "MultistateInputBasic.h"
#include "OnOff.h"
#include "dbg.h"
#include "zcl.h"

#ifdef DEBUG_ON_OFF_EP
#define TRACE_ON_OFF_EP TRUE
#else
#define TRACE_ON_OFF_EP FALSE
#endif

#define ON_OFF_EP_DBG(...) DBG_vPrintf(TRACE_ON_OFF_EP, "[OnOff EP] " __VA_ARGS__)

typedef struct {
    tsZCL_ClusterInstance sOnOffClient;
    tsZCL_ClusterInstance sGroupsClient;
    tsZCL_ClusterInstance sMultistateInputServer;
} tsZHA_OnOffEndpointClusterInstances __attribute__((aligned(4)));

typedef struct {
    tsZCL_EndPointDefinition sEndPoint;
    tsZHA_OnOffEndpointClusterInstances sClusterInstance;

    tsCLD_OnOffClient sOnOffClientCluster;
    tsCLD_MultistateInputBasic sMultistateInputServerCluster;

    tsCLD_Groups sGroupsClientCluster;
    tsCLD_GroupsCustomDataStructure sGroupsClientCustomDataStructure;
} tsZHA_OnOffEndpoint;

void OnOff_EP_Init(void);

#endif /* APP_ON_OFF_EP_H */
