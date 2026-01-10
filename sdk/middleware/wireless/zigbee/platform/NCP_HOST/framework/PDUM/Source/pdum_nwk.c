/*****************************************************************************
 *
 * MODULE:             PDUM
 *
 * COMPONENT:          pdum_nwk.c
 *
 * DESCRIPTION:        Manages network protcol data units
 *
 ****************************************************************************
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright Jennic Ltd. 2007 All rights reserved
 *
 ****************************************************************************/

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

#include <jendefs.h>
#include <string.h>
#include <MicroSpecific.h>
#include "dbg.h"

#include "pdum_private.h"
#include "pdum_nwk.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#ifndef TRACE_PDUM
#define TRACE_PDUM FALSE
#endif

#ifndef TRACE_NPDU
#define TRACE_NPDU FALSE
#endif

#ifndef TRACE_ASSERTS
#define TRACE_ASSERTS FALSE
#endif

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

#ifdef PDUM_PEDANTIC_CHECKS
PRIVATE bool bNPduInFreeList(PDUM_thNPdu hNPdu);
PUBLIC void  vCheckNPduPoolValid(void);
#endif

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/* pool of free NPDUs */
PRIVATE pdum_tsNPdu *s_psFreeListHead;
PRIVATE pdum_tsNPdu *s_psNPduPoolStart;
PRIVATE pdum_tsNPdu *s_psNPduPoolEnd;
// PRIVATE OS_thMutex s_hMutex;
//#define TRACE_NPDU_MAX  TRUE
#if TRACE_NPDU_MAX
PRIVATE uint8 u8Count   = 0;
PRIVATE uint8 u8MaxNpdu = 0;
#endif

PRIVATE PDUM_teStatus              eAllocateNPDU(PDUM_thNPdu *phNPdu);
PRIVATE PDUM_teStatus              eNPduFree(PDUM_thNPdu hNPdu);
PRIVATE pfCallbackNotificationType pfCallbackNpduNotification = NULL;
PRIVATE uint8                      u8MaxNpduCount             = 0;
/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME: PDUM_eNPduAllocate
 *
 * DESCRIPTION:
 * Allocate an NPDU from the free pool
 *
 * PARAMETERS:      Name            RW  Usage
 *
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/
PUBLIC PDUM_teStatus PDUM_eNPduAllocate(PDUM_thNPdu *phNPdu) /* [I SP001259_sfr 102] */
{
    PDUM_teStatus eStatus = PDUM_E_INVALID_HANDLE; /* [I SP001259_sfr 107] */

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Allocating NPDU ...");

    // if (NULL != s_hMutex) {
    //    DBG_vAssert(TRACE_ASSERTS, OS_E_OK == OS_eEnterCriticalSection(s_hMutex));
    //}
    eStatus = eAllocateNPDU(phNPdu);

    return eStatus;
}

PUBLIC PDUM_teStatus PDUM_eNPduAllocateFromISR(PDUM_thNPdu *phNPdu) /* [I SP001259_sfr 102] */
{
    PDUM_teStatus eStatus = PDUM_E_INVALID_HANDLE; /* [I SP001259_sfr 107] */
#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Allocating NPDU in ISR ...");

    // if (NULL != s_hMutex) {
    //    DBG_vAssert(TRACE_ASSERTS, OS_E_OK == OS_eEnterCriticalSection(s_hMutex));
    //}
    eStatus = eAllocateNPDU(phNPdu);
    return eStatus;
}

PRIVATE PDUM_teStatus eAllocateNPDU(PDUM_thNPdu *phNPdu)
{
    register uint32 u32Interrupts = 0;

    MICRO_DISABLE_AND_SAVE_INTERRUPTS(u32Interrupts);

    PDUM_teStatus eStatus = PDUM_E_OK;
    if (NULL != phNPdu)
    {
        if (NULL != s_psFreeListHead)
        {
            /* [I SP001259_sfr 103] begin */
            pdum_tsNPdu *psNPdu = s_psFreeListHead;
            s_psFreeListHead    = s_psFreeListHead->psNext;
            psNPdu->psNext      = NULL;
            psNPdu->u8Tag       = 0;
            /* [I SP001259_sfr 103] end */

            *phNPdu = (PDUM_thNPdu)psNPdu; /* [I SP001259_sfr 104] */
#if TRACE_NPDU_MAX
            if (++u8Count > u8MaxNpdu)
            {
                u8MaxNpdu = u8Count;
            }
#endif
#if TRACE_NPDU
            DBG_vPrintf(TRACE_NPDU, "G=%d h=%p ", u8Count, *phNPdu);
#endif
            DBG_vPrintf(TRACE_PDUM, " OK 0x%08x\r\n", *phNPdu);
        }
        else
        {
            DBG_vPrintf(TRACE_PDUM, " EXHAUSTED\r\n");
            *phNPdu = NULL;
            eStatus = PDUM_E_NPDUS_EXHAUSTED; /* [I SP001259_sfr 105] */
            ZPS_vSetExtendedStatus(ZPS_XS_E_NO_FREE_NPDU);
        }
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, " BAD PARAM\r\n");
        eStatus = PDUM_E_BAD_PARAM; /* [I SP001259_sfr 106] */
    }

    MICRO_RESTORE_INTERRUPTS(u32Interrupts);

    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eNPduClone
 *
 * DESCRIPTION:
 * Clone an NPDU from the free pool
 *
 * PARAMETERS:      Name            RW  Usage
 *
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/
PUBLIC PDUM_teStatus PDUM_eNPduClone(PDUM_thNPdu *phNPdu, PDUM_thNPdu hNPdu) /* [I SP001259_sfr 102] */
{
    PDUM_teStatus eStatus = PDUM_E_OK; /* [I SP001259_sfr 107] */

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Cloning NPDU ...");

    eStatus = PDUM_eNPduAllocate(phNPdu);

    if (PDUM_E_OK == eStatus)
    {
        (*phNPdu)->u8Tag        = hNPdu->u8Tag;
        (*phNPdu)->u8Header     = hNPdu->u8Header;
        (*phNPdu)->u8Footer     = hNPdu->u8Footer;
        (*phNPdu)->u8ClaimedHdr = hNPdu->u8ClaimedHdr;
        (*phNPdu)->u8ClaimedFtr = hNPdu->u8ClaimedFtr;
        memcpy((*phNPdu)->au8PayloadStorage, hNPdu->au8PayloadStorage, PDUM_NPDU_SIZE);
    }

    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eNPduInitDescending
 *
 * DESCRIPTION:
 * Initialise a descending NPDU
 *
 * PARAMETERS:      Name            RW  Usage
 *                  phNPdu          R   NPDU handle
 *                  u8DataSize      R   Size of data
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/
PUBLIC PDUM_teStatus PDUM_eNPduInitDescending(PDUM_thNPdu hNPdu) /* [I SP001259_sfr 108,109,110] */
{
    PDUM_teStatus eStatus = PDUM_E_OK; /* [I SP001259_sfr 160] */

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Init Descending NPDU 0x%08x ...", hNPdu);

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (NULL != hNPdu)
    {
        pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;

        psNPdu->u8Header     = PDUM_NPDU_SIZE; /* [I SP001259_sfr 111] */
        psNPdu->u8ClaimedHdr = PDUM_NPDU_SIZE; /* [I SP001259_sfr ?] */
        psNPdu->u8Footer     = 0;              /* [I SP001259_sfr 113] */
        psNPdu->u8ClaimedFtr = 0;              /* [I SP001259_sfr 111,113] */

        DBG_vPrintf(TRACE_PDUM, " OK\r\n");
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, " INVALID HANDLE\r\n");
        eStatus = PDUM_E_INVALID_HANDLE; /* [I SP001259_sfr 162] */
    }

    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eNPduInitAscending
 *
 * DESCRIPTION:
 * Initialise an ascending NPDU
 *
 * PARAMETERS:      Name            RW  Usage
 *                  phNPdu          R   NPDU handle
 *                  u8PayloadSize   R   Size of payload
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/
PUBLIC PDUM_teStatus PDUM_eNPduInitAscending(PDUM_thNPdu hNPdu, uint8 u8PayloadSize) /* [I SP001259_sfr 114,115,116] */
{
    PDUM_teStatus eStatus = PDUM_E_OK; /* [I SP001259_sfr 118] */

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Init Ascending NPDU 0x%08x ...", hNPdu);

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (NULL != hNPdu)
    {
        if (PDUM_NPDU_SIZE >= u8PayloadSize)
        { /* PR #14,#15 - Allow NPDU of maximum size */
            pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;

            psNPdu->u8Data       = 0;             /* [I SP001259_sfr 165] */
            psNPdu->u8Header     = 0;             /* [I SP001259_sfr 164] */
            psNPdu->u8Footer     = u8PayloadSize; /* [I SP001259_sfr 166] */
            psNPdu->u8ClaimedFtr = u8PayloadSize; /* [I SP001259_sfr 164,166] */
#if 0
            psNPdu->u8Total = u8PayloadSize; /* [I SP001259_sfr ?] */
#endif
            DBG_vPrintf(TRACE_PDUM, " OK\r\n");
        }
        else
        {
            DBG_vPrintf(TRACE_PDUM, " TOO BIG\r\n");
            eStatus = PDUM_E_NPDU_TOO_BIG; /* [I SP001259_sfr 117] */
        }
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, " INVALID HANDLE\r\n");
        eStatus = PDUM_E_INVALID_HANDLE; /* [I SP001259_sfr 163] */
    }

    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eNPduFree
 *
 * DESCRIPTION:
 * Return an NPDU to the free pool
 *
 * PARAMETERS:      Name            RW  Usage
 *                  phNPdu          R   NPDU handle
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/
PUBLIC PDUM_teStatus PDUM_eNPduFree(PDUM_thNPdu hNPdu) /* [I SP001259_sfr 119,120] */
{
    PDUM_teStatus eStatus = PDUM_E_INVALID_HANDLE; /* [I SP001259_sfr 124] */

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Freeing NPDU 0x%08x...", hNPdu);

    // if (NULL != s_hMutex) {
    //    DBG_vAssert(TRACE_ASSERTS, OS_E_OK == OS_eEnterCriticalSection(s_hMutex));
    //}
    eStatus = eNPduFree(hNPdu);
    return eStatus;
}

PUBLIC PDUM_teStatus PDUM_eNPduFreeFromISR(PDUM_thNPdu hNPdu) /* [I SP001259_sfr 119,120] */
{
    PDUM_teStatus eStatus = PDUM_E_INVALID_HANDLE; /* [I SP001259_sfr 124] */
#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Freeing NPDU 0x%08x in ISR...", hNPdu);

    // if (NULL != s_hMutex) {
    //    DBG_vAssert(TRACE_ASSERTS, OS_E_OK == OS_eEnterCriticalSection(s_hMutex));
    //}

    eStatus = eNPduFree(hNPdu);
    return eStatus;
}

PRIVATE PDUM_teStatus eNPduFree(PDUM_thNPdu hNPdu)
{
    register uint32 u32Interrupts  = 0;
    PDUM_teStatus   eStatus        = PDUM_E_OK;
    uint8           u8CurrentCount = 1; /* Taking current NPDU into account */
    MICRO_DISABLE_AND_SAVE_INTERRUPTS(u32Interrupts);

    /* PR #12 - Bound check NPDU pointer */
    if (hNPdu >= s_psNPduPoolStart && hNPdu < s_psNPduPoolEnd)
    {
        pdum_tsNPdu *psNPdu     = (pdum_tsNPdu *)hNPdu;
        pdum_tsNPdu *psNPduIter = s_psFreeListHead;

        while (NULL != psNPduIter)
        {
            if (psNPdu == psNPduIter)
            {
                eStatus = PDUM_E_NPDU_ALREADY_FREE;
                break;
            }
            psNPduIter = psNPduIter->psNext;
            u8CurrentCount++; /* These are already on the free list */
        }

        if (PDUM_E_OK == eStatus)
        {
            /* add back to free pool list */
            /* [I SP001259_sfr 123] begin */
            psNPdu->psNext   = s_psFreeListHead;
            psNPdu->u8Tag    = PDUM_NPDU_FREETAG;
            s_psFreeListHead = psNPdu;
#if TRACE_NPDU_MAX
            u8Count--;
#endif
#if TRACE_NPDU
            DBG_vPrintf(TRACE_NPDU, "F=%d h=%p\n", u8Count, psNPdu);
#endif
            DBG_vPrintf(TRACE_PDUM, " OK\r\n");
            if (u8CurrentCount == u8MaxNpduCount)
            {
                if (pfCallbackNpduNotification)
                {
                    pfCallbackNpduNotification();
                }
            }
        }

        /* [I SP001259_sfr 123] end */
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, " INVALID HANDLE\r\n");
        eStatus = PDUM_E_INVALID_HANDLE; /* [I SP001259_sfr 121] */
    }
    MICRO_RESTORE_INTERRUPTS(u32Interrupts);
    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eNPduClaimHeader
 *
 * DESCRIPTION:
 * Claim a section of header for a particular layer
 *
 * PARAMETERS:      Name            RW  Usage
 *                  phNPdu          R   NPDU handle
 *                  u8HeaderSize    R   Size of header to claim
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/
PUBLIC PDUM_teStatus PDUM_eNPduClaimHeader(PDUM_thNPdu hNPdu, uint8 u8HeaderSize) /* [I SP001259_sfr 125,126,127] */
{
    PDUM_teStatus eStatus = PDUM_E_OK; /* [I SP001259_sfr 129] */

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Claiming NPDU 0x%08x header of %d bytes ...", hNPdu, u8HeaderSize);

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (NULL != hNPdu)
    {
        pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;

        /* [I SP001259_sfr 167] begin */
        if (PDUM_NPDU_ASCENDING(hNPdu))
        {
            //            psNPdu->u8Claimed += u8HeaderSize;
            psNPdu->u8Data += u8HeaderSize;
        }
        else
        {
            psNPdu->u8ClaimedHdr -= u8HeaderSize;
        }
        /* [I SP001259_sfr 167] end */

        DBG_vPrintf(TRACE_PDUM, " OK\r\n");
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, " INVALID HANDLE\r\n");
        eStatus = PDUM_E_INVALID_HANDLE; /* [I SP001259_sfr 128] */
    }

    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eNPduClaimFooter
 *
 * DESCRIPTION:
 * Claim a section of footer for a particular layer
 *
 * PARAMETERS:      Name            RW  Usage
 *                  phNPdu          R   NPDU handle
 *                  u8FooterSize    R   Size of footer to claim
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/
PUBLIC PDUM_teStatus PDUM_eNPduClaimFooter(PDUM_thNPdu hNPdu, uint8 u8FooterSize) /* [I SP001259_sfr 133,134,135] */
{
    PDUM_teStatus eStatus = PDUM_E_OK; /* [I SP001259_sfr 137] */

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Claiming NPDU 0x%08x footer of %d bytes ...", hNPdu, u8FooterSize);

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (NULL != hNPdu)
    {
        pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;

        /* [I SP001259_sfr 168] begin */
        if (PDUM_NPDU_ASCENDING(hNPdu))
        {
            psNPdu->u8ClaimedFtr -= u8FooterSize;
        }
        else
        {
            psNPdu->u8ClaimedFtr += u8FooterSize;
        }
        /* [I SP001259_sfr 168] end */

        DBG_vPrintf(TRACE_PDUM, " OK\r\n");
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, " INVALID HANDLE\r\n");
        eStatus = PDUM_E_INVALID_HANDLE; /* [I SP001259_sfr 136] */
    }

    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eNPduClaimData
 *
 * DESCRIPTION:
 * Claim a section of data for a particular layer
 *
 * PARAMETERS:      Name            RW  Usage
 *                  hNPdu           R   NPDU handle
 *                  u8DataSize      R   Size of data to claim
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/

PUBLIC PDUM_teStatus PDUM_eNPduClaimData(PDUM_thNPdu hNPdu, uint8 u8DataSize)
{
    PDUM_teStatus eStatus = PDUM_E_OK;

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (PDUM_NPDU_SIZE > u8DataSize)
    {
        if (hNPdu)
        {
            if (PDUM_NPDU_DESCENDING(hNPdu))
            { /* should only set data size of descending NPDUs */
                pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;

                if (psNPdu->u8ClaimedFtr > psNPdu->u8Footer)
                {
                    /* must copy footer to increase / decrease size of data
                     * copy data in reverse order as we should always be increasing size
                     * and the source and destination may overlap
                     */

                    uint8        u8FooterSize = psNPdu->u8ClaimedFtr - psNPdu->u8Footer;
                    uint8 *      pu8Src       = &psNPdu->au8PayloadStorage[psNPdu->u8Footer + u8FooterSize];
                    uint8 *      pu8Dest      = &psNPdu->au8PayloadStorage[u8DataSize + u8FooterSize];
                    unsigned int i;

                    for (i = 0; i < u8FooterSize; i++)
                    {
                        *--pu8Dest = *--pu8Src;
                    }
                }

                psNPdu->u8Footer += u8DataSize;
                psNPdu->u8ClaimedFtr += u8DataSize;
            }
            else
            {
                eStatus = PDUM_E_BAD_DIRECTION;
            }
        }
        else
        {
            eStatus = PDUM_E_INVALID_HANDLE;
        }
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, " TOO BIG\r\n");
        eStatus = PDUM_E_NPDU_TOO_BIG; /* [I SP001259_sfr 161] */
    }

    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eNPduAscend
 *
 * DESCRIPTION:
 * Ascend an NPDU up to the next higher layer
 *
 * PARAMETERS:      Name            RW  Usage
 *                  phNPdu          R   NPDU handle
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/
PUBLIC PDUM_teStatus PDUM_eNPduAscend(PDUM_thNPdu hNPdu) /* [I SP001259_sfr 144,145] */
{
    PDUM_teStatus eStatus = PDUM_E_OK; /* [I SP001259_sfr 148] */

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Ascending NPDU 0x%08x...", hNPdu);

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (NULL != hNPdu)
    {
        pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;

        if (PDUM_NPDU_ASCENDING(hNPdu))
        {
            /* [I SP001259_sfr 147] begin */
            psNPdu->u8Header = psNPdu->u8Data;
            psNPdu->u8Footer = psNPdu->u8ClaimedFtr;
            /* [I SP001259_sfr 148] end */

            DBG_vPrintf(TRACE_PDUM, " OK\r\n");
        }
        else
        {
            DBG_vPrintf(TRACE_PDUM, " BAD DIR\r\n");
            eStatus = PDUM_E_BAD_DIRECTION; /* [I SP001259_sfr 169] */
        }
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, " INVALID HANDLE\r\n");
        eStatus = PDUM_E_INVALID_HANDLE; /* [I SP001259_sfr 146] */
    }

    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eNPduDescend
 *
 * DESCRIPTION:
 * Descend an NPDU down to the next lower layer
 *
 * PARAMETERS:      Name            RW  Usage
 *                  phNPdu          R   NPDU handle
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/
PUBLIC PDUM_teStatus PDUM_eNPduDescend(PDUM_thNPdu hNPdu) /* [I SP001259_sfr 149,150] */
{
    PDUM_teStatus eStatus = PDUM_E_OK; /* [I SP001259_sfr 153] */

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Descending NPDU 0x%08x...", hNPdu);

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (NULL != hNPdu)
    {
        if (PDUM_NPDU_DESCENDING(hNPdu))
        {
            pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;

            /* [I SP001259_sfr 152] begin */
            psNPdu->u8Footer = psNPdu->u8ClaimedFtr;
            psNPdu->u8Header = psNPdu->u8ClaimedHdr;
            /* [I SP001259_sfr 152] end */

            DBG_vPrintf(TRACE_PDUM, " OK\r\n");
        }
        else
        {
            DBG_vPrintf(TRACE_PDUM, " BAD DIR\r\n");
            eStatus = PDUM_E_BAD_DIRECTION; /* [I SP001259_sfr 170] */
        }
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, " INVALID HANDLE\r\n");
        eStatus = PDUM_E_INVALID_HANDLE; /* [I SP001259_sfr 151] */
    }

    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eWriteNPduHeaderNWB
 *
 * DESCRIPTION:
 * Write to the header section of an NPDU in network byte order
 *
 * PARAMETERS:      Name            RW  Usage
 *                  hNPdu           R   NPDU handle
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/

PUBLIC PDUM_teStatus PDUM_eNPduPrependHeaderNBO(PDUM_thNPdu hNPdu, const char *szFormat, ...)
{
    PDUM_teStatus eStatus = PDUM_E_OK;

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (hNPdu && szFormat)
    {
        if (PDUM_NPDU_DESCENDING(hNPdu))
        { /* should only write to descending NPDUs */
            va_list      args;
            pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;
            uint8 *      pu8Start;

            va_start(args, szFormat);

            /* write to header is pre-decrement of u8ClaimedHdr */
            PDUM_eNPduClaimHeader(hNPdu, PDUM_u16SizeNBO(szFormat));
            pu8Start = &psNPdu->au8PayloadStorage[psNPdu->u8ClaimedHdr];
            pdum_u16WriteNBO(pu8Start, szFormat, &args);

            va_end(args);
        }
        else
        {
            eStatus = PDUM_E_BAD_DIRECTION;
        }
    }
    else
    {
        eStatus = PDUM_E_INVALID_HANDLE;
    }

    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eWriteNPduFooterNBO
 *
 * DESCRIPTION:
 * Write to the footer section of an NPDU in network byte order
 *
 * PARAMETERS:      Name            RW  Usage
 *                  hNPdu           R   NPDU handle
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/

PUBLIC PDUM_teStatus PDUM_eNPduAppendFooterNBO(PDUM_thNPdu hNPdu, const char *szFormat, ...)
{
    PDUM_teStatus eStatus = PDUM_E_OK;

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (hNPdu && szFormat)
    {
        if (PDUM_NPDU_DESCENDING(hNPdu))
        { /* should only write to descending NPDUs */
            va_list      args;
            pdum_tsNPdu *psNPdu   = (pdum_tsNPdu *)hNPdu;
            uint8 *      pu8Start = &psNPdu->au8PayloadStorage[psNPdu->u8ClaimedFtr];

            va_start(args, szFormat);

            /* write to footer is post increment of u8ClaimedFtr */
            PDUM_eNPduClaimFooter(hNPdu, PDUM_u16SizeNBO(szFormat));
            pdum_u16WriteNBO(pu8Start, szFormat, &args);

            va_end(args);
        }
        else
        {
            eStatus = PDUM_E_BAD_DIRECTION;
        }
    }
    else
    {
        eStatus = PDUM_E_INVALID_HANDLE;
    }
    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eWriteNPduDataNBO
 *
 * DESCRIPTION:
 * Write to the data section of an NPDU in network byte order
 *
 * PARAMETERS:      Name            RW  Usage
 *                  hNPdu           R   NPDU handle
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/

PUBLIC PDUM_teStatus PDUM_eNPduAppendDataNBO(PDUM_thNPdu hNPdu, const char *szFormat, ...)
{
    PDUM_teStatus eStatus = PDUM_E_OK;

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (hNPdu && szFormat)
    {
        if (PDUM_NPDU_DESCENDING(hNPdu))
        { /* should only write to descending NPDUs */
            va_list      args;
            pdum_tsNPdu *psNPdu   = (pdum_tsNPdu *)hNPdu;
            uint8 *      pu8Start = &psNPdu->au8PayloadStorage[psNPdu->u8Footer];

            va_start(args, szFormat);

            eStatus = PDUM_eNPduClaimData(hNPdu, PDUM_u16SizeNBO(szFormat));

            if (PDUM_E_OK == eStatus)
            {
                pdum_u16WriteNBO(pu8Start, szFormat, &args);
            }
            va_end(args);
        }
        else
        {
            eStatus = PDUM_E_BAD_DIRECTION;
        }
    }
    else
    {
        eStatus = PDUM_E_INVALID_HANDLE;
    }
    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eReadNPduHeaderNBO
 *
 * DESCRIPTION:
 * Read from the header section of an NPDU in network byte order
 *
 * PARAMETERS:      Name            RW  Usage
 *                  hNPdu           R   NPDU handle
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/

PUBLIC PDUM_teStatus PDUM_eNPduReadHeaderNBO(void *pvStruct, const char *szFormat, PDUM_thNPdu hNPdu)
{
    PDUM_teStatus eStatus = PDUM_E_OK;

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (pvStruct && hNPdu && szFormat)
    {
        if (PDUM_NPDU_ASCENDING(hNPdu))
        { /* should only read from ascending NPDUs */
            //          uint8 *pu8Start = PDUM_pu8NPduGetData(hNPdu); /* data ptr is start of header to be claimed */
            pdum_tsNPdu *psNPdu   = (pdum_tsNPdu *)hNPdu;
            uint8 *      pu8Start = &psNPdu->au8PayloadStorage[psNPdu->u8Data];

            /* read of header is post increment of claimed header */
            PDUM_eNPduClaimHeader(hNPdu, PDUM_u16SizeNBO(szFormat));
            pdum_u16ReadNBO((uint8 *)pvStruct, szFormat, pu8Start);
        }
        else
        {
            eStatus = PDUM_E_BAD_DIRECTION;
        }
    }
    else
    {
        eStatus = PDUM_E_INVALID_HANDLE;
    }

    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eReadNPduFooterNBO
 *
 * DESCRIPTION:
 * Read from the footer section of an NPDU in network byte order
 *
 * PARAMETERS:      Name            RW  Usage
 *                  hNPdu           R   NPDU handle
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/

PUBLIC PDUM_teStatus PDUM_eNPduReadFooterNBO(void *pvStruct, const char *szFormat, PDUM_thNPdu hNPdu)
{
    PDUM_teStatus eStatus = PDUM_E_OK;

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (pvStruct && hNPdu && szFormat)
    {
        if (PDUM_NPDU_ASCENDING(hNPdu))
        { /* should only read from ascending NPDUs */
            pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;
            uint8 *      pu8Start;

            /* read of footer is pre-decrement of u8ClaimedFtr */
            PDUM_eNPduClaimFooter(hNPdu, PDUM_u16SizeNBO(szFormat));
            pu8Start = &psNPdu->au8PayloadStorage[psNPdu->u8ClaimedFtr];
            pdum_u16ReadNBO((uint8 *)pvStruct, szFormat, pu8Start);
        }
        else
        {
            eStatus = PDUM_E_BAD_DIRECTION;
        }
    }
    else
    {
        eStatus = PDUM_E_INVALID_HANDLE;
    }

    return eStatus;
}

/****************************************************************************
 *
 * NAME: PDUM_eNPduWriteToBuffer
 *
 * DESCRIPTION:
 * Write the contents of an NPDU into a buffer provided
 *
 * PARAMETERS:      Name            RW  Usage
 *                  hNPdu           R   NPDU handle
 *
 * RETURNS:
 * Status
 *
 * NOTES:
 * None.
 ****************************************************************************/

PUBLIC PDUM_teStatus PDUM_eNPduWriteToBuffer(PDUM_thNPdu hNPdu, uint8 *pu8Buffer)
{
    PDUM_teStatus eStatus = PDUM_E_OK;

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Writing NPDU out to a buffer ... ");

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (pu8Buffer)
    {
        if (hNPdu)
        {
            if (PDUM_NPDU_DESCENDING(hNPdu))
            {
                /* copy out header section, then data and footer sections */
                pdum_tsNPdu *psNPdu      = (pdum_tsNPdu *)hNPdu;
                uint8 *      pu8StartHdr = &psNPdu->au8PayloadStorage[psNPdu->u8ClaimedHdr];
                uint8        u8SizeHdr   = PDUM_NPDU_SIZE - psNPdu->u8ClaimedHdr;

                memcpy(pu8Buffer, pu8StartHdr, u8SizeHdr);
                memcpy(&pu8Buffer[u8SizeHdr], psNPdu->au8PayloadStorage, psNPdu->u8ClaimedFtr);
            }
            else
            {
                DBG_vPrintf(TRACE_PDUM, "ERROR BAD DIRECTION\n");
                eStatus = PDUM_E_BAD_DIRECTION;
            }
        }
        else
        {
            eStatus = PDUM_E_INVALID_HANDLE; /* PR #16 - Return correct status for invalid NPDU handle */
        }
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, "ERROR BAD PARAMETER\n");
        eStatus = PDUM_E_BAD_PARAM;
    }

    DBG_vPrintf(TRACE_PDUM, "OK\n");

    return eStatus;
}

PUBLIC PDUM_teStatus PDUM_eNPduWriteFromBuffer(PDUM_thNPdu hNPdu, uint8 *pu8Buffer)
{
    PDUM_teStatus eStatus = PDUM_E_OK;

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Writing data from a buffer into an NPDU ... ");

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (pu8Buffer)
    {
        if (hNPdu)
        {
            if (PDUM_NPDU_ASCENDING(hNPdu))
            {
                pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;

                /* copy data from buffer into NPDU */
#if 1
                memcpy(psNPdu->au8PayloadStorage, pu8Buffer, psNPdu->u8Footer);
#else
                memcpy(psNPdu->au8PayloadStorage, pu8Buffer, psNPdu->u8Total);
#endif
            }
            else
            {
                DBG_vPrintf(TRACE_PDUM, "ERROR BAD DIRECTION\n");
                eStatus = PDUM_E_BAD_DIRECTION;
            }
        }
        else
        {
            eStatus = PDUM_E_INVALID_HANDLE; /* PR #16 - Return correct status for invalid NPDU handle */
        }
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, "ERROR BAD PARAMETER\n");
        eStatus = PDUM_E_BAD_PARAM;
    }

    DBG_vPrintf(TRACE_PDUM, "OK\n");

    return eStatus;
}

PUBLIC PDUM_teStatus PDUM_eNPduWriteDataToBuffer(PDUM_thNPdu hNPdu, uint8 *pu8Buffer)
{
    PDUM_teStatus eStatus = PDUM_E_OK;

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Writing data section of NPDU out to a buffer ... ");

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (pu8Buffer)
    {
        if (hNPdu)
        {
            pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;

            if (PDUM_NPDU_ASCENDING(hNPdu))
            {
                size_t sz = psNPdu->u8ClaimedFtr - psNPdu->u8Data; /* this could overflow */
                if (sz < PDUM_NPDU_SIZE && (sz + psNPdu->u8Data) < PDUM_NPDU_SIZE)
                {
                    memcpy(pu8Buffer, &psNPdu->au8PayloadStorage[psNPdu->u8Data],
                           psNPdu->u8ClaimedFtr - psNPdu->u8Data);
                }
                else
                {
                    eStatus = PDUM_E_BAD_PARAM;
                }
            }
            else
            {
                if (psNPdu->u8Header < PDUM_NPDU_SIZE)
                {
                    /* copy out header that has become data due to descension of NPDU */
                    uint8 u8Size = PDUM_NPDU_SIZE - psNPdu->u8Header;
                    memcpy(pu8Buffer, &psNPdu->au8PayloadStorage[psNPdu->u8Header], u8Size);
                    pu8Buffer += u8Size;
                }
                memcpy(pu8Buffer, psNPdu->au8PayloadStorage, psNPdu->u8Footer);
            }
        }
        else
        {
            eStatus = PDUM_E_INVALID_HANDLE; /* PR #16 - Return correct status for invalid NPDU handle */
        }
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, "ERROR BAD PARAMETER\n");
        eStatus = PDUM_E_BAD_PARAM;
    }

    DBG_vPrintf(TRACE_PDUM, "OK\n");

    return eStatus;
}

PUBLIC PDUM_teStatus PDUM_eNPduWriteDataFromBuffer(PDUM_thNPdu hNPdu, uint8 *pu8Buffer)
{
    PDUM_teStatus eStatus = PDUM_E_OK;

#ifdef PDUM_PEDANTIC_CHECKS
    vCheckNPduPoolValid();
#endif

    DBG_vPrintf(TRACE_PDUM, "Writing payload of an NPDU from a buffer ... ");

#ifdef PDUM_PEDANTIC_CHECKS
    DBG_vAssert(TRACE_ASSERTS, !bNPduInFreeList(hNPdu));
#endif

    if (pu8Buffer)
    {
        if (hNPdu)
        {
            pdum_tsNPdu *psNPdu = (pdum_tsNPdu *)hNPdu;

            if (PDUM_NPDU_ASCENDING(hNPdu))
            {
                size_t sz = psNPdu->u8ClaimedFtr - psNPdu->u8Data; /* this could overflow */
                if (sz < PDUM_NPDU_SIZE && (sz + psNPdu->u8Data) < PDUM_NPDU_SIZE)
                {
                    memcpy(&psNPdu->au8PayloadStorage[psNPdu->u8Data], pu8Buffer,
                           psNPdu->u8ClaimedFtr - psNPdu->u8Data);
                }
                else
                {
                    eStatus = PDUM_E_BAD_PARAM;
                }
            }
            else
            {
                if (psNPdu->u8Header < PDUM_NPDU_SIZE)
                {
                    /* copy out header that has become data due to descension of NPDU */
                    uint8 u8Size = PDUM_NPDU_SIZE - psNPdu->u8Header;
                    memcpy(&psNPdu->au8PayloadStorage[psNPdu->u8Header], pu8Buffer, u8Size);
                    pu8Buffer += u8Size;
                }
                memcpy(psNPdu->au8PayloadStorage, pu8Buffer, psNPdu->u8Footer);
            }
        }
        else
        {
            eStatus = PDUM_E_INVALID_HANDLE; /* PR #16 - Return correct status for invalid NPDU handle */
        }
    }
    else
    {
        DBG_vPrintf(TRACE_PDUM, "ERROR BAD PARAMETER\n");
        eStatus = PDUM_E_BAD_PARAM;
    }

    DBG_vPrintf(TRACE_PDUM, "OK\n");

    return eStatus;
}

/****************************************************************************/
/***        Exported Private Functions                                    ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME: pdum_vNPduInitialise
 *
 * DESCRIPTION:
 * Initialise the network PDU manager
 *
 * PARAMETERS:      Name            RW  Usage
 *
 *
 * RETURNS:
 * None.
 *
 * NOTES:
 * None.
 ****************************************************************************/
PUBLIC void pdum_vNPduInit(pdum_tsNPdu *psNPduPool, uint16 u16Size)
{
    pdum_tsNPdu *psNPdu;
    uint16       i;

    // DBG_vPrintf(TRUE, "Initialising NPDU pool size %d\n", u16Size);
    u8MaxNpduCount   = u16Size;
    s_psFreeListHead = psNPduPool;
    /* PR #12 - Added pointers for bounds checking */
    s_psNPduPoolStart = psNPduPool;
    s_psNPduPoolEnd   = &psNPduPool[u16Size];

    /* PR #13 - Rewritten to fix size of 1 problem */
    psNPdu = s_psNPduPoolStart;
    for (i = 0; i < u16Size - 1; i++)
    {
        psNPdu->psNext = psNPdu + 1;
        psNPdu->u8Tag  = PDUM_NPDU_FREETAG;
#ifdef PDUM_PEDANTIC_CHECKS
        psNPdu->au8Magic1[0] = 0xde;
        psNPdu->au8Magic1[1] = 0xad;
        psNPdu->au8Magic1[2] = 0xbe;
        psNPdu->au8Magic1[3] = 0xef;
        psNPdu->au8Magic2[0] = 0xca;
        psNPdu->au8Magic2[1] = 0xfe;
        psNPdu->au8Magic2[2] = 0xba;
        psNPdu->au8Magic2[3] = 0xbe;
#endif
        psNPdu++;
    }
    /* PR #11 - The tail element must point to NULL */
    psNPdu->psNext = NULL;
    psNPdu->u8Tag  = PDUM_NPDU_FREETAG;
#ifdef PDUM_PEDANTIC_CHECKS
    psNPdu->au8Magic1[0] = 0xde;
    psNPdu->au8Magic1[1] = 0xad;
    psNPdu->au8Magic1[2] = 0xbe;
    psNPdu->au8Magic1[3] = 0xef;
    psNPdu->au8Magic2[0] = 0xca;
    psNPdu->au8Magic2[1] = 0xfe;
    psNPdu->au8Magic2[2] = 0xba;
    psNPdu->au8Magic2[3] = 0xbe;
#endif

#if TRACE_NPDU_MAX
    u8Count   = 0;
    u8MaxNpdu = 0;
#endif

    DBG_vPrintf(TRACE_PDUM, "Done.\n", u16Size);
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

#ifdef PDUM_PEDANTIC_CHECKS
PRIVATE bool bNPduInFreeList(PDUM_thNPdu hNPdu)
{
    pdum_tsNPdu *psNPduIter = s_psFreeListHead;

    while (NULL != psNPduIter)
    {
        if (hNPdu == psNPduIter)
        {
            return TRUE;
        }
        psNPduIter = psNPduIter->psNext;
    }

    return FALSE;
}

PUBLIC void vCheckNPduPoolValid(void)
{
    pdum_tsNPdu *psNPdu;

    DBG_vPrintf(TRUE, "Checking NPDU Pool\n");
    for (psNPdu = s_psNPduPoolStart; psNPdu < s_psNPduPoolEnd; psNPdu++)
    {
        //        DBG_vPrintf(TRUE, "idx %d, psNPdu->psNext = %08x\n", psNPdu - s_psNPduPoolStart, psNPdu->psNext);
        DBG_vAssert(TRACE_ASSERTS, psNPdu->au8Magic1[0] == 0xde && psNPdu->au8Magic1[1] == 0xad &&
                                       psNPdu->au8Magic1[2] == 0xbe && psNPdu->au8Magic1[3] == 0xef);
        DBG_vAssert(TRACE_ASSERTS, psNPdu->au8Magic2[0] == 0xca && psNPdu->au8Magic2[1] == 0xfe &&
                                       psNPdu->au8Magic2[2] == 0xba && psNPdu->au8Magic2[3] == 0xbe);

        if (!(psNPdu->psNext == NULL || (psNPdu->psNext >= s_psNPduPoolStart && psNPdu->psNext < s_psNPduPoolEnd)))
        {
            // dump the stack
            volatile uint32 *pu32Stack;
            asm volatile("l.or %0,r0,r1" : "=r"(pu32Stack) :);

            DBG_vPrintf(TRUE, "Stack back trace:\n");
            /* loop until we hit a 32k boundary. should be top of stack */
            while ((uint32)pu32Stack & 0x7fff)
            {
                DBG_vPrintf(TRUE, "% 8x : %08x\n", pu32Stack, *pu32Stack);
                pu32Stack++;
            }
        }

        DBG_vAssert(TRACE_ASSERTS, psNPdu->psNext == NULL ||
                                       (psNPdu->psNext >= s_psNPduPoolStart && psNPdu->psNext < s_psNPduPoolEnd));
    }
}
#endif

#if TRACE_NPDU_MAX
PUBLIC uint8 PDUM_u8GetMaxNpduUse(void)
{
    return u8MaxNpdu;
}
PUBLIC uint8 PDUM_u8GetNpduUse(void)
{
    return u8Count;
}
PUBLIC uint8 PDUM_u8GetNpduPool(void)
{
    return u8MaxNpduCount;
}
#endif

PUBLIC void ZPS_vNwkRegisterNpduNotification(void *pvFnCallback)
{
    pfCallbackNpduNotification = (pfCallbackNotificationType)pvFnCallback;
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
