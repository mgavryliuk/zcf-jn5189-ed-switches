#include "app_radiocb.h"

#include "PDM.h"

PUBLIC bool_t bRadioCB_WriteNVM(uint8* pu8DataBlock, uint16 u16DataLength) {
    bool_t bReturn = FALSE;
    PDM_teStatus eStatus;
    eStatus = PDM_eSaveRecordData(PDM_ID_RADIO_SETTINGS, (void*)pu8DataBlock, u16DataLength);

    if (PDM_E_STATUS_OK == eStatus) {
        bReturn = TRUE;
    }
    RADIO_CB_DBG("bRadioCB_WriteNVM(%d) = %d, eStatus = %d\n", u16DataLength, bReturn, eStatus);
    return bReturn;
}

PUBLIC uint16 u16RadioCB_ReadNVM(uint8* pu8DataBlock, uint16 u16DataLength) {
    PDM_teStatus eStatus;
    uint16 u16BytesRead = 0;
    eStatus = PDM_eReadDataFromRecord(PDM_ID_RADIO_SETTINGS, (void*)pu8DataBlock, u16DataLength, &u16BytesRead);
    RADIO_CB_DBG("u16RadioCB_ReadNVM(%d) = %d, eStatus = %d\n", u16DataLength, u16BytesRead, eStatus);
    return u16BytesRead;
}
