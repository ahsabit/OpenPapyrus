// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <icu-internal.h>
#pragma hdrstop

#if !UCONFIG_NO_FORMATTING

#include "number_patternstring.h"
#include "uresimp.h"

using namespace icu;
using namespace icu::number;
using namespace icu::number::impl;

namespace {
int16 getMinGroupingForLocale(const Locale & locale) {
	// TODO: Cache this?
	UErrorCode localStatus = U_ZERO_ERROR;
	LocalUResourceBundlePointer bundle(ures_open(NULL, locale.getName(), &localStatus));
	int32_t resultLen = 0;
	const char16_t* result = ures_getStringByKeyWithFallback(
		bundle.getAlias(),
		"NumberElements/minimumGroupingDigits",
		&resultLen,
		&localStatus);
	// TODO: Is it safe to assume resultLen == 1? Would locales set minGrouping >= 10?
	if(U_FAILURE(localStatus) || resultLen != 1) {
		return 1;
	}
	return result[0] - u'0';
}
}

Grouper Grouper::forStrategy(UNumberGroupingStrategy grouping) 
{
	switch(grouping) {
		case UNUM_GROUPING_OFF: return {-1, -1, -2, grouping};
		case UNUM_GROUPING_AUTO: return {-2, -2, -2, grouping};
		case UNUM_GROUPING_MIN2: return {-2, -2, -3, grouping};
		case UNUM_GROUPING_ON_ALIGNED: return {-4, -4, 1, grouping};
		case UNUM_GROUPING_THOUSANDS: return {3, 3, 1, grouping};
		default: UPRV_UNREACHABLE_EXIT;
	}
}

Grouper Grouper::forProperties(const DecimalFormatProperties& properties) 
{
	if(!properties.groupingUsed) {
		return forStrategy(UNUM_GROUPING_OFF);
	}
	auto grouping1 = static_cast<int16>(properties.groupingSize);
	auto grouping2 = static_cast<int16>(properties.secondaryGroupingSize);
	auto minGrouping = static_cast<int16>(properties.minimumGroupingDigits);
	grouping1 = grouping1 > 0 ? grouping1 : grouping2 > 0 ? grouping2 : grouping1;
	grouping2 = grouping2 > 0 ? grouping2 : grouping1;
	return {grouping1, grouping2, minGrouping, UNUM_GROUPING_COUNT};
}

void Grouper::setLocaleData(const impl::ParsedPatternInfo &patternInfo, const Locale & locale) 
{
	if(fMinGrouping == -2) {
		fMinGrouping = getMinGroupingForLocale(locale);
	}
	else if(fMinGrouping == -3) {
		fMinGrouping = static_cast<int16>(smax(2, static_cast<int>(getMinGroupingForLocale(locale))));
	}
	else {
		// leave fMinGrouping alone
	}
	if(fGrouping1 != -2 && fGrouping2 != -4) {
		return;
	}
	auto grouping1 = static_cast<int16> (patternInfo.positive.groupingSizes & 0xffff);
	auto grouping2 = static_cast<int16> ((patternInfo.positive.groupingSizes >> 16) & 0xffff);
	auto grouping3 = static_cast<int16> ((patternInfo.positive.groupingSizes >> 32) & 0xffff);
	if(grouping2 == -1) {
		grouping1 = fGrouping1 == -4 ? (short)3 : (short)-1;
	}
	if(grouping3 == -1) {
		grouping2 = grouping1;
	}
	fGrouping1 = grouping1;
	fGrouping2 = grouping2;
}

bool Grouper::groupAtPosition(int32_t position, const impl::DecimalQuantity &value) const 
{
	assert(fGrouping1 > -2);
	if(fGrouping1 == -1 || fGrouping1 == 0) {
		// Either -1 or 0 means "no grouping"
		return false;
	}
	position -= fGrouping1;
	return position >= 0 && (position % fGrouping2) == 0
	 && value.getUpperDisplayMagnitude() - fGrouping1 + 1 >= fMinGrouping;
}

int16 Grouper::getPrimary() const { return fGrouping1; }
int16 Grouper::getSecondary() const { return fGrouping2; }

#endif /* #if !UCONFIG_NO_FORMATTING */
