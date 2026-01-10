/*
* Copyright 2023 NXP
*
* SPDX-License-Identifier: BSD-3-Clause
*/

#ifndef _PDUM_GEN_H
#define _PDUM_GEN_H

#include <pdum_apl.h>

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
/* APDUs */
#define apduZDP pdum_apduZDP
#define apduZCL pdum_apduZCL

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        External Variables                                            ***/
/****************************************************************************/
/* APDUs */
extern struct pdum_tsAPdu_tag *pdum_apduZDP;
extern struct pdum_tsAPdu_tag *pdum_apduZCL;

#define ZPS_MAX_CHANNEL_LIST_SIZE     (1)

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

PUBLIC void PDUM_vInit(void);
PUBLIC uint16 PDUM_u16APduGetMaxUse(PDUM_thAPdu hAPdu);
PUBLIC uint16 PDUM_u16APduGetCrtUse(PDUM_thAPdu hAPdu);

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/
#endif /* _PDUM_GEN_H */

