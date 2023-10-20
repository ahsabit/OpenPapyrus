/// \file       common.c
/// \brief      Common functions needed in many places in liblzma
//  Author:     Lasse Collin
//  This file has been put into the public domain. You can do whatever you want with this file.
//
#include "common.h"
#pragma hdrstop
//
// Version //
//
uint32_t lzma_version_number(void) { return LZMA_VERSION; }
const char * lzma_version_string(void) { return LZMA_VERSION_STRING; }
//
// Memory allocation //
//
extern void * lzma_attribute((__malloc__)) lzma_attr_alloc_size(1) lzma_alloc(size_t size, const lzma_allocator *allocator)
{
	SETIFZ(size, 1); // Some malloc() variants return NULL if called with size == 0.
	void * ptr = (allocator && allocator->FnAlloc) ? allocator->FnAlloc(allocator->opaque, 1, size) : SAlloc::M(size);
	return ptr;
}

extern void * lzma_attribute((__malloc__)) lzma_attr_alloc_size(1) lzma_alloc_zero(size_t size, const lzma_allocator *allocator)
{
	SETIFZ(size, 1); // Some calloc() variants return NULL if called with size == 0.
	void * ptr;
	if(allocator && allocator->FnAlloc) {
		ptr = allocator->FnAlloc(allocator->opaque, 1, size);
		memzero(ptr, size);
	}
	else
		ptr = SAlloc::C(1, size);
	return ptr;
}

extern void lzma_free(void * ptr, const lzma_allocator * allocator)
{
	if(allocator && allocator->FnFree)
		allocator->FnFree(allocator->opaque, ptr);
	else
		SAlloc::F(ptr);
}
//
// Misc //
//
extern size_t lzma_bufcpy(const uint8 * in, size_t * in_pos, size_t in_size, uint8 * out, size_t * out_pos, size_t out_size)
{
	const size_t in_avail = in_size - *in_pos;
	const size_t out_avail = out_size - *out_pos;
	const size_t copy_size = MIN(in_avail, out_avail);
	// Call memcpy() only if there is something to copy. If there is
	// nothing to copy, in or out might be NULL and then the memcpy()
	// call would trigger undefined behavior.
	if(copy_size > 0)
		memcpy(out + *out_pos, in + *in_pos, copy_size);
	*in_pos += copy_size;
	*out_pos += copy_size;
	return copy_size;
}

extern lzma_ret lzma_next_filter_init(lzma_next_coder * next, const lzma_allocator * allocator, const lzma_filter_info * filters)
{
	lzma_next_coder_init(filters[0].init, next, allocator);
	next->id = filters[0].id;
	return filters[0].init == NULL ? LZMA_OK : filters[0].init(next, allocator, filters);
}

extern lzma_ret lzma_next_filter_update(lzma_next_coder * next, const lzma_allocator * allocator, const lzma_filter * reversed_filters)
{
	// Check that the application isn't trying to change the Filter ID.
	// End of filters is indicated with LZMA_VLI_UNKNOWN in both
	// reversed_filters[0].id and next->id.
	if(reversed_filters[0].id != next->id)
		return LZMA_PROG_ERROR;
	if(reversed_filters[0].id == LZMA_VLI_UNKNOWN)
		return LZMA_OK;
	assert(next->update != NULL);
	return next->update(next->coder, allocator, NULL, reversed_filters);
}

extern void lzma_next_end(lzma_next_coder * next, const lzma_allocator * allocator)
{
	if(next->init != (uintptr_t)(NULL)) {
		// To avoid tiny end functions that simply call
		// lzma_free(coder, allocator), we allow leaving next->end
		// NULL and call lzma_free() here.
		if(next->end != NULL)
			next->end(next->coder, allocator);
		else
			lzma_free(next->coder, allocator);

		// Reset the variables so the we don't accidentally think
		// that it is an already initialized coder.
		next->SetDefault();// = LZMA_NEXT_CODER_INIT;
	}
}
//
// External to internal API wrapper //
//
extern lzma_ret lzma_strm_init(lzma_stream * strm)
{
	if(strm == NULL)
		return LZMA_PROG_ERROR;
	if(strm->internal == NULL) {
		strm->internal = (lzma_internal *)lzma_alloc(sizeof(lzma_internal), strm->allocator);
		if(strm->internal == NULL)
			return LZMA_MEM_ERROR;
		strm->internal->next.SetDefault();// = LZMA_NEXT_CODER_INIT;
	}
	memzero(strm->internal->supported_actions, sizeof(strm->internal->supported_actions));
	strm->internal->sequence = lzma_internal_s::ISEQ_RUN;
	strm->internal->allow_buf_error = false;
	strm->total_in = 0;
	strm->total_out = 0;
	return LZMA_OK;
}

lzma_ret lzma_code(lzma_stream *strm, lzma_action action)
{
	// Sanity checks
	if((strm->next_in == NULL && strm->avail_in != 0)
	   || (strm->next_out == NULL && strm->avail_out != 0)
	   || strm->internal == NULL
	   || strm->internal->next.code == NULL
	   || (uint)(action) > LZMA_ACTION_MAX
	   || !strm->internal->supported_actions[action])
		return LZMA_PROG_ERROR;

	// Check if unsupported members have been set to non-zero or non-NULL,
	// which would indicate that some new feature is wanted.
	if(strm->reserved_ptr1 != NULL
	   || strm->reserved_ptr2 != NULL
	   || strm->reserved_ptr3 != NULL
	   || strm->reserved_ptr4 != NULL
	   || strm->reserved_int2 != 0
	   || strm->reserved_int3 != 0
	   || strm->reserved_int4 != 0
	   || strm->reserved_enum1 != LZMA_RESERVED_ENUM
	   || strm->reserved_enum2 != LZMA_RESERVED_ENUM)
		return LZMA_OPTIONS_ERROR;

	switch(strm->internal->sequence) {
		case lzma_internal_s::ISEQ_RUN:
		    switch(action) {
			    case LZMA_RUN: break;
			    case LZMA_SYNC_FLUSH: strm->internal->sequence = lzma_internal_s::ISEQ_SYNC_FLUSH; break;
			    case LZMA_FULL_FLUSH: strm->internal->sequence = lzma_internal_s::ISEQ_FULL_FLUSH; break;
			    case LZMA_FINISH: strm->internal->sequence = lzma_internal_s::ISEQ_FINISH; break;
			    case LZMA_FULL_BARRIER: strm->internal->sequence = lzma_internal_s::ISEQ_FULL_BARRIER; break;
		    }
		    break;
		case lzma_internal_s::ISEQ_SYNC_FLUSH:
		    // The same action must be used until we return
		    // LZMA_STREAM_END, and the amount of input must not change.
		    if(action != LZMA_SYNC_FLUSH || strm->internal->avail_in != strm->avail_in)
			    return LZMA_PROG_ERROR;
		    break;
		case lzma_internal_s::ISEQ_FULL_FLUSH:
		    if(action != LZMA_FULL_FLUSH || strm->internal->avail_in != strm->avail_in)
			    return LZMA_PROG_ERROR;
		    break;
		case lzma_internal_s::ISEQ_FINISH:
		    if(action != LZMA_FINISH || strm->internal->avail_in != strm->avail_in)
			    return LZMA_PROG_ERROR;
		    break;
		case lzma_internal_s::ISEQ_FULL_BARRIER:
		    if(action != LZMA_FULL_BARRIER || strm->internal->avail_in != strm->avail_in)
			    return LZMA_PROG_ERROR;
		    break;
		case lzma_internal_s::ISEQ_END:
		    return LZMA_STREAM_END;
		case lzma_internal_s::ISEQ_ERROR:
		default:
		    return LZMA_PROG_ERROR;
	}
	size_t in_pos = 0;
	size_t out_pos = 0;
	lzma_ret ret = strm->internal->next.code(strm->internal->next.coder, strm->allocator, strm->next_in, &in_pos, strm->avail_in,
		strm->next_out, &out_pos, strm->avail_out, action);
	strm->next_in += in_pos;
	strm->avail_in -= in_pos;
	strm->total_in += in_pos;
	strm->next_out += out_pos;
	strm->avail_out -= out_pos;
	strm->total_out += out_pos;
	strm->internal->avail_in = strm->avail_in;
	switch(ret) {
		case LZMA_OK:
		    // Don't return LZMA_BUF_ERROR when it happens the first time.
		    // This is to avoid returning LZMA_BUF_ERROR when avail_out
		    // was zero but still there was no more data left to written
		    // to next_out.
		    if(out_pos == 0 && in_pos == 0) {
			    if(strm->internal->allow_buf_error)
				    ret = LZMA_BUF_ERROR;
			    else
				    strm->internal->allow_buf_error = true;
		    }
		    else {
			    strm->internal->allow_buf_error = false;
		    }
		    break;

		case LZMA_TIMED_OUT:
		    strm->internal->allow_buf_error = false;
		    ret = LZMA_OK;
		    break;

		case LZMA_SEEK_NEEDED:
		    strm->internal->allow_buf_error = false;

		    // If LZMA_FINISH was used, reset it back to the
		    // LZMA_RUN-based state so that new input can be supplied
		    // by the application.
		    if(strm->internal->sequence == lzma_internal_s::ISEQ_FINISH)
			    strm->internal->sequence = lzma_internal_s::ISEQ_RUN;

		    break;

		case LZMA_STREAM_END:
		    if(strm->internal->sequence == lzma_internal_s::ISEQ_SYNC_FLUSH
			|| strm->internal->sequence == lzma_internal_s::ISEQ_FULL_FLUSH
			|| strm->internal->sequence
			== lzma_internal_s::ISEQ_FULL_BARRIER)
			    strm->internal->sequence = lzma_internal_s::ISEQ_RUN;
		    else
			    strm->internal->sequence = lzma_internal_s::ISEQ_END;

		// @fallthrough

		case LZMA_NO_CHECK:
		case LZMA_UNSUPPORTED_CHECK:
		case LZMA_GET_CHECK:
		case LZMA_MEMLIMIT_ERROR:
		    // Something else than LZMA_OK, but not a fatal error,
		    // that is, coding may be continued (except if ISEQ_END).
		    strm->internal->allow_buf_error = false;
		    break;

		default:
		    // All the other errors are fatal; coding cannot be continued.
		    assert(ret != LZMA_BUF_ERROR);
		    strm->internal->sequence = lzma_internal_s::ISEQ_ERROR;
		    break;
	}

	return ret;
}

void lzma_end(lzma_stream *strm)
{
	if(strm != NULL && strm->internal != NULL) {
		lzma_next_end(&strm->internal->next, strm->allocator);
		lzma_free(strm->internal, strm->allocator);
		strm->internal = NULL;
	}
}

void lzma_get_progress(lzma_stream *strm,  uint64 *progress_in,  uint64 *progress_out)
{
	if(strm->internal->next.get_progress != NULL) {
		strm->internal->next.get_progress(strm->internal->next.coder, progress_in, progress_out);
	}
	else {
		*progress_in = strm->total_in;
		*progress_out = strm->total_out;
	}
}

lzma_check lzma_get_check(const lzma_stream *strm)
{
	// Return LZMA_CHECK_NONE if we cannot know the check type.
	// It's a bug in the application if this happens.
	if(strm->internal->next.get_check == NULL)
		return LZMA_CHECK_NONE;
	return strm->internal->next.get_check(strm->internal->next.coder);
}

 uint64 lzma_memusage(const lzma_stream *strm)
{
	 uint64 memusage;
	 uint64 old_memlimit;
	if(!strm || !strm->internal || !strm->internal->next.memconfig || strm->internal->next.memconfig(strm->internal->next.coder, &memusage, &old_memlimit, 0) != LZMA_OK)
		return 0;
	return memusage;
}

 uint64 lzma_memlimit_get(const lzma_stream *strm)
{
	 uint64 old_memlimit;
	 uint64 memusage;
	if(!strm || !strm->internal || !strm->internal->next.memconfig || strm->internal->next.memconfig(strm->internal->next.coder, &memusage, &old_memlimit, 0) != LZMA_OK)
		return 0;
	return old_memlimit;
}

lzma_ret lzma_memlimit_set(lzma_stream *strm,  uint64 new_memlimit)
{
	// Dummy variables to simplify memconfig functions
	 uint64 old_memlimit;
	 uint64 memusage;
	if(strm == NULL || strm->internal == NULL || strm->internal->next.memconfig == NULL)
		return LZMA_PROG_ERROR;
	// Zero is a special value that cannot be used as an actual limit.
	// If 0 was specified, use 1 instead.
	if(new_memlimit == 0)
		new_memlimit = 1;
	return strm->internal->next.memconfig(strm->internal->next.coder, &memusage, &old_memlimit, new_memlimit);
}
