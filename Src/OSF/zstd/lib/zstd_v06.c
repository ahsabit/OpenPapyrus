/*
 * Copyright (c) Yann Collet, Facebook, Inc. All rights reserved.
 *
 * This source code is licensed under both the BSD-style license (found in the
 * LICENSE file in the root directory of this source tree) and the GPLv2 (found
 * in the COPYING file in the root directory of this source tree).
 * You may select, at your option, one of the above-listed licenses.
 */
#include <zstd-internal.h>
#pragma hdrstop
#include "zstd_v06.h"
#include <error_private.h>

/* ******************************************************************
   mem.h
   low-level memory access routines
   Copyright (C) 2013-2015, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

* Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

    You can contact the author at :
    - FSE source repository : https://github.com/Cyan4973/FiniteStateEntropy
    - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */
#ifndef __ZSTD_MEM_H_MODULE
#define __ZSTD_MEM_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif
//
// Compiler specifics
//
#if defined(__GNUC__)
	#define MEM_STATIC static __attribute__((unused))
#elif defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
	#define MEM_STATIC static inline
#elif defined(_MSC_VER)
	#define MEM_STATIC static __inline
#else
	#define MEM_STATIC static  /* this version may generate warnings for unused static functions; disable the relevant warning */
#endif
//
// Basic Types
//
#if  !defined (__VMS) && (defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */))
	#if defined(_AIX)
		#include <inttypes.h>
	#else
		#include <stdint.h> /* intptr_t */
	#endif
	typedef  uint8_t BYTE;
	//typedef uint16_t U16_Removed;
	//typedef  int16_t S16_Removed;
	//typedef uint32_t U32_Removed;
	//typedef  int32_t S32_Removed;
	//typedef uint64_t U64_Removed;
	//typedef  int64_t S64_Removed;
#else
	typedef unsigned char BYTE;
	//typedef unsigned short U16_Removed;
	//typedef   signed short S16_Removed;
	//typedef unsigned int U32_Removed;
	//typedef  signed int S32_Removed;
	//typedef uint64 U64_Removed;
	//typedef   signed long long S64_Removed;
#endif
//
// Memory I/O
//
/* MEM_FORCE_MEMORY_ACCESS :
 * By default, access to unaligned memory is controlled by `memcpy()`, which is safe and portable.
 * Unfortunately, on some target/compiler combinations, the generated assembly is sub-optimal.
 * The below switch allow to select different access method for improved performance.
 * Method 0 (default) : use `memcpy()`. Safe and portable.
 * Method 1 : `__packed` statement. It depends on compiler extension (ie, not portable).
 *            This method is safe if your compiler supports it, and *generally* as fast or faster than `memcpy`.
 * Method 2 : direct access. This method is portable but violate C standard.
 *            It can generate buggy code on targets depending on alignment.
 *            In some circumstances, it's the only known way to get the most performance (ie GCC + ARMv6)
 * See http://fastcompression.blogspot.fr/2015/08/accessing-unaligned-memory.html for details.
 * Prefer these methods in priority order (0 > 1 > 2)
 */
#ifndef MEM_FORCE_MEMORY_ACCESS   /* can be defined externally, on command line for example */
#if defined(__INTEL_COMPILER) || defined(__GNUC__) || defined(__ICCARM__)
#define MEM_FORCE_MEMORY_ACCESS 1
#endif
#endif

//MEM_STATIC uint MEM_32bits() { return sizeof(size_t)==4; }
//MEM_STATIC uint MEM_64bits() { return sizeof(size_t)==8; }

/*MEM_STATIC uint MEM_isLittleEndian()
{
	const union { uint32 u; BYTE c[4]; } one = { 1 }; // don't use static : performance detrimental
	return one.c[0];
}*/

#if defined(MEM_FORCE_MEMORY_ACCESS) && (MEM_FORCE_MEMORY_ACCESS==2)

/* violates C standard, by lying on structure alignment.
   Only use if no other choice to achieve best performance on target platform */
//MEM_STATIC uint16 MEM_read16_Removed(const void* memPtr) { return *(const uint16*)memPtr; }
//MEM_STATIC uint32 MEM_read32_Removed(const void* memPtr) { return *(const uint32 *)memPtr; }
//MEM_STATIC uint64 MEM_read64_Removed(const void* memPtr) { return *(const uint64*)memPtr; }
//MEM_STATIC void MEM_write16_Removed(void* memPtr, uint16 value) { *(uint16*)memPtr = value; }

#elif defined(MEM_FORCE_MEMORY_ACCESS) && (MEM_FORCE_MEMORY_ACCESS==1)

/* __pack instructions are safer, but compiler specific, hence potentially problematic for some compilers */
/* currently only defined for gcc and icc */
typedef union { uint16 u16; uint32 u32; uint64 u64; size_t st; } __attribute__((packed)) unalign;

//MEM_STATIC uint16 MEM_read16_Removed(const void* ptr) { return ((const unalign*)ptr)->u16; }
//MEM_STATIC uint32 MEM_read32_Removed(const void* ptr) { return ((const unalign*)ptr)->u32; }
//MEM_STATIC uint64 MEM_read64_Removed(const void* ptr) { return ((const unalign*)ptr)->u64; }
//MEM_STATIC void MEM_write16_Removed(void* memPtr, uint16 value) { ((unalign*)memPtr)->u16 = value; }

#else

/* default method, safe and standard. can sometimes prove slower */

//MEM_STATIC uint16 MEM_read16_Removed(const void* memPtr) { uint16 val; memcpy(&val, memPtr, sizeof(val)); return val; }
//MEM_STATIC uint32 MEM_read32_Removed(const void* memPtr) { uint32 val; memcpy(&val, memPtr, sizeof(val)); return val; }
//MEM_STATIC uint64 MEM_read64_Removed(const void* memPtr) { uint64 val; memcpy(&val, memPtr, sizeof(val)); return val; }
//MEM_STATIC void MEM_write16_Removed(void* memPtr, uint16 value) { memcpy(memPtr, &value, sizeof(value)); }

#endif /* MEM_FORCE_MEMORY_ACCESS */

/*MEM_STATIC uint32 MEM_swap32_Removed(uint32 in)
{
#if defined(_MSC_VER)
	return _byteswap_ulong(in);
#elif defined (__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 403)
	return __builtin_bswap32(in);
#else
	return ((in << 24) & 0xff000000 ) | ((in <<  8) & 0x00ff0000 ) | ((in >>  8) & 0x0000ff00 ) | ((in >> 24) & 0x000000ff );
#endif
}*/

/*MEM_STATIC uint64 MEM_swap64_Removed(uint64 in)
{
#if defined(_MSC_VER)
	return _byteswap_uint64(in);
#elif defined (__GNUC__) && (__GNUC__ * 100 + __GNUC_MINOR__ >= 403)
	return __builtin_bswap64(in);
#else
	return ((in << 56) & 0xff00000000000000ULL) |
	       ((in << 40) & 0x00ff000000000000ULL) |
	       ((in << 24) & 0x0000ff0000000000ULL) |
	       ((in << 8)  & 0x000000ff00000000ULL) |
	       ((in >> 8)  & 0x00000000ff000000ULL) |
	       ((in >> 24) & 0x0000000000ff0000ULL) |
	       ((in >> 40) & 0x000000000000ff00ULL) |
	       ((in >> 56) & 0x00000000000000ffULL);
#endif
}*/

/*=== Little endian r/w ===*/

/*MEM_STATIC uint16 MEM_readLE16_Removed(const void* memPtr)
{
	if(MEM_isLittleEndian())
		return SMem::Get16(memPtr);
	else {
		const BYTE * p = (const BYTE *)memPtr;
		return (uint16)(p[0] + (p[1]<<8));
	}
}*/

/*MEM_STATIC void MEM_writeLE16_Removed(void* memPtr, uint16 val)
{
	if(MEM_isLittleEndian()) {
		SMem::Put(memPtr, val);
	}
	else {
		BYTE * p = (BYTE *)memPtr;
		p[0] = (BYTE)val;
		p[1] = (BYTE)(val>>8);
	}
}*/

/*MEM_STATIC uint32 MEM_readLE32_Removed(const void* memPtr)
{
	if(MEM_isLittleEndian())
		return SMem::Get32(memPtr);
	else
		return SMem::BSwap(SMem::Get32(memPtr));
}*/

/*MEM_STATIC uint64 MEM_readLE64_Removed(const void* memPtr)
{
	if(MEM_isLittleEndian())
		return SMem::Get64(memPtr);
	else
		return SMem::BSwap(SMem::Get64(memPtr));
}*/

/*MEM_STATIC size_t MEM_readLEST_Removed(const void* memPtr)
{
	if(MEM_32bits())
		return (size_t)SMem::GetLe32(memPtr);
	else
		return (size_t)SMem::GetLe64(memPtr);
}*/

#if defined (__cplusplus)
}
#endif

#endif /* __ZSTD_MEM_H_MODULE */
/*
    zstd - standard compression library
    Header File for static linking only
    Copyright (C) 2014-2016, Yann Collet.

    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
 * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

    You can contact the author at : - zstd homepage : http://www.zstd.net
 */
#ifndef ZSTDv06_STATIC_H
#define ZSTDv06_STATIC_H

/* The prototypes defined within this file are considered experimental.
 * They should not be used in the context DLL as they may change in the future.
 * Prefer static linking if you need them, to control breaking version changes issues.
 */

#if defined (__cplusplus)
extern "C" {
#endif

/*- Advanced Decompression functions -*/

/*! ZSTDv06_decompress_usingPreparedDCtx() :
 *   Same as ZSTDv06_decompress_usingDict, but using a reference context `preparedDCtx`, where dictionary has been
 *loaded.
 *   It avoids reloading the dictionary each time.
 *   `preparedDCtx` must have been properly initialized using ZSTDv06_decompressBegin_usingDict().
 *   Requires 2 contexts : 1 for reference (preparedDCtx), which will not be modified, and 1 to run the decompression
 *operation (dctx) */
ZSTDLIBv06_API size_t ZSTDv06_decompress_usingPreparedDCtx(ZSTDv06_DCtx* dctx, const ZSTDv06_DCtx* preparedDCtx,
    void* dst, size_t dstCapacity, const void* src, size_t srcSize);

#define ZSTDv06_FRAMEHEADERSIZE_MAX 13    /* for static allocation */
static const size_t ZSTDv06_frameHeaderSize_min = 5;
static const size_t ZSTDv06_frameHeaderSize_max = ZSTDv06_FRAMEHEADERSIZE_MAX;

ZSTDLIBv06_API size_t ZSTDv06_decompressBegin(ZSTDv06_DCtx* dctx);

/*
   Streaming decompression, direct mode (bufferless)

   A ZSTDv06_DCtx object is required to track streaming operations.
   Use ZSTDv06_createDCtx() / ZSTDv06_freeDCtx() to manage it.
   A ZSTDv06_DCtx object can be re-used multiple times.

   First optional operation is to retrieve frame parameters, using ZSTDv06_getFrameParams(), which doesn't consume the
      input.
   It can provide the minimum size of rolling buffer required to properly decompress data,
   and optionally the final size of uncompressed content.
   (Note : content size is an optional info that may not be present. 0 means : content size unknown)
   Frame parameters are extracted from the beginning of compressed frame.
   The amount of data to read is variable, from ZSTDv06_frameHeaderSize_min to ZSTDv06_frameHeaderSize_max (so if
      `srcSize` >= ZSTDv06_frameHeaderSize_max, it will always work)
   If `srcSize` is too small for operation to succeed, function will return the minimum size it requires to produce a
      result.
   Result : 0 when successful, it means the ZSTDv06_frameParams structure has been filled.
          >0 : means there is not enough data into `src`. Provides the expected size to successfully decode header.
           errorCode, which can be tested using ZSTDv06_isError()

   Start decompression, with ZSTDv06_decompressBegin() or ZSTDv06_decompressBegin_usingDict().
   Alternatively, you can copy a prepared context, using ZSTDv06_copyDCtx().

   Then use ZSTDv06_nextSrcSizeToDecompress() and ZSTDv06_decompressContinue() alternatively.
   ZSTDv06_nextSrcSizeToDecompress() tells how much bytes to provide as 'srcSize' to ZSTDv06_decompressContinue().
   ZSTDv06_decompressContinue() requires this exact amount of bytes, or it will fail.
   ZSTDv06_decompressContinue() needs previous data blocks during decompression, up to (1 << windowlog).
   They should preferably be located contiguously, prior to current block. Alternatively, a round buffer is also
      possible.

   @result of ZSTDv06_decompressContinue() is the number of bytes regenerated within 'dst' (necessarily <= dstCapacity)
   It can be zero, which is not an error; it just means ZSTDv06_decompressContinue() has decoded some header.

   A frame is fully decoded when ZSTDv06_nextSrcSizeToDecompress() returns zero.
   Context can then be reset to start a new decompression.
 */

/* **************************************
*  Block functions
****************************************/
/*! Block functions produce and decode raw zstd blocks, without frame metadata.
    User will have to take in charge required information to regenerate data, such as compressed and content sizes.

    A few rules to respect :
    - Uncompressed block size must be <= ZSTDv06_BLOCKSIZE_MAX (128 KB)
    - Compressing or decompressing requires a context structure
 + Use ZSTDv06_createCCtx() and ZSTDv06_createDCtx()
    - It is necessary to init context before starting
 + compression : ZSTDv06_compressBegin()
 + decompression : ZSTDv06_decompressBegin()
 + variants _usingDict() are also allowed
 + copyCCtx() and copyDCtx() work too
    - When a block is considered not compressible enough, ZSTDv06_compressBlock() result will be zero.
      In which case, nothing is produced into `dst`.
 + User must test for such outcome and deal directly with uncompressed data
 + ZSTDv06_decompressBlock() doesn't accept uncompressed data as input !!
 */

#define ZSTDv06_BLOCKSIZE_MAX (128 * 1024)   /* define, for static allocation */
ZSTDLIBv06_API size_t ZSTDv06_decompressBlock(ZSTDv06_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize);

#if defined (__cplusplus)
}
#endif

#endif  /* ZSTDv06_STATIC_H */
/*
    zstd_internal - common functions to include
    Header File for include
    Copyright (C) 2014-2016, Yann Collet.

    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
 * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

    You can contact the author at : zstd homepage : https://www.zstd.net
 */
#ifndef ZSTDv06_CCOMMON_H_MODULE
#define ZSTDv06_CCOMMON_H_MODULE

/*-*************************************
*  Common macros
***************************************/
//#define MIN(a, b) ((a)<(b) ? (a) : (b))
//#define MAX(a, b) ((a)>(b) ? (a) : (b))

/*-*************************************
*  Common constants
***************************************/
#define ZSTDv06_DICT_MAGIC  0xEC30A436
#define ZSTDv06_REP_NUM    3
#define ZSTDv06_REP_INIT   ZSTDv06_REP_NUM
#define ZSTDv06_REP_MOVE   (ZSTDv06_REP_NUM-1)
//#define KB_Removed *(1 <<10)
//#define MB_Removed *(1 <<20)
//#define GB_Removed *(1U<<30)
#define BIT7 128
#define BIT6  64
#define BIT5  32
#define BIT4  16
#define BIT1   2
#define BIT0   1

#define ZSTDv06_WINDOWLOG_ABSOLUTEMIN 12
static const size_t ZSTDv06_fcs_fieldSize[4] = { 0, 1, 2, 8 };

#define ZSTDv06_BLOCKHEADERSIZE 3   /* because C standard does not allow a static const value to be defined using
	                               another static const value .... :( */
static const size_t ZSTDv06_blockHeaderSize = ZSTDv06_BLOCKHEADERSIZE;
typedef enum { bt_compressed, bt_raw, bt_rle, bt_end } blockType_t;

#define MIN_SEQUENCES_SIZE 1 /* nbSeq==0 */
#define MIN_CBLOCK_SIZE (1 /*litCSize*/ + 1 /* RLE or RAW */ + MIN_SEQUENCES_SIZE /* nbSeq==0 */)   /* for a non-null
	                                                                                               block */

#define ZSTD_HUFFDTABLE_CAPACITY_LOG 12

#define IS_HUF 0
#define IS_PCH 1
#define IS_RAW 2
#define IS_RLE 3

#define LONGNBSEQ 0x7F00

#define MINMATCH 3
#define EQUAL_READ32 4
#define REPCODE_STARTVALUE 1

#define Litbits  8
#define MaxLit ((1<<Litbits) - 1)
#define MaxML  52
#define MaxLL  35
#define MaxOff 28
#define MaxSeq MAX(MaxLL, MaxML)   /* Assumption : MaxOff < MaxLL,MaxML */
#define MLFSELog    9
#define LLFSELog    9
#define OffFSELog   8

#define FSEv06_ENCODING_RAW     0
#define FSEv06_ENCODING_RLE     1
#define FSEv06_ENCODING_STATIC  2
#define FSEv06_ENCODING_DYNAMIC 3

#define ZSTD_CONTENTSIZE_ERROR   (0ULL - 2)

static const uint32 LL_bits[MaxLL+1] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 3, 3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
static const int16 LL_defaultNorm[MaxLL+1] = { 4, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 1, 1, 1, 1, 1, -1, -1, -1, -1 };
static const uint32 LL_defaultNormLog = 6;

static const uint32 ML_bits[MaxML+1] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				      1, 1, 1, 1, 2, 2, 3, 3, 4, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16 };
static const int16 ML_defaultNorm[MaxML+1] = { 1, 4, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1,
					     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1, -1, -1 };
static const uint32 ML_defaultNormLog = 6;

static const int16 OF_defaultNorm[MaxOff+1] = { 1, 1, 1, 1, 1, 1, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, -1, -1, -1, -1, -1 };
static const uint32 OF_defaultNormLog = 5;

/*-*******************************************
*  Shared functions to include for inlining
*********************************************/
static void ZSTDv06_copy8(void* dst, const void* src) 
{
	memcpy(dst, src, 8);
}

#define COPY8(d, s) { ZSTDv06_copy8(d, s); d += 8; s += 8; }

/*! ZSTDv06_wildcopy() :
 *   custom version of memcpy(), can copy up to 7 bytes too many (8 bytes if length==0) */
#define WILDCOPY_OVERLENGTH 8
MEM_STATIC void ZSTDv06_wildcopy(void* dst, const void* src, ptrdiff_t length)
{
	const BYTE * ip = PTR8C(src);
	BYTE * op = (BYTE *)dst;
	BYTE * const oend = op + length;
	do {
		COPY8(op, ip)
	} while(op < oend);
}

/*-*******************************************
*  Private interfaces
*********************************************/
typedef struct {
	uint32 off;
	uint32 len;
} ZSTDv06_match_t;

typedef struct {
	uint32 price;
	uint32 off;
	uint32 mlen;
	uint32 litlen;
	uint32 rep[ZSTDv06_REP_INIT];
} ZSTDv06_optimal_t;

typedef struct { uint32 unused; } ZSTDv06_stats_t;

typedef struct {
	void* buffer;
	uint32 *  offsetStart;
	uint32 *  offset;
	BYTE * offCodeStart;
	BYTE * litStart;
	BYTE * lit;
	uint16*  litLengthStart;
	uint16*  litLength;
	BYTE * llCodeStart;
	uint16*  matchLengthStart;
	uint16*  matchLength;
	BYTE * mlCodeStart;
	uint32 longLengthID; /* 0 == no longLength; 1 == Lit.longLength; 2 == Match.longLength; */
	uint32 longLengthPos;
	/* opt */
	ZSTDv06_optimal_t* priceTable;
	ZSTDv06_match_t* matchTable;
	uint32 * matchLengthFreq;
	uint32 * litLengthFreq;
	uint32 * litFreq;
	uint32 * offCodeFreq;
	uint32 matchLengthSum;
	uint32 matchSum;
	uint32 litLengthSum;
	uint32 litSum;
	uint32 offCodeSum;
	uint32 log2matchLengthSum;
	uint32 log2matchSum;
	uint32 log2litLengthSum;
	uint32 log2litSum;
	uint32 log2offCodeSum;
	uint32 factor;
	uint32 cachedPrice;
	uint32 cachedLitLength;
	const BYTE * cachedLiterals;
	ZSTDv06_stats_t stats;
} seqStore_t;

void ZSTDv06_seqToCodes(const seqStore_t* seqStorePtr, const size_t nbSeq);

#endif   /* ZSTDv06_CCOMMON_H_MODULE */
/* ******************************************************************
   FSE : Finite State Entropy codec
   Public Prototypes declaration
   Copyright (C) 2013-2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

* Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   You can contact the author at :
   - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
****************************************************************** */
#ifndef FSEv06_H
#define FSEv06_H

#if defined (__cplusplus)
extern "C" {
#endif

/*-****************************************
*  FSE simple functions
******************************************/
/*! FSEv06_decompress():
    Decompress FSE data from buffer 'cSrc', of size 'cSrcSize',
    into already allocated destination buffer 'dst', of size 'dstCapacity'.
    @return : size of regenerated data (<= maxDstSize),
              or an error code, which can be tested using FSEv06_isError() .

 ** Important ** : FSEv06_decompress() does not decompress non-compressible nor RLE data !!!
    Why ? : making this distinction requires a header.
    Header management is intentionally delegated to the user layer, which can better manage special cases.
 */
size_t FSEv06_decompress(void* dst,  size_t dstCapacity, const void* cSrc, size_t cSrcSize);
// 
// Tool functions
//
size_t FSEv06_compressBound(size_t size);       /* maximum compressed size */
//
// Error Management
//
uint FSEv06_isError(size_t code);        /* tells if a return value is an error code */
const char* FSEv06_getErrorName(size_t code);   /* provides error code string (useful for debugging) */
/*-*****************************************
 *  FSE detailed API
 ******************************************/
/*!

   FSEv06_decompress() does the following:
   1. read normalized counters with readNCount()
   2. build decoding table 'DTable' from normalized counters
   3. decode the data stream using decoding table 'DTable'

   The following API allows targeting specific sub-functions for advanced tasks.
   For example, it's possible to compress several blocks using the same 'CTable',
   or to save and provide normalized distribution using external method.
 */

/* *** DECOMPRESSION *** */

/*! FSEv06_readNCount():
    Read compactly saved 'normalizedCounter' from 'rBuffer'.
    @return : size read from 'rBuffer',
              or an errorCode, which can be tested using FSEv06_isError().
              maxSymbolValuePtr[0] and tableLogPtr[0] will also be updated with their respective values */
size_t FSEv06_readNCount(short* normalizedCounter, uint * maxSymbolValuePtr, uint32 * tableLogPtr, const void* rBuffer, size_t rBuffSize);

/*! Constructor and Destructor of FSEv06_DTable.
    Note that its size depends on 'tableLog' */
typedef uint32 FSEv06_DTable;   /* don't allocate that. It's just a way to be more restrictive than void* */
FSEv06_DTable * FSEv06_createDTable(uint tableLog);
void        FSEv06_freeDTable(FSEv06_DTable* dt);

/*! FSEv06_buildDTable():
    Builds 'dt', which must be already allocated, using FSEv06_createDTable().
    return : 0, or an errorCode, which can be tested using FSEv06_isError() */
size_t FSEv06_buildDTable(FSEv06_DTable* dt, const short* normalizedCounter, uint maxSymbolValue, uint tableLog);

/*! FSEv06_decompress_usingDTable():
    Decompress compressed source `cSrc` of size `cSrcSize` using `dt`
    into `dst` which must be already allocated.
    @return : size of regenerated data (necessarily <= `dstCapacity`),
              or an errorCode, which can be tested using FSEv06_isError() */
size_t FSEv06_decompress_usingDTable(void* dst, size_t dstCapacity, const void* cSrc, size_t cSrcSize, const FSEv06_DTable* dt);

/*!
   Tutorial :
   ----------
   (Note : these functions only decompress FSE-compressed blocks.
   If block is uncompressed, use memcpy() instead
   If block is a single repeated byte, use memset() instead )

   The first step is to obtain the normalized frequencies of symbols.
   This can be performed by FSEv06_readNCount() if it was saved using FSEv06_writeNCount().
   'normalizedCounter' must be already allocated, and have at least 'maxSymbolValuePtr[0]+1' cells of signed short.
   In practice, that means it's necessary to know 'maxSymbolValue' beforehand,
   or size the table to handle worst case situations (typically 256).
   FSEv06_readNCount() will provide 'tableLog' and 'maxSymbolValue'.
   The result of FSEv06_readNCount() is the number of bytes read from 'rBuffer'.
   Note that 'rBufferSize' must be at least 4 bytes, even if useful information is less than that.
   If there is an error, the function will return an error code, which can be tested using FSEv06_isError().

   The next step is to build the decompression tables 'FSEv06_DTable' from 'normalizedCounter'.
   This is performed by the function FSEv06_buildDTable().
   The space required by 'FSEv06_DTable' must be already allocated using FSEv06_createDTable().
   If there is an error, the function will return an error code, which can be tested using FSEv06_isError().

   `FSEv06_DTable` can then be used to decompress `cSrc`, with FSEv06_decompress_usingDTable().
   `cSrcSize` must be strictly correct, otherwise decompression will fail.
   FSEv06_decompress_usingDTable() result will tell how many bytes were regenerated (<=`dstCapacity`).
   If there is an error, the function will return an error code, which can be tested using FSEv06_isError(). (ex: dst
      buffer too small)
 */

#if defined (__cplusplus)
}
#endif

#endif  /* FSEv06_H */
/* ******************************************************************
   bitstream
   Part of FSE library
   header file (to include)
   Copyright (C) 2013-2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

* Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   You can contact the author at :
   - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
****************************************************************** */
#ifndef BITSTREAM_H_MODULE
#define BITSTREAM_H_MODULE

#if defined (__cplusplus)
extern "C" {
#endif

/*
 *  This API consists of small unitary functions, which must be inlined for best performance.
 *  Since link-time-optimization is not available for all compilers,
 *  these functions are defined into a .h to be included.
 */

/*=========================================
*  Target specific
   =========================================*/
#if defined(__BMI__) && defined(__GNUC__)
#include <immintrin.h>   /* support for bextr (experimental) */
#endif

/*-********************************************
*  bitStream decoding API (read backward)
**********************************************/
typedef struct {
	size_t bitContainer;
	uint   bitsConsumed;
	const char * ptr;
	const char * start;
} BITv06_DStream_t;

typedef enum { BITv06_DStream_unfinished = 0,
	       BITv06_DStream_endOfBuffer = 1,
	       BITv06_DStream_completed = 2,
	       BITv06_DStream_overflow = 3 } BITv06_DStream_status;  /* result of BITv06_reloadDStream() */
/* 1,2,4,8 would be better for bitmap combinations, but slows down performance a bit ... :( */

MEM_STATIC size_t   BITv06_initDStream(BITv06_DStream_t* bitD, const void* srcBuffer, size_t srcSize);
MEM_STATIC size_t   BITv06_readBits(BITv06_DStream_t* bitD, uint nbBits);
MEM_STATIC BITv06_DStream_status BITv06_reloadDStream(BITv06_DStream_t* bitD);
MEM_STATIC uint BITv06_endOfDStream(const BITv06_DStream_t* bitD);
// 
// unsafe API
// 
MEM_STATIC size_t BITv06_readBitsFast(BITv06_DStream_t* bitD, uint nbBits);
/* faster, but works only if nbBits >= 1 */
// 
// Internal functions
// 
MEM_STATIC uint BITv06_highbit32(uint32 val)
{
#if defined(_MSC_VER)   /* Visual */
	ulong r;
	return _BitScanReverse(&r, val) ? (uint)r : 0;
#elif defined(__GNUC__) && (__GNUC__ >= 3)   /* Use GCC Intrinsic */
	return __builtin_clz(val) ^ 31;
#else   /* Software version */
	static const uint DeBruijnClz[32] =
	{ 0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30, 8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31 };
	uint32 v = val;
	uint r;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	r = DeBruijnClz[ (uint32)(v * 0x07C4ACDDU) >> 27];
	return r;
#endif
}

/*-********************************************************
* bitStream decoding
**********************************************************/
/*! BITv06_initDStream() :
 *   Initialize a BITv06_DStream_t.
 *   `bitD` : a pointer to an already allocated BITv06_DStream_t structure.
 *   `srcSize` must be the *exact* size of the bitStream, in bytes.
 *   @return : size of stream (== srcSize) or an errorCode if a problem is detected
 */
MEM_STATIC size_t BITv06_initDStream(BITv06_DStream_t* bitD, const void* srcBuffer, size_t srcSize)
{
	if(srcSize < 1) {
		memzero(bitD, sizeof(*bitD)); return ERROR(srcSize_wrong);
	}
	if(srcSize >=  sizeof(bitD->bitContainer)) {/* normal case */
		bitD->start = (const char *)srcBuffer;
		bitD->ptr   = (const char *)srcBuffer + srcSize - sizeof(bitD->bitContainer);
		bitD->bitContainer = SMem::GetLeSizeT(bitD->ptr);
		{ 
			BYTE const lastByte = ((const BYTE *)srcBuffer)[srcSize-1];
			if(lastByte == 0) 
				return ERROR(GENERIC); /* endMark not present */
			bitD->bitsConsumed = 8 - BITv06_highbit32(lastByte); 
		}
	}
	else {
		bitD->start = (const char *)srcBuffer;
		bitD->ptr   = bitD->start;
		bitD->bitContainer = *(const BYTE *)(bitD->start);
		switch(srcSize) {
			case 7: bitD->bitContainer += (size_t)(PTR8C(srcBuffer)[6]) << (sizeof(bitD->bitContainer)*8 - 16); // @fallthrough
			case 6: bitD->bitContainer += (size_t)(PTR8C(srcBuffer)[5]) << (sizeof(bitD->bitContainer)*8 - 24); // @fallthrough
			case 5: bitD->bitContainer += (size_t)(PTR8C(srcBuffer)[4]) << (sizeof(bitD->bitContainer)*8 - 32); // @fallthrough
			case 4: bitD->bitContainer += (size_t)(PTR8C(srcBuffer)[3]) << 24; // @fallthrough
			case 3: bitD->bitContainer += (size_t)(PTR8C(srcBuffer)[2]) << 16; // @fallthrough
			case 2: bitD->bitContainer += (size_t)(PTR8C(srcBuffer)[1]) <<  8; // @fallthrough
			default: break;
		}
		{ 
			BYTE const lastByte = ((const BYTE *)srcBuffer)[srcSize-1];
			if(lastByte == 0) return ERROR(GENERIC); /* endMark not present */
			bitD->bitsConsumed = 8 - BITv06_highbit32(lastByte); 
		}
		bitD->bitsConsumed += (uint32)(sizeof(bitD->bitContainer) - srcSize)*8;
	}
	return srcSize;
}

MEM_STATIC size_t BITv06_lookBits(const BITv06_DStream_t* bitD, uint32 nbBits)
{
	const uint32 bitMask = sizeof(bitD->bitContainer)*8 - 1;
	return ((bitD->bitContainer << (bitD->bitsConsumed & bitMask)) >> 1) >> ((bitMask-nbBits) & bitMask);
}

/*! BITv06_lookBitsFast() :
 *   unsafe version; only works if nbBits >= 1 */
MEM_STATIC size_t BITv06_lookBitsFast(const BITv06_DStream_t* bitD, uint nbBits)
{
	const uint32 bitMask = sizeof(bitD->bitContainer)*8 - 1;
	return (bitD->bitContainer << (bitD->bitsConsumed & bitMask)) >> (((bitMask+1)-nbBits) & bitMask);
}

MEM_STATIC void BITv06_skipBits(BITv06_DStream_t* bitD, uint nbBits)
{
	bitD->bitsConsumed += nbBits;
}

MEM_STATIC size_t BITv06_readBits(BITv06_DStream_t* bitD, uint nbBits)
{
	const size_t value = BITv06_lookBits(bitD, nbBits);
	BITv06_skipBits(bitD, nbBits);
	return value;
}

/*! BITv06_readBitsFast() :
 *   unsafe version; only works if nbBits >= 1 */
MEM_STATIC size_t BITv06_readBitsFast(BITv06_DStream_t* bitD, uint nbBits)
{
	const size_t value = BITv06_lookBitsFast(bitD, nbBits);
	BITv06_skipBits(bitD, nbBits);
	return value;
}

MEM_STATIC BITv06_DStream_status BITv06_reloadDStream(BITv06_DStream_t* bitD)
{
	if(bitD->bitsConsumed > (sizeof(bitD->bitContainer)*8)) /* should never happen */
		return BITv06_DStream_overflow;

	if(bitD->ptr >= bitD->start + sizeof(bitD->bitContainer)) {
		bitD->ptr -= bitD->bitsConsumed >> 3;
		bitD->bitsConsumed &= 7;
		bitD->bitContainer = SMem::GetLeSizeT(bitD->ptr);
		return BITv06_DStream_unfinished;
	}
	if(bitD->ptr == bitD->start) {
		if(bitD->bitsConsumed < sizeof(bitD->bitContainer)*8) return BITv06_DStream_endOfBuffer;
		return BITv06_DStream_completed;
	}
	{   uint32 nbBytes = bitD->bitsConsumed >> 3;
	    BITv06_DStream_status result = BITv06_DStream_unfinished;
	    if(bitD->ptr - nbBytes < bitD->start) {
		    nbBytes = (uint32)(bitD->ptr - bitD->start); /* ptr > start */
		    result = BITv06_DStream_endOfBuffer;
	    }
	    bitD->ptr -= nbBytes;
	    bitD->bitsConsumed -= nbBytes*8;
	    bitD->bitContainer = SMem::GetLeSizeT(bitD->ptr); /* reminder : srcSize > sizeof(bitD) */
	    return result;}
}

/*! BITv06_endOfDStream() :
 *   @return Tells if DStream has exactly reached its end (all bits consumed).
 */
MEM_STATIC uint BITv06_endOfDStream(const BITv06_DStream_t* DStream)
{
	return ((DStream->ptr == DStream->start) && (DStream->bitsConsumed == sizeof(DStream->bitContainer)*8));
}

#if defined (__cplusplus)
}
#endif

#endif /* BITSTREAM_H_MODULE */
/* ******************************************************************
   FSE : Finite State Entropy coder
   header file for static linking (only)
   Copyright (C) 2013-2015, Yann Collet

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

* Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   You can contact the author at :
   - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
   - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */
#ifndef FSEv06_STATIC_H
#define FSEv06_STATIC_H

#if defined (__cplusplus)
extern "C" {
#endif

/* *****************************************
*  Static allocation
*******************************************/
/* FSE buffer bounds */
#define FSEv06_NCOUNTBOUND 512
#define FSEv06_BLOCKBOUND(size) (size + (size>>7))
#define FSEv06_COMPRESSBOUND(size) (FSEv06_NCOUNTBOUND + FSEv06_BLOCKBOUND(size))   /* Macro version, useful for static
	                                                                               allocation */

/* It is possible to statically allocate FSE CTable/DTable as a table of unsigned using below macros */
#define FSEv06_DTABLE_SIZE_U32(maxTableLog)                   (1 + (1<<maxTableLog))

/* *****************************************
*  FSE advanced API
*******************************************/
size_t FSEv06_countFast(uint * count, uint * maxSymbolValuePtr, const void* src, size_t srcSize);
/* same as FSEv06_count(), but blindly trusts that all byte values within src are <= *maxSymbolValuePtr  */

size_t FSEv06_buildDTable_raw(FSEv06_DTable* dt, uint nbBits);
/* build a fake FSEv06_DTable, designed to read an uncompressed bitstream where each symbol uses nbBits */

size_t FSEv06_buildDTable_rle(FSEv06_DTable* dt, uchar symbolValue);
/* build a fake FSEv06_DTable, designed to always generate the same symbolValue */

/* *****************************************
*  FSE symbol decompression API
*******************************************/
typedef struct {
	size_t state;
	const void* table; /* precise table may vary, depending on uint16 */
} FSEv06_DState_t;

static void  FSEv06_initDState(FSEv06_DState_t* DStatePtr, BITv06_DStream_t* bitD, const FSEv06_DTable* dt);
static uchar FSEv06_decodeSymbol(FSEv06_DState_t* DStatePtr, BITv06_DStream_t* bitD);
//
// FSE unsafe API
//
static uchar FSEv06_decodeSymbolFast(FSEv06_DState_t* DStatePtr, BITv06_DStream_t* bitD);
/* faster, but works only if nbBits is always >= 1 (otherwise, result will be corrupted) */
//
// Implementation of inlined functions
//
/* ======    Decompression    ====== */

typedef struct {
	uint16 tableLog;
	uint16 fastMode;
} FSEv06_DTableHeader;   /* sizeof uint32 */

typedef struct {
	ushort newState;
	uchar symbol;
	uchar nbBits;
} FSEv06_decode_t;   /* size == uint32 */

MEM_STATIC void FSEv06_initDState(FSEv06_DState_t* DStatePtr, BITv06_DStream_t* bitD, const FSEv06_DTable* dt)
{
	const void* ptr = dt;
	const FSEv06_DTableHeader* const DTableH = (const FSEv06_DTableHeader*)ptr;
	DStatePtr->state = BITv06_readBits(bitD, DTableH->tableLog);
	BITv06_reloadDStream(bitD);
	DStatePtr->table = dt + 1;
}

MEM_STATIC BYTE FSEv06_peekSymbol(const FSEv06_DState_t* DStatePtr)
{
	FSEv06_decode_t const DInfo = ((const FSEv06_decode_t*)(DStatePtr->table))[DStatePtr->state];
	return DInfo.symbol;
}

MEM_STATIC void FSEv06_updateState(FSEv06_DState_t* DStatePtr, BITv06_DStream_t* bitD)
{
	FSEv06_decode_t const DInfo = ((const FSEv06_decode_t*)(DStatePtr->table))[DStatePtr->state];
	const uint32 nbBits = DInfo.nbBits;
	const size_t lowBits = BITv06_readBits(bitD, nbBits);
	DStatePtr->state = DInfo.newState + lowBits;
}

MEM_STATIC BYTE FSEv06_decodeSymbol(FSEv06_DState_t* DStatePtr, BITv06_DStream_t* bitD)
{
	FSEv06_decode_t const DInfo = ((const FSEv06_decode_t*)(DStatePtr->table))[DStatePtr->state];
	const uint32 nbBits = DInfo.nbBits;
	BYTE const symbol = DInfo.symbol;
	const size_t lowBits = BITv06_readBits(bitD, nbBits);

	DStatePtr->state = DInfo.newState + lowBits;
	return symbol;
}

/*! FSEv06_decodeSymbolFast() :
    unsafe, only works if no symbol has a probability > 50% */
MEM_STATIC BYTE FSEv06_decodeSymbolFast(FSEv06_DState_t* DStatePtr, BITv06_DStream_t* bitD)
{
	FSEv06_decode_t const DInfo = ((const FSEv06_decode_t*)(DStatePtr->table))[DStatePtr->state];
	uint const nbBits = DInfo.nbBits;
	BYTE const symbol = DInfo.symbol;
	const size_t lowBits = BITv06_readBitsFast(bitD, nbBits);
	DStatePtr->state = DInfo.newState + lowBits;
	return symbol;
}

#ifndef FSEv06_COMMONDEFS_ONLY
//
// Tuning parameters
//
/*!MEMORY_USAGE :
 *  Memory usage formula : N->2^N Bytes (examples : 10 -> 1KB; 12 -> 4KB ; 16 -> 64KB; 20 -> 1MB; etc.)
 *  Increasing memory usage improves compression ratio
 *  Reduced memory usage can improve speed, due to cache effect
 *  Recommended max value is 14, for 16KB, which nicely fits into Intel x86 L1 cache */
#define FSEv06_MAX_MEMORY_USAGE 14
#define FSEv06_DEFAULT_MEMORY_USAGE 13

/*!FSEv06_MAX_SYMBOL_VALUE :
 *  Maximum symbol value authorized.
 *  Required for proper stack allocation */
#define FSEv06_MAX_SYMBOL_VALUE 255
//
// template functions type & suffix
//
#define FSEv06_FUNCTION_TYPE BYTE
#define FSEv06_FUNCTION_EXTENSION
#define FSEv06_DECODE_TYPE FSEv06_decode_t

#endif   /* !FSEv06_COMMONDEFS_ONLY */
// 
// Constants
// 
#define FSEv06_MAX_TABLELOG  (FSEv06_MAX_MEMORY_USAGE-2)
#define FSEv06_MAX_TABLESIZE (1U<<FSEv06_MAX_TABLELOG)
#define FSEv06_MAXTABLESIZE_MASK (FSEv06_MAX_TABLESIZE-1)
#define FSEv06_DEFAULT_TABLELOG (FSEv06_DEFAULT_MEMORY_USAGE-2)
#define FSEv06_MIN_TABLELOG 5

#define FSEv06_TABLELOG_ABSOLUTE_MAX 15
#if FSEv06_MAX_TABLELOG > FSEv06_TABLELOG_ABSOLUTE_MAX
#error "FSEv06_MAX_TABLELOG > FSEv06_TABLELOG_ABSOLUTE_MAX is not supported"
#endif

#define FSEv06_TABLESTEP(tableSize) ((tableSize>>1) + (tableSize>>3) + 3)

#if defined (__cplusplus)
}
#endif

#endif  /* FSEv06_STATIC_H */
/*
   Common functions of New Generation Entropy library
   Copyright (C) 2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

 * Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

    You can contact the author at :
    - FSE+HUF source repository : https://github.com/Cyan4973/FiniteStateEntropy
    - Public forum : https://groups.google.com/forum/#!forum/lz4c
 *************************************************************************** */
//
// Error Management
//
uint FSEv06_isError(size_t code) { return ERR_isError(code); }
const char * FSEv06_getErrorName(size_t code) { return ERR_getErrorName(code); }
//
// HUF Error Management
//
static uint HUFv06_isError(size_t code) { return ERR_isError(code); }
// 
// FSE NCount encoding-decoding
// 
static short FSEv06_abs(short a) { return a<0 ? -a : a; }

size_t FSEv06_readNCount(short* normalizedCounter, uint * maxSVPtr, uint32 * tableLogPtr, const void * headerBuffer, size_t hbSize)
{
	const BYTE * const istart = (const BYTE *)headerBuffer;
	const BYTE * const iend = istart + hbSize;
	const BYTE * ip = istart;
	int nbBits;
	int remaining;
	int threshold;
	uint32 bitStream;
	int bitCount;
	uint charnum = 0;
	int previous0 = 0;
	if(hbSize < 4) 
		return ERROR(srcSize_wrong);
	bitStream = SMem::GetLe32(ip);
	nbBits = (bitStream & 0xF) + FSEv06_MIN_TABLELOG; /* extract tableLog */
	if(nbBits > FSEv06_TABLELOG_ABSOLUTE_MAX) return ERROR(tableLog_tooLarge);
	bitStream >>= 4;
	bitCount = 4;
	*tableLogPtr = nbBits;
	remaining = (1<<nbBits)+1;
	threshold = 1<<nbBits;
	nbBits++;

	while((remaining>1) && (charnum<=*maxSVPtr)) {
		if(previous0) {
			uint n0 = charnum;
			while((bitStream & 0xFFFF) == 0xFFFF) {
				n0 += 24;
				if(ip < iend-5) {
					ip += 2;
					bitStream = SMem::GetLe32(ip) >> bitCount;
				}
				else {
					bitStream >>= 16;
					bitCount += 16;
				}
			}
			while((bitStream & 3) == 3) {
				n0 += 3;
				bitStream >>= 2;
				bitCount += 2;
			}
			n0 += bitStream & 3;
			bitCount += 2;
			if(n0 > *maxSVPtr) return ERROR(maxSymbolValue_tooSmall);
			while(charnum < n0) normalizedCounter[charnum++] = 0;
			if((ip <= iend-7) || (ip + (bitCount>>3) <= iend-4)) {
				ip += bitCount>>3;
				bitCount &= 7;
				bitStream = SMem::GetLe32(ip) >> bitCount;
			}
			else
				bitStream >>= 2;
		}
		{   
			short const max = (short)((2*threshold-1)-remaining);
		    short count;
		    if((bitStream & (threshold-1)) < (uint32)max) {
			    count = (short)(bitStream & (threshold-1));
			    bitCount   += nbBits-1;
		    }
		    else {
			    count = (short)(bitStream & (2*threshold-1));
			    if(count >= threshold) 
					count -= max;
			    bitCount   += nbBits;
		    }
		    count--; /* extra accuracy */
		    remaining -= FSEv06_abs(count);
		    normalizedCounter[charnum++] = count;
		    previous0 = !count;
		    while(remaining < threshold) {
			    nbBits--;
			    threshold >>= 1;
		    }
		    if((ip <= iend-7) || (ip + (bitCount>>3) <= iend-4)) {
			    ip += bitCount>>3;
			    bitCount &= 7;
		    }
		    else {
			    bitCount -= (int)(8 * (iend - 4 - ip));
			    ip = iend - 4;
		    }
		    bitStream = SMem::GetLe32(ip) >> (bitCount & 31);}
	}   /* while ((remaining>1) && (charnum<=*maxSVPtr)) */
	if(remaining != 1) 
		return ERROR(GENERIC);
	*maxSVPtr = charnum-1;
	ip += (bitCount+7)>>3;
	if((size_t)(ip-istart) > hbSize) 
		return ERROR(srcSize_wrong);
	return ip-istart;
}

/* ******************************************************************
   FSE : Finite State Entropy decoder
   Copyright (C) 2013-2015, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

* Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

    You can contact the author at :
    - FSE source repository : https://github.com/Cyan4973/FiniteStateEntropy
    - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */
//
// Compiler specifics
//
#ifdef _MSC_VER    /* Visual Studio */
	//#define FORCE_INLINE static __forceinline
	#pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
	#pragma warning(disable : 4214)        /* disable: C4214: non-int bitfields */
//#else
	//#if defined (__cplusplus) || defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L   /* C99 */
		//#ifdef __GNUC__
			//#define FORCE_INLINE static inline __attribute__((always_inline))
		//#else
			//#define FORCE_INLINE static inline
		//#endif
	//#else
		//#define FORCE_INLINE static
	//#endif /* __STDC_VERSION__ */
#endif
//
// Error Management
//
#define FSEv06_isError ERR_isError
#define FSEv06_STATIC_ASSERT(c) { enum { FSEv06_static_assert = 1/(int)(!!(c)) }; }   /* use only *after* variable declarations */
//
// Complex types
//
typedef uint32 DTable_max_t[FSEv06_DTABLE_SIZE_U32(FSEv06_MAX_TABLELOG)];
//
// Templates
//
/*
   designed to be included
   for type-specific functions (template emulation in C)
   Objective is to write these functions only once, for improved maintenance
 */
/* safety checks */
#ifndef FSEv06_FUNCTION_EXTENSION
	#error "FSEv06_FUNCTION_EXTENSION must be defined"
#endif
#ifndef FSEv06_FUNCTION_TYPE
	#error "FSEv06_FUNCTION_TYPE must be defined"
#endif
/* Function names */
#define FSEv06_CAT(X, Y) X ## Y
#define FSEv06_FUNCTION_NAME(X, Y) FSEv06_CAT(X, Y)
#define FSEv06_TYPE_NAME(X, Y) FSEv06_CAT(X, Y)

/* Function templates */
FSEv06_DTable* FSEv06_createDTable(uint tableLog)
{
	SETMIN(tableLog, FSEv06_TABLELOG_ABSOLUTE_MAX);
	return (FSEv06_DTable*)SAlloc::M(FSEv06_DTABLE_SIZE_U32(tableLog) * sizeof(uint32) );
}

void FSEv06_freeDTable(FSEv06_DTable* dt) { SAlloc::F(dt); }

size_t FSEv06_buildDTable(FSEv06_DTable* dt, const short* normalizedCounter, uint maxSymbolValue, uint tableLog)
{
	void* const tdPtr = dt+1; /* because *dt is unsigned, 32-bits aligned on 32-bits */
	FSEv06_DECODE_TYPE* const tableDecode = (FSEv06_DECODE_TYPE*)(tdPtr);
	uint16 symbolNext[FSEv06_MAX_SYMBOL_VALUE+1];
	const uint32 maxSV1 = maxSymbolValue + 1;
	const uint32 tableSize = 1 << tableLog;
	uint32 highThreshold = tableSize-1;
	/* Sanity Checks */
	if(maxSymbolValue > FSEv06_MAX_SYMBOL_VALUE) return ERROR(maxSymbolValue_tooLarge);
	if(tableLog > FSEv06_MAX_TABLELOG) return ERROR(tableLog_tooLarge);
	/* Init, lay down lowprob symbols */
	{   
		FSEv06_DTableHeader DTableH;
	    DTableH.tableLog = (uint16)tableLog;
	    DTableH.fastMode = 1;
	    {   
		int16 const largeLimit = (int16)(1 << (tableLog-1));
		uint32 s;
		for(s = 0; s<maxSV1; s++) {
			if(normalizedCounter[s] == -1) {
				tableDecode[highThreshold--].symbol = (FSEv06_FUNCTION_TYPE)s;
				symbolNext[s] = 1;
			}
			else {
				if(normalizedCounter[s] >= largeLimit) DTableH.fastMode = 0;
				symbolNext[s] = normalizedCounter[s];
			}
		}
	    }
	    memcpy(dt, &DTableH, sizeof(DTableH));
	}
	/* Spread symbols */
	{   
		const uint32 tableMask = tableSize-1;
	    const uint32 step = FSEv06_TABLESTEP(tableSize);
	    uint32 position = 0;
	    for(uint32 s = 0; s<maxSV1; s++) {
		    for(int i = 0; i<normalizedCounter[s]; i++) {
			    tableDecode[position].symbol = (FSEv06_FUNCTION_TYPE)s;
			    position = (position + step) & tableMask;
			    while(position > highThreshold) 
					position = (position + step) & tableMask; /* lowprob area */
		    }
	    }
	    if(position!=0) 
			return ERROR(GENERIC); /* position must reach all cells once, otherwise normalizedCounter is incorrect */
	}
	/* Build Decoding table */
	{   uint32 u;
	    for(u = 0; u<tableSize; u++) {
		    FSEv06_FUNCTION_TYPE const symbol = (FSEv06_FUNCTION_TYPE)(tableDecode[u].symbol);
		    uint16 nextState = symbolNext[symbol]++;
		    tableDecode[u].nbBits = (BYTE)(tableLog - BITv06_highbit32((uint32)nextState) );
		    tableDecode[u].newState = (uint16)( (nextState << tableDecode[u].nbBits) - tableSize);
	    }
	}

	return 0;
}

#ifndef FSEv06_COMMONDEFS_ONLY

/*-*******************************************************
*  Decompression (Byte symbols)
*********************************************************/
size_t FSEv06_buildDTable_rle(FSEv06_DTable* dt, BYTE symbolValue)
{
	void* ptr = dt;
	FSEv06_DTableHeader* const DTableH = (FSEv06_DTableHeader*)ptr;
	void* dPtr = dt + 1;
	FSEv06_decode_t* const cell = (FSEv06_decode_t*)dPtr;

	DTableH->tableLog = 0;
	DTableH->fastMode = 0;

	cell->newState = 0;
	cell->symbol = symbolValue;
	cell->nbBits = 0;

	return 0;
}

size_t FSEv06_buildDTable_raw(FSEv06_DTable* dt, uint nbBits)
{
	void* ptr = dt;
	FSEv06_DTableHeader* const DTableH = (FSEv06_DTableHeader*)ptr;
	void* dPtr = dt + 1;
	FSEv06_decode_t* const dinfo = (FSEv06_decode_t*)dPtr;
	const uint tableSize = 1 << nbBits;
	const uint tableMask = tableSize - 1;
	const uint maxSV1 = tableMask+1;
	uint s;
	/* Sanity checks */
	if(nbBits < 1) return ERROR(GENERIC);      /* min size */
	/* Build Decoding Table */
	DTableH->tableLog = (uint16)nbBits;
	DTableH->fastMode = 1;
	for(s = 0; s<maxSV1; s++) {
		dinfo[s].newState = 0;
		dinfo[s].symbol = (BYTE)s;
		dinfo[s].nbBits = (BYTE)nbBits;
	}

	return 0;
}

static FORCEINLINE size_t FSEv06_decompress_usingDTable_generic(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize, const FSEv06_DTable* dt, const uint fast)
{
	BYTE * const ostart = (BYTE *)dst;
	BYTE * op = ostart;
	BYTE * const omax = op + maxDstSize;
	BYTE * const olimit = omax-3;
	BITv06_DStream_t bitD;
	FSEv06_DState_t state1;
	FSEv06_DState_t state2;
	/* Init */
	{ 
		const size_t errorCode = BITv06_initDStream(&bitD, cSrc, cSrcSize); /* replaced last arg by maxCompressed Size */
	  if(FSEv06_isError(errorCode)) return errorCode; }

	FSEv06_initDState(&state1, &bitD, dt);
	FSEv06_initDState(&state2, &bitD, dt);

#define FSEv06_GETSYMBOL(statePtr) fast ? FSEv06_decodeSymbolFast(statePtr, &bitD) : FSEv06_decodeSymbol(statePtr, &bitD)

	/* 4 symbols per loop */
	for(; (BITv06_reloadDStream(&bitD)==BITv06_DStream_unfinished) && (op<olimit); op += 4) {
		op[0] = FSEv06_GETSYMBOL(&state1);

		if(FSEv06_MAX_TABLELOG*2+7 > sizeof(bitD.bitContainer)*8) /* This test must be static */
			BITv06_reloadDStream(&bitD);

		op[1] = FSEv06_GETSYMBOL(&state2);

		if(FSEv06_MAX_TABLELOG*4+7 > sizeof(bitD.bitContainer)*8) { /* This test must be static */
			if(BITv06_reloadDStream(&bitD) > BITv06_DStream_unfinished) {
				op += 2; break;
			}
		}

		op[2] = FSEv06_GETSYMBOL(&state1);

		if(FSEv06_MAX_TABLELOG*2+7 > sizeof(bitD.bitContainer)*8) /* This test must be static */
			BITv06_reloadDStream(&bitD);

		op[3] = FSEv06_GETSYMBOL(&state2);
	}

	/* tail */
	/* note : BITv06_reloadDStream(&bitD) >= FSEv06_DStream_partiallyFilled; Ends at exactly
	   BITv06_DStream_completed */
	while(1) {
		if(op>(omax-2)) return ERROR(dstSize_tooSmall);

		*op++ = FSEv06_GETSYMBOL(&state1);

		if(BITv06_reloadDStream(&bitD)==BITv06_DStream_overflow) {
			*op++ = FSEv06_GETSYMBOL(&state2);
			break;
		}

		if(op>(omax-2)) return ERROR(dstSize_tooSmall);

		*op++ = FSEv06_GETSYMBOL(&state2);

		if(BITv06_reloadDStream(&bitD)==BITv06_DStream_overflow) {
			*op++ = FSEv06_GETSYMBOL(&state1);
			break;
		}
	}

	return op-ostart;
}

size_t FSEv06_decompress_usingDTable(void* dst, size_t originalSize, const void* cSrc, size_t cSrcSize, const FSEv06_DTable * dt)
{
	const void* ptr = dt;
	const FSEv06_DTableHeader* DTableH = (const FSEv06_DTableHeader*)ptr;
	const uint32 fastMode = DTableH->fastMode;

	/* select fast mode (static) */
	if(fastMode) return FSEv06_decompress_usingDTable_generic(dst, originalSize, cSrc, cSrcSize, dt, 1);
	return FSEv06_decompress_usingDTable_generic(dst, originalSize, cSrc, cSrcSize, dt, 0);
}

size_t FSEv06_decompress(void* dst, size_t maxDstSize, const void* cSrc, size_t cSrcSize)
{
	const BYTE * const istart = PTR8C(cSrc);
	const BYTE * ip = istart;
	short counting[FSEv06_MAX_SYMBOL_VALUE+1];
	DTable_max_t dt; /* Static analyzer seems unable to understand this table will be properly initialized later */
	uint32 tableLog;
	uint maxSymbolValue = FSEv06_MAX_SYMBOL_VALUE;
	if(cSrcSize<2) 
		return ERROR(srcSize_wrong); /* too small input size */
	/* normal FSE decoding mode */
	{   
		const size_t NCountLength = FSEv06_readNCount(counting, &maxSymbolValue, &tableLog, istart, cSrcSize);
	    if(FSEv06_isError(NCountLength)) return NCountLength;
	    if(NCountLength >= cSrcSize) return ERROR(srcSize_wrong); /* too small input size */
	    ip += NCountLength;
	    cSrcSize -= NCountLength;
	}
	{ 
		const size_t errorCode = FSEv06_buildDTable(dt, counting, maxSymbolValue, tableLog);
		if(FSEv06_isError(errorCode)) 
			return errorCode; 
	}
	return FSEv06_decompress_usingDTable(dst, maxDstSize, ip, cSrcSize, dt); /* always return, even if it is an error code */
}

#endif   /* FSEv06_COMMONDEFS_ONLY */
/* ******************************************************************
   Huffman coder, part of New Generation Entropy library
   header file
   Copyright (C) 2013-2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

* Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   You can contact the author at : Source repository : https://github.com/Cyan4973/FiniteStateEntropy
****************************************************************** */
#ifndef HUFv06_H
#define HUFv06_H

#if defined (__cplusplus)
extern "C" {
#endif

/* ****************************************
*  HUF simple functions
******************************************/
size_t HUFv06_decompress(void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize);
/*
   HUFv06_decompress() :
    Decompress HUF data from buffer 'cSrc', of size 'cSrcSize',
    into already allocated destination buffer 'dst', of size 'dstSize'.
    `dstSize` : must be the **exact** size of original (uncompressed) data.
    Note : in contrast with FSE, HUFv06_decompress can regenerate
           RLE (cSrcSize==1) and uncompressed (cSrcSize==dstSize) data,
           because it knows size to regenerate.
    @return : size of regenerated data (== dstSize)
              or an error code, which can be tested using HUFv06_isError()
 */
// 
// Tool functions
//
size_t HUFv06_compressBound(size_t size);       /**< maximum compressed size */

#if defined (__cplusplus)
}
#endif

#endif   /* HUFv06_H */
/* ******************************************************************
   Huffman codec, part of New Generation Entropy library
   header file, for static linking only
   Copyright (C) 2013-2016, Yann Collet

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

* Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

   You can contact the author at :
   - Source repository : https://github.com/Cyan4973/FiniteStateEntropy
****************************************************************** */
#ifndef HUFv06_STATIC_H
#define HUFv06_STATIC_H

#if defined (__cplusplus)
extern "C" {
#endif

/* ****************************************
*  Static allocation
******************************************/
/* HUF buffer bounds */
#define HUFv06_CTABLEBOUND 129
#define HUFv06_BLOCKBOUND(size) (size + (size>>8) + 8)   /* only true if incompressible pre-filtered with fast heuristic */
#define HUFv06_COMPRESSBOUND(size) (HUFv06_CTABLEBOUND + HUFv06_BLOCKBOUND(size))   /* Macro version, useful for static allocation */

/* static allocation of HUF's DTable */
#define HUFv06_DTABLE_SIZE(maxTableLog)   (1 + (1<<maxTableLog))
#define HUFv06_CREATE_STATIC_DTABLEX2(DTable, maxTableLog) ushort DTable[HUFv06_DTABLE_SIZE(maxTableLog)] = { maxTableLog }
#define HUFv06_CREATE_STATIC_DTABLEX4(DTable, maxTableLog) uint32 DTable[HUFv06_DTABLE_SIZE(maxTableLog)] = { maxTableLog }
#define HUFv06_CREATE_STATIC_DTABLEX6(DTable, maxTableLog) uint32 DTable[HUFv06_DTABLE_SIZE(maxTableLog) * 3 / 2] = { maxTableLog }

/* ****************************************
*  Advanced decompression functions
******************************************/
size_t HUFv06_decompress4X2(void * dst, size_t dstSize, const void * cSrc, size_t cSrcSize);    /* single-symbol decoder */
size_t HUFv06_decompress4X4(void * dst, size_t dstSize, const void * cSrc, size_t cSrcSize);    /* double-symbols decoder */

/*!
   HUFv06_decompress() does the following:
   1. select the decompression algorithm (X2, X4, X6) based on pre-computed heuristics
   2. build Huffman table from save, using HUFv06_readDTableXn()
   3. decode 1 or 4 segments in parallel using HUFv06_decompressSXn_usingDTable
 */
size_t HUFv06_readDTableX2(ushort * DTable, const void * src, size_t srcSize);
size_t HUFv06_readDTableX4(uint32 * DTable, const void * src, size_t srcSize);
size_t HUFv06_decompress4X2_usingDTable(void * dst, size_t maxDstSize, const void * cSrc, size_t cSrcSize, const ushort * DTable);
size_t HUFv06_decompress4X4_usingDTable(void * dst, size_t maxDstSize, const void * cSrc, size_t cSrcSize, const uint32 * DTable);
/* single stream variants */
size_t HUFv06_decompress1X2(void * dst, size_t dstSize, const void * cSrc, size_t cSrcSize);    /* single-symbol decoder */
size_t HUFv06_decompress1X4(void * dst, size_t dstSize, const void * cSrc, size_t cSrcSize);    /* double-symbol decoder */
size_t HUFv06_decompress1X2_usingDTable(void * dst, size_t maxDstSize, const void * cSrc, size_t cSrcSize, const ushort * DTable);
size_t HUFv06_decompress1X4_usingDTable(void * dst, size_t maxDstSize, const void * cSrc, size_t cSrcSize, const uint32 * DTable);
// 
// Constants
// 
#define HUFv06_ABSOLUTEMAX_TABLELOG  16   /* absolute limit of HUFv06_MAX_TABLELOG. Beyond that value, code does not work */
#define HUFv06_MAX_TABLELOG  12           /* max configured tableLog (for static allocation); can be modified up to HUFv06_ABSOLUTEMAX_TABLELOG */
#define HUFv06_DEFAULT_TABLELOG  HUFv06_MAX_TABLELOG   /* tableLog by default, when not specified */
#define HUFv06_MAX_SYMBOL_VALUE 255
#if (HUFv06_MAX_TABLELOG > HUFv06_ABSOLUTEMAX_TABLELOG)
#error "HUFv06_MAX_TABLELOG is too large !"
#endif

/*! HUFv06_readStats() :
    Read compact Huffman tree, saved by HUFv06_writeCTable().
    `huffWeight` is destination buffer.
    @return : size read from `src`
 */
MEM_STATIC size_t HUFv06_readStats(BYTE * huffWeight, size_t hwSize, uint32 * rankStats, uint32 * nbSymbolsPtr, uint32 * tableLogPtr, const void* src, size_t srcSize)
{
	uint32 weightTotal;
	const BYTE * ip = PTR8C(src);
	size_t iSize;
	size_t oSize;
	if(!srcSize) return ERROR(srcSize_wrong);
	iSize = ip[0];
	/* memset(huffWeight, 0, hwSize); */   /* is not necessary, even though some analyzer complain ... */
	if(iSize >= 128) { /* special header */
		if(iSize >= (242)) { /* RLE */
			static uint32 l[14] = { 1, 2, 3, 4, 7, 8, 15, 16, 31, 32, 63, 64, 127, 128 };
			oSize = l[iSize-242];
			memset(huffWeight, 1, hwSize);
			iSize = 0;
		}
		else { /* Incompressible */
			oSize = iSize - 127;
			iSize = ((oSize+1)/2);
			if(iSize+1 > srcSize) return ERROR(srcSize_wrong);
			if(oSize >= hwSize) return ERROR(corruption_detected);
			ip += 1;
			{   uint32 n;
			    for(n = 0; n<oSize; n += 2) {
				    huffWeight[n]   = ip[n/2] >> 4;
				    huffWeight[n+1] = ip[n/2] & 15;
			    }
			}
		}
	}
	else { /* header compressed with FSE (normal case) */
		if(iSize+1 > srcSize) 
			return ERROR(srcSize_wrong);
		oSize = FSEv06_decompress(huffWeight, hwSize-1, ip+1, iSize); /* max (hwSize-1) values decoded, as last one is implied */
		if(FSEv06_isError(oSize)) return oSize;
	}
	/* collect weight stats */
	memzero(rankStats, (HUFv06_ABSOLUTEMAX_TABLELOG + 1) * sizeof(uint32));
	weightTotal = 0;
	{   uint32 n; for(n = 0; n<oSize; n++) {
		    if(huffWeight[n] >= HUFv06_ABSOLUTEMAX_TABLELOG) return ERROR(corruption_detected);
		    rankStats[huffWeight[n]]++;
		    weightTotal += (1 << huffWeight[n]) >> 1;
	    }
	}
	if(weightTotal == 0) 
		return ERROR(corruption_detected);
	/* get last non-null symbol weight (implied, total must be 2^n) */
	{   
		const uint32 tableLog = BITv06_highbit32(weightTotal) + 1;
	    if(tableLog > HUFv06_ABSOLUTEMAX_TABLELOG) return ERROR(corruption_detected);
	    *tableLogPtr = tableLog;
		/* determine last weight */
	    {   const uint32 total = 1 << tableLog;
		const uint32 rest = total - weightTotal;
		const uint32 verif = 1 << BITv06_highbit32(rest);
		const uint32 lastWeight = BITv06_highbit32(rest) + 1;
		if(verif != rest) return ERROR(corruption_detected); /* last value must be a clean power of 2 */
		huffWeight[oSize] = (BYTE)lastWeight;
		rankStats[lastWeight]++;}   }
	/* check tree construction validity */
	if((rankStats[1] < 2) || (rankStats[1] & 1)) 
		return ERROR(corruption_detected); /* by construction : at least 2 elts of rank 1, must be even */
	/* results */
	*nbSymbolsPtr = (uint32)(oSize+1);
	return iSize+1;
}

#if defined (__cplusplus)
}
#endif

#endif /* HUFv06_STATIC_H */
/* ******************************************************************
   Huffman decoder, part of New Generation Entropy library
   Copyright (C) 2013-2016, Yann Collet.

   BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

* Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following disclaimer
   in the documentation and/or other materials provided with the
   distribution.

    You can contact the author at :
    - FSE+HUF source repository : https://github.com/Cyan4973/FiniteStateEntropy
    - Public forum : https://groups.google.com/forum/#!forum/lz4c
****************************************************************** */
//
// Compiler specifics
//
#if defined (__cplusplus) || (defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* C99 */)
/* inline is defined */
#elif defined(_MSC_VER)
#define inline __inline
#else
#define inline /* disable inline */
#endif

#ifdef _MSC_VER    /* Visual Studio */
#pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
#endif
//
// Error Management
//
#define HUFv06_STATIC_ASSERT(c) { enum { HUFv06_static_assert = 1/(int)(!!(c)) }; }   /* use only *after* variable declarations */

/* *******************************************************
*  HUF : Huffman block decompression
*********************************************************/
typedef struct { 
	BYTE byte; 
	BYTE nbBits; 
} HUFv06_DEltX2;   /* single-symbol decoding */

typedef struct { 
	uint16 sequence; 
	BYTE nbBits; 
	BYTE length; 
} HUFv06_DEltX4;  /* double-symbols decoding */

typedef struct { 
	BYTE symbol; 
	BYTE weight; 
} sortedSymbol_t;

/*-***************************/
/*  single-symbol decoding   */
/*-***************************/

size_t HUFv06_readDTableX2(uint16* DTable, const void* src, size_t srcSize)
{
	BYTE huffWeight[HUFv06_MAX_SYMBOL_VALUE + 1];
	uint32 rankVal[HUFv06_ABSOLUTEMAX_TABLELOG + 1]; /* large enough for values from 0 to 16 */
	uint32 tableLog = 0;
	size_t iSize;
	uint32 nbSymbols = 0;
	uint32 n;
	uint32 nextRankStart;
	void* const dtPtr = DTable + 1;
	HUFv06_DEltX2* const dt = (HUFv06_DEltX2*)dtPtr;

	HUFv06_STATIC_ASSERT(sizeof(HUFv06_DEltX2) == sizeof(uint16)); /* if compilation fails here, assertion is false */
	/* memset(huffWeight, 0, sizeof(huffWeight)); */   /* is not necessary, even though some analyzer complain ...
	   */

	iSize = HUFv06_readStats(huffWeight, HUFv06_MAX_SYMBOL_VALUE + 1, rankVal, &nbSymbols, &tableLog, src, srcSize);
	if(HUFv06_isError(iSize)) return iSize;

	/* check result */
	if(tableLog > DTable[0]) return ERROR(tableLog_tooLarge); /* DTable is too small */
	DTable[0] = (uint16)tableLog; /* maybe should separate sizeof allocated DTable, from used size of DTable, in case
	                              of re-use */

	/* Prepare ranks */
	nextRankStart = 0;
	for(n = 1; n<tableLog+1; n++) {
		uint32 current = nextRankStart;
		nextRankStart += (rankVal[n] << (n-1));
		rankVal[n] = current;
	}
	/* fill DTable */
	for(n = 0; n<nbSymbols; n++) {
		const uint32 w = huffWeight[n];
		const uint32 length = (1 << w) >> 1;
		HUFv06_DEltX2 D;
		D.byte = (BYTE)n; D.nbBits = (BYTE)(tableLog + 1 - w);
		for(uint32 i = rankVal[w]; i < rankVal[w] + length; i++)
			dt[i] = D;
		rankVal[w] += length;
	}
	return iSize;
}

static BYTE HUFv06_decodeSymbolX2(BITv06_DStream_t* Dstream, const HUFv06_DEltX2* dt, const uint32 dtLog)
{
	const size_t val = BITv06_lookBitsFast(Dstream, dtLog); /* note : dtLog >= 1 */
	const BYTE c = dt[val].byte;
	BITv06_skipBits(Dstream, dt[val].nbBits);
	return c;
}

#define HUFv06_DECODE_SYMBOLX2_0(ptr, DStreamPtr) \
	*ptr++ = HUFv06_decodeSymbolX2(DStreamPtr, dt, dtLog)

#define HUFv06_DECODE_SYMBOLX2_1(ptr, DStreamPtr) \
	if(MEM_64bits() || (HUFv06_MAX_TABLELOG<=12)) \
		HUFv06_DECODE_SYMBOLX2_0(ptr, DStreamPtr)

#define HUFv06_DECODE_SYMBOLX2_2(ptr, DStreamPtr) \
	if(MEM_64bits()) \
		HUFv06_DECODE_SYMBOLX2_0(ptr, DStreamPtr)

static inline size_t HUFv06_decodeStreamX2(BYTE * p,
    BITv06_DStream_t* const bitDPtr,
    BYTE * const pEnd,
    const HUFv06_DEltX2* const dt,
    const uint32 dtLog)
{
	BYTE * const pStart = p;

	/* up to 4 symbols at a time */
	while((BITv06_reloadDStream(bitDPtr) == BITv06_DStream_unfinished) && (p <= pEnd-4)) {
		HUFv06_DECODE_SYMBOLX2_2(p, bitDPtr);
		HUFv06_DECODE_SYMBOLX2_1(p, bitDPtr);
		HUFv06_DECODE_SYMBOLX2_2(p, bitDPtr);
		HUFv06_DECODE_SYMBOLX2_0(p, bitDPtr);
	}

	/* closer to the end */
	while((BITv06_reloadDStream(bitDPtr) == BITv06_DStream_unfinished) && (p < pEnd))
		HUFv06_DECODE_SYMBOLX2_0(p, bitDPtr);

	/* no more data to retrieve from bitstream, hence no need to reload */
	while(p < pEnd)
		HUFv06_DECODE_SYMBOLX2_0(p, bitDPtr);

	return pEnd-pStart;
}

size_t HUFv06_decompress1X2_usingDTable(void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const uint16* DTable)
{
	BYTE * op = (BYTE *)dst;
	BYTE * const oend = op + dstSize;
	const uint32 dtLog = DTable[0];
	const void* dtPtr = DTable;
	const HUFv06_DEltX2* const dt = ((const HUFv06_DEltX2*)dtPtr)+1;
	BITv06_DStream_t bitD;

	{ const size_t errorCode = BITv06_initDStream(&bitD, cSrc, cSrcSize);
	  if(HUFv06_isError(errorCode)) return errorCode; }

	HUFv06_decodeStreamX2(op, &bitD, oend, dt, dtLog);

	/* check */
	if(!BITv06_endOfDStream(&bitD)) return ERROR(corruption_detected);

	return dstSize;
}

size_t HUFv06_decompress1X2(void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
	HUFv06_CREATE_STATIC_DTABLEX2(DTable, HUFv06_MAX_TABLELOG);
	const BYTE * ip = PTR8C(cSrc);

	const size_t errorCode = HUFv06_readDTableX2(DTable, cSrc, cSrcSize);
	if(HUFv06_isError(errorCode)) return errorCode;
	if(errorCode >= cSrcSize) return ERROR(srcSize_wrong);
	ip += errorCode;
	cSrcSize -= errorCode;
	return HUFv06_decompress1X2_usingDTable(dst, dstSize, ip, cSrcSize, DTable);
}

size_t HUFv06_decompress4X2_usingDTable(void* dst,  size_t dstSize, const void* cSrc, size_t cSrcSize, const uint16* DTable)
{
	/* Check */
	if(cSrcSize < 10) 
		return ERROR(corruption_detected); /* strict minimum : jump table + 1 byte per stream */
	{   
		const BYTE * const istart = PTR8C(cSrc);
	    BYTE * const ostart = (BYTE *)dst;
	    BYTE * const oend = ostart + dstSize;
	    const void* const dtPtr = DTable;
	    const HUFv06_DEltX2* const dt = ((const HUFv06_DEltX2*)dtPtr) +1;
	    const uint32 dtLog = DTable[0];
	    size_t errorCode;
		/* Init */
	    BITv06_DStream_t bitD1;
	    BITv06_DStream_t bitD2;
	    BITv06_DStream_t bitD3;
	    BITv06_DStream_t bitD4;
	    const size_t length1 = SMem::GetLe16(istart);
	    const size_t length2 = SMem::GetLe16(istart+2);
	    const size_t length3 = SMem::GetLe16(istart+4);
	    size_t length4;
	    const BYTE * const istart1 = istart + 6; /* jumpTable */
	    const BYTE * const istart2 = istart1 + length1;
	    const BYTE * const istart3 = istart2 + length2;
	    const BYTE * const istart4 = istart3 + length3;
	    const size_t segmentSize = (dstSize+3) / 4;
	    BYTE * const opStart2 = ostart + segmentSize;
	    BYTE * const opStart3 = opStart2 + segmentSize;
	    BYTE * const opStart4 = opStart3 + segmentSize;
	    BYTE * op1 = ostart;
	    BYTE * op2 = opStart2;
	    BYTE * op3 = opStart3;
	    BYTE * op4 = opStart4;
	    uint32 endSignal;

	    length4 = cSrcSize - (length1 + length2 + length3 + 6);
	    if(length4 > cSrcSize) return ERROR(corruption_detected); /* overflow */
	    errorCode = BITv06_initDStream(&bitD1, istart1, length1);
	    if(HUFv06_isError(errorCode)) return errorCode;
	    errorCode = BITv06_initDStream(&bitD2, istart2, length2);
	    if(HUFv06_isError(errorCode)) return errorCode;
	    errorCode = BITv06_initDStream(&bitD3, istart3, length3);
	    if(HUFv06_isError(errorCode)) return errorCode;
	    errorCode = BITv06_initDStream(&bitD4, istart4, length4);
	    if(HUFv06_isError(errorCode)) return errorCode;

		/* 16-32 symbols per loop (4-8 symbols per stream) */
	    endSignal = BITv06_reloadDStream(&bitD1) | BITv06_reloadDStream(&bitD2) | BITv06_reloadDStream(&bitD3) | BITv06_reloadDStream(
		    &bitD4);
	    for(; (endSignal==BITv06_DStream_unfinished) && (op4<(oend-7));) {
		    HUFv06_DECODE_SYMBOLX2_2(op1, &bitD1);
		    HUFv06_DECODE_SYMBOLX2_2(op2, &bitD2);
		    HUFv06_DECODE_SYMBOLX2_2(op3, &bitD3);
		    HUFv06_DECODE_SYMBOLX2_2(op4, &bitD4);
		    HUFv06_DECODE_SYMBOLX2_1(op1, &bitD1);
		    HUFv06_DECODE_SYMBOLX2_1(op2, &bitD2);
		    HUFv06_DECODE_SYMBOLX2_1(op3, &bitD3);
		    HUFv06_DECODE_SYMBOLX2_1(op4, &bitD4);
		    HUFv06_DECODE_SYMBOLX2_2(op1, &bitD1);
		    HUFv06_DECODE_SYMBOLX2_2(op2, &bitD2);
		    HUFv06_DECODE_SYMBOLX2_2(op3, &bitD3);
		    HUFv06_DECODE_SYMBOLX2_2(op4, &bitD4);
		    HUFv06_DECODE_SYMBOLX2_0(op1, &bitD1);
		    HUFv06_DECODE_SYMBOLX2_0(op2, &bitD2);
		    HUFv06_DECODE_SYMBOLX2_0(op3, &bitD3);
		    HUFv06_DECODE_SYMBOLX2_0(op4, &bitD4);
		    endSignal = BITv06_reloadDStream(&bitD1) | BITv06_reloadDStream(&bitD2) | BITv06_reloadDStream(&bitD3) |
			BITv06_reloadDStream(&bitD4);
	    }

		/* check corruption */
	    if(op1 > opStart2) return ERROR(corruption_detected);
	    if(op2 > opStart3) return ERROR(corruption_detected);
	    if(op3 > opStart4) return ERROR(corruption_detected);
		/* note : op4 supposed already verified within main loop */

		/* finish bitStreams one by one */
	    HUFv06_decodeStreamX2(op1, &bitD1, opStart2, dt, dtLog);
	    HUFv06_decodeStreamX2(op2, &bitD2, opStart3, dt, dtLog);
	    HUFv06_decodeStreamX2(op3, &bitD3, opStart4, dt, dtLog);
	    HUFv06_decodeStreamX2(op4, &bitD4, oend,     dt, dtLog);

		/* check */
	    endSignal = BITv06_endOfDStream(&bitD1) & BITv06_endOfDStream(&bitD2) & BITv06_endOfDStream(&bitD3) &
		BITv06_endOfDStream(&bitD4);
	    if(!endSignal) return ERROR(corruption_detected);

		/* decoded size */
	    return dstSize;}
}

size_t HUFv06_decompress4X2(void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
	HUFv06_CREATE_STATIC_DTABLEX2(DTable, HUFv06_MAX_TABLELOG);
	const BYTE * ip = PTR8C(cSrc);
	const size_t errorCode = HUFv06_readDTableX2(DTable, cSrc, cSrcSize);
	if(HUFv06_isError(errorCode)) return errorCode;
	if(errorCode >= cSrcSize) return ERROR(srcSize_wrong);
	ip += errorCode;
	cSrcSize -= errorCode;
	return HUFv06_decompress4X2_usingDTable(dst, dstSize, ip, cSrcSize, DTable);
}
// 
// double-symbols decoding 
// 
static void HUFv06_fillDTableX4Level2(HUFv06_DEltX4* DTable, uint32 sizeLog, const uint32 consumed, const uint32 * rankValOrigin, const int minWeight,
    const sortedSymbol_t* sortedSymbols, const uint32 sortedListSize, uint32 nbBitsBaseline, uint16 baseSeq)
{
	HUFv06_DEltX4 DElt;
	uint32 rankVal[HUFv06_ABSOLUTEMAX_TABLELOG + 1];
	/* get pre-calculated rankVal */
	memcpy(rankVal, rankValOrigin, sizeof(rankVal));
	/* fill skipped values */
	if(minWeight>1) {
		uint32 i, skipSize = rankVal[minWeight];
		SMem::PutLe(&(DElt.sequence), baseSeq);
		DElt.nbBits   = (BYTE)(consumed);
		DElt.length   = 1;
		for(i = 0; i < skipSize; i++)
			DTable[i] = DElt;
	}
	/* fill DTable */
	{ 
		uint32 s; 
		for(s = 0; s<sortedListSize; s++) { /* note : sortedSymbols already skipped */
		  const uint32 symbol = sortedSymbols[s].symbol;
		  const uint32 weight = sortedSymbols[s].weight;
		  const uint32 nbBits = nbBitsBaseline - weight;
		  const uint32 length = 1 << (sizeLog-nbBits);
		  const uint32 start = rankVal[weight];
		  uint32 i = start;
		  const uint32 end = start + length;

		  SMem::PutLe(&(DElt.sequence), (uint16)(baseSeq + (symbol << 8)));
		  DElt.nbBits = (BYTE)(nbBits + consumed);
		  DElt.length = 2;
		  do {
			  DTable[i++] = DElt;
		  } while(i<end);                   /* since length >= 1 */

		  rankVal[weight] += length;
	  }
	}
}

typedef uint32 rankVal_t[HUFv06_ABSOLUTEMAX_TABLELOG][HUFv06_ABSOLUTEMAX_TABLELOG + 1];

static void HUFv06_fillDTableX4(HUFv06_DEltX4* DTable, const uint32 targetLog, const sortedSymbol_t* sortedList, const uint32 sortedListSize,
    const uint32 * rankStart, rankVal_t rankValOrigin, const uint32 maxWeight, const uint32 nbBitsBaseline)
{
	uint32 rankVal[HUFv06_ABSOLUTEMAX_TABLELOG + 1];
	const int scaleLog = nbBitsBaseline - targetLog; /* note : targetLog >= srcLog, hence scaleLog <= 1 */
	const uint32 minBits  = nbBitsBaseline - maxWeight;
	uint32 s;
	memcpy(rankVal, rankValOrigin, sizeof(rankVal));
	/* fill DTable */
	for(s = 0; s<sortedListSize; s++) {
		const uint16 symbol = sortedList[s].symbol;
		const uint32 weight = sortedList[s].weight;
		const uint32 nbBits = nbBitsBaseline - weight;
		const uint32 start = rankVal[weight];
		const uint32 length = 1 << (targetLog-nbBits);
		if(targetLog-nbBits >= minBits) { /* enough room for a second symbol */
			uint32 sortedRank;
			int minWeight = nbBits + scaleLog;
			if(minWeight < 1) minWeight = 1;
			sortedRank = rankStart[minWeight];
			HUFv06_fillDTableX4Level2(DTable+start, targetLog-nbBits, nbBits,
			    rankValOrigin[nbBits], minWeight,
			    sortedList+sortedRank, sortedListSize-sortedRank,
			    nbBitsBaseline, symbol);
		}
		else {
			HUFv06_DEltX4 DElt;
			SMem::PutLe(&(DElt.sequence), symbol);
			DElt.nbBits = (BYTE)(nbBits);
			DElt.length = 1;
			{   
			    const uint32 end = start + length;
			    for(uint32 u = start; u < end; u++) 
					DTable[u] = DElt; 
			}
		}
		rankVal[weight] += length;
	}
}

size_t HUFv06_readDTableX4(uint32 * DTable, const void* src, size_t srcSize)
{
	BYTE weightList[HUFv06_MAX_SYMBOL_VALUE + 1];
	sortedSymbol_t sortedSymbol[HUFv06_MAX_SYMBOL_VALUE + 1];
	uint32 rankStats[HUFv06_ABSOLUTEMAX_TABLELOG + 1] = { 0 };
	uint32 rankStart0[HUFv06_ABSOLUTEMAX_TABLELOG + 2] = { 0 };
	uint32 * const rankStart = rankStart0+1;
	rankVal_t rankVal;
	uint32 tableLog, maxW, sizeOfSort, nbSymbols;
	const uint32 memLog = DTable[0];
	size_t iSize;
	void* dtPtr = DTable;
	HUFv06_DEltX4* const dt = ((HUFv06_DEltX4*)dtPtr) + 1;
	HUFv06_STATIC_ASSERT(sizeof(HUFv06_DEltX4) == sizeof(uint32)); /* if compilation fails here, assertion is false */
	if(memLog > HUFv06_ABSOLUTEMAX_TABLELOG) 
		return ERROR(tableLog_tooLarge);
	// memset(weightList, 0, sizeof(weightList)); */   /* is not necessary, even though some analyzer complain ...
	iSize = HUFv06_readStats(weightList, HUFv06_MAX_SYMBOL_VALUE + 1, rankStats, &nbSymbols, &tableLog, src, srcSize);
	if(HUFv06_isError(iSize)) 
		return iSize;
	/* check result */
	if(tableLog > memLog) 
		return ERROR(tableLog_tooLarge); /* DTable can't fit code depth */
	/* find maxWeight */
	for(maxW = tableLog; rankStats[maxW]==0; maxW--) {
	}                                                 /* necessarily finds a solution before 0 */
	/* Get start index of each weight */
	{   
		uint32 nextRankStart = 0;
	    for(uint32 w = 1; w<maxW+1; w++) {
		    uint32 current = nextRankStart;
		    nextRankStart += rankStats[w];
		    rankStart[w] = current;
	    }
	    rankStart[0] = nextRankStart; /* put all 0w symbols at the end of sorted list*/
	    sizeOfSort = nextRankStart;
	}
	/* sort symbols by weight */
	{   
	    for(uint32 s = 0; s<nbSymbols; s++) {
		    const uint32 w = weightList[s];
		    const uint32 r = rankStart[w]++;
		    sortedSymbol[r].symbol = (BYTE)s;
		    sortedSymbol[r].weight = (BYTE)w;
	    }
	    rankStart[0] = 0; /* forget 0w symbols; this is beginning of weight(1) */
	}
	/* Build rankVal */
	{   
		uint32 * const rankVal0 = rankVal[0];
	    {   
			int const rescale = (memLog-tableLog) - 1;/* tableLog <= memLog */
		uint32 nextRankVal = 0;
		uint32 w;
		for(w = 1; w<maxW+1; w++) {
			uint32 current = nextRankVal;
			nextRankVal += rankStats[w] << (w+rescale);
			rankVal0[w] = current;
		}
	    }
	    {   
		const uint32 minBits = tableLog+1 - maxW;
		uint32 consumed;
		for(consumed = minBits; consumed < memLog - minBits + 1; consumed++) {
			uint32 * const rankValPtr = rankVal[consumed];
			uint32 w;
			for(w = 1; w < maxW+1; w++) {
				rankValPtr[w] = rankVal0[w] >> consumed;
			}
		}
	    }   
	}
	HUFv06_fillDTableX4(dt, memLog, sortedSymbol, sizeOfSort, rankStart0, rankVal, maxW, tableLog+1);
	return iSize;
}

static uint32 HUFv06_decodeSymbolX4(void* op, BITv06_DStream_t* DStream, const HUFv06_DEltX4* dt, const uint32 dtLog)
{
	const size_t val = BITv06_lookBitsFast(DStream, dtLog); /* note : dtLog >= 1 */
	memcpy(op, dt+val, 2);
	BITv06_skipBits(DStream, dt[val].nbBits);
	return dt[val].length;
}

static uint32 HUFv06_decodeLastSymbolX4(void* op, BITv06_DStream_t* DStream, const HUFv06_DEltX4* dt, const uint32 dtLog)
{
	const size_t val = BITv06_lookBitsFast(DStream, dtLog); /* note : dtLog >= 1 */
	memcpy(op, dt+val, 1);
	if(dt[val].length==1) 
		BITv06_skipBits(DStream, dt[val].nbBits);
	else {
		if(DStream->bitsConsumed < (sizeof(DStream->bitContainer)*8)) {
			BITv06_skipBits(DStream, dt[val].nbBits);
			if(DStream->bitsConsumed > (sizeof(DStream->bitContainer)*8))
				DStream->bitsConsumed = (sizeof(DStream->bitContainer)*8); // ugly hack; works only because it's the last symbol. Note : can't easily extract nbBits from just this symbol
		}
	}
	return 1;
}

#define HUFv06_DECODE_SYMBOLX4_0(ptr, DStreamPtr) ptr += HUFv06_decodeSymbolX4(ptr, DStreamPtr, dt, dtLog)
#define HUFv06_DECODE_SYMBOLX4_1(ptr, DStreamPtr) if(MEM_64bits() || (HUFv06_MAX_TABLELOG<=12)) ptr += HUFv06_decodeSymbolX4(ptr, DStreamPtr, dt, dtLog)
#define HUFv06_DECODE_SYMBOLX4_2(ptr, DStreamPtr) if(MEM_64bits()) ptr += HUFv06_decodeSymbolX4(ptr, DStreamPtr, dt, dtLog)

static inline size_t HUFv06_decodeStreamX4(BYTE * p, BITv06_DStream_t* bitDPtr, BYTE * const pEnd, const HUFv06_DEltX4* const dt, const uint32 dtLog)
{
	BYTE * const pStart = p;
	/* up to 8 symbols at a time */
	while((BITv06_reloadDStream(bitDPtr) == BITv06_DStream_unfinished) && (p < pEnd-7)) {
		HUFv06_DECODE_SYMBOLX4_2(p, bitDPtr);
		HUFv06_DECODE_SYMBOLX4_1(p, bitDPtr);
		HUFv06_DECODE_SYMBOLX4_2(p, bitDPtr);
		HUFv06_DECODE_SYMBOLX4_0(p, bitDPtr);
	}

	/* closer to the end */
	while((BITv06_reloadDStream(bitDPtr) == BITv06_DStream_unfinished) && (p <= pEnd-2))
		HUFv06_DECODE_SYMBOLX4_0(p, bitDPtr);

	while(p <= pEnd-2)
		HUFv06_DECODE_SYMBOLX4_0(p, bitDPtr); /* no need to reload : reached the end of DStream */

	if(p < pEnd)
		p += HUFv06_decodeLastSymbolX4(p, bitDPtr, dt, dtLog);

	return p-pStart;
}

size_t HUFv06_decompress1X4_usingDTable(void* dst,  size_t dstSize,
    const void* cSrc, size_t cSrcSize,
    const uint32 * DTable)
{
	const BYTE * const istart = PTR8C(cSrc);
	BYTE * const ostart = (BYTE *)dst;
	BYTE * const oend = ostart + dstSize;

	const uint32 dtLog = DTable[0];
	const void* const dtPtr = DTable;
	const HUFv06_DEltX4* const dt = ((const HUFv06_DEltX4*)dtPtr) +1;

	/* Init */
	BITv06_DStream_t bitD;
	{ const size_t errorCode = BITv06_initDStream(&bitD, istart, cSrcSize);
	  if(HUFv06_isError(errorCode)) return errorCode; }

	/* decode */
	HUFv06_decodeStreamX4(ostart, &bitD, oend, dt, dtLog);

	/* check */
	if(!BITv06_endOfDStream(&bitD)) return ERROR(corruption_detected);

	/* decoded size */
	return dstSize;
}

size_t HUFv06_decompress1X4(void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
	HUFv06_CREATE_STATIC_DTABLEX4(DTable, HUFv06_MAX_TABLELOG);
	const BYTE * ip = PTR8C(cSrc);
	const size_t hSize = HUFv06_readDTableX4(DTable, cSrc, cSrcSize);
	if(HUFv06_isError(hSize)) return hSize;
	if(hSize >= cSrcSize) return ERROR(srcSize_wrong);
	ip += hSize;
	cSrcSize -= hSize;
	return HUFv06_decompress1X4_usingDTable(dst, dstSize, ip, cSrcSize, DTable);
}

size_t HUFv06_decompress4X4_usingDTable(void* dst,  size_t dstSize, const void* cSrc, size_t cSrcSize, const uint32 * DTable)
{
	if(cSrcSize < 10) 
		return ERROR(corruption_detected); /* strict minimum : jump table + 1 byte per stream */
	{   
		const BYTE * const istart = PTR8C(cSrc);
	    BYTE * const ostart = (BYTE *)dst;
	    BYTE * const oend = ostart + dstSize;
	    const void * const dtPtr = DTable;
	    const HUFv06_DEltX4* const dt = ((const HUFv06_DEltX4*)dtPtr) +1;
	    const uint32 dtLog = DTable[0];
	    size_t errorCode;

		/* Init */
	    BITv06_DStream_t bitD1;
	    BITv06_DStream_t bitD2;
	    BITv06_DStream_t bitD3;
	    BITv06_DStream_t bitD4;
	    const size_t length1 = SMem::GetLe16(istart);
	    const size_t length2 = SMem::GetLe16(istart+2);
	    const size_t length3 = SMem::GetLe16(istart+4);
	    size_t length4;
	    const BYTE * const istart1 = istart + 6; /* jumpTable */
	    const BYTE * const istart2 = istart1 + length1;
	    const BYTE * const istart3 = istart2 + length2;
	    const BYTE * const istart4 = istart3 + length3;
	    const size_t segmentSize = (dstSize+3) / 4;
	    BYTE * const opStart2 = ostart + segmentSize;
	    BYTE * const opStart3 = opStart2 + segmentSize;
	    BYTE * const opStart4 = opStart3 + segmentSize;
	    BYTE * op1 = ostart;
	    BYTE * op2 = opStart2;
	    BYTE * op3 = opStart3;
	    BYTE * op4 = opStart4;
	    uint32 endSignal;

	    length4 = cSrcSize - (length1 + length2 + length3 + 6);
	    if(length4 > cSrcSize) return ERROR(corruption_detected); /* overflow */
	    errorCode = BITv06_initDStream(&bitD1, istart1, length1);
	    if(HUFv06_isError(errorCode)) return errorCode;
	    errorCode = BITv06_initDStream(&bitD2, istart2, length2);
	    if(HUFv06_isError(errorCode)) return errorCode;
	    errorCode = BITv06_initDStream(&bitD3, istart3, length3);
	    if(HUFv06_isError(errorCode)) return errorCode;
	    errorCode = BITv06_initDStream(&bitD4, istart4, length4);
	    if(HUFv06_isError(errorCode)) return errorCode;

		/* 16-32 symbols per loop (4-8 symbols per stream) */
	    endSignal = BITv06_reloadDStream(&bitD1) | BITv06_reloadDStream(&bitD2) | BITv06_reloadDStream(&bitD3) | BITv06_reloadDStream(
		    &bitD4);
	    for(; (endSignal==BITv06_DStream_unfinished) && (op4<(oend-7));) {
		    HUFv06_DECODE_SYMBOLX4_2(op1, &bitD1);
		    HUFv06_DECODE_SYMBOLX4_2(op2, &bitD2);
		    HUFv06_DECODE_SYMBOLX4_2(op3, &bitD3);
		    HUFv06_DECODE_SYMBOLX4_2(op4, &bitD4);
		    HUFv06_DECODE_SYMBOLX4_1(op1, &bitD1);
		    HUFv06_DECODE_SYMBOLX4_1(op2, &bitD2);
		    HUFv06_DECODE_SYMBOLX4_1(op3, &bitD3);
		    HUFv06_DECODE_SYMBOLX4_1(op4, &bitD4);
		    HUFv06_DECODE_SYMBOLX4_2(op1, &bitD1);
		    HUFv06_DECODE_SYMBOLX4_2(op2, &bitD2);
		    HUFv06_DECODE_SYMBOLX4_2(op3, &bitD3);
		    HUFv06_DECODE_SYMBOLX4_2(op4, &bitD4);
		    HUFv06_DECODE_SYMBOLX4_0(op1, &bitD1);
		    HUFv06_DECODE_SYMBOLX4_0(op2, &bitD2);
		    HUFv06_DECODE_SYMBOLX4_0(op3, &bitD3);
		    HUFv06_DECODE_SYMBOLX4_0(op4, &bitD4);

		    endSignal = BITv06_reloadDStream(&bitD1) | BITv06_reloadDStream(&bitD2) | BITv06_reloadDStream(&bitD3) |
			BITv06_reloadDStream(&bitD4);
	    }

		/* check corruption */
	    if(op1 > opStart2) return ERROR(corruption_detected);
	    if(op2 > opStart3) return ERROR(corruption_detected);
	    if(op3 > opStart4) return ERROR(corruption_detected);
		/* note : op4 supposed already verified within main loop */

		/* finish bitStreams one by one */
	    HUFv06_decodeStreamX4(op1, &bitD1, opStart2, dt, dtLog);
	    HUFv06_decodeStreamX4(op2, &bitD2, opStart3, dt, dtLog);
	    HUFv06_decodeStreamX4(op3, &bitD3, opStart4, dt, dtLog);
	    HUFv06_decodeStreamX4(op4, &bitD4, oend,     dt, dtLog);

		/* check */
	    endSignal = BITv06_endOfDStream(&bitD1) & BITv06_endOfDStream(&bitD2) & BITv06_endOfDStream(&bitD3) &
		BITv06_endOfDStream(&bitD4);
	    if(!endSignal) return ERROR(corruption_detected);

		/* decoded size */
	    return dstSize;}
}

size_t HUFv06_decompress4X4(void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
	HUFv06_CREATE_STATIC_DTABLEX4(DTable, HUFv06_MAX_TABLELOG);
	const BYTE * ip = PTR8C(cSrc);

	size_t hSize = HUFv06_readDTableX4(DTable, cSrc, cSrcSize);
	if(HUFv06_isError(hSize)) return hSize;
	if(hSize >= cSrcSize) return ERROR(srcSize_wrong);
	ip += hSize;
	cSrcSize -= hSize;

	return HUFv06_decompress4X4_usingDTable(dst, dstSize, ip, cSrcSize, DTable);
}

/* ********************************/
/* Generic decompression selector */
/* ********************************/

typedef struct { uint32 tableTime; uint32 decode256Time; } algo_time_t;
static const algo_time_t algoTime[16 /* Quantization */][3 /* single, double, quad */] =
{
	/* single, double, quad */
	{{0, 0}, {1, 1}, {2, 2}}, /* Q==0 : impossible */
	{{0, 0}, {1, 1}, {2, 2}}, /* Q==1 : impossible */
	{{  38, 130}, {1313, 74}, {2151, 38}},/* Q == 2 : 12-18% */
	{{ 448, 128}, {1353, 74}, {2238, 41}}, /* Q == 3 : 18-25% */
	{{ 556, 128}, {1353, 74}, {2238, 47}}, /* Q == 4 : 25-32% */
	{{ 714, 128}, {1418, 74}, {2436, 53}}, /* Q == 5 : 32-38% */
	{{ 883, 128}, {1437, 74}, {2464, 61}}, /* Q == 6 : 38-44% */
	{{ 897, 128}, {1515, 75}, {2622, 68}}, /* Q == 7 : 44-50% */
	{{ 926, 128}, {1613, 75}, {2730, 75}}, /* Q == 8 : 50-56% */
	{{ 947, 128}, {1729, 77}, {3359, 77}}, /* Q == 9 : 56-62% */
	{{1107, 128}, {2083, 81}, {4006, 84}}, /* Q ==10 : 62-69% */
	{{1177, 128}, {2379, 87}, {4785, 88}}, /* Q ==11 : 69-75% */
	{{1242, 128}, {2415, 93}, {5155, 84}}, /* Q ==12 : 75-81% */
	{{1349, 128}, {2644, 106}, {5260, 106}}, /* Q ==13 : 81-87% */
	{{1455, 128}, {2422, 124}, {4174, 124}}, /* Q ==14 : 87-93% */
	{{ 722, 128}, {1891, 145}, {1936, 146}}, /* Q ==15 : 93-99% */
};

typedef size_t (* decompressionAlgo)(void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize);

size_t HUFv06_decompress(void* dst, size_t dstSize, const void* cSrc, size_t cSrcSize)
{
	static const decompressionAlgo decompress[3] = { HUFv06_decompress4X2, HUFv06_decompress4X4, NULL };
	uint32 Dtime[3]; /* decompression time estimation */

	/* validation checks */
	if(dstSize == 0) return ERROR(dstSize_tooSmall);
	if(cSrcSize > dstSize) return ERROR(corruption_detected); /* invalid */
	if(cSrcSize == dstSize) {
		memcpy(dst, cSrc, dstSize); return dstSize;
	}                                                                      /* not compressed */
	if(cSrcSize == 1) {
		memset(dst, *PTR8C(cSrc), dstSize); return dstSize;
	}                                                                              /* RLE */

	/* decoder timing evaluation */
	{   const uint32 Q = (uint32)(cSrcSize * 16 / dstSize);/* Q < 16 since dstSize > cSrcSize */
	    const uint32 D256 = (uint32)(dstSize >> 8);
	    uint32 n; for(n = 0; n<3; n++)
		    Dtime[n] = algoTime[Q][n].tableTime + (algoTime[Q][n].decode256Time * D256); }

	Dtime[1] += Dtime[1] >> 4; Dtime[2] += Dtime[2] >> 3; /* advantage to algorithms using less memory, for cache
	                                                         eviction */

	{   uint32 algoNb = 0;
	    if(Dtime[1] < Dtime[0]) algoNb = 1;
		/* if (Dtime[2] < Dtime[algoNb]) algoNb = 2; */   /* current speed of HUFv06_decompress4X6 is not good
		   */
	    return decompress[algoNb](dst, dstSize, cSrc, cSrcSize);}

	/* return HUFv06_decompress4X2(dst, dstSize, cSrc, cSrcSize); */   /* multi-streams single-symbol decoding */
	/* return HUFv06_decompress4X4(dst, dstSize, cSrc, cSrcSize); */   /* multi-streams double-symbols decoding */
	/* return HUFv06_decompress4X6(dst, dstSize, cSrc, cSrcSize); */   /* multi-streams quad-symbols decoding */
}

/*
    Common functions of Zstd compression library
    Copyright (C) 2015-2016, Yann Collet.

    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
 * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

    You can contact the author at :
    - zstd homepage : http://www.zstd.net/
 */

/*-****************************************
*  Version
******************************************/

/*-****************************************
*  ZSTD Error Management
******************************************/
/*! ZSTDv06_isError() :
 *   tells if a return value is an error code */
uint ZSTDv06_isError(size_t code) { return ERR_isError(code); }

/*! ZSTDv06_getErrorName() :
 *   provides error code string from function result (useful for debugging) */
const char* ZSTDv06_getErrorName(size_t code) { return ERR_getErrorName(code); }

/* **************************************************************
*  ZBUFF Error Management
****************************************************************/
uint ZBUFFv06_isError(size_t errorCode) { return ERR_isError(errorCode); }
const char* ZBUFFv06_getErrorName(size_t errorCode) { return ERR_getErrorName(errorCode); }

/*
    zstd - standard compression library
    Copyright (C) 2014-2016, Yann Collet.

    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
 * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

    You can contact the author at :
    - zstd homepage : http://www.zstd.net
 */
//
// Tuning parameters
//
/*!
 * HEAPMODE :
 * Select how default decompression function ZSTDv06_decompress() will allocate memory,
 * in memory stack (0), or in memory heap (1, requires malloc())
 */
#ifndef ZSTDv06_HEAPMODE
#define ZSTDv06_HEAPMODE 1
#endif
//
// Compiler specifics
//
#ifdef _MSC_VER    /* Visual Studio */
	#pragma warning(disable : 4127)        /* disable: C4127: conditional expression is constant */
	#pragma warning(disable : 4324)        /* disable: C4324: padded structure */
#endif

/*-*************************************
*  Macros
***************************************/
#define ZSTDv06_isError ERR_isError   /* for inlining */
#define FSEv06_isError  ERR_isError
#define HUFv06_isError  ERR_isError
// 
// Memory operations
// 
static void ZSTDv06_copy4(void* dst, const void* src) { memcpy(dst, src, 4); }
// 
// Context management
// 
typedef enum { 
	ZSTDds_getFrameHeaderSize, 
	ZSTDds_decodeFrameHeader,
	ZSTDds_decodeBlockHeader, 
	ZSTDds_decompressBlock 
} ZSTDv06_dStage;

struct ZSTDv06_DCtx_s {
	FSEv06_DTable LLTable[FSEv06_DTABLE_SIZE_U32(LLFSELog)];
	FSEv06_DTable OffTable[FSEv06_DTABLE_SIZE_U32(OffFSELog)];
	FSEv06_DTable MLTable[FSEv06_DTABLE_SIZE_U32(MLFSELog)];
	uint32 hufTableX4[HUFv06_DTABLE_SIZE(ZSTD_HUFFDTABLE_CAPACITY_LOG)];
	const void * previousDstEnd;
	const void * base;
	const void * vBase;
	const void * dictEnd;
	size_t expected;
	size_t headerSize;
	ZSTDv06_frameParams fParams;
	blockType_t bType; /* used in ZSTDv06_decompressContinue(), to transfer blockType between header decoding and
	                      block decoding stages */
	ZSTDv06_dStage stage;
	uint32 flagRepeatTable;
	const BYTE * litPtr;
	size_t litSize;
	BYTE litBuffer[ZSTDv06_BLOCKSIZE_MAX + WILDCOPY_OVERLENGTH];
	BYTE headerBuffer[ZSTDv06_FRAMEHEADERSIZE_MAX];
};  /* typedef'd to ZSTDv06_DCtx within "zstd_static.h" */

size_t ZSTDv06_sizeofDCtx(void);  /* Hidden declaration */
size_t ZSTDv06_sizeofDCtx(void) {
	return sizeof(ZSTDv06_DCtx);
}

size_t ZSTDv06_decompressBegin(ZSTDv06_DCtx* dctx)
{
	dctx->expected = ZSTDv06_frameHeaderSize_min;
	dctx->stage = ZSTDds_getFrameHeaderSize;
	dctx->previousDstEnd = NULL;
	dctx->base = NULL;
	dctx->vBase = NULL;
	dctx->dictEnd = NULL;
	dctx->hufTableX4[0] = ZSTD_HUFFDTABLE_CAPACITY_LOG;
	dctx->flagRepeatTable = 0;
	return 0;
}

ZSTDv06_DCtx* ZSTDv06_createDCtx(void)
{
	ZSTDv06_DCtx* dctx = (ZSTDv06_DCtx*)SAlloc::M(sizeof(ZSTDv06_DCtx));
	if(dctx)
		ZSTDv06_decompressBegin(dctx);
	return dctx;
}

size_t ZSTDv06_freeDCtx(ZSTDv06_DCtx* dctx)
{
	SAlloc::F(dctx);
	return 0; /* reserved as a potential error code in the future */
}

void ZSTDv06_copyDCtx(ZSTDv06_DCtx* dstDCtx, const ZSTDv06_DCtx* srcDCtx)
{
	memcpy(dstDCtx, srcDCtx, sizeof(ZSTDv06_DCtx) - (ZSTDv06_BLOCKSIZE_MAX+WILDCOPY_OVERLENGTH + ZSTDv06_frameHeaderSize_max)); // no need to copy workspace 
}
// 
// Decompression section
// 
/* Frame format description
   Frame Header -  [ Block Header - Block ] - Frame End
   1) Frame Header
      - 4 bytes - Magic Number : ZSTDv06_MAGICNUMBER (defined within zstd_static.h)
      - 1 byte  - Frame Descriptor
   2) Block Header
      - 3 bytes, starting with a 2-bits descriptor
                 Uncompressed, Compressed, Frame End, unused
   3) Block
      See Block Format Description
   4) Frame End
      - 3 bytes, compatible with Block Header
 */

/* Frame descriptor

   1 byte, using :
   bit 0-3 : windowLog - ZSTDv06_WINDOWLOG_ABSOLUTEMIN   (see zstd_internal.h)
   bit 4   : minmatch 4(0) or 3(1)
   bit 5   : reserved (must be zero)
   bit 6-7 : Frame content size : unknown, 1 byte, 2 bytes, 8 bytes

   Optional : content size (0, 1, 2 or 8 bytes)
   0 : unknown
   1 : 0-255 bytes
   2 : 256 - 65535+256
   8 : up to 16 exa
 */

/* Compressed Block, format description

   Block = Literal Section - Sequences Section
   Prerequisite : size of (compressed) block, maximum size of regenerated data

   1) Literal Section

   1.1) Header : 1-5 bytes
        flags: 2 bits
            00 compressed by Huff0
            01 unused
            10 is Raw (uncompressed)
            11 is Rle
            Note : using 01 => Huff0 with precomputed table ?
            Note : delta map ? => compressed ?

   1.1.1) Huff0-compressed literal block : 3-5 bytes
            srcSize < 1 KB => 3 bytes (2-2-10-10) => single stream
            srcSize < 1 KB => 3 bytes (2-2-10-10)
            srcSize < 16KB => 4 bytes (2-2-14-14)
            else           => 5 bytes (2-2-18-18)
            big endian convention

   1.1.2) Raw (uncompressed) literal block header : 1-3 bytes
        size :  5 bits: (IS_RAW<<6) + (0<<4) + size
               12 bits: (IS_RAW<<6) + (2<<4) + (size>>8)
                        size&255
               20 bits: (IS_RAW<<6) + (3<<4) + (size>>16)
                        size>>8&255
                        size&255

   1.1.3) Rle (repeated single byte) literal block header : 1-3 bytes
        size :  5 bits: (IS_RLE<<6) + (0<<4) + size
               12 bits: (IS_RLE<<6) + (2<<4) + (size>>8)
                        size&255
               20 bits: (IS_RLE<<6) + (3<<4) + (size>>16)
                        size>>8&255
                        size&255

   1.1.4) Huff0-compressed literal block, using precomputed CTables : 3-5 bytes
            srcSize < 1 KB => 3 bytes (2-2-10-10) => single stream
            srcSize < 1 KB => 3 bytes (2-2-10-10)
            srcSize < 16KB => 4 bytes (2-2-14-14)
            else           => 5 bytes (2-2-18-18)
            big endian convention

        1- CTable available (stored into workspace ?)
        2- Small input (fast heuristic ? Full comparison ? depend on clevel ?)


   1.2) Literal block content

   1.2.1) Huff0 block, using sizes from header
        See Huff0 format

   1.2.2) Huff0 block, using prepared table

   1.2.3) Raw content

   1.2.4) single byte


   2) Sequences section
      TO DO
 */

/** ZSTDv06_frameHeaderSize() :
 *   srcSize must be >= ZSTDv06_frameHeaderSize_min.
 *   @return : size of the Frame Header */
static size_t ZSTDv06_frameHeaderSize(const void* src, size_t srcSize)
{
	if(srcSize < ZSTDv06_frameHeaderSize_min) 
		return ERROR(srcSize_wrong);
	{ 
		const uint32 fcsId = (((const BYTE *)src)[4]) >> 6;
		return ZSTDv06_frameHeaderSize_min + ZSTDv06_fcs_fieldSize[fcsId]; 
	}
}

/** ZSTDv06_getFrameParams() :
 *   decode Frame Header, or provide expected `srcSize`.
 *   @return : 0, `fparamsPtr` is correctly filled,
 *            >0, `srcSize` is too small, result is expected `srcSize`,
 *             or an error code, which can be tested using ZSTDv06_isError() */
size_t ZSTDv06_getFrameParams(ZSTDv06_frameParams* fparamsPtr, const void* src, size_t srcSize)
{
	const BYTE * ip = PTR8C(src);
	if(srcSize < ZSTDv06_frameHeaderSize_min) 
		return ZSTDv06_frameHeaderSize_min;
	if(SMem::GetLe32(src) != ZSTDv06_MAGICNUMBER) 
		return ERROR(prefix_unknown);
	/* ensure there is enough `srcSize` to fully read/decode frame header */
	{ 
		const size_t fhsize = ZSTDv06_frameHeaderSize(src, srcSize);
		if(srcSize < fhsize) 
			return fhsize; 
	}
	memzero(fparamsPtr, sizeof(*fparamsPtr));
	{   
		BYTE const frameDesc = ip[4];
	    fparamsPtr->windowLog = (frameDesc & 0xF) + ZSTDv06_WINDOWLOG_ABSOLUTEMIN;
	    if((frameDesc & 0x20) != 0) return ERROR(frameParameter_unsupported); /* reserved 1 bit */
	    switch(frameDesc >> 6) /* fcsId */ {
		    default: /* impossible */
		    case 0: fparamsPtr->frameContentSize = 0; break;
		    case 1: fparamsPtr->frameContentSize = ip[5]; break;
		    case 2: fparamsPtr->frameContentSize = SMem::GetLe16(ip+5)+256; break;
		    case 3: fparamsPtr->frameContentSize = SMem::GetLe64(ip+5); break;
	    }   
	}
	return 0;
}

/** ZSTDv06_decodeFrameHeader() :
 *   `srcSize` must be the size provided by ZSTDv06_frameHeaderSize().
 *   @return : 0 if success, or an error code, which can be tested using ZSTDv06_isError() */
static size_t ZSTDv06_decodeFrameHeader(ZSTDv06_DCtx* zc, const void* src, size_t srcSize)
{
	const size_t result = ZSTDv06_getFrameParams(&(zc->fParams), src, srcSize);
	if((MEM_32bits()) && (zc->fParams.windowLog > 25)) 
		return ERROR(frameParameter_unsupported);
	return result;
}

typedef struct {
	blockType_t blockType;
	uint32 origSize;
} blockProperties_t;

/*! ZSTDv06_getcBlockSize() :
 *   Provides the size of compressed block from block header `src` */
static size_t ZSTDv06_getcBlockSize(const void* src, size_t srcSize, blockProperties_t* bpPtr)
{
	const BYTE * const in = PTR8C(src);
	uint32 cSize;

	if(srcSize < ZSTDv06_blockHeaderSize) return ERROR(srcSize_wrong);

	bpPtr->blockType = (blockType_t)((*in) >> 6);
	cSize = in[2] + (in[1]<<8) + ((in[0] & 7)<<16);
	bpPtr->origSize = (bpPtr->blockType == bt_rle) ? cSize : 0;

	if(bpPtr->blockType == bt_end) return 0;
	if(bpPtr->blockType == bt_rle) return 1;
	return cSize;
}

static size_t ZSTDv06_copyRawBlock(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
	if(!dst || (srcSize > dstCapacity))
		return ERROR(dstSize_tooSmall);
	memcpy(dst, src, srcSize);
	return srcSize;
}

/*! ZSTDv06_decodeLiteralsBlock() :
    @return : nb of bytes read from src (< srcSize ) */
static size_t ZSTDv06_decodeLiteralsBlock(ZSTDv06_DCtx* dctx, const void* src, size_t srcSize) /* note : srcSize < BLOCKSIZE */
{
	const BYTE * const istart = PTR8C(src);
	/* any compressed block with literals segment must be at least this size */
	if(srcSize < MIN_CBLOCK_SIZE) 
		return ERROR(corruption_detected);
	switch(istart[0]>> 6) {
		case IS_HUF:
	    {   
			size_t litSize, litCSize, singleStream = 0;
			uint32 lhSize = ((istart[0]) >> 4) & 3;
			if(srcSize < 5) 
				return ERROR(corruption_detected); /* srcSize >= MIN_CBLOCK_SIZE == 3; here we need up to 5 for lhSize, + cSize (+nbSeq) */
			switch(lhSize) {
				case 0: case 1: default: /* note : default is impossible, since lhSize into [0..3] */
					/* 2 - 2 - 10 - 10 */
					lhSize = 3;
					singleStream = istart[0] & 16;
					litSize  = ((istart[0] & 15) << 6) + (istart[1] >> 2);
					litCSize = ((istart[1] &  3) << 8) + istart[2];
					break;
				case 2:
					/* 2 - 2 - 14 - 14 */
					lhSize = 4;
					litSize  = ((istart[0] & 15) << 10) + (istart[1] << 2) + (istart[2] >> 6);
					litCSize = ((istart[2] & 63) <<  8) + istart[3];
					break;
				case 3:
					/* 2 - 2 - 18 - 18 */
					lhSize = 5;
					litSize  = ((istart[0] & 15) << 14) + (istart[1] << 6) + (istart[2] >> 2);
					litCSize = ((istart[2] &  3) << 16) + (istart[3] << 8) + istart[4];
					break;
			}
			if(litSize > ZSTDv06_BLOCKSIZE_MAX) 
				return ERROR(corruption_detected);
			if(litCSize + lhSize > srcSize) 
				return ERROR(corruption_detected);
			if(HUFv06_isError(singleStream ? HUFv06_decompress1X2(dctx->litBuffer, litSize, istart+lhSize, litCSize) : HUFv06_decompress(dctx->litBuffer, litSize, istart+lhSize, litCSize)))
				return ERROR(corruption_detected);
		dctx->litPtr = dctx->litBuffer;
		dctx->litSize = litSize;
		memzero(dctx->litBuffer + dctx->litSize, WILDCOPY_OVERLENGTH);
		return litCSize + lhSize;}
		case IS_PCH:
	    {   
			size_t litSize, litCSize;
			uint32 lhSize = ((istart[0]) >> 4) & 3;
			if(lhSize != 1) /* only case supported for now : small litSize, single stream */
				return ERROR(corruption_detected);
			if(!dctx->flagRepeatTable)
				return ERROR(dictionary_corrupted);

		/* 2 - 2 - 10 - 10 */
		lhSize = 3;
		litSize  = ((istart[0] & 15) << 6) + (istart[1] >> 2);
		litCSize = ((istart[1] &  3) << 8) + istart[2];
		if(litCSize + lhSize > srcSize) 
			return ERROR(corruption_detected);
		{   
			const size_t errorCode = HUFv06_decompress1X4_usingDTable(dctx->litBuffer, litSize, istart+lhSize, litCSize, dctx->hufTableX4);
		    if(HUFv06_isError(errorCode)) return ERROR(corruption_detected); }
		dctx->litPtr = dctx->litBuffer;
		dctx->litSize = litSize;
		memzero(dctx->litBuffer + dctx->litSize, WILDCOPY_OVERLENGTH);
		return litCSize + lhSize;}
		case IS_RAW:
	    {   
		size_t litSize;
		uint32 lhSize = ((istart[0]) >> 4) & 3;
		switch(lhSize) {
			case 0: case 1: default: /* note : default is impossible, since lhSize into [0..3] */
			    lhSize = 1;
			    litSize = istart[0] & 31;
			    break;
			case 2:
			    litSize = ((istart[0] & 15) << 8) + istart[1];
			    break;
			case 3:
			    litSize = ((istart[0] & 15) << 16) + (istart[1] << 8) + istart[2];
			    break;
		}

		if(lhSize+litSize+WILDCOPY_OVERLENGTH > srcSize) { /* risk reading beyond src buffer with wildcopy */
			if(litSize+lhSize > srcSize) 
				return ERROR(corruption_detected);
			memcpy(dctx->litBuffer, istart+lhSize, litSize);
			dctx->litPtr = dctx->litBuffer;
			dctx->litSize = litSize;
			memzero(dctx->litBuffer + dctx->litSize, WILDCOPY_OVERLENGTH);
			return lhSize+litSize;
		}
		/* direct reference into compressed stream */
		dctx->litPtr = istart+lhSize;
		dctx->litSize = litSize;
		return lhSize+litSize;}
		case IS_RLE:
	    {   
		size_t litSize;
		uint32 lhSize = ((istart[0]) >> 4) & 3;
		switch(lhSize) {
			case 0: case 1: default: /* note : default is impossible, since lhSize into [0..3] */
			    lhSize = 1;
			    litSize = istart[0] & 31;
			    break;
			case 2:
			    litSize = ((istart[0] & 15) << 8) + istart[1];
			    break;
			case 3:
			    litSize = ((istart[0] & 15) << 16) + (istart[1] << 8) + istart[2];
			    if(srcSize<4) return ERROR(corruption_detected); /* srcSize >= MIN_CBLOCK_SIZE == 3; here we
				                                                need lhSize+1 = 4 */
			    break;
		}
		if(litSize > ZSTDv06_BLOCKSIZE_MAX) return ERROR(corruption_detected);
		memset(dctx->litBuffer, istart[lhSize], litSize + WILDCOPY_OVERLENGTH);
		dctx->litPtr = dctx->litBuffer;
		dctx->litSize = litSize;
		return lhSize+1;}
		default:
		    return ERROR(corruption_detected); /* impossible */
	}
}

/*! ZSTDv06_buildSeqTable() :
    @return : nb bytes read from src,
              or an error code if it fails, testable with ZSTDv06_isError()
 */
static size_t ZSTDv06_buildSeqTable(FSEv06_DTable* DTable, uint32 type, uint max, uint32 maxLog,
    const void* src, size_t srcSize, const int16* defaultNorm, uint32 defaultLog, uint32 flagRepeatTable)
{
	switch(type) {
		case FSEv06_ENCODING_RLE:
		    if(!srcSize) 
				return ERROR(srcSize_wrong);
		    if( (*(const BYTE *)src) > max) 
				return ERROR(corruption_detected);
		    FSEv06_buildDTable_rle(DTable, *(const BYTE *)src); /* if *src > max, data is corrupted */
		    return 1;
		case FSEv06_ENCODING_RAW:
		    FSEv06_buildDTable(DTable, defaultNorm, max, defaultLog);
		    return 0;
		case FSEv06_ENCODING_STATIC:
		    if(!flagRepeatTable) return ERROR(corruption_detected);
		    return 0;
		default: /* impossible */
		case FSEv06_ENCODING_DYNAMIC:
	    {   
			uint32 tableLog;
			int16 norm[MaxSeq+1];
			const size_t headerSize = FSEv06_readNCount(norm, &max, &tableLog, src, srcSize);
			if(FSEv06_isError(headerSize)) 
				return ERROR(corruption_detected);
			if(tableLog > maxLog) 
				return ERROR(corruption_detected);
			FSEv06_buildDTable(DTable, norm, max, tableLog);
			return headerSize;
		}
	}
}

static size_t ZSTDv06_decodeSeqHeaders(int* nbSeqPtr,
    FSEv06_DTable* DTableLL, FSEv06_DTable* DTableML, FSEv06_DTable* DTableOffb, uint32 flagRepeatTable,
    const void* src, size_t srcSize)
{
	const BYTE * const istart = PTR8C(src);
	const BYTE * const iend = istart + srcSize;
	const BYTE * ip = istart;

	/* check */
	if(srcSize < MIN_SEQUENCES_SIZE) return ERROR(srcSize_wrong);

	/* SeqHead */
	{   int nbSeq = *ip++;
	    if(!nbSeq) {
		    *nbSeqPtr = 0; return 1;
	    }
	    if(nbSeq > 0x7F) {
		    if(nbSeq == 0xFF) {
			    if(ip+2 > iend) return ERROR(srcSize_wrong);
			    nbSeq = SMem::GetLe16(ip) + LONGNBSEQ, ip += 2;
		    }
		    else {
			    if(ip >= iend) return ERROR(srcSize_wrong);
			    nbSeq = ((nbSeq-0x80)<<8) + *ip++;
		    }
	    }
	    *nbSeqPtr = nbSeq;}

	/* FSE table descriptors */
	if(ip + 4 > iend) return ERROR(srcSize_wrong); /* min : header byte + all 3 are "raw", hence no header, but at least xxLog bits per type */
	{   
		const uint32 LLtype  = *ip >> 6;
	    const uint32 Offtype = (*ip >> 4) & 3;
	    const uint32 MLtype  = (*ip >> 2) & 3;
	    ip++;
		/* Build DTables */
	    {   
			const size_t bhSize = ZSTDv06_buildSeqTable(DTableLL, LLtype, MaxLL, LLFSELog, ip, iend-ip, LL_defaultNorm, LL_defaultNormLog, flagRepeatTable);
			if(ZSTDv06_isError(bhSize)) 
				return ERROR(corruption_detected);
			ip += bhSize;
		}
	    {   
			const size_t bhSize = ZSTDv06_buildSeqTable(DTableOffb, Offtype, MaxOff, OffFSELog, ip, iend-ip, OF_defaultNorm, OF_defaultNormLog, flagRepeatTable);
			if(ZSTDv06_isError(bhSize)) 
				return ERROR(corruption_detected);
			ip += bhSize;
		}
	    {   
			const size_t bhSize = ZSTDv06_buildSeqTable(DTableML, MLtype, MaxML, MLFSELog, ip, iend-ip, ML_defaultNorm, ML_defaultNormLog, flagRepeatTable);
			if(ZSTDv06_isError(bhSize)) 
				return ERROR(corruption_detected);
			ip += bhSize;
		}   
	}
	return ip-istart;
}

typedef struct {
	size_t litLength;
	size_t matchLength;
	size_t offset;
} seq_t;

typedef struct {
	BITv06_DStream_t DStream;
	FSEv06_DState_t stateLL;
	FSEv06_DState_t stateOffb;
	FSEv06_DState_t stateML;
	size_t prevOffset[ZSTDv06_REP_INIT];
} seqState_t;

static void ZSTDv06_decodeSequence(seq_t* seq, seqState_t* seqState)
{
	/* Literal length */
	const uint32 llCode = FSEv06_peekSymbol(&(seqState->stateLL));
	const uint32 mlCode = FSEv06_peekSymbol(&(seqState->stateML));
	const uint32 ofCode = FSEv06_peekSymbol(&(seqState->stateOffb)); /* <= maxOff, by table construction */

	const uint32 llBits = LL_bits[llCode];
	const uint32 mlBits = ML_bits[mlCode];
	const uint32 ofBits = ofCode;
	const uint32 totalBits = llBits+mlBits+ofBits;

	static const uint32 LL_base[MaxLL+1] = {
		0,  1,  2,  3,  4,  5,  6,  7,  8,  9,   10,    11,    12,    13,    14,     15,
		16, 18, 20, 22, 24, 28, 32, 40, 48, 64, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000,
		0x2000, 0x4000, 0x8000, 0x10000
	};

	static const uint32 ML_base[MaxML+1] = {
		0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10,   11,    12,    13,    14,    15,
		16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,   27,    28,    29,    30,    31,
		32, 34, 36, 38, 40, 44, 48, 56, 64, 80, 96, 0x80, 0x100, 0x200, 0x400, 0x800,
		0x1000, 0x2000, 0x4000, 0x8000, 0x10000
	};

	static const uint32 OF_base[MaxOff+1] = {
		0,        1,       3,       7,     0xF,     0x1F,     0x3F,     0x7F,
		0xFF,   0x1FF,   0x3FF,   0x7FF,   0xFFF,   0x1FFF,   0x3FFF,   0x7FFF,
		0xFFFF, 0x1FFFF, 0x3FFFF, 0x7FFFF, 0xFFFFF, 0x1FFFFF, 0x3FFFFF, 0x7FFFFF,
		0xFFFFFF, 0x1FFFFFF, 0x3FFFFFF, /*fake*/ 1, 1
	};
	/* sequence */
	{   
		size_t offset;
	    if(!ofCode)
		    offset = 0;
	    else {
		    offset = OF_base[ofCode] + BITv06_readBits(&(seqState->DStream), ofBits); /* <=  26 bits */
		    if(MEM_32bits()) BITv06_reloadDStream(&(seqState->DStream));
	    }

	    if(offset < ZSTDv06_REP_NUM) {
		    if(llCode == 0 && offset <= 1) offset = 1-offset;

		    if(offset != 0) {
			    size_t temp = seqState->prevOffset[offset];
			    if(offset != 1) {
				    seqState->prevOffset[2] = seqState->prevOffset[1];
			    }
			    seqState->prevOffset[1] = seqState->prevOffset[0];
			    seqState->prevOffset[0] = offset = temp;
		    }
		    else {
			    offset = seqState->prevOffset[0];
		    }
	    }
	    else {
		    offset -= ZSTDv06_REP_MOVE;
		    seqState->prevOffset[2] = seqState->prevOffset[1];
		    seqState->prevOffset[1] = seqState->prevOffset[0];
		    seqState->prevOffset[0] = offset;
	    }
	    seq->offset = offset;}

	seq->matchLength = ML_base[mlCode] + MINMATCH + ((mlCode>31) ? BITv06_readBits(&(seqState->DStream), mlBits) : 0); /* <= 16 bits */
	if(MEM_32bits() && (mlBits+llBits>24)) BITv06_reloadDStream(&(seqState->DStream));
	seq->litLength = LL_base[llCode] + ((llCode>15) ? BITv06_readBits(&(seqState->DStream), llBits) : 0); /* <=  16 bits */
	if(MEM_32bits() ||
	    (totalBits > 64 - 7 - (LLFSELog+MLFSELog+OffFSELog)) ) BITv06_reloadDStream(&(seqState->DStream));

	/* ANS state update */
	FSEv06_updateState(&(seqState->stateLL), &(seqState->DStream)); /* <=  9 bits */
	FSEv06_updateState(&(seqState->stateML), &(seqState->DStream)); /* <=  9 bits */
	if(MEM_32bits()) BITv06_reloadDStream(&(seqState->DStream));  /* <= 18 bits */
	FSEv06_updateState(&(seqState->stateOffb), &(seqState->DStream)); /* <=  8 bits */
}

static size_t ZSTDv06_execSequence(BYTE * op,
    BYTE * const oend, seq_t sequence,
    const BYTE ** litPtr, const BYTE * const litLimit,
    const BYTE * const base, const BYTE * const vBase, const BYTE * const dictEnd)
{
	BYTE * const oLitEnd = op + sequence.litLength;
	const size_t sequenceLength = sequence.litLength + sequence.matchLength;
	BYTE * const oMatchEnd = op + sequenceLength; /* risk : address space overflow (32-bits) */
	BYTE * const oend_8 = oend-8;
	const BYTE * const iLitEnd = *litPtr + sequence.litLength;
	const BYTE * match = oLitEnd - sequence.offset;

	/* check */
	if(oLitEnd > oend_8) return ERROR(dstSize_tooSmall); /* last match must start at a minimum distance of 8 from oend */
	if(oMatchEnd > oend) return ERROR(dstSize_tooSmall); /* overwrite beyond dst buffer */
	if(iLitEnd > litLimit) return ERROR(corruption_detected); /* over-read beyond lit buffer */

	/* copy Literals */
	ZSTDv06_wildcopy(op, *litPtr, sequence.litLength); /* note : oLitEnd <= oend-8 : no risk of overwrite beyond oend */
	op = oLitEnd;
	*litPtr = iLitEnd; /* update for next sequence */

	/* copy Match */
	if(sequence.offset > (size_t)(oLitEnd - base)) {
		/* offset beyond prefix */
		if(sequence.offset > (size_t)(oLitEnd - vBase)) return ERROR(corruption_detected);
		match = dictEnd - (base-match);
		if(match + sequence.matchLength <= dictEnd) {
			memmove(oLitEnd, match, sequence.matchLength);
			return sequenceLength;
		}
		/* span extDict & currentPrefixSegment */
		{   const size_t length1 = dictEnd - match;
		    memmove(oLitEnd, match, length1);
		    op = oLitEnd + length1;
		    sequence.matchLength -= length1;
		    match = base;
		    if(op > oend_8 || sequence.matchLength < MINMATCH) {
			    while(op < oMatchEnd) *op++ = *match++;
			    return sequenceLength;
		    }
		}
	}
	/* Requirement: op <= oend_8 */

	/* match within prefix */
	if(sequence.offset < 8) {
		/* close range match, overlap */
		static const uint32 dec32table[] = { 0, 1, 2, 1, 4, 4, 4, 4 }; /* added */
		static const int dec64table[] = { 8, 8, 8, 7, 8, 9, 10, 11 }; /* subtracted */
		int const sub2 = dec64table[sequence.offset];
		op[0] = match[0];
		op[1] = match[1];
		op[2] = match[2];
		op[3] = match[3];
		match += dec32table[sequence.offset];
		ZSTDv06_copy4(op+4, match);
		match -= sub2;
	}
	else {
		ZSTDv06_copy8(op, match);
	}
	op += 8; match += 8;

	if(oMatchEnd > oend-(16-MINMATCH)) {
		if(op < oend_8) {
			ZSTDv06_wildcopy(op, match, oend_8 - op);
			match += oend_8 - op;
			op = oend_8;
		}
		while(op < oMatchEnd) *op++ = *match++;
	}
	else {
		ZSTDv06_wildcopy(op, match, (ptrdiff_t)sequence.matchLength-8); /* works even if matchLength < 8 */
	}
	return sequenceLength;
}

static size_t ZSTDv06_decompressSequences(ZSTDv06_DCtx* dctx,
    void* dst, size_t maxDstSize,
    const void* seqStart, size_t seqSize)
{
	const BYTE * ip = (const BYTE *)seqStart;
	const BYTE * const iend = ip + seqSize;
	BYTE * const ostart = (BYTE *)dst;
	BYTE * const oend = ostart + maxDstSize;
	BYTE * op = ostart;
	const BYTE * litPtr = dctx->litPtr;
	const BYTE * const litEnd = litPtr + dctx->litSize;
	FSEv06_DTable* DTableLL = dctx->LLTable;
	FSEv06_DTable* DTableML = dctx->MLTable;
	FSEv06_DTable* DTableOffb = dctx->OffTable;
	const BYTE * const base = (const BYTE *)(dctx->base);
	const BYTE * const vBase = (const BYTE *)(dctx->vBase);
	const BYTE * const dictEnd = (const BYTE *)(dctx->dictEnd);
	int nbSeq;

	/* Build Decoding Tables */
	{   const size_t seqHSize = ZSTDv06_decodeSeqHeaders(&nbSeq, DTableLL, DTableML, DTableOffb, dctx->flagRepeatTable, ip, seqSize);
	    if(ZSTDv06_isError(seqHSize)) return seqHSize;
	    ip += seqHSize;
	    dctx->flagRepeatTable = 0;}

	/* Regen sequences */
	if(nbSeq) {
		seq_t sequence;
		seqState_t seqState;
		memzero(&sequence, sizeof(sequence));
		sequence.offset = REPCODE_STARTVALUE;
		{ 
			for(uint32 i = 0; i<ZSTDv06_REP_INIT; i++) 
				seqState.prevOffset[i] = REPCODE_STARTVALUE; 
		}
		{ 
			const size_t errorCode = BITv06_initDStream(&(seqState.DStream), ip, iend-ip);
			if(ERR_isError(errorCode)) 
				return ERROR(corruption_detected); 
		}
		FSEv06_initDState(&(seqState.stateLL), &(seqState.DStream), DTableLL);
		FSEv06_initDState(&(seqState.stateOffb), &(seqState.DStream), DTableOffb);
		FSEv06_initDState(&(seqState.stateML), &(seqState.DStream), DTableML);
		for(; (BITv06_reloadDStream(&(seqState.DStream)) <= BITv06_DStream_completed) && nbSeq;) {
			nbSeq--;
			ZSTDv06_decodeSequence(&sequence, &seqState);

#if 0  /* debug */
			static BYTE * start = NULL;
			if(!start) 
				start = op;
			size_t pos = (size_t)(op-start);
			if((pos >= 5810037) && (pos < 5810400))
				printf("Dpos %6u :%5u literals & match %3u bytes at distance %6u \n", pos, (uint32)sequence.litLength, (uint32)sequence.matchLength, (uint32)sequence.offset);
#endif

			{   
				const size_t oneSeqSize = ZSTDv06_execSequence(op, oend, sequence, &litPtr, litEnd, base, vBase, dictEnd);
			    if(ZSTDv06_isError(oneSeqSize)) 
					return oneSeqSize;
			    op += oneSeqSize;
			}
		}
		/* check if reached exact end */
		if(nbSeq) 
			return ERROR(corruption_detected);
	}
	/* last literal segment */
	{   
		const size_t lastLLSize = litEnd - litPtr;
	    if(litPtr > litEnd) return ERROR(corruption_detected); /* too many literals already used */
	    if(op+lastLLSize > oend) return ERROR(dstSize_tooSmall);
	    if(lastLLSize > 0) {
		    memcpy(op, litPtr, lastLLSize);
		    op += lastLLSize;
	    }
	}
	return op-ostart;
}

static void ZSTDv06_checkContinuity(ZSTDv06_DCtx* dctx, const void* dst)
{
	if(dst != dctx->previousDstEnd) { /* not contiguous */
		dctx->dictEnd = dctx->previousDstEnd;
		dctx->vBase = (const char *)dst - ((const char *)(dctx->previousDstEnd) - (const char *)(dctx->base));
		dctx->base = dst;
		dctx->previousDstEnd = dst;
	}
}

static size_t ZSTDv06_decompressBlock_internal(ZSTDv06_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{   /* blockType == blockCompressed */
	const BYTE * ip = PTR8C(src);
	if(srcSize >= ZSTDv06_BLOCKSIZE_MAX) 
		return ERROR(srcSize_wrong);
	/* Decode literals sub-block */
	{   
		const size_t litCSize = ZSTDv06_decodeLiteralsBlock(dctx, src, srcSize);
	    if(ZSTDv06_isError(litCSize)) return litCSize;
	    ip += litCSize;
	    srcSize -= litCSize;
	}
	return ZSTDv06_decompressSequences(dctx, dst, dstCapacity, ip, srcSize);
}

size_t ZSTDv06_decompressBlock(ZSTDv06_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
	ZSTDv06_checkContinuity(dctx, dst);
	return ZSTDv06_decompressBlock_internal(dctx, dst, dstCapacity, src, srcSize);
}

/*! ZSTDv06_decompressFrame() :
 *   `dctx` must be properly initialized */
static size_t ZSTDv06_decompressFrame(ZSTDv06_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
	const BYTE * ip = PTR8C(src);
	const BYTE * const iend = ip + srcSize;
	BYTE * const ostart = (BYTE *)dst;
	BYTE * op = ostart;
	BYTE * const oend = ostart + dstCapacity;
	size_t remainingSize = srcSize;
	blockProperties_t blockProperties = { bt_compressed, 0 };
	/* check */
	if(srcSize < ZSTDv06_frameHeaderSize_min+ZSTDv06_blockHeaderSize) 
		return ERROR(srcSize_wrong);
	/* Frame Header */
	{   
		const size_t frameHeaderSize = ZSTDv06_frameHeaderSize(src, ZSTDv06_frameHeaderSize_min);
	    if(ZSTDv06_isError(frameHeaderSize)) return frameHeaderSize;
	    if(srcSize < frameHeaderSize+ZSTDv06_blockHeaderSize) return ERROR(srcSize_wrong);
	    if(ZSTDv06_decodeFrameHeader(dctx, src, frameHeaderSize)) return ERROR(corruption_detected);
	    ip += frameHeaderSize; remainingSize -= frameHeaderSize;
	}
	/* Loop on each block */
	while(1) {
		size_t decodedSize = 0;
		const size_t cBlockSize = ZSTDv06_getcBlockSize(ip, iend-ip, &blockProperties);
		if(ZSTDv06_isError(cBlockSize)) 
			return cBlockSize;
		ip += ZSTDv06_blockHeaderSize;
		remainingSize -= ZSTDv06_blockHeaderSize;
		if(cBlockSize > remainingSize) 
			return ERROR(srcSize_wrong);
		switch(blockProperties.blockType) {
			case bt_compressed:
			    decodedSize = ZSTDv06_decompressBlock_internal(dctx, op, oend-op, ip, cBlockSize);
			    break;
			case bt_raw:
			    decodedSize = ZSTDv06_copyRawBlock(op, oend-op, ip, cBlockSize);
			    break;
			case bt_rle:
			    return ERROR(GENERIC); /* not yet supported */
			    break;
			case bt_end:
			    /* end of frame */
			    if(remainingSize) return ERROR(srcSize_wrong);
			    break;
			default:
			    return ERROR(GENERIC); /* impossible */
		}
		if(cBlockSize == 0) 
			break; /* bt_end */
		if(ZSTDv06_isError(decodedSize)) 
			return decodedSize;
		op += decodedSize;
		ip += cBlockSize;
		remainingSize -= cBlockSize;
	}
	return op-ostart;
}

size_t ZSTDv06_decompress_usingPreparedDCtx(ZSTDv06_DCtx* dctx, const ZSTDv06_DCtx* refDCtx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
	ZSTDv06_copyDCtx(dctx, refDCtx);
	ZSTDv06_checkContinuity(dctx, dst);
	return ZSTDv06_decompressFrame(dctx, dst, dstCapacity, src, srcSize);
}

size_t ZSTDv06_decompress_usingDict(ZSTDv06_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize, const void* dict, size_t dictSize)
{
	ZSTDv06_decompressBegin_usingDict(dctx, dict, dictSize);
	ZSTDv06_checkContinuity(dctx, dst);
	return ZSTDv06_decompressFrame(dctx, dst, dstCapacity, src, srcSize);
}

size_t ZSTDv06_decompressDCtx(ZSTDv06_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
	return ZSTDv06_decompress_usingDict(dctx, dst, dstCapacity, src, srcSize, NULL, 0);
}

size_t ZSTDv06_decompress(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
#if defined(ZSTDv06_HEAPMODE) && (ZSTDv06_HEAPMODE==1)
	size_t regenSize;
	ZSTDv06_DCtx* dctx = ZSTDv06_createDCtx();
	if(!dctx) 
		return ERROR(memory_allocation);
	regenSize = ZSTDv06_decompressDCtx(dctx, dst, dstCapacity, src, srcSize);
	ZSTDv06_freeDCtx(dctx);
	return regenSize;
#else   /* stack mode */
	ZSTDv06_DCtx dctx;
	return ZSTDv06_decompressDCtx(&dctx, dst, dstCapacity, src, srcSize);
#endif
}

/* ZSTD_errorFrameSizeInfoLegacy() :
   assumes `cSize` and `dBound` are _not_ NULL */
static void ZSTD_errorFrameSizeInfoLegacy(size_t* cSize, uint64* dBound, size_t ret)
{
	*cSize = ret;
	*dBound = ZSTD_CONTENTSIZE_ERROR;
}

void ZSTDv06_findFrameSizeInfoLegacy(const void * src, size_t srcSize, size_t* cSize, uint64* dBound)
{
	const BYTE * ip = PTR8C(src);
	size_t remainingSize = srcSize;
	size_t nbBlocks = 0;
	blockProperties_t blockProperties = { bt_compressed, 0 };

	/* Frame Header */
	{   const size_t frameHeaderSize = ZSTDv06_frameHeaderSize(src, srcSize);
	    if(ZSTDv06_isError(frameHeaderSize)) {
		    ZSTD_errorFrameSizeInfoLegacy(cSize, dBound, frameHeaderSize);
		    return;
	    }
	    if(SMem::GetLe32(src) != ZSTDv06_MAGICNUMBER) {
		    ZSTD_errorFrameSizeInfoLegacy(cSize, dBound, ERROR(prefix_unknown));
		    return;
	    }
	    if(srcSize < frameHeaderSize+ZSTDv06_blockHeaderSize) {
		    ZSTD_errorFrameSizeInfoLegacy(cSize, dBound, ERROR(srcSize_wrong));
		    return;
	    }
	    ip += frameHeaderSize; remainingSize -= frameHeaderSize;}

	/* Loop on each block */
	while(1) {
		const size_t cBlockSize = ZSTDv06_getcBlockSize(ip, remainingSize, &blockProperties);
		if(ZSTDv06_isError(cBlockSize)) {
			ZSTD_errorFrameSizeInfoLegacy(cSize, dBound, cBlockSize);
			return;
		}

		ip += ZSTDv06_blockHeaderSize;
		remainingSize -= ZSTDv06_blockHeaderSize;
		if(cBlockSize > remainingSize) {
			ZSTD_errorFrameSizeInfoLegacy(cSize, dBound, ERROR(srcSize_wrong));
			return;
		}

		if(cBlockSize == 0) break; /* bt_end */

		ip += cBlockSize;
		remainingSize -= cBlockSize;
		nbBlocks++;
	}

	*cSize = ip - (const BYTE *)src;
	*dBound = nbBlocks * ZSTDv06_BLOCKSIZE_MAX;
}

/*_******************************
*  Streaming Decompression API
********************************/
size_t ZSTDv06_nextSrcSizeToDecompress(ZSTDv06_DCtx* dctx)
{
	return dctx->expected;
}

size_t ZSTDv06_decompressContinue(ZSTDv06_DCtx* dctx, void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
	/* Sanity check */
	if(srcSize != dctx->expected) 
		return ERROR(srcSize_wrong);
	if(dstCapacity) 
		ZSTDv06_checkContinuity(dctx, dst);
	/* Decompress : frame header; part 1 */
	switch(dctx->stage) {
		case ZSTDds_getFrameHeaderSize:
		    if(srcSize != ZSTDv06_frameHeaderSize_min) 
				return ERROR(srcSize_wrong); /* impossible */
		    dctx->headerSize = ZSTDv06_frameHeaderSize(src, ZSTDv06_frameHeaderSize_min);
		    if(ZSTDv06_isError(dctx->headerSize)) 
				return dctx->headerSize;
		    memcpy(dctx->headerBuffer, src, ZSTDv06_frameHeaderSize_min);
		    if(dctx->headerSize > ZSTDv06_frameHeaderSize_min) {
			    dctx->expected = dctx->headerSize - ZSTDv06_frameHeaderSize_min;
			    dctx->stage = ZSTDds_decodeFrameHeader;
			    return 0;
		    }
		    dctx->expected = 0; /* not necessary to copy more */
			// @fallthrough
		case ZSTDds_decodeFrameHeader:
			{   
				size_t result;
				memcpy(dctx->headerBuffer + ZSTDv06_frameHeaderSize_min, src, dctx->expected);
				result = ZSTDv06_decodeFrameHeader(dctx, dctx->headerBuffer, dctx->headerSize);
				if(ZSTDv06_isError(result)) 
					return result;
				dctx->expected = ZSTDv06_blockHeaderSize;
				dctx->stage = ZSTDds_decodeBlockHeader;
				return 0;
			}
		case ZSTDds_decodeBlockHeader:
			{   
				blockProperties_t bp;
				const size_t cBlockSize = ZSTDv06_getcBlockSize(src, ZSTDv06_blockHeaderSize, &bp);
				if(ZSTDv06_isError(cBlockSize)) 
					return cBlockSize;
				if(bp.blockType == bt_end) {
					dctx->expected = 0;
					dctx->stage = ZSTDds_getFrameHeaderSize;
				}
				else {
					dctx->expected = cBlockSize;
					dctx->bType = bp.blockType;
					dctx->stage = ZSTDds_decompressBlock;
				}
				return 0;
			}
		case ZSTDds_decompressBlock:
			{   
				size_t rSize;
				switch(dctx->bType) {
					case bt_compressed:
						rSize = ZSTDv06_decompressBlock_internal(dctx, dst, dstCapacity, src, srcSize);
						break;
					case bt_raw:
						rSize = ZSTDv06_copyRawBlock(dst, dstCapacity, src, srcSize);
						break;
					case bt_rle:
						return ERROR(GENERIC); /* not yet handled */
						break;
					case bt_end: /* should never happen (filtered at phase 1) */
						rSize = 0;
						break;
					default:
						return ERROR(GENERIC); /* impossible */
				}
				dctx->stage = ZSTDds_decodeBlockHeader;
				dctx->expected = ZSTDv06_blockHeaderSize;
				dctx->previousDstEnd = (char *)dst + rSize;
				return rSize;
			}
		default:
		    return ERROR(GENERIC); /* impossible */
	}
}

static void ZSTDv06_refDictContent(ZSTDv06_DCtx* dctx, const void* dict, size_t dictSize)
{
	dctx->dictEnd = dctx->previousDstEnd;
	dctx->vBase = (const char *)dict - ((const char *)(dctx->previousDstEnd) - (const char *)(dctx->base));
	dctx->base = dict;
	dctx->previousDstEnd = (const char *)dict + dictSize;
}

static size_t ZSTDv06_loadEntropy(ZSTDv06_DCtx* dctx, const void* dict, size_t dictSize)
{
	size_t offcodeHeaderSize, matchlengthHeaderSize, litlengthHeaderSize;
	size_t hSize = HUFv06_readDTableX4(dctx->hufTableX4, dict, dictSize);
	if(HUFv06_isError(hSize)) 
		return ERROR(dictionary_corrupted);
	dict = (const char *)dict + hSize;
	dictSize -= hSize;
	{   
		short offcodeNCount[MaxOff+1];
	    uint  offcodeMaxValue = MaxOff;
		uint32 offcodeLog;
	    offcodeHeaderSize = FSEv06_readNCount(offcodeNCount, &offcodeMaxValue, &offcodeLog, dict, dictSize);
	    if(FSEv06_isError(offcodeHeaderSize)) 
			return ERROR(dictionary_corrupted);
	    if(offcodeLog > OffFSELog) return ERROR(dictionary_corrupted);
	    { 
			const size_t errorCode = FSEv06_buildDTable(dctx->OffTable, offcodeNCount, offcodeMaxValue, offcodeLog);
			if(FSEv06_isError(errorCode)) 
				return ERROR(dictionary_corrupted); 
		}
	    dict = (const char *)dict + offcodeHeaderSize;
	    dictSize -= offcodeHeaderSize;
	}
	{   
		short  matchlengthNCount[MaxML+1];
	    uint   matchlengthMaxValue = MaxML;
		uint32 matchlengthLog;
	    matchlengthHeaderSize = FSEv06_readNCount(matchlengthNCount, &matchlengthMaxValue, &matchlengthLog, dict, dictSize);
	    if(FSEv06_isError(matchlengthHeaderSize)) return ERROR(dictionary_corrupted);
	    if(matchlengthLog > MLFSELog) 
			return ERROR(dictionary_corrupted);
	    { 
			const size_t errorCode = FSEv06_buildDTable(dctx->MLTable, matchlengthNCount, matchlengthMaxValue, matchlengthLog);
			if(FSEv06_isError(errorCode)) 
				return ERROR(dictionary_corrupted); 
		}
	    dict = (const char *)dict + matchlengthHeaderSize;
	    dictSize -= matchlengthHeaderSize;
	}
	{   
		short litlengthNCount[MaxLL+1];
	    uint litlengthMaxValue = MaxLL;
		uint32 litlengthLog;
	    litlengthHeaderSize = FSEv06_readNCount(litlengthNCount, &litlengthMaxValue, &litlengthLog, dict, dictSize);
	    if(FSEv06_isError(litlengthHeaderSize)) return ERROR(dictionary_corrupted);
	    if(litlengthLog > LLFSELog) 
			return ERROR(dictionary_corrupted);
	    { 
			const size_t errorCode = FSEv06_buildDTable(dctx->LLTable, litlengthNCount, litlengthMaxValue, litlengthLog);
			if(FSEv06_isError(errorCode)) 
				return ERROR(dictionary_corrupted); 
		}
	}
	dctx->flagRepeatTable = 1;
	return hSize + offcodeHeaderSize + matchlengthHeaderSize + litlengthHeaderSize;
}

static size_t ZSTDv06_decompress_insertDictionary(ZSTDv06_DCtx* dctx, const void* dict, size_t dictSize)
{
	size_t eSize;
	const uint32 magic = SMem::GetLe32(dict);
	if(magic != ZSTDv06_DICT_MAGIC) {
		/* pure content mode */
		ZSTDv06_refDictContent(dctx, dict, dictSize);
		return 0;
	}
	/* load entropy tables */
	dict = (const char *)dict + 4;
	dictSize -= 4;
	eSize = ZSTDv06_loadEntropy(dctx, dict, dictSize);
	if(ZSTDv06_isError(eSize)) return ERROR(dictionary_corrupted);

	/* reference dictionary content */
	dict = (const char *)dict + eSize;
	dictSize -= eSize;
	ZSTDv06_refDictContent(dctx, dict, dictSize);

	return 0;
}

size_t ZSTDv06_decompressBegin_usingDict(ZSTDv06_DCtx* dctx, const void* dict, size_t dictSize)
{
	{ const size_t errorCode = ZSTDv06_decompressBegin(dctx);
	  if(ZSTDv06_isError(errorCode)) return errorCode; }

	if(dict && dictSize) {
		const size_t errorCode = ZSTDv06_decompress_insertDictionary(dctx, dict, dictSize);
		if(ZSTDv06_isError(errorCode)) return ERROR(dictionary_corrupted);
	}

	return 0;
}
/*
    Buffered version of Zstd compression library
    Copyright (C) 2015-2016, Yann Collet.

    BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:
 * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

    You can contact the author at :
    - zstd homepage : http://www.zstd.net/
 */

/*-***************************************************************************
 *  Streaming decompression howto
 *
 *  A ZBUFFv06_DCtx object is required to track streaming operations.
 *  Use ZBUFFv06_createDCtx() and ZBUFFv06_freeDCtx() to create/release resources.
 *  Use ZBUFFv06_decompressInit() to start a new decompression operation,
 *   or ZBUFFv06_decompressInitDictionary() if decompression requires a dictionary.
 *  Note that ZBUFFv06_DCtx objects can be re-init multiple times.
 *
 *  Use ZBUFFv06_decompressContinue() repetitively to consume your input.
 *  *srcSizePtr and *dstCapacityPtr can be any size.
 *  The function will report how many bytes were read or written by modifying *srcSizePtr and *dstCapacityPtr.
 *  Note that it may not consume the entire input, in which case it's up to the caller to present remaining input again.
 *  The content of @dst will be overwritten (up to *dstCapacityPtr) at each function call, so save its content if it
 *matters, or change @dst.
 *  @return : a hint to preferred nb of bytes to use as input for next function call (it's only a hint, to help
 *latency),
 *            or 0 when a frame is completely decoded,
 *            or an error code, which can be tested using ZBUFFv06_isError().
 *
 *  Hint : recommended buffer sizes (not compulsory) : ZBUFFv06_recommendedDInSize() and ZBUFFv06_recommendedDOutSize()
 *  output : ZBUFFv06_recommendedDOutSize==128 KB block size is the internal unit, it ensures it's always possible to
 *write a full block when decoded.
 *  input  : ZBUFFv06_recommendedDInSize == 128KB + 3;
 *           just follow indications from ZBUFFv06_decompressContinue() to minimize latency. It should always be <= 128
 *KB + 3 .
 * *******************************************************************************/

typedef enum { 
	ZBUFFds_init, 
	ZBUFFds_loadHeader, 
	ZBUFFds_read, 
	ZBUFFds_load, 
	ZBUFFds_flush 
} ZBUFFv06_dStage;

/* *** Resource management *** */
struct ZBUFFv06_DCtx_s {
	ZSTDv06_DCtx* zd;
	ZSTDv06_frameParams fParams;
	ZBUFFv06_dStage stage;
	char*  inBuff;
	size_t inBuffSize;
	size_t inPos;
	char*  outBuff;
	size_t outBuffSize;
	size_t outStart;
	size_t outEnd;
	size_t blockSize;
	BYTE headerBuffer[ZSTDv06_FRAMEHEADERSIZE_MAX];
	size_t lhSize;
};   /* typedef'd to ZBUFFv06_DCtx within "zstd_buffered.h" */

ZBUFFv06_DCtx* ZBUFFv06_createDCtx(void)
{
	ZBUFFv06_DCtx* zbd = (ZBUFFv06_DCtx*)SAlloc::M(sizeof(ZBUFFv06_DCtx));
	if(!zbd)
		return NULL;
	memzero(zbd, sizeof(*zbd));
	zbd->zd = ZSTDv06_createDCtx();
	zbd->stage = ZBUFFds_init;
	return zbd;
}

size_t ZBUFFv06_freeDCtx(ZBUFFv06_DCtx* zbd)
{
	if(!zbd) 
		return 0; /* support free on null */
	ZSTDv06_freeDCtx(zbd->zd);
	SAlloc::F(zbd->inBuff);
	SAlloc::F(zbd->outBuff);
	SAlloc::F(zbd);
	return 0;
}

/* *** Initialization *** */

size_t ZBUFFv06_decompressInitDictionary(ZBUFFv06_DCtx* zbd, const void* dict, size_t dictSize)
{
	zbd->stage = ZBUFFds_loadHeader;
	zbd->lhSize = zbd->inPos = zbd->outStart = zbd->outEnd = 0;
	return ZSTDv06_decompressBegin_usingDict(zbd->zd, dict, dictSize);
}

size_t ZBUFFv06_decompressInit(ZBUFFv06_DCtx* zbd)
{
	return ZBUFFv06_decompressInitDictionary(zbd, NULL, 0);
}

MEM_STATIC size_t ZBUFFv06_limitCopy(void* dst, size_t dstCapacity, const void* src, size_t srcSize)
{
	size_t length = MIN(dstCapacity, srcSize);
	if(length > 0) {
		memcpy(dst, src, length);
	}
	return length;
}

/* *** Decompression *** */

size_t ZBUFFv06_decompressContinue(ZBUFFv06_DCtx* zbd, void* dst, size_t* dstCapacityPtr, const void* src, size_t* srcSizePtr)
{
	const char* const istart = (const char *)src;
	const char* const iend = istart + *srcSizePtr;
	const char* ip = istart;
	char* const ostart = (char *)dst;
	char* const oend = ostart + *dstCapacityPtr;
	char* op = ostart;
	uint32 notDone = 1;
	while(notDone) {
		switch(zbd->stage) {
			case ZBUFFds_init:
			    return ERROR(init_missing);
			case ZBUFFds_loadHeader:
		    {   
				const size_t hSize = ZSTDv06_getFrameParams(&(zbd->fParams), zbd->headerBuffer, zbd->lhSize);
			if(hSize != 0) {
				const size_t toLoad = hSize - zbd->lhSize; /* if hSize!=0, hSize > zbd->lhSize */
				if(ZSTDv06_isError(hSize)) return hSize;
				if(toLoad > (size_t)(iend-ip)) { /* not enough input to load full header */
					memcpy(zbd->headerBuffer + zbd->lhSize, ip, iend-ip);
					zbd->lhSize += iend-ip;
					*dstCapacityPtr = 0;
					return (hSize - zbd->lhSize) + ZSTDv06_blockHeaderSize; /* remaining header bytes + next block header */
				}
				memcpy(zbd->headerBuffer + zbd->lhSize, ip, toLoad); zbd->lhSize = hSize; ip += toLoad;
				break;
			}
		    }

			    /* Consume header */
			    {   
					const size_t h1Size = ZSTDv06_nextSrcSizeToDecompress(zbd->zd);/* == ZSTDv06_frameHeaderSize_min */
				const size_t h1Result = ZSTDv06_decompressContinue(zbd->zd, NULL, 0, zbd->headerBuffer, h1Size);
				if(ZSTDv06_isError(h1Result)) return h1Result;
				if(h1Size < zbd->lhSize) { /* long header */
					const size_t h2Size = ZSTDv06_nextSrcSizeToDecompress(zbd->zd);
					const size_t h2Result = ZSTDv06_decompressContinue(zbd->zd,
						NULL,
						0,
						zbd->headerBuffer+h1Size,
						h2Size);
					if(ZSTDv06_isError(h2Result)) return h2Result;
				}
			    }

			    /* Frame header instruct buffer sizes */
			    {   const size_t blockSize = MIN(1 << zbd->fParams.windowLog, ZSTDv06_BLOCKSIZE_MAX);
				zbd->blockSize = blockSize;
				if(zbd->inBuffSize < blockSize) {
					SAlloc::F(zbd->inBuff);
					zbd->inBuffSize = blockSize;
					zbd->inBuff = (char *)SAlloc::M(blockSize);
					if(zbd->inBuff == NULL) return ERROR(memory_allocation);
				}
				{   
					const size_t neededOutSize = ((size_t)1 << zbd->fParams.windowLog) + blockSize + WILDCOPY_OVERLENGTH * 2;
				    if(zbd->outBuffSize < neededOutSize) {
					    SAlloc::F(zbd->outBuff);
					    zbd->outBuffSize = neededOutSize;
					    zbd->outBuff = (char *)SAlloc::M(neededOutSize);
					    if(zbd->outBuff == NULL) return ERROR(memory_allocation);
				    }
				}   }
			    zbd->stage = ZBUFFds_read;
			// @fallthrough
			case ZBUFFds_read:
		    {   const size_t neededInSize = ZSTDv06_nextSrcSizeToDecompress(zbd->zd);
			if(neededInSize==0) { /* end of frame */
				zbd->stage = ZBUFFds_init;
				notDone = 0;
				break;
			}
			if((size_t)(iend-ip) >= neededInSize) { /* decode directly from src */
				const size_t decodedSize = ZSTDv06_decompressContinue(zbd->zd, zbd->outBuff + zbd->outStart, zbd->outBuffSize - zbd->outStart, ip, neededInSize);
				if(ZSTDv06_isError(decodedSize)) 
					return decodedSize;
				ip += neededInSize;
				if(!decodedSize) 
					break; /* this was just a header */
				zbd->outEnd = zbd->outStart +  decodedSize;
				zbd->stage = ZBUFFds_flush;
				break;
			}
			if(ip==iend) {
				notDone = 0; break;
			} /* no more input */
			zbd->stage = ZBUFFds_load;}
			// @fallthrough
			case ZBUFFds_load:
		    {   
				const size_t neededInSize = ZSTDv06_nextSrcSizeToDecompress(zbd->zd);
				const size_t toLoad = neededInSize - zbd->inPos; /* should always be <= remaining space within inBuff */
				size_t loadedSize;
				if(toLoad > zbd->inBuffSize - zbd->inPos) 
					return ERROR(corruption_detected); /* should never happen */
				loadedSize = ZBUFFv06_limitCopy(zbd->inBuff + zbd->inPos, toLoad, ip, iend-ip);
				ip += loadedSize;
				zbd->inPos += loadedSize;
				if(loadedSize < toLoad) {
					notDone = 0; 
					break;
				} /* not enough input, wait for more */
				/* decode loaded input */
				{   
					const size_t decodedSize = ZSTDv06_decompressContinue(zbd->zd, zbd->outBuff + zbd->outStart, zbd->outBuffSize - zbd->outStart, zbd->inBuff, neededInSize);
					if(ZSTDv06_isError(decodedSize)) 
						return decodedSize;
					zbd->inPos = 0; /* input is consumed */
					if(!decodedSize) {
						zbd->stage = ZBUFFds_read; 
						break;
					} /* this was just a header */
					zbd->outEnd = zbd->outStart +  decodedSize;
					zbd->stage = ZBUFFds_flush;
					/* break; */ /* ZBUFFds_flush follows */
				}
			}
			// @fallthrough
			case ZBUFFds_flush:
		    {   const size_t toFlushSize = zbd->outEnd - zbd->outStart;
			const size_t flushedSize = ZBUFFv06_limitCopy(op, oend-op, zbd->outBuff + zbd->outStart, toFlushSize);
			op += flushedSize;
			zbd->outStart += flushedSize;
			if(flushedSize == toFlushSize) {
				zbd->stage = ZBUFFds_read;
				if(zbd->outStart + zbd->blockSize > zbd->outBuffSize)
					zbd->outStart = zbd->outEnd = 0;
				break;
			}
			/* cannot flush everything */
			notDone = 0;
			break;}
			default: return ERROR(GENERIC); /* impossible */
		}
	}

	/* result */
	*srcSizePtr = ip-istart;
	*dstCapacityPtr = op-ostart;
	{   
		size_t nextSrcSizeHint = ZSTDv06_nextSrcSizeToDecompress(zbd->zd);
	    if(nextSrcSizeHint > ZSTDv06_blockHeaderSize)
			nextSrcSizeHint += ZSTDv06_blockHeaderSize; // get following block header too 
	    nextSrcSizeHint -= zbd->inPos; /* already loaded*/
	    return nextSrcSizeHint;
	}
}
// 
// Tool functions
//
size_t ZBUFFv06_recommendedDInSize(void)  { return ZSTDv06_BLOCKSIZE_MAX + ZSTDv06_blockHeaderSize /* block header size*/; }
size_t ZBUFFv06_recommendedDOutSize(void) { return ZSTDv06_BLOCKSIZE_MAX; }
