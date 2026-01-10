/*****************************************************************************
 *
 * MODULE:      	   PDUM
 *
 * COMPONENT:          pdum_dbg.c
 *
 * DESCRIPTION:        Manages application protcol data units
 *
 *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright Jennic Ltd. 2007 All rights reserved
 *
 ****************************************************************************/

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

#include "jendefs.h"

/* force debug on when building library call for debugging */
#ifndef DBG_ENABLE
#define DBG_ENABLE
#endif

#include "pdum_apl.h"
#include "pdum_nwk.h"
#include "dbg.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#ifndef TRACE_PDUM
#define TRACE_PDUM FALSE
#endif

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

PRIVATE void vDumpPayload(unsigned int num, uint8 *pu8Payload);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Public Functions                                     ***/
/****************************************************************************/

PUBLIC void PDUM_vDBGPrintAPduInstance(PDUM_thAPduInstance hAPduInst)
{
    DBG_vPrintf(TRACE_PDUM, "APDU Instance:");

    if (NULL != hAPduInst)
    {
        unsigned int i;

        for (i = 0; i < PDUM_u16APduInstanceGetPayloadSize(hAPduInst); i++)
        {
            if (0 == (i % 16))
            {
                DBG_vPrintf(TRACE_PDUM, "\r\n\t");
            }
            DBG_vPrintf(TRACE_PDUM, "%02x ", ((uint8 *)PDUM_pvAPduInstanceGetPayload(hAPduInst))[i]);
        }
        DBG_vPrintf(TRACE_PDUM, "\r\n");
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, "\tINVALID HANDLE!\r\n");
    }
}

PUBLIC void PDUM_vDBGPrintNPdu(PDUM_thNPdu hNPdu)
{
    DBG_vPrintf(TRACE_PDUM, "NPDU ");
    if (NULL != hNPdu)
    {
        if (PDUM_NPDU_ASCENDING(hNPdu))
        {
            DBG_vPrintf(TRACE_PDUM, "ascending :\n");
        }
        else
        { /* descending */
            DBG_vPrintf(TRACE_PDUM, "descending :\n");
        }

        DBG_vPrintf(TRACE_PDUM, "\tHDR -> ");
        vDumpPayload(PDUM_u8NPduGetHeaderSize(hNPdu), (uint8 *)PDUM_pvNPduGetHeader(hNPdu));

        DBG_vPrintf(TRACE_PDUM, "\n\tDAT -> ");
        if (PDUM_NPDU_ASCENDING(hNPdu))
        {
            pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;
            vDumpPayload(PDUM_u8NPduGetDataSize(hNPdu), &psNPdu->au8PayloadStorage[psNPdu->u8Data]);
        }
        else
        {
            pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;
            vDumpPayload(PDUM_NPDU_SIZE - psNPdu->u8Header, &psNPdu->au8PayloadStorage[psNPdu->u8Header]);
            vDumpPayload(psNPdu->u8Footer, psNPdu->au8PayloadStorage);
        }

        DBG_vPrintf(TRACE_PDUM, "\n\tFTR -> ");
        vDumpPayload(PDUM_u8NPduGetFooterSize(hNPdu), (uint8 *)PDUM_pvNPduGetFooter(hNPdu));
        DBG_vPrintf(TRACE_PDUM, "\n");
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, "\tINVALID HANDLE!\n");
    }
}

/****************************************************************************/
/***        Exported Private Functions                                      */
/****************************************************************************/

/****************************************************************************/
/***        Local Functions                                                 */
/****************************************************************************/

PRIVATE void vDumpPayload(unsigned int num, uint8 *pu8Payload)
{
    unsigned int i;

    for (i = 0; i < num; i++)
    {
        DBG_vPrintf(TRACE_PDUM, "%02x ", pu8Payload[i]);
    }
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
