// 
// Copyright (c) 2003-2010 Tim Kientzle All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
#include "archive_platform.h"
#pragma hdrstop
__FBSDID("$FreeBSD: head/lib/libarchive/archive_check_magic.c 201089 2009-12-28 02:20:23Z kientzle $");
//#ifdef HAVE_UNISTD_H
	//#include <unistd.h>
//#endif
//#include "archive_private.h"

static void errmsg(const char * m)
{
	size_t s = sstrlen(m);
	while(s > 0) {
		const ssize_t written = write(2, m, s);
		if(written <= 0)
			return;
		m += written;
		s -= written;
	}
}

static __LA_DEAD void diediedie(void)
{
#if defined(_WIN32) && !defined(__CYGWIN__) && defined(_DEBUG)
	/* Cause a breakpoint exception  */
	DebugBreak();
#endif
	abort(); /* Terminate the program abnormally. */
}

static const char * FASTCALL state_name(uint s)
{
	switch(s) {
		case ARCHIVE_STATE_NEW:         return ("new");
		case ARCHIVE_STATE_HEADER:      return ("header");
		case ARCHIVE_STATE_DATA:        return ("data");
		case ARCHIVE_STATE_EOF:         return ("eof");
		case ARCHIVE_STATE_CLOSED:      return ("closed");
		case ARCHIVE_STATE_FATAL:       return ("fatal");
		default:                        return ("??");
	}
}

static const char * FASTCALL archive_handle_type_name(uint m)
{
	switch(m) {
		case ARCHIVE_WRITE_MAGIC:       return ("archive_write");
		case ARCHIVE_READ_MAGIC:        return ("ArchiveRead");
		case ARCHIVE_WRITE_DISK_MAGIC:  return ("archive_write_disk");
		case ARCHIVE_READ_DISK_MAGIC:   return ("archive_read_disk");
		case ARCHIVE_MATCH_MAGIC:       return ("archive_match");
		default:                        return NULL;
	}
}

static char * write_all_states(char * buff, uint states)
{
	uint lowbit;
	buff[0] = '\0';
	// A trick for computing the lowest set bit
	while((lowbit = states & (1 + ~states)) != 0) {
		states &= ~lowbit; // Clear the low bit
		strcat(buff, state_name(lowbit));
		if(states != 0)
			strcat(buff, "/");
	}
	return buff;
}
/*
 * Check magic value and current state.
 *   Magic value mismatches are fatal and result in calls to abort().
 *   State mismatches return ARCHIVE_FATAL.
 *   Otherwise, returns ARCHIVE_OK.
 *
 * This is designed to catch serious programming errors that violate
 * the libarchive API.
 */
int STDCALL __archive_check_magic(Archive * a, uint magic, uint state, const char * function)
{
	char states1[64];
	char states2[64];
	/*
	 * If this isn't some form of archive handle,
	 * then the library user has screwed up so bad that
	 * we don't even have a reliable way to report an error.
	 */
	const char * handle_type = archive_handle_type_name(a->magic);
	if(!handle_type) {
		errmsg("PROGRAMMER ERROR: Function ");
		errmsg(function);
		errmsg(" invoked with invalid archive handle.\n");
		diediedie();
	}
	if(a->magic != magic) {
		archive_set_error(a, -1, "PROGRAMMER ERROR: Function '%s' invoked on '%s' archive object, which is not supported.", function, handle_type);
		a->state = ARCHIVE_STATE_FATAL;
		return ARCHIVE_FATAL;
	}
	if((a->state & state) == 0) {
		/* If we're already FATAL, don't overwrite the error. */
		if(a->state != ARCHIVE_STATE_FATAL)
			archive_set_error(a, -1, "INTERNAL ERROR: Function '%s' invoked with archive structure in state '%s', should be in state '%s'",
			    function, write_all_states(states1, a->state), write_all_states(states2, state));
		a->state = ARCHIVE_STATE_FATAL;
		return ARCHIVE_FATAL;
	}
	return ARCHIVE_OK;
}
