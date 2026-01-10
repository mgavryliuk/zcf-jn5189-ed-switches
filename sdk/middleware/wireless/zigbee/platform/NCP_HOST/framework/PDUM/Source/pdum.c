/*****************************************************************************
 *
 * MODULE:             PDUM
 *
 * COMPONENT:          pdum_nwk.c
 *
 * DESCRIPTION:        Manages network protcol data units
 *
 *****************************************************************************
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright Jennic Ltd. 2007 All rights reserved
 *
 ****************************************************************************/

/****************************************************************************/
/***        Include files                                                 ***/
/****************************************************************************/

#include <jendefs.h>
#include <stdlib.h>
#include <string.h>
#include "dbg.h"

/****************************************************************************/
/***        Macro Definitions                                             ***/
/****************************************************************************/

#ifndef TRACE_PDUM
#define TRACE_PDUM FALSE
#endif

#define ALIGN(n, v) (((uint32)(v) + ((n)-1)) & (~((n)-1)))

/****************************************************************************/
/***        Type Definitions                                              ***/
/****************************************************************************/

/****************************************************************************/
/***        Local Function Prototypes                                     ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Variables                                            ***/
/****************************************************************************/

PUBLIC const uint32 PDUM_g_u32Version USED = 347422; /*These versions must be kept instep */

/****************************************************************************/
/***        Local Variables                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        Exported Functions                                            ***/
/****************************************************************************/

/****************************************************************************
 *
 * NAME:       PDUM_u16SizeNBO
 */
/**
 * Return the size of the consumed payload for the supplied format string
 * The supplied format string specifies the size of parameters and any packing:
 *  'b' - 8 bit value (byte)
 *  'h' - 16 bit value (half word)
 *  'w' - 32 bit value (word / long word)
 *  'l' - 64 bit value (long long word)
 *  'a' - series of 8 bit values the length of which is specified in the
 *        following character value. eg "a\x10" is a string of 16 bytes
 *  'p' - packing bytes the length of which is specified in the
 *        following character value. eg "p\x8" is padding of 8 bytes
 *
 * @ingroup PDUM
 *
 * @param ppu8Data      Pointer to pointer to payload data
 * @param szFormat      Format string
 * @param ...           Value arguments
 *
 * @return uint16       Size of data to be written
 *
 * @note
 *
 ****************************************************************************/

PUBLIC uint16 PDUM_u16SizeNBO(const char *szFormat)
{
    uint16 u16Size = 0;

    for (; *szFormat != '\0'; szFormat++)
    {
        if (*szFormat == 'b')
        { /* 8 bit integer value */
            u16Size += sizeof(uint8);
        }
        else if (*szFormat == 'h')
        { /* 16 bit integer value */
            u16Size += sizeof(uint16);
        }
        else if (*szFormat == 'w')
        { /* 32 bit integer value */
            u16Size += sizeof(uint32);
        }
        else if (*szFormat == 'l')
        { /* 64 bit integer value */
            u16Size += sizeof(uint64);
        }
        else if (*szFormat == 'a')
        { /* string of octets with size specified in proceeding char */
            u16Size += *++szFormat;
        }
        else if (*szFormat == 'p')
        { /* padding with size specified in proceeding char */
            u16Size += *++szFormat;
        }
    }

    return u16Size;
}

/****************************************************************************
 *
 * NAME:       pdum_u16WriteNBO
 */
/**
 * Write the supplied list of parameters into payload in network byte order
 * The supplied format string specifies the size of parameters and any packing:
 *  'b' - 8 bit value (byte)
 *  'h' - 16 bit value (half word)
 *  'w' - 32 bit value (word / long word)
 *  'l' - 64 bit value (long long word)
 *  'a' - series of 8 bit values the length of which is specified in the
 *        following character value. eg "a\x10" is a string of 16 bytes
 *  'p' - packing bytes the length of which is specified in the
 *        following character value. eg "p\x8" is padding of 8 bytes
 *
 * @ingroup PDUM
 *
 * @param ppu8Data      Pointer to pointer to payload data
 * @param szFormat      Format string
 * @param ...           Value arguments
 *
 * @return uint16       size of data written into pdu
 *
 * @note
 *
 ****************************************************************************/

PUBLIC uint16 pdum_u16WriteNBO(uint8 *pu8Data, const char *szFormat, va_list *pArgs)
{
    uint8 *pu8Start = pu8Data;

    for (; *szFormat != '\0'; szFormat++)
    {
        if (*szFormat == 'b')
        { /* 8 bit integer value */
            *pu8Data++ = va_arg(*pArgs, unsigned int);
        }
        else if (*szFormat == 'h')
        { /* 16 bit integer value */
            uint16 u16Data = va_arg(*pArgs, unsigned int);
            *pu8Data++     = (uint8)u16Data;
            *pu8Data++     = (uint8)(u16Data >> 8);
        }
        else if (*szFormat == 'w')
        { /* 32 bit integer value */
            uint32 u32Data = va_arg(*pArgs, uint32);
            *pu8Data++     = (uint8)u32Data;
            *pu8Data++     = (uint8)(u32Data >> 8);
            *pu8Data++     = (uint8)(u32Data >> 16);
            *pu8Data++     = (uint8)(u32Data >> 24);
        }
        else if (*szFormat == 'l')
        { /* 64 bit integer value */
            uint64 u64Data = va_arg(*pArgs, uint64);
            *pu8Data++     = (uint8)(u64Data & 0xff);
            *pu8Data++     = (uint8)(u64Data >> 8);
            *pu8Data++     = (uint8)(u64Data >> 16);
            *pu8Data++     = (uint8)(u64Data >> 24);
            *pu8Data++     = (uint8)(u64Data >> 32);
            *pu8Data++     = (uint8)(u64Data >> 40);
            *pu8Data++     = (uint8)(u64Data >> 48);
            *pu8Data++     = (uint8)(u64Data >> 56);
        }
        else if (*szFormat == 'a')
        { /* string of octets with size specified in proceeding char */
            uint8 *      pu8String = va_arg(*pArgs, uint8 *);
            uint8        u8Size;
            unsigned int i;

            szFormat++;
            u8Size = *szFormat;

            DBG_vPrintf(TRACE_PDUM, "PDUM array size = %d\n", u8Size);

            for (i = 0; i < u8Size; i++)
            {
                *pu8Data++ = *pu8String++;
            }
        }
        else if (*szFormat == 'p')
        { /* padding with size specified in proceeding char */
            uint8        u8Size;
            unsigned int i;

            szFormat++;
            u8Size = *szFormat;

            for (i = 0; i < u8Size; i++)
            {
                *pu8Data++ = 0;
            }
        }
    }

    return (uint16)(pu8Data - pu8Start);
}

/****************************************************************************
 *
 * NAME:       pdum_u16WriteStrNBO
 */
/**
 * Write the supplied list of parameters into payload in network byte order
 * The supplied format string specifies the size of parameters and any packing:
 *  'b' - 8 bit value (byte)
 *  'h' - 16 bit value (half word)
 *  'w' - 32 bit value (word / long word)
 *  'l' - 64 bit value (long long word)
 *  'a' - series of 8 bit values the length of which is specified in the
 *        following character value. eg "a\x10" is a string of 16 bytes
 *  'p' - packing bytes the length of which is specified in the
 *        following character value. eg "p\x8" is padding of 8 bytes
 *
 * @ingroup PDUM
 *
 * @param ppu8Data      Pointer to pointer to payload data
 * @param szFormat      Format string
 * @param pvStruct      Pointer to struct containing values in order of fmt string
 *
 * @return uint16       size of data written into pdu
 *
 * @note
 *
 ****************************************************************************/

PUBLIC uint16 pdum_u16WriteStrNBO(uint8 *pu8Data, const char *szFormat, void *pvStruct)
{
    uint8 *pu8Start  = pu8Data;
    uint32 u32Offset = 0;

    for (; *szFormat != '\0'; szFormat++)
    {
        if (*szFormat == 'b')
        { /* 8 bit integer value */
            *pu8Data++ = ((uint8 *)pvStruct)[u32Offset++];
        }
        else if (*szFormat == 'h')
        { /* 16 bit integer value */
            uint16 u16Data;

            u32Offset = ALIGN(sizeof(uint16), u32Offset);

            memcpy(&u16Data, (uint8 *)pvStruct + u32Offset, sizeof(uint16));
            *pu8Data++ = u16Data;
            *pu8Data++ = u16Data >> 8;

            u32Offset += sizeof(uint16);
        }
        else if (*szFormat == 'w')
        { /* 32 bit integer value */
            uint32 u32Data;

            u32Offset = ALIGN(sizeof(uint32), u32Offset);

            memcpy(&u32Data, (uint8 *)pvStruct + u32Offset, sizeof(uint32));
            *pu8Data++ = (uint8)u32Data;
            *pu8Data++ = (uint8)(u32Data >> 8);
            *pu8Data++ = (uint8)(u32Data >> 16);
            *pu8Data++ = (uint8)(u32Data >> 24);

            u32Offset += sizeof(uint32);
        }
        else if (*szFormat == 'l')
        { /* 64 bit integer value */
            uint64 u64Data;

            u32Offset = ALIGN(sizeof(uint64), u32Offset);

            memcpy(&u64Data, (uint8 *)pvStruct + u32Offset, sizeof(uint64));
            *pu8Data++ = (uint8)(u64Data & 0xff);
            *pu8Data++ = (uint8)(u64Data >> 8);
            *pu8Data++ = (uint8)(u64Data >> 16);
            *pu8Data++ = (uint8)(u64Data >> 24);
            *pu8Data++ = (uint8)(u64Data >> 32);
            *pu8Data++ = (uint8)(u64Data >> 40);
            *pu8Data++ = (uint8)(u64Data >> 48);
            *pu8Data++ = (uint8)(u64Data >> 56);

            u32Offset += sizeof(uint64);
        }
        else if (*szFormat == 'a')
        { /* string of octets with size specified in proceeding char */
            uint8        u8Size = *++szFormat;
            unsigned int i;

            for (i = 0; i < u8Size; i++)
            {
                *pu8Data++ = ((uint8 *)pvStruct)[u32Offset++];
            }
        }
        else if (*szFormat == 'p')
        { /* padding with size specified in proceeding char */
            uint8        u8Size = *++szFormat;
            unsigned int i;

            for (i = 0; i < u8Size; i++)
            {
                *pu8Data++ = 0;
            }
        }
    }

    return (uint16)(pu8Data - pu8Start);
}

/****************************************************************************
 *
 * NAME:       pdum_u16ReadNWBO
 */
/**
 * Read the supplied list of parameters into a structure in host byte order
 * from payload in network byte order.
 * The supplied format string specifies the size of parameters and any packing:
 *  'b' - 8 bit value (byte)
 *  'h' - 16 bit value (half word)
 *  'w' - 32 bit value (word / long word)
 *  'l' - 64 bit value (long long word)
 *  'a' - series of 8 bit values the length of which is specified in the
 *        following character value. eg "a\x10" is a string of 16 bytes
 *  'p' - packing bytes the length of which is specified in the
 *        following character value. eg "p\x8" is padding of 8 bytes
 *
 * @ingroup PDUM
 *
 * @param pvStruct      Pointer to structure which will contain the data
 * @param szFormat      Format string
 * @param pu8Data       Payload data
 *
 * @return uint16       Size of data read from pdu
 *
 * @note
 *
 ****************************************************************************/

PUBLIC uint16 pdum_u16ReadNBO(uint8 *pu8Struct, const char *szFormat, uint8 *pu8Data)
{
    uint8 *pu8Start  = pu8Data;
    uint32 u32Offset = 0;

    if (!pu8Struct || !szFormat || !pu8Data)
    {
        return 0;
    }

    for (; *szFormat != '\0'; szFormat++)
    {
        if (*szFormat == 'b')
        {
            pu8Struct[u32Offset++] = *pu8Data++;
        }
        else if (*szFormat == 'h')
        {
            uint16 u16Val = *pu8Data++;
            u16Val |= (*pu8Data++) << 8;

            /* align to half-word boundary */
            u32Offset = ALIGN(sizeof(uint16), u32Offset);

            memcpy(pu8Struct + u32Offset, &u16Val, sizeof(uint16));

            u32Offset += sizeof(uint16);
        }
        else if (*szFormat == 'w')
        {
            uint32 u32Val = *pu8Data++;
            u32Val |= (*pu8Data++) << 8;
            u32Val |= (*pu8Data++) << 16;
            u32Val |= (*pu8Data++) << 24;

            /* align to word (32 bit) boundary */
            u32Offset = ALIGN(sizeof(uint32), u32Offset);

            memcpy(pu8Struct + u32Offset, &u32Val, sizeof(uint32));

            u32Offset += sizeof(uint32);
        }
        else if (*szFormat == 'l')
        {
            uint64 u64Val;
            u64Val = *pu8Data++;
            u64Val |= (uint64)(*pu8Data++) << 8;
            u64Val |= (uint64)(*pu8Data++) << 16;
            u64Val |= (uint64)(*pu8Data++) << 24;
            u64Val |= (uint64)(*pu8Data++) << 32;
            u64Val |= (uint64)(*pu8Data++) << 40;
            u64Val |= (uint64)(*pu8Data++) << 48;
            u64Val |= (uint64)(*pu8Data++) << 56;

            /*
             *  align to long long word (64 bit) boundary
             *  but relative to structure start
             */
            u32Offset = ALIGN(sizeof(uint64), u32Offset);

            memcpy(pu8Struct + u32Offset, &u64Val, sizeof(uint64));

            u32Offset += sizeof(uint64);
        }
        else if (*szFormat == 'a')
        {
            uint8        u8Size = *++szFormat;
            unsigned int i;

            for (i = 0; i < u8Size; i++)
            {
                *(pu8Struct + u32Offset) = *pu8Data++;
                u32Offset++;
            }
        }
        else if (*szFormat == 'p')
        {
            pu8Data += *++szFormat;
        }
    }

    return (uint16)(pu8Data - pu8Start);
}

/****************************************************************************/
/***        Local Functions                                               ***/
/****************************************************************************/

/****************************************************************************/
/***        END OF FILE                                                   ***/
/****************************************************************************/
