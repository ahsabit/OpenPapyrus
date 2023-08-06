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
//
// Tuning parameters
//
#define MINRATIO 4   /* minimum nb of apparition to be selected in dictionary */
#define ZDICT_MAX_SAMPLES_SIZE (2000U << 20)
#define ZDICT_MIN_SAMPLES_SIZE (ZDICT_CONTENTSIZE_MIN * MINRATIO)
//
// Compiler Options
//
// #define _FILE_OFFSET_BITS 64 /* Unix Large Files support (>4GB) */
#if (defined(__sun__) && (!defined(__LP64__)))   /* Sun Solaris 32-bits requires specific definitions */
	#ifndef _LARGEFILE_SOURCE
		#define _LARGEFILE_SOURCE
	#endif
#elif !defined(__LP64__)                         /* No point defining Large file for 64 bit */
	#ifndef _LARGEFILE64_SOURCE
		#define _LARGEFILE64_SOURCE
	#endif
#endif
#ifndef ZDICT_STATIC_LINKING_ONLY
	#define ZDICT_STATIC_LINKING_ONLY
#endif
#define HUF_STATIC_LINKING_ONLY

#include <zstd_mem.h> // read
#include <fse.h>           /* FSE_normalizeCount, FSE_writeNCount */
#include <huf.h>           /* HUF_buildCTable, HUF_writeCTable */
#include <zstd_internal.h> /* includes zstd.h */
#include <zstd_compress_internal.h> /* ZSTD_loadCEntropy() */
#include <zdict.h>
#include "divsufsort.h"
#include <bits.h>          /* ZSTD_NbCommonBytes */
// 
// Constants
// 
//#define KB_Removed *(1 <<10)
//#define MB_Removed *(1 <<20)
//#define GB_Removed *(1U<<30)
#define DICTLISTSIZE_DEFAULT 10000
#define NOISELENGTH 32

static const uint32 g_selectivity_default = 9;
//
// Console display
//
#undef  DISPLAY
#define DISPLAY(...)         { slfprintf_stderr(__VA_ARGS__); fflush(stderr); }
#undef  DISPLAYLEVEL
#define DISPLAYLEVEL(l, ...) if(notificationLevel>=l) { DISPLAY(__VA_ARGS__); } // 0 : no display;   1: errors;   2: default;  3: details;  4: debug

static clock_t ZDICT_clockSpan(clock_t nPrevious) { return clock() - nPrevious; }

static void ZDICT_printHex(const void * ptr, size_t length)
{
	const BYTE * const b = (const BYTE *)ptr;
	size_t u;
	for(u = 0; u<length; u++) {
		BYTE c = b[u];
		if(c<32 || c>126) c = '.'; /* non-printable char */
		DISPLAY("%c", c);
	}
}
// 
// Helper functions
// 
uint ZDICT_isError(size_t errorCode) { return ERR_isError(errorCode); }
const char* ZDICT_getErrorName(size_t errorCode) { return ERR_getErrorName(errorCode); }

uint ZDICT_getDictID(const void * dictBuffer, size_t dictSize)
{
	if(dictSize < 8) 
		return 0;
	if(SMem::GetLe32(dictBuffer) != ZSTD_MAGIC_DICTIONARY) 
		return 0;
	return SMem::GetLe32((const char *)dictBuffer + 4);
}

size_t ZDICT_getDictHeaderSize(const void * dictBuffer, size_t dictSize)
{
	size_t headerSize;
	if(dictSize <= 8 || SMem::GetLe32(dictBuffer) != ZSTD_MAGIC_DICTIONARY) 
		return ERROR(dictionary_corrupted);
	{   
		ZSTD_compressedBlockState_t* bs = (ZSTD_compressedBlockState_t*)SAlloc::M(sizeof(ZSTD_compressedBlockState_t));
	    uint32 * wksp = (uint32 *)SAlloc::M(HUF_WORKSPACE_SIZE);
	    if(!bs || !wksp) {
		    headerSize = ERROR(memory_allocation);
	    }
	    else {
		    ZSTD_reset_compressedBlockState(bs);
		    headerSize = ZSTD_loadCEntropy(bs, wksp, dictBuffer, dictSize);
	    }
	    SAlloc::F(bs);
	    SAlloc::F(wksp);
	}
	return headerSize;
}

/*-********************************************************
*  Dictionary training functions
**********************************************************/
/*! ZDICT_count() :
    Count the nb of common bytes between 2 pointers.
    Note : this function presumes end of buffer followed by noisy guard band.
 */
static size_t ZDICT_count(const void * pIn, const void * pMatch)
{
	const char* const pStart = (const char *)pIn;
	for(;;) {
		const size_t diff = SMem::GetSizeT(pMatch) ^ SMem::GetSizeT(pIn);
		if(!diff) {
			pIn = (const char *)pIn+sizeof(size_t);
			pMatch = (const char *)pMatch+sizeof(size_t);
			continue;
		}
		pIn = (const char *)pIn+ZSTD_NbCommonBytes(diff);
		return (size_t)((const char *)pIn - pStart);
	}
}

typedef struct {
	uint32 pos;
	uint32 length;
	uint32 savings;
} dictItem;

static void ZDICT_initDictItem(dictItem* d)
{
	d->pos = 1;
	d->length = 0;
	d->savings = (uint32)(-1);
}

#define LLIMIT 64          /* heuristic determined experimentally */
#define MINMATCHLENGTH 7   /* heuristic determined experimentally */
static dictItem ZDICT_analyzePos(BYTE * doneMarks, const int* suffix, uint32 start, const void * buffer, uint32 minRatio, uint32 notificationLevel)
{
	uint32 lengthList[LLIMIT] = {0};
	uint32 cumulLength[LLIMIT] = {0};
	uint32 savings[LLIMIT] = {0};
	const BYTE * b = (const BYTE *)buffer;
	size_t maxLength = LLIMIT;
	size_t pos = (size_t)suffix[start];
	uint32 end = start;
	dictItem solution;
	/* init */
	memzero(&solution, sizeof(solution));
	doneMarks[pos] = 1;
	/* trivial repetition cases */
	if((SMem::Get16(b+pos+0) == SMem::Get16(b+pos+2)) ||(SMem::Get16(b+pos+1) == SMem::Get16(b+pos+3)) ||(SMem::Get16(b+pos+2) == SMem::Get16(b+pos+4))) {
		/* skip and mark segment */
		uint16 const pattern16 = SMem::Get16(b+pos+4);
		uint32 u, patternEnd = 6;
		while(SMem::Get16(b+pos+patternEnd) == pattern16) 
			patternEnd += 2;
		if(b[pos+patternEnd] == b[pos+patternEnd-1]) 
			patternEnd++;
		for(u = 1; u<patternEnd; u++)
			doneMarks[pos+u] = 1;
		return solution;
	}
	/* look forward */
	{   
		size_t length;
	    do {
		    end++;
		    length = ZDICT_count(b + pos, b + suffix[end]);
	    } while(length >= MINMATCHLENGTH);
	}
	/* look backward */
	{   
		size_t length;
	    do {
		    length = ZDICT_count(b + pos, b + *(suffix+start-1));
		    if(length >=MINMATCHLENGTH) 
				start--;
	    } while(length >= MINMATCHLENGTH);
	}
	/* exit if not found a minimum nb of repetitions */
	if(end-start < minRatio) {
		uint32 idx;
		for(idx = start; idx<end; idx++)
			doneMarks[suffix[idx]] = 1;
		return solution;
	}
	{   
		int i;
	    uint32 mml;
	    uint32 refinedStart = start;
	    uint32 refinedEnd = end;

	    DISPLAYLEVEL(4, "\n");
	    DISPLAYLEVEL(4, "found %3u matches of length >= %i at pos %7u  ", (uint)(end-start), MINMATCHLENGTH, (uint)pos);
	    DISPLAYLEVEL(4, "\n");

	    for(mml = MINMATCHLENGTH;; mml++) {
		    BYTE currentChar = 0;
		    uint32 currentCount = 0;
		    uint32 currentID = refinedStart;
		    uint32 id;
		    uint32 selectedCount = 0;
		    uint32 selectedID = currentID;
		    for(id = refinedStart; id < refinedEnd; id++) {
			    if(b[suffix[id] + mml] != currentChar) {
				    if(currentCount > selectedCount) {
					    selectedCount = currentCount;
					    selectedID = currentID;
				    }
				    currentID = id;
				    currentChar = b[ suffix[id] + mml];
				    currentCount = 0;
			    }
			    currentCount++;
		    }
		    if(currentCount > selectedCount) { /* for last */
			    selectedCount = currentCount;
			    selectedID = currentID;
		    }

		    if(selectedCount < minRatio)
			    break;
		    refinedStart = selectedID;
		    refinedEnd = refinedStart + selectedCount;
	    }

		/* evaluate gain based on new dict */
	    start = refinedStart;
	    pos = suffix[refinedStart];
	    end = start;
	    memzero(lengthList, sizeof(lengthList));
		/* look forward */
	    {   
			size_t length;
			do {
				end++;
				length = ZDICT_count(b + pos, b + suffix[end]);
				if(length >= LLIMIT) length = LLIMIT-1;
				lengthList[length]++;
			} while(length >=MINMATCHLENGTH);
		}
		/* look backward */
	    {   
		size_t length = MINMATCHLENGTH;
		while((length >= MINMATCHLENGTH) & (start > 0)) {
			length = ZDICT_count(b + pos, b + suffix[start - 1]);
			if(length >= LLIMIT) length = LLIMIT - 1;
			lengthList[length]++;
			if(length >= MINMATCHLENGTH) start--;
		}
	    }
		/* largest useful length */
	    memzero(cumulLength, sizeof(cumulLength));
	    cumulLength[maxLength-1] = lengthList[maxLength-1];
	    for(i = (int)(maxLength-2); i>=0; i--)
		    cumulLength[i] = cumulLength[i+1] + lengthList[i];

	    for(i = LLIMIT-1; i>=MINMATCHLENGTH; i--) if(cumulLength[i]>=minRatio) break;
	    maxLength = i;

		/* reduce maxLength in case of final into repetitive data */
	    {   
			uint32 l = (uint32)maxLength;
			BYTE const c = b[pos + maxLength-1];
			while(b[pos+l-2]==c) 
				l--;
			maxLength = l;
		}
	    if(maxLength < MINMATCHLENGTH) return solution; /* skip : no long-enough solution */

		/* calculate savings */
	    savings[5] = 0;
	    for(i = MINMATCHLENGTH; i<=(int)maxLength; i++)
		    savings[i] = savings[i-1] + (lengthList[i] * (i-3));

	    DISPLAYLEVEL(4, "Selected dict at position %u, of length %u : saves %u (ratio: %.2f)  \n",
		(uint)pos, (uint)maxLength, (uint)savings[maxLength], (double)savings[maxLength] / (double)maxLength);

	    solution.pos = (uint32)pos;
	    solution.length = (uint32)maxLength;
	    solution.savings = savings[maxLength];

		/* mark positions done */
	    {   uint32 id;
		for(id = start; id<end; id++) {
			uint32 p, pEnd, length;
			const uint32 testedPos = (uint32)suffix[id];
			if(testedPos == pos)
				length = solution.length;
			else {
				length = (uint32)ZDICT_count(b+pos, b+testedPos);
				if(length > solution.length) length = solution.length;
			}
			pEnd = (uint32)(testedPos + length);
			for(p = testedPos; p<pEnd; p++)
				doneMarks[p] = 1;
		}
	    }   }

	return solution;
}

static int isIncluded(const void * in, const void * container, size_t length)
{
	const char* const ip = (const char *)in;
	const char* const into = (const char *)container;
	size_t u;

	for(u = 0; u<length; u++) { /* works because end of buffer is a noisy guard band */
		if(ip[u] != into[u]) break;
	}

	return u==length;
}

/*! ZDICT_tryMerge() :
    check if dictItem can be merged, do it if possible
    @return : id of destination elt, 0 if not merged
 */
static uint32 ZDICT_tryMerge(dictItem* table, dictItem elt, uint32 eltNbToSkip, const void * buffer)
{
	const uint32 tableSize = table->pos;
	const uint32 eltEnd = elt.pos + elt.length;
	const char* const buf = (const char *)buffer;

	/* tail overlap */
	uint32 u; for(u = 1; u<tableSize; u++) {
		if(u==eltNbToSkip) continue;
		if((table[u].pos > elt.pos) && (table[u].pos <= eltEnd)) { /* overlap, existing > new */
			/* append */
			const uint32 addedLength = table[u].pos - elt.pos;
			table[u].length += addedLength;
			table[u].pos = elt.pos;
			table[u].savings += elt.savings * addedLength / elt.length; /* rough approx */
			table[u].savings += elt.length / 8; /* rough approx bonus */
			elt = table[u];
			/* sort : improve rank */
			while((u>1) && (table[u-1].savings < elt.savings))
				table[u] = table[u-1], u--;
			table[u] = elt;
			return u;
		}
	}

	/* front overlap */
	for(u = 1; u<tableSize; u++) {
		if(u==eltNbToSkip) continue;

		if((table[u].pos + table[u].length >= elt.pos) && (table[u].pos < elt.pos)) { /* overlap, existing < new
			                                                                         */
			/* append */
			int const addedLength = (int)eltEnd - (int)(table[u].pos + table[u].length);
			table[u].savings += elt.length / 8; /* rough approx bonus */
			if(addedLength > 0) { /* otherwise, elt fully included into existing */
				table[u].length += addedLength;
				table[u].savings += elt.savings * addedLength / elt.length; /* rough approx */
			}
			/* sort : improve rank */
			elt = table[u];
			while((u>1) && (table[u-1].savings < elt.savings))
				table[u] = table[u-1], u--;
			table[u] = elt;
			return u;
		}

		if(SMem::Get64(buf + table[u].pos) == SMem::Get64(buf + elt.pos + 1)) {
			if(isIncluded(buf + table[u].pos, buf + elt.pos + 1, table[u].length)) {
				const size_t addedLength = MAX( (int)elt.length - (int)table[u].length, 1);
				table[u].pos = elt.pos;
				table[u].savings += (uint32)(elt.savings * addedLength / elt.length);
				table[u].length = MIN(elt.length, table[u].length + 1);
				return u;
			}
		}
	}

	return 0;
}

static void ZDICT_removeDictItem(dictItem* table, uint32 id)
{
	/* convention : table[0].pos stores nb of elts */
	const uint32 max = table[0].pos;
	uint32 u;
	if(!id) return; /* protection, should never happen */
	for(u = id; u<max-1; u++)
		table[u] = table[u+1];
	table->pos--;
}

static void ZDICT_insertDictItem(dictItem* table, uint32 maxSize, dictItem elt, const void * buffer)
{
	/* merge if possible */
	uint32 mergeId = ZDICT_tryMerge(table, elt, 0, buffer);
	if(mergeId) {
		uint32 newMerge = 1;
		while(newMerge) {
			newMerge = ZDICT_tryMerge(table, table[mergeId], mergeId, buffer);
			if(newMerge) ZDICT_removeDictItem(table, mergeId);
			mergeId = newMerge;
		}
		return;
	}
	/* insert */
	{   
		uint32 current;
	    uint32 nextElt = table->pos;
	    if(nextElt >= maxSize) nextElt = maxSize-1;
	    current = nextElt-1;
	    while(table[current].savings < elt.savings) {
		    table[current+1] = table[current];
		    current--;
	    }
	    table[current+1] = elt;
	    table->pos = nextElt+1;
	}
}

static uint32 ZDICT_dictSize(const dictItem* dictList)
{
	uint32 u, dictSize = 0;
	for(u = 1; u<dictList[0].pos; u++)
		dictSize += dictList[u].length;
	return dictSize;
}

static size_t ZDICT_trainBuffer_legacy(dictItem* dictList, uint32 dictListSize,
    const void * const buffer, size_t bufferSize/* buffer must end with noisy guard band */,
    const size_t* fileSizes, uint nbFiles, uint minRatio, uint32 notificationLevel)
{
	int* const suffix0 = (int*)SAlloc::M((bufferSize+2)*sizeof(*suffix0));
	int* const suffix = suffix0+1;
	uint32 * reverseSuffix = (uint32 *)SAlloc::M((bufferSize)*sizeof(*reverseSuffix));
	BYTE * doneMarks = (BYTE *)SAlloc::M((bufferSize+16)*sizeof(*doneMarks)); /* +16 for overflow security */
	uint32 * filePos = (uint32 *)SAlloc::M(nbFiles * sizeof(*filePos));
	size_t result = 0;
	clock_t displayClock = 0;
	clock_t const refreshRate = CLOCKS_PER_SEC * 3 / 10;

#undef  DISPLAYUPDATE
#define DISPLAYUPDATE(l, ...) if(notificationLevel>=l) { \
		if(ZDICT_clockSpan(displayClock) > refreshRate)  \
		{ displayClock = clock(); DISPLAY(__VA_ARGS__); \
		  if(notificationLevel>=4) fflush(stderr); } }

	/* init */
	DISPLAYLEVEL(2, "\r%70s\r", ""); /* clean display line */
	if(!suffix0 || !reverseSuffix || !doneMarks || !filePos) {
		result = ERROR(memory_allocation);
		goto _cleanup;
	}
	if(minRatio < MINRATIO) 
		minRatio = MINRATIO;
	memzero(doneMarks, bufferSize+16);
	/* limit sample set size (divsufsort limitation)*/
	if(bufferSize >
	    ZDICT_MAX_SAMPLES_SIZE) DISPLAYLEVEL(3, "sample set too large : reduced to %u MB ...\n", (uint)(ZDICT_MAX_SAMPLES_SIZE>>20));
	while(bufferSize > ZDICT_MAX_SAMPLES_SIZE) 
		bufferSize -= fileSizes[--nbFiles];
	/* sort */
	DISPLAYLEVEL(2, "sorting %u files of total size %u MB ...\n", nbFiles, (uint)(bufferSize>>20));
	{   
		int const divSuftSortResult = divsufsort((const uchar*)buffer, suffix, (int)bufferSize, 0);
	    if(divSuftSortResult != 0) {
		    result = ERROR(GENERIC); goto _cleanup;
	    }
	}
	suffix[bufferSize] = (int)bufferSize; /* leads into noise */
	suffix0[0] = (int)bufferSize;       /* leads into noise */
	/* build reverse suffix sort */
	{   
		size_t pos;
	    for(pos = 0; pos < bufferSize; pos++)
		    reverseSuffix[suffix[pos]] = (uint32)pos;
		/* note filePos tracks borders between samples.
		It's not used at this stage, but planned to become useful in a later update */
	    filePos[0] = 0;
	    for(pos = 1; pos<nbFiles; pos++)
		    filePos[pos] = (uint32)(filePos[pos-1] + fileSizes[pos-1]); }

	DISPLAYLEVEL(2, "finding patterns ... \n");
	DISPLAYLEVEL(3, "minimum ratio : %u \n", minRatio);

	{   uint32 cursor; for(cursor = 0; cursor < bufferSize;) {
		    dictItem solution;
		    if(doneMarks[cursor]) {
			    cursor++; continue;
		    }
		    solution = ZDICT_analyzePos(doneMarks, suffix, reverseSuffix[cursor], buffer, minRatio, notificationLevel);
		    if(solution.length==0) {
			    cursor++; continue;
		    }
		    ZDICT_insertDictItem(dictList, dictListSize, solution, buffer);
		    cursor += solution.length;
		    DISPLAYUPDATE(2, "\r%4.2f %% \r", (double)cursor / bufferSize * 100);
	    }
	}
_cleanup:
	SAlloc::F(suffix0);
	SAlloc::F(reverseSuffix);
	SAlloc::F(doneMarks);
	SAlloc::F(filePos);
	return result;
}

static void ZDICT_fillNoise(void * buffer, size_t length)
{
	//const uint prime1 = SlConst::MagicHashPrime32 /*2654435761U*/;
	const uint prime2 = 2246822519U;
	uint acc = SlConst::MagicHashPrime32/*prime1*/;
	for(size_t p = 0; p<length; p++) {
		acc *= prime2;
		((uchar*)buffer)[p] = (uchar)(acc >> 21);
	}
}

typedef struct {
	ZSTD_CDict* dict; /* dictionary */
	ZSTD_CCtx* zc; /* working context */
	void * workPlace; /* must be ZSTD_BLOCKSIZE_MAX allocated */
} EStats_ress_t;

#define MAXREPOFFSET 1024

static void ZDICT_countEStats(EStats_ress_t esr, const ZSTD_parameters* params,
    uint * countLit, uint * offsetcodeCount, uint * matchlengthCount, uint * litlengthCount, uint32 * repOffsets,
    const void * src, size_t srcSize, uint32 notificationLevel)
{
	const size_t blockSizeMax = MIN(ZSTD_BLOCKSIZE_MAX, 1 << params->cParams.windowLog);
	size_t cSize;
	if(srcSize > blockSizeMax) 
		srcSize = blockSizeMax; /* protection vs large samples */
	{   
		const size_t errorCode = ZSTD_compressBegin_usingCDict(esr.zc, esr.dict);
	    if(ZSTD_isError(errorCode)) {
		    DISPLAYLEVEL(1, "warning : ZSTD_compressBegin_usingCDict failed \n"); return;
	    }
	}
	cSize = ZSTD_compressBlock(esr.zc, esr.workPlace, ZSTD_BLOCKSIZE_MAX, src, srcSize);
	if(ZSTD_isError(cSize)) {
		DISPLAYLEVEL(3, "warning : could not compress sample size %u \n", (uint)srcSize); return;
	}

	if(cSize) { /* if == 0; block is not compressible */
		const seqStore_t* const seqStorePtr = ZSTD_getSeqStore(esr.zc);

		/* literals stats */
		{   
			const BYTE * bytePtr;
		    for(bytePtr = seqStorePtr->litStart; bytePtr < seqStorePtr->lit; bytePtr++)
			    countLit[*bytePtr]++; 
		}
		/* seqStats */
		{   
			const uint32 nbSeq = (uint32)(seqStorePtr->sequences - seqStorePtr->sequencesStart);
		    ZSTD_seqToCodes(seqStorePtr);
		    {   
			const BYTE * codePtr = seqStorePtr->ofCode;
			uint32 u;
			for(u = 0; u<nbSeq; u++) 
				offsetcodeCount[codePtr[u]]++; 
			}
		    {   
			const BYTE * codePtr = seqStorePtr->mlCode;
			uint32 u;
			for(u = 0; u<nbSeq; u++) matchlengthCount[codePtr[u]]++; 
			}
		    {   
				const BYTE * codePtr = seqStorePtr->llCode;
				uint32 u;
				for(u = 0; u<nbSeq; u++) litlengthCount[codePtr[u]]++; 
			}

		    if(nbSeq >= 2) { /* rep offsets */
			    const seqDef* const seq = seqStorePtr->sequencesStart;
			    uint32 offset1 = seq[0].offBase - ZSTD_REP_NUM;
			    uint32 offset2 = seq[1].offBase - ZSTD_REP_NUM;
			    if(offset1 >= MAXREPOFFSET) offset1 = 0;
			    if(offset2 >= MAXREPOFFSET) offset2 = 0;
			    repOffsets[offset1] += 3;
			    repOffsets[offset2] += 1;
		    }
		}
	}
}

static size_t FASTCALL ZDICT_totalSampleSize(const size_t* fileSizes, uint nbFiles)
{
	size_t total = 0;
	for(uint u = 0; u<nbFiles; u++) 
		total += fileSizes[u];
	return total;
}

typedef struct { uint32 offset; uint32 count; } offsetCount_t;

static void ZDICT_insertSortCount(offsetCount_t table[ZSTD_REP_NUM+1], uint32 val, uint32 count)
{
	uint32 u;
	table[ZSTD_REP_NUM].offset = val;
	table[ZSTD_REP_NUM].count = count;
	for(u = ZSTD_REP_NUM; u>0; u--) {
		offsetCount_t tmp;
		if(table[u-1].count >= table[u].count) break;
		tmp = table[u-1];
		table[u-1] = table[u];
		table[u] = tmp;
	}
}

/* ZDICT_flatLit() :
 * rewrite `countLit` to contain a mostly flat but still compressible distribution of literals.
 * necessary to avoid generating a non-compressible distribution that HUF_writeCTable() cannot encode.
 */
static void ZDICT_flatLit(uint * countLit)
{
	int u;
	for(u = 1; u<256; u++) 
		countLit[u] = 2;
	countLit[0]   = 4;
	countLit[253] = 1;
	countLit[254] = 1;
}

#define OFFCODE_MAX 30  /* only applicable to first block */
static size_t ZDICT_analyzeEntropy(void *  dstBuffer, size_t maxDstSize, int compressionLevel,
    const void *  srcBuffer, const size_t* fileSizes, uint nbFiles, const void * dictBuffer, size_t dictBufferSize, uint notificationLevel)
{
	uint countLit[256];
	HUF_CREATE_STATIC_CTABLE(hufTable, 255);
	uint offcodeCount[OFFCODE_MAX+1];
	short offcodeNCount[OFFCODE_MAX+1];
	uint32 offcodeMax = ZSTD_highbit32((uint32)(dictBufferSize + SKILOBYTE(128)));
	uint matchLengthCount[MaxML+1];
	short matchLengthNCount[MaxML+1];
	uint litLengthCount[MaxLL+1];
	short litLengthNCount[MaxLL+1];
	uint32 repOffset[MAXREPOFFSET];
	offsetCount_t bestRepOffset[ZSTD_REP_NUM+1];
	EStats_ress_t esr = { NULL, NULL, NULL };
	ZSTD_parameters params;
	uint32 u, huffLog = 11, Offlog = OffFSELog, mlLog = MLFSELog, llLog = LLFSELog, total;
	size_t pos = 0, errorCode;
	size_t eSize = 0;
	const size_t totalSrcSize = ZDICT_totalSampleSize(fileSizes, nbFiles);
	const size_t averageSampleSize = totalSrcSize / (nbFiles + !nbFiles);
	BYTE * dstPtr = (BYTE *)dstBuffer;

	/* init */
	DEBUGLOG(4, "ZDICT_analyzeEntropy");
	if(offcodeMax>OFFCODE_MAX) {
		eSize = ERROR(dictionaryCreation_failed); goto _cleanup;
	} // too large dictionary
	for(u = 0; u<256; u++) countLit[u] = 1; /* any character must be described */
	for(u = 0; u<=offcodeMax; u++) offcodeCount[u] = 1;
	for(u = 0; u<=MaxML; u++) matchLengthCount[u] = 1;
	for(u = 0; u<=MaxLL; u++) litLengthCount[u] = 1;
	memzero(repOffset, sizeof(repOffset));
	repOffset[1] = repOffset[4] = repOffset[8] = 1;
	memzero(bestRepOffset, sizeof(bestRepOffset));
	if(compressionLevel==0) 
		compressionLevel = ZSTD_CLEVEL_DEFAULT;
	params = ZSTD_getParams(compressionLevel, averageSampleSize, dictBufferSize);
	esr.dict = ZSTD_createCDict_advanced(dictBuffer, dictBufferSize, ZSTD_dlm_byRef, ZSTD_dct_rawContent, params.cParams, ZSTD_defaultCMem);
	esr.zc = ZSTD_createCCtx();
	esr.workPlace = SAlloc::M(ZSTD_BLOCKSIZE_MAX);
	if(!esr.dict || !esr.zc || !esr.workPlace) {
		eSize = ERROR(memory_allocation);
		DISPLAYLEVEL(1, "Not enough memory \n");
		goto _cleanup;
	}

	/* collect stats on all samples */
	for(u = 0; u<nbFiles; u++) {
		ZDICT_countEStats(esr, &params, countLit, offcodeCount, matchLengthCount, litLengthCount, repOffset,
		    (const char *)srcBuffer + pos, fileSizes[u], notificationLevel);
		pos += fileSizes[u];
	}
	if(notificationLevel >= 4) {
		/* writeStats */
		DISPLAYLEVEL(4, "Offset Code Frequencies : \n");
		for(u = 0; u<=offcodeMax; u++) {
			DISPLAYLEVEL(4, "%2u :%7u \n", u, offcodeCount[u]);
		}
	}
	/* analyze, build stats, starting with literals */
	{   
		size_t maxNbBits = HUF_buildCTable(hufTable, countLit, 255, huffLog);
	    if(HUF_isError(maxNbBits)) {
		    eSize = maxNbBits;
		    DISPLAYLEVEL(1, " HUF_buildCTable error \n");
		    goto _cleanup;
	    }
	    if(maxNbBits==8) { /* not compressible : will fail on HUF_writeCTable() */
		    DISPLAYLEVEL(2, "warning : pathological dataset : literals are not compressible : samples are noisy or too regular \n");
		    ZDICT_flatLit(countLit); /* replace distribution by a fake "mostly flat but still compressible"
			                        distribution, that HUF_writeCTable() can encode */
		    maxNbBits = HUF_buildCTable(hufTable, countLit, 255, huffLog);
		    assert(maxNbBits==9);
	    }
	    huffLog = (uint32)maxNbBits;
	}
	/* looking for most common first offsets */
	{   
		uint32 offset;
	    for(offset = 1; offset<MAXREPOFFSET; offset++)
		    ZDICT_insertSortCount(bestRepOffset, offset, repOffset[offset]); }
	/* note : the result of this phase should be used to better appreciate the impact on statistics */

	total = 0; for(u = 0; u<=offcodeMax; u++) total += offcodeCount[u];
	errorCode = FSE_normalizeCount(offcodeNCount, Offlog, offcodeCount, total, offcodeMax, /* useLowProbCount */ 1);
	if(FSE_isError(errorCode)) {
		eSize = errorCode;
		DISPLAYLEVEL(1, "FSE_normalizeCount error with offcodeCount \n");
		goto _cleanup;
	}
	Offlog = (uint32)errorCode;

	total = 0; for(u = 0; u<=MaxML; u++) total += matchLengthCount[u];
	errorCode = FSE_normalizeCount(matchLengthNCount, mlLog, matchLengthCount, total, MaxML, /* useLowProbCount */ 1);
	if(FSE_isError(errorCode)) {
		eSize = errorCode;
		DISPLAYLEVEL(1, "FSE_normalizeCount error with matchLengthCount \n");
		goto _cleanup;
	}
	mlLog = (uint32)errorCode;

	total = 0; for(u = 0; u<=MaxLL; u++) total += litLengthCount[u];
	errorCode = FSE_normalizeCount(litLengthNCount, llLog, litLengthCount, total, MaxLL, /* useLowProbCount */ 1);
	if(FSE_isError(errorCode)) {
		eSize = errorCode;
		DISPLAYLEVEL(1, "FSE_normalizeCount error with litLengthCount \n");
		goto _cleanup;
	}
	llLog = (uint32)errorCode;

	/* write result to buffer */
	{   
		const size_t hhSize = HUF_writeCTable(dstPtr, maxDstSize, hufTable, 255, huffLog);
	    if(HUF_isError(hhSize)) {
		    eSize = hhSize;
		    DISPLAYLEVEL(1, "HUF_writeCTable error \n");
		    goto _cleanup;
	    }
	    dstPtr += hhSize;
	    maxDstSize -= hhSize;
	    eSize += hhSize;
	}
	{   
		const size_t ohSize = FSE_writeNCount(dstPtr, maxDstSize, offcodeNCount, OFFCODE_MAX, Offlog);
	    if(FSE_isError(ohSize)) {
		    eSize = ohSize;
		    DISPLAYLEVEL(1, "FSE_writeNCount error with offcodeNCount \n");
		    goto _cleanup;
	    }
	    dstPtr += ohSize;
	    maxDstSize -= ohSize;
	    eSize += ohSize;
	}
	{   
		const size_t mhSize = FSE_writeNCount(dstPtr, maxDstSize, matchLengthNCount, MaxML, mlLog);
	    if(FSE_isError(mhSize)) {
		    eSize = mhSize;
		    DISPLAYLEVEL(1, "FSE_writeNCount error with matchLengthNCount \n");
		    goto _cleanup;
	    }
	    dstPtr += mhSize;
	    maxDstSize -= mhSize;
	    eSize += mhSize;
	}
	{   
		const size_t lhSize = FSE_writeNCount(dstPtr, maxDstSize, litLengthNCount, MaxLL, llLog);
	    if(FSE_isError(lhSize)) {
		    eSize = lhSize;
		    DISPLAYLEVEL(1, "FSE_writeNCount error with litlengthNCount \n");
		    goto _cleanup;
	    }
	    dstPtr += lhSize;
	    maxDstSize -= lhSize;
	    eSize += lhSize;
	}
	if(maxDstSize<12) {
		eSize = ERROR(dstSize_tooSmall);
		DISPLAYLEVEL(1, "not enough space to write RepOffsets \n");
		goto _cleanup;
	}
#if 0
	SMem::PutLe(dstPtr+0, bestRepOffset[0].offset);
	SMem::PutLe(dstPtr+4, bestRepOffset[1].offset);
	SMem::PutLe(dstPtr+8, bestRepOffset[2].offset);
#else
	/* at this stage, we don't use the result of "most common first offset",
	 * as the impact of statistics is not properly evaluated */
	SMem::PutLe(dstPtr+0, repStartValue[0]);
	SMem::PutLe(dstPtr+4, repStartValue[1]);
	SMem::PutLe(dstPtr+8, repStartValue[2]);
#endif
	eSize += 12;
_cleanup:
	ZSTD_freeCDict(esr.dict);
	ZSTD_freeCCtx(esr.zc);
	SAlloc::F(esr.workPlace);
	return eSize;
}
/**
 * @returns the maximum repcode value
 */
static uint32 FASTCALL ZDICT_maxRep(const uint32 reps[ZSTD_REP_NUM])
{
	uint32 maxRep = reps[0];
	for(int r = 1; r < ZSTD_REP_NUM; ++r)
		maxRep = MAX(maxRep, reps[r]);
	return maxRep;
}

size_t ZDICT_finalizeDictionary(void * dictBuffer, size_t dictBufferCapacity, const void * customDictContent, size_t dictContentSize,
    const void * samplesBuffer, const size_t* samplesSizes, uint nbSamples, ZDICT_params_t params)
{
	size_t hSize;
#define HBUFFSIZE 256   /* should prove large enough for all entropy headers */
	BYTE header[HBUFFSIZE];
	int const compressionLevel = (params.compressionLevel == 0) ? ZSTD_CLEVEL_DEFAULT : params.compressionLevel;
	const uint32 notificationLevel = params.notificationLevel;
	/* The final dictionary content must be at least as large as the largest repcode */
	const size_t minContentSize = (size_t)ZDICT_maxRep(repStartValue);
	size_t paddingSize;
	/* check conditions */
	DEBUGLOG(4, "ZDICT_finalizeDictionary");
	if(dictBufferCapacity < dictContentSize) return ERROR(dstSize_tooSmall);
	if(dictBufferCapacity < ZDICT_DICTSIZE_MIN) return ERROR(dstSize_tooSmall);
	/* dictionary header */
	SMem::PutLe(header, ZSTD_MAGIC_DICTIONARY);
	{   
		uint64 const randomID = XXH64(customDictContent, dictContentSize, 0);
	    const uint32 compliantID = (randomID % ((1U<<31)-32768)) + 32768;
	    const uint32 dictID = params.dictID ? params.dictID : compliantID;
	    SMem::PutLe(header+4, dictID);
	}
	hSize = 8;
	/* entropy tables */
	DISPLAYLEVEL(2, "\r%70s\r", ""); /* clean display line */
	DISPLAYLEVEL(2, "statistics ... \n");
	{   
		const size_t eSize = ZDICT_analyzeEntropy(header+hSize, HBUFFSIZE-hSize, compressionLevel,
		    samplesBuffer, samplesSizes, nbSamples, customDictContent, dictContentSize, notificationLevel);
	    if(ZDICT_isError(eSize)) 
			return eSize;
	    hSize += eSize;
	}
	/* Shrink the content size if it doesn't fit in the buffer */
	if(hSize + dictContentSize > dictBufferCapacity) {
		dictContentSize = dictBufferCapacity - hSize;
	}
	/* Pad the dictionary content with zeros if it is too small */
	if(dictContentSize < minContentSize) {
		RETURN_ERROR_IF(hSize + minContentSize > dictBufferCapacity, dstSize_tooSmall, "dictBufferCapacity too small to fit max repcode");
		paddingSize = minContentSize - dictContentSize;
	}
	else {
		paddingSize = 0;
	}
	{
		const size_t dictSize = hSize + paddingSize + dictContentSize;
		/* The dictionary consists of the header, optional padding, and the content.
		 * The padding comes before the content because the "best" position in the
		 * dictionary is the last byte.
		 */
		BYTE * const outDictHeader = (BYTE *)dictBuffer;
		BYTE * const outDictPadding = outDictHeader + hSize;
		BYTE * const outDictContent = outDictPadding + paddingSize;
		assert(dictSize <= dictBufferCapacity);
		assert(outDictContent + dictContentSize == (BYTE *)dictBuffer + dictSize);
		/* First copy the customDictContent into its final location.
		 * `customDictContent` and `dictBuffer` may overlap, so we must
		 * do this before any other writes into the output buffer.
		 * Then copy the header & padding into the output buffer.
		 */
		memmove(outDictContent, customDictContent, dictContentSize);
		memcpy(outDictHeader, header, hSize);
		memzero(outDictPadding, paddingSize);
		return dictSize;
	}
}

static size_t ZDICT_addEntropyTablesFromBuffer_advanced(void * dictBuffer, size_t dictContentSize, size_t dictBufferCapacity,
    const void * samplesBuffer, const size_t* samplesSizes, uint nbSamples, ZDICT_params_t params)
{
	int const compressionLevel = (params.compressionLevel == 0) ? ZSTD_CLEVEL_DEFAULT : params.compressionLevel;
	const uint32 notificationLevel = params.notificationLevel;
	size_t hSize = 8;
	/* calculate entropy tables */
	DISPLAYLEVEL(2, "\r%70s\r", ""); /* clean display line */
	DISPLAYLEVEL(2, "statistics ... \n");
	{   
		const size_t eSize = ZDICT_analyzeEntropy((char *)dictBuffer+hSize, dictBufferCapacity-hSize, compressionLevel,
		    samplesBuffer, samplesSizes, nbSamples, (char *)dictBuffer + dictBufferCapacity - dictContentSize, dictContentSize,
		    notificationLevel);
	    if(ZDICT_isError(eSize)) return eSize;
	    hSize += eSize;
	}
	/* add dictionary header (after entropy tables) */
	SMem::PutLe(dictBuffer, ZSTD_MAGIC_DICTIONARY);
	{   
		uint64 const randomID = XXH64((char *)dictBuffer + dictBufferCapacity - dictContentSize, dictContentSize, 0);
	    const uint32 compliantID = (randomID % ((1U<<31)-32768)) + 32768;
	    const uint32 dictID = params.dictID ? params.dictID : compliantID;
	    SMem::PutLe((char *)dictBuffer+4, dictID);
	}
	if(hSize + dictContentSize < dictBufferCapacity)
		memmove((char *)dictBuffer + hSize, (char *)dictBuffer + dictBufferCapacity - dictContentSize, dictContentSize);
	return MIN(dictBufferCapacity, hSize+dictContentSize);
}

/*! ZDICT_trainFromBuffer_unsafe_legacy() :
 *   Warning : `samplesBuffer` must be followed by noisy guard band !!!
 *   @return : size of dictionary, or an error code which can be tested with ZDICT_isError()
 */
static size_t ZDICT_trainFromBuffer_unsafe_legacy(void * dictBuffer, size_t maxDictSize,
    const void * samplesBuffer, const size_t* samplesSizes, uint nbSamples, ZDICT_legacy_params_t params)
{
	const uint32 dictListSize = MAX(MAX(DICTLISTSIZE_DEFAULT, nbSamples), (uint32)(maxDictSize/16));
	dictItem* const dictList = (dictItem*)SAlloc::M(dictListSize * sizeof(*dictList));
	const uint selectivity = params.selectivityLevel == 0 ? g_selectivity_default : params.selectivityLevel;
	const uint minRep = (selectivity > 30) ? MINRATIO : nbSamples >> selectivity;
	const size_t targetDictSize = maxDictSize;
	const size_t samplesBuffSize = ZDICT_totalSampleSize(samplesSizes, nbSamples);
	size_t dictSize = 0;
	const uint32 notificationLevel = params.zParams.notificationLevel;
	/* checks */
	if(!dictList) return ERROR(memory_allocation);
	if(maxDictSize < ZDICT_DICTSIZE_MIN) {
		SAlloc::F(dictList); 
		return ERROR(dstSize_tooSmall);
	} // requested dictionary size is too small 
	if(samplesBuffSize < ZDICT_MIN_SAMPLES_SIZE) {
		SAlloc::F(dictList); 
		return ERROR(dictionaryCreation_failed);
	} // not enough source to create dictionary
	/* init */
	ZDICT_initDictItem(dictList);
	/* build dictionary */
	ZDICT_trainBuffer_legacy(dictList, dictListSize, samplesBuffer, samplesBuffSize, samplesSizes, nbSamples, minRep, notificationLevel);
	/* display best matches */
	if(params.zParams.notificationLevel>= 3) {
		const uint nb = MIN(25, dictList[0].pos);
		const uint dictContentSize = ZDICT_dictSize(dictList);
		uint u;
		DISPLAYLEVEL(3, "\n %u segments found, of total size %u \n", (uint)dictList[0].pos-1, dictContentSize);
		DISPLAYLEVEL(3, "list %u best segments \n", nb-1);
		for(u = 1; u<nb; u++) {
			const uint pos = dictList[u].pos;
			const uint length = dictList[u].length;
			const uint32 printedLength = MIN(40, length);
			if((pos > samplesBuffSize) || ((pos + length) > samplesBuffSize)) {
				SAlloc::F(dictList);
				return ERROR(GENERIC); /* should never happen */
			}
			DISPLAYLEVEL(3, "%3u:%3u bytes at pos %8u, savings %7u bytes |", u, length, pos, (uint)dictList[u].savings);
			ZDICT_printHex((const char *)samplesBuffer+pos, printedLength);
			DISPLAYLEVEL(3, "| \n");
		}
	}
	/* create dictionary */
	{   
		uint dictContentSize = ZDICT_dictSize(dictList);
	    if(dictContentSize < ZDICT_CONTENTSIZE_MIN) {
		    SAlloc::F(dictList); 
			return ERROR(dictionaryCreation_failed);
	    } // dictionary content too small
	    if(dictContentSize < targetDictSize/4) {
		    DISPLAYLEVEL(2, "!  warning : selected content significantly smaller than requested (%u < %u) \n", dictContentSize,
			(uint)maxDictSize);
		    if(samplesBuffSize < 10 * targetDictSize)
			    DISPLAYLEVEL(2, "!  consider increasing the number of samples (total size : %u MB)\n",
				(uint)(samplesBuffSize>>20));
		    if(minRep > MINRATIO) {
			    DISPLAYLEVEL(2, "!  consider increasing selectivity to produce larger dictionary (-s%u) \n", selectivity+1);
			    DISPLAYLEVEL(2, "!  note : larger dictionaries are not necessarily better, test its efficiency on samples \n");
		    }
	    }
	    if((dictContentSize > targetDictSize*3) && (nbSamples > 2*MINRATIO) && (selectivity>1)) {
		    uint proposedSelectivity = selectivity-1;
		    while((nbSamples >> proposedSelectivity) <= MINRATIO) {
			    proposedSelectivity--;
		    }
		    DISPLAYLEVEL(2, "!  note : calculated dictionary significantly larger than requested (%u > %u) \n", dictContentSize,
			(uint)maxDictSize);
		    DISPLAYLEVEL(2, "!  consider increasing dictionary size, or produce denser dictionary (-s%u) \n", proposedSelectivity);
		    DISPLAYLEVEL(2, "!  always test dictionary efficiency on real samples \n");
	    }
		/* limit dictionary size */
	    {   
			const uint32 max = dictList->pos;/* convention : nb of useful elts within dictList */
			uint32 currentSize = 0;
			uint32 n; 
			for(n = 1; n<max; n++) {
				currentSize += dictList[n].length;
				if(currentSize > targetDictSize) {
					currentSize -= dictList[n].length; break;
				}
			}
			dictList->pos = n;
			dictContentSize = currentSize;
		}
		/* build dict content */
	    {   
			BYTE * ptr = (BYTE *)dictBuffer + maxDictSize;
			for(uint32 u = 1; u<dictList->pos; u++) {
				uint32 l = dictList[u].length;
				ptr -= l;
				if(ptr<(BYTE *)dictBuffer) {
					SAlloc::F(dictList); 
					return ERROR(GENERIC);
				} /* should not happen */
				memcpy(ptr, (const char *)samplesBuffer+dictList[u].pos, l);
			}
	    }
	    dictSize = ZDICT_addEntropyTablesFromBuffer_advanced(dictBuffer, dictContentSize, maxDictSize, samplesBuffer, samplesSizes, nbSamples, params.zParams);
	}
	/* clean up */
	SAlloc::F(dictList);
	return dictSize;
}

/* ZDICT_trainFromBuffer_legacy() :
 * issue : samplesBuffer need to be followed by a noisy guard band.
 * work around : duplicate the buffer, and add the noise */
size_t ZDICT_trainFromBuffer_legacy(void * dictBuffer, size_t dictBufferCapacity,
    const void * samplesBuffer, const size_t* samplesSizes, uint nbSamples, ZDICT_legacy_params_t params)
{
	size_t result;
	void * newBuff;
	const size_t sBuffSize = ZDICT_totalSampleSize(samplesSizes, nbSamples);
	if(sBuffSize < ZDICT_MIN_SAMPLES_SIZE) 
		return 0; /* not enough content => no dictionary */
	newBuff = SAlloc::M(sBuffSize + NOISELENGTH);
	if(!newBuff) 
		return ERROR(memory_allocation);
	memcpy(newBuff, samplesBuffer, sBuffSize);
	ZDICT_fillNoise((char *)newBuff + sBuffSize, NOISELENGTH); /* guard band, for end of buffer condition */
	result = ZDICT_trainFromBuffer_unsafe_legacy(dictBuffer, dictBufferCapacity, newBuff, samplesSizes, nbSamples, params);
	SAlloc::F(newBuff);
	return result;
}

size_t ZDICT_trainFromBuffer(void * dictBuffer, size_t dictBufferCapacity, const void * samplesBuffer, const size_t* samplesSizes, uint nbSamples)
{
	ZDICT_fastCover_params_t params;
	DEBUGLOG(3, "ZDICT_trainFromBuffer");
	memzero(&params, sizeof(params));
	params.d = 8;
	params.steps = 4;
	/* Use default level since no compression level information is available */
	params.zParams.compressionLevel = ZSTD_CLEVEL_DEFAULT;
#if defined(DEBUGLEVEL) && (DEBUGLEVEL>=1)
	params.zParams.notificationLevel = DEBUGLEVEL;
#endif
	return ZDICT_optimizeTrainFromBuffer_fastCover(dictBuffer, dictBufferCapacity, samplesBuffer, samplesSizes, nbSamples, &params);
}

size_t ZDICT_addEntropyTablesFromBuffer(void * dictBuffer, size_t dictContentSize, size_t dictBufferCapacity,
    const void * samplesBuffer, const size_t* samplesSizes, uint nbSamples)
{
	ZDICT_params_t params;
	memzero(&params, sizeof(params));
	return ZDICT_addEntropyTablesFromBuffer_advanced(dictBuffer, dictContentSize, dictBufferCapacity, samplesBuffer, samplesSizes, nbSamples, params);
}
