/*
 * Copyright © 2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * Google Author(s): Behdad Esfahbod
 */
#include "harfbuzz-internal.h"
#pragma hdrstop

static const hb_shaper_entry_t all_shapers[] = {
#define HB_SHAPER_IMPLEMENT(name) {#name, _hb_ ## name ## _shape},
#include "hb-shaper-list.hh"
#undef HB_SHAPER_IMPLEMENT
};
#ifndef HB_NO_SHAPER
	static_assert(0 != SIZEOFARRAY(all_shapers), "No shaper enabled.");
#endif
#if HB_USE_ATEXIT
	static void free_static_shapers();
#endif

static struct hb_shapers_lazy_loader_t : hb_lazy_loader_t<const hb_shaper_entry_t, hb_shapers_lazy_loader_t> 
{
	static hb_shaper_entry_t * create()
	{
		char * env = getenv("HB_SHAPER_LIST");
		if(!env || !*env)
			return nullptr;
		hb_shaper_entry_t * shapers = (hb_shaper_entry_t*)SAlloc::C(1, sizeof(all_shapers));
		if(UNLIKELY(!shapers))
			return nullptr;
		memcpy(shapers, all_shapers, sizeof(all_shapers));
		/* Reorder shaper list to prefer requested shapers. */
		uint i = 0;
		char * end;
		char * p = env;
		for(;;) {
			end = sstrchr(p, ',');
			if(!end)
				end = p + strlen(p);
			for(uint j = i; j < ARRAY_LENGTH(all_shapers); j++) {
				if(end - p == (int)strlen(shapers[j].name) && 0 == strncmp(shapers[j].name, p, end - p)) {
					/* Reorder this shaper to position i */
					struct hb_shaper_entry_t t = shapers[j];
					memmove(&shapers[i+1], &shapers[i], sizeof(shapers[i]) * (j - i));
					shapers[i] = t;
					i++;
				}
			}
			if(!*end)
				break;
			else
				p = end + 1;
		}
#if HB_USE_ATEXIT
		atexit(free_static_shapers);
#endif
		return shapers;
	}
	static void destroy(const hb_shaper_entry_t * p) 
	{
		SAlloc::F((void *)p);
	}
	static const hb_shaper_entry_t * get_null() { return all_shapers; }
} static_shapers;

#if HB_USE_ATEXIT
static void free_static_shapers()
{
	static_shapers.free_instance();
}
#endif

const hb_shaper_entry_t * _hb_shapers_get()
{
	return static_shapers.get_unconst();
}
