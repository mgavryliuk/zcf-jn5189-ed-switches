/*
 * Copyright 2023 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <string.h>

#include "OtaSupport.h"
#include "dbg.h"

/* Filename for OTA client image */
char *ota_filename  = "";
char *ota_filestart = "OTA_Image";

uint32_t _flash_start;
uint32_t _FlsLinkKey;
uint32_t FlsZcCert;
uint32_t _FlsOtaHeader;

uint8_t endsWithExtension(const char *filename, const char *extension)
{
    uint32_t fileLength = strlen(filename);
    uint32_t extLength  = strlen(extension);

    if (fileLength >= extLength)
    {
        const char *substring = filename + (fileLength - extLength);
        if (strcmp(substring, extension) == 0)
        {
            return 1;
        }
    }
    return 0;
}

otaResult_t OTA_ServiceInit(void *posted_ops_storage, size_t posted_ops_sz)
{
    DIR *dir = NULL;
    char cwd[1024];
    struct dirent *entry = NULL;

    if (getcwd(cwd, sizeof(cwd)) == NULL)
    {
        return gOtaNoImage_c;
    }

    if ((dir = opendir(cwd)) == NULL)
    {
        return gOtaNoImage_c;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (endsWithExtension(entry->d_name, ".bin"))
        {
            if (strncmp(entry->d_name, ota_filestart, sizeof(ota_filestart)) == 0)
            {
                ota_filename = entry->d_name;
                return gOtaSuccess_c;
            }
        }
    }
    return gOtaNoImage_c;
}

otaResult_t OTA_PushImageChunk(uint8_t *pData, uint16_t length, uint32_t *pImageLength, uint32_t *pImageOffset)
{
    return gOtaSuccess_c;
}

otaResult_t OTA_PullImageChunk(uint8_t *pData, uint16_t length, uint32_t *pImageOffset)
{
    FILE *file = NULL;
    uint8_t buf[PROGRAM_PAGE_SZ];
    uint16 count = 0;
    uint32 i     = 0;

    /* Validate parameters */
    if ((length == 0U) || (pData == NULL) || (pImageOffset == NULL))
    {
        return gOtaInvalidParam_c;
    }

    if (strlen(ota_filename) == 0)
    {
        return gOtaNoImage_c;
    }

    /* Open Client image file for read */
    file = fopen(ota_filename, "rb");
    if (file == NULL)
    {
        return gOtaImageInvalid_c;
    }

    if (fseek(file, *pImageOffset, SEEK_SET) != 0)
    {
        DBG_vPrintf(TRUE, "OTA: Fail to position at offset %d\n", *pImageOffset);
        fclose(file);
        return gOtaError_c;
    }
    if (fread(&buf, 1, length, file) != length)
    {
        DBG_vPrintf(TRUE, "OTA: Fail to read %d bytes\n", length);
        fclose(file);
        return gOtaError_c;
    }
    memcpy(pData, (void *)&buf, length);
    fclose(file);
    return gOtaSuccess_c;
}

otaResult_t OTA_CommitImage(uint8_t *pBitmap)
{
    return gOtaSuccess_c;
}

void OTA_SetNewImageFlag(void)
{
    return;
}

void OTA_CancelImage(void)
{
    return;
}
