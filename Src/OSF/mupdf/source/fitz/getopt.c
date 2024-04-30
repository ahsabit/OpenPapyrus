/*
 * This is a version of the public domain getopt implementation by
 * Henry Spencer originally posted to net.sources.
 *
 * This file is in the public domain.
 */
#include "mupdf/fitz.h"
#pragma hdrstop

#define getopt fz_getopt
#define optarg fz_optarg
#define optind fz_optind

char * optarg; /* Global argument pointer. */
int optind = 0; /* Global argv index. */

static char * scan = NULL; /* Private scan pointer. */

//extern 
//int fz_getopt(int nargc, char * const * nargv, const char * ostr);
int getopt(int argc, char * const * argv, const char * optstring)
{
	char c;
	const char * place;
	optarg = NULL;
	if(!scan || *scan == '\0') {
		if(optind == 0)
			optind++;
		if(optind >= argc || argv[optind][0] != '-' || argv[optind][1] == '\0')
			return EOF;
		if(argv[optind][1] == '-' && argv[optind][2] == '\0') {
			optind++;
			return EOF;
		}
		scan = argv[optind]+1;
		optind++;
	}
	c = *scan++;
	place = sstrchr(optstring, c);
	if(!place || c == ':') {
		slfprintf_stderr("%s: unknown option -%c\n", argv[0], c);
		return '?';
	}
	place++;
	if(*place == ':') {
		if(*scan != '\0') {
			optarg = scan;
			scan = NULL;
		}
		else if(optind < argc) {
			optarg = argv[optind];
			optind++;
		}
		else {
			slfprintf_stderr("%s: option requires argument -%c\n", argv[0], c);
			return ':';
		}
	}
	return c;
}
