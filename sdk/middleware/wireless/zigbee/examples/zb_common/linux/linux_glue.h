/*
 * Copyright 2023-2024 NXP
 *
 * SPDX-License-Identifier:    BSD-3-Clause
 */

#ifndef __LINUX_GLUE_H__
#define __LINUX_GLUE_H__


#include <pdum_apl.h>
#include <signal.h>

#include "zps_apl_aib.h"
#include "pdum_gen.h"

#define JENNIC_CHIP_FAMILY_JN518x
#define FSL_OSA_TIME_RANGE          (0xFFFFFFFF)

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/
PUBLIC bool eSL_SearchExtendedPanId(uint64 u64ExtPanId, uint16 u16PanId);

#endif /*  __LINUX_GLUE_H__ */
