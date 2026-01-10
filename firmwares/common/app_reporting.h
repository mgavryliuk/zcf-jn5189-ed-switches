#ifndef APP_REPORTING_H
#define APP_REPORTING_H

#include "dbg.h"
#include "zcl.h"

#ifdef DEBUG_REPORTING
#define TRACE_REPORTING TRUE
#else
#define TRACE_REPORTING FALSE
#endif

#define REPORTING_DBG(...) DBG_vPrintf(TRACE_REPORTING, "[Reporting] " __VA_ARGS__)

typedef struct {
    uint16 u16ClusterID;
    bool_t bManufacturerSpecific;
    tsZCL_AttributeReportingConfigurationRecord sAttributeReportingConfigurationRecord;
} tsReports;

void REPORTING_MakeSupportedAttributesReportable(void);

#endif /* APP_REPORTING_H */
