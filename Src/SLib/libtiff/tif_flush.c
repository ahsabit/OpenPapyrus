/* $Id: tif_flush.c,v 1.9 2010-03-31 06:40:10 fwarmerdam Exp $ */
/*
 * Copyright (c) 1988-1997 Sam Leffler
 * Copyright (c) 1991-1997 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 */
/*
 * TIFF Library.
 */
#include <slib-internal.h>
#pragma hdrstop
#include "tiffiop.h"

int TIFFFlush(TIFF * tif)
{
	if(tif->tif_mode == O_RDONLY)
		return 1;
	if(!TIFFFlushData(tif))
		return 0;
	/* In update (r+) mode we try to detect the case where
	   only the strip/tile map has been altered, and we try to
	   rewrite only that portion of the directory without
	   making any other changes */
	if((tif->tif_flags & TIFF_DIRTYSTRIP) && !(tif->tif_flags & TIFF_DIRTYDIRECT) && tif->tif_mode == O_RDWR) {
		uint64  * offsets = NULL, * sizes = NULL;
		if(TIFFIsTiled(tif)) {
			if(TIFFGetField(tif, TIFFTAG_TILEOFFSETS, &offsets) && TIFFGetField(tif, TIFFTAG_TILEBYTECOUNTS, &sizes) && 
				_TIFFRewriteField(tif, TIFFTAG_TILEOFFSETS, TIFF_LONG8, tif->tif_dir.td_nstrips, offsets) && 
				_TIFFRewriteField(tif, TIFFTAG_TILEBYTECOUNTS, TIFF_LONG8, tif->tif_dir.td_nstrips, sizes)) {
				tif->tif_flags &= ~TIFF_DIRTYSTRIP;
				tif->tif_flags &= ~TIFF_BEENWRITING;
				return 1;
			}
		}
		else {
			if(TIFFGetField(tif, TIFFTAG_STRIPOFFSETS, &offsets) && 
				TIFFGetField(tif, TIFFTAG_STRIPBYTECOUNTS, &sizes) && 
				_TIFFRewriteField(tif, TIFFTAG_STRIPOFFSETS, TIFF_LONG8, tif->tif_dir.td_nstrips, offsets) && 
				_TIFFRewriteField(tif, TIFFTAG_STRIPBYTECOUNTS, TIFF_LONG8, tif->tif_dir.td_nstrips, sizes)) {
				tif->tif_flags &= ~TIFF_DIRTYSTRIP;
				tif->tif_flags &= ~TIFF_BEENWRITING;
				return 1;
			}
		}
	}
	if((tif->tif_flags & (TIFF_DIRTYDIRECT|TIFF_DIRTYSTRIP)) && !TIFFRewriteDirectory(tif))
		return 0;
	return 1;
}
/*
 * Flush buffered data to the file.
 *
 * Frank Warmerdam'2000: I modified this to return 1 if TIFF_BEENWRITING
 * is not set, so that TIFFFlush() will proceed to write out the directory.
 * The documentation says returning 1 is an error indicator, but not having
 * been writing isn't exactly a an error.  Hopefully this doesn't cause
 * problems for other people.
 */
int TIFFFlushData(TIFF * tif)
{
	if((tif->tif_flags & TIFF_BEENWRITING) == 0)
		return 1;
	if(tif->tif_flags & TIFF_POSTENCODE) {
		tif->tif_flags &= ~TIFF_POSTENCODE;
		if(!(*tif->tif_postencode)(tif))
			return 0;
	}
	return (TIFFFlushData1(tif));
}
