/** @file
 * @brief Pack types into strings and unpack them again.
 */
/* Copyright (C) 2009,2015,2016,2017,2018 Olly Betts
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef XAPIAN_INCLUDED_PACK_H
#define XAPIAN_INCLUDED_PACK_H

#ifndef PACKAGE
	#error config.h must be included first in each C++ source file
#endif

/** How many bits to store the length of a sortable uint in.
 *
 *  Setting this to 2 limits us to 2**32 documents in the database.  If set
 *  to 3, then 2**64 documents are possible, but the database format isn't
 *  compatible.
 */
const uint SORTABLE_UINT_LOG2_MAX_BYTES = 2;
const uint SORTABLE_UINT_MAX_BYTES = 1 << SORTABLE_UINT_LOG2_MAX_BYTES; /// Calculated value used below.
const uint SORTABLE_UINT_1ST_BYTE_MASK = (0xffu >> SORTABLE_UINT_LOG2_MAX_BYTES); /// Calculated value used below.

/** Throw appropriate SerialisationError.
 *
 *  @param p If NULL, out of data; otherwise type overflow.
 */
[[noreturn]] void unpack_throw_serialisation_error(const char * p);

/** Append an encoded bool to a string.
 *
 *  @param s		The string to append to.
 *  @param value	The bool to encode.
 */
inline void pack_bool(std::string & s, bool value)
{
	s += char('0' | static_cast<char>(value));
}

/** Decode a bool from a string.
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result.
 */
inline bool unpack_bool(const char ** p, const char * end, bool * result)
{
	Assert(result);
	const char * & ptr = *p;
	Assert(ptr);
	char ch;
	if(UNLIKELY(ptr == end || ((ch = *ptr++ - '0') &~1))) {
		ptr = NULL;
		return false;
	}
	*result = static_cast<bool>(ch);
	return true;
}

/** Append an encoded unsigned integer to a string as the last item.
 *
 *  This encoding is only suitable when this is the last thing encoded as
 *  the encoding used doesn't contain its own length.
 *
 *  @param s		The string to append to.
 *  @param value	The unsigned integer to encode.
 */
template <class U> inline void pack_uint_last(std::string & s, U value)
{
	static_assert(std::is_unsigned<U>::value, "Unsigned type required");
	while(value) {
		s += char(value & 0xff);
		value >>= 8;
	}
}
/** Decode an unsigned integer as the last item in a string.
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result.
 */
template <class U> inline bool unpack_uint_last(const char ** p, const char * end, U * result)
{
	static_assert(std::is_unsigned<U>::value, "Unsigned type required");
	Assert(result);
	const char * ptr = *p;
	Assert(ptr);
	*p = end;
	// Check for overflow.
	if(UNLIKELY(end - ptr > int(sizeof(U)))) {
		return false;
	}
	*result = 0;
	while(end != ptr) {
		*result = (*result << 8) | U(static_cast<uchar>(*--end));
	}
	return true;
}

#if HAVE_DECL___BUILTIN_CLZ && HAVE_DECL___BUILTIN_CLZL && HAVE_DECL___BUILTIN_CLZLL
	template <typename T> inline int do_clz(T value) 
	{
		extern int no_clz_builtin_for_this_type(T);
		return no_clz_builtin_for_this_type(value);
	}
	template <> inline int do_clz(uint value) { return __builtin_clz(value); }
	template <> inline int do_clz(ulong value) { return __builtin_clzl(value); }
	template <> inline int do_clz(ulong long value) { return __builtin_clzll(value); }
	#define HAVE_DO_CLZ
#endif

/** Append an encoded unsigned integer to a string, preserving the sort order.
 *
 *  The appended string data will sort in the same order as the unsigned
 *  integer being encoded.
 *
 *  Note that the first byte of the encoding will never be \xff, so it is
 *  safe to store the result of this function immediately after the result of
 *  pack_string_preserving_sort().
 *
 *  @param s		The string to append to.
 *  @param value	The unsigned integer to encode.
 */
template <class U> inline void pack_uint_preserving_sort(std::string & s, U value)
{
	static_assert(std::is_unsigned<U>::value, "Unsigned type required");
	static_assert(sizeof(U) <= 8, "Template type U too wide for database format");
	// The clz() functions are undefined for 0, so handle the smallest band
	// as a special case.
	if(value < 0x8000) {
		s.resize(s.size() + 2);
		s[s.size() - 2] = static_cast<uchar>(value >> 8);
		Assert(s[s.size() - 2] != '\xff');
		s[s.size() - 1] = static_cast<uchar>(value);
		return;
	}

#ifdef HAVE_DO_CLZ
	size_t len = ((sizeof(U) * 8 + 5) - do_clz(value)) / 7;
#else
	size_t len = 3;
	for(U x = value >> 22; x; x >>= 7) 
		++len;
#endif
	uint mask = 0xff << (10 - len);
	s.resize(s.size() + len);
	for(size_t i = 1; i != len; ++i) {
		s[s.size() - i] = static_cast<uchar>(value);
		value >>= 8;
	}

	s[s.size() - len] = static_cast<uchar>(value | mask);
	Assert(s[s.size() - len] != '\xff');

	AssertRel(len, >, 2);
	AssertRel(len, <=, 9);
}

/** Decode an "sort preserved" unsigned integer from a string.
 *
 *  The unsigned integer must have been encoded with
 *  pack_uint_preserving_sort().
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result.
 */
template <class U> inline bool unpack_uint_preserving_sort(const char ** p, const char * end, U * result)
{
	static_assert(std::is_unsigned<U>::value, "Unsigned type required");
	static_assert(sizeof(U) <= 8, "Template type U too wide for database format");
	Assert(result);
	const char * ptr = *p;
	Assert(ptr);
	if(UNLIKELY(ptr == end)) {
		return false;
	}
	uchar len_byte = static_cast<uchar>(*ptr++);
	if(len_byte < 0x80) {
		*result = (U(len_byte) << 8) | static_cast<uchar>(*ptr++);
		*p = ptr;
		return true;
	}
	if(len_byte == 0xff) {
		return false;
	}
	// len is how many bytes there are after the length byte.
#ifdef HAVE_DO_CLZ
	size_t len = do_clz(len_byte ^ 0xffu) + 9 - sizeof(uint) * 8;
#else
	size_t len = 2;
	for(uchar m = 0x40; len_byte &m; m >>= 1) ++len;
#endif
	if(UNLIKELY(size_t(end - ptr) < len)) {
		return false;
	}
	uint mask = 0xff << (9 - len);
	len_byte &= ~mask;
	// Check for overflow.
	if(UNLIKELY(len > int(sizeof(U)))) return false;
	if(sizeof(U) != 8) {
		// Need to check the top byte too.
		if(UNLIKELY(len == int(sizeof(U)) && len_byte != 0)) 
			return false;
	}
	end = ptr + len;
	*p = end;
	U r = len_byte;
	while(ptr != end) {
		r = (r << 8) | U(static_cast<uchar>(*ptr++));
	}
	*result = r;
	return true;
}
/** Append an encoded unsigned integer to a string.
 *
 *  @param s		The string to append to.
 *  @param value	The unsigned integer to encode.
 */
template <class U> inline void pack_uint(std::string & s, U value)
{
	static_assert(std::is_unsigned<U>::value, "Unsigned type required");
	while(value >= 128) {
		s += static_cast<char>(static_cast<uchar>(value) | 0x80);
		value >>= 7;
	}
	s += static_cast<char>(value);
}

/** Append an encoded unsigned integer (bool type) to a string.
 *
 *  @param s		The string to append to.
 *  @param value	The unsigned integer to encode.
 */
template <> inline void pack_uint(std::string & s, bool value)
{
	s += static_cast<char>(value);
}

/** Decode an unsigned integer from a string.
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result (or NULL to just skip it).
 */
template <class U> inline bool unpack_uint(const char ** p, const char * end, U * result)
{
	static_assert(std::is_unsigned<U>::value, "Unsigned type required");
	const char * ptr = *p;
	Assert(ptr);
	const char * start = ptr;
	// Check the length of the encoded integer first.
	do {
		if(UNLIKELY(ptr == end)) {
			// Out of data.
			*p = NULL;
			return false;
		}
	} while(static_cast<uchar>(*ptr++) >= 128);

	*p = ptr;

	if(!result) return true;

	*result = U(*--ptr);
	if(ptr == start) {
		// Special case for small values.
		return true;
	}

	size_t maxbits = size_t(ptr - start) * 7;
	if(maxbits <= sizeof(U) * 8) {
		// No possibility of overflow.
		do {
			uchar chunk = static_cast<uchar>(*--ptr) & 0x7f;
			*result = (*result << 7) | U(chunk);
		} while(ptr != start);
		return true;
	}

	size_t minbits = maxbits - 6;
	if(UNLIKELY(minbits > sizeof(U) * 8)) {
		// Overflow.
		return false;
	}

	while(--ptr != start) {
		uchar chunk = static_cast<uchar>(*--ptr) & 0x7f;
		*result = (*result << 7) | U(chunk);
	}

	U tmp = *result;
	*result <<= 7;
	if(UNLIKELY(*result < tmp)) {
		// Overflow.
		return false;
	}
	*result |= U(static_cast<uchar>(*ptr) & 0x7f);
	return true;
}
/** Decode an unsigned integer from a string, going backwards.
 *
 *  @param p	    Pointer to pointer just after the position in the string.
 *  @param start    Pointer to the start of the string.
 *  @param result   Where to store the result (or NULL to just skip it).
 */
template <class U> inline bool unpack_uint_backwards(const char ** p, const char * start, U * result)
{
	static_assert(std::is_unsigned<U>::value, "Unsigned type required");
	const char * ptr = *p;
	Assert(ptr);
	// Check it's not empty and that the final byte is valid.
	if(UNLIKELY(ptr == start || static_cast<uchar>(ptr[-1]) >= 128)) {
		// Out of data.
		*p = NULL;
		return false;
	}
	do {
		if(UNLIKELY(--ptr == start))
			break;
	} while(static_cast<uchar>(ptr[-1]) >= 128);
	const char * end = *p;
	*p = ptr;
	return unpack_uint(&ptr, end, result);
}

/** Append an encoded std::string to a string.
 *
 *  @param s		The string to append to.
 *  @param value	The std::string to encode.
 */
inline void pack_string(std::string & s, const std::string & value)
{
	pack_uint(s, value.size());
	s += value;
}

/** Append an empty encoded std::string to a string.
 *
 *  This is equivalent to pack_string(s, std::string()) but is probably a bit
 *  more efficient.
 *
 *  @param s		The string to append to.
 */
inline void pack_string_empty(std::string & s)
{
	s += '\0';
}

/** Append an encoded C-style string to a string.
 *
 *  @param s		The string to append to.
 *  @param ptr		The C-style string to encode.
 */
inline void pack_string(std::string & s, const char * ptr)
{
	Assert(ptr);
	size_t len = sstrlen(ptr);
	pack_uint(s, len);
	s.append(ptr, len);
}

/** Decode a std::string from a string.
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result.
 */
inline bool unpack_string(const char ** p, const char * end, std::string & result)
{
	size_t len;
	if(UNLIKELY(!unpack_uint(p, end, &len))) {
		return false;
	}

	const char * & ptr = *p;
	if(UNLIKELY(len > size_t(end - ptr))) {
		ptr = NULL;
		return false;
	}

	result.assign(ptr, len);
	ptr += len;
	return true;
}

/** Decode a std::string from a string and append.
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result.
 */
inline bool unpack_string_append(const char ** p, const char * end, std::string & result)
{
	size_t len;
	if(UNLIKELY(!unpack_uint(p, end, &len))) {
		return false;
	}
	const char * & ptr = *p;
	if(UNLIKELY(len > size_t(end - ptr))) {
		ptr = NULL;
		return false;
	}
	result.append(ptr, len);
	ptr += len;
	return true;
}

/** Append an encoded std::string to a string, preserving the sort order.
 *
 *  The byte which follows this encoded value *must not* be \xff, or the sort
 *  order won't be correct.  You may need to store a padding byte (\0 say) to
 *  ensure this.  Note that pack_uint_preserving_sort() can never produce
 *  \xff as its first byte so is safe to use immediately afterwards.
 *
 *  @param s		The string to append to.
 *  @param value	The std::string to encode.
 *  @param last		If true, this is the last thing to be encoded in this
 *			string - see note below (default: false)
 *
 *  It doesn't make sense to use pack_string_preserving_sort() if nothing can
 *  ever follow, but if optional items can, you can set last=true in cases
 *  where nothing does and get a shorter encoding in those cases.
 */
inline void pack_string_preserving_sort(std::string & s, const std::string & value,
    bool last = false)
{
	std::string::size_type b = 0, e;
	while((e = value.find('\0', b)) != std::string::npos) {
		++e;
		s.append(value, b, e - b);
		s += '\xff';
		b = e;
	}
	s.append(value, b, std::string::npos);
	if(!last) s += '\0';
}

/** Decode a "sort preserved" std::string from a string.
 *
 *  The std::string must have been encoded with pack_string_preserving_sort().
 *
 *  @param p	    Pointer to pointer to the current position in the string.
 *  @param end	    Pointer to the end of the string.
 *  @param result   Where to store the result.
 */
inline bool unpack_string_preserving_sort(const char ** p, const char * end,
    std::string & result)
{
	result.resize(0);

	const char * ptr = *p;
	Assert(ptr);

	while(ptr != end) {
		char ch = *ptr++;
		if(UNLIKELY(ch == '\0')) {
			if(LIKELY(ptr == end || *ptr != '\xff')) {
				break;
			}
			++ptr;
		}
		result += ch;
	}
	*p = ptr;
	return true;
}

inline std::string pack_glass_postlist_key(const std::string &term)
{
	// Special case for doclen lists.
	if(term.empty())
		return std::string("\x00\xe0", 2);

	std::string key;
	pack_string_preserving_sort(key, term, true);
	return key;
}

inline std::string pack_glass_postlist_key(const std::string &term, Xapian::docid did)
{
	// Special case for doclen lists.
	if(term.empty()) {
		std::string key("\x00\xe0", 2);
		pack_uint_preserving_sort(key, did);
		return key;
	}

	std::string key;
	pack_string_preserving_sort(key, term);
	pack_uint_preserving_sort(key, did);
	return key;
}

inline std::string pack_honey_postlist_key(const std::string & term)
{
	Assert(!term.empty());
	std::string key;
	pack_string_preserving_sort(key, term, true);
	return key;
}

inline std::string pack_honey_postlist_key(const std::string & term, Xapian::docid did)
{
	Assert(!term.empty());
	std::string key;
	pack_string_preserving_sort(key, term);
	pack_uint_preserving_sort(key, did);
	return key;
}

#endif // XAPIAN_INCLUDED_PACK_H
