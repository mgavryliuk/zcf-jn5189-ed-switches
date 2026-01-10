/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "zb_platform.h"
#include "dbg.h"

#if defined(CRYPTO_USE_MBEDTLS)
/* mbedTLS headers */
#include "mbedtls/aes.h"
#endif

#define AES_128_KEY_BITS     128u
#define AES_128_BLOCK_SIZE   16u

#define Lo8(x) ((uint8_t) ((x) & 0xFF))

#define INITIALISE_NONCE_TOP_BYTE(NONCE); \
            NONCE[14] = 0;
#define UPDATE_NONCE_COUNTER(NONCE,VALUE); \
            NONCE[15] = Lo8(VALUE);

#define FLib_MemCpy(pDst, pSrc, cBytes)     ((void)memcpy(pDst, pSrc, (uint32_t)cBytes))
#define FLib_MemSet(pData, value, cBytes)   ((void)memset(pData, (int)value, cBytes))
#define FLib_MemCmp(pData1, pData2, cBytes) ((memcmp((const uint32_t *)pData1, (const uint32_t *)pData2, cBytes) != 0) ? FALSE : TRUE)

/* Used by ZigBee stack adaptation */
enum
{
    MAX_PACKET    =   127,    // largest packet size
    A_DATA        =   0x40,   // the Adata bit in the flags
    M_SHIFT       =   3,      // how much to shift the 3-bit M field
    L_SHIFT       =   0,      // how much to shift the 3-bit L field
    L_SIZE        =   2       // size of the l(m) length field (in octets)
};
static CRYPTO_tsReg128 sKey;
bool_t g_bLegacyHwBugEmulation;

static void AES_128_ECB_Encrypt(const uint8_t *pInput, uint32_t inputLen, const uint8_t *pKey, uint8_t *pOutput);
static void AES_128_Encrypt(const uint8_t *pInput, const uint8_t *pKey, uint8_t *pOutput);

void zbPlatCryptoInit(void)
{
    /* NOT IMPLEMENTED */
    return;
}

void zbPlatCryptoDeInit(void)
{
    /* NOT IMPLEMENTED */
    return;
}

uint8_t zbPlatCryptoRandomInit(void)
{
    /* NOT IMPLEMENTED */
    return 0;
}

uint32_t zbPlatCryptoRandomGet(uint32_t u32Min, uint32_t u32Max)
{
    FILE *file  = NULL;
    int entropy = 0, n = 0;

    if (u32Min >= u32Max)
    {
        DBG_vPrintf(TRUE, "Invalid input parameters\n");
        return 0;
    }

    file = fopen("/dev/urandom", "rb");
    if (file == NULL)
    {
        DBG_vPrintf(TRUE, "Fail to open /dev/urandom\n");
        return 0;
    }

    if (fread(&entropy, 1, sizeof(entropy), file) != sizeof(entropy))
    {
        DBG_vPrintf(TRUE, "Fail to obtain entropy from /dev/urandom\n");
        fclose(file);
        return 0;
    }

    /* Seed and generate random number */
    srand(entropy);
    n = rand();

    fclose(file);
    return n % (u32Max - u32Min) + u32Min;
}

uint32_t zbPlatCryptoRandom256Get(void)
{
    zbPlatCryptoRandomGet(0, 256);
}

void zbPlatCryptoAesMmoBlockUpdate(void *hash, void *block)
{
    CRYPTO_tsAesBlock *puHash = (CRYPTO_tsAesBlock *)hash;
    CRYPTO_tsAesBlock *puBlock = (CRYPTO_tsAesBlock *)block;
    CRYPTO_tsAesBlock uOut;
    int i;

#ifdef DBG_MMO
    vMainDumpBuf("block", puBlock->au8, CRYPTO_AES_BLK_SIZE);
    vMainDumpBuf("hash before", puHash->au8, CRYPTO_AES_BLK_SIZE);
#endif

    /* Block cipher using Hash as key */
    AES_128_Encrypt((uint8_t*)puBlock, (uint8_t*)puHash, (uint8_t*)&uOut);

    /* Prepare next hash as (result XOR block) */
    for (i = 0; i < CRYPTO_AES_BLK_SIZE/4; i++)
    {
        uOut.au32[i] ^= puBlock->au32[i];
        puHash->au32[i] = uOut.au32[i];
    }

#ifdef DBG_MMO
    vMainDumpBuf("hash after", puHash->au8, CRYPTO_AES_BLK_SIZE);
#endif
}

void zbPlatCryptoAesMmoFinalUpdate(void *hash, uint8_t *pu8Data, int iDataLen, int iFinalLen)
{
    CRYPTO_tsAesBlock *puHash = (CRYPTO_tsAesBlock *)hash;
	uint8_t *pu8Buf;
    int iPad;
    int iCount;
    int iAdjust;
    int iLen = iFinalLen;
    uint32_t u32DataLen;
    CRYPTO_tsAesBlock uBuf;

    /* Do complete blocks */
    while (iLen >= CRYPTO_AES_BLK_SIZE)
    {
        FLib_MemCpy(uBuf.au8, pu8Data, CRYPTO_AES_BLK_SIZE);
        zbPlatCryptoAesMmoBlockUpdate(puHash, &uBuf);
        pu8Data += CRYPTO_AES_BLK_SIZE;
        iLen -= CRYPTO_AES_BLK_SIZE;
    }

    /* At this point, length remaining can be 15 to 0 */
    pu8Buf = uBuf.au8;
    if (iLen > 0)
    {
        FLib_MemCpy(pu8Buf, pu8Data, iLen);
        pu8Buf += iLen;
    }
    *pu8Buf++ = 0x80;

    /*
     * Now we can start to put padding in.
     * New MMO (>= 8192 bytes): Need space for 6 octets for bitlength field (32 bits plus extra 0x0000)
     * Original MMO (< 8192 bytes): Need space for 2 octets for bitlength field (16 bits)
     * Calculate adjustment needed based on bitlength field space requirements
     */
    iAdjust = (iDataLen >= 8192) ? (CRYPTO_AES_BLK_SIZE - 6) : (CRYPTO_AES_BLK_SIZE - 2);
    iPad = iAdjust - 1 - iLen; /* Take off another 1 as 0x80 already gone in */
    if (iPad < 0)
    {
        /* Can't finish off on this block - pad the rest if any, transform and move on */
        iCount = iPad + (CRYPTO_AES_BLK_SIZE - iAdjust);
        while (iCount-- > 0)
        {
            *pu8Buf++ = 0x00;
        }

        zbPlatCryptoAesMmoBlockUpdate(puHash, &uBuf);

        /* Reset padding and buffer pointer for final block */
        pu8Buf = uBuf.au8;
        iPad = iAdjust;
    }

    /* Finish off block with length */
    if (iPad > 0)
    {
        FLib_MemSet(pu8Buf, 0, iPad);
        pu8Buf += iPad;
    }
    u32DataLen = (uint32_t)iDataLen << 3; /* in bits so x8 */
    if (iAdjust == (CRYPTO_AES_BLK_SIZE - 6))
    {
        *pu8Buf++ = (uint8_t)((u32DataLen >> 24) & 0xff);
        *pu8Buf++ = (uint8_t)((u32DataLen >> 16) & 0xff);
    }
    *pu8Buf++ = (uint8_t)((u32DataLen >> 8) & 0xff);
    *pu8Buf++ = (uint8_t)(u32DataLen & 0xff);
    if (iAdjust == (CRYPTO_AES_BLK_SIZE - 6))
    {
        *pu8Buf++ = 0;
        *pu8Buf++ = 0;
    }

    zbPlatCryptoAesMmoBlockUpdate(puHash, &uBuf);
}

void zbPlatCryptoAesHmacMmo(uint8_t *pu8Data, int iDataLen, void *key, void *hash)
{
	CRYPTO_tsAesBlock *puKeyData = (CRYPTO_tsAesBlock *)key;
	CRYPTO_tsAesBlock *puHash = (CRYPTO_tsAesBlock *)hash;
	CRYPTO_tsAesBlock uHash;    /* Temporary hash result */
    CRYPTO_tsAesBlock uKeyIPad; /* Inner padding - key XORd with ipad */
    CRYPTO_tsAesBlock uKeyOPad; /* Outer padding - key XORd with opad */
    int i;

    /*
     * the HMAC_MMO transform looks like:
     *
     * MMO(K XOR opad, MMO(K XOR ipad, data))
     *
     * where K is a 16 octet key
     * ipad is the octet 0x36 repeated 16 times
     * opad is the octet 0x5c repeated 16 times
     * and data is the data being protected
     */

    /* Start out by storing key in pads */
    uKeyIPad = uKeyOPad = *puKeyData;

    /* XOR key with ipad and opad values */
    for (i = 0; i < 4; i++)
    {
        uKeyIPad.au32[i] ^= 0x36363636;
        uKeyOPad.au32[i] ^= 0x5c5c5c5c;
    }

    /* Perform inner MMO */
    FLib_MemSet(puHash->au8, 0, CRYPTO_AES_BLK_SIZE); /* Clear hash to zero */
    zbPlatCryptoAesMmoBlockUpdate(puHash, &uKeyIPad); /* Key ^ iPad */
    zbPlatCryptoAesMmoFinalUpdate(puHash, pu8Data, iDataLen+CRYPTO_AES_BLK_SIZE, iDataLen); /* Data to protect */

    /* Hash passed by reference now contains intermediate result */

    /*
     * Perform outer MMO by "hashing the hash".
     * Need to use temporary hash for result before writing back to hash passed by ref.
     */
    FLib_MemSet(uHash.au8, 0, CRYPTO_AES_BLK_SIZE); /* Clear temp hash to zero */
    zbPlatCryptoAesMmoBlockUpdate(&uHash, &uKeyOPad); /* Key ^ oPad */
    zbPlatCryptoAesMmoFinalUpdate(&uHash, puHash->au8, CRYPTO_AES_BLK_SIZE*2, CRYPTO_AES_BLK_SIZE); /* Result from inner MMO */

    /* Return result of outer MMO */
    *puHash = uHash;
}

bool_t zbPlatCryptoAesSetKey(CRYPTO_tsReg128 *psKeyData)
{
    /* For kw41, LTC_AES_EncryptEcb api directly needs key data instead
    * of JN where key is set first. */
    FLib_MemCpy(&sKey, psKeyData, sizeof(CRYPTO_tsReg128));
    return TRUE;
}

void zbPlatCryptoAes128EcbEncrypt(const uint8_t* pu8Input, uint32_t u32InputLen,
		const uint8_t* pu8Key, uint8_t* pu8Output)
{
    return AES_128_ECB_Encrypt(pu8Input, u32InputLen, pu8Key, pu8Output);
}

void zbPlatCryptoAes128EcbDecrypt(const uint8_t* pu8Input, const uint8_t* pu8Key, uint8_t* pu8Output)
{
    /* NOT IMPLEMENTED */
    return;
}

void zbPlatCryptoAesCcmStar(bool_t bEncrypt, uint8_t u8M, uint8_t  u8AuthLen,
		uint8_t u8InputLen, CRYPTO_tsAesBlock *puNonce, uint8_t *pu8AuthData,
		uint8_t *pu8Input, uint8_t *pu8ChecksumData, bool_t *pbChecksumVerify)
{
    uint32_t u32BlockCnt;
    int i,j;
    CRYPTO_tsAesBlock uOut;
    CRYPTO_tsAesBlock uCalculatedMic;
    CRYPTO_tsAesBlock uRxedMic;
    uint8_t *pu8Out;
    uint8_t *pu8In;

    /* Bounds checking on u8M:
     * 1. To ensure that it will never overflow the MIC storage arrays, for
     *    MISRA C compliance
     * 2. To limit u8M to even values. According to the generic CCM*
     *    definition in IEEE802.15.4-2006, valid values are 0, 4, 6, 8, 10,
     *    12, 14 or 16 but we let 2 through as well. This does mean that the M
     *    field of the Flags field in the Nonce has the same value for a u8M
     *    of 0 or 2, but in practice this form of the Flags field is only used
     *    when u8M is non-0 anyway */
    if (u8M > 16U)
    {
        u8M = 16U;
    }
    else
    {
        /* Round down to even value by zeroing bit 0 */
        u8M &= 0xfeU;
    }

    /**************************************************/
    /* If we are Decrypting - then do this first      */
    /**************************************************/
    if (bEncrypt == FALSE)
    {
        /* Decrypt the data */
        pu8Out = pu8Input;

        /* Add in CTR Flags */
        puNonce->au8[0] =(uint8_t)((L_SIZE - 1) << L_SHIFT);
        INITIALISE_NONCE_TOP_BYTE(puNonce->au8);

        u32BlockCnt = 0;
        for (i = 0; i < u8InputLen; i++)
        {
            j = i & 0xf;
            if (j == 0)
            {
                /* generate new keystream block */
                /* Start block counter at 1 for PDU; A[0] is for Mic */
                u32BlockCnt++;
                UPDATE_NONCE_COUNTER(puNonce->au8, u32BlockCnt);

                /* Encrypt the Counter */
                (void)AES_128_ECB_Encrypt(
                         (uint8_t*)puNonce,
                         CRYPTO_AES_BLK_SIZE,
                         (uint8_t*)&sKey,
                         (uint8_t*)&uOut);
            }

            /* XOR in the keystream and copy into both output buffer and temp buffer */
            *pu8Out ^= uOut.au8[j];
            pu8Out++;
        }

        /* Decrypt the Received MIC */
        if (u8M > 0)
        {
            /* MIC encryption uses block counter = 0 */
            puNonce->au8[14] = 0;
            puNonce->au8[15] = 0;
            (void)AES_128_ECB_Encrypt(
                         (uint8_t*)puNonce,
                         CRYPTO_AES_BLK_SIZE,
                         (uint8_t*)&sKey,
                         (uint8_t*)&uOut);
            /* Store the MIC for Comparison later */
            pu8Out = uRxedMic.au8;
            pu8In = pu8ChecksumData;
            for (i = 0; i < u8M; i++)
            {
                *pu8Out = *pu8In ^ uOut.au8[i];
                pu8Out++;
                pu8In++;
            }
        }
    }
    /********************************************/
    /* If we are calculating the MIC, do it now */
    /********************************************/
    if (u8M > 0) /* Only even values are possible, enforced at the start of
                  * the function, so > 0 also implies >= 2, hence there is no
                  * possibility of underflow of the (u8M - 2) calculation
                  * below */
    {
        /* Add in CBC-MAC Flags */
        puNonce->au8[0] =(uint8_t)(  ((u8AuthLen > 0) ? A_DATA : 0)
                                 + (((u8M - 2) / 2) << M_SHIFT)
                                 + ((L_SIZE - 1) << L_SHIFT)
                                );

        /* Add in l(m). The octet order is clearly specified in CCM* as */
        /* 'most-significant-octet first order' */
        puNonce->au8[14] = 0; /* Was: Lo8(u8InputLen >> 8); */
        puNonce->au8[15] = Lo8(u8InputLen);

        /* Generate the first AES block for CBC-MAC */
        (void)AES_128_ECB_Encrypt(
                         (uint8_t*)puNonce,
                         CRYPTO_AES_BLK_SIZE,
                         (uint8_t*)&sKey,
                         (uint8_t*)&uOut);
        /* Initialise input counter */
        j = 0;

        /* Encode L(a) at the header if necessary */
        if (u8AuthLen > 0)
        {
            /* Removed XOR with top byte of u8AuthLen, as will always be 0.
             * Was: uOut.au8[j++] ^= Lo8(u8AuthLen >> 8);
             * Now: j++; */
            j++;
            uOut.au8[j++] ^= Lo8(u8AuthLen);
        }

        /* Generate further codes for the MIC in place */
        for (i = 0; i < u8AuthLen; i++)
        {
            /* Perform the CBC XOR */
            uOut.au8[j] ^= pu8AuthData[i];
            j++;

            /* Full Block, or hit pad boundary */
            if ((j == CRYPTO_AES_BLK_SIZE) || (i == (u8AuthLen - 1)))
            {
                /* Following is to emulate the hardware bug in the JN5148. The
                   hardware prepended the pair of length bytes in hardware,
                   and although it took in 16 bytes at a time (appended with
                   zeroes as necessary for the last stripe) it only used 14 of
                   them at that time (to complete a 16 byte stripe) and held
                   back 2 bytes for the next stripe. But if the final stripe
                   only consisted of one or both of the held back bytes, there
                   wasn't another stripe coming in from the higher layer and
                   the hardware appended 14 bytes from the previous stripe
                   rather than add zeroes itself. So we emulate this by XORing
                   in those bytes again */
                if (g_bLegacyHwBugEmulation && (j < 3))
                {
                    uint8_t *pu8Data;

                    /* Bug only happens for u8AuthLen of (15 + 16.x) or
                       (16 + 16.x); we aren't bothering with the case where
                       u8AuthLen is 0. In both cases we need to XOR back in
                       the 14 bytes from just before the one or two in this
                       stripe. So if u8AuthLen is 15, there is 1 byte in this
                       stripe and we want bytes 0-13. For u8AuthLen of 16,
                       there are 2 bytes in this stripe and we still want
                       bytes 0-13. Similarly for u8AuthLen of 31 or 32, 47 or
                       48, etc. */
                    pu8Data = &pu8AuthData[(u8AuthLen - 15) & 0xfe];

                    /* XOR data from previous stripe */
                    for (j = 2; j < CRYPTO_AES_BLK_SIZE; j++)
                    {
                        uOut.au8[j] ^= *pu8Data;
                        pu8Data++;
                    }
                }
                /* Encrypt the CBC-MAC block, in place */
                (void)AES_128_ECB_Encrypt(
                         (uint8_t*)&uOut,
                         CRYPTO_AES_BLK_SIZE,
                         (uint8_t*)&sKey,
                         (uint8_t*)&uOut);
                /* the block is now empty */
                j = 0;
            }
        }

        for (i = 0; i < u8InputLen; i++)
        {
            /* Perform the CBC XOR */
            uOut.au8[j] ^= pu8Input[i];
            j++;

            /* Full Block, or hit pad boundary */
            if ((j == CRYPTO_AES_BLK_SIZE) || (i == (u8InputLen - 1)))
            {
                /* Encrypt the CBC-MAC block, in place */
               (void)AES_128_ECB_Encrypt(
                         (uint8_t*)&uOut,
                         CRYPTO_AES_BLK_SIZE,
                         (uint8_t*)&sKey,
                         (uint8_t*)&uOut);
                /* the block is now empty */
                j = 0;
            }
        }

        /* Save the MIC */
        FLib_MemCpy(uCalculatedMic.au8, uOut.au8, u8M);
    }

    /**************************************************/
    /* If we are Encrypting */
    /**************************************************/
    if (bEncrypt)
    {
        /* Encrypt the data */
        pu8Out = pu8Input;

        /* Add in CTR Flags */
        puNonce->au8[0] =(uint8_t)((L_SIZE - 1) << L_SHIFT);
        INITIALISE_NONCE_TOP_BYTE(puNonce->au8);

        u32BlockCnt = 0;
        for (i = 0; i < u8InputLen; i++)
        {
            j = i & 0xf;
            if (j == 0)
            {
                /* generate new keystream block */
                /* Start block counter at 1 for PDU; A[0] is for Mic */
                u32BlockCnt++;
                UPDATE_NONCE_COUNTER(puNonce->au8, u32BlockCnt);

                /* Encrypt the Counter */
                (void)AES_128_ECB_Encrypt(
                         (uint8_t*)puNonce,
                         CRYPTO_AES_BLK_SIZE,
                         (uint8_t*)&sKey,
                         (uint8_t*)&uOut);
            }
            /* XOR in the keystream and copy to output buffer */
            *pu8Out ^= uOut.au8[j];
            pu8Out++;
        }

        /* MIC encryption uses block counter = 0 */
        puNonce->au8[14] = 0;
        puNonce->au8[15] = 0;

        /*---- truncate, encrypt, and append MIC to packet */
        if (u8M > 0)
        {
            /* Encrypt the Counter */
           (void)AES_128_ECB_Encrypt(
                         (uint8_t*)puNonce,
                         CRYPTO_AES_BLK_SIZE,
                         (uint8_t*)&sKey,
                         (uint8_t*)&uOut);
            /* Append the encrypted MIC */
            pu8Out = pu8ChecksumData;
            pu8In = uCalculatedMic.au8;
            for (i = 0; i < u8M; i++)
            {
                *pu8Out = *pu8In ^ uOut.au8[i];
                pu8Out++;
                pu8In++;
            }
        }
    }
    else
    {
        /* Compare the Received MIC with Calculated MIC */
        if (FLib_MemCmp(uCalculatedMic.au8, uRxedMic.au8, u8M))
        {
            *pbChecksumVerify = TRUE;
        }
        else
        {
            *pbChecksumVerify = FALSE;
        }
    }
}

static void AES_128_ECB_Encrypt(const uint8_t *pInput, uint32_t inputLen, const uint8_t *pKey, uint8_t *pOutput)
{
    /* If the input length is not a multiple of AES 128 block size return */
    if ((inputLen == 0U) || ((inputLen % AES_128_BLOCK_SIZE) != 0U))
    {
        return;
    }

    /* Process all data blocks*/
    while (inputLen != 0U)
    {
        AES_128_Encrypt(pInput, pKey, pOutput);
        pInput += AES_128_BLOCK_SIZE;
        pOutput += AES_128_BLOCK_SIZE;
        inputLen -= AES_128_BLOCK_SIZE;
    }
}

#if defined(CRYPTO_USE_MBEDTLS)
static void AES_128_Encrypt(const uint8_t *pInput, const uint8_t *pKey, uint8_t *pOutput)
{
    int                 result;
    mbedtls_aes_context aesCtx;

    mbedtls_aes_init(&aesCtx);

    result = mbedtls_aes_setkey_enc(&aesCtx, pKey, AES_128_KEY_BITS);
    if (result != 0)
    {
        assert(0);
    }
    result = mbedtls_aes_crypt_ecb(&aesCtx, MBEDTLS_AES_ENCRYPT, pInput, pOutput);
    if (result != 0)
    {
        assert(0);
    }

    mbedtls_aes_free(&aesCtx);
}
#else
static void AES_128_Encrypt(const uint8_t *pInput, const uint8_t *pKey, uint8_t *pOutput)
{
    /* NOT IMPLEMENTED */
    return;
}
#endif
