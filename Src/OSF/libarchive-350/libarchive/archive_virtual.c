/*-
 * Copyright (c) 2003-2007 Tim Kientzle All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 */
#include "archive_platform.h"
#pragma hdrstop
__FBSDID("$FreeBSD: head/lib/libarchive/archive_virtual.c 201098 2009-12-28 02:58:14Z kientzle $");

int archive_filter_code(Archive * a, int n) { return ((a->vtable->archive_filter_code)(a, n)); }
int archive_filter_count(Archive * a) { return ((a->vtable->archive_filter_count)(a)); }
const char * archive_filter_name(Archive * a, int n) { return ((a->vtable->archive_filter_name)(a, n)); }
int64 archive_filter_bytes(Archive * a, int n) { return ((a->vtable->archive_filter_bytes)(a, n)); }
int archive_free(Archive * a) { return a ? ((a->vtable->archive_free)(a)) : ARCHIVE_OK; }
int archive_write_close(Archive * a) { return ((a->vtable->archive_close)(a)); }
int archive_read_close(Archive * a) { return ((a->vtable->archive_close)(a)); }

int archive_write_fail(Archive * a)
{
	a->state = ARCHIVE_STATE_FATAL;
	return a->state;
}

int archive_write_free(Archive * a) { return archive_free(a); }
int archive_read_free(Archive * a) { return archive_free(a); }

#if ARCHIVE_VERSION_NUMBER < 4000000
	int archive_write_finish(Archive * a) { return archive_write_free(a); } // For backwards compatibility; will be removed with libarchive 4.0
	int archive_read_finish(Archive * a)  { return archive_read_free(a); } // For backwards compatibility; will be removed with libarchive 4.0
#endif

int archive_write_header(Archive * a, ArchiveEntry * entry)
{
	++a->file_count;
	return ((a->vtable->archive_write_header)(a, entry));
}

int archive_write_finish_entry(Archive * a) { return ((a->vtable->archive_write_finish_entry)(a)); }
la_ssize_t archive_write_data(Archive * a, const void * buff, size_t s) { return ((a->vtable->archive_write_data)(a, buff, s)); }

la_ssize_t archive_write_data_block(Archive * a, const void * buff, size_t s, int64 o)
{
	if(a->vtable->archive_write_data_block == NULL) {
		archive_set_error(a, ARCHIVE_ERRNO_MISC, "archive_write_data_block not supported");
		a->state = ARCHIVE_STATE_FATAL;
		return ARCHIVE_FATAL;
	}
	return ((a->vtable->archive_write_data_block)(a, buff, s, o));
}

int archive_read_next_header(Archive * a, ArchiveEntry ** entry) { return ((a->vtable->archive_read_next_header)(a, entry)); }
int archive_read_next_header2(Archive * a, ArchiveEntry * entry) { return ((a->vtable->archive_read_next_header2)(a, entry)); }
int archive_read_data_block(Archive * a, const void ** buff, size_t * s, int64 * o) { return ((a->vtable->archive_read_data_block)(a, buff, s, o)); }
