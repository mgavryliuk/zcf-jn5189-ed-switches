/*
 * Copyright 2023-2024 NXP
 *
 * SPDX-License-Identifier:    BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dbg.h"
#include "PDM.h"

#ifdef DEBUG_PDM
    #define TRACE_PDM TRUE
#else
    #define TRACE_PDM FALSE
#endif

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/
#ifndef PDM_FILENAME
#define PDM_FILENAME "pdm_data.bin"
#endif

#ifndef PDM_BUFFER_SIZE
#define PDM_BUFFER_SIZE 1024
#endif

/* Maximum size of data that can be stored by a record */
#define PDM_RECORD_DATA_SIZE 128

/* Maximum number of records that can be stored in PDM record file */
#define PDM_RECORD_NUM (PDM_BUFFER_SIZE / PDM_RECORD_DATA_SIZE)

#define zbEXPECT_ACTION(aCondition, aAction, args...) \
    do                                                \
    {                                                 \
        if (!(aCondition))                            \
        {                                             \
            DBG_vPrintf(TRACE_PDM, args);             \
            aAction;                                  \
            goto exit;                                \
        }                                             \
    } while (0)

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/
/* Record structure holds ID, length and offset in file for data */
struct pdm_rcrd_hdr
{
    uint16_t key;
    uint16_t len;
    uint16_t offset;
};

/*Filename for PDM record based file */
static char *pdm_filename = PDM_FILENAME;

/* Number of records available in PDM record file */
static uint32_t rcrd_num = 0;

/* Array of records from PDM record file */
static struct pdm_rcrd_hdr rcrd_array[PDM_RECORD_NUM] = {0};

/* Data offset in file for first PDM record  */
#define PDM_RECORD_DATA_OFFSET (sizeof(uint32_t) + sizeof(struct pdm_rcrd_hdr) * PDM_RECORD_NUM)

static PDM_teStatus pdm_rcrd_init(void)
{
    PDM_teStatus status = PDM_E_STATUS_OK;
    FILE *file          = NULL;
    int ret             = 0;

    /* create the PDM record based file */
    file = fopen(pdm_filename, "wb");
    if (file == NULL)
    {
        DBG_vPrintf(TRACE_PDM, "PDM: FAIL to create PDM record based file\n");
        return PDM_E_STATUS_INTERNAL_ERROR;
    }

    /* initialize number of records from beginning of file */
    rcrd_num = 0;

    ret = fwrite(&rcrd_num, 1, sizeof(rcrd_num), file);
    zbEXPECT_ACTION(ret == sizeof(rcrd_num), status = PDM_E_STATUS_NOT_SAVED, "PDM: FAIL write on line %d\n", __LINE__);

exit:
    fclose(file);
    return status;
}

static PDM_teStatus pdm_rcrd_restore(FILE *file)
{
    PDM_teStatus status = PDM_E_STATUS_OK;
    int ret             = 0;

    /* load number of records and array of records */
    ret = fread(&rcrd_num, 1, sizeof(rcrd_num), file);
    zbEXPECT_ACTION(ret == sizeof(rcrd_num), status = PDM_E_STATUS_INTERNAL_ERROR, "PDM: FAIL read on line %d\n",
                    __LINE__);

    ret = fread(&rcrd_array, rcrd_num, sizeof(struct pdm_rcrd_hdr), file);
    zbEXPECT_ACTION(ret == sizeof(struct pdm_rcrd_hdr), status = PDM_E_STATUS_INTERNAL_ERROR; rcrd_num = 0,
                    "PDM: FAIL read on line %d\n", __LINE__);
exit:
    return status;
}

/* Search for PDM record based on ID: return TRUE if found, together with index
 * of record in record list and length of data stored for that record. Function
 * returns FALSE if record was not found but can return index of first available
 * record: either free position in record list of end of record list */
static bool pdm_rcrd_search(uint16_t id, uint16_t *len, uint16_t *idx)
{
    uint16 free_idx = rcrd_num;
    bool bFound = false, bFree = false;
    uint32 i;

    for (i = 0; i < rcrd_num; i++)
    {
        /* record was found */
        if (rcrd_array[i].key == id)
        {
            if (len)
            {
                *len = rcrd_array[i].len;
            }
            *idx   = (uint16_t)i;
            bFound = true;
            break;
        }
        else
        {
            if (!bFree)
            {
                /* save first available record position */
                if (rcrd_array[i].key == PDM_INVALID_ID)
                {
                    bFree    = true;
                    free_idx = i;
                }
            }
        }
    }

    /* return available record index */
    if (!bFound)
    {
        *idx = free_idx;
    }
    return bFound;
}

static void pdm_rcrd_dump(void)
{
    struct pdm_rcrd_hdr rcrd[PDM_RECORD_NUM];
    uint8 data[PDM_RECORD_DATA_SIZE] = {0};
    uint32 num                       = 0, i;
    FILE *file                       = NULL;
    uint16 count                     = 0;
    int ret                          = 0;

    file = fopen(pdm_filename, "rb");
    if (file == NULL)
    {
        DBG_vPrintf(TRACE_PDM, "PDM: FAIL to open PDM record based file\n");
        return;
    }
    DBG_vPrintf(TRACE_PDM, "\r\n **** PDM Dump **** \r\n");

    /* read number of records and list of records from file */
    ret = fread(&num, 1, sizeof(num), file);
    zbEXPECT_ACTION(ret == sizeof(num), "", "PDM: FAIL read on line %d\n", __LINE__);

    DBG_vPrintf(TRACE_PDM, " PDM FILE RCRD NR = %d\n", num);

    ret = fread(&rcrd, num, sizeof(struct pdm_rcrd_hdr), file);
    zbEXPECT_ACTION(ret == sizeof(struct pdm_rcrd_hdr), "", "PDM: FAIL read on line %d\n", __LINE__);

    for (i = 0; i < num; i++)
    {
        DBG_vPrintf(TRACE_PDM, " PDM FILE RCRD[%d]:: id=0x%04x len=0x%04x off=0x%04x\n", i, rcrd[i].key, rcrd[i].len,
                    rcrd[i].offset);
    }
    for (i = 0; i < num; i++)
    {
        if (rcrd[i].key == PDM_INVALID_ID)
            continue;

        ret = fseek(file, PDM_RECORD_DATA_OFFSET + i * PDM_RECORD_DATA_SIZE, SEEK_SET);
        zbEXPECT_ACTION(ret == 0, "", "PDM: FAIL seek on line %d\n", __LINE__);

        ret = fread(&data, 1, rcrd[i].len, file);
        zbEXPECT_ACTION(ret == rcrd[i].len, "", "PDM: FAIL read on line %d\n", __LINE__);

        DBG_vPrintf(TRACE_PDM, " PDM FILE RCRD[%d]:: DATA\n", i);
        for (int j = 0; j < rcrd[i].len; j++)
        {
            DBG_vPrintf(TRACE_PDM, " 0x%02x ", data[j]);
        }
        DBG_vPrintf(TRACE_PDM, "\n");
    }
    /* dump record information from RAM */
    DBG_vPrintf(TRACE_PDM, " PDM RCRD NR = %d\n", rcrd_num);
    for (i = 0; i < rcrd_num; i++)
    {
        DBG_vPrintf(TRACE_PDM, " PDM RCRD[%d]:: id=0x%04x len=0x%04x off=0x%04x\n", i, rcrd_array[i].key,
                    rcrd_array[i].len, rcrd_array[i].offset);
    }
exit:
    fclose(file);
}

PDM_teStatus PDM_eInitialise(uint16_t segment, uint8_t cnt, PDM_tpfvSystemEventCallback f)
{
    PDM_teStatus ret = PDM_E_STATUS_INVLD_PARAM;
    FILE *file       = NULL;

    (void)segment;
    (void)cnt;
    (void)f;

    /* check if we have available a record based file */
    file = fopen(pdm_filename, "rb");
    if (file == NULL)
    {
        ret = pdm_rcrd_init();
        if (ret != PDM_E_STATUS_OK)
        {
            DBG_vPrintf(TRACE_PDM, "PDM: FAIL to initialize PDM record based file\n");
            return PDM_E_STATUS_INTERNAL_ERROR;
        }
    }
    else
    {
        ret = pdm_rcrd_restore(file);
        if (ret != PDM_E_STATUS_OK)
        {
            DBG_vPrintf(TRACE_PDM, "PDM: FAIL to restore PDM record based file\n");
            ret = PDM_E_STATUS_INTERNAL_ERROR;
        }
        fclose(file);
    }
    return ret;
}

PDM_teStatus PDM_eSaveRecordData(uint16_t id, void *data, uint16_t len)
{
    PDM_teStatus status = PDM_E_STATUS_OK;
    struct pdm_rcrd_hdr rcrd;
    FILE *file = NULL;
    int ret    = 0;
    uint16 idx = 0;

    if (!data || !len || (len >= PDM_RECORD_DATA_SIZE))
        return PDM_E_STATUS_INVLD_PARAM;

    file = fopen(pdm_filename, "r+b");
    if (file == NULL)
    {
        DBG_vPrintf(TRACE_PDM, "PDM: FAIL to open PDM record based file\n");
        return PDM_E_STATUS_INTERNAL_ERROR;
    }

    /* record was not found and there are no more free spots */
    if (!pdm_rcrd_search(id, NULL, &idx) && (idx == PDM_RECORD_NUM))
    {
        DBG_vPrintf(TRACE_PDM, "PDM: FAIL to save record, PDM is full\n");
        return PDM_E_STATUS_PDM_FULL;
    }

    memset(&rcrd, 0, sizeof(struct pdm_rcrd_hdr));
    rcrd.key    = id;
    rcrd.len    = len;
    rcrd.offset = PDM_RECORD_DATA_OFFSET + idx * PDM_RECORD_DATA_SIZE;

    /* write data and if successful write record in file */
    ret = fseek(file, rcrd.offset, SEEK_SET);
    zbEXPECT_ACTION(ret == 0, status = PDM_E_STATUS_NOT_SAVED, "PDM: FAIL seek on line %d\n", __LINE__);

    ret = fwrite(data, 1, len, file);
    zbEXPECT_ACTION(ret == len, status = PDM_E_STATUS_NOT_SAVED, "PDM: FAIL write on line %d\n", __LINE__);

    ret = fseek(file, sizeof(rcrd_num) + idx * sizeof(struct pdm_rcrd_hdr), SEEK_SET);
    zbEXPECT_ACTION(ret == 0, status = PDM_E_STATUS_NOT_SAVED, "PDM: FAIL seek on line %d\n", __LINE__);

    ret = fwrite(&rcrd, 1, sizeof(struct pdm_rcrd_hdr), file);
    zbEXPECT_ACTION(ret == sizeof(struct pdm_rcrd_hdr), status = PDM_E_STATUS_NOT_SAVED,
            "PDM: FAIL write on line %d\n", __LINE__);

    /* update list of pdm records */
    memcpy(&rcrd_array[idx], &rcrd, sizeof(struct pdm_rcrd_hdr));

    /* update number of records from beginning of file */
    if (idx == rcrd_num)
    {
        rcrd_num++;

        ret = fseek(file, 0, SEEK_SET);
        zbEXPECT_ACTION(ret == 0, status = PDM_E_STATUS_NOT_SAVED, "PDM: FAIL seek on line %d\n", __LINE__);

        ret = fwrite(&rcrd_num, 1, sizeof(rcrd_num), file);
        zbEXPECT_ACTION(ret == sizeof(rcrd_num), status = PDM_E_STATUS_NOT_SAVED,
                "PDM: FAIL write on line %d\n", __LINE__);
    }
exit:
    fclose(file);
    return status;
}

PDM_teStatus PDM_eReadDataFromRecord(uint16_t id, void *data, uint16_t len, uint16_t *cnt)
{
    PDM_teStatus status = PDM_E_STATUS_INVLD_PARAM;
    uint8 rcrd_data[PDM_RECORD_DATA_SIZE];
    uint16 idx = 0, tmp_len = 0;
    FILE *file = NULL;
    int ret    = 0;

    if (!data || !len || !cnt)
    {
        return PDM_E_STATUS_INVLD_PARAM;
    }

    file = fopen(pdm_filename, "rb");
    if (file == NULL)
    {
        DBG_vPrintf(TRACE_PDM, "PDM: FAIL to open PDM record based file\n");
        return PDM_E_STATUS_INTERNAL_ERROR;
    }

    if (pdm_rcrd_search(id, &tmp_len, &idx))
    {
        ret = PDM_E_STATUS_OK;

        /* if requested length is smaller than length stored in record */
        if (tmp_len > len)
        {
            tmp_len = len;
        }

        ret = fseek(file, rcrd_array[idx].offset, SEEK_SET);
        zbEXPECT_ACTION(ret == 0, status = PDM_E_STATUS_NOT_SAVED, "PDM: FAIL seek on line %d\n", __LINE__);

        ret = fread(&rcrd_data, 1, tmp_len, file);
        zbEXPECT_ACTION(ret == tmp_len, status = PDM_E_STATUS_NOT_SAVED, "PDM: FAIL read on line %d\n", __LINE__);

        memcpy(data, (void *)&rcrd_data, tmp_len);

        /* set length */
        *cnt = tmp_len;
    }
exit:
    fclose(file);
    return status;
}

void PDM_vDeleteDataRecord(uint16_t id)
{
    FILE *file = NULL;
    uint16 idx = 0;
    int ret    = 0;

    file = fopen(pdm_filename, "r+b");
    if (file == NULL)
    {
        DBG_vPrintf(TRACE_PDM, "PDM: FAIL to open PDM record based file\n");
        return;
    }

    if (pdm_rcrd_search(id, NULL, &idx))
    {
        /* invalidate record both in RAM and in file */
        struct pdm_rcrd_hdr tmp_rcrd;

        tmp_rcrd.key    = PDM_INVALID_ID;
        tmp_rcrd.len    = 0;
        tmp_rcrd.offset = 0;

        ret = fseek(file, sizeof(rcrd_num) + idx * sizeof(struct pdm_rcrd_hdr), SEEK_SET);
        zbEXPECT_ACTION(ret == 0, "", "PDM: FAIL seek on line %d\n", __LINE__);

        ret = fwrite(&tmp_rcrd, 1, sizeof(struct pdm_rcrd_hdr), file);
        zbEXPECT_ACTION(ret == sizeof(struct pdm_rcrd_hdr), "", "PDM: FAIL write on line %d\n", __LINE__);

        memcpy(&rcrd_array[idx], &tmp_rcrd, sizeof(struct pdm_rcrd_hdr));

        /* reached last record, decrease number of stored records */
        if (idx == (rcrd_num - 1))
        {
            rcrd_num--;

            ret = fseek(file, 0, SEEK_SET);
            zbEXPECT_ACTION(ret == 0, "", "PDM: FAIL seek on line %d\n", __LINE__);

            ret = fwrite(&rcrd_num, 1, sizeof(rcrd_num), file);
            zbEXPECT_ACTION(ret == sizeof(rcrd_num), "", "PDM: FAIL write on line %d\n", __LINE__);
        }
    }

exit:
    fclose(file);
}

void PDM_vDeleteAllDataRecords()
{
    FILE *file   = NULL;
    uint16 count = 0;
    int ret      = 0;

    file = fopen(pdm_filename, "r+b");
    if (file == NULL)
    {
        DBG_vPrintf(TRACE_PDM, "PDM: FAIL to open PDM record based file\n");
        return;
    }
    ret = fseek(file, 0, SEEK_SET);
    zbEXPECT_ACTION(ret == 0, "", "PDM: FAIL seek on line %d\n", __LINE__);

    rcrd_num = 0;
    ret      = fwrite(&rcrd_num, 1, sizeof(rcrd_num), file);
    zbEXPECT_ACTION(ret == sizeof(rcrd_num), "", "PDM: FAIL write on line %d\n", __LINE__);

exit:
    fclose(file);
    return;
}

bool_t PDM_bDoesDataExist(uint16_t id, uint16_t *len)
{
    bool ret;
    uint16 idx = 0;

    ret = pdm_rcrd_search(id, len, &idx);

    return ret;
}

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
