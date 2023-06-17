// UCSDET.CPP
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2005-2016, International Business Machines
// Corporation and others.  All Rights Reserved.
//
#include <icu-internal.h>
#pragma hdrstop

#if !UCONFIG_NO_CONVERSION
#include "unicode/ucsdet.h"
#include "csdetect.h"
#include "csrsbcs.h"
#include "csrmbcs.h"
#include "csrutf8.h"
#include "csrucode.h"
#include "csr2022.h"

U_NAMESPACE_USE

#define NEW_ARRAY(type, count) (type*)uprv_malloc((count) * sizeof(type))
#define DELETE_ARRAY(array) uprv_free((void *)(array))

U_CDECL_BEGIN

U_CAPI UCharsetDetector * U_EXPORT2 ucsdet_open(UErrorCode * status)
{
	if(U_FAILURE(*status)) {
		return 0;
	}
	CharsetDetector* csd = new CharsetDetector(*status);
	if(U_FAILURE(*status)) {
		ZDELETE(csd);
	}
	return (UCharsetDetector*)csd;
}

U_CAPI void U_EXPORT2 ucsdet_close(UCharsetDetector * ucsd)
{
	CharsetDetector * csd = (CharsetDetector *)ucsd;
	delete csd;
}

U_CAPI void U_EXPORT2 ucsdet_setText(UCharsetDetector * ucsd, const char * textIn, int32_t len, UErrorCode * status)
{
	if(U_SUCCESS(*status))
		((CharsetDetector*)ucsd)->setText(textIn, len);
}

U_CAPI const char * U_EXPORT2 ucsdet_getName(const UCharsetMatch * ucsm, UErrorCode * status)
	{ return U_SUCCESS(*status) ? ((CharsetMatch*)ucsm)->getName() : NULL; }
U_CAPI int32_t U_EXPORT2 ucsdet_getConfidence(const UCharsetMatch * ucsm, UErrorCode * status)
	{ return U_SUCCESS(*status) ? ((CharsetMatch*)ucsm)->getConfidence() : 0; }
U_CAPI const char * U_EXPORT2 ucsdet_getLanguage(const UCharsetMatch * ucsm, UErrorCode * status)
	{ return U_SUCCESS(*status) ? ((CharsetMatch*)ucsm)->getLanguage() : NULL; }
U_CAPI const UCharsetMatch * U_EXPORT2 ucsdet_detect(UCharsetDetector * ucsd, UErrorCode * status)
	{ return U_SUCCESS(*status) ? (const UCharsetMatch*)((CharsetDetector*)ucsd)->detect(*status) : NULL; }

U_CAPI void U_EXPORT2 ucsdet_setDeclaredEncoding(UCharsetDetector * ucsd, const char * encoding, int32_t length, UErrorCode * status)
{
	if(U_SUCCESS(*status))
		((CharsetDetector*)ucsd)->setDeclaredEncoding(encoding, length);
}

U_CAPI const UCharsetMatch** ucsdet_detectAll(UCharsetDetector * ucsd, int32_t * maxMatchesFound, UErrorCode * status)
{
	if(U_FAILURE(*status)) {
		return NULL;
	}
	CharsetDetector * csd = (CharsetDetector*)ucsd;
	return (const UCharsetMatch**)csd->detectAll(*maxMatchesFound, *status);
}

// U_CAPI  const char * U_EXPORT2
// ucsdet_getDetectableCharsetName(const UCharsetDetector *csd, int32_t index, UErrorCode *status)
// {
//     if(U_FAILURE(*status)) {
//         return 0;
//     }
//     return csd->getCharsetName(index,*status);
// }

// U_CAPI  int32_t U_EXPORT2
// ucsdet_getDetectableCharsetsCount(const UCharsetDetector *csd, UErrorCode *status)
// {
//     if(U_FAILURE(*status)) {
//         return -1;
//     }
//     return UCharsetDetector::getDetectableCount();
// }

U_CAPI bool U_EXPORT2 ucsdet_isInputFilterEnabled(const UCharsetDetector * ucsd)
{
	// todo: could use an error return...
	return ucsd ? ((CharsetDetector*)ucsd)->getStripTagsFlag() : FALSE;
}

U_CAPI bool U_EXPORT2 ucsdet_enableInputFilter(UCharsetDetector * ucsd, bool filter)
{
	// todo: could use an error return...
	if(ucsd == NULL) {
		return FALSE;
	}
	CharsetDetector * csd = (CharsetDetector*)ucsd;
	bool prev = csd->getStripTagsFlag();
	csd->setStripTagsFlag(filter);
	return prev;
}

U_CAPI int32_t U_EXPORT2 ucsdet_getUChars(const UCharsetMatch * ucsm, char16_t * buf, int32_t cap, UErrorCode * status)
	{ return U_SUCCESS(*status) ? ((CharsetMatch*)ucsm)->getUChars(buf, cap, status) : 0; }
U_CAPI void U_EXPORT2 ucsdet_setDetectableCharset(UCharsetDetector * ucsd, const char * encoding, bool enabled, UErrorCode * status)
	{ ((CharsetDetector*)ucsd)->setDetectableCharset(encoding, enabled, *status); }
U_CAPI UEnumeration * U_EXPORT2 ucsdet_getAllDetectableCharsets(const UCharsetDetector * /*ucsd*/, UErrorCode * status)
	{ return CharsetDetector::getAllDetectableCharsets(*status); }
U_CAPI UEnumeration * U_EXPORT2 ucsdet_getDetectableCharsets(const UCharsetDetector * ucsd,  UErrorCode * status)
	{ return ((CharsetDetector*)ucsd)->getDetectableCharsets(*status); }

U_CDECL_END

#endif
