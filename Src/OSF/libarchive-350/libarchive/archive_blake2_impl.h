/*
   BLAKE2 reference source code package - reference C implementations

   Copyright 2012, Samuel Neves <sneves@dei.uc.pt>.  You may use this under the
   terms of the CC0, the OpenSSL Licence, or the Apache Public License 2.0, at
   your option.  The terms of these licenses can be found at:

   - CC0 1.0 Universal : http://creativecommons.org/publicdomain/zero/1.0
   - OpenSSL license   : https://www.openssl.org/source/license.html
   - Apache 2.0        : http://www.apache.org/licenses/LICENSE-2.0

   More information about the BLAKE2 hash function can be found at
   https://blake2.net.
 */

#ifndef ARCHIVE_BLAKE2_IMPL_H
#define ARCHIVE_BLAKE2_IMPL_H

#include <stdint.h>
#include <string.h>

#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 199901L)
  #if   defined(_MSC_VER)
    #define BLAKE2_INLINE __inline
  #elif defined(__GNUC__)
    #define BLAKE2_INLINE __inline__
  #else
    #define BLAKE2_INLINE
  #endif
#else
  #define BLAKE2_INLINE inline
#endif

static BLAKE2_INLINE uint32 load32(const void * src)
{
#if defined(NATIVE_LITTLE_ENDIAN)
	uint32 w;
	memcpy(&w, src, sizeof w);
	return w;
#else
	const uint8 * p = (const uint8 *)src;
	return ((uint32)( p[0] ) <<  0) | ((uint32)( p[1] ) <<  8) | ((uint32)( p[2] ) << 16) | ((uint32)( p[3] ) << 24);
#endif
}

static BLAKE2_INLINE uint64 load64(const void * src)
{
#if defined(NATIVE_LITTLE_ENDIAN)
	uint64 w;
	memcpy(&w, src, sizeof w);
	return w;
#else
	const uint8 * p = (const uint8 *)src;
	return ((uint64)( p[0] ) <<  0) |
	       ((uint64)( p[1] ) <<  8) |
	       ((uint64)( p[2] ) << 16) |
	       ((uint64)( p[3] ) << 24) |
	       ((uint64)( p[4] ) << 32) |
	       ((uint64)( p[5] ) << 40) |
	       ((uint64)( p[6] ) << 48) |
	       ((uint64)( p[7] ) << 56);
#endif
}

static BLAKE2_INLINE uint16 load16(const void * src)
{
#if defined(NATIVE_LITTLE_ENDIAN)
	uint16 w;
	memcpy(&w, src, sizeof w);
	return w;
#else
	const uint8 * p = (const uint8 *)src;
	return (uint16)(((uint32)(p[0]) <<  0) | ((uint32)(p[1]) <<  8));
#endif
}

static BLAKE2_INLINE void store16(void * dst, uint16 w)
{
#if defined(NATIVE_LITTLE_ENDIAN)
	memcpy(dst, &w, sizeof w);
#else
	uint8 * p = (uint8 *)dst;
	*p++ = (uint8)w; w >>= 8;
	*p++ = (uint8)w;
#endif
}

static BLAKE2_INLINE void store32(void * dst, uint32 w)
{
#if defined(NATIVE_LITTLE_ENDIAN)
	memcpy(dst, &w, sizeof w);
#else
	uint8 * p = (uint8 *)dst;
	p[0] = (uint8)(w >>  0);
	p[1] = (uint8)(w >>  8);
	p[2] = (uint8)(w >> 16);
	p[3] = (uint8)(w >> 24);
#endif
}

static BLAKE2_INLINE void store64(void * dst, uint64 w)
{
#if defined(NATIVE_LITTLE_ENDIAN)
	memcpy(dst, &w, sizeof w);
#else
	uint8 * p = (uint8 *)dst;
	p[0] = (uint8)(w >>  0);
	p[1] = (uint8)(w >>  8);
	p[2] = (uint8)(w >> 16);
	p[3] = (uint8)(w >> 24);
	p[4] = (uint8)(w >> 32);
	p[5] = (uint8)(w >> 40);
	p[6] = (uint8)(w >> 48);
	p[7] = (uint8)(w >> 56);
#endif
}

static BLAKE2_INLINE uint64 load48(const void * src)
{
	const uint8 * p = (const uint8 *)src;
	return ((uint64)(p[0]) <<  0) | ((uint64)(p[1]) <<  8) | ((uint64)(p[2]) << 16) | ((uint64)(p[3]) << 24) | ((uint64)(p[4]) << 32) | ((uint64)(p[5]) << 40);
}

static BLAKE2_INLINE void store48(void * dst, uint64 w)
{
	uint8 * p = (uint8 *)dst;
	p[0] = (uint8)(w >>  0);
	p[1] = (uint8)(w >>  8);
	p[2] = (uint8)(w >> 16);
	p[3] = (uint8)(w >> 24);
	p[4] = (uint8)(w >> 32);
	p[5] = (uint8)(w >> 40);
}

// @v11.4.0 (replaced with SBits::Rotr) static BLAKE2_INLINE uint32 rotr32(const uint32 w, const unsigned c) { return ( w >> c ) | ( w << ( 32 - c ) ); }
// @v11.4.0 (replaced with SBits::Rotr) static BLAKE2_INLINE uint64 rotr64(const uint64 w, const unsigned c) { return ( w >> c ) | ( w << ( 64 - c ) ); }

/* prevents compiler optimizing out memset() */
static BLAKE2_INLINE void secure_zero_memory(void * v, size_t n)
{
	//static void *(*const volatile memset_v)(void *, int, size_t) = &memset;
	//memset_v(v, 0, n);
	memzero(v, n);
}

#endif
