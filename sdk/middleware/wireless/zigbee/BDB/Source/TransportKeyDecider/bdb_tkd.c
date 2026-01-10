/****************************************************************************
 *
 * Copyright 2024 NXP
 *
 * NXP Confidential. 
 * 
 * This software is owned or controlled by NXP and may only be used strictly 
 * in accordance with the applicable license terms.  
 * By expressly accepting such terms or by downloading, installing, activating 
 * and/or otherwise using the software, you are agreeing that you have read, 
 * and that you agree to comply with and are bound by, such license terms.  
 * If you do not agree to be bound by the applicable license terms, 
 * then you may not retain, install, activate or otherwise use the software. 
 * 
 *
 ****************************************************************************/


/*###############################################################################
#
# MODULE:      BDB
#
# COMPONENT:   bdb_tkd.c
#
# DESCRIPTION: BDB Trnasport Key Decider implementation
#              
#
###############################################################################*/

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/
#include "jendefs.h"
#include "bdb_api.h"

#if (BDB_SET_DEFAULT_TC_POLICY == TRUE)
#include "dbg.h"
#include "bdb_tkd.h"

PRIVATE uint8 u8GetTrspKeyDeciderIndex(BDB_tsTrspKeyDeciderEntry s_TrspKeyDeciderEntry);
PRIVATE uint8 u8GetTrspKeyDeciderFreeIndex(void);

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#define BDB_TRSP_KEY_DECIDER_TABLE_SIZE       8

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
PRIVATE BDB_tsTrspKeyDeciderEntry s_TrspKeyDeciderTableStorage[BDB_TRSP_KEY_DECIDER_TABLE_SIZE];
PRIVATE BDB_tsTrspKeyDeciderTable s_TrspKeyDeciderTable = { s_TrspKeyDeciderTableStorage, 0, 0 };

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME: doSendTransportKey
 *
 * DESCRIPTION:
 *  Returns true if the transport key is to be sent to the (re)joining device
 *  identified with the IEEE address u64DeviceAddress or false otherwise.
 *
 * PARAMETERS:      Name              RW  Usage
 *                  u64DeviceAddress      IEEE address of (re)joining device
 * RETURNS:
 *  uint8
 *
 ****************************************************************************/
PUBLIC bool_t doSendTransportKey(uint64 u64DeviceAddress)
{
    BDB_tsTrspKeyDeciderEntry s_TrspKeyDeciderEntry = (BDB_tsTrspKeyDeciderEntry)u64DeviceAddress;

    switch(s_TrspKeyDeciderTable.eTrspKeyDeciderPolicy)
    {
        case E_DECIDER_ALLOW_ALL:
            return TRUE;
        case E_DECIDER_WHITELIST:
            return (u8GetTrspKeyDeciderIndex(s_TrspKeyDeciderEntry) != BDB_TRSP_KEY_DECIDER_TABLE_SIZE);
        case E_DECIDER_BLACKLIST:
            return (u8GetTrspKeyDeciderIndex(s_TrspKeyDeciderEntry) == BDB_TRSP_KEY_DECIDER_TABLE_SIZE);
        default:
            return TRUE;
    }
}

/****************************************************************************
 *
 * NAME: eGetTrspKeyDeciderPolicy
 *
 * DESCRIPTION:
 *  Return the policy of the transport key decider table.
 *
 * PARAMETERS:      Name            RW      Usage
 * 
 * RETURNS:
 *  BDB_eTrspKeyDeciderPolicy
 *
 ****************************************************************************/
PUBLIC BDB_eTrspKeyDeciderPolicy eGetTrspKeyDeciderPolicy()
{
    return s_TrspKeyDeciderTable.eTrspKeyDeciderPolicy;
}

/****************************************************************************
 *
 * NAME: vSetTrspKeyDeciderPolicy
 *
 * DESCRIPTION:
 *  Sets the policy of the transport key decider table to the given policy
 *  Policy modes are ignore, whitelist and blacklist. 
 *  Fails if given value is not a valid policy
 *
 * PARAMETERS:      Name            RW      Usage
 * 
 * RETURNS:
 *  BDB_teStatus
 *
 ****************************************************************************/
PUBLIC BDB_teStatus vSetTrspKeyDeciderPolicy(BDB_eTrspKeyDeciderPolicy eTrspKeyDeciderPolicy)
{
    if(eTrspKeyDeciderPolicy > 2)
    {
        /* Invalid policy value*/
        return BDB_E_FAILURE;
    }
    s_TrspKeyDeciderTable.eTrspKeyDeciderPolicy = eTrspKeyDeciderPolicy;
    return BDB_E_SUCCESS;
}

/****************************************************************************
 *
 * NAME: u8GetTrspKeyDeciderTableSize
 *
 * DESCRIPTION:
 *  Returns maximum number of transport key decider table entries. 
 *
 * PARAMETERS:      Name            RW      Usage
 * 
 * RETURNS:
 *  uint8
 *
 ****************************************************************************/
PUBLIC uint8 u8GetTrspKeyDeciderTableMaxSize()
{
    return BDB_TRSP_KEY_DECIDER_TABLE_SIZE;
}

/****************************************************************************
 *
 * NAME: vGetTrspKeyDeciderEntry
 *
 * DESCRIPTION:
 * Gets entry of the transport key decider table for the given index.
 * Fails if given index is out of bounds.
 * PARAMETERS:      Name                    RW      Usage
 *                  s_TrspKeyDeciderEntry           Entry to remove
 * RETURNS:
 *  BDB_teStatus
 *
 ****************************************************************************/
PUBLIC BDB_teStatus tsGetTrspKeyDeciderEntry(uint8 u8Index, BDB_tsTrspKeyDeciderEntry* psTrspKeyDeciderEntry)
{
    if(u8Index < BDB_TRSP_KEY_DECIDER_TABLE_SIZE)
    {
        *psTrspKeyDeciderEntry = s_TrspKeyDeciderTable.psTrspKeyDeciderEntry[u8Index];
        return BDB_E_SUCCESS;
    }
    /* Index out of bounds */
    return BDB_E_FAILURE;
}

/****************************************************************************
 *
 * NAME: vAddTrspKeyDeciderEntry
 *
 * DESCRIPTION:
 *  Adds given entry to transport key decider table if not already present. 
 *  Fails if table full.
 *
 * PARAMETERS:      Name                    RW      Usage
 *                  s_TrspKeyDeciderEntry           Entry to add
 * RETURNS:
 *  BDB_teStatus
 *
 ****************************************************************************/
PUBLIC BDB_teStatus vAddTrspKeyDeciderEntry(BDB_tsTrspKeyDeciderEntry s_TrspKeyDeciderEntry)
{
    if(u8GetTrspKeyDeciderIndex(s_TrspKeyDeciderEntry) != BDB_TRSP_KEY_DECIDER_TABLE_SIZE)
    {
        /* Entry already present, nothing to do */
        return BDB_E_SUCCESS;
    }

    if(s_TrspKeyDeciderTable.u16SizeOfTrspKeyDeciderTable == BDB_TRSP_KEY_DECIDER_TABLE_SIZE)
    {
        /* Table full */
        return BDB_E_FAILURE;
    }

    uint8 u8FreeIndex = u8GetTrspKeyDeciderFreeIndex();
    s_TrspKeyDeciderTable.psTrspKeyDeciderEntry[u8FreeIndex] = s_TrspKeyDeciderEntry;
    s_TrspKeyDeciderTable.u16SizeOfTrspKeyDeciderTable++;
    return BDB_E_SUCCESS;
}

/****************************************************************************
 *
 * NAME: vRemoveTrspKeyDeciderEntry
 *
 * DESCRIPTION:
 *  Removes given entry from transport key decider table if not already missing. 
 *  Fails if table empty.
 *
 * PARAMETERS:      Name                    RW      Usage
 *                  s_TrspKeyDeciderEntry           Entry to remove
 * RETURNS:
 *  BDB_teStatus
 *
 ****************************************************************************/
PUBLIC BDB_teStatus vRemoveTrspKeyDeciderEntry(BDB_tsTrspKeyDeciderEntry s_TrspKeyDeciderEntry)
{
    if(u8GetTrspKeyDeciderIndex(s_TrspKeyDeciderEntry) == BDB_TRSP_KEY_DECIDER_TABLE_SIZE)
    {
        /* Entry already absent, nothing to do */
        return BDB_E_SUCCESS;
    }

    if(s_TrspKeyDeciderTable.u16SizeOfTrspKeyDeciderTable == 0)
    {
        /* Table empty */
        return BDB_E_FAILURE;
    }

    uint8 u8IndexToRemove = u8GetTrspKeyDeciderIndex(s_TrspKeyDeciderEntry);
    s_TrspKeyDeciderTable.psTrspKeyDeciderEntry[u8IndexToRemove] = 0;
    s_TrspKeyDeciderTable.u16SizeOfTrspKeyDeciderTable--;
    return BDB_E_SUCCESS;
}

/****************************************************************************
 *
 * NAME: vClearTrspKeyDeciderTable
 *
 * DESCRIPTION:
 *  Zeroes out the transport key decider table.
 *
 * PARAMETERS:      Name            RW  Usage
 * 
 * RETURNS:
 *  void
 *
 ****************************************************************************/
PUBLIC void vClearTrspKeyDeciderTable()
{
    uint8 u8Index;

    if(s_TrspKeyDeciderTable.u16SizeOfTrspKeyDeciderTable == 0)
    {
        /* Table empty */
        return;
    }
    else
    {   
        for(u8Index = 0; u8Index < BDB_TRSP_KEY_DECIDER_TABLE_SIZE; u8Index++)
        {
           s_TrspKeyDeciderTable.psTrspKeyDeciderEntry[u8Index] = 0;
        }
        s_TrspKeyDeciderTable.u16SizeOfTrspKeyDeciderTable = 0;
    }
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME: u8GetTrspKeyDeciderFreeIndex
 *
 * DESCRIPTION:
 *  Returns the index of the first free entry in the transport key decider table.
 *
 * PARAMETERS:      Name            RW  Usage
 * 
 * RETURNS:
 *  uint8
 *
 ****************************************************************************/
PRIVATE uint8 u8GetTrspKeyDeciderFreeIndex()
{
    uint8 u8FreeIndex = 0;
    while(s_TrspKeyDeciderTable.psTrspKeyDeciderEntry[u8FreeIndex] && u8FreeIndex < BDB_TRSP_KEY_DECIDER_TABLE_SIZE)
    {
        u8FreeIndex++;
    }
    return u8FreeIndex;
}

/****************************************************************************
 *
 * NAME: u8GetTrspKeyDeciderIndex
 *
 * DESCRIPTION:
 *  Returns the index of the entry received as parameter
 *  If entry is not present, returns table size
 *
 * PARAMETERS:      Name                    RW  Usage
 *                  s_TrspKeyDeciderEntry       Searched entry
 * RETURNS:
 *  uint8
 *
 ****************************************************************************/
PRIVATE uint8 u8GetTrspKeyDeciderIndex(BDB_tsTrspKeyDeciderEntry s_TrspKeyDeciderEntry)
{
    uint8 u8Index;
    for(u8Index = 0; u8Index < BDB_TRSP_KEY_DECIDER_TABLE_SIZE; u8Index++)
    {
        if(s_TrspKeyDeciderTable.psTrspKeyDeciderEntry[u8Index] == s_TrspKeyDeciderEntry)
        {
            return u8Index;
        }
    }
    return BDB_TRSP_KEY_DECIDER_TABLE_SIZE;
}

#endif /* BDB_SET_DEFAULT_TC_POLICY */
