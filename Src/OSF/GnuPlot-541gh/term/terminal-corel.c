// corel.trm
//
/*
   A modified ai.trm for CorelDraw import filters
   by Chris Parks, parks@physics.purdue.edu
   Import from CorelDraw with the CorelTrace filter

   syntax: set terminal default
           set terminal mode "fontname" fontsize,xsize,ysize,linewidth

           mode= color or monochrome             (default=mono)
           "fontname"= postscript font name      (default="SwitzerlandLight")
           fontsize  = size of font in points    (default=22pt)
           xsize     = width of page in inches   (default=8.2in)
           ysize     = height of page in inches  (default=10in)
           linewidth = width of lines in points  (default=1.2pt)

 */
/*
 * adapted to the new terminal layout by Stefan Bodewig (Dec. 1995)
 */
/*
 * 2017: removed from default build and marked "legacy"
 */
#include <gnuplot.h>
#pragma hdrstop
#include "driver.h"

// @experimental {
#define TERM_BODY
#define TERM_PUBLIC static
#define TERM_TABLE
#define TERM_TABLE_START(x) GpTermEntry_Static x {
#define TERM_TABLE_END(x)   };
// } @experimental

#ifdef TERM_REGISTER
	register_term(corel)
#endif

//#ifdef TERM_PROTO
TERM_PUBLIC void COREL_options(GpTermEntry_Static * pThis, GnuPlot * pGp);
TERM_PUBLIC void COREL_init(GpTermEntry_Static * pThis);
TERM_PUBLIC void COREL_graphics(GpTermEntry_Static * pThis);
TERM_PUBLIC void COREL_text(GpTermEntry_Static * pThis);
TERM_PUBLIC void COREL_reset(GpTermEntry_Static * pThis);
TERM_PUBLIC void COREL_linetype(GpTermEntry_Static * pThis, int linetype);
TERM_PUBLIC void COREL_move(GpTermEntry_Static * pThis, uint x, uint y);
TERM_PUBLIC void COREL_vector(GpTermEntry_Static * pThis, uint x, uint y);
TERM_PUBLIC void COREL_put_text(GpTermEntry_Static * pThis, uint x, uint y, const char * str);
TERM_PUBLIC int  COREL_text_angle(GpTermEntry_Static * pThis, int ang);
TERM_PUBLIC int  COREL_justify_text(GpTermEntry_Static * pThis, enum JUSTIFY mode);
#define CORELD_XMAX  5960       /* 8.2 inches wide */
#define CORELD_YMAX  7200       /* 10 inches high  */
#define CORELD_VTIC  (CORELD_YMAX/80)
#define CORELD_HTIC  (CORELD_YMAX/80)
#define CORELD_VCHAR (22*COREL_SC)      /* default is 22 point characters */
#define CORELD_HCHAR (22*COREL_SC*6/10)
//#endif

#ifndef TERM_PROTO_ONLY
#ifdef TERM_BODY

#define DEFAULT_CORELFONT "SwitzerlandLight"

// plots for publication should be sans-serif (don't use TimesRoman) 
static char corel_font[MAX_ID_LEN+1] = DEFAULT_CORELFONT; /* name of font */
static int corel_fontsize = 22; /* size of font in pts */
static bool corel_color = FALSE;
static bool corel_stroke = FALSE;
static int corel_path_count = 0; /* count of lines in path */
static int corel_ang = 0; /* text angle */
static enum JUSTIFY corel_justify = LEFT; /* text is flush left */

/* default mode constants */
#define CORELD_XOFF  0          /* page offset in pts */
#define CORELD_YOFF  0
#define COREL_SC     (10.0)     /* scale is 1pt = 10 units */
#define CORELD_LW    (1.2*COREL_SC)     /* linewidth = 1.2 pts */

static uint corel_xmax = CORELD_XMAX;
static uint corel_ymax = CORELD_YMAX;
static float corel_lw = CORELD_LW;

enum COREL_id {COREL_DEFAULT, COREL_MONOCHROME, COREL_COLOR, COREL_OTHER };

static struct gen_table COREL_opts[] =
{
	{ "def$ault", COREL_DEFAULT},
	{ "mono$chrome", COREL_MONOCHROME },
	{ "color$", COREL_COLOR },
	{ "colour$", COREL_COLOR },
	{ NULL, COREL_OTHER }
};

TERM_PUBLIC void COREL_options(GpTermEntry_Static * pThis, GnuPlot * pGp)
{
	GpValue a;
	while(!pGp->Pgm.EndOfCommand()) {
		switch(pGp->Pgm.LookupTableForCurrentToken(&COREL_opts[0])) {
			case COREL_DEFAULT:
			    corel_color = FALSE;
			    strcpy(corel_font, DEFAULT_CORELFONT);
			    corel_fontsize = 22;
			    corel_lw = CORELD_LW;
			    corel_xmax = CORELD_XMAX;
			    corel_ymax = CORELD_YMAX;
			    pGp->Pgm.Shift();
			    break;
			case COREL_MONOCHROME:
			    corel_color = FALSE;
			    pGp->Pgm.Shift();
			    break;
			case COREL_COLOR:
			    corel_color = TRUE;
			    pGp->Pgm.Shift();
			    break;
			case COREL_OTHER:
			default:
			    // font name 
			    if(pGp->IsStringValue(pGp->Pgm.GetCurTokenIdx())) {
				    const char * font = pGp->TryToGetString();
				    strnzcpy(corel_font, font, sizeof(corel_font));
			    }
			    else {
				    // We have font size specified 
				    corel_fontsize = (int)pGp->Real(pGp->ConstExpress(&a));
				    pGp->Pgm.Shift();
				    pThis->SetCharSize((uint)(corel_fontsize * COREL_SC * 6 / 10), (uint)(corel_fontsize * COREL_SC));
			    }
			    break;
		}
	}
	// FIXME - argh. Stupid syntax alert here 
	if(!pGp->Pgm.EndOfCommand()) {
		corel_xmax = (uint)(pGp->Real(pGp->ConstExpress(&a)) * 720);
		pGp->Pgm.Shift();
		if(!pGp->Pgm.EndOfCommand()) {
			corel_ymax = (uint)(pGp->Real(pGp->ConstExpress(&a)) * 720);
			pGp->Pgm.Shift();
		}
		pThis->SetMax(corel_xmax, corel_ymax);
		pThis->SetTic(corel_ymax / 80);
	}
	if(!pGp->Pgm.EndOfCommand()) {
		corel_lw = static_cast<float>(pGp->Real(pGp->ConstExpress(&a)) * COREL_SC);
		pGp->Pgm.Shift();
	}
	slprintf(GPT._TermOptions, "%s \"%s\" %d,%0.1f,%0.1f,%0.1f", corel_color ? "color" : "monochrome", corel_font,
	    corel_fontsize, corel_xmax / 720.0, corel_ymax / 720.0, corel_lw / COREL_SC);
}

TERM_PUBLIC void COREL_init(GpTermEntry_Static * pThis)
{
	fprintf(GPT.P_GpOutFile,
	    "\
%%!PS-Adobe-2.0 EPSF-1.2\n\
%%%%BoundingBox: %d %d %d %d\n\
%%%%TemplateBox: %d %d %d %d\n\
%%%%EndComments\n\
%%%%EndProlog\n\
%%%%BeginSetup\n%%%%EndSetup\n",
	    CORELD_XOFF,
	    CORELD_YOFF,
	    (int)((corel_xmax) / COREL_SC + 0.5 + CORELD_XOFF),
	    (int)((corel_ymax) / COREL_SC + 0.5 + CORELD_YOFF),
	    CORELD_XOFF,
	    CORELD_YOFF,
	    (int)((corel_xmax) / COREL_SC + 0.5 + CORELD_XOFF),
	    (int)((corel_ymax) / COREL_SC + 0.5 + CORELD_YOFF));
}

TERM_PUBLIC void COREL_graphics(GpTermEntry_Static * pThis)
{
	corel_path_count = 0;
	corel_stroke = FALSE;
}

TERM_PUBLIC void COREL_text(GpTermEntry_Static * pThis)
{
	if(corel_stroke) {
		fputs("S\n", GPT.P_GpOutFile);
		corel_stroke = FALSE;
	}
	corel_path_count = 0;
}

TERM_PUBLIC void COREL_reset(GpTermEntry_Static * pThis)
{
	fputs("%%Trailer\n", GPT.P_GpOutFile);
}

TERM_PUBLIC void COREL_linetype(GpTermEntry_Static * pThis, int linetype)
{
	if(corel_stroke) {
		fputs("S\n", GPT.P_GpOutFile);
		corel_stroke = FALSE;
	}
	switch(linetype) {
		case LT_BLACK:
		    fprintf(GPT.P_GpOutFile, "%.2f w\n", corel_lw / COREL_SC);
		    if(corel_color) {
			    fputs("0 0 0 1 K\n", GPT.P_GpOutFile);
		    }
		    else {
			    fputs("\
[] 0 d\n\
0 j\n0 G\n", GPT.P_GpOutFile);
		    }
		    break;

		case LT_AXIS:
		    fprintf(GPT.P_GpOutFile, "%.2f w\n", corel_lw / COREL_SC);
		    if(corel_color) {
			    fputs("0 0 0 1 K\n", GPT.P_GpOutFile);
		    }
		    else {
			    fputs("\
[1 2] 0 d\n\
0 j\n0 G\n", GPT.P_GpOutFile);
		    }
		    break;

		case 0:
		    fprintf(GPT.P_GpOutFile, "%.2f w\n", corel_lw / COREL_SC);
		    if(corel_color) {
			    fputs("1 0 1 0 K\n", GPT.P_GpOutFile);
		    }
		    else {
			    fputs("\
[] 0 d\n\
2 j\n0 G\n", GPT.P_GpOutFile);
		    }
		    break;

		case 1:
		    fprintf(GPT.P_GpOutFile, "%.2f w\n", corel_lw / COREL_SC);
		    if(corel_color) {
			    fputs("1 1 0 0 K\n", GPT.P_GpOutFile);
		    }
		    else {
			    fputs("\
[4 2] 0 d\n\
2 j\n0 G\n", GPT.P_GpOutFile);
		    }
		    break;

		case 2:
		    fprintf(GPT.P_GpOutFile, "%.2f w\n", corel_lw / COREL_SC);
		    if(corel_color) {
			    fputs("0 1 1 0 K\n", GPT.P_GpOutFile);
		    }
		    else {
			    fputs("\
[2 3] 0 d\n\
2 j\n0 G\n", GPT.P_GpOutFile);
		    }
		    break;

		case 3:
		    fprintf(GPT.P_GpOutFile, "%.2f w\n", corel_lw / COREL_SC);
		    if(corel_color) {
			    fputs("0 1 0 0 K\n", GPT.P_GpOutFile);
		    }
		    else {
			    fputs("\
[1 1.5] 0 d\n\
2 j\n0 G\n", GPT.P_GpOutFile);
		    }
		    break;

		case 4:
		    fprintf(GPT.P_GpOutFile, "%f w\n", corel_lw / COREL_SC);
		    if(corel_color) {
			    fputs("1 0 0 0 K\n", GPT.P_GpOutFile);
		    }
		    else {
			    fputs("\
[5 2 1 2] 0 d\n\
2 j\n0 G\n", GPT.P_GpOutFile);
		    }
		    break;

		case 5:
		    fprintf(GPT.P_GpOutFile, "%.2f w\n", corel_lw / COREL_SC);
		    if(corel_color) {
			    fputs("0 0 1 0 K\n", GPT.P_GpOutFile);
		    }
		    else {
			    fputs("\
[4 3 1 3] 0 d\n\
2 j\n0 G\n", GPT.P_GpOutFile);
		    }
		    break;

		case 6:
		    fprintf(GPT.P_GpOutFile, "%.2f w\n", corel_lw / COREL_SC);
		    if(corel_color) {
			    fputs("0 0 0 1 K\n", GPT.P_GpOutFile);
		    }
		    else {
			    fputs("\
[2 2 2 4] 0 d\n\
2 j\n0 G\n", GPT.P_GpOutFile);
		    }
		    break;

		case 7:
		    fprintf(GPT.P_GpOutFile, "%.2f w\n", corel_lw / COREL_SC);
		    if(corel_color) {
			    fputs("0 0.7 1 0 K\n", GPT.P_GpOutFile);
		    }
		    else {
			    fputs("\
[2 2 2 2 2 4] 0 d\n\
2 j\n0 G\n", GPT.P_GpOutFile);
		    }
		    break;

		case 8:
		    fprintf(GPT.P_GpOutFile, "%.2f w\n", corel_lw / COREL_SC);
		    if(corel_color) {
			    fputs("0.5 0.5 0.5 0 K\n", GPT.P_GpOutFile);
		    }
		    else {
			    fputs("\
[2 2 2 2 2 2 2 4] 0 d\n\
2 j\n0 G\n", GPT.P_GpOutFile);
		    }
		    break;
	}
	corel_path_count = 0;
}

TERM_PUBLIC void COREL_move(GpTermEntry_Static * pThis, uint x, uint y)
{
	if(corel_stroke)
		fputs("S\n", GPT.P_GpOutFile);
	fprintf(GPT.P_GpOutFile, "%0.2f %0.2f m\n", x / COREL_SC, y / COREL_SC);
	corel_path_count += 1;
	corel_stroke = TRUE;
}

TERM_PUBLIC void COREL_vector(GpTermEntry_Static * pThis, uint x, uint y)
{
	fprintf(GPT.P_GpOutFile, "%.2f %.2f l\n", x / COREL_SC, y / COREL_SC);
	corel_path_count += 1;
	corel_stroke = TRUE;
	if(corel_path_count >= 400) {
		fprintf(GPT.P_GpOutFile, "S\n%.2f %.2f m\n", x / COREL_SC, y / COREL_SC);
		corel_path_count = 0;
	}
}

TERM_PUBLIC void COREL_put_text(GpTermEntry_Static * pThis, uint x, uint y, const char * str)
{
	char ch;
	if(corel_stroke) {
		fputs("S\n", GPT.P_GpOutFile);
		corel_stroke = FALSE;
	}
	switch(corel_justify) {
		case LEFT:
		    fprintf(GPT.P_GpOutFile, "/_%s %d %d 0 0 z\n",
			corel_font, corel_fontsize, corel_fontsize);
		    break;
		case CENTRE:
		    fprintf(GPT.P_GpOutFile, "/_%s %d %d 0 1 z\n",
			corel_font, corel_fontsize, corel_fontsize);
		    break;
		case RIGHT:
		    fprintf(GPT.P_GpOutFile, "/_%s %d %d 0 2 z\n",
			corel_font, corel_fontsize, corel_fontsize);
		    break;
	}
	if(corel_ang == 0) {
		fprintf(GPT.P_GpOutFile, "[1 0 0 1 %.2f %.2f]e\n0 g\n", x / COREL_SC, y / COREL_SC - corel_fontsize / 3.0);
	}
	else {
		fprintf(GPT.P_GpOutFile, "[0 1 -1 0 %.2f %.2f]e\n0 g\n", x / COREL_SC - corel_fontsize / 3.0, y / COREL_SC);
	}
	putc('(', GPT.P_GpOutFile);
	ch = *str++;
	while(ch) {
		if((ch == '(') || (ch == ')') || (ch == '\\'))
			putc('\\', GPT.P_GpOutFile);
		putc(ch, GPT.P_GpOutFile);
		ch = *str++;
	}
	fputs(")t\nT\n", GPT.P_GpOutFile);
	corel_path_count = 0;
}

TERM_PUBLIC int COREL_text_angle(GpTermEntry_Static * pThis, int ang)
{
	corel_ang = ang;
	return TRUE;
}

TERM_PUBLIC int COREL_justify_text(GpTermEntry_Static * pThis, enum JUSTIFY mode)
{
	corel_justify = mode;
	return TRUE;
}
#endif /* TERM_BODY */

#ifdef TERM_TABLE

TERM_TABLE_START(corel_driver)
	"corel", 
	"EPS format for CorelDRAW",
	CORELD_XMAX, 
	CORELD_YMAX, 
	static_cast<uint>(CORELD_VCHAR),
	static_cast<uint>(CORELD_HCHAR),
	CORELD_VTIC, 
	CORELD_HTIC, 
	COREL_options, 
	COREL_init, 
	COREL_reset,
	COREL_text, 
	GnuPlot::NullScale, 
	COREL_graphics, 
	COREL_move, 
	COREL_vector,
	COREL_linetype, 
	COREL_put_text, 
	COREL_text_angle,
	COREL_justify_text, 
	GnuPlot::DoPoint, 
	GnuPlot::DoArrow, 
	set_font_null 
TERM_TABLE_END(corel_driver)

#undef LAST_TERM
#define LAST_TERM corel_driver

#endif /* TERM_TABLE */
#endif /* TERM_PROTO_ONLY */

#ifdef TERM_HELP
START_HELP(corel)
"1 corel",
"?commands set terminal corel",
"?set terminal corel",
"?set term corel",
"?terminal corel",
"?term corel",
"?corel",
" Legacy terminal for CorelDraw (circa 1995).",
"",
" Syntax:",
"       set terminal corel {monochrome | color} {\"<font>\" {<fontsize>}}",
"                          {<xsize> <ysize> {<linewidth> }}",
"",
" where the fontsize and linewidth are specified in points and the sizes in",
" inches.  The defaults are monochrome, \"SwitzerlandLight\", 22, 8.2, 10 and 1.2."
END_HELP(corel)
#endif /* TERM_HELP */
