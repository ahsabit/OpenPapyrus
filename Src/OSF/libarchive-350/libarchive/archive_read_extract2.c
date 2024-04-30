/*-
 * Copyright (c) 2003-2007 Tim Kientzle All rights reserved.
 */
#include "archive_platform.h"
#pragma hdrstop
__FBSDID("$FreeBSD: src/lib/libarchive/archive_read_extract.c,v 1.61 2008/05/26 17:00:22 kientzle Exp $");

static int copy_data(Archive * ar, Archive * aw);
static int archive_read_extract_cleanup(ArchiveRead *);

/* Retrieve an extract object without initialising the associated
 * archive_write_disk object.
 */
struct archive_read_extract * __archive_read_get_extract(ArchiveRead * a)                               
{
	if(a->extract == NULL) {
		a->extract = (struct archive_read_extract *)SAlloc::C(1, sizeof(*a->extract));
		if(a->extract == NULL) {
			archive_set_error(&a->archive, ENOMEM, "Can't extract");
			return NULL;
		}
		a->cleanup_archive_extract = archive_read_extract_cleanup;
	}
	return (a->extract);
}
/*
 * Cleanup function for archive_extract.
 */
static int archive_read_extract_cleanup(ArchiveRead * a)
{
	int ret = ARCHIVE_OK;
	if(a) {
		if(a->extract->ad)
			ret = archive_write_free(a->extract->ad);
		SAlloc::F(a->extract);
		a->extract = NULL;
	}
	return ret;
}

int archive_read_extract2(Archive * _a, ArchiveEntry * entry, Archive * ad)
{
	ArchiveRead * a = reinterpret_cast<ArchiveRead *>(_a);
	int r, r2;
	/* Set up for this particular entry. */
	if(a->skip_file_set)
		archive_write_disk_set_skip_file(ad, a->skip_file_dev, a->skip_file_ino);
	r = archive_write_header(ad, entry);
	if(r < ARCHIVE_WARN)
		r = ARCHIVE_WARN;
	if(r != ARCHIVE_OK)
		/* If _write_header failed, copy the error. */
		archive_copy_error(&a->archive, ad);
	else if(!archive_entry_size_is_set(entry) || archive_entry_size(entry) > 0)
		/* Otherwise, pour data into the entry. */
		r = copy_data(_a, ad);
	r2 = archive_write_finish_entry(ad);
	if(r2 < ARCHIVE_WARN)
		r2 = ARCHIVE_WARN;
	/* Use the first message. */
	if(r2 != ARCHIVE_OK && r == ARCHIVE_OK)
		archive_copy_error(&a->archive, ad);
	/* Use the worst error return. */
	if(r2 < r)
		r = r2;
	return r;
}

void archive_read_extract_set_progress_callback(Archive * _a, void (*progress_func)(void *), void * user_data)
{
	ArchiveRead * a = reinterpret_cast<ArchiveRead *>(_a);
	struct archive_read_extract * extract = __archive_read_get_extract(a);
	if(extract) {
		extract->extract_progress = progress_func;
		extract->extract_progress_user_data = user_data;
	}
}

static int copy_data(Archive * ar, Archive * aw)
{
	int64 offset;
	const void * buff;
	size_t size;
	int r;
	struct archive_read_extract * extract = __archive_read_get_extract((ArchiveRead *)ar);
	if(extract == NULL)
		return ARCHIVE_FATAL;
	for(;;) {
		r = archive_read_data_block(ar, &buff, &size, &offset);
		if(r == ARCHIVE_EOF)
			return ARCHIVE_OK;
		if(r != ARCHIVE_OK)
			return r;
		r = (int)archive_write_data_block(aw, buff, size, offset);
		if(r < ARCHIVE_WARN)
			r = ARCHIVE_WARN;
		if(r < ARCHIVE_OK) {
			archive_set_error(ar, archive_errno(aw), "%s", archive_error_string(aw));
			return r;
		}
		if(extract->extract_progress)
			extract->extract_progress(extract->extract_progress_user_data);
	}
}
