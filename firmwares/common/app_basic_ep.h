#ifndef APP_BASIC_EP_H
#define APP_BASIC_EP_H

#include "Basic.h"
#include "ConfigurationCluster.h"
#include "Identify.h"
#include "PowerConfiguration.h"
#include "dbg.h"
#include "zcl.h"

#ifdef DEBUG_BASIC_EP
#define TRACE_BASIC_EP TRUE
#else
#define TRACE_BASIC_EP FALSE
#endif

#define BASIC_EP_DBG(...) DBG_vPrintf(TRACE_BASIC_EP, "[Basic Endpoint] " __VA_ARGS__)

typedef struct {
    tsZCL_ClusterInstance sBasicServer;
    tsZCL_ClusterInstance sIdentifyServer;
    tsZCL_ClusterInstance sPowerConfigurationServer;
    tsZCL_ClusterInstance sConfigurationServer;
} tsZHA_BasicEndpointClusterInstances __attribute__((aligned(4)));

typedef struct {
    tsZCL_EndPointDefinition sEndPoint;
    tsZHA_BasicEndpointClusterInstances sClusterInstance;

    tsCLD_Basic sBasicServerCluster;

    tsCLD_Identify sIdentifyServerCluster;
    tsCLD_IdentifyCustomDataStructure sIdentifyServerCustomDataStructure;

    tsCLD_PowerConfiguration sPowerConfigurationCluster;

    tsCLD_Configuration sConfigurationCluster;
} tsZHA_BasicEndpoint;

void BASIC_EP_Init(void);

#endif
