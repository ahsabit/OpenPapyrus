/*-
 * Copyright (c) 2003-2007 Tim Kientzle
 * Copyright (c) 2012 Michihiro NAKAJIMA
 * All rights reserved.
 */
#include "archive_platform.h"
#pragma hdrstop

__FBSDID("$FreeBSD: head/lib/libarchive/archive_write_set_compression_bzip2.c 201091 2009-12-28 02:22:41Z kientzle $");

#if ARCHIVE_VERSION_NUMBER < 4000000
int archive_write_set_compression_bzip2(Archive * a)
{
	__archive_write_filters_free(a);
	return (archive_write_add_filter_bzip2(a));
}

#endif

struct private_data {
	int compression_level;
#if defined(HAVE_BZLIB_H) && defined(BZ_CONFIG_ERROR)
	bz_stream stream;
	int64 total_in;
	char * compressed;
	size_t compressed_buffer_size;
#else
	struct archive_write_program_data * pdata;
#endif
};

static int archive_compressor_bzip2_close(struct archive_write_filter *);
static int archive_compressor_bzip2_free(struct archive_write_filter *);
static int archive_compressor_bzip2_open(struct archive_write_filter *);
static int archive_compressor_bzip2_options(struct archive_write_filter *, const char *, const char *);
static int archive_compressor_bzip2_write(struct archive_write_filter *, const void *, size_t);
/*
 * Add a bzip2 compression filter to this write handle.
 */
int archive_write_add_filter_bzip2(Archive * _a)
{
	struct archive_write * a = (struct archive_write *)_a;
	struct archive_write_filter * f = __archive_write_allocate_filter(_a);
	struct private_data * data;
	archive_check_magic(&a->archive, ARCHIVE_WRITE_MAGIC, ARCHIVE_STATE_NEW, __FUNCTION__);
	data = static_cast<struct private_data *>(SAlloc::C(1, sizeof(*data)));
	if(!data) {
		archive_set_error(&a->archive, ENOMEM, SlTxtOutOfMem);
		return ARCHIVE_FATAL;
	}
	data->compression_level = 9; /* default */
	f->data = data;
	f->FnOptions = &archive_compressor_bzip2_options;
	f->FnClose = &archive_compressor_bzip2_close;
	f->FnFree = &archive_compressor_bzip2_free;
	f->FnOpen = &archive_compressor_bzip2_open;
	f->code = ARCHIVE_FILTER_BZIP2;
	f->name = "bzip2";
#if defined(HAVE_BZLIB_H) && defined(BZ_CONFIG_ERROR)
	return ARCHIVE_OK;
#else
	data->pdata = __archive_write_program_allocate("bzip2");
	if(data->pdata == NULL) {
		SAlloc::F(data);
		archive_set_error(&a->archive, ENOMEM, SlTxtOutOfMem);
		return ARCHIVE_FATAL;
	}
	data->compression_level = 0;
	archive_set_error(&a->archive, ARCHIVE_ERRNO_MISC, "Using external bzip2 program");
	return ARCHIVE_WARN;
#endif
}
/*
 * Set write options.
 */
static int archive_compressor_bzip2_options(struct archive_write_filter * f, const char * key, const char * value)
{
	struct private_data * data = (struct private_data *)f->data;
	if(sstreq(key, "compression-level")) {
		if(value == NULL || !(value[0] >= '0' && value[0] <= '9') || value[1] != '\0')
			return ARCHIVE_WARN;
		data->compression_level = value[0] - '0';
		// Make '0' be a synonym for '1'. 
		// This way, bzip2 compressor supports the same 0..9 range of levels as gzip. 
		if(data->compression_level < 1)
			data->compression_level = 1;
		return ARCHIVE_OK;
	}
	// Note: The "warn" return is just to inform the options supervisor that we didn't handle it.  It will generate a suitable error if no one used this option
	return ARCHIVE_WARN;
}

#if defined(HAVE_BZLIB_H) && defined(BZ_CONFIG_ERROR)
/* Don't compile this if we don't have bzlib. */
/*
 * Yuck.  bzlib.h is not const-correct, so I need this one bit
 * of ugly hackery to convert a const * pointer to a non-const pointer.
 */
#define SET_NEXT_IN(st, src) (st)->stream.next_in = (char *)(uintptr_t)(const void*)(src)
static int drive_compressor(struct archive_write_filter *, struct private_data *, int finishing);
/*
 * Setup callback.
 */
static int archive_compressor_bzip2_open(struct archive_write_filter * f)
{
	struct private_data * data = (struct private_data *)f->data;
	int ret;
	if(data->compressed == NULL) {
		size_t bs = 65536, bpb;
		if(f->archive->magic == ARCHIVE_WRITE_MAGIC) {
			// Buffer size should be a multiple number of the of bytes per block for performance.
			bpb = archive_write_get_bytes_per_block(f->archive);
			if(bpb > bs)
				bs = bpb;
			else if(bpb != 0)
				bs -= bs % bpb;
		}
		data->compressed_buffer_size = bs;
		data->compressed = (char *)SAlloc::M(data->compressed_buffer_size);
		if(data->compressed == NULL) {
			archive_set_error(f->archive, ENOMEM, "Can't allocate data for compression buffer");
			return ARCHIVE_FATAL;
		}
	}
	memzero(&data->stream, sizeof(data->stream));
	data->stream.next_out = data->compressed;
	data->stream.avail_out = data->compressed_buffer_size;
	f->FnWrite = archive_compressor_bzip2_write;
	// Initialize compression library 
	ret = BZ2_bzCompressInit(&(data->stream), data->compression_level, 0, 30);
	if(ret == BZ_OK) {
		f->data = data;
		return ARCHIVE_OK;
	}
	// Library setup failed: clean up.
	archive_set_error(f->archive, ARCHIVE_ERRNO_MISC, "Internal error initializing compression library");
	// Override the error message if we know what really went wrong.
	switch(ret) {
		case BZ_PARAM_ERROR:
		    archive_set_error(f->archive, ARCHIVE_ERRNO_MISC, "Internal error initializing compression library: invalid setup parameter");
		    break;
		case BZ_MEM_ERROR:
		    archive_set_error(f->archive, ENOMEM, "Internal error initializing compression library: out of memory");
		    break;
		case BZ_CONFIG_ERROR:
		    archive_set_error(f->archive, ARCHIVE_ERRNO_MISC, "Internal error initializing compression library: mis-compiled library");
		    break;
	}
	return ARCHIVE_FATAL;
}
/*
 * Write data to the compressed stream.
 *
 * Returns ARCHIVE_OK if all data written, error otherwise.
 */
static int archive_compressor_bzip2_write(struct archive_write_filter * f, const void * buff, size_t length)
{
	struct private_data * data = (struct private_data *)f->data;
	/* Update statistics */
	data->total_in += length;
	/* Compress input data to output buffer */
	SET_NEXT_IN(data, buff);
	data->stream.avail_in = length;
	if(drive_compressor(f, data, 0))
		return ARCHIVE_FATAL;
	return ARCHIVE_OK;
}
/*
 * Finish the compression.
 */
static int archive_compressor_bzip2_close(struct archive_write_filter * f)
{
	struct private_data * data = (struct private_data *)f->data;
	/* Finish compression cycle. */
	int ret = drive_compressor(f, data, 1);
	if(ret == ARCHIVE_OK) {
		/* Write the last block */
		ret = __archive_write_filter(f->next_filter, data->compressed, data->compressed_buffer_size - data->stream.avail_out);
	}
	switch(BZ2_bzCompressEnd(&(data->stream))) {
		case BZ_OK:
		    break;
		default:
		    archive_set_error(f->archive, ARCHIVE_ERRNO_PROGRAMMER, "Failed to clean up compressor");
		    ret = ARCHIVE_FATAL;
	}
	return ret;
}

static int archive_compressor_bzip2_free(struct archive_write_filter * f)
{
	struct private_data * data = (struct private_data *)f->data;
	SAlloc::F(data->compressed);
	SAlloc::F(data);
	f->data = NULL;
	return ARCHIVE_OK;
}
/*
 * Utility function to push input data through compressor, writing
 * full output blocks as necessary.
 *
 * Note that this handles both the regular write case (finishing ==false) and the end-of-archive case (finishing == true).
 */
static int drive_compressor(struct archive_write_filter * f, struct private_data * data, int finishing)
{
	int ret;
	for(;;) {
		if(data->stream.avail_out == 0) {
			ret = __archive_write_filter(f->next_filter, data->compressed, data->compressed_buffer_size);
			if(ret != ARCHIVE_OK)
				return ARCHIVE_FATAL; // TODO: Handle this write failure
			data->stream.next_out = data->compressed;
			data->stream.avail_out = data->compressed_buffer_size;
		}

		/* If there's nothing to do, we're done. */
		if(!finishing && data->stream.avail_in == 0)
			return ARCHIVE_OK;
		ret = BZ2_bzCompress(&(data->stream), finishing ? BZ_FINISH : BZ_RUN);
		switch(ret) {
			case BZ_RUN_OK:
			    // In non-finishing case, did compressor consume everything? 
			    if(!finishing && data->stream.avail_in == 0)
				    return ARCHIVE_OK;
			    break;
			case BZ_FINISH_OK: /* Finishing: There's more work to do */
			    break;
			case BZ_STREAM_END: /* Finishing: all done */
			    /* Only occurs in finishing case */
			    return ARCHIVE_OK;
			default:
			    /* Any other return value indicates an error */
			    archive_set_error(f->archive, ARCHIVE_ERRNO_PROGRAMMER, "Bzip2 compression failed; BZ2_bzCompress() returned %d", ret);
			    return ARCHIVE_FATAL;
		}
	}
}

#else /* HAVE_BZLIB_H && BZ_CONFIG_ERROR */

static int archive_compressor_bzip2_open(struct archive_write_filter * f)
{
	struct private_data * data = (struct private_data *)f->data;
	archive_string as;
	int r;
	archive_string_init(&as);
	archive_strcpy(&as, "bzip2");
	/* Specify compression level. */
	if(data->compression_level > 0) {
		archive_strcat(&as, " -");
		archive_strappend_char(&as, '0' + data->compression_level);
	}
	f->write = archive_compressor_bzip2_write;
	r = __archive_write_program_open(f, data->pdata, as.s);
	archive_string_free(&as);
	return r;
}

static int archive_compressor_bzip2_write(struct archive_write_filter * f, const void * buff, size_t length)
{
	struct private_data * data = (struct private_data *)f->data;
	return __archive_write_program_write(f, data->pdata, buff, length);
}

static int archive_compressor_bzip2_close(struct archive_write_filter * f)
{
	struct private_data * data = (struct private_data *)f->data;
	return __archive_write_program_close(f, data->pdata);
}

static int archive_compressor_bzip2_free(struct archive_write_filter * f)
{
	struct private_data * data = (struct private_data *)f->data;
	__archive_write_program_free(data->pdata);
	SAlloc::F(data);
	return ARCHIVE_OK;
}

#endif /* HAVE_BZLIB_H && BZ_CONFIG_ERROR */
