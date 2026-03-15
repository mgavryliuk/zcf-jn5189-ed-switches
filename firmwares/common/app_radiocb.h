#ifndef APP_RADIO_CALIBRATIONS_H
#define APP_RADIO_CALIBRATIONS_H

#include "dbg.h"
#include "jendefs.h"

#ifdef DEBUG_RADIO_CALIBRATIONS
#define TRACE_RADIO_CALIBRATIONS TRUE
#else
#define TRACE_RADIO_CALIBRATIONS FALSE
#endif

#define RADIO_CB_DBG(...) DBG_vPrintf(TRACE_RADIO_CALIBRATIONS, "[Radio Calibrations] " __VA_ARGS__)

/*
This allows the radio driver to store calibration settings in PDM so that it does not
have to re-calculate them on cold starts. Without this, or if PDM has been erased, cold
start will be 55.6ms longer because the radio driver will have to perform a full calibration
*/

bool_t bRadioCB_WriteNVM(uint8* pu8DataBlock, uint16 u16DataLength);
uint16 u16RadioCB_ReadNVM(uint8* pu8DataBlock, uint16 u16DataLength);

#endif /* APP_RADIO_CALIBRATIONS_H */
