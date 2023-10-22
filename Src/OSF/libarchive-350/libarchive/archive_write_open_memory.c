/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "archive_platform.h"
#pragma hdrstop
__FBSDID("$FreeBSD: src/lib/libarchive/archive_write_open_memory.c,v 1.3 2007/01/09 08:05:56 kientzle Exp $");

struct write_memory_data {
	size_t used;
	size_t size;
	size_t * client_size;
	uchar * buff;
};

static int memory_write_free(Archive *, void *);
static int memory_write_open(Archive *, void *);
static ssize_t  memory_write(Archive *, void *, const void * buff, size_t);
/*
 * Client provides a pointer to a block of memory to receive
 * the data.  The 'size' param both tells us the size of the
 * client buffer and lets us tell the client the final size.
 */
int archive_write_open_memory(Archive * a, void * buff, size_t buffSize, size_t * used)
{
	struct write_memory_data * mine = (struct write_memory_data *)SAlloc::C(1, sizeof(*mine));
	if(mine == NULL) {
		archive_set_error(a, ENOMEM, SlTxtOutOfMem);
		return ARCHIVE_FATAL;
	}
	mine->buff = static_cast<uchar *>(buff);
	mine->size = buffSize;
	mine->client_size = used;
	return (archive_write_open2(a, mine, memory_write_open, reinterpret_cast<archive_write_callback *>(memory_write), NULL, memory_write_free));
}

static int memory_write_open(Archive * a, void * client_data)
{
	struct write_memory_data * mine = static_cast<struct write_memory_data *>(client_data);
	mine->used = 0;
	if(mine->client_size != NULL)
		*mine->client_size = mine->used;
	/* Disable padding if it hasn't been set explicitly. */
	if(-1 == archive_write_get_bytes_in_last_block(a))
		archive_write_set_bytes_in_last_block(a, 1);
	return ARCHIVE_OK;
}

/*
 * Copy the data into the client buffer.
 * Note that we update mine->client_size on every write.
 * In particular, this means the client can follow exactly
 * how much has been written into their buffer at any time.
 */
static ssize_t memory_write(Archive * a, void * client_data, const void * buff, size_t length)
{
	struct write_memory_data * mine = static_cast<struct write_memory_data *>(client_data);
	if(mine->used + length > mine->size) {
		archive_set_error(a, ENOMEM, "Buffer exhausted");
		return ARCHIVE_FATAL;
	}
	memcpy(mine->buff + mine->used, buff, length);
	mine->used += length;
	if(mine->client_size != NULL)
		*mine->client_size = mine->used;
	return (length);
}

static int memory_write_free(Archive * a, void * client_data)
{
	CXX_UNUSED(a);
	struct write_memory_data * mine = static_cast<struct write_memory_data *>(client_data);
	if(mine == NULL)
		return ARCHIVE_OK;
	SAlloc::F(mine);
	return ARCHIVE_OK;
}
