/*-
 * Copyright (c) 2003-2009 Tim Kientzle
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 */
#include "archive_platform.h"
#pragma hdrstop
__FBSDID("$FreeBSD: head/lib/libarchive/archive_read_support_format_raw.c 201107 2009-12-28 03:25:33Z kientzle $");
//#include "archive_read_private.h"

struct raw_info {
	int64 offset; /* Current position in the file. */
	int64 unconsumed;
	int end_of_file;
};

static int archive_read_format_raw_bid(ArchiveRead *, int);
static int archive_read_format_raw_cleanup(ArchiveRead *);
static int archive_read_format_raw_read_data(ArchiveRead *, const void **, size_t *, int64 *);
static int archive_read_format_raw_read_data_skip(ArchiveRead *);
static int archive_read_format_raw_read_header(ArchiveRead *, ArchiveEntry *);

int archive_read_support_format_raw(Archive * _a)
{
	struct raw_info * info;
	ArchiveRead * a = reinterpret_cast<ArchiveRead *>(_a);
	int r;
	archive_check_magic(_a, ARCHIVE_READ_MAGIC, ARCHIVE_STATE_NEW, __FUNCTION__);
	info = (struct raw_info *)SAlloc::C(1, sizeof(*info));
	if(!info) {
		archive_set_error(&a->archive, ENOMEM, "Can't allocate raw_info data");
		return ARCHIVE_FATAL;
	}
	r = __archive_read_register_format(a, info, "raw", archive_read_format_raw_bid, NULL,
		archive_read_format_raw_read_header, archive_read_format_raw_read_data, archive_read_format_raw_read_data_skip,
		NULL, archive_read_format_raw_cleanup, NULL, NULL);
	if(r != ARCHIVE_OK)
		SAlloc::F(info);
	return r;
}
/*
 * Bid 1 if this is a non-empty file.  Anyone who can really support
 * this should outbid us, so it should generally be safe to use "raw"
 * in conjunction with other formats.  But, this could really confuse
 * folks if there are bid errors or minor file damage, so we don't
 * include "raw" as part of support_format_all().
 */
static int archive_read_format_raw_bid(ArchiveRead * a, int best_bid)
{
	if(best_bid < 1 && __archive_read_ahead(a, 1, NULL) != NULL)
		return 1;
	return -1;
}
/*
 * Mock up a fake header.
 */
static int archive_read_format_raw_read_header(ArchiveRead * a, ArchiveEntry * entry)
{
	struct raw_info * info = (struct raw_info *)(a->format->data);
	if(info->end_of_file)
		return (ARCHIVE_EOF);
	a->archive.archive_format = ARCHIVE_FORMAT_RAW;
	a->archive.archive_format_name = "raw";
	archive_entry_set_pathname(entry, "data");
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_perm(entry, 0644);
	/* I'm deliberately leaving most fields unset here. */

	/* Let the filter fill out any fields it might have. */
	return __archive_read_header(a, entry);
}

static int archive_read_format_raw_read_data(ArchiveRead * a,
    const void ** buff, size_t * size, int64 * offset)
{
	struct raw_info * info;
	ssize_t avail;

	info = (struct raw_info *)(a->format->data);

	/* Consume the bytes we read last time. */
	if(info->unconsumed) {
		__archive_read_consume(a, info->unconsumed);
		info->unconsumed = 0;
	}

	if(info->end_of_file)
		return (ARCHIVE_EOF);

	/* Get whatever bytes are immediately available. */
	*buff = __archive_read_ahead(a, 1, &avail);
	if(avail > 0) {
		/* Return the bytes we just read */
		*size = avail;
		*offset = info->offset;
		info->offset += *size;
		info->unconsumed = avail;
		return ARCHIVE_OK;
	}
	else if(0 == avail) {
		/* Record and return end-of-file. */
		info->end_of_file = 1;
		*size = 0;
		*offset = info->offset;
		return (ARCHIVE_EOF);
	}
	else {
		/* Record and return an error. */
		*size = 0;
		*offset = info->offset;
		return ((int)avail);
	}
}

static int archive_read_format_raw_read_data_skip(ArchiveRead * a)
{
	struct raw_info * info = (struct raw_info *)(a->format->data);

	/* Consume the bytes we read last time. */
	if(info->unconsumed) {
		__archive_read_consume(a, info->unconsumed);
		info->unconsumed = 0;
	}
	info->end_of_file = 1;
	return ARCHIVE_OK;
}

static int archive_read_format_raw_cleanup(ArchiveRead * a)
{
	struct raw_info * info;
	info = (struct raw_info *)(a->format->data);
	SAlloc::F(info);
	a->format->data = NULL;
	return ARCHIVE_OK;
}
