/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#include "archive_platform.h"
#pragma hdrstop
__FBSDID("$FreeBSD: head/lib/libarchive/archive_read_open_fd.c 201103 2009-12-28 03:13:49Z kientzle $");

#ifdef HAVE_UNISTD_H
	#include <unistd.h>
#endif

struct read_fd_data {
	int fd;
	size_t block_size;
	char use_lseek;
	void    * buffer;
};

static int file_close(Archive *, void *);
static ssize_t  file_read(Archive *, void *, const void ** buff);
static int64  file_seek(Archive *, void *, int64 request, int);
static int64  file_skip(Archive *, void *, int64 request);

int archive_read_open_fd(Archive * a, int fd, size_t block_size)
{
	struct stat st;
	struct read_fd_data * mine;
	void * b;
	archive_clear_error(a);
	if(fstat(fd, &st) != 0) {
		archive_set_error(a, errno, "Can't stat fd %d", fd);
		return ARCHIVE_FATAL;
	}
	mine = (struct read_fd_data *)SAlloc::C(1, sizeof(*mine));
	b = SAlloc::M(block_size);
	if(mine == NULL || b == NULL) {
		archive_set_error(a, ENOMEM, SlTxtOutOfMem);
		SAlloc::F(mine);
		SAlloc::F(b);
		return ARCHIVE_FATAL;
	}
	mine->block_size = block_size;
	mine->buffer = b;
	mine->fd = fd;
	/*
	 * Skip support is a performance optimization for anything
	 * that supports lseek().  On FreeBSD, only regular files and
	 * raw disk devices support lseek() and there's no portable
	 * way to determine if a device is a raw disk device, so we
	 * only enable this optimization for regular files.
	 */
	if(S_ISREG(st.st_mode)) {
		archive_read_extract_set_skip_file(a, st.st_dev, st.st_ino);
		mine->use_lseek = 1;
	}
#if defined(__CYGWIN__) || defined(_WIN32)
	setmode(mine->fd, O_BINARY);
#endif
	archive_read_set_read_callback(a, reinterpret_cast<archive_read_callback *>(file_read));
	archive_read_set_skip_callback(a, file_skip);
	archive_read_set_seek_callback(a, file_seek);
	archive_read_set_close_callback(a, file_close);
	archive_read_set_callback_data(a, mine);
	return (archive_read_open1(a));
}

static ssize_t file_read(Archive * a, void * client_data, const void ** buff)
{
	struct read_fd_data * mine = (struct read_fd_data *)client_data;
	ssize_t bytes_read;

	*buff = mine->buffer;
	for(;;) {
		bytes_read = read(mine->fd, mine->buffer, mine->block_size);
		if(bytes_read < 0) {
			if(errno == EINTR)
				continue;
			archive_set_error(a, errno, "Error reading fd %d",
			    mine->fd);
		}
		return (bytes_read);
	}
}

static int64 file_skip(Archive * a, void * client_data, int64 request)
{
	struct read_fd_data * mine = (struct read_fd_data *)client_data;
	int64 skip = request;
	int64 old_offset, new_offset;
	int skip_bits = sizeof(skip) * 8 - 1; /* off_t is a signed type. */

	if(!mine->use_lseek)
		return 0;

	/* Reduce a request that would overflow the 'skip' variable. */
	if(sizeof(request) > sizeof(skip)) {
		int64 max_skip =
		    (((int64)1 << (skip_bits - 1)) - 1) * 2 + 1;
		if(request > max_skip)
			skip = max_skip;
	}

	/* Reduce request to the next smallest multiple of block_size */
	request = (request / mine->block_size) * mine->block_size;
	if(request == 0)
		return 0;

	if(((old_offset = lseek(mine->fd, 0, SEEK_CUR)) >= 0) &&
	    ((new_offset = lseek(mine->fd, skip, SEEK_CUR)) >= 0))
		return (new_offset - old_offset);

	/* If seek failed once, it will probably fail again. */
	mine->use_lseek = 0;

	/* Let libarchive recover with read+discard. */
	if(errno == ESPIPE)
		return 0;

	/*
	 * There's been an error other than ESPIPE. This is most
	 * likely caused by a programmer error (too large request)
	 * or a corrupted archive file.
	 */
	archive_set_error(a, errno, "Error seeking");
	return -1;
}

/*
 * TODO: Store the offset and use it in the read callback.
 */
static int64 file_seek(Archive * a, void * client_data, int64 request, int whence)
{
	struct read_fd_data * mine = (struct read_fd_data *)client_data;
	// We use off_t here because lseek() is declared that way.
	// See above for notes about when off_t is less than 64 bits.
	int64 r = lseek(mine->fd, request, whence);
	if(r >= 0)
		return r;
	if(errno == ESPIPE) {
		archive_set_error(a, errno, "A file descriptor(%d) is not seekable(PIPE)", mine->fd);
		return ARCHIVE_FAILED;
	}
	else {
		/* If the input is corrupted or truncated, fail. */
		archive_set_error(a, errno, "Error seeking in a file descriptor(%d)", mine->fd);
		return ARCHIVE_FATAL;
	}
}

static int file_close(Archive * a, void * client_data)
{
	struct read_fd_data * mine = (struct read_fd_data *)client_data;
	CXX_UNUSED(a);
	SAlloc::F(mine->buffer);
	SAlloc::F(mine);
	return ARCHIVE_OK;
}
