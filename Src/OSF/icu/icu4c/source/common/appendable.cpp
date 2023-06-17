// APPENDABLE.CPP
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
// Copyright (C) 2011-2012, International Business Machines Corporation and others.  All Rights Reserved.
// @codepage UTF-8
// created on: 2010dec07
// created by: Markus W. Scherer
// 
#include <icu-internal.h>
#pragma hdrstop

U_NAMESPACE_BEGIN

Appendable::~Appendable() 
{
}

bool Appendable::appendCodePoint(UChar32 c) 
{
	return (c<=0xffff) ? appendCodeUnit((char16_t)c) : (appendCodeUnit(U16_LEAD(c)) && appendCodeUnit(U16_TRAIL(c)));
}

bool Appendable::appendString(const char16_t * s, int32_t length) 
{
	if(length<0) {
		char16_t c;
		while((c = *s++)!=0) {
			if(!appendCodeUnit(c)) {
				return FALSE;
			}
		}
	}
	else if(length > 0) {
		const char16_t * limit = s+length;
		do {
			if(!appendCodeUnit(*s++)) {
				return FALSE;
			}
		} while(s < limit);
	}
	return TRUE;
}

bool Appendable::reserveAppendCapacity(int32_t /*appendCapacity*/) { return true; }

char16_t * Appendable::getAppendBuffer(int32_t minCapacity, int32_t /*desiredCapacityHint*/, char16_t * scratch, int32_t scratchCapacity, int32_t * resultCapacity) 
{
	if(minCapacity<1 || scratchCapacity<minCapacity) {
		*resultCapacity = 0;
		return NULL;
	}
	*resultCapacity = scratchCapacity;
	return scratch;
}

// UnicodeStringAppendable is implemented in unistr.cpp.

U_NAMESPACE_END
