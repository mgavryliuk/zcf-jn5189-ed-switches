/*****************************************************************************
 *
 * MODULE:             PDU Manager
 *
 * COMPONENT:          pdum_private.h
 *
 * DESCRIPTION:        PDU manager private functions
 *
 *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright Jennic Ltd. 2007 All rights reserved
 *
 ****************************************************************************/

#ifndef PDUM_PRIVATE_H_
#define PDUM_PRIVATE_H_

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

#include "jendefs.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

struct pdum_tsAPdu_tag
{
    struct pdum_tsAPduInstance_tag *psAPduInstances;
    uint16                          u16FreeListHeadIdx;
    uint16                          u16Size;
    uint16                          u16NumInstances;
};

struct pdum_tsAPduInstance_tag
{
    uint8 *au8Storage;
    uint16 u16Size;
    uint16 u16NextAPduInstIdx;
    uint16 u16APduIdx;
};

typedef struct pdum_tsAPdu_tag         pdum_tsAPdu;
typedef struct pdum_tsAPduInstance_tag pdum_tsAPduInstance;

/****************************************************************************/
/***        External Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

PUBLIC uint16 pdum_u16WriteNBO(uint8 *pu8Data, const char *szFormat, va_list *pArgs);
PUBLIC uint16 pdum_u16WriteStrNBO(uint8 *pu8Data, const char *szFormat, void *pvStruct);
PUBLIC uint16 pdum_u16ReadNBO(uint8 *pu8Struct, const char *szFormat, uint8 *pu8Data);

/****************************************************************************/
/***        Extended error codes from the Zigbee Pro stack                ***/
/*** Must be kept in sync with zps_nwk_pub.h                              ***/
/*** If the PDUM needs to be built for use without the Zigbee Pro stack   ***/
/*** these should become a separate build.  Having a separate call back   ***/
/*** for the PDU Manager would be neater but would cost code size and     ***/
/*** add complexity for customers who write applications                  ***/
/****************************************************************************/
typedef enum ZPS_teExtendedStatus_tag
{
    ZPS_XS_E_NO_FREE_NPDU =
        0x80, // 0x80 There are no free Network PDUs to process the request.  This is set in the PDU Manager
    ZPS_XS_E_NO_FREE_APDU =
        0x81, // 0x81 There are no free Application PDUs to process the request.  This is set in the PDU Manager
} ZPS_teExtendedStatus;

PUBLIC void ZPS_vSetExtendedStatus(ZPS_teExtendedStatus eExtendedStatus);

#endif /* PDUM_PRIVATE_H_ */

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
