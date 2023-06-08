/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2004 Keith Packard
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA 02110-1335, USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * The Original Code is the cairo graphics library.
 * The Initial Developer of the Original Code is Keith Packard
 *
 * Contributor(s): Keith R. Packard <keithp@keithp.com>
 */
#ifndef CAIRO_WIDEINT_TYPE_H
#define CAIRO_WIDEINT_TYPE_H

#if   HAVE_STDINT_H
	#include <stdint.h>
#elif HAVE_INTTYPES_H
	#include <inttypes.h>
#elif HAVE_SYS_INT_TYPES_H
	#include <sys/int_types.h>
#elif defined(_MSC_VER)
// @sobolev typedef __int8 int8_t;
//typedef unsigned __int8 uint8;
//typedef __int16 int16;
//typedef unsigned __int16 uint16;
//typedef __int32 int32;
//typedef unsigned __int32 uint32;
//typedef __int64 int64;
//typedef unsigned __int64 uint64;
#ifndef HAVE_UINT64_T
	#define HAVE_UINT64_T 1
#endif
#else
	#error Cannot find definitions for fixed-width integral types (uint8, uint32, etc.)
#endif
#ifndef INT16_MIN
	#define INT16_MIN      (-32767-1)
#endif
#ifndef INT16_MAX
	#define INT16_MAX      (32767)
#endif
#ifndef UINT16_MAX
	#define UINT16_MAX     (65535)
#endif
#ifndef INT32_MIN
	#define INT32_MIN      (-2147483647-1)
#endif
#ifndef INT32_MAX
	#define INT32_MAX      (2147483647)
#endif
#ifndef UINT32_MAX
	#define UINT32_MAX     (4294967295U)
#endif
#if HAVE_BYTESWAP_H
	#include <byteswap.h>
#endif
//#ifndef bswap_16 // @sobolev (replaced with sbswap16)
	//#define bswap_16(p) (((((uint16)(p)) & 0x00ff) << 8) | (((uint16)(p)) >> 8)); 
//#endif
//#ifndef bswap_32 // @sobolev (replaced with sbswap32)
	//#define bswap_32(p) (((((uint32)(p)) & 0x000000ff) << 24) | ((((uint32)(p)) & 0x0000ff00) << 8) | ((((uint32)(p)) & 0x00ff0000) >> 8) | ((((uint32)(p))) >> 24));
//#endif
//typedef uint64 cairo_uint64_t__Removed;
//typedef int64 cairo_int64_t__Removed;

typedef struct _cairo_uquorem64 {
	uint64 quo;
	uint64 rem;
} cairo_uquorem64_t;

typedef struct _cairo_quorem64 {
	int64 quo;
	int64 rem;
} cairo_quorem64_t;

/* gcc has a non-standard name. */
#if HAVE___UINT128_T && !HAVE_UINT128_T
	typedef __uint128_t uint128_t;
	typedef __int128_t int128_t;
	#define HAVE_UINT128_T 1
#endif
#if !HAVE_UINT128_T
	typedef struct cairo_uint128 {
		uint64 lo, hi;
	} cairo_uint128_t, cairo_int128_t;
#else
	typedef uint128_t cairo_uint128_t;
	typedef int128_t cairo_int128_t;
#endif

typedef struct _cairo_uquorem128 {
	cairo_uint128_t quo;
	cairo_uint128_t rem;
} cairo_uquorem128_t;

typedef struct _cairo_quorem128 {
	cairo_int128_t quo;
	cairo_int128_t rem;
} cairo_quorem128_t;

#endif /* CAIRO_WIDEINT_H */
