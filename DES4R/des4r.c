//
//  des4r.c
//  Proj01_DES4r
//  Created by Xiaowen Zhang on 9/30/18.
/*
 *  FIPS-46-3 compliant Triple-DES implementation
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */
/*
 *  DES, on which TDES is based, was originally designed by Horst Feistel
 *  at IBM in 1974, and was adopted as a standard by NIST (formerly NBS).
 *
 *  http://csrc.nist.gov/publications/fips/fips46-3/fips46-3.pdf
 */


#include "des4r.h"

#include <string.h>
#include <stdio.h>

// The following four lines are copied from config.h, otherwise we would have to include it
#define mbedtls_fprintf fprintf

static void * (* const volatile memset_func)( void *, int, size_t ) = memset;

void mbedtls_platform_zeroize( void *buf, size_t len )
{
    memset_func( buf, 0, len );
}

/*
 * 32-bit integer manipulation macros (big endian)
 */
#ifndef GET_UINT32_BE
#define GET_UINT32_BE(n,b,i)                    \
{                                               \
(n) = ( (uint32_t) (b)[(i)    ] << 24 )         \
| ( (uint32_t) (b)[(i) + 1] << 16 )             \
| ( (uint32_t) (b)[(i) + 2] <<  8 )             \
| ( (uint32_t) (b)[(i) + 3]       );            \
}
#endif

#ifndef PUT_UINT32_BE
#define PUT_UINT32_BE(n,b,i)                        \
{                                                   \
(b)[(i)    ] = (unsigned char) ( (n) >> 24 );       \
(b)[(i) + 1] = (unsigned char) ( (n) >> 16 );       \
(b)[(i) + 2] = (unsigned char) ( (n) >>  8 );       \
(b)[(i) + 3] = (unsigned char) ( (n)       );       \
}
#endif

/*
 * Expanded DES S-boxes
 */
static const uint32_t SB1[64] =
{
    0x01010400, 0x00000000, 0x00010000, 0x01010404,
    0x01010004, 0x00010404, 0x00000004, 0x00010000,
    0x00000400, 0x01010400, 0x01010404, 0x00000400,
    0x01000404, 0x01010004, 0x01000000, 0x00000004,
    0x00000404, 0x01000400, 0x01000400, 0x00010400,
    0x00010400, 0x01010000, 0x01010000, 0x01000404,
    0x00010004, 0x01000004, 0x01000004, 0x00010004,
    0x00000000, 0x00000404, 0x00010404, 0x01000000,
    0x00010000, 0x01010404, 0x00000004, 0x01010000,
    0x01010400, 0x01000000, 0x01000000, 0x00000400,
    0x01010004, 0x00010000, 0x00010400, 0x01000004,
    0x00000400, 0x00000004, 0x01000404, 0x00010404,
    0x01010404, 0x00010004, 0x01010000, 0x01000404,
    0x01000004, 0x00000404, 0x00010404, 0x01010400,
    0x00000404, 0x01000400, 0x01000400, 0x00000000,
    0x00010004, 0x00010400, 0x00000000, 0x01010004
};

static const uint32_t SB2[64] =
{
    0x80108020, 0x80008000, 0x00008000, 0x00108020,
    0x00100000, 0x00000020, 0x80100020, 0x80008020,
    0x80000020, 0x80108020, 0x80108000, 0x80000000,
    0x80008000, 0x00100000, 0x00000020, 0x80100020,
    0x00108000, 0x00100020, 0x80008020, 0x00000000,
    0x80000000, 0x00008000, 0x00108020, 0x80100000,
    0x00100020, 0x80000020, 0x00000000, 0x00108000,
    0x00008020, 0x80108000, 0x80100000, 0x00008020,
    0x00000000, 0x00108020, 0x80100020, 0x00100000,
    0x80008020, 0x80100000, 0x80108000, 0x00008000,
    0x80100000, 0x80008000, 0x00000020, 0x80108020,
    0x00108020, 0x00000020, 0x00008000, 0x80000000,
    0x00008020, 0x80108000, 0x00100000, 0x80000020,
    0x00100020, 0x80008020, 0x80000020, 0x00100020,
    0x00108000, 0x00000000, 0x80008000, 0x00008020,
    0x80000000, 0x80100020, 0x80108020, 0x00108000
};

static const uint32_t SB3[64] =
{
    0x00000208, 0x08020200, 0x00000000, 0x08020008,
    0x08000200, 0x00000000, 0x00020208, 0x08000200,
    0x00020008, 0x08000008, 0x08000008, 0x00020000,
    0x08020208, 0x00020008, 0x08020000, 0x00000208,
    0x08000000, 0x00000008, 0x08020200, 0x00000200,
    0x00020200, 0x08020000, 0x08020008, 0x00020208,
    0x08000208, 0x00020200, 0x00020000, 0x08000208,
    0x00000008, 0x08020208, 0x00000200, 0x08000000,
    0x08020200, 0x08000000, 0x00020008, 0x00000208,
    0x00020000, 0x08020200, 0x08000200, 0x00000000,
    0x00000200, 0x00020008, 0x08020208, 0x08000200,
    0x08000008, 0x00000200, 0x00000000, 0x08020008,
    0x08000208, 0x00020000, 0x08000000, 0x08020208,
    0x00000008, 0x00020208, 0x00020200, 0x08000008,
    0x08020000, 0x08000208, 0x00000208, 0x08020000,
    0x00020208, 0x00000008, 0x08020008, 0x00020200
};

static const uint32_t SB4[64] =
{
    0x00802001, 0x00002081, 0x00002081, 0x00000080,
    0x00802080, 0x00800081, 0x00800001, 0x00002001,
    0x00000000, 0x00802000, 0x00802000, 0x00802081,
    0x00000081, 0x00000000, 0x00800080, 0x00800001,
    0x00000001, 0x00002000, 0x00800000, 0x00802001,
    0x00000080, 0x00800000, 0x00002001, 0x00002080,
    0x00800081, 0x00000001, 0x00002080, 0x00800080,
    0x00002000, 0x00802080, 0x00802081, 0x00000081,
    0x00800080, 0x00800001, 0x00802000, 0x00802081,
    0x00000081, 0x00000000, 0x00000000, 0x00802000,
    0x00002080, 0x00800080, 0x00800081, 0x00000001,
    0x00802001, 0x00002081, 0x00002081, 0x00000080,
    0x00802081, 0x00000081, 0x00000001, 0x00002000,
    0x00800001, 0x00002001, 0x00802080, 0x00800081,
    0x00002001, 0x00002080, 0x00800000, 0x00802001,
    0x00000080, 0x00800000, 0x00002000, 0x00802080
};

static const uint32_t SB5[64] =
{
    0x00000100, 0x02080100, 0x02080000, 0x42000100,
    0x00080000, 0x00000100, 0x40000000, 0x02080000,
    0x40080100, 0x00080000, 0x02000100, 0x40080100,
    0x42000100, 0x42080000, 0x00080100, 0x40000000,
    0x02000000, 0x40080000, 0x40080000, 0x00000000,
    0x40000100, 0x42080100, 0x42080100, 0x02000100,
    0x42080000, 0x40000100, 0x00000000, 0x42000000,
    0x02080100, 0x02000000, 0x42000000, 0x00080100,
    0x00080000, 0x42000100, 0x00000100, 0x02000000,
    0x40000000, 0x02080000, 0x42000100, 0x40080100,
    0x02000100, 0x40000000, 0x42080000, 0x02080100,
    0x40080100, 0x00000100, 0x02000000, 0x42080000,
    0x42080100, 0x00080100, 0x42000000, 0x42080100,
    0x02080000, 0x00000000, 0x40080000, 0x42000000,
    0x00080100, 0x02000100, 0x40000100, 0x00080000,
    0x00000000, 0x40080000, 0x02080100, 0x40000100
};

static const uint32_t SB6[64] =
{
    0x20000010, 0x20400000, 0x00004000, 0x20404010,
    0x20400000, 0x00000010, 0x20404010, 0x00400000,
    0x20004000, 0x00404010, 0x00400000, 0x20000010,
    0x00400010, 0x20004000, 0x20000000, 0x00004010,
    0x00000000, 0x00400010, 0x20004010, 0x00004000,
    0x00404000, 0x20004010, 0x00000010, 0x20400010,
    0x20400010, 0x00000000, 0x00404010, 0x20404000,
    0x00004010, 0x00404000, 0x20404000, 0x20000000,
    0x20004000, 0x00000010, 0x20400010, 0x00404000,
    0x20404010, 0x00400000, 0x00004010, 0x20000010,
    0x00400000, 0x20004000, 0x20000000, 0x00004010,
    0x20000010, 0x20404010, 0x00404000, 0x20400000,
    0x00404010, 0x20404000, 0x00000000, 0x20400010,
    0x00000010, 0x00004000, 0x20400000, 0x00404010,
    0x00004000, 0x00400010, 0x20004010, 0x00000000,
    0x20404000, 0x20000000, 0x00400010, 0x20004010
};

static const uint32_t SB7[64] =
{
    0x00200000, 0x04200002, 0x04000802, 0x00000000,
    0x00000800, 0x04000802, 0x00200802, 0x04200800,
    0x04200802, 0x00200000, 0x00000000, 0x04000002,
    0x00000002, 0x04000000, 0x04200002, 0x00000802,
    0x04000800, 0x00200802, 0x00200002, 0x04000800,
    0x04000002, 0x04200000, 0x04200800, 0x00200002,
    0x04200000, 0x00000800, 0x00000802, 0x04200802,
    0x00200800, 0x00000002, 0x04000000, 0x00200800,
    0x04000000, 0x00200800, 0x00200000, 0x04000802,
    0x04000802, 0x04200002, 0x04200002, 0x00000002,
    0x00200002, 0x04000000, 0x04000800, 0x00200000,
    0x04200800, 0x00000802, 0x00200802, 0x04200800,
    0x00000802, 0x04000002, 0x04200802, 0x04200000,
    0x00200800, 0x00000000, 0x00000002, 0x04200802,
    0x00000000, 0x00200802, 0x04200000, 0x00000800,
    0x04000002, 0x04000800, 0x00000800, 0x00200002
};

static const uint32_t SB8[64] =
{
    0x10001040, 0x00001000, 0x00040000, 0x10041040,
    0x10000000, 0x10001040, 0x00000040, 0x10000000,
    0x00040040, 0x10040000, 0x10041040, 0x00041000,
    0x10041000, 0x00041040, 0x00001000, 0x00000040,
    0x10040000, 0x10000040, 0x10001000, 0x00001040,
    0x00041000, 0x00040040, 0x10040040, 0x10041000,
    0x00001040, 0x00000000, 0x00000000, 0x10040040,
    0x10000040, 0x10001000, 0x00041040, 0x00040000,
    0x00041040, 0x00040000, 0x10041000, 0x00001000,
    0x00000040, 0x10040040, 0x00001000, 0x00041040,
    0x10001000, 0x00000040, 0x10000040, 0x10040000,
    0x10040040, 0x10000000, 0x00040000, 0x10001040,
    0x00000000, 0x10041040, 0x00040040, 0x10000040,
    0x10040000, 0x10001000, 0x10001040, 0x00000000,
    0x10041040, 0x00041000, 0x00041000, 0x00001040,
    0x00001040, 0x00040040, 0x10000000, 0x10041000
};

/*
 * PC1: left and right halves bit-swap
 */
static const uint32_t LHs[16] =
{
    0x00000000, 0x00000001, 0x00000100, 0x00000101,
    0x00010000, 0x00010001, 0x00010100, 0x00010101,
    0x01000000, 0x01000001, 0x01000100, 0x01000101,
    0x01010000, 0x01010001, 0x01010100, 0x01010101
};

static const uint32_t RHs[16] =
{
    0x00000000, 0x01000000, 0x00010000, 0x01010000,
    0x00000100, 0x01000100, 0x00010100, 0x01010100,
    0x00000001, 0x01000001, 0x00010001, 0x01010001,
    0x00000101, 0x01000101, 0x00010101, 0x01010101,
};

/*
 * Initial Permutation macro
 */
#define DES_IP(X,Y)                                         \
{                                                           \
T = ((X >>  4) ^ Y) & 0x0F0F0F0F; Y ^= T; X ^= (T <<  4);   \
T = ((X >> 16) ^ Y) & 0x0000FFFF; Y ^= T; X ^= (T << 16);   \
T = ((Y >>  2) ^ X) & 0x33333333; X ^= T; Y ^= (T <<  2);   \
T = ((Y >>  8) ^ X) & 0x00FF00FF; X ^= T; Y ^= (T <<  8);   \
Y = ((Y << 1) | (Y >> 31)) & 0xFFFFFFFF;                    \
T = (X ^ Y) & 0xAAAAAAAA; Y ^= T; X ^= T;                   \
X = ((X << 1) | (X >> 31)) & 0xFFFFFFFF;                    \
}

/*
 * Final Permutation macro
 */
#define DES_FP(X,Y)                                         \
{                                                           \
X = ((X << 31) | (X >> 1)) & 0xFFFFFFFF;                    \
T = (X ^ Y) & 0xAAAAAAAA; X ^= T; Y ^= T;                   \
Y = ((Y << 31) | (Y >> 1)) & 0xFFFFFFFF;                    \
T = ((Y >>  8) ^ X) & 0x00FF00FF; X ^= T; Y ^= (T <<  8);   \
T = ((Y >>  2) ^ X) & 0x33333333; X ^= T; Y ^= (T <<  2);   \
T = ((X >> 16) ^ Y) & 0x0000FFFF; Y ^= T; X ^= (T << 16);   \
T = ((X >>  4) ^ Y) & 0x0F0F0F0F; Y ^= T; X ^= (T <<  4);   \
}

/*
 * DES round macro
 */
#define DES_ROUND(X,Y)                 \
{                                      \
T = *SK++ ^ X;                         \
Y ^= SB8[ (T      ) & 0x3F ] ^         \
SB6[ (T >>  8) & 0x3F ] ^              \
SB4[ (T >> 16) & 0x3F ] ^              \
SB2[ (T >> 24) & 0x3F ];               \
\
T = *SK++ ^ ((X << 28) | (X >> 4));    \
Y ^= SB7[ (T      ) & 0x3F ] ^         \
SB5[ (T >>  8) & 0x3F ] ^              \
SB3[ (T >> 16) & 0x3F ] ^              \
SB1[ (T >> 24) & 0x3F ];               \
}

#define SWAP(a,b) { uint32_t t = a; a = b; b = t; t = 0; }

void mbedtls_des_init( mbedtls_des_context *ctx )
{
    memset( ctx, 0, sizeof( mbedtls_des_context ) );
}

void mbedtls_des_free( mbedtls_des_context *ctx )
{
    if( ctx == NULL )
        return;
    
    mbedtls_platform_zeroize( ctx, sizeof( mbedtls_des_context ) );
}


static const unsigned char odd_parity_table[128] = { 1,  2,  4,  7,  8,
    11, 13, 14, 16, 19, 21, 22, 25, 26, 28, 31, 32, 35, 37, 38, 41, 42, 44,
    47, 49, 50, 52, 55, 56, 59, 61, 62, 64, 67, 69, 70, 73, 74, 76, 79, 81,
    82, 84, 87, 88, 91, 93, 94, 97, 98, 100, 103, 104, 107, 109, 110, 112,
    115, 117, 118, 121, 122, 124, 127, 128, 131, 133, 134, 137, 138, 140,
    143, 145, 146, 148, 151, 152, 155, 157, 158, 161, 162, 164, 167, 168,
    171, 173, 174, 176, 179, 181, 182, 185, 186, 188, 191, 193, 194, 196,
    199, 200, 203, 205, 206, 208, 211, 213, 214, 217, 218, 220, 223, 224,
    227, 229, 230, 233, 234, 236, 239, 241, 242, 244, 247, 248, 251, 253,
    254 };

void mbedtls_des_key_set_parity( unsigned char key[MBEDTLS_DES_KEY_SIZE] )
{
    int i;
    
    for( i = 0; i < MBEDTLS_DES_KEY_SIZE; i++ )
        key[i] = odd_parity_table[key[i] / 2];
}

/*
 * Check the given key's parity, returns 1 on failure, 0 on SUCCESS
 */
int mbedtls_des_key_check_key_parity( const unsigned char key[MBEDTLS_DES_KEY_SIZE] )
{
    int i;
    
    for( i = 0; i < MBEDTLS_DES_KEY_SIZE; i++ )
        if( key[i] != odd_parity_table[key[i] / 2] )
            return( 1 );
    
    return( 0 );
}

/*
 * Table of weak and semi-weak keys
 *
 * Source: http://en.wikipedia.org/wiki/Weak_key
 *
 * Weak:
 * Alternating ones + zeros (0x0101010101010101)
 * Alternating 'F' + 'E' (0xFEFEFEFEFEFEFEFE)
 * '0xE0E0E0E0F1F1F1F1'
 * '0x1F1F1F1F0E0E0E0E'
 *
 * Semi-weak:
 * 0x011F011F010E010E and 0x1F011F010E010E01
 * 0x01E001E001F101F1 and 0xE001E001F101F101
 * 0x01FE01FE01FE01FE and 0xFE01FE01FE01FE01
 * 0x1FE01FE00EF10EF1 and 0xE01FE01FF10EF10E
 * 0x1FFE1FFE0EFE0EFE and 0xFE1FFE1FFE0EFE0E
 * 0xE0FEE0FEF1FEF1FE and 0xFEE0FEE0FEF1FEF1
 *
 */

#define WEAK_KEY_COUNT 16

static const unsigned char weak_key_table[WEAK_KEY_COUNT][MBEDTLS_DES_KEY_SIZE] =
{
    { 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01 },
    { 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE, 0xFE },
    { 0x1F, 0x1F, 0x1F, 0x1F, 0x0E, 0x0E, 0x0E, 0x0E },
    { 0xE0, 0xE0, 0xE0, 0xE0, 0xF1, 0xF1, 0xF1, 0xF1 },
    
    { 0x01, 0x1F, 0x01, 0x1F, 0x01, 0x0E, 0x01, 0x0E },
    { 0x1F, 0x01, 0x1F, 0x01, 0x0E, 0x01, 0x0E, 0x01 },
    { 0x01, 0xE0, 0x01, 0xE0, 0x01, 0xF1, 0x01, 0xF1 },
    { 0xE0, 0x01, 0xE0, 0x01, 0xF1, 0x01, 0xF1, 0x01 },
    { 0x01, 0xFE, 0x01, 0xFE, 0x01, 0xFE, 0x01, 0xFE },
    { 0xFE, 0x01, 0xFE, 0x01, 0xFE, 0x01, 0xFE, 0x01 },
    { 0x1F, 0xE0, 0x1F, 0xE0, 0x0E, 0xF1, 0x0E, 0xF1 },
    { 0xE0, 0x1F, 0xE0, 0x1F, 0xF1, 0x0E, 0xF1, 0x0E },
    { 0x1F, 0xFE, 0x1F, 0xFE, 0x0E, 0xFE, 0x0E, 0xFE },
    { 0xFE, 0x1F, 0xFE, 0x1F, 0xFE, 0x0E, 0xFE, 0x0E },
    { 0xE0, 0xFE, 0xE0, 0xFE, 0xF1, 0xFE, 0xF1, 0xFE },
    { 0xFE, 0xE0, 0xFE, 0xE0, 0xFE, 0xF1, 0xFE, 0xF1 }
};

int mbedtls_des_key_check_weak( const unsigned char key[MBEDTLS_DES_KEY_SIZE] )
{
    int i;
    
    for( i = 0; i < WEAK_KEY_COUNT; i++ )
        if( memcmp( weak_key_table[i], key, MBEDTLS_DES_KEY_SIZE) == 0 )
            return( 1 );
    
    return( 0 );
}

void mbedtls_des_setkey( uint32_t SK[8], const unsigned char key[MBEDTLS_DES_KEY_SIZE] )
{
    int i;
    uint32_t X, Y, T;
    
    GET_UINT32_BE( X, key, 0 );
    GET_UINT32_BE( Y, key, 4 );
    
    /*
     * Permuted Choice 1
     */
    T =  ((Y >>  4) ^ X) & 0x0F0F0F0F;  X ^= T; Y ^= (T <<  4);
    T =  ((Y      ) ^ X) & 0x10101010;  X ^= T; Y ^= (T      );
    
    X =   (LHs[ (X      ) & 0xF] << 3) | (LHs[ (X >>  8) & 0xF ] << 2)
    | (LHs[ (X >> 16) & 0xF] << 1) | (LHs[ (X >> 24) & 0xF ]     )
    | (LHs[ (X >>  5) & 0xF] << 7) | (LHs[ (X >> 13) & 0xF ] << 6)
    | (LHs[ (X >> 21) & 0xF] << 5) | (LHs[ (X >> 29) & 0xF ] << 4);
    
    Y =   (RHs[ (Y >>  1) & 0xF] << 3) | (RHs[ (Y >>  9) & 0xF ] << 2)
    | (RHs[ (Y >> 17) & 0xF] << 1) | (RHs[ (Y >> 25) & 0xF ]     )
    | (RHs[ (Y >>  4) & 0xF] << 7) | (RHs[ (Y >> 12) & 0xF ] << 6)
    | (RHs[ (Y >> 20) & 0xF] << 5) | (RHs[ (Y >> 28) & 0xF ] << 4);
    
    X &= 0x0FFFFFFF;
    Y &= 0x0FFFFFFF;
    
    /*
     * calculate subkeys
     */
    for( i = 0; i < 4; i++ )
    {
        if( i < 2 )
        {
            X = ((X <<  1) | (X >> 27)) & 0x0FFFFFFF;
            Y = ((Y <<  1) | (Y >> 27)) & 0x0FFFFFFF;
        }
        else
        {
            X = ((X <<  2) | (X >> 26)) & 0x0FFFFFFF;
            Y = ((Y <<  2) | (Y >> 26)) & 0x0FFFFFFF;
        }
        
        *SK++ =   ((X <<  4) & 0x24000000) | ((X << 28) & 0x10000000)
        | ((X << 14) & 0x08000000) | ((X << 18) & 0x02080000)
        | ((X <<  6) & 0x01000000) | ((X <<  9) & 0x00200000)
        | ((X >>  1) & 0x00100000) | ((X << 10) & 0x00040000)
        | ((X <<  2) & 0x00020000) | ((X >> 10) & 0x00010000)
        | ((Y >> 13) & 0x00002000) | ((Y >>  4) & 0x00001000)
        | ((Y <<  6) & 0x00000800) | ((Y >>  1) & 0x00000400)
        | ((Y >> 14) & 0x00000200) | ((Y      ) & 0x00000100)
        | ((Y >>  5) & 0x00000020) | ((Y >> 10) & 0x00000010)
        | ((Y >>  3) & 0x00000008) | ((Y >> 18) & 0x00000004)
        | ((Y >> 26) & 0x00000002) | ((Y >> 24) & 0x00000001);
        
        *SK++ =   ((X << 15) & 0x20000000) | ((X << 17) & 0x10000000)
        | ((X << 10) & 0x08000000) | ((X << 22) & 0x04000000)
        | ((X >>  2) & 0x02000000) | ((X <<  1) & 0x01000000)
        | ((X << 16) & 0x00200000) | ((X << 11) & 0x00100000)
        | ((X <<  3) & 0x00080000) | ((X >>  6) & 0x00040000)
        | ((X << 15) & 0x00020000) | ((X >>  4) & 0x00010000)
        | ((Y >>  2) & 0x00002000) | ((Y <<  8) & 0x00001000)
        | ((Y >> 14) & 0x00000808) | ((Y >>  9) & 0x00000400)
        | ((Y      ) & 0x00000200) | ((Y <<  7) & 0x00000100)
        | ((Y >>  7) & 0x00000020) | ((Y >>  3) & 0x00000011)
        | ((Y <<  2) & 0x00000004) | ((Y >> 21) & 0x00000002);
    }
}

/*
 * DES key schedule (56-bit, encryption)
 */
int mbedtls_des_setkey_enc( mbedtls_des_context *ctx, const unsigned char key[MBEDTLS_DES_KEY_SIZE] )
{
    mbedtls_des_setkey( ctx->sk, key );
    
    return( 0 );
}

/*
 * DES key schedule (56-bit, decryption)
 */
int mbedtls_des_setkey_dec( mbedtls_des_context *ctx, const unsigned char key[MBEDTLS_DES_KEY_SIZE] )
{
    int i;
    
    mbedtls_des_setkey( ctx->sk, key );
    
    for( i = 0; i < 4; i += 2 )
    {
        SWAP( ctx->sk[i    ], ctx->sk[6 - i] );
        SWAP( ctx->sk[i + 1], ctx->sk[7 - i] );
    }
    
    return( 0 );
}


/*
 * DES-ECB block encryption/decryption
 */
//#if !defined(MBEDTLS_DES_CRYPT_ECB_ALT)
int des4r_crypt_ecb( mbedtls_des_context *ctx,
                           const unsigned char * inFile,
                          unsigned char * outFile )
{
    int i;
    uint32_t X, Y, T, *SK;
    
    SK = ctx->sk;
    
		GET_UINT32_BE(X, inFile, 0);
		GET_UINT32_BE(Y, inFile, 4);

		DES_IP(X, Y);

		for (i = 0; i < 2; i++)
		{
			DES_ROUND(Y, X);
			DES_ROUND(X, Y);
		}

		DES_FP(Y, X);

		PUT_UINT32_BE(Y, outFile, 0);
		PUT_UINT32_BE(X, outFile, 4);
	
    return( 0 );
}

/*
 * DES-CBC buffer encryption/decryption
 */
int des4r_crypt_cbc( mbedtls_des_context *ctx,
                          int mode,
                          size_t length,
                          unsigned char iv[8],
                          const unsigned char *inFile,
                          unsigned char *outFile )
{
    int i;
    unsigned char temp[8];
    
    if( length % 8 )
        return( MBEDTLS_ERR_DES_INVALID_INPUT_LENGTH );
    
    if( mode == MBEDTLS_DES_ENCRYPT )
    {
        while( length > 0 )
        {
            for( i = 0; i < 8; i++ )
                outFile[i] = (unsigned char)( inFile[i] ^ iv[i] );
            
            des4r_crypt_ecb( ctx, outFile, outFile );
            memcpy( iv, outFile, 8 );
            
            inFile  += 8;
            outFile += 8;
            length -= 8;
        }
    }
    else /* MBEDTLS_DES_DECRYPT */
    {
        while( length > 0 )
        {
            memcpy( temp, inFile, 8 );
            des4r_crypt_ecb( ctx, inFile, outFile );
            
            for( i = 0; i < 8; i++ )
                outFile[i] = (unsigned char)( outFile[i] ^ iv[i] );
            
            memcpy( iv, temp, 8 );
            
            inFile  += 8;
            outFile += 8;
            length -= 8;
        }
    }
    
    return( 0 );
}
/*
* DES-OFB buffer encryption/decryption
*/
int des4r_crypt_ofb(mbedtls_des_context *ctx, int mode, size_t length, unsigned char iv[8],
	char* inFile, char* outFile){

	int i;
	unsigned char prev[8]; //array that will be used to keep in track of des encryption starting with initial vector
	

	//copy initial vector into prev char array
	memcpy(prev, iv, sizeof(iv));

	
	if (length % 8)
		return(MBEDTLS_ERR_DES_INVALID_INPUT_LENGTH);

	if (mode == MBEDTLS_DES_ENCRYPT)
	{
		while (length > 0)
		{
			des4r_crypt_ecb(ctx, prev, prev);
			for (i = 0; i < 8; i++)
				outFile[i] = (unsigned char)(inFile[i] ^ prev[i]);
			inFile += 8;
			outFile += 8;
			length -= 8;
		}
	}
	else /* MBEDTLS_DES_DECRYPT */
	{
		while (length > 0)
		{
			des4r_crypt_ecb(ctx, prev, prev);

			for (i = 0; i < 8; i++)
				outFile[i] = (unsigned char)(inFile[i] ^ prev[i]);

			

			inFile += 8;
			outFile += 8;
			length -= 8;
		}
	}

	return(0);
}


/*
 * DES and 3DES test vectors from:
 *
 * http://csrc.nist.gov/groups/STM/cavp/documents/des/tripledes-vectors.zip
 */
static const unsigned char des4r_test_keys[8] =
{
    0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
 
};

static const unsigned char des3_test_buf[8] =
{
    0x4E, 0x6F, 0x77, 0x20, 0x69, 0x73, 0x20, 0x74
};

static const unsigned char des3_test_ecb_dec[3][8] =
{
    { 0xCD, 0xD6, 0x4F, 0x2F, 0x94, 0x27, 0xC1, 0x5D },
    { 0x69, 0x96, 0xC8, 0xFA, 0x47, 0xA2, 0xAB, 0xEB },
    { 0x83, 0x25, 0x39, 0x76, 0x44, 0x09, 0x1A, 0x0A }
};

static const unsigned char des3_test_ecb_enc[3][8] =
{
    { 0x6A, 0x2A, 0x19, 0xF4, 0x1E, 0xCA, 0x85, 0x4B },
    { 0x03, 0xE6, 0x9F, 0x5B, 0xFA, 0x58, 0xEB, 0x42 },
    { 0xDD, 0x17, 0xE8, 0xB8, 0xB4, 0x37, 0xD2, 0x32 }
};


static const unsigned char des4r_test_iv[8] =
{
    0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF,
};

static const unsigned char des3_test_cbc_dec[3][8] =
{
    { 0x12, 0x9F, 0x40, 0xB9, 0xD2, 0x00, 0x56, 0xB3 },
    { 0x47, 0x0E, 0xFC, 0x9A, 0x6B, 0x8E, 0xE3, 0x93 },
    { 0xC5, 0xCE, 0xCF, 0x63, 0xEC, 0xEC, 0x51, 0x4C }
};

static const unsigned char des3_test_cbc_enc[3][8] =
{
    { 0x54, 0xF1, 0x5A, 0xF6, 0xEB, 0xE3, 0xA4, 0xB4 },
    { 0x35, 0x76, 0x11, 0x56, 0x5F, 0xA1, 0x8E, 0x4D },
    { 0xCB, 0x19, 0x1F, 0x85, 0xD1, 0xED, 0x84, 0x39 }
};

/*
 * Checkup routine
 */
int mbedtls_des_self_test( int verbose)
{ 
	int j = 0;// u, v, ret = 0;
    mbedtls_des_context ctx;
    unsigned char iv[8];
	int i = 0;
	
	//opening file and extracting text from file
	FILE * input;
	fopen_s(&input, "plain.txt", "r");
	unsigned char text[528];
	while (!feof(input))
		text[i++] = fgetc(input);
	//add extra padding to make block size a multiple of 64
	text[527] = 0x80;
	
	
	//closing file
	fclose(input);

    //output file that will show encryption and decryption
	FILE * outFile;
	fopen_s(&outFile, "output.txt", "w");
	

	char encryptedText[528];
	char decryptedText[528];
	char block[8];
	char output[8];
	int loopCounter = 0;


	/*
	* ECB mode 
	*/

	fprintf(outFile, "ECB MODE\n");
	fprintf(outFile, "--------------------------------\n");
	for (j = 0; j < sizeof(text); j += 8){
		memcpy(block, text + j, 8);
		mbedtls_des_setkey_enc(&ctx, des4r_test_keys);
		des4r_crypt_ecb(&ctx, block, output);


		memcpy(encryptedText + j, output, 8);
		
		
		mbedtls_des_setkey_dec(&ctx, des4r_test_keys);
		des4r_crypt_ecb(&ctx, output, output);
        
		memcpy(decryptedText + j, output, 8);
	}
	
	
	fprintf(outFile, "Encrypted Text:\n");
	for (i = 0; i < 528; i++){
		fprintf(outFile, "%x", encryptedText[i]);
	}
	fprintf(outFile, "\n");
	fprintf(outFile,"\n");

	
	fprintf(outFile, "Decrypted Text:\n");
	for (i = 0; i < 527; i++){
		fprintf(outFile, "%x", decryptedText[i]);
	}
	fprintf(outFile, "\n\n");	

    /*
     * CBC mode
     */
	fprintf(outFile, "CBC MODE\n");
	fprintf(outFile, "--------------------------------\n");
	
	//set encryptedText and decrpytedText back to null
	memset(encryptedText, '\0', sizeof(encryptedText));
	memset(decryptedText, '\0', sizeof(decryptedText));
	
	int mode;
	for (i = 0; i < 2; i++)
	{
		mode = i == 0 ? 1 : 0;

		memcpy(iv, des4r_test_iv, 8);



		switch (i)
		{
		case 0:
			mbedtls_des_setkey_enc(&ctx, des4r_test_keys);
			des4r_crypt_cbc(&ctx, mode, sizeof(text), iv, text, encryptedText);
			break;

		case 1:

			mbedtls_des_setkey_dec(&ctx, des4r_test_keys);
			des4r_crypt_cbc(&ctx, mode, sizeof(encryptedText), iv, encryptedText, decryptedText);
			break;
		default:
			return(1);
		}

	}

	fprintf(outFile,"Encrypted Text:\n");
	for (i = 0; i < 528; i++)
		fprintf(outFile,"%x", encryptedText[i]);
	fprintf(outFile,"\n");
	fprintf(outFile,"\n");

	fprintf(outFile,"Decrypted Text:\n");
	for (i = 0; i < 527; i++)
		fprintf(outFile,"%x", decryptedText[i]);
	fprintf(outFile,"\n\n");

	fprintf(outFile, "OFB MODE\n");
	fprintf(outFile, "--------------------------------\n");

	//set encryptedText and decrpytedText back to null
	memset(encryptedText, '\0', sizeof(encryptedText));
	memset(decryptedText, '\0', sizeof(decryptedText));

	for (i = 0; i < 2; i++)
	{
		mode = i == 0 ? 1 : 0;

		memcpy(iv, des4r_test_iv, 8);



		switch (i)
		{
		case 0:
			mbedtls_des_setkey_enc(&ctx, des4r_test_keys);
			des4r_crypt_ofb(&ctx, mode, sizeof(text), iv, text, encryptedText);
			break;

		case 1:

			mbedtls_des_setkey_enc(&ctx, des4r_test_keys);
			des4r_crypt_ofb(&ctx, mode, sizeof(encryptedText), iv, encryptedText, decryptedText);
			break;
		default:
			return(1);
		}

	}

	fprintf(outFile, "Encrypted Text:\n");
	for (i = 0; i < 528; i++)
		fprintf(outFile, "%x", encryptedText[i]);
	fprintf(outFile, "\n");
	fprintf(outFile, "\n");

	fprintf(outFile, "Decrypted Text:\n");
	for (i = 0; i < 527; i++)
		fprintf(outFile, "%x", decryptedText[i]);
	fprintf(outFile, "\n");

	fclose(outFile);
	return 0;
}



