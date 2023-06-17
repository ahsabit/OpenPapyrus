// ucnv_bld.h
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 1999-2015 International Business Machines Corporation and others.  All Rights Reserved.
// Contains internal data structure definitions
// Created by Bertrand A. Damiba
// Change history:
// 06/29/2000  helena      Major rewrite of the callback APIs.
// 
#ifndef UCNV_BLD_H
#define UCNV_BLD_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_CONVERSION

#include "unicode/ucnv.h"
#include "unicode/ucnv_err.h"
#include "unicode/utf16.h"
#include "ucnv_cnv.h"
#include "ucnvmbcs.h"
#include "ucnv_ext.h"
#include "udataswp.h"

#define UCNV_ERROR_BUFFER_LENGTH 32 /* size of the overflow buffers in UConverter, enough for escaping callbacks */
#define UCNV_MAX_SUBCHAR_LEN 4 /* at most 4 bytes per substitution character (part of .cnv file format! see UConverterStaticData) */
#define UCNV_MAX_CHAR_LEN 8 /* at most 8 bytes per character in toUBytes[] (UTF-8 uses up to 6) */
/* converter options bits */
#define UCNV_OPTION_VERSION     0xf
#define UCNV_OPTION_SWAP_LFNL   0x10

#define UCNV_GET_VERSION(cnv) ((cnv)->options&UCNV_OPTION_VERSION)

U_CDECL_BEGIN // We must declare the following as 'extern "C"' so that if ucnv itself is compiled under C++, the linkage of the funcptrs will work.

union UConverterTable {
	UConverterMBCSTable mbcs;
};

typedef union UConverterTable UConverterTable;

struct UConverterImpl;

typedef struct UConverterImpl UConverterImpl;

/** values for the unicodeMask */
#define UCNV_HAS_SUPPLEMENTARY 1
#define UCNV_HAS_SURROGATES    2

typedef struct UConverterStaticData {   /* +offset: size */
	uint32_t structSize; /* +0: 4 Size of this structure */
	char name[UCNV_MAX_CONVERTER_NAME_LENGTH]; /* +4: 60  internal name of the converter- invariant chars */
	int32_t codepage; /* +64: 4 codepage # (now IBM-$codepage) */
	int8 platform; /* +68: 1 platform of the converter (only IBM now) */
	int8 conversionType; /* +69: 1 conversion type */
	int8 minBytesPerChar; /* +70: 1 Minimum # bytes per char in this codepage */
	int8 maxBytesPerChar; /* +71: 1 Maximum # bytes output per char16_t in this codepage */
	uint8 subChar[UCNV_MAX_SUBCHAR_LEN]; /* +72: 4  [note:  4 and 8 byte boundary] */
	int8 subCharLen; /* +76: 1 */
	uint8 hasToUnicodeFallback; /* +77: 1 bool needs to be changed to bool to be consistent across platform */
	uint8 hasFromUnicodeFallback; /* +78: 1 */
	uint8 unicodeMask; /* +79: 1  bit 0: has supplementary  bit 1: has single surrogates */
	uint8 subChar1; /* +80: 1  single-byte substitution character for IBM MBCS (0 if none) */
	uint8 reserved[19]; /* +81: 19 to round out the structure */
	/* total size: 100 */
} UConverterStaticData;

/*
 * Defines the UConverterSharedData struct,
 * the immutable, shared part of UConverter.
 */
struct UConverterSharedData {
	uint32_t structSize; /* Size of this structure */
	uint32_t referenceCounter; /* used to count number of clients, unused for static/immutable SharedData */
	const void * dataMemory; /* from udata_openChoice() - for cleanup */
	const UConverterStaticData * staticData; /* pointer to the static (non changing) data. */
	bool sharedDataCached; /* true:  shared data is in cache, don't destroy on ucnv_close() if 0 ref.
		false: shared data isn't in the cache, do attempt to clean it up if the ref is 0 */
	/** If false, then referenceCounter is not used. Must not change after initialization. */
	bool isReferenceCounted;
	const UConverterImpl * impl; /* vtable-style struct of mostly function pointers */
	/*initial values of some members of the mutable part of object */
	uint32_t toUnicodeStatus;
	/*
	 * Shared data structures currently come in two flavors:
	 * - readonly for built-in algorithmic converters
	 * - allocated for MBCS, with a pointer to an allocated UConverterTable
	 *   which always has a UConverterMBCSTable
	 *
	 * To eliminate one allocation, I am making the UConverterMBCSTable
	 * a member of the shared data.
	 *
	 * markus 2003-nov-07
	 */
	UConverterMBCSTable mbcs;
};

/** UConverterSharedData initializer for static, non-reference-counted converters. */
#define UCNV_IMMUTABLE_SHARED_DATA_INITIALIZER(pStaticData, pImpl) \
	{ sizeof(UConverterSharedData), ~((uint32_t)0), NULL, pStaticData, false, false, pImpl, 0, UCNV_MBCS_TABLE_INITIALIZER }

/* Defines a UConverter, the lightweight mutable part the user sees */

struct UConverter {
	/*
	 * Error function pointer called when conversion issues
	 * occur during a ucnv_fromUnicode call
	 */
	void(U_EXPORT2 *fromUCharErrorBehaviour)(const void * context, UConverterFromUnicodeArgs *args, const char16_t *codeUnits, int32_t length, UChar32 codePoint, UConverterCallbackReason reason, UErrorCode *);
	/*
	 * Error function pointer called when conversion issues
	 * occur during a ucnv_toUnicode call
	 */
	void(U_EXPORT2 *fromCharErrorBehaviour)(const void * context, UConverterToUnicodeArgs *args, const char * codeUnits, int32_t length, UConverterCallbackReason reason, UErrorCode *);
	/*
	 * Pointer to additional data that depends on the converter type.
	 * Used by ISO 2022, SCSU, GB 18030 converters, possibly more.
	 */
	void * extraInfo;
	const void * fromUContext;
	const void * toUContext;
	/*
	 * Pointer to charset bytes for substitution string if subCharLen>0,
	 * or pointer to Unicode string (char16_t *) if subCharLen<0.
	 * subCharLen==0 is equivalent to using a skip callback.
	 * If the pointer is !=subUChars then it is allocated with
	 * UCNV_ERROR_BUFFER_LENGTH * U_SIZEOF_UCHAR bytes.
	 * The subUChars field is declared as char16_t[] not uint8[] to
	 * guarantee alignment for UChars.
	 */
	uint8 * subChars;
	UConverterSharedData * sharedData; /* Pointer to the shared immutable part of the converter object */
	uint32_t options; /* options flags from UConverterOpen, may contain additional bits */
	bool  sharedDataIsCached; /* true:  shared data is in cache, don't destroy on ucnv_close() if 0 ref.  false: shared data isn't in the cache, do attempt to clean it up if the ref is 0 */
	bool  isCopyLocal; /* true if UConverter is not owned and not released in ucnv_close() (stack-allocated, safeClone(), etc.) */
	bool  isExtraLocal; /* true if extraInfo is not owned and not released in ucnv_close() (stack-allocated, safeClone(), etc.) */
	bool  useFallback;
	int8  toULength;               /* number of bytes in toUBytes */
	uint8 toUBytes[UCNV_MAX_CHAR_LEN-1]; /* more "toU status"; keeps the bytes of the current character */
	uint32_t toUnicodeStatus; /* Used to internalize stream status information */
	int32_t mode;
	uint32_t fromUnicodeStatus;
	/*
	 * More fromUnicode() status. Serves 3 purposes:
	 * - keeps a lead surrogate between buffers (similar to toUBytes[])
	 * - keeps a lead surrogate at the end of the stream,
	 *   which the framework handles as truncated input
	 * - if the fromUnicode() implementation returns to the framework
	 *   (ucnv.c ucnv_fromUnicode()), then the framework calls the callback
	 *   for this code point
	 */
	UChar32 fromUChar32;
	/*
	 * value for ucnv_getMaxCharSize()
	 *
	 * usually simply copied from the static data, but ucnvmbcs.c modifies
	 * the value depending on the converter type and options
	 */
	int8  maxBytesPerUChar;
	int8  subCharLen; /* length of the codepage specific character sequence */
	int8  invalidCharLength;
	int8  charErrorBufferLength; /* number of valid bytes in charErrorBuffer */
	int8  invalidUCharLength;
	int8  UCharErrorBufferLength; /* number of valid UChars in charErrorBuffer */
	uint8 subChar1;                               /* single-byte substitution character if different from subChar */
	bool useSubChar1;
	char invalidCharBuffer[UCNV_MAX_CHAR_LEN]; /* bytes from last error/callback situation */
	uint8 charErrorBuffer[UCNV_ERROR_BUFFER_LENGTH]; /* codepage output from Error functions */
	char16_t subUChars[UCNV_MAX_SUBCHAR_LEN/U_SIZEOF_UCHAR]; /* see subChars documentation */
	char16_t invalidUCharBuffer[U16_MAX_LENGTH]; /* UChars from last error/callback situation */
	char16_t UCharErrorBuffer[UCNV_ERROR_BUFFER_LENGTH]; /* unicode output from Error functions */
	/* fields for conversion extension */

	/* store previous UChars/chars to continue partial matches */
	UChar32 preFromUFirstCP; /* >=0: partial match */
	char16_t preFromU[UCNV_EXT_MAX_UCHARS];
	char preToU[UCNV_EXT_MAX_BYTES];
	int8 preFromULength, preToULength; /* negative: replay */
	int8 preToUFirstLength; /* length of first character */

	/* new fields for ICU 4.0 */
	UConverterCallbackReason toUCallbackReason; /* (*fromCharErrorBehaviour) reason, set when error is detected */
};

U_CDECL_END /* end of UConverter */

#define CONVERTER_FILE_EXTENSION ".cnv"

/**
 * Return the number of all converter names.
 * @param pErrorCode The error code
 * @return the number of all converter names
 */
U_CFUNC uint16 ucnv_bld_countAvailableConverters(UErrorCode * pErrorCode);

/**
 * Return the (n)th converter name in mixed case, or NULL
 * if there is none (typically, if the data cannot be loaded).
 * 0<=index<ucnv_io_countAvailableConverters().
 * @param n The number specifies which converter name to get
 * @param pErrorCode The error code
 * @return the (n)th converter name in mixed case, or NULL if there is none.
 */
U_CFUNC const char * ucnv_bld_getAvailableConverter(uint16 n, UErrorCode * pErrorCode);

/**
 * Load a non-algorithmic converter.
 * If pkg==NULL, then this function must be called inside umtx_lock(&cnvCacheMutex).
 */
U_CAPI UConverterSharedData * ucnv_load(UConverterLoadArgs * pArgs, UErrorCode * err);

/**
 * Unload a non-algorithmic converter.
 * It must be sharedData->isReferenceCounted
 * and this function must be called inside umtx_lock(&cnvCacheMutex).
 */
U_CAPI void ucnv_unload(UConverterSharedData * sharedData);
/**
 * Swap ICU .cnv conversion tables. See udataswp.h.
 * @internal
 */
U_CAPI int32_t U_EXPORT2 ucnv_swap(const UDataSwapper * ds, const void * inData, int32_t length, void * outData, UErrorCode * pErrorCode);
U_CAPI void U_EXPORT2 ucnv_enableCleanup();

#endif
#endif /* _UCNV_BLD */
