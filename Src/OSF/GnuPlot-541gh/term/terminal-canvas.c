// Hello, Emacs, this is -*-C-*- 
// GNUPLOT - canvas.trm 
/*
 * This file is included by ../term.c.
 * This terminal driver supports: W3C HTML <canvas> tag
 *
 * AUTHOR
 *   Bruce Lueckenhoff, Aug 2008, Bruce_Lueckenhoff@yahoo.com
 *
 * Additions
 *   Ethan A Merritt, Jan 2009
 *	CANVAS_set_color(), CANVAS_make_palette(), CANVAS_fillbox(), fillstyles,
 *	CANVAS_point(), CANVAS_pointsize()
 *	"name <foo>" option to create only a callable javascript file foo.js
 *	"fsize <F>" option to select font size (default remains 10.0)
 *   Ethan A Merritt, Jan 2009
 *	Prototype mousing code in separate file gnuplot_mouse.js
 *   Ethan A Merritt, Feb 2009
 *	Enhanced text support. Note: character placement could be done more
 *	precisely by moving the enhanced text code into the javascript routines,
 *	where exact character widths are known, and save/restore can be used.
 *   Ethan A Merritt, Mar 2009
 *	Oversampling and client-side zoom/unzoom, hotkeys
 *   Ethan A Merritt, May 2009
 *	Give each plot its own namespace for mousing (allows multiple mouseable
 *	plots in a single HTML document).
 *   Ethan A Merritt, Nov 2010
 *	Dashed line support, butt/rounded line properties
 *	revised javascript with all methods and variables in container gnuplot()
 *   Ethan A Merritt, Feb/Mar/Apr 2011
 *	Optional explicit background
 *	Wrap each plot in a test for gnuplot.hide_plot_N
 *	Handle image data by storing it in a parallel PNG file
 *   Ethan A Merritt, Mar 2012
 *	Hypertext
 *   Ethan A Merritt, Jan 2018
 *	Always apply new color, including alpha channel, to both strokeStyle
 *	and fillStyle. This allows the terminal to honor RGBA colors for
 *	lines and points (added to core code in version 4.6)
 *
 * send your comments or suggestions to (gnuplot-info@lists.sourceforge.net).
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
	register_term(canvas_driver)
#endif
//#ifdef TERM_PROTO
TERM_PUBLIC void CANVAS_options();
TERM_PUBLIC void CANVAS_init(GpTermEntry_Static * pThis);
TERM_PUBLIC void CANVAS_graphics(GpTermEntry_Static * pThis);
TERM_PUBLIC int  CANVAS_justify_text(GpTermEntry_Static * pThis, enum JUSTIFY mode);
TERM_PUBLIC void CANVAS_text(GpTermEntry_Static * pThis);
TERM_PUBLIC void CANVAS_reset(GpTermEntry_Static * pThis);
TERM_PUBLIC void CANVAS_linetype(GpTermEntry_Static * pThis, int linetype);
TERM_PUBLIC void CANVAS_dashtype(GpTermEntry_Static * pThis, int type, t_dashtype * custom_dash_type);
TERM_PUBLIC void CANVAS_fillbox(GpTermEntry_Static * pThis, int style, uint x1, uint y1, uint width, uint height);
TERM_PUBLIC void CANVAS_linewidth(GpTermEntry_Static * pThis, double linewidth);
TERM_PUBLIC void CANVAS_move(GpTermEntry_Static * pThis, uint x, uint y);
TERM_PUBLIC void CANVAS_vector(GpTermEntry_Static * pThis, uint x, uint y);
TERM_PUBLIC void CANVAS_point(GpTermEntry_Static * pThis, uint x, uint y, int number);
TERM_PUBLIC void CANVAS_pointsize(GpTermEntry_Static * pThis, double size);
TERM_PUBLIC void CANVAS_put_text(GpTermEntry_Static * pThis, uint x, uint y, const char * str);
TERM_PUBLIC int  CANVAS_text_angle(GpTermEntry_Static * pThis, int ang);
TERM_PUBLIC void CANVAS_filled_polygon(GpTermEntry_Static * pThis, int, gpiPoint *);
TERM_PUBLIC void CANVAS_set_color(GpTermEntry_Static * pThis, const t_colorspec * colorspec);
TERM_PUBLIC int  CANVAS_make_palette(GpTermEntry_Static * pThis, t_sm_palette * palette);
TERM_PUBLIC void CANVAS_layer(GpTermEntry_Static * pThis, t_termlayer);
TERM_PUBLIC void CANVAS_path(GpTermEntry_Static * pThis, int);
TERM_PUBLIC void CANVAS_hypertext(GpTermEntry_Static * pThis, int, const char *);
TERM_PUBLIC int  CANVAS_set_font(GpTermEntry_Static * pThis, const char *);
TERM_PUBLIC void ENHCANVAS_OPEN(GpTermEntry_Static * pThis, const char * pFontName, double, double, bool, bool, int);
TERM_PUBLIC void ENHCANVAS_FLUSH(GpTermEntry_Static * pThis);
TERM_PUBLIC void ENHCANVAS_put_text(GpTermEntry_Static * pThis, uint, uint, const char *);

#define CANVAS_OVERSAMPLE       10.
#define CANVAS_XMAX             (600 * CANVAS_OVERSAMPLE)
#define CANVAS_YMAX             (400 * CANVAS_OVERSAMPLE)
#define CANVASVTIC              (10  * CANVAS_OVERSAMPLE)
#define CANVASHTIC              (10  * CANVAS_OVERSAMPLE)
#define CANVASVCHAR             (10  * CANVAS_OVERSAMPLE)
#define CANVASHCHAR             (8   * CANVAS_OVERSAMPLE)
//#endif // TERM_PROTO 

#ifdef TERM_BODY

#define CANVAS_AXIS_CONST '\1'
#define CANVAS_BORDER_CONST '\2'

static int canvas_x = -1; // current X position 
static int canvas_y = -1; // current Y position 
static int canvas_xmax = static_cast<int>(CANVAS_XMAX);
static int canvas_ymax = static_cast<int>(CANVAS_YMAX);
static int canvas_line_type = LT_UNDEFINED;
static int canvas_dash_type = DASHTYPE_SOLID;
static double canvas_linewidth = 1.0;
static double canvas_dashlength_factor = 1.0;
static double CANVAS_ps = 1; /* pointsize multiplier */
static double CANVAS_default_fsize = 10;
static double canvas_font_size = 10;
static double canvas_fontscale = 1.0;
static char * canvas_font_name = NULL;
static const char * canvas_justify = "";
static int canvas_text_angle = 0;
static int canvas_in_a_path = FALSE;
static int already_closed = FALSE;
static bool canvas_dashed = TRUE; /* Version 5: dashes always enabled */
static t_linecap canvas_linecap = ROUNDED;
static bool CANVAS_mouseable = FALSE;
static bool CANVAS_standalone = TRUE;
static char CANVAS_background[18] = {'\0'};
static char * CANVAS_name = NULL;
static char * CANVAS_scriptdir = NULL;
static char * CANVAS_title = NULL;
static char * CANVAS_hypertext_text = NULL;
// 
// Stuff for tracking images stored in separate files
// to be referenced by canvas.drawImage();
// 
static int CANVAS_imageno = 0;
typedef struct canvas_imagefile {
	int imageno; /* Used to generate the internal name */
	char * filename; /* The parallel file that it maps to  */
	struct canvas_imagefile * next;
} canvas_imagefile;
static canvas_imagefile * imagelist = NULL;

static struct {
	int previous_linewidth;
	double alpha; /* alpha channel */
	char color[24]; /* rgba(rrr,ggg,bbb,aaaa) */
	char previous_color[24]; /* rgba(rrr,ggg,bbb,aaaa) */
	char previous_fill[24]; /* rgba(rrr,ggg,bbb,aaaa) */
	int plotno; /* Which plot are we in? */
} canvas_state;

enum CANVAS_case {
	CANVAS_SIZE, 
	CANVAS_FONT, 
	CANVAS_FSIZE,
	CANVAS_NAME, 
	CANVAS_STANDALONE, 
	CANVAS_TITLE,
	CANVAS_LINEWIDTH, 
	CANVAS_MOUSING, 
	CANVAS_JSDIR, 
	CANVAS_ENH, 
	CANVAS_NOENH,
	CANVAS_FONTSCALE, 
	CANVAS_SOLID, 
	CANVAS_DASHED, 
	CANVAS_DASHLENGTH,
	CANVAS_ROUNDED, 
	CANVAS_BUTT, 
	CANVAS_SQUARE, 
	CANVAS_BACKGROUND, 
	CANVAS_OTHER
};

static struct gen_table CANVAS_opts[] = {
	{ "font", CANVAS_FONT },
	{ "fsize", CANVAS_FSIZE },
	{ "name", CANVAS_NAME },
	{ "size", CANVAS_SIZE },
	{ "standalone", CANVAS_STANDALONE },
	{ "mous$ing", CANVAS_MOUSING },
	{ "mouse", CANVAS_MOUSING },
	{ "js$dir", CANVAS_JSDIR },
	{ "enh$anced", CANVAS_ENH },
	{ "noenh$anced", CANVAS_NOENH },
	{ "lw", CANVAS_LINEWIDTH },
	{ "linew$idth", CANVAS_LINEWIDTH },
	{ "title", CANVAS_TITLE },
	{ "fontscale", CANVAS_FONTSCALE },
	{ "solid", CANVAS_SOLID },
	{ "dash$ed", CANVAS_DASHED },
	{ "dashl$ength", CANVAS_DASHLENGTH },
	{ "dl", CANVAS_DASHLENGTH },
	{ "round$ed", CANVAS_ROUNDED },
	{ "butt", CANVAS_BUTT },
	{ "square", CANVAS_SQUARE },
	{ "backg$round", CANVAS_BACKGROUND },
	{ NULL, CANVAS_OTHER }
};

// Fill patterns 
#define PATTERN1 "tile.moveTo(0,0); tile.lineTo(32,32); tile.moveTo(0,16); tile.lineTo(16,32); tile.moveTo(16,0); tile.lineTo(32,16);"
#define PATTERN2 "tile.moveTo(0,32); tile.lineTo(32,0); tile.moveTo(0,16); tile.lineTo(16,0); tile.moveTo(16,32); tile.lineTo(32,16);"
#define PATTERN3 \
	"tile.moveTo(8,0); tile.lineTo(32,24); tile.moveTo(0,8); tile.lineTo(24,32); tile.moveTo(24,0); tile.lineTo(32,8); tile.moveTo(0,24); tile.lineTo(8,32); tile.moveTo(8,32); tile.lineTo(32,8); tile.moveTo(0,24); tile.lineTo(24,0); tile.moveTo(24,32); tile.lineTo(32,24); tile.moveTo(0,8); tile.lineTo(8,0);"

static void CANVAS_start(void)
{
	if(!canvas_in_a_path) {
		fprintf(GPT.P_GpOutFile, "ctx.beginPath();\n");
		canvas_in_a_path = TRUE;
		already_closed = FALSE;
	}
}

static void CANVAS_finish(void)
{
	if(canvas_in_a_path) {
		fprintf(GPT.P_GpOutFile, "ctx.stroke();\n");
		if(!already_closed)
			fprintf(GPT.P_GpOutFile, "ctx.closePath();\n");
		canvas_in_a_path = FALSE;
		already_closed = TRUE;
	}
}

TERM_PUBLIC void CANVAS_options(GpTermEntry_Static * pThis, GnuPlot * pGp)
{
	int canvas_background = 0;
	if(!pGp->Pgm.AlmostEquals(pGp->Pgm.GetPrevTokenIdx(), "termopt$ion")) {
		// Re-initialize a few things 
		canvas_font_size = CANVAS_default_fsize = 10;
		canvas_fontscale = 1.0;
		CANVAS_standalone = TRUE;
		CANVAS_mouseable = FALSE;
		ZFREE(CANVAS_name);
		ZFREE(CANVAS_title);
		ZFREE(CANVAS_scriptdir);
		canvas_linewidth = 1.0;
		canvas_dashed = TRUE;
		canvas_dashlength_factor = 1.0;
		CANVAS_background[0] = '\0';
		/* Default to enhanced text mode */
		pThis->put_text = ENHCANVAS_put_text;
		pThis->SetFlag(TERM_ENHANCED_TEXT);
	}
	while(!pGp->Pgm.EndOfCommand()) {
		const int _option = pGp->Pgm.LookupTableForCurrentToken(&CANVAS_opts[0]);
		pGp->Pgm.Shift();
		switch(_option) {
			case CANVAS_SIZE:
			    if(pGp->Pgm.EndOfCommand()) {
				    canvas_xmax = static_cast<int>(CANVAS_XMAX);
				    canvas_ymax = static_cast<int>(CANVAS_YMAX);
			    }
			    else {
				    canvas_xmax = static_cast<int>(pGp->IntExpression() * CANVAS_OVERSAMPLE);
				    if(pGp->Pgm.EqualsCur(",")) {
					    pGp->Pgm.Shift();
					    canvas_ymax = static_cast<int>(pGp->IntExpression() * CANVAS_OVERSAMPLE);
				    }
			    }
			    if(canvas_xmax <= 0)
				    canvas_xmax = static_cast<int>(CANVAS_XMAX);
			    if(canvas_ymax <= 0)
				    canvas_ymax = static_cast<int>(CANVAS_YMAX);
			    pThis->SetMax(canvas_xmax, canvas_ymax);
			    break;

			case CANVAS_TITLE:
			    CANVAS_title = pGp->TryToGetString();
			    if(!CANVAS_title)
				    pGp->IntErrorCurToken("expecting an HTML title string");
			    break;
			case CANVAS_NAME:
			    CANVAS_name = pGp->TryToGetString();
			    if(!CANVAS_name)
				    pGp->IntErrorCurToken("expecting a javascript function name");
			    if(CANVAS_name[strspn(CANVAS_name, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz_1234567890")])
				    pGp->IntError(pGp->Pgm.GetPrevTokenIdx(), "illegal javascript function name");
			    CANVAS_standalone = FALSE;
			    break;
			case CANVAS_STANDALONE:
			    CANVAS_standalone = TRUE;
			    break;
			case CANVAS_FONT:
			    /* FIXME: See note at CANVAS_set_font() */
			    SAlloc::F(canvas_font_name);
			    if(!(canvas_font_name = pGp->TryToGetString()))
				    pGp->IntErrorCurToken("font: expecting string");
			    CANVAS_set_font(pThis, canvas_font_name);
			    break;
			case CANVAS_FSIZE:
			    CANVAS_default_fsize = pGp->RealExpression();
			    if(CANVAS_default_fsize <= 0)
				    CANVAS_default_fsize = 10;
			    canvas_font_size = CANVAS_default_fsize;
			    break;
			case CANVAS_MOUSING:
			    CANVAS_mouseable = TRUE;
			    break;
			case CANVAS_JSDIR:
			    CANVAS_scriptdir = pGp->TryToGetString();
			    break;
			case CANVAS_ENH:
			    pThis->put_text = ENHCANVAS_put_text;
			    pThis->SetFlag(TERM_ENHANCED_TEXT);
			    break;
			case CANVAS_NOENH:
			    pThis->put_text = CANVAS_put_text;
			    pThis->ResetFlag(TERM_ENHANCED_TEXT);
			    break;
			case CANVAS_LINEWIDTH:
			    canvas_linewidth = pGp->RealExpression();
			    if(canvas_linewidth <= 0)
				    canvas_linewidth = 1.0;
			    break;
			case CANVAS_FONTSCALE:
			    canvas_fontscale = pGp->RealExpression();
			    if(canvas_fontscale <= 0)
				    canvas_fontscale = 1.0;
			    break;
			case CANVAS_SOLID:
			case CANVAS_DASHED:
			    /* Version 5 always allows dashes */
			    canvas_dashed = TRUE;
			    break;
			case CANVAS_DASHLENGTH:
			    canvas_dashlength_factor = pGp->RealExpression();
			    if(canvas_dashlength_factor <= 0.2)
				    canvas_dashlength_factor = 1.0;
			    break;
			case CANVAS_ROUNDED: canvas_linecap = ROUNDED; break;
			case CANVAS_BUTT:    canvas_linecap = BUTT; break;
			case CANVAS_SQUARE:  canvas_linecap = SQUARE; break;
			case CANVAS_BACKGROUND:
			    canvas_background = pGp->ParseColorName();
			    sprintf(CANVAS_background, " rgb(%03d,%03d,%03d)", (canvas_background >> 16) & 0xff, (canvas_background >> 8) & 0xff, canvas_background & 0xff);
			    break;
			default:
			    pGp->IntWarn(pGp->Pgm.GetPrevTokenIdx(), "unrecognized terminal option");
			    break;
		}
	}
	pThis->SetCharSize(static_cast<uint>(canvas_font_size * canvas_fontscale * 0.8 * CANVAS_OVERSAMPLE), static_cast<uint>(canvas_font_size * canvas_fontscale * CANVAS_OVERSAMPLE));
	if(canvas_dashlength_factor != 1.0)
		slprintf_cat(GPT._TermOptions, " dashlength %3.1f", canvas_dashlength_factor);
	slprintf_cat(GPT._TermOptions, canvas_linecap == ROUNDED ? " rounded" : canvas_linecap == SQUARE ? " square" : " butt");
	slprintf_cat(GPT._TermOptions, " size %d,%d", (int)(pThis->MaxX/CANVAS_OVERSAMPLE), (int)(pThis->MaxY/CANVAS_OVERSAMPLE));
	slprintf_cat(GPT._TermOptions, "%s fsize %g lw %g", pThis->put_text == ENHCANVAS_put_text ? " enhanced" : "", canvas_font_size, canvas_linewidth);
	slprintf_cat(GPT._TermOptions, " fontscale %g", canvas_fontscale);
	if(*CANVAS_background)
		slprintf_cat(GPT._TermOptions, " background \"#%06x\"", canvas_background);
	if(CANVAS_name)
		slprintf_cat(GPT._TermOptions, " name \"%s\"", CANVAS_name);
	else {
		slprintf_cat(GPT._TermOptions, " standalone");
		if(CANVAS_mouseable)
			slprintf_cat(GPT._TermOptions, " mousing");
		if(CANVAS_title)
			slprintf_cat(GPT._TermOptions, " title \"%s\"", CANVAS_title);
	}
	if(CANVAS_scriptdir)
		slprintf_cat(GPT._TermOptions, " jsdir \"%s\"", CANVAS_scriptdir);
}

TERM_PUBLIC void CANVAS_init(GpTermEntry_Static * pThis)
{
}

TERM_PUBLIC void CANVAS_graphics(GpTermEntry_Static * pThis)
{
	int len;
	// Force initialization at the beginning of each plot 
	canvas_line_type = LT_UNDEFINED;
	canvas_text_angle = 0;
	canvas_in_a_path = FALSE;
	canvas_state.previous_linewidth = -1;
	canvas_state.previous_color[0] = '\0';
	canvas_state.previous_fill[0] = '\0';
	strcpy(canvas_state.color, "rgba(000,000,000,0.00)");
	canvas_state.plotno = 0;
	/*
	 * FIXME: This code could be shared with svg.trm
	 *  Figure out where to find javascript support routines
	 *  when the page is viewed.
	 */
	if(CANVAS_scriptdir == NULL) {
#ifdef GNUPLOT_JS_DIR
#if defined(_WIN32)
		CANVAS_scriptdir = RelativePathToGnuplot(GNUPLOT_JS_DIR);
#else
		CANVAS_scriptdir = sstrdup(GNUPLOT_JS_DIR); // use hardcoded _absolute_ path 
#endif
#else
		CANVAS_scriptdir = sstrdup("");
#endif /* GNUPLOT_JS_DIR */
	}

	len = strlen(CANVAS_scriptdir);
#if defined(_WIN32)
	if(*CANVAS_scriptdir && CANVAS_scriptdir[len-1] != '\\' && CANVAS_scriptdir[len-1] != '/') {
		CANVAS_scriptdir = (char *)SAlloc::R(CANVAS_scriptdir, len+2);
		if(CANVAS_scriptdir[len-1] == '\\') /* use backslash if used in jsdir, otherwise slash */
			strcat(CANVAS_scriptdir, "\\");
		else
			strcat(CANVAS_scriptdir, "/");
	}
#else
	if(*CANVAS_scriptdir && CANVAS_scriptdir[len-1] != '/') {
		CANVAS_scriptdir = SAlloc::R(CANVAS_scriptdir, len+2);
		strcat(CANVAS_scriptdir, "/");
	}
#endif
	if(CANVAS_standalone) {
		fprintf(GPT.P_GpOutFile, "<!DOCTYPE HTML>\n<html>\n<head>\n<title>%s</title>\n", CANVAS_title ? CANVAS_title : "Gnuplot Canvas Graph");
		if(oneof2(GPT._Encoding, S_ENC_UTF8, S_ENC_DEFAULT))
			fprintf(GPT.P_GpOutFile, "<meta http-equiv=\"content-type\" content=\"text/html; charset=UTF-8\">\n");
		fprintf(GPT.P_GpOutFile, "<!--[if IE]><script type=\"text/javascript\" src=\"excanvas.js\"></script><![endif]-->\n"
		    "<script src=\"%s%s.js\"></script>\n"
		    "<script src=\"%sgnuplot_common.js\"></script>\n", CANVAS_scriptdir, (GPT._Encoding == S_ENC_UTF8) ? "canvasmath" : "canvastext", CANVAS_scriptdir);
		if(canvas_dashed)
			fprintf(GPT.P_GpOutFile, "<script src=\"%sgnuplot_dashedlines.js\"></script>\n", CANVAS_scriptdir);
		if(CANVAS_mouseable) {
			fprintf(GPT.P_GpOutFile, "<script src=\"%sgnuplot_mouse.js\"></script>\n", CANVAS_scriptdir);
			fprintf(GPT.P_GpOutFile, "<script type=\"text/javascript\"> gnuplot.help_URL = \"%s/canvas_help.html\"; </script>\n", CANVAS_scriptdir);
		}
		else {
			fprintf(GPT.P_GpOutFile, "<script type=\"text/javascript\">gnuplot.init = function() {};</script>\n");
		}
		fprintf(GPT.P_GpOutFile, "<script type=\"text/javascript\">\n"
		    "var canvas, ctx;\n"
		    "gnuplot.grid_lines = true;\n"
		    "gnuplot.zoomed = false;\n"
		    "gnuplot.active_plot_name = \"gnuplot_canvas\";\n\n"
		    "function gnuplot_canvas() {\n"
		    "canvas = document.getElementById(\"gnuplot_canvas\");\n"
		    "ctx = canvas.getContext(\"2d\");\n");
	}
	else {
		fprintf(GPT.P_GpOutFile, "function %s() {\n"
		    "canvas = document.getElementById(\"%s\");\n"
		    "ctx = canvas.getContext(\"2d\");\n",
		    CANVAS_name, CANVAS_name);
		fprintf(GPT.P_GpOutFile, "// Suppress refresh on mouseover if this was the plot we just left\n"
		    "if ((gnuplot.active_plot == %s && gnuplot.display_is_uptodate)) return;\n"
		    "else gnuplot.display_is_uptodate = true;\n", CANVAS_name);
		fprintf(GPT.P_GpOutFile, "// Reinitialize mouse tracking and zoom for this particular plot\n"
		    "if ((typeof(gnuplot.active_plot) == \"undefined\" || gnuplot.active_plot != %s) && typeof(gnuplot.mouse_update) != \"undefined\") {\n"
		    "  gnuplot.active_plot_name = \"%s\";\n"
		    "  gnuplot.active_plot = %s;\n"
		    "  canvas.onmousemove = gnuplot.mouse_update;\n"
		    "  canvas.onmouseup = gnuplot.zoom_in;\n"
		    "  canvas.onmousedown = gnuplot.saveclick;\n"
		    "  canvas.onkeypress = gnuplot.do_hotkey;\n"
		    "  if (canvas.attachEvent) {canvas.attachEvent('mouseover', %s);}\n"
		    "  else if (canvas.addEventListener) {canvas.addEventListener('mouseover', %s, false);} \n"
		    "  gnuplot.zoomed = false;\n"
		    "  gnuplot.zoom_axis_width = 0;\n"
		    "  gnuplot.zoom_in_progress = false;\n", CANVAS_name, CANVAS_name, CANVAS_name, CANVAS_name, CANVAS_name);
		fprintf(GPT.P_GpOutFile, "  gnuplot.polar_mode = %s;\n  gnuplot.polar_theta0 = %d;\n  gnuplot.polar_sense = %d;\n  ctx.clearRect(0,0,%d,%d);\n}\n",
		    STextConst::GetBool(pThis->P_Gp->Gg.Polar), (int)pThis->P_Gp->AxS.ThetaOrigin, (int)pThis->P_Gp->AxS.ThetaDirection, (int)(pThis->MaxX / CANVAS_OVERSAMPLE), (int)(pThis->MaxY / CANVAS_OVERSAMPLE));
	}
	fprintf(GPT.P_GpOutFile, "// Gnuplot version %s.%s\n", gnuplot_version, gnuplot_patchlevel);
	fprintf(GPT.P_GpOutFile, "// short forms of commands provided by gnuplot_common.js\n"
	    "function DT  (dt) {gnuplot.dashtype(dt);};\n"
	    "function DS  (x,y) {gnuplot.dashstart(x,y);};\n"
	    "function DL  (x,y) {gnuplot.dashstep(x,y);};\n"
	    "function M   (x,y) {if (gnuplot.pattern.length > 0) DS(x,y); else gnuplot.M(x,y);};\n"
	    "function L   (x,y) {if (gnuplot.pattern.length > 0) DL(x,y); else gnuplot.L(x,y);};\n"
	    "function Dot (x,y) {gnuplot.Dot(x/10.,y/10.);};\n"
	    "function Pt  (N,x,y,w) {gnuplot.Pt(N,x/10.,y/10.,w/10.);};\n"
	    "function R   (x,y,w,h) {gnuplot.R(x,y,w,h);};\n"
	    "function T   (x,y,fontsize,justify,string) {gnuplot.T(x,y,fontsize,justify,string);};\n"
	    "function TR  (x,y,angle,fontsize,justify,string) {gnuplot.TR(x,y,angle,fontsize,justify,string);};\n"
	    "function bp  (x,y) {gnuplot.bp(x,y);};\n"
	    "function cfp () {gnuplot.cfp();};\n"
	    "function cfsp() {gnuplot.cfsp();};\n"
	    "\n");
	fprintf(GPT.P_GpOutFile, "gnuplot.hypertext_list = [];\n"
	    "gnuplot.on_hypertext = -1;\n"
	    "function Hypertext(x,y,w,text) {\n"
	    "    newtext = {x:x, y:y, w:w, text:text};\n"
	    "    gnuplot.hypertext_list.push(newtext);\n"
	    "}\n");
	fprintf(GPT.P_GpOutFile, "gnuplot.dashlength = %d;\n", (int)(400 * canvas_dashlength_factor));
	fprintf(GPT.P_GpOutFile, "ctx.lineCap = \"%s\"; ctx.lineJoin = \"%s\";\n",
	    canvas_linecap == ROUNDED ? "round" : canvas_linecap == SQUARE ? "square" : "butt",
	    canvas_linecap == ROUNDED ? "round" : "miter");
	if(*CANVAS_background) {
		fprintf(GPT.P_GpOutFile, "ctx.fillStyle = \"%s\";\nctx.fillRect(0,0,%d,%d);\n",
		    CANVAS_background, (int)(pThis->MaxX / CANVAS_OVERSAMPLE), (int)(pThis->MaxY / CANVAS_OVERSAMPLE));
	}
	fprintf(GPT.P_GpOutFile, "CanvasTextFunctions.enable(ctx);\nctx.strokeStyle = \" rgb(215,215,215)\";\nctx.lineWidth = %.1g;\n\n", canvas_linewidth);
}

static void CANVAS_mouse_param(GpTermEntry_Static * pThis, const char * gp_name, const char * js_name)
{
	struct udvt_entry * udv;
	if((udv = pThis->P_Gp->Ev.AddUdvByName(gp_name)) != 0) {
		if(udv->udv_value.Type == INTGR) {
			fprintf(GPT.P_GpOutFile, "%s = ", js_name);
			fprintf(GPT.P_GpOutFile, PLD, udv->udv_value.v.int_val);
			fprintf(GPT.P_GpOutFile, "\n");
		}
		else if(udv->udv_value.Type == CMPLX) {
			fprintf(GPT.P_GpOutFile, "%s = %g;\n", js_name, udv->udv_value.v.cmplx_val.real);
		}
	}
}

TERM_PUBLIC void CANVAS_text(GpTermEntry_Static * pThis)
{
	GnuPlot * p_gp = pThis->P_Gp;
	GpAxis * this_axis;
	CANVAS_finish();
	// FIXME: I am not sure whether these variable names should always be the 
	// same, so that they are re-used by all plots in a document, or whether  
	// they should be tied to the function name and hence private.            
	if(TRUE) {
		fprintf(GPT.P_GpOutFile, "\n// plot boundaries and axis scaling information for mousing \n");
		fprintf(GPT.P_GpOutFile, "gnuplot.plot_term_xmax = %d;\n", (int)(pThis->MaxX / CANVAS_OVERSAMPLE));
		fprintf(GPT.P_GpOutFile, "gnuplot.plot_term_ymax = %d;\n", (int)(pThis->MaxY / CANVAS_OVERSAMPLE));
		fprintf(GPT.P_GpOutFile, "gnuplot.plot_xmin = %.1f;\n", (double)p_gp->V.BbPlot.xleft / CANVAS_OVERSAMPLE);
		fprintf(GPT.P_GpOutFile, "gnuplot.plot_xmax = %.1f;\n", (double)p_gp->V.BbPlot.xright / CANVAS_OVERSAMPLE);
		fprintf(GPT.P_GpOutFile, "gnuplot.plot_ybot = %.1f;\n", (double)(pThis->MaxY-p_gp->V.BbPlot.ybot) / CANVAS_OVERSAMPLE);
		fprintf(GPT.P_GpOutFile, "gnuplot.plot_ytop = %.1f;\n", (double)(pThis->MaxY-p_gp->V.BbPlot.ytop) / CANVAS_OVERSAMPLE);
		fprintf(GPT.P_GpOutFile, "gnuplot.plot_width = %.1f;\n", (double)(p_gp->V.BbPlot.xright - p_gp->V.BbPlot.xleft) / CANVAS_OVERSAMPLE);
		fprintf(GPT.P_GpOutFile, "gnuplot.plot_height = %.1f;\n", (double)(p_gp->V.BbPlot.ytop - p_gp->V.BbPlot.ybot) / CANVAS_OVERSAMPLE);
		// Get true axis ranges as used in the plot 
		p_gp->UpdateGpvalVariables(pThis, 1);
#define MOUSE_PARAM(GP_NAME, js_NAME) CANVAS_mouse_param(pThis, GP_NAME, js_NAME)
		if(p_gp->AxS[FIRST_X_AXIS].datatype != DT_TIMEDATE) {
			MOUSE_PARAM("GPVAL_X_MIN", "gnuplot.plot_axis_xmin");
			MOUSE_PARAM("GPVAL_X_MAX", "gnuplot.plot_axis_xmax");
		}
		// FIXME: Should this inversion be done at a higher level? 
		if(p_gp->Gg.Is3DPlot && p_gp->_3DBlk.splot_map) {
			MOUSE_PARAM("GPVAL_Y_MAX", "gnuplot.plot_axis_ymin");
			MOUSE_PARAM("GPVAL_Y_MIN", "gnuplot.plot_axis_ymax");
		}
		else {
			MOUSE_PARAM("GPVAL_Y_MIN", "gnuplot.plot_axis_ymin");
			MOUSE_PARAM("GPVAL_Y_MAX", "gnuplot.plot_axis_ymax");
		}
		if(p_gp->Gg.Polar) {
			fprintf(GPT.P_GpOutFile, "gnuplot.plot_axis_rmin = %g;\n", (p_gp->AxS.__R().autoscale & AUTOSCALE_MIN) ? 0.0 : p_gp->AxS.__R().set_min);
			fprintf(GPT.P_GpOutFile, "gnuplot.plot_axis_rmax = %g;\n", p_gp->AxS.__R().set_max);
		}
		if((p_gp->AxS[SECOND_X_AXIS].ticmode & TICS_MASK) != NO_TICS) {
			MOUSE_PARAM("GPVAL_X2_MIN", "gnuplot.plot_axis_x2min");
			MOUSE_PARAM("GPVAL_X2_MAX", "gnuplot.plot_axis_x2max");
		}
		else
			fprintf(GPT.P_GpOutFile, "gnuplot.plot_axis_x2min = \"none\"\n");
		if(p_gp->AxS[SECOND_X_AXIS].linked_to_primary && p_gp->AxS[FIRST_X_AXIS].link_udf->at) {
			fprintf(GPT.P_GpOutFile, "gnuplot.x2_mapping = function(x) { return x; };");
			fprintf(GPT.P_GpOutFile, "  // replace returned value with %s\n", p_gp->AxS[FIRST_X_AXIS].link_udf->definition);
		}
		if((p_gp->AxS[SECOND_Y_AXIS].ticmode & TICS_MASK) != NO_TICS) {
			MOUSE_PARAM("GPVAL_Y2_MIN", "gnuplot.plot_axis_y2min");
			MOUSE_PARAM("GPVAL_Y2_MAX", "gnuplot.plot_axis_y2max");
		}
		else
			fprintf(GPT.P_GpOutFile, "gnuplot.plot_axis_y2min = \"none\"\n");
		if(p_gp->AxS[SECOND_Y_AXIS].linked_to_primary && p_gp->AxS[FIRST_Y_AXIS].link_udf->at) {
			fprintf(GPT.P_GpOutFile, "gnuplot.y2_mapping = function(y) { return y; };");
			fprintf(GPT.P_GpOutFile, "  // replace returned value with %s\n", p_gp->AxS[FIRST_Y_AXIS].link_udf->definition);
		}
#undef MOUSE_PARAM
		/*
		 * Note:
		 * Offline mousing cannot automatically deal with
		 * (1) nonlinear axes other than logscale
		 * (2) [x,y]->plot_coordinates as specified by mouse_mode 8
		 *		'set mouse mouseformat function <foo>'
		 *     Both of these states are noted by gnuplot_svg.plot_logaxis_* < 0
		 *     Linked axes that happen to be nonlinear are incorrectly treated as linear
		 * (3) generic user-specified coordinate format (mouse_alt_string)
		 *        'set mouse mouseformat "foo"'
		 *     Special case mouse_alt_string formats recognized by gnuplot_svg.js are
		 *     "Time", "Date", and "DateTime".
		 * FIXME: This all needs to be documented somewhere!
		 */
		#define is_nonlinear(axis) ((axis)->linked_to_primary && (axis)->link_udf->at && (axis)->index == -((axis)->linked_to_primary->index))

		this_axis = &p_gp->AxS[FIRST_X_AXIS];
		fprintf(GPT.P_GpOutFile, "gnuplot.plot_logaxis_x = %d;\n", this_axis->log ? 1 : (mouse_mode == MOUSE_COORDINATES_FUNCTION || is_nonlinear(this_axis)) ? -1 : 0);
		this_axis = &p_gp->AxS[FIRST_Y_AXIS];
		fprintf(GPT.P_GpOutFile, "gnuplot.plot_logaxis_y = %d;\n", this_axis->log ? 1 : (mouse_mode == MOUSE_COORDINATES_FUNCTION || is_nonlinear(this_axis)) ? -1 : 0);
		if(p_gp->Gg.Polar)
			fprintf(GPT.P_GpOutFile, "gnuplot.plot_logaxis_r = %d;\n", p_gp->AxS[POLAR_AXIS].log ? 1 : 0);
		if(p_gp->AxS[FIRST_X_AXIS].datatype == DT_TIMEDATE) {
			// May need to reconstruct time to millisecond precision 
			fprintf(GPT.P_GpOutFile, "gnuplot.plot_axis_xmin = %.3f;\n", p_gp->AxS[FIRST_X_AXIS].min);
			fprintf(GPT.P_GpOutFile, "gnuplot.plot_axis_xmax = %.3f;\n", p_gp->AxS[FIRST_X_AXIS].max);
			fprintf(GPT.P_GpOutFile, "gnuplot.plot_timeaxis_x = \"%s\";\n", (mouse_alt_string) ? mouse_alt_string : (mouse_mode == 4) ? "Date" : (mouse_mode == 5) ? "Time" : "DateTime");
		}
		else if(p_gp->AxS[FIRST_X_AXIS].datatype == DT_DMS) {
			fprintf(GPT.P_GpOutFile, "gnuplot.plot_timeaxis_x = \"%s\";\n", (mouse_alt_string) ? mouse_alt_string : "DMS");
		}
		else
			fprintf(GPT.P_GpOutFile, "gnuplot.plot_timeaxis_x = \"\";\n");
		if(p_gp->AxS[FIRST_Y_AXIS].datatype == DT_DMS) {
			fprintf(GPT.P_GpOutFile, "gnuplot.plot_timeaxis_y = \"%s\";\n", (mouse_alt_string) ? mouse_alt_string : "DMS");
		}
		else
			fprintf(GPT.P_GpOutFile, "gnuplot.plot_timeaxis_y = \"\";\n");
		fprintf(GPT.P_GpOutFile, "gnuplot.plot_axis_width = gnuplot.plot_axis_xmax - gnuplot.plot_axis_xmin;\n");
		fprintf(GPT.P_GpOutFile, "gnuplot.plot_axis_height = gnuplot.plot_axis_ymax - gnuplot.plot_axis_ymin;\n");
	} /* End of section writing out variables for mousing */
	// This is the end of the javascript plot 
	fprintf(GPT.P_GpOutFile, "}\n");
#ifdef WRITE_PNG_IMAGE
	// Link internal image structures to external PNG files 
	if(imagelist) {
		canvas_imagefile * thisimage = imagelist;
		char * base_name = CANVAS_name ? CANVAS_name : "gp";
		while(thisimage) {
			fprintf(stderr, " linking image %d to external file %s\n", thisimage->imageno, thisimage->filename);
			fprintf(GPT.P_GpOutFile, "  var %s_image_%02d = new Image();", base_name, thisimage->imageno);
			fprintf(GPT.P_GpOutFile, "  %s_image_%02d.src = \"%s\";\n", base_name, thisimage->imageno, thisimage->filename);
			imagelist = thisimage->next;
			SAlloc::F(thisimage->filename);
			SAlloc::F(thisimage);
			thisimage = imagelist;
		}
	}
#endif
	if(CANVAS_standalone) {
		fprintf(GPT.P_GpOutFile, "</script>\n"
		    "<link type=\"text/css\" href=\"%sgnuplot_mouse.css\" rel=\"stylesheet\">\n"
		    "</head>\n"
		    "<body onload=\"gnuplot_canvas(); gnuplot.init();\" oncontextmenu=\"return false;\">\n\n"
		    "<div class=\"gnuplot\">\n",
		    CANVAS_scriptdir ? CANVAS_scriptdir : "");
		// Pattern-fill requires having a canvas element to hold a template
		// pattern tile.  Define it here but try to hide it (may not work in IE?)
		fprintf(GPT.P_GpOutFile, "<canvas id=\"Tile\" width=\"32\" height=\"32\" hidden></canvas>\n");
		// The format of the plot box and in particular the mouse tracking box
		// are determined by the CSS specs in customizable file gnuplot_mouse.css
		// We could make this even more customizable by providing an external HTML
		// template, but in that case the user might as well just create a *.js
		// file and provide his own wrapping HTML document.
		if(CANVAS_mouseable) {
			int i;
			fprintf(GPT.P_GpOutFile, "<table class=\"mbleft\"><tr><td class=\"mousebox\">\n"
			    "<table class=\"mousebox\" border=0>\n"
			    "  <tr><td class=\"mousebox\">\n"
			    "    <table class=\"mousebox\" id=\"gnuplot_mousebox\" border=0>\n"
			    "    <tr><td class=\"mbh\"></td></tr>\n"
			    "    <tr><td class=\"mbh\">\n"
			    "      <table class=\"mousebox\">\n"
			    "	<tr>\n"
			    "	  <td class=\"icon\"></td>\n"
			    "	  <td class=\"icon\" onclick=gnuplot.toggle_grid><img src=\"%sgrid.png\" id=\"gnuplot_grid_icon\" class=\"icon-image\" alt=\"#\" title=\"toggle grid\"></td>\n"
			    "	  <td class=\"icon\" onclick=gnuplot.unzoom><img src=\"%spreviouszoom.png\" id=\"gnuplot_unzoom_icon\" class=\"icon-image\" alt=\"unzoom\" title=\"unzoom\"></td>\n"
			    "	  <td class=\"icon\" onclick=gnuplot.rezoom><img src=\"%snextzoom.png\" id=\"gnuplot_rezoom_icon\" class=\"icon-image\" alt=\"rezoom\" title=\"rezoom\"></td>\n"
			    "	  <td class=\"icon\" onclick=gnuplot.toggle_zoom_text><img src=\"%stextzoom.png\" id=\"gnuplot_textzoom_icon\" class=\"icon-image\" alt=\"zoom text\" title=\"zoom text with plot\"></td>\n"
			    "	  <td class=\"icon\" onclick=gnuplot.popup_help()><img src=\"%shelp.png\" id=\"gnuplot_help_icon\" class=\"icon-image\" alt=\"?\" title=\"help\"></td>\n"
			    "	</tr>\n",
			    CANVAS_scriptdir ? CANVAS_scriptdir : "", CANVAS_scriptdir ? CANVAS_scriptdir : "", CANVAS_scriptdir ? CANVAS_scriptdir : "",
			    CANVAS_scriptdir ? CANVAS_scriptdir : "", CANVAS_scriptdir ? CANVAS_scriptdir : "");

			/* Table row of plot toggle icons goes here */
			for(i = 1; i<=6*((canvas_state.plotno+5)/6); i++) {
				if((i%6) == 1)
					fprintf(GPT.P_GpOutFile, "	<tr>\n");
				if(i<=canvas_state.plotno)
					fprintf(GPT.P_GpOutFile, "	  <td class=\"icon\" onclick=gnuplot.toggle_plot(\"gp_plot_%d\")>%d</td>\n", i, i);
				else
					fprintf(GPT.P_GpOutFile, "	  <td class=\"icon\" > </td>\n");
				if((i%6) == 0)
					fprintf(GPT.P_GpOutFile, "	</tr>\n");
			}
			fprintf(GPT.P_GpOutFile, "      </table>\n  </td></tr>\n</table></td></tr><tr><td class=\"mousebox\">\n");
			fprintf(GPT.P_GpOutFile, "<table class=\"mousebox\" id=\"gnuplot_mousebox\" border=1>\n"
			    "<tr> <td class=\"mb0\">x&nbsp;</td> <td class=\"mb1\"><span id=\"gnuplot_canvas_x\">&nbsp;</span></td> </tr>\n"
			    "<tr> <td class=\"mb0\">y&nbsp;</td> <td class=\"mb1\"><span id=\"gnuplot_canvas_y\">&nbsp;</span></td> </tr>\n");

			if((p_gp->AxS[SECOND_X_AXIS].ticmode & TICS_MASK) != NO_TICS)
				fprintf(GPT.P_GpOutFile, "<tr> <td class=\"mb0\">x2&nbsp;</td> <td class=\"mb1\"><span id=\"gnuplot_canvas_x2\">&nbsp;</span></td> </tr>\n");
			if((p_gp->AxS[SECOND_Y_AXIS].ticmode & TICS_MASK) != NO_TICS)
				fprintf(GPT.P_GpOutFile, "<tr> <td class=\"mb0\">y2&nbsp;</td> <td class=\"mb1\"><span id=\"gnuplot_canvas_y2\">&nbsp;</span></td> </tr>\n");
			fprintf(GPT.P_GpOutFile, "</table></td></tr>\n</table>\n");
			fprintf(GPT.P_GpOutFile, "</td><td>\n");
		} /* End if (CANVAS_mouseable) */
		fprintf(GPT.P_GpOutFile, "<table class=\"plot\">\n"
		    "<tr><td>\n"
		    "    <canvas id=\"gnuplot_canvas\" width=\"%d\" height=\"%d\" tabindex=\"0\">\n"
		    "	Sorry, your browser seems not to support the HTML 5 canvas element\n"
		    "    </canvas>\n"
		    "</td></tr>\n"
		    "</table>\n",
		    (int)(pThis->MaxX/CANVAS_OVERSAMPLE), (int)(pThis->MaxY/CANVAS_OVERSAMPLE));
		if(CANVAS_mouseable) {
			fprintf(GPT.P_GpOutFile, "</td></tr></table>\n");
		}
		fprintf(GPT.P_GpOutFile, "</div>\n\n</body>\n</html>\n");
	}
	fflush(GPT.P_GpOutFile);
}

TERM_PUBLIC void CANVAS_reset(GpTermEntry_Static * pThis)
{
	;
}

TERM_PUBLIC void CANVAS_linetype(GpTermEntry_Static * pThis, int linetype)
{
	/* NB: These values are manipulated as numbers; */
	/* it does not work to give only the color name */
	static const char * pen_type[17] = {
		" rgb(255,255,255)", /* should be background */
		" rgb(000,000,000)", /* black */
		" rgb(160,160,160)", /* grey */
		" rgb(255,000,000)", /* red */
		" rgb(000,171,000)", /* green */
		" rgb(000,000,225)", /* blue */
		" rgb(190,000,190)", /* purple */
		" rgb(000,255,255)", /* cyan */
		" rgb(021,117,069)", /* pine green*/
		" rgb(000,000,148)", /* navy */
		" rgb(255,153,000)", /* orange */
		" rgb(000,153,161)", /* green blue*/
		" rgb(214,214,069)", /* olive*/
		" rgb(163,145,255)", /* cornflower*/
		" rgb(255,204,000)", /* gold*/
		" rgb(214,000,120)", /* mulberry*/
		" rgb(171,214,000)", /* green yellow*/
	};
	// FIXME if (linetype == canvas_line_type) return; 
	canvas_line_type = linetype;
	CANVAS_finish();
	if(linetype >= 14)
		linetype %= 14;
	if(linetype <= LT_NODRAW) /* LT_NODRAW, LT_BACKGROUND, LT_UNDEFINED */
		linetype = -3;
	if(linetype == -3 && *CANVAS_background)
		strcpy(canvas_state.color, CANVAS_background);
	else
		strcpy(canvas_state.color, pen_type[linetype + 3]);
	if(strcmp(canvas_state.color, canvas_state.previous_color)) {
		fprintf(GPT.P_GpOutFile, "ctx.strokeStyle = \"%s\";\n", canvas_state.color);
		strcpy(canvas_state.previous_color, canvas_state.color);
	}
	if(canvas_line_type == LT_NODRAW)
		CANVAS_dashtype(pThis, DASHTYPE_NODRAW, NULL);
}

TERM_PUBLIC void CANVAS_dashtype(GpTermEntry_Static * pThis, int type, t_dashtype * custom_dash_type)
{
	if(canvas_line_type == LT_NODRAW)
		type = DASHTYPE_NODRAW;
	if(canvas_line_type == LT_AXIS)
		type = DASHTYPE_AXIS;
	switch(type) {
		case DASHTYPE_AXIS:
		    fprintf(GPT.P_GpOutFile, "DT(gnuplot.dashpattern3);\n");
		    break;
		case DASHTYPE_SOLID:
		    if(canvas_dash_type != DASHTYPE_SOLID)
			    fprintf(GPT.P_GpOutFile, "DT(gnuplot.solid);\n");
		    break;
		case DASHTYPE_NODRAW:
		    fprintf(GPT.P_GpOutFile, "DT([0.0,1.0]);\n");
		    break;
		default:
		    type = type % 5;
		    if(canvas_dash_type != type)
			    fprintf(GPT.P_GpOutFile, "DT(gnuplot.dashpattern%1d);\n", type + 1);
		    break;
		case DASHTYPE_CUSTOM:
		    if(custom_dash_type) {
			    double length = 0;
			    double cumulative = 0;
			    int i;
			    for(i = 0; i < DASHPATTERN_LENGTH && custom_dash_type->pattern[i] > 0; i++)
				    length += custom_dash_type->pattern[i];
			    fprintf(GPT.P_GpOutFile, "DT([");
			    for(i = 0; i < DASHPATTERN_LENGTH && custom_dash_type->pattern[i] > 0; i++) {
				    cumulative += custom_dash_type->pattern[i];
				    fprintf(GPT.P_GpOutFile, " %4.2f,", cumulative / length);
			    }
			    fprintf(GPT.P_GpOutFile, " 0]);\n");
		    }
		    break;
	}
	canvas_dash_type = type;
}

TERM_PUBLIC void CANVAS_move(GpTermEntry_Static * pThis, uint arg_x, uint arg_y)
{
	if(canvas_in_a_path && (canvas_x == arg_x) && (canvas_y == arg_y)) {
		return;
	}
	CANVAS_start();
	fprintf(GPT.P_GpOutFile, "M(%u,%u);\n", arg_x, canvas_ymax - arg_y);
	canvas_x = arg_x;
	canvas_y = arg_y;
}

TERM_PUBLIC void CANVAS_vector(GpTermEntry_Static * pThis, uint arg_x, uint arg_y)
{
	if((canvas_x == arg_x) && (canvas_y == arg_y))
		return;
	if(!canvas_in_a_path) {
		// Force a new path 
		CANVAS_move(pThis, canvas_x, canvas_y);
	}
	fprintf(GPT.P_GpOutFile, "L(%u,%u);\n", arg_x, canvas_ymax - arg_y);
	canvas_x = arg_x;
	canvas_y = arg_y;
}

TERM_PUBLIC int CANVAS_justify_text(GpTermEntry_Static * pThis, enum JUSTIFY mode)
{
	switch(mode) {
		case (CENTRE): canvas_justify = "Center"; break;
		case (RIGHT): canvas_justify = "Right"; break;
		default:
		case (LEFT): canvas_justify = ""; break;
	}
	return TRUE;
}

TERM_PUBLIC void CANVAS_point(GpTermEntry_Static * pThis, uint x, uint y, int number)
{
	double width  = CANVAS_ps * 0.6 * CANVASHTIC;
	int pt = number % 9;
	// Skip size 0 points;  the alternative would be to draw a Dot instead 
	if(width <= 0 && pt >= 0)
		return;
	CANVAS_finish();
	switch(pt) {
		default:
		    fprintf(GPT.P_GpOutFile, "Dot(%d,%d);\n", x, (canvas_ymax-y));
		    break;
		case 4:
		case 6:
		case 8:
		    if(strcmp(canvas_state.previous_fill, canvas_state.color)) {
			    fprintf(GPT.P_GpOutFile, "ctx.fillStyle = \"%s\";\n", canvas_state.color);
			    strcpy(canvas_state.previous_fill, canvas_state.color);
		    }
		// @fallthrough
		case 0:
		case 1:
		case 2:
		case 3:
		case 5:
		case 7:
		    fprintf(GPT.P_GpOutFile, "Pt(%d,%d,%d,%.1f);\n", pt, x, (canvas_ymax-y), width);
		    break;
	}
	/* Hypertext support */
	if(CANVAS_hypertext_text) {
		/* EAM DEBUG: embedded \n produce illegal javascript output */
		while(sstrchr(CANVAS_hypertext_text, '\n'))
			*sstrchr(CANVAS_hypertext_text, '\n') = '\v';
		fprintf(GPT.P_GpOutFile, "Hypertext(%d,%d,%.1f,\"%s\");\n", x, (canvas_ymax-y), width, CANVAS_hypertext_text);
		ZFREE(CANVAS_hypertext_text);
	}
}

TERM_PUBLIC void CANVAS_pointsize(GpTermEntry_Static * pThis, double ptsize)
{
	CANVAS_ps = (ptsize < 0.0) ? 1.0 : ptsize;
}

TERM_PUBLIC int CANVAS_text_angle(GpTermEntry_Static * pThis, int ang)
{
	canvas_text_angle = -1 * ang;
	return TRUE;
}

TERM_PUBLIC void CANVAS_put_text(GpTermEntry_Static * pThis, uint x, uint y, const char * str)
{
	if(!isempty(str)) {
		CANVAS_finish();
		// ctx.fillText uses fillStyle rather than strokeStyle 
		if(strcmp(canvas_state.previous_fill, canvas_state.color)) {
			fprintf(GPT.P_GpOutFile, "ctx.fillStyle = \"%s\";\n", canvas_state.color);
			strcpy(canvas_state.previous_fill, canvas_state.color);
		}
		if(0 != canvas_text_angle) {
			fprintf(GPT.P_GpOutFile, "TR(%d,%d,%d,%.1f,\"%s\",\"", x, (int)(canvas_ymax + 5*CANVAS_OVERSAMPLE - y),
				canvas_text_angle, canvas_font_size * canvas_fontscale, canvas_justify);
		}
		else {
			fprintf(GPT.P_GpOutFile, "T(%d,%d,%.1f,\"%s\",\"", x, (int)(canvas_ymax + 5*CANVAS_OVERSAMPLE - y),
				canvas_font_size * canvas_fontscale, canvas_justify);
		}
		// Sanitize string by escaping quote characters 
		do {
			if(*str == '"' || *str == '\\')
				fputc('\\', GPT.P_GpOutFile);
			fputc(*str++, GPT.P_GpOutFile);
		} while(*str);
		fprintf(GPT.P_GpOutFile, "\");\n");
	}
}

TERM_PUBLIC void CANVAS_linewidth(GpTermEntry_Static * pThis, double linewidth)
{
	CANVAS_finish();
	if(canvas_state.previous_linewidth != linewidth) {
		fprintf(GPT.P_GpOutFile, "ctx.lineWidth = %g;\n", linewidth * canvas_linewidth);
		canvas_state.previous_linewidth = static_cast<int>(linewidth);
	}
}

TERM_PUBLIC void CANVAS_set_color(GpTermEntry_Static * pThis, const t_colorspec * colorspec)
{
	rgb255_color rgb255;
	canvas_state.alpha = 0.0;
	if(colorspec->type == TC_LT) {
		CANVAS_linetype(pThis, colorspec->lt);
		return;
	}
	else if(colorspec->type == TC_RGB) {
		rgb255.r = (colorspec->lt >> 16) & 0xff;
		rgb255.g = (colorspec->lt >> 8) & 0xff;
		rgb255.b = colorspec->lt & 0xff;
		canvas_state.alpha = (double)((colorspec->lt >> 24) & 0xff)/255.0;
	}
	else if(colorspec->type == TC_FRAC) {
		pThis->P_Gp->Rgb255MaxColorsFromGray(colorspec->value, &rgb255);
	}
	else
		return; // Other color types not yet supported 
	CANVAS_finish();
	// Jan 2017: Always allow for alpha channel 
	sprintf(canvas_state.color, "rgba(%03d,%03d,%03d,%4.2f)", rgb255.r, rgb255.g, rgb255.b, 1.0 - canvas_state.alpha);
	// Jan 2017: Apply color to BOTH strokeStyle and fillStyle 
	if(strcmp(canvas_state.color, canvas_state.previous_color)) {
		fprintf(GPT.P_GpOutFile, "ctx.strokeStyle = \"%s\";\n", canvas_state.color);
		fprintf(GPT.P_GpOutFile, "ctx.fillStyle = \"%s\";\n", canvas_state.color);
		strcpy(canvas_state.previous_color, canvas_state.color);
		strcpy(canvas_state.previous_fill, canvas_state.color);
	}
	canvas_line_type = LT_UNDEFINED;
}

TERM_PUBLIC int CANVAS_make_palette(GpTermEntry_Static * pThis, t_sm_palette * palette)
{
	return 0; // We can do full RGB color 
}

static char * CANVAS_fillstyle(int style)
{
	float density = (float)(style >> 4) / 100.0f;
	int pattern = style >> 4;
	bool transparent_pattern = TRUE;
	static char fillcolor[24];
	switch(style & 0xf) {
		case FS_TRANSPARENT_SOLID:
		    sprintf(fillcolor, "rgba(%11.11s,%4.2f)%c", &canvas_state.color[5], density, '\0');
		    break;
		case FS_EMPTY:
		    strcpy(fillcolor, "rgba(255,255,255,0.00)");
		    break;
		case FS_PATTERN:
		    transparent_pattern = FALSE;
		case FS_TRANSPARENT_PATTERN:
		    if(pattern%6 == 3) {
			    strcpy(fillcolor, canvas_state.color);
			    strcpy(canvas_state.previous_fill, "");
			    break;
		    }
		    /* Write one copy of the pattern into the Tile element. */
		    fprintf(GPT.P_GpOutFile, "var template = document.getElementById('Tile');\nvar tile = template.getContext('2d');\ntile.clearRect(0,0,32,32);\n");
		    if(!transparent_pattern)
			    fprintf(GPT.P_GpOutFile, "tile.fillStyle = \"%s\"; tile.fillRect(0,0,32,32);\n", (*CANVAS_background) ? CANVAS_background : "white");
		    fprintf(GPT.P_GpOutFile, "tile.beginPath();\n");
		    switch(pattern%6) {
			    case 1: fprintf(GPT.P_GpOutFile, "%s %s\n", PATTERN1, PATTERN2); break;
			    case 2: fprintf(GPT.P_GpOutFile, "%s %s %s\n", PATTERN1, PATTERN2, PATTERN3); break;
			    case 4: fprintf(GPT.P_GpOutFile, "%s\n", PATTERN1); break;
			    case 5: fprintf(GPT.P_GpOutFile, "%s\n", PATTERN2); break;
			    default: break;
		    }
		    fprintf(GPT.P_GpOutFile, "tile.strokeStyle=\"%s\"; tile.lineWidth=\"2\"; tile.stroke();\n", canvas_state.color);
		    // Then load it as a fill style 
		    fprintf(GPT.P_GpOutFile, "ctx.fillStyle = ctx.createPattern(template,\"repeat\");\n");
		    strcpy(fillcolor, "pattern");
		    break;
		case FS_SOLID:
		    if(canvas_state.alpha > 0) {
			    sprintf(fillcolor, "rgba(%11.11s,%4.2f)%c", &canvas_state.color[5], 1.0 - canvas_state.alpha, '\0');
		    }
		    else if(density == 1) {
			    strcpy(fillcolor, canvas_state.color);
		    }
		    else {
			    int r = satoi(&canvas_state.color[5]);
			    int g = satoi(&canvas_state.color[9]);
			    int b = satoi(&canvas_state.color[13]);
			    r = static_cast<int>((float)r*density + 255.0*(1.0-density));
			    g = static_cast<int>((float)g*density + 255.0*(1.0-density));
			    b = static_cast<int>((float)b*density + 255.0*(1.0-density));
			    sprintf(fillcolor, " rgb(%3d,%3d,%3d)%c", r, g, b, '\0');
		    }
		    break;
		default:
		    memcpy(fillcolor, canvas_state.color, sizeof(fillcolor)); // Use current color, wherever it came from 
	}
	return fillcolor;
}

TERM_PUBLIC void CANVAS_filled_polygon(GpTermEntry_Static * pThis, int points, gpiPoint * corners)
{
	int i;
	CANVAS_finish();
	// FIXME: I do not understand why this is necessary, but without it  a dashed line followed by a filled area fails to fill.
	if(canvas_dashed) {
		fprintf(GPT.P_GpOutFile, "DT(gnuplot.solid);\n");
		canvas_line_type = LT_UNDEFINED;
	}
	if(corners->style != FS_OPAQUE && corners->style != FS_DEFAULT) {
		char * fillcolor = CANVAS_fillstyle(corners->style);
		if(strcmp(fillcolor, "pattern"))
			if(strcmp(canvas_state.previous_fill, fillcolor)) {
				fprintf(GPT.P_GpOutFile, "ctx.fillStyle = \"%s\";\n", fillcolor);
				strcpy(canvas_state.previous_fill, fillcolor);
			}
	}
	fprintf(GPT.P_GpOutFile, "bp(%d, %d);\n", corners[0].x, canvas_ymax - corners[0].y);
	for(i = 1; i < points; i++) {
		fprintf(GPT.P_GpOutFile, "L(%d, %d);\n", corners[i].x, canvas_ymax - corners[i].y);
	}
	if(corners->style != FS_OPAQUE && corners->style != FS_DEFAULT)
		fprintf(GPT.P_GpOutFile, "cfp();\n"); // Fill with separate fillStyle color 
	else
		fprintf(GPT.P_GpOutFile, "cfsp();\n"); // Fill with stroke color 
}

TERM_PUBLIC void CANVAS_fillbox(GpTermEntry_Static * pThis, int style, uint x1, uint y1, uint width, uint height)
{
	char * fillcolor = CANVAS_fillstyle(style);
	// FIXME: I do not understand why this is necessary, but without it a dashed line followed by a filled area fails to fill. 
	if(canvas_dashed) {
		fprintf(GPT.P_GpOutFile, "DT(gnuplot.solid);\n");
		canvas_line_type = LT_UNDEFINED;
	}
	/* Since filled-rectangle is a primitive operation for the canvas element */
	/* it's worth making this a special case rather than using filled_polygon */
	if(strcmp(fillcolor, "pattern"))
		if(strcmp(canvas_state.previous_fill, fillcolor)) {
			fprintf(GPT.P_GpOutFile, "ctx.fillStyle = \"%s\";\n", fillcolor);
			strcpy(canvas_state.previous_fill, fillcolor);
		}
	fprintf(GPT.P_GpOutFile, "R(%d,%d,%d,%d);\n", x1, canvas_ymax - (y1+height), width, height);
}

TERM_PUBLIC void CANVAS_layer(GpTermEntry_Static * pThis, t_termlayer layer)
{
	const char * basename = NZOR(CANVAS_name, "gp");
	switch(layer) {
		case TERM_LAYER_BEFORE_PLOT:
		    canvas_state.plotno++;
		    CANVAS_finish();
		    fprintf(GPT.P_GpOutFile, "if (typeof(gnuplot.hide_%s_plot_%d) == \"undefined\"|| !gnuplot.hide_%s_plot_%d) {\n",
				basename, canvas_state.plotno, basename, canvas_state.plotno);
		    break;
		case TERM_LAYER_AFTER_PLOT:
		    CANVAS_finish();
		    fprintf(GPT.P_GpOutFile, "} // End %s_plot_%d \n", basename, canvas_state.plotno);
		    canvas_line_type = LT_UNDEFINED;
		    canvas_dash_type = LT_UNDEFINED;
		    canvas_state.previous_linewidth = -1;
		    canvas_state.previous_color[0] = '\0';
		    canvas_state.previous_fill[0] = '\0';
		    break;

		case TERM_LAYER_BEGIN_GRID:
		    fprintf(GPT.P_GpOutFile, "if (gnuplot.grid_lines) {\nvar saveWidth = ctx.lineWidth;\nctx.lineWidth = ctx.lineWidth * 0.5;\n");
		    break;
		case TERM_LAYER_END_GRID:
		    fprintf(GPT.P_GpOutFile, "ctx.lineWidth = saveWidth;\n} // grid_lines\n");
		    break;
		case TERM_LAYER_RESET:
		case TERM_LAYER_RESET_PLOTNO:
		    canvas_state.plotno = 0;
		    break;
		default:
		    break;
	}
}

TERM_PUBLIC void CANVAS_path(GpTermEntry_Static * pThis, int p)
{
	switch(p) {
		case 1: // Close path 
		    fprintf(GPT.P_GpOutFile, "ctx.closePath();\n");
		    already_closed = TRUE;
		    break;
		case 0:
		    break;
	}
}

TERM_PUBLIC void CANVAS_hypertext(GpTermEntry_Static * pThis, int type, const char * hypertext)
{
	if(type != TERM_HYPERTEXT_TOOLTIP)
		return;
	SAlloc::F(CANVAS_hypertext_text);
	if(hypertext)
		CANVAS_hypertext_text = sstrdup(hypertext);
	else
		CANVAS_hypertext_text = NULL;
}

/* FIXME: The HTML5 canvas element did not originally provide any	*/
/* support for text output, let alone fonts.There are now text routines	*/
/* in the HTML5 draft spec, but they are not yet widely supported.	*/
/* So for now we will accept a font request, but ignore it except for	*/
/* the size.								*/
/* Eventually the text calls will look something like			*/
/*	ctx.font = "12pt Times";					*/
/*	ctx.textAlign = "center";					*/
/*	ctx.fillText( "Oh goody, text support.", x, y );		*/
/*									*/
TERM_PUBLIC int CANVAS_set_font(GpTermEntry_Static * pThis, const char * newfont)
{
	int sep;
	if(!newfont || !(*newfont))
		canvas_font_size = CANVAS_default_fsize;
	else {
		sep = strcspn(newfont, ",");
		if(newfont[sep] == ',') {
			sscanf(&(newfont[sep+1]), "%lf", &canvas_font_size);
			if(canvas_font_size <= 0)
				canvas_font_size = CANVAS_default_fsize;
		}
	}
	pThis->SetCharSize(static_cast<uint>(canvas_font_size * canvas_fontscale * 0.8 * CANVAS_OVERSAMPLE), static_cast<uint>(canvas_font_size * canvas_fontscale * CANVAS_OVERSAMPLE));
	return 1;
}
//
// Enhanced text mode support starts here 
//
static double ENHCANVAS_base = 0.0;
static double ENHCANVAS_fontsize = 0;
static bool ENHCANVAS_opened_string = FALSE;
static bool ENHCANVAS_show = TRUE;
static bool ENHCANVAS_sizeonly = FALSE;
static bool ENHCANVAS_widthflag = TRUE;
static bool ENHCANVAS_overprint = FALSE;

TERM_PUBLIC void ENHCANVAS_OPEN(GpTermEntry_Static * pThis, const char * fontname, double fontsize, double base, bool widthflag, bool showflag, int overprint)
{
	static int save_x, save_y;
	// overprint = 1 means print the base text (leave position in center)
	// overprint = 2 means print the overlying text
	// overprint = 3 means save current position
	// overprint = 4 means restore saved position
	if(overprint == 3) {
		save_x = canvas_x;
		save_y = canvas_y;
	}
	else if(overprint == 4) {
		canvas_x = save_x;
		canvas_y = save_y;
	}
	else if(!ENHCANVAS_opened_string) {
		ENHCANVAS_opened_string = TRUE;
		pThis->P_Gp->Enht.P_CurText = &pThis->P_Gp->Enht.Text[0];
		ENHCANVAS_fontsize = fontsize;
		ENHCANVAS_base = base * CANVAS_OVERSAMPLE;
		ENHCANVAS_show = showflag;
		ENHCANVAS_widthflag = widthflag;
		ENHCANVAS_overprint = overprint;
	}
}
// 
// Since we only have the one font, and the character widths are known,
// we can go to the trouble of actually counting character widths.
// As it happens, the averages width of ascii characters is 20.
// 
static int canvas_strwidth(char * s)
{
	int width = 0;
	char * end = s + strlen(s);
	while(*s) {
		if((*s & 0x80) == 0) {
			if(sstrchr("iIl|", *s)) width += 8;
			else if(sstrchr("j`',;:!.", *s)) width += 10;
			else if(sstrchr("ftr", *s)) width += 12;
			else if(sstrchr("()[]{}\\", *s)) width += 14;
			else if(sstrchr(" JTv^_\"*ykLsxz", *s)) width += 16;
			else if(sstrchr("AceFV?abdEghnopqu", *s)) width += 18;
			else if(sstrchr("M~<>%W=&@", *s)) width += 24;
			else if(sstrchr("m", *s)) width += 30;
			else width += 20;
			s++;
			continue;
		}
		else if(GPT._Encoding != S_ENC_UTF8) s++;
		else if((*s & 0xE0) == 0xC0) s += 2;
		else if((*s & 0xF0) == 0xE0) s += 3;
		else s += 4;
		width += 18; /* Assumed average width for UTF8 characters */
		if(s > end) break;
	}
	return (width);
}

TERM_PUBLIC void ENHCANVAS_FLUSH(GpTermEntry_Static * pThis)
{
	GnuPlot * p_gp = pThis->P_Gp;
	if(ENHCANVAS_opened_string) {
		ENHCANVAS_opened_string = FALSE;
		*p_gp->Enht.P_CurText = '\0';
		const double save_fontsize = canvas_font_size;
		const double w = canvas_strwidth(p_gp->Enht.Text) * CANVAS_OVERSAMPLE * ENHCANVAS_fontsize/25.0;
		int x = canvas_x;
		int y = canvas_y;
		canvas_font_size = ENHCANVAS_fontsize;
		x += sin((double)canvas_text_angle * SMathConst::PiDiv180) * ENHCANVAS_base;
		y += cos((double)canvas_text_angle * SMathConst::PiDiv180) * ENHCANVAS_base;
		if(ENHCANVAS_show && !ENHCANVAS_sizeonly)
			CANVAS_put_text(pThis, x, y, p_gp->Enht.Text);
		if(ENHCANVAS_overprint == 1) {
			canvas_x += w * cos((double)canvas_text_angle * SMathConst::PiDiv180)/2;
			canvas_y -= w * sin((double)canvas_text_angle * SMathConst::PiDiv180)/2;
		}
		else if(ENHCANVAS_widthflag) {
			canvas_x += w * cos((double)canvas_text_angle * SMathConst::PiDiv180);
			canvas_y -= w * sin((double)canvas_text_angle * SMathConst::PiDiv180);
		}
		canvas_font_size = save_fontsize;
	}
}

TERM_PUBLIC void ENHCANVAS_put_text(GpTermEntry_Static * pThis, uint x, uint y, const char * str)
{
	GnuPlot * p_gp = pThis->P_Gp;
	if(!isempty(str)) {
		const char * original_string = str;
		// Save starting font properties 
		const double fontsize = canvas_font_size;
		const char * fontname = "";
		if(p_gp->Enht.Ignore || (!strpbrk(str, "{}^_@&~") && !contains_unicode(str))) {
			CANVAS_put_text(pThis, x, y, str);
		}
		else {
			// ctx.fillText uses fillStyle rather than strokeStyle 
			if(strcmp(canvas_state.previous_fill, canvas_state.color)) {
				fprintf(GPT.P_GpOutFile, "ctx.fillStyle = \"%s\";\n", canvas_state.color);
				strcpy(canvas_state.previous_fill, canvas_state.color);
			}
			CANVAS_move(pThis, x, y);
			// Set up global variables needed by enhanced_recursion() 
			p_gp->Enht.FontScale = 1.0;
			strncpy(p_gp->Enht.EscapeFormat, "%c", sizeof(p_gp->Enht.EscapeFormat));
			ENHCANVAS_opened_string = FALSE;
			ENHCANVAS_fontsize = canvas_font_size;
			if(sstreq(canvas_justify, "Right") || sstreq(canvas_justify, "Center"))
				ENHCANVAS_sizeonly = TRUE;
			while(*(str = enhanced_recursion(GPT.P_Term, (char *)str, TRUE, fontname, fontsize, 0.0, TRUE, TRUE, 0))) {
				(pThis->enhanced_flush)(pThis);
				p_gp->EnhErrCheck(str);
				if(!*++str)
					break; /* end of string */
			}
			// We can do text justification by running the entire top level string 
			// through 2 times, with the ENHgd_sizeonly flag set the first time.   
			// After seeing where the final position is, we then offset the start  
			// point accordingly and run it again without the flag set.            
			if(sstreq(canvas_justify, "Right") || sstreq(canvas_justify, "Center")) {
				const char * justification = canvas_justify;
				int x_offset = canvas_x - x;
				int y_offset = (canvas_text_angle == 0) ? 0 : canvas_y - y;
				canvas_justify = "";
				ENHCANVAS_sizeonly = FALSE;
				if(sstreq(justification, "Right")) {
					ENHCANVAS_put_text(pThis, x - x_offset, y - y_offset, original_string);
				}
				else if(sstreq(justification, "Center")) {
					ENHCANVAS_put_text(pThis, x - x_offset/2, y - y_offset/2, original_string);
				}
				canvas_justify = justification;
			}
			// Make sure we leave with the same font properties as on entry 
			canvas_font_size = fontsize;
			ENHCANVAS_base = 0;
		}
	}
}

#ifdef WRITE_PNG_IMAGE
TERM_PUBLIC void CANVAS_image(GpTermEntry_Static * pThis, uint m, uint n, coordval * image, gpiPoint * corner, t_imagecolor color_mode)
{
	char * base_name = CANVAS_name ? CANVAS_name : "gp";
	canvas_imagefile * thisimage = NULL;
	// Write the image to a png file 
	char * image_file = static_cast<char * >(SAlloc::M(strlen(base_name)+16));
	sprintf(image_file, "%s_image_%02d.png", base_name, ++CANVAS_imageno);
	write_png_image(pThis, m, n, image, color_mode, image_file);
	// Map it onto the terminals coordinate system. 
	fprintf(GPT.P_GpOutFile, "gnuplot.ZI(%s_image_%02d, %d, %d, %d, %d, %d, %d);\n", base_name, CANVAS_imageno, m, n,
	    corner[0].x, canvas_ymax - corner[0].y, corner[1].x, canvas_ymax - corner[1].y);
	// Maintain a linked list of imageno|filename pairs 
	// so that we can load them at the end of the plot. 
	thisimage = static_cast<canvas_imagefile *>(SAlloc::M(sizeof(canvas_imagefile)));
	thisimage->imageno = CANVAS_imageno;
	thisimage->filename = image_file;
	thisimage->next = imagelist;
	imagelist = thisimage;
}
#endif

#endif /* TERM_BODY */

#ifdef TERM_TABLE
	TERM_TABLE_START(canvas_driver)
		"canvas", 
		"HTML Canvas object",
		static_cast<uint>(CANVAS_XMAX),
		static_cast<uint>(CANVAS_YMAX),
		static_cast<uint>(CANVASVCHAR),
		static_cast<uint>(CANVASHCHAR),
		static_cast<uint>(CANVASVTIC), 
		static_cast<uint>(CANVASHTIC), 
		CANVAS_options, 
		CANVAS_init, 
		CANVAS_reset,
		CANVAS_text, 
		GnuPlot::NullScale, 
		CANVAS_graphics, 
		CANVAS_move, 
		CANVAS_vector,
		CANVAS_linetype, 
		CANVAS_put_text, 
		CANVAS_text_angle,
		CANVAS_justify_text, 
		CANVAS_point, 
		GnuPlot::DoArrow,
		CANVAS_set_font,
		CANVAS_pointsize,
		TERM_CAN_MULTIPLOT|TERM_ALPHA_CHANNEL|TERM_LINEWIDTH|TERM_CAN_DASH,
		NULL, 
		NULL, 
		CANVAS_fillbox, 
		CANVAS_linewidth,
		#ifdef USE_MOUSE
		NULL, 
		NULL, 
		NULL, 
		NULL, 
		NULL,
		#endif
		CANVAS_make_palette, 
		NULL, 
		CANVAS_set_color, 
		CANVAS_filled_polygon,
		#ifdef WRITE_PNG_IMAGE
			CANVAS_image,
		#else
			NULL,
		#endif
		ENHCANVAS_OPEN, 
		ENHCANVAS_FLUSH, 
		do_enh_writec, 
		CANVAS_layer,
		CANVAS_path,                   /* path */
		1.0,                           /* Should this be CANVAS_OVERSAMPLE? */
		CANVAS_hypertext,
		NULL,                          /* boxed text */
		NULL,                          /* modify_plots */
		CANVAS_dashtype 
TERM_TABLE_END(canvas_driver)

#undef LAST_TERM
#define LAST_TERM canvas_driver

#endif /* TERM_TABLE */

#ifdef TERM_HELP
START_HELP(canvas)
"1 canvas",
"?commands set terminal canvas",
"?set terminal canvas",
"?set term canvas",
"?terminal canvas",
"?term canvas",
"=canvas terminal",
"",
" The `canvas` terminal creates a set of javascript commands that draw onto the",
" HTML5 canvas element.",
" Syntax:",
"       set terminal canvas {size <xsize>, <ysize>} {background <rgb_color>}",
"                           {font {<fontname>}{,<fontsize>}} | {fsize <fontsize>}",
"                           {{no}enhanced} {linewidth <lw>}",
"                           {rounded | butt | square}",
"                           {dashlength <dl>}",
"                           {standalone {mousing} | name '<funcname>'}",
"                           {jsdir 'URL/for/javascripts'}",
"                           {title '<some string>'}",
"",
" where <xsize> and <ysize> set the size of the plot area in pixels.",
" The default size in standalone mode is 600 by 400 pixels.",
" The default font size is 10.",
"",
" NB: Only one font is available, the ascii portion of Hershey simplex Roman",
" provided in the file canvastext.js. You can replace this with the file",
" canvasmath.js, which contains also UTF-8 encoded Hershey simplex Greek and",
" math symbols. For consistency with other terminals, it is also possible to",
" use `font \"name,size\"`. Currently the font `name` is ignored, but browser",
" support for named fonts is likely to arrive eventually.",
"",
" The default `standalone` mode creates an html page containing javascript",
" code that renders the plot using the HTML 5 canvas element.  The html page",
" links to two required javascript files 'canvastext.js' and 'gnuplot_common.js'.",
" An additional file 'gnuplot_dashedlines.js' is needed to support dashed lines.",
" By default these point to local files, on unix-like systems usually in",
" directory /usr/local/share/gnuplot/<version>/js.  See installation notes for",
" other platforms. You can change this by using the `jsdir` option to specify",
" either a different local directory or a general URL.  The latter is usually",
" appropriate if the plot is exported for viewing on remote client machines.",
"",
" All plots produced by the canvas terminal are mouseable.  The additional",
" keyword `mousing` causes the `standalone` mode to add a mouse-tracking box",
" underneath the plot. It also adds a link to a javascript file",
" 'gnuplot_mouse.js' and to a stylesheet for the mouse box 'gnuplot_mouse.css'",
" in the same local or URL directory as 'canvastext.js'.",
"",
" The `name` option creates a file containing only javascript. Both the",
" javascript function it contains and the id of the canvas element that it",
" draws onto are taken from the following string parameter.  The commands",
"       set term canvas name 'fishplot'",
"       set output 'fishplot.js'",
" will create a file containing a javascript function fishplot() that will",
" draw onto a canvas with id=fishplot.  An html page that invokes this",
" javascript function must also load the canvastext.js function as described",
" above.  A minimal html file to wrap the fishplot created above might be:",
"",
"       <html>",
"       <head>",
"           <script src=\"canvastext.js\"></script>",
"           <script src=\"gnuplot_common.js\"></script>",
"       </head>",
"       <body onload=\"fishplot();\">",
"           <script src=\"fishplot.js\"></script>",
"           <canvas id=\"fishplot\" width=600 height=400>",
"               <div id=\"err_msg\">No support for HTML 5 canvas element</div>",
"           </canvas>",
"       </body>",
"       </html>",
"",
" The individual plots drawn on this canvas will have names fishplot_plot_1,",
" fishplot_plot_2, and so on. These can be referenced by external javascript",
" routines, for example gnuplot.toggle_visibility(\"fishplot_plot_2\").",
""
END_HELP(canvas)
#endif /* TERM_HELP */
