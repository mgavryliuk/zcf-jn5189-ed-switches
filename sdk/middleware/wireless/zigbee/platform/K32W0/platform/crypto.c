/*
 * Copyright 2023-2024 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "SecLib.h"
#include "RNG_Interface.h"
#include "zb_platform.h"
#include "fsl_aes.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpacked"
#pragma GCC diagnostic ignored "-Wattributes"
#ifdef CRYPTO_ECDH_P256
#include "SecLib_ecp256.h"
#elif defined (CRYPTO_ECDH_CURVE25519)
#include "mbedtls/ecdh.h"
#endif
#pragma GCC diagnostic pop


void zbPlatCryptoInit(void)
{
    SecLib_Init();
}

void zbPlatCryptoDeInit(void)
{
   /* SecLib_DeInit() not available on W0 */
}


uint8_t zbPlatCryptoRandomInit(void)
{
    return RNG_Init();
}

uint32_t zbPlatCryptoRandomGet(uint32_t u32Min, uint32_t u32Max)
{
    return RND_u32GetRand(u32Min, u32Max);
}

uint32_t zbPlatCryptoRandom256Get(void)
{
    return RND_u32GetRand256();
}

int16_t zbPlatRngGetPseudoRandom(uint8_t* pOut, uint8_t outBytes, uint8_t* pSeed)
{
    return RNG_GetPseudoRandomNo(pOut, outBytes, pSeed);
}

void zbPlatCryptoAesHmacMmo(uint8_t *pu8Data, int iDataLen, void *key, void *hash)
{
    return AESSW_vHMAC_MMO(pu8Data, iDataLen, (AESSW_Block_u *)key, (AESSW_Block_u *)hash);
}

void zbPlatCryptoAesMmoBlockUpdate(void *hash, void *block)
{
    return AESSW_vMMOBlockUpdate((AESSW_Block_u *)hash, (AESSW_Block_u *)block);
}

void zbPlatCryptoAesMmoFinalUpdate(void *hash, uint8_t *pu8Data, int iDataLen, int iFinalLen)
{
    return AESSW_vMMOFinalUpdate((AESSW_Block_u *)hash, pu8Data, iDataLen, iFinalLen);
}

bool_t zbPlatCryptoAesSetKey(CRYPTO_tsReg128 *psKeyData)
{
    return bACI_WriteKey((tsReg128 *)psKeyData);
}

void zbPlatCryptoAes128EcbEncrypt(const uint8_t* pu8Input, uint32_t u32InputLen,
		const uint8_t* pu8Key, uint8_t* pu8Output)
{
    AES_SetKey(AES0, pu8Key, CRYPTO_AES_BLK_SIZE);
    AES_EncryptEcb(AES0, pu8Input, pu8Output, CRYPTO_AES_BLK_SIZE);
}

void zbPlatCryptoAes128EcbDecrypt(const uint8_t* pu8Input, const uint8_t* pu8Key, uint8_t* pu8Output)
{
    AES_SetKey(AES0, pu8Key, CRYPTO_AES_BLK_SIZE);
    AES_DecryptEcb(AES0, pu8Input, pu8Output, CRYPTO_AES_BLK_SIZE);
}

void zbPlatCryptoAesCcmStar(bool_t bEncrypt, uint8_t u8M, uint8_t  u8AuthLen,
		uint8_t u8InputLen, CRYPTO_tsAesBlock *puNonce, uint8_t *pu8AuthData,
		uint8_t *pu8Input, uint8_t *pu8ChecksumData, bool_t *pbChecksumVerify)
{
    return vACI_OptimisedCcmStar(bEncrypt, u8M, u8AuthLen, u8InputLen, (tuAES_Block *)puNonce,
            pu8AuthData, pu8Input, pu8ChecksumData, pbChecksumVerify);
}

bool_t zbPlatCryptoEcdhGenerateKeys(CRYPTO_ecdhPublicKey_t *psPublicKey,
                                    CRYPTO_ecdhPrivateKey_t *psSecretKey,
                                    const uint8_t* pu8BasePointG)
{
#ifdef CRYPTO_ECDH_P256
    if (gSecSuccess_c == ECDH_P256_GenerateKeys((ecdhPublicKey_t *)psPublicKey,
        (ecdhPrivateKey_t *)psSecretKey))
    {
        return TRUE;
    }
#elif defined(CRYPTO_ECDH_CURVE25519)
#endif
    return FALSE;
}

bool_t zbPlatCryptoEcdhComputeDhKey(CRYPTO_ecdhPrivateKey_t *psSecretKey,
                                    CRYPTO_ecdhPublicKey_t *psPeerPublicKey,
                                    CRYPTO_ecdhDhKey_t *psOutEcdhKey,
                                    const uint8_t* pu8BasePointG)
{
#ifdef CRYPTO_ECDH_P256
    if (gSecSuccess_c == ECDH_P256_ComputeDhKey((ecdhPrivateKey_t *)psSecretKey,
        (ecdhPublicKey_t *)psPeerPublicKey, (ecdhDhKey_t *)psOutEcdhKey))
    {
        return TRUE;
    }
#elif defined(CRYPTO_ECDH_CURVE25519)
#endif
    return FALSE;
}

fpZbRngPrng_t zbPlatRngGetPrngFunc(void)
{
    return (fpZbRngPrng_t)RNG_GetPrngFunc();
}

void* zbPlatRngGetPrngContext(void)
{
    return RNG_GetPrngContext();
}
