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
#include "zstd_ldm.h"
#include <debug.h>
#include "zstd_fast.h"          /* ZSTD_fillHashTable() */
#include "zstd_double_fast.h"   /* ZSTD_fillDoubleHashTable() */
#include "zstd_ldm_geartab.h"

#define LDM_BUCKET_SIZE_LOG 3
#define LDM_MIN_MATCH_LENGTH 64
#define LDM_HASH_RLOG 7

typedef struct {
	uint64 rolling;
	uint64 stopMask;
} ldmRollingHashState_t;

/** ZSTD_ldm_gear_init():
 *
 * Initializes the rolling hash state such that it will honor the
 * settings in params. */
static void ZSTD_ldm_gear_init(ldmRollingHashState_t* state, ldmParams_t const* params)
{
	uint maxBitsInMask = MIN(params->minMatchLength, 64);
	uint hashRateLog = params->hashRateLog;
	state->rolling = ~(uint32)0;
	/* The choice of the splitting criterion is subject to two conditions:
	 *   1. it has to trigger on average every 2^(hashRateLog) bytes;
	 *   2. ideally, it has to depend on a window of minMatchLength bytes.
	 *
	 * In the gear hash algorithm, bit n depends on the last n bytes;
	 * so in order to obtain a good quality splitting criterion it is
	 * preferable to use bits with high weight.
	 *
	 * To match condition 1 we use a mask with hashRateLog bits set
	 * and, because of the previous remark, we make sure these bits
	 * have the highest possible weight while still respecting
	 * condition 2.
	 */
	if(hashRateLog > 0 && hashRateLog <= maxBitsInMask) {
		state->stopMask = (((uint64)1 << hashRateLog) - 1) << (maxBitsInMask - hashRateLog);
	}
	else {
		/* In this degenerate case we simply honor the hash rate. */
		state->stopMask = ((uint64)1 << hashRateLog) - 1;
	}
}

/** ZSTD_ldm_gear_reset()
 * Feeds [data, data + minMatchLength) into the hash without registering any
 * splits. This effectively resets the hash state. This is used when skipping
 * over data, either at the beginning of a block, or skipping sections.
 */
static void ZSTD_ldm_gear_reset(ldmRollingHashState_t* state, BYTE const* data, size_t minMatchLength)
{
	uint64 hash = state->rolling;
	size_t n = 0;
#define GEAR_ITER_ONCE() do {                                  \
		hash = (hash << 1) + ZSTD_ldm_gearTab[data[n] & 0xff]; \
		n += 1;                                                \
} while(0)
	while(n + 3 < minMatchLength) {
		GEAR_ITER_ONCE();
		GEAR_ITER_ONCE();
		GEAR_ITER_ONCE();
		GEAR_ITER_ONCE();
	}
	while(n < minMatchLength) {
		GEAR_ITER_ONCE();
	}
#undef GEAR_ITER_ONCE
}

/** ZSTD_ldm_gear_feed():
 *
 * Registers in the splits array all the split points found in the first
 * size bytes following the data pointer. This function terminates when
 * either all the data has been processed or LDM_BATCH_SIZE splits are
 * present in the splits array.
 *
 * Precondition: The splits array must not be full.
 * Returns: The number of bytes processed. */
static size_t ZSTD_ldm_gear_feed(ldmRollingHashState_t* state, BYTE const* data, size_t size, size_t* splits, uint * numSplits)
{
	uint64 hash = state->rolling;
	uint64 mask = state->stopMask;
	size_t n = 0;
#define GEAR_ITER_ONCE() do { \
		hash = (hash << 1) + ZSTD_ldm_gearTab[data[n] & 0xff]; \
		n += 1; \
		if(UNLIKELY((hash & mask) == 0)) { \
			splits[*numSplits] = n; \
			*numSplits += 1; \
			if(*numSplits == LDM_BATCH_SIZE) \
				goto done; \
		} \
} while(0)

	while(n + 3 < size) {
		GEAR_ITER_ONCE();
		GEAR_ITER_ONCE();
		GEAR_ITER_ONCE();
		GEAR_ITER_ONCE();
	}
	while(n < size) {
		GEAR_ITER_ONCE();
	}
#undef GEAR_ITER_ONCE
done:
	state->rolling = hash;
	return n;
}

void ZSTD_ldm_adjustParameters(ldmParams_t* params, ZSTD_compressionParameters const* cParams)
{
	params->windowLog = cParams->windowLog;
	ZSTD_STATIC_ASSERT(LDM_BUCKET_SIZE_LOG <= ZSTD_LDM_BUCKETSIZELOG_MAX);
	DEBUGLOG(4, "ZSTD_ldm_adjustParameters");
	if(!params->bucketSizeLog) params->bucketSizeLog = LDM_BUCKET_SIZE_LOG;
	if(!params->minMatchLength) params->minMatchLength = LDM_MIN_MATCH_LENGTH;
	if(params->hashLog == 0) {
		params->hashLog = MAX(ZSTD_HASHLOG_MIN, params->windowLog - LDM_HASH_RLOG);
		assert(params->hashLog <= ZSTD_HASHLOG_MAX);
	}
	if(params->hashRateLog == 0) {
		params->hashRateLog = params->windowLog < params->hashLog ? 0 : params->windowLog - params->hashLog;
	}
	params->bucketSizeLog = MIN(params->bucketSizeLog, params->hashLog);
}

size_t ZSTD_ldm_getTableSize(ldmParams_t params)
{
	const size_t ldmHSize = ((size_t)1) << params.hashLog;
	const size_t ldmBucketSizeLog = MIN(params.bucketSizeLog, params.hashLog);
	const size_t ldmBucketSize = ((size_t)1) << (params.hashLog - ldmBucketSizeLog);
	const size_t totalSize = ZSTD_cwksp_alloc_size(ldmBucketSize) + ZSTD_cwksp_alloc_size(ldmHSize * sizeof(ldmEntry_t));
	return params.enableLdm == ZSTD_ps_enable ? totalSize : 0;
}

size_t ZSTD_ldm_getMaxNbSeq(ldmParams_t params, size_t maxChunkSize)
{
	return params.enableLdm == ZSTD_ps_enable ? (maxChunkSize / params.minMatchLength) : 0;
}

/** ZSTD_ldm_getBucket() :
 *  Returns a pointer to the start of the bucket associated with hash. */
static ldmEntry_t* ZSTD_ldm_getBucket(ldmState_t* ldmState, size_t hash, ldmParams_t const ldmParams)
{
	return ldmState->hashTable + (hash << ldmParams.bucketSizeLog);
}

/** ZSTD_ldm_insertEntry() :
 *  Insert the entry with corresponding hash into the hash table */
static void ZSTD_ldm_insertEntry(ldmState_t* ldmState, const size_t hash, const ldmEntry_t entry, ldmParams_t const ldmParams)
{
	BYTE * const pOffset = ldmState->bucketOffsets + hash;
	const uint offset = *pOffset;
	*(ZSTD_ldm_getBucket(ldmState, hash, ldmParams) + offset) = entry;
	*pOffset = (BYTE)((offset + 1) & ((1u << ldmParams.bucketSizeLog) - 1));
}

/** ZSTD_ldm_countBackwardsMatch() :
 *  Returns the number of bytes that match backwards before pIn and pMatch.
 *
 *  We count only bytes where pMatch >= pBase and pIn >= pAnchor. */
static size_t ZSTD_ldm_countBackwardsMatch(const BYTE * pIn, const BYTE * pAnchor, const BYTE * pMatch, const BYTE * pMatchBase)
{
	size_t matchLength = 0;
	while(pIn > pAnchor && pMatch > pMatchBase && pIn[-1] == pMatch[-1]) {
		pIn--;
		pMatch--;
		matchLength++;
	}
	return matchLength;
}

/** ZSTD_ldm_countBackwardsMatch_2segments() :
 *  Returns the number of bytes that match backwards from pMatch,
 *  even with the backwards match spanning 2 different segments.
 *
 *  On reaching `pMatchBase`, start counting from mEnd */
static size_t ZSTD_ldm_countBackwardsMatch_2segments(const BYTE * pIn, const BYTE * pAnchor,
    const BYTE * pMatch, const BYTE * pMatchBase,
    const BYTE * pExtDictStart, const BYTE * pExtDictEnd)
{
	size_t matchLength = ZSTD_ldm_countBackwardsMatch(pIn, pAnchor, pMatch, pMatchBase);
	if(pMatch - matchLength != pMatchBase || pMatchBase == pExtDictStart) {
		/* If backwards match is entirely in the extDict or prefix, immediately return */
		return matchLength;
	}
	DEBUGLOG(7, "ZSTD_ldm_countBackwardsMatch_2segments: found 2-parts backwards match (length in prefix==%zu)", matchLength);
	matchLength += ZSTD_ldm_countBackwardsMatch(pIn - matchLength, pAnchor, pExtDictEnd, pExtDictStart);
	DEBUGLOG(7, "final backwards match length = %zu", matchLength);
	return matchLength;
}

/** ZSTD_ldm_fillFastTables() :
 *
 *  Fills the relevant tables for the ZSTD_fast and ZSTD_dfast strategies.
 *  This is similar to ZSTD_loadDictionaryContent.
 *
 *  The tables for the other strategies are filled within their
 *  block compressors. */
static size_t ZSTD_ldm_fillFastTables(ZSTD_matchState_t* ms, void const* end)
{
	const BYTE * const iend = (const BYTE *)end;
	switch(ms->cParams.strategy) {
		case ZSTD_fast:
		    ZSTD_fillHashTable(ms, iend, ZSTD_dtlm_fast);
		    break;

		case ZSTD_dfast:
		    ZSTD_fillDoubleHashTable(ms, iend, ZSTD_dtlm_fast);
		    break;

		case ZSTD_greedy:
		case ZSTD_lazy:
		case ZSTD_lazy2:
		case ZSTD_btlazy2:
		case ZSTD_btopt:
		case ZSTD_btultra:
		case ZSTD_btultra2:
		    break;
		default:
		    assert(0); /* not possible : not a valid strategy id */
	}

	return 0;
}

void ZSTD_ldm_fillHashTable(ldmState_t* ldmState, const BYTE * ip, const BYTE * iend, ldmParams_t const* params)
{
	const uint32 minMatchLength = params->minMatchLength;
	const uint32 hBits = params->hashLog - params->bucketSizeLog;
	BYTE const* const base = ldmState->window.base;
	BYTE const* const istart = ip;
	ldmRollingHashState_t hashState;
	size_t* const splits = ldmState->splitIndices;
	uint numSplits;
	DEBUGLOG(5, "ZSTD_ldm_fillHashTable");
	ZSTD_ldm_gear_init(&hashState, params);
	while(ip < iend) {
		size_t hashed;
		uint n;
		numSplits = 0;
		hashed = ZSTD_ldm_gear_feed(&hashState, ip, iend - ip, splits, &numSplits);
		for(n = 0; n < numSplits; n++) {
			if(ip + splits[n] >= istart + minMatchLength) {
				BYTE const* const split = ip + splits[n] - minMatchLength;
				uint64 const xxhash = XXH64(split, minMatchLength, 0);
				const uint32 hash = (uint32)(xxhash & (((uint32)1 << hBits) - 1));
				ldmEntry_t entry;

				entry.offset = (uint32)(split - base);
				entry.checksum = (uint32)(xxhash >> 32);
				ZSTD_ldm_insertEntry(ldmState, hash, entry, *params);
			}
		}

		ip += hashed;
	}
}

/** ZSTD_ldm_limitTableUpdate() :
 *
 *  Sets cctx->nextToUpdate to a position corresponding closer to anchor
 *  if it is far way
 *  (after a long match, only update tables a limited amount). */
static void ZSTD_ldm_limitTableUpdate(ZSTD_matchState_t* ms, const BYTE * anchor)
{
	const uint32 curr = (uint32)(anchor - ms->window.base);
	if(curr > ms->nextToUpdate + 1024) {
		ms->nextToUpdate =
		    curr - MIN(512, curr - ms->nextToUpdate - 1024);
	}
}

static size_t ZSTD_ldm_generateSequences_internal(ldmState_t* ldmState, rawSeqStore_t* rawSeqStore,
    ldmParams_t const* params, void const* src, size_t srcSize)
{
	/* LDM parameters */
	int const extDict = ZSTD_window_hasExtDict(ldmState->window);
	const uint32 minMatchLength = params->minMatchLength;
	const uint32 entsPerBucket = 1U << params->bucketSizeLog;
	const uint32 hBits = params->hashLog - params->bucketSizeLog;
	/* Prefix and extDict parameters */
	const uint32 dictLimit = ldmState->window.dictLimit;
	const uint32 lowestIndex = extDict ? ldmState->window.lowLimit : dictLimit;
	BYTE const* const base = ldmState->window.base;
	BYTE const* const dictBase = extDict ? ldmState->window.dictBase : NULL;
	BYTE const* const dictStart = extDict ? dictBase + lowestIndex : NULL;
	BYTE const* const dictEnd = extDict ? dictBase + dictLimit : NULL;
	BYTE const* const lowPrefixPtr = base + dictLimit;
	/* Input bounds */
	BYTE const* const istart = (BYTE const*)src;
	BYTE const* const iend = istart + srcSize;
	BYTE const* const ilimit = iend - HASH_READ_SIZE;
	/* Input positions */
	BYTE const* anchor = istart;
	BYTE const* ip = istart;
	/* Rolling hash state */
	ldmRollingHashState_t hashState;
	/* Arrays for staged-processing */
	size_t* const splits = ldmState->splitIndices;
	ldmMatchCandidate_t* const candidates = ldmState->matchCandidates;
	uint numSplits;
	if(srcSize < minMatchLength)
		return iend - anchor;
	/* Initialize the rolling hash state with the first minMatchLength bytes */
	ZSTD_ldm_gear_init(&hashState, params);
	ZSTD_ldm_gear_reset(&hashState, ip, minMatchLength);
	ip += minMatchLength;
	while(ip < ilimit) {
		size_t hashed;
		uint n;
		numSplits = 0;
		hashed = ZSTD_ldm_gear_feed(&hashState, ip, ilimit - ip, splits, &numSplits);
		for(n = 0; n < numSplits; n++) {
			BYTE const* const split = ip + splits[n] - minMatchLength;
			uint64 const xxhash = XXH64(split, minMatchLength, 0);
			const uint32 hash = (uint32)(xxhash & (((uint32)1 << hBits) - 1));

			candidates[n].split = split;
			candidates[n].hash = hash;
			candidates[n].checksum = (uint32)(xxhash >> 32);
			candidates[n].bucket = ZSTD_ldm_getBucket(ldmState, hash, *params);
			PREFETCH_L1(candidates[n].bucket);
		}

		for(n = 0; n < numSplits; n++) {
			size_t forwardMatchLength = 0;
			size_t backwardMatchLength = 0;
			size_t bestMatchLength = 0;
			size_t mLength;
			uint32 offset;
			BYTE const* const split = candidates[n].split;
			const uint32 checksum = candidates[n].checksum;
			const uint32 hash = candidates[n].hash;
			ldmEntry_t* const bucket = candidates[n].bucket;
			ldmEntry_t const* cur;
			ldmEntry_t const* bestEntry = NULL;
			ldmEntry_t newEntry;
			newEntry.offset = (uint32)(split - base);
			newEntry.checksum = checksum;
			/* If a split point would generate a sequence overlapping with
			 * the previous one, we merely register it in the hash table and
			 * move on */
			if(split < anchor) {
				ZSTD_ldm_insertEntry(ldmState, hash, newEntry, *params);
				continue;
			}
			for(cur = bucket; cur < bucket + entsPerBucket; cur++) {
				size_t curForwardMatchLength;
				size_t curBackwardMatchLength;
				size_t curTotalMatchLength;
				if(cur->checksum != checksum || cur->offset <= lowestIndex) {
					continue;
				}
				if(extDict) {
					BYTE const* const curMatchBase = cur->offset < dictLimit ? dictBase : base;
					BYTE const* const pMatch = curMatchBase + cur->offset;
					BYTE const* const matchEnd = cur->offset < dictLimit ? dictEnd : iend;
					BYTE const* const lowMatchPtr = cur->offset < dictLimit ? dictStart : lowPrefixPtr;
					curForwardMatchLength = ZSTD_count_2segments(split, pMatch, iend, matchEnd, lowPrefixPtr);
					if(curForwardMatchLength < minMatchLength) {
						continue;
					}
					curBackwardMatchLength = ZSTD_ldm_countBackwardsMatch_2segments(
						split, anchor, pMatch, lowMatchPtr, dictStart, dictEnd);
				}
				else { /* !extDict */
					BYTE const* const pMatch = base + cur->offset;
					curForwardMatchLength = ZSTD_count(split, pMatch, iend);
					if(curForwardMatchLength < minMatchLength) {
						continue;
					}
					curBackwardMatchLength = ZSTD_ldm_countBackwardsMatch(split, anchor, pMatch, lowPrefixPtr);
				}
				curTotalMatchLength = curForwardMatchLength + curBackwardMatchLength;
				if(curTotalMatchLength > bestMatchLength) {
					bestMatchLength = curTotalMatchLength;
					forwardMatchLength = curForwardMatchLength;
					backwardMatchLength = curBackwardMatchLength;
					bestEntry = cur;
				}
			}
			// No match found -- insert an entry into the hash table and process the next candidate match 
			if(bestEntry == NULL) {
				ZSTD_ldm_insertEntry(ldmState, hash, newEntry, *params);
				continue;
			}
			/* Match found */
			offset = (uint32)(split - base) - bestEntry->offset;
			mLength = forwardMatchLength + backwardMatchLength;
			{
				rawSeq* const seq = rawSeqStore->seq + rawSeqStore->size;
				/* Out of sequence storage */
				if(rawSeqStore->size == rawSeqStore->capacity)
					return ERROR(dstSize_tooSmall);
				seq->litLength = (uint32)(split - backwardMatchLength - anchor);
				seq->matchLength = (uint32)mLength;
				seq->offset = offset;
				rawSeqStore->size++;
			}
			/* Insert the current entry into the hash table --- it must be
			 * done after the previous block to avoid clobbering bestEntry */
			ZSTD_ldm_insertEntry(ldmState, hash, newEntry, *params);
			anchor = split + forwardMatchLength;
			/* If we find a match that ends after the data that we've hashed
			 * then we have a repeating, overlapping, pattern. E.g. all zeros.
			 * If one repetition of the pattern matches our `stopMask` then all
			 * repetitions will. We don't need to insert them all into out table,
			 * only the first one. So skip over overlapping matches.
			 * This is a major speed boost (20x) for compressing a single byte
			 * repeated, when that byte ends up in the table.
			 */
			if(anchor > ip + hashed) {
				ZSTD_ldm_gear_reset(&hashState, anchor - minMatchLength, minMatchLength);
				/* Continue the outer loop at anchor (ip + hashed == anchor). */
				ip = anchor - hashed;
				break;
			}
		}
		ip += hashed;
	}
	return iend - anchor;
}

/*! ZSTD_ldm_reduceTable() :
 *  reduce table indexes by `reducerValue` */
static void ZSTD_ldm_reduceTable(ldmEntry_t* const table, const uint32 size, const uint32 reducerValue)
{
	for(uint32 u = 0; u < size; u++) {
		if(table[u].offset < reducerValue) 
			table[u].offset = 0;
		else 
			table[u].offset -= reducerValue;
	}
}

size_t ZSTD_ldm_generateSequences(ldmState_t* ldmState, rawSeqStore_t* sequences, ldmParams_t const* params, void const* src, size_t srcSize)
{
	const uint32 maxDist = 1U << params->windowLog;
	BYTE const* const istart = (BYTE const*)src;
	BYTE const* const iend = istart + srcSize;
	const size_t kMaxChunkSize = 1 << 20;
	const size_t nbChunks = (srcSize / kMaxChunkSize) + ((srcSize % kMaxChunkSize) != 0);
	size_t chunk;
	size_t leftoverSize = 0;
	assert(ZSTD_CHUNKSIZE_MAX >= kMaxChunkSize);
	/* Check that ZSTD_window_update() has been called for this chunk prior
	 * to passing it to this function.
	 */
	assert(ldmState->window.nextSrc >= (BYTE const*)src + srcSize);
	/* The input could be very large (in zstdmt), so it must be broken up into
	 * chunks to enforce the maximum distance and handle overflow correction.
	 */
	assert(sequences->pos <= sequences->size);
	assert(sequences->size <= sequences->capacity);
	for(chunk = 0; chunk < nbChunks && sequences->size < sequences->capacity; ++chunk) {
		BYTE const* const chunkStart = istart + chunk * kMaxChunkSize;
		const size_t remaining = (size_t)(iend - chunkStart);
		BYTE const * const chunkEnd = (remaining < kMaxChunkSize) ? iend : chunkStart + kMaxChunkSize;
		const size_t chunkSize = chunkEnd - chunkStart;
		size_t newLeftoverSize;
		const size_t prevSize = sequences->size;

		assert(chunkStart < iend);
		/* 1. Perform overflow correction if necessary. */
		if(ZSTD_window_needOverflowCorrection(ldmState->window, 0, maxDist, ldmState->loadedDictEnd, chunkStart, chunkEnd)) {
			const uint32 ldmHSize = 1U << params->hashLog;
			const uint32 correction = ZSTD_window_correctOverflow(&ldmState->window, /* cycleLog */ 0, maxDist, chunkStart);
			ZSTD_ldm_reduceTable(ldmState->hashTable, ldmHSize, correction);
			/* invalidate dictionaries on overflow correction */
			ldmState->loadedDictEnd = 0;
		}
		/* 2. We enforce the maximum offset allowed.
		 *
		 * kMaxChunkSize should be small enough that we don't lose too much of
		 * the window through early invalidation.
		 * TODO: * Test the chunk size.
		 *       * Try invalidation after the sequence generation and test the
		 *         offset against maxDist directly.
		 *
		 * NOTE: Because of dictionaries + sequence splitting we MUST make sure
		 * that any offset used is valid at the END of the sequence, since it may
		 * be split into two sequences. This condition holds when using
		 * ZSTD_window_enforceMaxDist(), but if we move to checking offsets
		 * against maxDist directly, we'll have to carefully handle that case.
		 */
		ZSTD_window_enforceMaxDist(&ldmState->window, chunkEnd, maxDist, &ldmState->loadedDictEnd, NULL);
		/* 3. Generate the sequences for the chunk, and get newLeftoverSize. */
		newLeftoverSize = ZSTD_ldm_generateSequences_internal(
			ldmState, sequences, params, chunkStart, chunkSize);
		if(ZSTD_isError(newLeftoverSize))
			return newLeftoverSize;
		/* 4. We add the leftover literals from previous iterations to the first
		 *    newly generated sequence, or add the `newLeftoverSize` if none are
		 *    generated.
		 */
		/* Prepend the leftover literals from the last call */
		if(prevSize < sequences->size) {
			sequences->seq[prevSize].litLength += (uint32)leftoverSize;
			leftoverSize = newLeftoverSize;
		}
		else {
			assert(newLeftoverSize == chunkSize);
			leftoverSize += chunkSize;
		}
	}
	return 0;
}

void ZSTD_ldm_skipSequences(rawSeqStore_t* rawSeqStore, size_t srcSize, const uint32 minMatch)
{
	while(srcSize > 0 && rawSeqStore->pos < rawSeqStore->size) {
		rawSeq* seq = rawSeqStore->seq + rawSeqStore->pos;
		if(srcSize <= seq->litLength) {
			/* Skip past srcSize literals */
			seq->litLength -= (uint32)srcSize;
			return;
		}
		srcSize -= seq->litLength;
		seq->litLength = 0;
		if(srcSize < seq->matchLength) {
			/* Skip past the first srcSize of the match */
			seq->matchLength -= (uint32)srcSize;
			if(seq->matchLength < minMatch) {
				/* The match is too short, omit it */
				if(rawSeqStore->pos + 1 < rawSeqStore->size) {
					seq[1].litLength += seq[0].matchLength;
				}
				rawSeqStore->pos++;
			}
			return;
		}
		srcSize -= seq->matchLength;
		seq->matchLength = 0;
		rawSeqStore->pos++;
	}
}

/**
 * If the sequence length is longer than remaining then the sequence is split
 * between this block and the next.
 *
 * Returns the current sequence to handle, or if the rest of the block should
 * be literals, it returns a sequence with offset == 0.
 */
static rawSeq maybeSplitSequence(rawSeqStore_t* rawSeqStore, const uint32 remaining, const uint32 minMatch)
{
	rawSeq sequence = rawSeqStore->seq[rawSeqStore->pos];
	assert(sequence.offset > 0);
	/* Likely: No partial sequence */
	if(remaining >= sequence.litLength + sequence.matchLength) {
		rawSeqStore->pos++;
		return sequence;
	}
	/* Cut the sequence short (offset == 0 ==> rest is literals). */
	if(remaining <= sequence.litLength) {
		sequence.offset = 0;
	}
	else if(remaining < sequence.litLength + sequence.matchLength) {
		sequence.matchLength = remaining - sequence.litLength;
		if(sequence.matchLength < minMatch) {
			sequence.offset = 0;
		}
	}
	/* Skip past `remaining` bytes for the future sequences. */
	ZSTD_ldm_skipSequences(rawSeqStore, remaining, minMatch);
	return sequence;
}

void ZSTD_ldm_skipRawSeqStoreBytes(rawSeqStore_t* rawSeqStore, size_t nbBytes) 
{
	uint32 currPos = (uint32)(rawSeqStore->posInSequence + nbBytes);
	while(currPos && rawSeqStore->pos < rawSeqStore->size) {
		rawSeq currSeq = rawSeqStore->seq[rawSeqStore->pos];
		if(currPos >= currSeq.litLength + currSeq.matchLength) {
			currPos -= currSeq.litLength + currSeq.matchLength;
			rawSeqStore->pos++;
		}
		else {
			rawSeqStore->posInSequence = currPos;
			break;
		}
	}
	if(currPos == 0 || rawSeqStore->pos == rawSeqStore->size) {
		rawSeqStore->posInSequence = 0;
	}
}

size_t ZSTD_ldm_blockCompress(rawSeqStore_t* rawSeqStore,
    ZSTD_matchState_t* ms, seqStore_t* seqStore, uint32 rep[ZSTD_REP_NUM],
    ZSTD_paramSwitch_e useRowMatchFinder, void const* src, size_t srcSize)
{
	const ZSTD_compressionParameters* const cParams = &ms->cParams;
	const uint minMatch = cParams->minMatch;
	ZSTD_blockCompressor const blockCompressor = ZSTD_selectBlockCompressor(cParams->strategy, useRowMatchFinder, ZSTD_matchState_dictMode(ms));
	/* Input bounds */
	BYTE const* const istart = (BYTE const*)src;
	BYTE const* const iend = istart + srcSize;
	/* Input positions */
	BYTE const* ip = istart;

	DEBUGLOG(5, "ZSTD_ldm_blockCompress: srcSize=%zu", srcSize);
	/* If using opt parser, use LDMs only as candidates rather than always accepting them */
	if(cParams->strategy >= ZSTD_btopt) {
		size_t lastLLSize;
		ms->ldmSeqStore = rawSeqStore;
		lastLLSize = blockCompressor(ms, seqStore, rep, src, srcSize);
		ZSTD_ldm_skipRawSeqStoreBytes(rawSeqStore, srcSize);
		return lastLLSize;
	}

	assert(rawSeqStore->pos <= rawSeqStore->size);
	assert(rawSeqStore->size <= rawSeqStore->capacity);
	/* Loop through each sequence and apply the block compressor to the literals */
	while(rawSeqStore->pos < rawSeqStore->size && ip < iend) {
		/* maybeSplitSequence updates rawSeqStore->pos */
		rawSeq const sequence = maybeSplitSequence(rawSeqStore,
			(uint32)(iend - ip), minMatch);
		int i;
		/* End signal */
		if(sequence.offset == 0)
			break;

		assert(ip + sequence.litLength + sequence.matchLength <= iend);

		/* Fill tables for block compressor */
		ZSTD_ldm_limitTableUpdate(ms, ip);
		ZSTD_ldm_fillFastTables(ms, ip);
		/* Run the block compressor */
		DEBUGLOG(5, "pos %u : calling block compressor on segment of size %u", (uint)(ip-istart), sequence.litLength);
		{
			const size_t newLitLength =
			    blockCompressor(ms, seqStore, rep, ip, sequence.litLength);
			ip += sequence.litLength;
			/* Update the repcodes */
			for(i = ZSTD_REP_NUM - 1; i > 0; i--)
				rep[i] = rep[i-1];
			rep[0] = sequence.offset;
			/* Store the sequence */
			ZSTD_storeSeq(seqStore, newLitLength, ip - newLitLength, iend,
			    OFFSET_TO_OFFBASE(sequence.offset),
			    sequence.matchLength);
			ip += sequence.matchLength;
		}
	}
	/* Fill the tables for the block compressor */
	ZSTD_ldm_limitTableUpdate(ms, ip);
	ZSTD_ldm_fillFastTables(ms, ip);
	/* Compress the last literals */
	return blockCompressor(ms, seqStore, rep, ip, iend - ip);
}
