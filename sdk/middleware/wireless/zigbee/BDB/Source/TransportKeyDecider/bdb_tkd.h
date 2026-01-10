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
# COMPONENT:   bdb_tkd.h
#
# DESCRIPTION: BDB Transport Key Decider API 
#              
#
###############################################################################*/

#ifndef BDB_TKD_INCLUDED
#define BDB_TKD_INCLUDED

#include "bdb_api.h"

#if (BDB_SET_DEFAULT_TC_POLICY == TRUE)

#if defined __cplusplus
extern "C" {
#endif

/****************************************************************************/
/***        Include Files                                                 ***/
/****************************************************************************/

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
typedef enum
{
    E_DECIDER_ALLOW_ALL = 0,
    E_DECIDER_WHITELIST,
    E_DECIDER_BLACKLIST
} BDB_eTrspKeyDeciderPolicy;

typedef uint64 BDB_tsTrspKeyDeciderEntry;

typedef struct
{
    BDB_tsTrspKeyDeciderEntry *psTrspKeyDeciderEntry;
    uint16  u16SizeOfTrspKeyDeciderTable;
    BDB_eTrspKeyDeciderPolicy eTrspKeyDeciderPolicy;
} BDB_tsTrspKeyDeciderTable;

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
PUBLIC BDB_eTrspKeyDeciderPolicy eGetTrspKeyDeciderPolicy(void);
PUBLIC BDB_teStatus vSetTrspKeyDeciderPolicy(BDB_eTrspKeyDeciderPolicy eTrspKeyDeciderPolicy);
PUBLIC uint8 u8GetTrspKeyDeciderTableMaxSize(void);
PUBLIC BDB_teStatus tsGetTrspKeyDeciderEntry(uint8 u8Index, BDB_tsTrspKeyDeciderEntry* psTrspKeyDeciderEntry);
PUBLIC BDB_teStatus vAddTrspKeyDeciderEntry(BDB_tsTrspKeyDeciderEntry s_TrspKeyDeciderEntry);
PUBLIC BDB_teStatus vRemoveTrspKeyDeciderEntry(BDB_tsTrspKeyDeciderEntry s_TrspKeyDeciderEntry);
PUBLIC void vClearTrspKeyDeciderTable(void);
PUBLIC bool_t doSendTransportKey(uint64 u64DeviceAddress);

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

#if defined __cplusplus
}
#endif

#endif /* BDB_SET_DEFAULT_TC_POLICY */
#endif  /* BDB_NF_INCLUDED */

