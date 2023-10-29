/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * All rights reserved.
 */
#ifndef ARCHIVE_PRIVATE_H_INCLUDED
#define ARCHIVE_PRIVATE_H_INCLUDED

#ifndef __LIBARCHIVE_BUILD
	#error This header is only to be used internally to libarchive.
#endif
#if HAVE_ICONV_H
	#include <iconv.h>
#endif
//#include "archive.h"
//#include "archive_string.h"

#if defined(__GNUC__) && (__GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 5))
	#define __LA_DEAD       __attribute__((__noreturn__))
#else
	#define __LA_DEAD
#endif

#define ARCHIVE_WRITE_MAGIC     (0xb0c5c0deU)
#define ARCHIVE_READ_MAGIC      (0xdeb0c5U)
#define ARCHIVE_WRITE_DISK_MAGIC (0xc001b0c5U)
#define ARCHIVE_READ_DISK_MAGIC (0xbadb0c5U)
#define ARCHIVE_MATCH_MAGIC     (0xcad11c9U)

#define ARCHIVE_STATE_NEW       1U
#define ARCHIVE_STATE_HEADER    2U
#define ARCHIVE_STATE_DATA      4U
#define ARCHIVE_STATE_EOF       0x10U
#define ARCHIVE_STATE_CLOSED    0x20U
#define ARCHIVE_STATE_FATAL     0x8000U
#define ARCHIVE_STATE_ANY       (0xFFFFU & ~ARCHIVE_STATE_FATAL)

struct archive_vtable {
	int (* archive_close)(Archive *);
	int (* archive_free)(Archive *);
	int (* archive_write_header)(Archive *, ArchiveEntry *);
	int (* archive_write_finish_entry)(Archive *);
	ssize_t (* archive_write_data)(Archive *, const void *, size_t);
	ssize_t (* archive_write_data_block)(Archive *, const void *, size_t, int64);
	int (* archive_read_next_header)(Archive *, ArchiveEntry **);
	int (* archive_read_next_header2)(Archive *, ArchiveEntry *);
	int (* archive_read_data_block)(Archive *, const void **, size_t *, int64 *);
	int (* archive_filter_count)(Archive *);
	int64 (* archive_filter_bytes)(Archive *, int);
	int (* archive_filter_code)(Archive *, int);
	const char * (* archive_filter_name)(Archive *, int);
};

struct archive_string_conv;

struct Archive {
	/*
	 * The magic/state values are used to sanity-check the
	 * client's usage.  If an API function is called at a
	 * ridiculous time, or the client passes us an invalid
	 * pointer, these values allow me to catch that.
	 */
	uint magic;
	uint state;
	/*
	 * Some public API functions depend on the "real" type of the archive object.
	 */
	struct archive_vtable * vtable;
	int archive_format;
	const char * archive_format_name;
	int compression_code; /* Currently active compression. */
	const char * compression_name;
	int file_count; /* Number of file entries processed. */
	int archive_error_number;
	const char * error;
	archive_string error_string;
	char * current_code;
	uint   current_codepage; /* Current ACP(ANSI CodePage). */
	uint   current_oemcp; /* Current OEMCP(OEM CodePage). */
	archive_string_conv * sconv;
	/*
	 * Used by archive_read_data() to track blocks and copy
	 * data to client buffers, filling gaps with zero bytes.
	 */
	const char * read_data_block;
	int64 read_data_offset;
	int64 read_data_output_offset;
	size_t read_data_remaining;
	/*
	 * Used by formats/filters to determine the amount of data
	 * requested from a call to archive_read_data(). This is only
	 * useful when the format/filter has seek support.
	 */
	char read_data_is_posix_read;
	size_t read_data_requested;
};

/* Check magic value and state; return(ARCHIVE_FATAL) if it isn't valid. */
int STDCALL __archive_check_magic(Archive *, uint magic, uint state, const char * func);
#define archive_check_magic(a, expected_magic, allowed_states, function_name) \
	do { \
		int magic_test = __archive_check_magic((a), (expected_magic), (allowed_states), (function_name)); \
		if(magic_test == ARCHIVE_FATAL) \
			return ARCHIVE_FATAL; \
	} while(0)

void    FASTCALL __archive_errx(int retvalue, const char * msg) __LA_DEAD;
void    __archive_ensure_cloexec_flag(int fd);
int     __archive_mktemp(const char * tmpdir);
#if defined(_WIN32) && !defined(__CYGWIN__)
	int __archive_mkstemp(wchar_t * p_template);
#else
	int __archive_mkstemp(char * p_template);
#endif
int     __archive_clean(Archive *);
void __archive_reset_read_data(Archive *);

#define err_combine(a, b)        ((a) < (b) ? (a) : (b))

#if defined(__BORLANDC__) || (defined(_MSC_VER) &&  _MSC_VER <= 1300)
#define        ARCHIVE_LITERAL_LL(x)   x ## i64
#define        ARCHIVE_LITERAL_ULL(x)  x ## ui64
#else
#define        ARCHIVE_LITERAL_LL(x)   x ## ll
#define        ARCHIVE_LITERAL_ULL(x)  x ## ull
#endif

#endif
