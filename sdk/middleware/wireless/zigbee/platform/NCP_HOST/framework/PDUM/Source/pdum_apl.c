/*****************************************************************************
 *
 * MODULE:             PDUM
 *
 * COMPONENT:          pdum_apl.c
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

#include <jendefs.h>
#include "dbg.h"

#include <stdint.h>
#include "pdum_private.h"
#include "pdum_apl.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#ifndef TRACE_PDUM
#define TRACE_PDUM FALSE
#endif

#ifndef TRACE_ASSERTS
#define TRACE_ASSERTS FALSE
#endif

#define PDUM_NULL_IDX  (0xffff) /* list terminator */
#define PDUM_ALLOC_IDX (0xeeee) /* indicates APDU instance has been allocated */

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

// PRIVATE OS_thMutex s_hMutex;
PRIVATE uint16 s_u16NumAPdus;
PRIVATE pdum_tsAPdu *                  s_asAPduPool;
PRIVATE pfCallbackNotificationApduType pfCallbackApduNotification = NULL;
/****************************************************************************/
/***        Exported Public Functions                                     ***/
/****************************************************************************/

PUBLIC uint16 PDUM_u16APduInstanceReadNBO(PDUM_thAPduInstance hAPduInst,
                                          uint16              u16Pos,
                                          const char *        szFormat,
                                          void *              pvStruct)
{
    if (hAPduInst && szFormat && pvStruct)
    {
        pdum_tsAPduInstance *psAPduInst = (pdum_tsAPduInstance *)hAPduInst;

        DBG_vPrintf(TRACE_PDUM, "start pos = %d\n", u16Pos);
        DBG_vPrintf(TRACE_PDUM, "format size = %d\n", PDUM_u16SizeNBO(szFormat));
        DBG_vPrintf(TRACE_PDUM, "instance size = %d\n", psAPduInst->u16Size);

        if ((u16Pos + PDUM_u16SizeNBO(szFormat) - 1) < psAPduInst->u16Size)
        {
            uint8 *pu8Start = &(psAPduInst->au8Storage[u16Pos]);

            return pdum_u16ReadNBO(pvStruct, szFormat, pu8Start);
        }
    }

    return 0;
}

PUBLIC uint16 PDUM_u16APduInstanceWriteNBO(PDUM_thAPduInstance hAPduInst, uint16 u16Pos, const char *szFormat, ...)
{
    uint16 u16Size = 0;
    if (hAPduInst && szFormat)
    {
        pdum_tsAPduInstance *psAPduInst  = (pdum_tsAPduInstance *)hAPduInst;
        uint16               u16APduSize = s_asAPduPool[psAPduInst->u16APduIdx].u16Size;

        //      DBG_vPrintf(TRACE_PDUM, "start pos = %d\n", u16Pos);
        //      DBG_vPrintf(TRACE_PDUM, "format size = %d\n", PDUM_u16SizeNBO(szFormat));
        //      DBG_vPrintf(TRACE_PDUM, "max size = %d\n", u16APduSize);

        if ((u16Pos + PDUM_u16SizeNBO(szFormat) - 1) < u16APduSize)
        {
            va_list args;
            va_start(args, szFormat);
            uint8 *pu8Start = &psAPduInst->au8Storage[u16Pos];

            u16Size = pdum_u16WriteNBO(pu8Start, szFormat, &args);
            va_end(args);
        }
    }

    return u16Size;
}

PUBLIC uint16 PDUM_u16APduInstanceWriteStrNBO(PDUM_thAPduInstance hAPduInst,
                                              uint16              u16Pos,
                                              const char *        szFormat,
                                              void *              pvStruct)
{
    if (hAPduInst && szFormat && pvStruct)
    {
        pdum_tsAPduInstance *psAPduInst  = (pdum_tsAPduInstance *)hAPduInst;
        uint16               u16APduSize = s_asAPduPool[psAPduInst->u16APduIdx].u16Size;

        if ((u16Pos + PDUM_u16SizeNBO(szFormat) - 1) < u16APduSize)
        {
            uint8 *pu8Start = &psAPduInst->au8Storage[u16Pos];

            return pdum_u16WriteStrNBO(pu8Start, szFormat, pvStruct);
        }
    }

    return 0;
}

PUBLIC void *PDUM_pvAPduInstanceGetPayload(PDUM_thAPduInstance hAPduInst)
{
    return (hAPduInst) ? ((pdum_tsAPduInstance *)hAPduInst)->au8Storage : PDUM_INVALID_HANDLE;
}

#if 0
PUBLIC void * PDUM_pvAPduInstanceGetState (PDUM_thAPduInstance hAPduInst)
{
    pdum_tsAPduInstance *psAPduInst = (pdum_tsAPduInstance *)hAPduInst;

    /* state is stored on the end of the data */
    return (hAPduInst) ? &psAPduInst->au8Storage[s_asAPduPool[psAPduInst->u16APduIdx].u16Size] : PDUM_INVALID_HANDLE;
}
#endif

PUBLIC uint16 PDUM_u16APduInstanceGetPayloadSize(PDUM_thAPduInstance hAPduInst)
{
    /* state is stored on the end of the data */
    return (hAPduInst) ? ((pdum_tsAPduInstance *)hAPduInst)->u16Size : 0;
}

PUBLIC PDUM_teStatus PDUM_eAPduInstanceSetPayloadSize(PDUM_thAPduInstance hAPduInst, uint16 u16Size)
{
    if (PDUM_INVALID_HANDLE != hAPduInst)
    {
        if (u16Size <= s_asAPduPool[((pdum_tsAPduInstance *)hAPduInst)->u16APduIdx].u16Size)
        {
            ((pdum_tsAPduInstance *)hAPduInst)->u16Size = u16Size;
            return PDUM_E_OK;
        }
    }
    return PDUM_E_APDU_INSTANCE_TOO_BIG;
}

PUBLIC PDUM_thAPduInstance PDUM_hAPduAllocateAPduInstance(PDUM_thAPdu hAPdu)
{
    pdum_tsAPdu *       psAPdu       = (pdum_tsAPdu *)hAPdu;
    PDUM_thAPduInstance hNewInstance = PDUM_INVALID_HANDLE;

    // if (NULL != s_hMutex) {
    //    DBG_vAssert(TRACE_ASSERTS, OS_E_OK == OS_eEnterCriticalSection(s_hMutex));
    //}
    /* either got or don't need the mutex */
    if (psAPdu->u16FreeListHeadIdx != PDUM_NULL_IDX)
    {
        uint16 u16NewIdx                                      = psAPdu->u16FreeListHeadIdx;
        psAPdu->u16FreeListHeadIdx                            = psAPdu->psAPduInstances[u16NewIdx].u16NextAPduInstIdx;
        psAPdu->psAPduInstances[u16NewIdx].u16NextAPduInstIdx = PDUM_ALLOC_IDX;
        psAPdu->psAPduInstances[u16NewIdx].u16Size            = 0;

        DBG_vPrintf(TRACE_PDUM, "PDUM: Allocated APDU %d instance %d.\n", psAPdu->psAPduInstances[u16NewIdx].u16APduIdx,
                    u16NewIdx);

        hNewInstance = (PDUM_thAPduInstance) & (psAPdu->psAPduInstances[u16NewIdx]);
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, "PDUM: No instances left to allocate.\n");
        ZPS_vSetExtendedStatus(ZPS_XS_E_NO_FREE_APDU);
    }

    //      vDumpAPduPool();

    // if (NULL != s_hMutex) {
    //    DBG_vAssert(TRACE_ASSERTS, OS_E_OK == OS_eExitCriticalSection(s_hMutex));
    //}

    return hNewInstance;
}

PUBLIC PDUM_teStatus PDUM_eAPduFreeAPduInstance(PDUM_thAPduInstance hAPduInst)
{
    pdum_tsAPduInstance *psAPduInst = (pdum_tsAPduInstance *)hAPduInst;
    PDUM_teStatus        eStatus    = PDUM_E_OK;

    // if (NULL != s_hMutex) {
    //    DBG_vAssert(TRACE_ASSERTS, OS_E_OK == OS_eEnterCriticalSection(s_hMutex));
    //}
    if (PDUM_INVALID_HANDLE != hAPduInst)
    { /* PR #19 - Check for NULL */
        if (PDUM_ALLOC_IDX == psAPduInst->u16NextAPduInstIdx)
        {
            pdum_tsAPdu *psAPdu = &s_asAPduPool[psAPduInst->u16APduIdx];

            uint16 u16APduInstIdx = (psAPduInst - psAPdu->psAPduInstances); /* PR #18 - C sorts out divide by size */

            DBG_vPrintf(TRACE_PDUM, "PDUM: Freeing APDU %d instance %d\n", psAPduInst->u16APduIdx, u16APduInstIdx);

            if (u16APduInstIdx < psAPdu->u16NumInstances)
            {
                psAPduInst->u16NextAPduInstIdx = psAPdu->u16FreeListHeadIdx;
                psAPdu->u16FreeListHeadIdx     = u16APduInstIdx;
                if (pfCallbackApduNotification)
                {
                    uint8 i;
                    uint8 count = 0;
                    for (i = 0; i < psAPdu->u16NumInstances; i++)
                    {
                        if (psAPdu->psAPduInstances[i].u16NextAPduInstIdx == PDUM_ALLOC_IDX)
                        {
                            count++;
                        }
                    }
                    if (count == 0)
                    {
                        pfCallbackApduNotification(psAPduInst->u16APduIdx);
                    }
                }
            }
            else
            {
                eStatus = PDUM_E_INTERNAL_ERROR;
            }
        }
        else
        {
            eStatus = PDUM_E_APDU_INSTANCE_ALREADY_FREE;
        }
    }
    else
    {
        eStatus = PDUM_E_INVALID_HANDLE; /* PR #19 */
    }

    // if (NULL != s_hMutex) {
    //    DBG_vAssert(TRACE_ASSERTS, OS_E_OK == OS_eExitCriticalSection(s_hMutex));
    //}

    return eStatus;
}

PUBLIC PDUM_thAPdu PDUM_thAPduInstanceGetApdu(PDUM_thAPduInstance hAPduInst)
{
    pdum_tsAPduInstance *psAPduInst = (pdum_tsAPduInstance *)hAPduInst;
    pdum_tsAPdu *        psAPdu     = NULL;

    if (PDUM_INVALID_HANDLE != hAPduInst)
    {
        psAPdu = &s_asAPduPool[psAPduInst->u16APduIdx];
    }

    return psAPdu;
}

PUBLIC uint16 PDUM_u16APduGetSize(PDUM_thAPdu hAPdu)
{
    return (hAPdu) ? ((pdum_tsAPdu *)hAPdu)->u16Size : 0;
}

/****************************************************************************/
/***        Exported Private Functions                                    ***/
/****************************************************************************/

PUBLIC void pdum_vAPduInit(pdum_tsAPdu *asAPduPool, uint16 u16NumAPdus)
{
    unsigned int a, i = 0;

    s_u16NumAPdus = u16NumAPdus;
    s_asAPduPool  = asAPduPool;

    DBG_vPrintf(TRACE_PDUM, "Initialising APDU pool size %d ptr %08x\n", u16NumAPdus, s_asAPduPool);

    /* initialise free list for all APDUs */
    for (a = 0; a < u16NumAPdus; a++)
    {
        pdum_tsAPdu *psAPdu = &s_asAPduPool[a];

        if (psAPdu->u16NumInstances > 0)
        {
            for (i = 0; i < psAPdu->u16NumInstances - 1; i++)
            {
                psAPdu->psAPduInstances[i].u16APduIdx         = a;
                psAPdu->psAPduInstances[i].u16NextAPduInstIdx = i + 1;
            }
            psAPdu->psAPduInstances[i].u16APduIdx         = a; /* PR #17 initialise last element correctly */
            psAPdu->psAPduInstances[i].u16NextAPduInstIdx = PDUM_NULL_IDX;
        }
    }

    //  vDumpAPduPool();

    DBG_vPrintf(TRACE_PDUM, "Done.\n", u16NumAPdus);
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

#if 0
void vDumpAPduPool(void)
{
    unsigned int a, i;

    for (a = 0; a < s_u16NumAPdus; a++) {
        pdum_tsAPdu *psAPdu = &s_asAPduPool[a];

        DBG_vPrintf(TRACE_PDUM, "APDU %d\n", a);

        DBG_vPrintf(TRACE_PDUM, "\n\tpsAPduInstances = %08x\n", psAPdu->psAPduInstances);
        DBG_vPrintf(TRACE_PDUM, "\tu16FreeListHeadIdx = %d\n", psAPdu->u16FreeListHeadIdx);
        DBG_vPrintf(TRACE_PDUM, "\tu16Size = %d\n", psAPdu->u16Size);
        DBG_vPrintf(TRACE_PDUM, "\tu16NumInstances = %d\n", psAPdu->u16NumInstances);

        if (psAPdu->u16NumInstances > 0) {
            for (i = 0; i < psAPdu->u16NumInstances; i++) {
                DBG_vPrintf(TRACE_PDUM, "\n\t\tAPDU Instance %d\n", i);
                DBG_vPrintf(TRACE_PDUM, "\t\tau8Storage = %08x\n", psAPdu->psAPduInstances[i].au8Storage);
                DBG_vPrintf(TRACE_PDUM, "\t\tu16APduIdx = %08x\n", psAPdu->psAPduInstances[i].u16APduIdx);
                DBG_vPrintf(TRACE_PDUM, "\t\tu16NextAPduInstIdx = %08x\n", psAPdu->psAPduInstances[i].u16NextAPduInstIdx);
                DBG_vPrintf(TRACE_PDUM, "\t\tu16Size = %08x\n", psAPdu->psAPduInstances[i].u16Size);
            }
        }
    }
}
#endif

PUBLIC void ZPS_vNwkRegisterApduNotification(void *pvFnCallback)
{
    pfCallbackApduNotification = (pfCallbackNotificationApduType)pvFnCallback;
}
/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
