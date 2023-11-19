/* ******************************************************************
* hist : Histogram functions
* part of Finite State Entropy project
* Copyright (c) Yann Collet, Facebook, Inc.
*
*  You can contact the author at :
*  - FSE source repository : https://github.com/Cyan4973/FiniteStateEntropy
*  - Public forum : https://groups.google.com/forum/#!forum/lz4c
*
* This source code is licensed under both the BSD-style license (found in the
* LICENSE file in the root directory of this source tree) and the GPLv2 (found
* in the COPYING file in the root directory of this source tree).
* You may select, at your option, one of the above-listed licenses.
****************************************************************** */

#include <zstd-internal.h>
#pragma hdrstop
#include <zstd_mem.h> // uint32, BYTE, etc.
#include <debug.h>           /* assert, DEBUGLOG */
#include <error_private.h>   /* ERROR */
#include "hist.h"

/* --- Error management --- */
uint HIST_isError(size_t code) { return ERR_isError(code); }
// 
// Histogram functions
// 
uint HIST_count_simple(uint * count, uint * maxSymbolValuePtr, const void * src, size_t srcSize)
{
	const BYTE * ip = PTR8C(src);
	const BYTE * const end = ip + srcSize;
	uint maxSymbolValue = *maxSymbolValuePtr;
	uint largestCount = 0;
	memzero(count, (maxSymbolValue+1) * sizeof(*count));
	if(srcSize==0) {
		*maxSymbolValuePtr = 0; return 0;
	}
	while(ip<end) {
		assert(*ip <= maxSymbolValue);
		count[*ip++]++;
	}
	while(!count[maxSymbolValue]) 
		maxSymbolValue--;
	*maxSymbolValuePtr = maxSymbolValue;
	{   
		uint32 s;
	    for(s = 0; s<=maxSymbolValue; s++)
		    if(count[s] > largestCount) largestCount = count[s]; 
	}
	return largestCount;
}

typedef enum { trustInput, checkMaxSymbolValue } HIST_checkInput_e;

/* HIST_count_parallel_wksp() :
 * store histogram into 4 intermediate tables, recombined at the end.
 * this design makes better use of OoO cpus,
 * and is noticeably faster when some values are heavily repeated.
 * But it needs some additional workspace for intermediate tables.
 * `workSpace` must be a uint32 table of size >= HIST_WKSP_SIZE_U32.
 * @return : largest histogram frequency,
 *           or an error code (notably when histogram's alphabet is larger than *maxSymbolValuePtr) */
static size_t HIST_count_parallel_wksp(uint * count, uint * maxSymbolValuePtr,
    const void * source, size_t sourceSize, HIST_checkInput_e check, uint32 * const workSpace)
{
	const BYTE * ip = (const BYTE *)source;
	const BYTE * const iend = ip+sourceSize;
	const size_t countSize = (*maxSymbolValuePtr + 1) * sizeof(*count);
	uint max = 0;
	uint32 * const Counting1 = workSpace;
	uint32 * const Counting2 = Counting1 + 256;
	uint32 * const Counting3 = Counting2 + 256;
	uint32 * const Counting4 = Counting3 + 256;

	/* safety checks */
	assert(*maxSymbolValuePtr <= 255);
	if(!sourceSize) {
		memzero(count, countSize);
		*maxSymbolValuePtr = 0;
		return 0;
	}
	memzero(workSpace, 4*256*sizeof(uint));
	/* by stripes of 16 bytes */
	{   
		uint32 cached = SMem::Get32(ip); 
		ip += 4;
	    while(ip < iend-15) {
		    uint32 c = cached; cached = SMem::Get32(ip); ip += 4;
		    Counting1[(BYTE)c     ]++;
		    Counting2[(BYTE)(c>>8) ]++;
		    Counting3[(BYTE)(c>>16)]++;
		    Counting4[       c>>24 ]++;
		    c = cached; cached = SMem::Get32(ip); ip += 4;
		    Counting1[(BYTE)c     ]++;
		    Counting2[(BYTE)(c>>8) ]++;
		    Counting3[(BYTE)(c>>16)]++;
		    Counting4[       c>>24 ]++;
		    c = cached; cached = SMem::Get32(ip); ip += 4;
		    Counting1[(BYTE)c     ]++;
		    Counting2[(BYTE)(c>>8) ]++;
		    Counting3[(BYTE)(c>>16)]++;
		    Counting4[       c>>24 ]++;
		    c = cached; cached = SMem::Get32(ip); ip += 4;
		    Counting1[(BYTE)c     ]++;
		    Counting2[(BYTE)(c>>8) ]++;
		    Counting3[(BYTE)(c>>16)]++;
		    Counting4[       c>>24 ]++;
	    }
	    ip -= 4;
	}
	/* finish last symbols */
	while(ip<iend) 
		Counting1[*ip++]++;
	{	
	    for(uint32 s = 0; s<256; s++) {
		    Counting1[s] += Counting2[s] + Counting3[s] + Counting4[s];
		    if(Counting1[s] > max) max = Counting1[s];
	    }
	}
	{   
		uint maxSymbolValue = 255;
	    while(!Counting1[maxSymbolValue]) maxSymbolValue--;
	    if(check && maxSymbolValue > *maxSymbolValuePtr) return ERROR(maxSymbolValue_tooSmall);
	    *maxSymbolValuePtr = maxSymbolValue;
	    memmove(count, Counting1, countSize); /* in case count & Counting1 are overlapping */
	}
	return (size_t)max;
}

/* HIST_countFast_wksp() :
 * Same as HIST_countFast(), but using an externally provided scratch buffer.
 * `workSpace` is a writable buffer which must be 4-bytes aligned,
 * `workSpaceSize` must be >= HIST_WKSP_SIZE
 */
size_t HIST_countFast_wksp(uint * count, uint * maxSymbolValuePtr, const void * source, size_t sourceSize, void * workSpace, size_t workSpaceSize)
{
	if(sourceSize < 1500) /* heuristic threshold */
		return HIST_count_simple(count, maxSymbolValuePtr, source, sourceSize);
	if((size_t)workSpace & 3) return ERROR(GENERIC); /* must be aligned on 4-bytes boundaries */
	if(workSpaceSize < HIST_WKSP_SIZE) return ERROR(workSpace_tooSmall);
	return HIST_count_parallel_wksp(count, maxSymbolValuePtr, source, sourceSize, trustInput, (uint32 *)workSpace);
}

/* HIST_count_wksp() :
 * Same as HIST_count(), but using an externally provided scratch buffer.
 * `workSpace` size must be table of >= HIST_WKSP_SIZE_U32 unsigned */
size_t HIST_count_wksp(uint * count, uint * maxSymbolValuePtr, const void * source, size_t sourceSize, void * workSpace, size_t workSpaceSize)
{
	if((size_t)workSpace & 3) return ERROR(GENERIC); /* must be aligned on 4-bytes boundaries */
	if(workSpaceSize < HIST_WKSP_SIZE) return ERROR(workSpace_tooSmall);
	if(*maxSymbolValuePtr < 255)
		return HIST_count_parallel_wksp(count, maxSymbolValuePtr, source, sourceSize, checkMaxSymbolValue, (uint32 *)workSpace);
	*maxSymbolValuePtr = 255;
	return HIST_countFast_wksp(count, maxSymbolValuePtr, source, sourceSize, workSpace, workSpaceSize);
}

#ifndef ZSTD_NO_UNUSED_FUNCTIONS
/* fast variant (unsafe : won't check if src contains values beyond count[] limit) */
size_t HIST_countFast(uint * count, uint * maxSymbolValuePtr, const void * source, size_t sourceSize)
{
	uint tmpCounters[HIST_WKSP_SIZE_U32];
	return HIST_countFast_wksp(count, maxSymbolValuePtr, source, sourceSize, tmpCounters, sizeof(tmpCounters));
}

size_t HIST_count(uint * count, uint * maxSymbolValuePtr, const void * src, size_t srcSize)
{
	uint tmpCounters[HIST_WKSP_SIZE_U32];
	return HIST_count_wksp(count, maxSymbolValuePtr, src, srcSize, tmpCounters, sizeof(tmpCounters));
}

#endif
