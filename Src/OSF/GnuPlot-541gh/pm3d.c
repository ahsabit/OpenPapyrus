// GNUPLOT - pm3d.c 
// Copyright: open source as much as possible Petr Mikulik, since December 1998
// What is here: global variables and routines for the pm3d splotting mode.
//
#include <gnuplot.h>
#pragma hdrstop

// Used to initialize `set pm3d border` 
const lp_style_type default_pm3d_border(lp_style_type::defCommon); // = DEFAULT_LP_STYLE_TYPE;
//bool track_pm3d_quadrangles; // Set by plot styles that use pm3d quadrangles even in non-pm3d mode 
// 
// Global options for pm3d algorithm (to be accessed by set / show).
// 
//pm3d_struct pm3d; // Initialized via init_session->reset_command->reset_pm3d 
//lighting_model pm3d_shade;

#define QUAD_TYPE_NORMAL   0
#define QUAD_TYPE_TRIANGLE 3
#define QUAD_TYPE_4SIDES   4
#define QUAD_TYPE_LARGEPOLYGON 5

#define PM3D_USE_COLORSPEC_INSTEAD_OF_GRAY -12345
#define PM3D_USE_RGB_COLOR_INSTEAD_OF_GRAY -12346
#define PM3D_USE_BACKGROUND_INSTEAD_OF_GRAY -12347

// Internal prototypes for this module 
//static double geomean4(double, double, double, double);
//static double harmean4(double, double, double, double);
//static double median4(double, double, double, double);
//static double rms4(double, double, double, double);
// 
// Utility routines.
// 
// Geometrical mean = pow( prod(x_i > 0) x_i, 1/N )
// In order to facilitate plotting surfaces with all color coords negative,
// All 4 corners positive - return positive geometric mean
// All 4 corners negative - return negative geometric mean
// otherwise return 0
// EAM Oct 2012: This is a CHANGE
// 
static double geomean4(double x1, double x2, double x3, double x4)
{
	int neg = (x1 < 0) + (x2 < 0) + (x3 < 0) + (x4 < 0);
	double product = x1 * x2 * x3 * x4;
	if(product == 0) 
		return 0;
	else if(neg == 1 || neg == 2 || neg == 3) 
		return 0;
	else {
		product = sqrt(sqrt(fabs(product)));
		return (neg == 0) ? product : -product;
	}
}

static double harmean4(double x1, double x2, double x3, double x4)
{
	return (x1 <= 0.0 || x2 <= 0.0 || x3 <= 0.0 || x4 <= 0.0) ? fgetnan() : (4.0 / ((1.0/x1) + (1.0/x2) + (1.0/x3) + (1.0/x4)));
}
//
// Median: sort values, and then: for N odd, it is the middle value; for N even,
// it is mean of the two middle values.
//
static double median4(double x1, double x2, double x3, double x4)
{
	// sort them: x1 < x2 and x3 < x4 
	SExchangeForOrder(&x1, &x2);
	SExchangeForOrder(&x3, &x4);
	// sum middle numbers 
	double tmp  = (x1 < x3) ? x3 : x1;
	tmp += (x2 < x4) ? x2 : x4;
	return tmp * 0.5;
}
//
// The root mean square of the 4 numbers 
//
static double rms4(double x1, double x2, double x3, double x4)
{
	return 0.5*sqrt(x1*x1 + x2*x2 + x3*x3 + x4*x4);
}
//
// Now the routines which are really just those for pm3d.c
// 
// Rescale cb (color) value into the interval of grays [0,1], taking care
// of palette being positive or negative.
// Note that it is OK for logarithmic cb-axis too.
// 
double GnuPlot::Cb2Gray(double cb)
{
	const GpAxis * p_ax_cb = &AxS.__CB();
	if(cb <= p_ax_cb->Range.low)
		return (SmPltt.Positive == SMPAL_POSITIVE) ? 0 : 1;
	else if(cb >= p_ax_cb->Range.upp)
		return (SmPltt.Positive == SMPAL_POSITIVE) ? 1 : 0;
	else {
		if(p_ax_cb->IsNonLinear()) {
			p_ax_cb = p_ax_cb->linked_to_primary;
			cb = EvalLinkFunction(p_ax_cb, cb);
		}
		cb = (cb - p_ax_cb->Range.low) / p_ax_cb->GetRange();
		return (SmPltt.Positive == SMPAL_POSITIVE) ? cb : 1-cb;
	}
}
//
// Rearrange...
//
void GnuPlot::Pm3DRearrangePart(iso_curve * pSrc, const int len, struct iso_curve *** pppDest, int * invert)
{
	iso_curve * scanA;
	iso_curve * scanB;
	iso_curve ** scan_array;
	int i, scan;
	int invert_order = 0;
	/* loop over scans in one surface
	   Scans are linked from this_plot->iso_crvs in the opposite order than
	   they are in the datafile.
	   Therefore it is necessary to make vector scan_array of iso_curves.
	   Scans are sorted in scan_array according to pm3d.direction (this can
	   be PM3D_SCANS_FORWARD or PM3D_SCANS_BACKWARD).
	 */
	scan_array = *pppDest = (iso_curve **)SAlloc::M(len * sizeof(scanA));
	if(_Pm3D.pm3d.direction == PM3D_SCANS_AUTOMATIC) {
		int cnt;
		int len2 = len;
		bool exit_outer_loop = 0;
		for(scanA = pSrc; scanA && 0 == exit_outer_loop; scanA = scanA->next, len2--) {
			int from, i;
			GpVertex vA, vA2;
			if((cnt = scanA->p_count - 1) <= 0)
				continue;
			// ordering within one scan 
			for(from = 0; from<=cnt; from++) // find 1st non-undefined point 
				if(scanA->points[from].type != UNDEFINED) {
					Map3D_XYZ(scanA->points[from].Pt.GetXY(), 0.0, &vA);
					break;
				}
			for(i = cnt; i>from; i--) // find the last non-undefined point 
				if(scanA->points[i].type != UNDEFINED) {
					Map3D_XYZ(scanA->points[i].Pt.GetXY(), 0.0, &vA2);
					break;
				}
			if(i - from > cnt * 0.1)
				// it is completely arbitrary to request at least
				// 10% valid samples in this scan. (joze Jun-05-2002) 
				*invert = (vA2.z > vA.z) ? 0 : 1;
			else
				continue; // all points were undefined, so check next scan 
			// check the z ordering between scans
			// Find last scan. If this scan has all points undefined,
			// find last but one scan, an so on. 
			for(; len2 >= 3 && !exit_outer_loop; len2--) {
				for(scanB = scanA->next, i = len2 - 2; i && scanB; i--)
					scanB = scanB->next; /* skip over to last scan */
				if(scanB && scanB->p_count) {
					GpVertex vB;
					for(i = from /* we compare vA.z with vB.z */; i<scanB->p_count; i++) {
						// find 1st non-undefined point 
						if(scanB->points[i].type != UNDEFINED) {
							Map3D_XYZ(scanB->points[i].Pt.GetXY(), 0.0, &vB);
							invert_order = (vB.z > vA.z) ? 0 : 1;
							exit_outer_loop = 1;
							break;
						}
					}
				}
			}
		}
	}
	FPRINTF((stderr, "(pm3d_rearrange_part) invert       = %d\n", *invert));
	FPRINTF((stderr, "(pm3d_rearrange_part) invert_order = %d\n", invert_order));
	for(scanA = pSrc, scan = len - 1, i = 0; scan >= 0; --scan, i++) {
		if(_Pm3D.pm3d.direction == PM3D_SCANS_AUTOMATIC) {
			switch(invert_order) {
				case 1:
				    scan_array[scan] = scanA;
				    break;
				case 0:
				default:
				    scan_array[i] = scanA;
				    break;
			}
		}
		else if(_Pm3D.pm3d.direction == PM3D_SCANS_FORWARD)
			scan_array[scan] = scanA;
		else // PM3D_SCANS_BACKWARD: i counts scans 
			scan_array[i] = scanA;
		scanA = scanA->next;
	}
}
// 
// Rearrange scan array
// 
// Allocates *first_ptr (and eventually *second_ptr)
// which must be freed by the caller
// 
void GnuPlot::Pm3DRearrangeScanArray(GpSurfacePoints * pPlot, iso_curve *** pppFirstPtr, int * pFirstN, int * pFirstInvert,
	iso_curve *** pppSecondPtr, int * pSecondN, int * pSecondInvert)
{
	if(pppFirstPtr) {
		Pm3DRearrangePart(pPlot->iso_crvs, pPlot->num_iso_read, pppFirstPtr, pFirstInvert);
		*pFirstN = pPlot->num_iso_read;
	}
	if(pppSecondPtr) {
		iso_curve * icrvs = pPlot->iso_crvs;
		iso_curve * icrvs2;
		int i;
		// advance until second part 
		for(i = 0; i < pPlot->num_iso_read; i++)
			icrvs = icrvs->next;
		// count the number of scans of second part 
		for(i = 0, icrvs2 = icrvs; icrvs2; icrvs2 = icrvs2->next)
			i++;
		if(i > 0) {
			*pSecondN = i;
			Pm3DRearrangePart(icrvs, i, pppSecondPtr, pSecondInvert);
		}
		else {
			*pppSecondPtr = (iso_curve **)0;
		}
	}
}

static int compare_quadrangles(const void * v1, const void * v2)
{
	const Quadrangle * q1 = (const Quadrangle *)v1;
	const Quadrangle * q2 = (const Quadrangle *)v2;
	if(q1->z > q2->z)
		return 1;
	else if(q1->z < q2->z)
		return -1;
	else
		return 0;
}

void GnuPlot::Pm3DDepthQueueClear()
{
	ZFREE(_Pm3D.P_Quadrangles);
	_Pm3D.allocated_quadrangles = 0;
	_Pm3D.current_quadrangle = 0;
}

void GnuPlot::Pm3DDepthQueueFlush(GpTermEntry * pTerm)
{
	if(_Pm3D.pm3d.direction == PM3D_DEPTH || _Pm3D.track_pm3d_quadrangles) {
		pTerm->Layer_(TERM_LAYER_BEGIN_PM3D_FLUSH);
		if(_Pm3D.current_quadrangle > 0 && _Pm3D.P_Quadrangles) {
			Quadrangle * qp;
			Quadrangle * qe;
			gpdPoint * gpdPtr;
			GpVertex out;
			int nv, i;
			const double zbase = _Pm3D.pm3d.base_sort ? AxS.__Z().ClipToRange(0.0) : 0.0;
			for(qp = _Pm3D.P_Quadrangles, qe = _Pm3D.P_Quadrangles + _Pm3D.current_quadrangle; qp != qe; qp++) {
				double z = 0;
				double zmean = 0;
				if(qp->type == QUAD_TYPE_LARGEPOLYGON) {
					gpdPtr = &_Pm3D.P_PolygonList[qp->vertex.array_index];
					nv = static_cast<int>(gpdPtr[2].c);
				}
				else {
					gpdPtr = qp->vertex.corners;
					nv = 4;
				}
				for(i = 0; i < nv; i++, gpdPtr++) {
					// 3D boxes want to be sorted on z of the base, not the mean z 
					Map3D_XYZ(gpdPtr->x, gpdPtr->y, _Pm3D.pm3d.base_sort ? zbase : gpdPtr->z, &out);
					zmean += out.z;
					if(i == 0 || out.z > z)
						z = out.z;
				}
				qp->z = _Pm3D.pm3d.zmean_sort ? (zmean / nv) : z;
			}
			qsort(_Pm3D.P_Quadrangles, _Pm3D.current_quadrangle, sizeof(Quadrangle), compare_quadrangles);
			for(qp = _Pm3D.P_Quadrangles, qe = _Pm3D.P_Quadrangles + _Pm3D.current_quadrangle; qp != qe; qp++) {
				// set the color 
				if(qp->gray == PM3D_USE_COLORSPEC_INSTEAD_OF_GRAY)
					ApplyPm3DColor(pTerm, qp->qcolor.colorspec);
				else if(qp->gray == PM3D_USE_BACKGROUND_INSTEAD_OF_GRAY)
					pTerm->SetLineType_(LT_BACKGROUND);
				else if(qp->gray == PM3D_USE_RGB_COLOR_INSTEAD_OF_GRAY)
					SetRgbColorVar(pTerm, qp->qcolor.rgb_color);
				else if(_Pm3D.pm3d_shade.strength > 0)
					SetRgbColorVar(pTerm, (uint)qp->gray);
				else
					set_color(pTerm, qp->gray);
				if(qp->type == QUAD_TYPE_LARGEPOLYGON) {
					gpdPoint * vertices = &_Pm3D.P_PolygonList[qp->vertex.array_index];
					int nv = static_cast<int>(vertices[2].c);
					FilledPolygon(pTerm, vertices, qp->fillstyle, nv);
				}
				else {
					FilledPolygon(pTerm, qp->vertex.corners, qp->fillstyle, 4);
				}
			}
		}
		Pm3DDepthQueueClear();
		_Pm3D.FreePolygonList();
		pTerm->Layer_(TERM_LAYER_END_PM3D_FLUSH);
	}
}
// 
// Now the implementation of the pm3d (s)plotting mode
// 
void GnuPlot::Pm3DPlot(GpTermEntry * pTerm, GpSurfacePoints * pPlot, int at_which_z)
{
	int j, i, i1, ii, ii1, from, scan, up_to, up_to_minus, invert = 0;
	int go_over_pts, max_pts;
	int are_ftriangles, ftriangles_low_pt = -999, ftriangles_high_pt = -999;
	iso_curve * scanA, * scanB;
	GpCoordinate * pointsA, * pointsB;
	iso_curve ** scan_array;
	int scan_array_n;
	double avgC, gray = 0;
	double cb1, cb2, cb3, cb4;
	gpdPoint corners[4];
	int interp_i, interp_j;
	gpdPoint ** bl_point = NULL; /* used for bilinear interpolation */
	bool color_from_fillcolor = FALSE;
	udvt_entry * private_colormap = NULL;
	// should never happen 
	if(pPlot) {
		// just a shortcut 
		const int  plot_fillstyle = style_from_fill(&pPlot->fill_properties);
		const bool color_from_column = pPlot->pm3d_color_from_column;
		_Pm3D.color_from_rgbvar = FALSE;
		if(pPlot->lp_properties.pm3d_color.type == TC_RGB && pPlot->lp_properties.pm3d_color.value == -1)
			_Pm3D.color_from_rgbvar = TRUE;
		if(oneof2(pPlot->fill_properties.border_color.type, TC_RGB, TC_LINESTYLE)) {
			_Pm3D.color_from_rgbvar = TRUE;
			color_from_fillcolor = TRUE;
		}
		if(pPlot->lp_properties.P_Colormap)
			private_colormap = pPlot->lp_properties.P_Colormap;
		// Apply and save the user-requested line properties 
		TermApplyLpProperties(pTerm, &pPlot->lp_properties);
		if(at_which_z != PM3D_AT_BASE && at_which_z != PM3D_AT_TOP && at_which_z != PM3D_AT_SURFACE)
			return;
		else
			_Pm3D.pm3d_plot_at = at_which_z; /* clip_filled_polygon() will check this */
		// return if the terminal does not support filled polygons 
		if(!pTerm->filled_polygon)
			return;
		// for pm3dCompress.awk and pm3dConvertToImage.awk 
		if(_Pm3D.pm3d.direction != PM3D_DEPTH)
			pTerm->Layer_(TERM_LAYER_BEGIN_PM3D_MAP);
		switch(at_which_z) {
			case PM3D_AT_BASE: corners[0].z = corners[1].z = corners[2].z = corners[3].z = _3DBlk.base_z; break;
			case PM3D_AT_TOP:  corners[0].z = corners[1].z = corners[2].z = corners[3].z = _3DBlk.ceiling_z; break;
				// the 3rd possibility is surface, PM3D_AT_SURFACE, coded below 
		}
		scanA = pPlot->iso_crvs;
		Pm3DRearrangeScanArray(pPlot, &scan_array, &scan_array_n, &invert, (struct iso_curve ***)0, (int *)0, (int *)0);
		interp_i = _Pm3D.pm3d.interp_i;
		interp_j = _Pm3D.pm3d.interp_j;
		if(interp_i <= 0 || interp_j <= 0) {
			// Number of interpolations will be determined from desired number of points.
			// Search for number of scans and maximal number of points in a scan for points
			// which will be plotted (INRANGE). Then set interp_i,j so that number of points
			// will be a bit larger than |interp_i,j|.
			// If (interp_i,j==0) => set this number of points according to DEFAULT_OPTIMAL_NB_POINTS.
			// Ideally this should be comparable to the resolution of the output device, which
			// can hardly by done at this high level instead of the driver level.
		#define DEFAULT_OPTIMAL_NB_POINTS 200
			int max_scan_pts = 0;
			int max_scans = 0;
			int pts;
			for(scan = 0; scan < pPlot->num_iso_read - 1; scan++) {
				scanA = scan_array[scan];
				pointsA = scanA->points;
				pts = 0;
				for(j = 0; j<scanA->p_count; j++)
					if(pointsA[j].type == INRANGE) 
						pts++;
				if(pts > 0) {
					max_scan_pts = MAX(max_scan_pts, pts);
					max_scans++;
				}
			}
			if(max_scan_pts == 0 || max_scans == 0)
				IntError(NO_CARET, "all scans empty");
			if(interp_i <= 0) {
				ii = (interp_i == 0) ? DEFAULT_OPTIMAL_NB_POINTS : -interp_i;
				interp_i = (ii / max_scan_pts) + 1;
			}
			if(interp_j <= 0) {
				ii = (interp_j == 0) ? DEFAULT_OPTIMAL_NB_POINTS : -interp_j;
				interp_j = (ii / max_scans) + 1;
			}
	#if 0
			fprintf(stderr, "pm3d.interp_i=%i\t pm3d.interp_j=%i\n", pm3d.interp_i, pm3d.interp_j);
			fprintf(stderr, "INRANGE: max_scans=%i  max_scan_pts=%i\n", max_scans, max_scan_pts);
			fprintf(stderr, "setting interp_i=%i\t interp_j=%i => there will be %i and %i points\n", interp_i, interp_j, interp_i*max_scan_pts, interp_j*max_scans);
	#endif
		}
		if(_Pm3D.pm3d.direction == PM3D_DEPTH) {
			int needed_quadrangles = 0;
			for(scan = 0; scan < pPlot->num_iso_read - 1; scan++) {
				scanA = scan_array[scan];
				scanB = scan_array[scan + 1];
				are_ftriangles = _Pm3D.pm3d.ftriangles && (scanA->p_count != scanB->p_count);
				if(!are_ftriangles)
					needed_quadrangles += MIN(scanA->p_count, scanB->p_count) - 1;
				else
					needed_quadrangles += MAX(scanA->p_count, scanB->p_count) - 1;
			}
			needed_quadrangles *= (interp_i > 1) ? interp_i : 1;
			needed_quadrangles *= (interp_j > 1) ? interp_j : 1;
			if(needed_quadrangles > 0) {
				while(_Pm3D.current_quadrangle + needed_quadrangles >= _Pm3D.allocated_quadrangles) {
					FPRINTF((stderr, "allocated_quadrangles = %d current = %d needed = %d\n", _Pm3D.allocated_quadrangles, _Pm3D.current_quadrangle, needed_quadrangles));
					_Pm3D.allocated_quadrangles = needed_quadrangles + 2 * _Pm3D.allocated_quadrangles;
					_Pm3D.P_Quadrangles = (Quadrangle *)SAlloc::R(_Pm3D.P_Quadrangles, _Pm3D.allocated_quadrangles * sizeof(Quadrangle));
				}
			}
		}
		// Pm3DRearrangeScanArray(pPlot, (struct iso_curve***)0, (int *)0, &scan_array, &invert); 
	#if 0
		// debugging: print scan_array 
		for(scan = 0; scan < pPlot->num_iso_read; scan++) {
			printf("**** SCAN=%d  points=%d\n", scan, scan_array[scan]->p_count);
		}
	#endif
	#if 0
		/* debugging: this loop prints properties of all scans */
		for(scan = 0; scan < pPlot->num_iso_read; scan++) {
			GpCoordinate * points;
			scanA = scan_array[scan];
			printf("\n#IsoCurve = scan nb %d, %d points\n#x y z type(in,out,undef)\n", scan, scanA->p_count);
			for(i = 0, points = scanA->points; i < scanA->p_count; i++) {
				printf("%g %g %g %c\n", points[i].x, points[i].y, points[i].z, points[i].type == INRANGE ? 'i' : points[i].type == OUTRANGE ? 'o' : 'u');
				/* Note: INRANGE, OUTRANGE, UNDEFINED */
			}
		}
		printf("\n");
	#endif
		// 
		// if bilinear interpolation is enabled, allocate memory for the
		// interpolated points here
		//
		if(interp_i > 1 || interp_j > 1) {
			bl_point = (gpdPoint**)SAlloc::M(sizeof(gpdPoint*) * (interp_i+1));
			for(i1 = 0; i1 <= interp_i; i1++)
				bl_point[i1] = (gpdPoint*)SAlloc::M(sizeof(gpdPoint) * (interp_j+1));
		}
		// 
		// this loop does the pm3d draw of joining two curves
		// 
		// How the loop below works:
		// - scanB = scan last read; scanA = the previous one
		// - link the scan from A to B, then move B to A, then read B, then draw
		// 
		for(scan = 0; scan < pPlot->num_iso_read - 1; scan++) {
			scanA = scan_array[scan];
			scanB = scan_array[scan + 1];
			FPRINTF((stderr, "\n#IsoCurveA = scan nb %d has %d points   ScanB has %d points\n", scan, scanA->p_count, scanB->p_count));
			pointsA = scanA->points;
			pointsB = scanB->points;
			// if the number of points in both scans is not the same, then the
			// starting index (offset) of scan B according to the flushing setting
			// has to be determined
			from = 0; // default is pm3d.flush==PM3D_FLUSH_BEGIN 
			if(_Pm3D.pm3d.flush == PM3D_FLUSH_END)
				from = abs(scanA->p_count - scanB->p_count);
			else if(_Pm3D.pm3d.flush == PM3D_FLUSH_CENTER)
				from = abs(scanA->p_count - scanB->p_count) / 2;
			// find the minimal number of points in both scans 
			up_to = MIN(scanA->p_count, scanB->p_count) - 1;
			up_to_minus = up_to - 1; // calculate only once 
			are_ftriangles = _Pm3D.pm3d.ftriangles && (scanA->p_count != scanB->p_count);
			if(!are_ftriangles)
				go_over_pts = up_to;
			else {
				max_pts = MAX(scanA->p_count, scanB->p_count);
				go_over_pts = max_pts - 1;
				// the j-subrange of quadrangles; in the remainder of the interval
				// [0..up_to] the flushing triangles are to be drawn 
				ftriangles_low_pt = from;
				ftriangles_high_pt = from + up_to_minus;
			}
			// Go over
			//   - the minimal number of points from both scans, if only quadrangles.
			//   - the maximal number of points from both scans if flush triangles
			//   (the missing points in the scan of lower nb of points will be
			//   duplicated from the begin/end points).
			// 
			// Notice: if it would be once necessary to go over points in `backward'
			// direction, then the loop body below would require to replace the data
			// point indices `i' by `up_to-i' and `i+1' by `up_to-i-1'.
			// 
			for(j = 0; j < go_over_pts; j++) {
				// Now i be the index of the scan with smaller number of points,
				// ii of the scan with larger number of points. 
				if(are_ftriangles && (j < ftriangles_low_pt || j > ftriangles_high_pt)) {
					i = (j <= ftriangles_low_pt) ? 0 : ftriangles_high_pt-from+1;
					ii = j;
					i1 = i;
					ii1 = ii + 1;
				}
				else {
					int jj = are_ftriangles ? j - from : j;
					i = jj;
					if(PM3D_SCANS_AUTOMATIC == _Pm3D.pm3d.direction && invert)
						i = up_to_minus - jj;
					ii = i + from;
					i1 = i + 1;
					ii1 = ii + 1;
				}
				// From here, i is index to scan A, ii to scan B 
				if(scanA->p_count > scanB->p_count) {
					int itmp = i;
					i = ii;
					ii = itmp;
					itmp = i1;
					i1 = ii1;
					ii1 = itmp;
				}
				FPRINTF((stderr, "j=%i:  i=%i i1=%i  [%i]   ii=%i ii1=%i  [%i]\n", j, i, i1, scanA->p_count, ii, ii1, scanB->p_count));
				// choose the clipping method 
				if(_Pm3D.pm3d.clip == PM3D_CLIP_4IN) {
					// (1) all 4 points of the quadrangle must be in x and y range 
					if(!(pointsA[i].type == INRANGE && pointsA[i1].type == INRANGE && pointsB[ii].type == INRANGE && pointsB[ii1].type == INRANGE))
						continue;
				}
				else {
					// (2) all 4 points of the quadrangle must be defined 
					if(pointsA[i].type == UNDEFINED || pointsA[i1].type == UNDEFINED || pointsB[ii].type == UNDEFINED || pointsB[ii1].type == UNDEFINED)
						continue;
					// and at least 1 point of the quadrangle must be in x and y range 
					if(pointsA[i].type == OUTRANGE && pointsA[i1].type == OUTRANGE && pointsB[ii].type == OUTRANGE && pointsB[ii1].type == OUTRANGE)
						continue;
				}
				if((interp_i <= 1 && interp_j <= 1) || _Pm3D.pm3d.direction == PM3D_DEPTH) {
					{
						// Get the gray as the average of the corner z- or gray-positions
						// (note: log scale is already included). The average is calculated here
						// if there is no interpolation (including the "pm3d depthorder" option),
						// otherwise it is done for each interpolated quadrangle later.
						if(color_from_column) {
							// color is set in plot3d.c:get_3ddata() 
							cb1 = pointsA[i].CRD_COLOR;
							cb2 = pointsA[i1].CRD_COLOR;
							cb3 = pointsB[ii].CRD_COLOR;
							cb4 = pointsB[ii1].CRD_COLOR;
						}
						else if(color_from_fillcolor) {
							// color is set by "fc <rgbvalue>" 
							cb1 = cb2 = cb3 = cb4 = pPlot->fill_properties.border_color.lt;
							// EXPERIMENTAL
							// pm3d fc linestyle N generates
							// top/bottom color difference as with hidden3d
							if(pPlot->fill_properties.border_color.type == TC_LINESTYLE) {
								lp_style_type style;
								int side = Pm3DSide(&pointsA[i], &pointsA[i1], &pointsB[ii]);
								LpUseProperties(pTerm, &style, static_cast<int>(side < 0 ? (cb1 + 1) : cb1));
								cb1 = cb2 = cb3 = cb4 = style.pm3d_color.lt;
							}
						}
						else {
							cb1 = pointsA[i].Pt.z;
							cb2 = pointsA[i1].Pt.z;
							cb3 = pointsB[ii].Pt.z;
							cb4 = pointsB[ii1].Pt.z;
						}
						// Fancy averages of RGB color make no sense 
						if(_Pm3D.color_from_rgbvar) {
							uint r, g, b, a;
							uint u1 = static_cast<uint>(cb1);
							uint u2 = static_cast<uint>(cb2);
							uint u3 = static_cast<uint>(cb3);
							uint u4 = static_cast<uint>(cb4);
							switch(_Pm3D.pm3d.which_corner_color) {
								default:
									r = (u1&0xff0000) + (u2&0xff0000) + (u3&0xff0000) + (u4&0xff0000);
									g = (u1&0xff00) + (u2&0xff00) + (u3&0xff00) + (u4&0xff00);
									b = (u1&0xff) + (u2&0xff) + (u3&0xff) + (u4&0xff);
									avgC = ((r>>2)&0xff0000) + ((g>>2)&0xff00) + ((b>>2)&0xff);
									a = ((u1>>24)&0xff) + ((u2>>24)&0xff) + ((u3>>24)&0xff) + ((u4>>24)&0xff);
									avgC += (a<<22)&0xff000000;
									break;
								case PM3D_WHICHCORNER_C1: avgC = cb1; break;
								case PM3D_WHICHCORNER_C2: avgC = cb2; break;
								case PM3D_WHICHCORNER_C3: avgC = cb3; break;
								case PM3D_WHICHCORNER_C4: avgC = cb4; break;
							}
							// But many different averages are possible for gray values 
						}
						else {
							switch(_Pm3D.pm3d.which_corner_color) {
								default:
								case PM3D_WHICHCORNER_MEAN: avgC = (cb1 + cb2 + cb3 + cb4) * 0.25; break;
								case PM3D_WHICHCORNER_GEOMEAN: avgC = geomean4(cb1, cb2, cb3, cb4); break;
								case PM3D_WHICHCORNER_HARMEAN: avgC = harmean4(cb1, cb2, cb3, cb4); break;
								case PM3D_WHICHCORNER_MEDIAN: avgC = median4(cb1, cb2, cb3, cb4); break;
								case PM3D_WHICHCORNER_MIN: avgC = smin4(cb1, cb2, cb3, cb4); break;
								case PM3D_WHICHCORNER_MAX: avgC = smax4(cb1, cb2, cb3, cb4); break;
								case PM3D_WHICHCORNER_RMS: avgC = rms4(cb1, cb2, cb3, cb4); break;
								case PM3D_WHICHCORNER_C1: avgC = cb1; break;
								case PM3D_WHICHCORNER_C2: avgC = cb2; break;
								case PM3D_WHICHCORNER_C3: avgC = cb3; break;
								case PM3D_WHICHCORNER_C4: avgC = cb4; break;
							}
						}
						// The value is out of range, but we didn't figure it out until now 
						if(isnan(avgC))
							continue;
						// Option to not drawn quadrangles with cb out of range 
						if(_Pm3D.pm3d.no_clipcb && (avgC > AxS.__CB().Range.upp || avgC < AxS.__CB().Range.low))
							continue;
						if(_Pm3D.color_from_rgbvar) /* we were given an RGB color */
							gray = avgC;
						else if(private_colormap)
							gray = Map2Gray(avgC, private_colormap);
						else // transform z value to gray, i.e. to interval [0,1] 
							gray = Cb2Gray(avgC);
						// apply lighting model 
						if(_Pm3D.pm3d_shade.strength > 0) {
							if(at_which_z == PM3D_AT_SURFACE) {
								bool gray_is_rgb = _Pm3D.color_from_rgbvar;
								if(private_colormap) {
									gray = rgb_from_colormap(gray, private_colormap);
									gray_is_rgb = TRUE;
								}
								gray = ApplyLightingModel(&pointsA[i], &pointsA[i1], &pointsB[ii], &pointsB[ii1], gray, gray_is_rgb);
							}
							// Don't apply lighting model to TOP/BOTTOM projections  
							// but convert from floating point 0<gray<1 to RGB color 
							// since that is what would have been returned from the  
							// lighting code.					    
							else if(private_colormap) {
								gray = rgb_from_colormap(gray, private_colormap);
							}
							else if(!_Pm3D.color_from_rgbvar) {
								rgb255_color temp;
								Rgb255MaxColorsFromGray(gray, &temp);
								gray = (long)((temp.r << 16) + (temp.g << 8) + (temp.b));
							}
						}
						// set the color 
						if(_Pm3D.pm3d.direction != PM3D_DEPTH) {
							if(_Pm3D.color_from_rgbvar || _Pm3D.pm3d_shade.strength > 0)
								SetRgbColorVar(pTerm, (uint)gray);
							else if(private_colormap)
								SetRgbColorVar(pTerm, rgb_from_colormap(gray, private_colormap));
							else
								set_color(pTerm, gray);
						}
					}
				}
				corners[0] = pointsA[i].Pt.GetXY();
				corners[1] = pointsB[ii].Pt.GetXY();
				corners[2] = pointsB[ii1].Pt.GetXY();
				corners[3] = pointsA[i1].Pt.GetXY();
				if(interp_i > 1 || interp_j > 1 || at_which_z == PM3D_AT_SURFACE) {
					corners[0].z = pointsA[i].Pt.z;
					corners[1].z = pointsB[ii].Pt.z;
					corners[2].z = pointsB[ii1].Pt.z;
					corners[3].z = pointsA[i1].Pt.z;
					if(color_from_column) {
						corners[0].c = pointsA[i].CRD_COLOR;
						corners[1].c = pointsB[ii].CRD_COLOR;
						corners[2].c = pointsB[ii1].CRD_COLOR;
						corners[3].c = pointsA[i1].CRD_COLOR;
					}
				}
				if(interp_i > 1 || interp_j > 1) {
					// Interpolation is enabled.
					// interp_i is the # of points along scan lines
					// interp_j is the # of points between scan lines
					// Algorithm is to first sample i points along the scan lines
					// defined by corners[3],corners[0] and corners[2],corners[1]. 
					int j1;
					for(i1 = 0; i1 <= interp_i; i1++) {
						bl_point[i1][0].x = ((corners[3].x - corners[0].x) / interp_i) * i1 + corners[0].x;
						bl_point[i1][interp_j].x = ((corners[2].x - corners[1].x) / interp_i) * i1 + corners[1].x;
						bl_point[i1][0].y = ((corners[3].y - corners[0].y) / interp_i) * i1 + corners[0].y;
						bl_point[i1][interp_j].y = ((corners[2].y - corners[1].y) / interp_i) * i1 + corners[1].y;
						bl_point[i1][0].z = ((corners[3].z - corners[0].z) / interp_i) * i1 + corners[0].z;
						bl_point[i1][interp_j].z = ((corners[2].z - corners[1].z) / interp_i) * i1 + corners[1].z;
						if(color_from_column) {
							bl_point[i1][0].c = ((corners[3].c - corners[0].c) / interp_i) * i1 + corners[0].c;
							bl_point[i1][interp_j].c = ((corners[2].c - corners[1].c) / interp_i) * i1 + corners[1].c;
						}
						/* Next we sample j points between each of the new points
						 * created in the previous step (this samples between
						 * scan lines) in the same manner. */
						for(j1 = 1; j1 < interp_j; j1++) {
							bl_point[i1][j1].x = ((bl_point[i1][interp_j].x - bl_point[i1][0].x) / interp_j) * j1 + bl_point[i1][0].x;
							bl_point[i1][j1].y = ((bl_point[i1][interp_j].y - bl_point[i1][0].y) / interp_j) * j1 + bl_point[i1][0].y;
							bl_point[i1][j1].z = ((bl_point[i1][interp_j].z - bl_point[i1][0].z) / interp_j) * j1 + bl_point[i1][0].z;
							if(color_from_column)
								bl_point[i1][j1].c = ((bl_point[i1][interp_j].c - bl_point[i1][0].c) / interp_j) * j1 + bl_point[i1][0].c;
						}
					}
					// Once all points are created, move them into an appropriate
					// structure and call set_color on each to retrieve the
					// correct color mapping for this new sub-sampled quadrangle. 
					for(i1 = 0; i1 < interp_i; i1++) {
						for(j1 = 0; j1 < interp_j; j1++) {
							corners[0].x = bl_point[i1][j1].x;
							corners[0].y = bl_point[i1][j1].y;
							corners[0].z = bl_point[i1][j1].z;
							corners[1].x = bl_point[i1+1][j1].x;
							corners[1].y = bl_point[i1+1][j1].y;
							corners[1].z = bl_point[i1+1][j1].z;
							corners[2].x = bl_point[i1+1][j1+1].x;
							corners[2].y = bl_point[i1+1][j1+1].y;
							corners[2].z = bl_point[i1+1][j1+1].z;
							corners[3].x = bl_point[i1][j1+1].x;
							corners[3].y = bl_point[i1][j1+1].y;
							corners[3].z = bl_point[i1][j1+1].z;
							if(color_from_column) {
								corners[0].c = bl_point[i1][j1].c;
								corners[1].c = bl_point[i1+1][j1].c;
								corners[2].c = bl_point[i1+1][j1+1].c;
								corners[3].c = bl_point[i1][j1+1].c;
							}
							FPRINTF((stderr, "(%g,%g),(%g,%g),(%g,%g),(%g,%g)\n", corners[0].x, corners[0].y, corners[1].x, corners[1].y, corners[2].x, corners[2].y, corners[3].x, corners[3].y));
							/* If the colors are given separately, we already loaded them above */
							if(color_from_column) {
								cb1 = corners[0].c;
								cb2 = corners[1].c;
								cb3 = corners[2].c;
								cb4 = corners[3].c;
							}
							else {
								cb1 = corners[0].z;
								cb2 = corners[1].z;
								cb3 = corners[2].z;
								cb4 = corners[3].z;
							}
							switch(_Pm3D.pm3d.which_corner_color) {
								default:
								case PM3D_WHICHCORNER_MEAN: avgC = (cb1 + cb2 + cb3 + cb4) * 0.25; break;
								case PM3D_WHICHCORNER_GEOMEAN: avgC = geomean4(cb1, cb2, cb3, cb4); break;
								case PM3D_WHICHCORNER_HARMEAN: avgC = harmean4(cb1, cb2, cb3, cb4); break;
								case PM3D_WHICHCORNER_MEDIAN: avgC = median4(cb1, cb2, cb3, cb4); break;
								case PM3D_WHICHCORNER_MIN: avgC = smin4(cb1, cb2, cb3, cb4); break;
								case PM3D_WHICHCORNER_MAX: avgC = smax4(cb1, cb2, cb3, cb4); break;
								case PM3D_WHICHCORNER_RMS: avgC = rms4(cb1, cb2, cb3, cb4); break;
								case PM3D_WHICHCORNER_C1: avgC = cb1; break;
								case PM3D_WHICHCORNER_C2: avgC = cb2; break;
								case PM3D_WHICHCORNER_C3: avgC = cb3; break;
								case PM3D_WHICHCORNER_C4: avgC = cb4; break;
							}
							if(color_from_fillcolor) {
								// color is set by "fc <rgbval>" 
								gray = pPlot->fill_properties.border_color.lt;
								// EXPERIMENTAL
								// pm3d fc linestyle N generates
								// top/bottom color difference as with hidden3d
								if(pPlot->fill_properties.border_color.type == TC_LINESTYLE) {
									lp_style_type style;
									int side = Pm3DSide(&pointsA[i], &pointsB[ii], &pointsB[ii1]);
									LpUseProperties(pTerm, &style, static_cast<int>(side < 0 ? (gray + 1) : gray));
									gray = style.pm3d_color.lt;
								}
							}
							else if(_Pm3D.color_from_rgbvar) {
								// we were given an explicit color 
								gray = avgC;
							}
							else {
								// clip if out of range 
								if(_Pm3D.pm3d.no_clipcb && (avgC < AxS.__CB().Range.low || avgC > AxS.__CB().Range.upp))
									continue;
								// transform z value to gray, i.e. to interval [0,1] 
								gray = Cb2Gray(avgC);
							}
							// apply lighting model 
							if(_Pm3D.pm3d_shade.strength > 0) {
								// FIXME: coordinate->quadrangle->coordinate seems crazy 
								GpCoordinate corcorners[4];
								for(uint i = 0; i < 4; i++) {
									corcorners[i].Pt = corners[i];
								}
								if(private_colormap) {
									gray = rgb_from_colormap(gray, private_colormap);
									if(at_which_z == PM3D_AT_SURFACE)
										gray = ApplyLightingModel(&corcorners[0], &corcorners[1], &corcorners[2], &corcorners[3], gray, TRUE);
								}
								else if(at_which_z == PM3D_AT_SURFACE) {
									gray = ApplyLightingModel(&corcorners[0], &corcorners[1], &corcorners[2], &corcorners[3], gray, _Pm3D.color_from_rgbvar);
									// Don't apply lighting model to TOP/BOTTOM projections
									// but convert from floating point 0<gray<1 to RGB color
									// since that is what would have been returned from the
									// lighting code.					
								}
								else if(!_Pm3D.color_from_rgbvar) {
									rgb255_color temp;
									Rgb255MaxColorsFromGray(gray, &temp);
									gray = (long)((temp.r << 16) + (temp.g << 8) + (temp.b));
								}
							}
							if(_Pm3D.pm3d.direction == PM3D_DEPTH) {
								// copy quadrangle 
								Quadrangle * qp = _Pm3D.P_Quadrangles + _Pm3D.current_quadrangle;
								memcpy(qp->vertex.corners, corners, 4 * sizeof(gpdPoint));
								if(_Pm3D.color_from_rgbvar || _Pm3D.pm3d_shade.strength > 0) {
									qp->gray = PM3D_USE_RGB_COLOR_INSTEAD_OF_GRAY;
									qp->qcolor.rgb_color = (uint)gray;
								}
								else if(private_colormap) {
									qp->gray = PM3D_USE_RGB_COLOR_INSTEAD_OF_GRAY;
									qp->qcolor.rgb_color = rgb_from_colormap(gray, private_colormap);
								}
								else {
									qp->gray = gray;
									qp->qcolor.colorspec = &pPlot->lp_properties.pm3d_color;
								}
								qp->fillstyle = plot_fillstyle;
								qp->type = QUAD_TYPE_NORMAL;
								_Pm3D.current_quadrangle++;
							}
							else {
								if(_Pm3D.pm3d_shade.strength > 0 || _Pm3D.color_from_rgbvar)
									SetRgbColorVar(pTerm, (uint)gray);
								else if(private_colormap)
									SetRgbColorVar(pTerm, rgb_from_colormap(gray, private_colormap));
								else
									set_color(pTerm, gray);
								if(at_which_z == PM3D_AT_BASE)
									corners[0].z = corners[1].z = corners[2].z = corners[3].z = _3DBlk.base_z;
								else if(at_which_z == PM3D_AT_TOP)
									corners[0].z = corners[1].z = corners[2].z = corners[3].z = _3DBlk.ceiling_z;
								FilledPolygon(pTerm, corners, plot_fillstyle, 4);
							}
						}
					}
				}
				else { // thus (interp_i == 1 && interp_j == 1) 
					if(_Pm3D.pm3d.direction != PM3D_DEPTH)
						FilledPolygon(pTerm, corners, plot_fillstyle, 4);
					else {
						// copy quadrangle 
						Quadrangle * qp = _Pm3D.P_Quadrangles + _Pm3D.current_quadrangle;
						memcpy(qp->vertex.corners, corners, 4 * sizeof(gpdPoint));
						if(_Pm3D.color_from_rgbvar || _Pm3D.pm3d_shade.strength > 0) {
							qp->gray = PM3D_USE_RGB_COLOR_INSTEAD_OF_GRAY;
							qp->qcolor.rgb_color = (uint)gray;
						}
						else if(private_colormap) {
							qp->gray = PM3D_USE_RGB_COLOR_INSTEAD_OF_GRAY;
							qp->qcolor.rgb_color = rgb_from_colormap(gray, private_colormap);
						}
						else {
							qp->gray = gray;
							qp->qcolor.colorspec = &pPlot->lp_properties.pm3d_color;
						}
						qp->fillstyle = plot_fillstyle;
						qp->type = QUAD_TYPE_NORMAL;
						_Pm3D.current_quadrangle++;
					}
				}
			}
		}
		if(bl_point) {
			for(i1 = 0; i1 <= interp_i; i1++)
				SAlloc::F(bl_point[i1]);
			SAlloc::F(bl_point);
		}
		// free memory allocated by scan_array 
		SAlloc::F(scan_array);
		// reset local flags 
		_Pm3D.pm3d_plot_at = 0;
		// for pm3dCompress.awk and pm3dConvertToImage.awk 
		if(_Pm3D.pm3d.direction != PM3D_DEPTH)
			pTerm->Layer_(TERM_LAYER_END_PM3D_MAP);
	}
}
//
// unset pm3d for the reset command
//
void GnuPlot::Pm3DReset()
{
	strcpy(_Pm3D.pm3d.where, "s");
	_Pm3D.pm3d_plot_at = 0;
	_Pm3D.pm3d.flush = PM3D_FLUSH_BEGIN;
	_Pm3D.pm3d.ftriangles = 0;
	_Pm3D.pm3d.clip = PM3D_CLIP_Z; /* prior to Dec 2019 default was PM3D_CLIP_4IN */
	_Pm3D.pm3d.no_clipcb = FALSE;
	_Pm3D.pm3d.direction = PM3D_SCANS_AUTOMATIC;
	_Pm3D.pm3d.base_sort = FALSE;
	_Pm3D.pm3d.zmean_sort = TRUE; /* DEBUG: prior to Oct 2019 default sort used zmax */
	_Pm3D.pm3d.implicit = PM3D_EXPLICIT;
	_Pm3D.pm3d.which_corner_color = PM3D_WHICHCORNER_MEAN;
	_Pm3D.pm3d.interp_i = 1;
	_Pm3D.pm3d.interp_j = 1;
	_Pm3D.pm3d.border = default_pm3d_border;
	_Pm3D.pm3d.border.l_type = LT_NODRAW;
	_Pm3D.pm3d_shade.strength = 0.0;
	_Pm3D.pm3d_shade.spec = 0.0;
	_Pm3D.pm3d_shade.fixed = TRUE;
}
// 
// Draw (one) PM3D color surface.
// 
void GnuPlot::Pm3DDrawOne(GpTermEntry * pTerm, GpSurfacePoints * pPlot)
{
	const char * p_where = pPlot->pm3d_where[0] ? pPlot->pm3d_where : _Pm3D.pm3d.where;
	// Draw either at 'where' option of the given surface or at pm3d.where global option. 
	if(p_where[0]) {
		// Initialize lighting model 
		if(_Pm3D.pm3d_shade.strength > 0)
			_Pm3D.InitLightingModel();
		for(int i = 0; p_where[i]; i++) {
			Pm3DPlot(pTerm, pPlot, p_where[i]);
		}
	}
}
// 
// Add one pm3d quadrangle to the mix.
// Called by zerrorfill() and by plot3d_boxes().
// Also called by vplot_isosurface().
// 
void GnuPlot::Pm3DAddQuadrangle(GpTermEntry * pTerm, GpSurfacePoints * pPlot, gpdPoint corners[4])
{
	Pm3DAddPolygon(pTerm, pPlot, corners, 4);
}
// 
// The general case.
// (plot == NULL) if we were called from do_polygon().
// 
void GnuPlot::Pm3DAddPolygon(GpTermEntry * pTerm, GpSurfacePoints * pPlot, gpdPoint corners[4], int vertices)
{
	// FIXME: I have no idea how to estimate the number of facets for an isosurface 
	if(!pPlot || (pPlot->plot_style == ISOSURFACE)) {
		if(_Pm3D.allocated_quadrangles < _Pm3D.current_quadrangle + 100) {
			_Pm3D.allocated_quadrangles += 1000;
			_Pm3D.P_Quadrangles = (Quadrangle *)SAlloc::R(_Pm3D.P_Quadrangles, _Pm3D.allocated_quadrangles * sizeof(Quadrangle));
		}
	}
	else if(_Pm3D.allocated_quadrangles < _Pm3D.current_quadrangle + pPlot->iso_crvs->p_count) {
		_Pm3D.allocated_quadrangles += 2 * pPlot->iso_crvs->p_count;
		_Pm3D.P_Quadrangles = (Quadrangle *)SAlloc::R(_Pm3D.P_Quadrangles, _Pm3D.allocated_quadrangles * sizeof(Quadrangle));
	}
	{
		Quadrangle * q = _Pm3D.P_Quadrangles + _Pm3D.current_quadrangle++;
		memcpy(q->vertex.corners, corners, 4*sizeof(gpdPoint));
		q->fillstyle = pPlot ? style_from_fill(&pPlot->fill_properties) : 0;
		// For triangles and normal quadrangles, the vertices are stored in
		// q->vertex.corners.  For larger polygons we store them in external array
		// polygonlist and keep only an index into the array q->vertex.array.
		q->type = QUAD_TYPE_NORMAL;
		if(corners[3].x == corners[2].x && corners[3].y == corners[2].y && corners[3].z == corners[2].z)
			q->type = QUAD_TYPE_TRIANGLE;
		if(vertices > 4) {
			gpdPoint * save_corners = _Pm3D.GetPolygon(vertices);
			q->vertex.array_index = _Pm3D.current_polygon;
			q->type = QUAD_TYPE_LARGEPOLYGON;
			memcpy(save_corners, corners, vertices * sizeof(gpdPoint));
			save_corners[2].c = vertices;
		}
		if(!pPlot) {
			// This quadrangle came from "set object polygon" rather than "splot with pm3d" 
			if(corners[0].c == LT_BACKGROUND) {
				q->gray = PM3D_USE_BACKGROUND_INSTEAD_OF_GRAY;
			}
			else {
				q->qcolor.rgb_color = static_cast<uint>(corners[0].c);
				q->gray = PM3D_USE_RGB_COLOR_INSTEAD_OF_GRAY;
			}
			q->fillstyle = static_cast<short>(corners[1].c);
		}
		else if(pPlot->pm3d_color_from_column && !(pPlot->plot_style == POLYGONS)) {
			// FIXME: color_from_rgbvar need only be set once per plot 
			// This is the usual path for 'splot with boxes' 
			_Pm3D.color_from_rgbvar = TRUE;
			if(_Pm3D.pm3d_shade.strength > 0) {
				q->gray = pPlot->lp_properties.pm3d_color.lt;
				IlluminateOneQuadrangle(q);
			}
			else {
				q->qcolor.rgb_color = pPlot->lp_properties.pm3d_color.lt;
				q->gray = PM3D_USE_RGB_COLOR_INSTEAD_OF_GRAY;
			}
		}
		else if(pPlot->lp_properties.pm3d_color.type == TC_Z) {
			// This is a special case for 'splot with boxes lc palette z' 
			q->gray = Cb2Gray(corners[1].z);
			_Pm3D.color_from_rgbvar = FALSE;
			if(_Pm3D.pm3d_shade.strength > 0)
				IlluminateOneQuadrangle(q);
		}
		else if(oneof2(pPlot->plot_style, ISOSURFACE, POLYGONS)) {
			int rgb_color = static_cast<int>(corners[0].c);
			q->gray = (corners[0].c == LT_BACKGROUND) ? PM3D_USE_BACKGROUND_INSTEAD_OF_GRAY : PM3D_USE_RGB_COLOR_INSTEAD_OF_GRAY;
			if(pPlot->plot_style == ISOSURFACE && _VG.IsoSurfaceOptions.inside_offset > 0) {
				lp_style_type style;
				GpCoordinate v[3];
				int i;
				for(i = 0; i < 3; i++) {
					v[i].Pt = corners[i];
				}
				i = pPlot->hidden3d_top_linetype + 1;
				if(Pm3DSide(&v[0], &v[1], &v[2]) < 0)
					i += _VG.IsoSurfaceOptions.inside_offset;
				LpUseProperties(pTerm, &style, i);
				rgb_color = style.pm3d_color.lt;
			}
			q->qcolor.rgb_color = rgb_color;
			if(_Pm3D.pm3d_shade.strength > 0) {
				q->gray = rgb_color;
				_Pm3D.color_from_rgbvar = TRUE;
				IlluminateOneQuadrangle(q);
			}
		}
		else {
			// This is the usual [only?] path for 'splot with zerror' 
			q->qcolor.colorspec = &pPlot->fill_properties.border_color;
			q->gray = PM3D_USE_COLORSPEC_INSTEAD_OF_GRAY;
		}
	}
}
//
// Display an error message for the routine get_pm3d_at_option() below.
//
void GnuPlot::Pm3DOptionAtError()
{
	IntErrorCurToken("parameter to `pm3d at` requires combination of up to 6 characters b,s,t\n\t(drawing at bottom, surface, top)");
}
// 
// Read the option for 'pm3d at' command.
// Used by 'set pm3d at ...' or by 'splot ... with pm3d at ...'.
// If no option given, then returns empty string, otherwise copied there.
// The string is unchanged on error, and 1 is returned.
// On success, 0 is returned.
// 
int GnuPlot::GetPm3DAtOption(char * pm3d_where)
{
	if(Pgm.EndOfCommand() || Pgm.GetCurTokenLength() >= sizeof(_Pm3D.pm3d.where)) {
		Pm3DOptionAtError();
		return 1;
	}
	else {
		memcpy(pm3d_where, Pgm.P_InputLine + Pgm.GetCurTokenStartIndex(), Pgm.GetCurTokenLength());
		pm3d_where[Pgm.GetCurTokenLength()] = 0;
		// verify the parameter 
		for(const char * c = pm3d_where; *c; c++) {
			if(!oneof3(*c, PM3D_AT_BASE, PM3D_AT_TOP, PM3D_AT_SURFACE)) {
				Pm3DOptionAtError();
				return 1;
			}
		}
		Pgm.Shift();
		return 0;
	}
}
// 
// Set flag plot_has_palette to TRUE if there is any element on the graph
// which requires palette of continuous colors.
// 
void GnuPlot::SetPlotWithPalette(int plotNum, int plotMode)
{
	GpSurfacePoints * this_3dplot = _Plt.first_3dplot;
	curve_points * this_2dplot = _Plt.P_FirstPlot;
	int surface = 0;
	text_label * this_label = Gg.P_FirstLabel;
	GpObject * this_object;
	_Pm3D.plot_has_palette = true;
	// diagnose palette gradient type 
	if(SmPltt.GradientType == SMPAL_GRADIENT_TYPE_NONE) {
		//check_palette_gradient_type();
		SmPltt.CheckGradientType();
	}
	// Is pm3d switched on globally? 
	if(_Pm3D.pm3d.implicit == PM3D_IMPLICIT)
		return;
	// Check 2D plots 
	if(plotMode == MODE_PLOT) {
		while(this_2dplot) {
			if(this_2dplot->plot_style == IMAGE)
				return;
			if(oneof3(this_2dplot->lp_properties.pm3d_color.type, TC_CB, TC_FRAC, TC_Z))
				return;
			if(this_2dplot->labels && oneof3(this_2dplot->labels->textcolor.type, TC_CB, TC_FRAC, TC_Z))
				return;
			this_2dplot = this_2dplot->next;
		}
	}
	// Check 3D plots 
	if(plotMode == MODE_SPLOT) {
		// Any surface 'with pm3d', 'with image' or 'with line|dot palette'? 
		while(surface++ < plotNum) {
			int type;
			if(this_3dplot->plot_style == PM3DSURFACE)
				return;
			if(this_3dplot->plot_style == IMAGE)
				return;
			type = this_3dplot->lp_properties.pm3d_color.type;
			if(oneof3(type, TC_LT, TC_LINESTYLE, TC_RGB))
				; // don't return yet 
			else
				// TC_DEFAULT: splot x with line|lp|dot palette 
				return;
			if(this_3dplot->labels && this_3dplot->labels->textcolor.type >= TC_CB)
				return;
			this_3dplot = this_3dplot->next_sp;
		}
	}
#define TC_USES_PALETTE(tctype) (tctype==TC_Z) || (tctype==TC_CB) || (tctype==TC_FRAC)
	// Any label with 'textcolor palette'? 
	for(; this_label; this_label = this_label->next) {
		if(TC_USES_PALETTE(this_label->textcolor.type))
			return;
	}
	// Any of title, xlabel, ylabel, zlabel, ... with 'textcolor palette'? 
	if(TC_USES_PALETTE(Gg.LblTitle.textcolor.type)) return;
	if(TC_USES_PALETTE(AxS[FIRST_X_AXIS].label.textcolor.type)) return;
	if(TC_USES_PALETTE(AxS[FIRST_Y_AXIS].label.textcolor.type)) return;
	if(TC_USES_PALETTE(AxS[SECOND_X_AXIS].label.textcolor.type)) return;
	if(TC_USES_PALETTE(AxS[SECOND_Y_AXIS].label.textcolor.type)) return;
	if(plotMode == MODE_SPLOT)
		if(TC_USES_PALETTE(AxS[FIRST_Z_AXIS].label.textcolor.type)) 
			return;
	if(TC_USES_PALETTE(AxS[COLOR_AXIS].label.textcolor.type)) 
		return;
	for(this_object = Gg.P_FirstObject; this_object; this_object = this_object->next) {
		if(TC_USES_PALETTE(this_object->lp_properties.pm3d_color.type))
			return;
	}
#undef TC_USES_PALETTE
	// Palette with continuous colors is not used. 
	_Pm3D.plot_has_palette = false; // otherwise it stays TRUE 
}

bool GnuPlot::IsPlotWithPalette() const { return _Pm3D.plot_has_palette; }
bool GnuPlot::IsPlotWithColorbox() const { return (_Pm3D.plot_has_palette && (Gg.ColorBox.where != SMCOLOR_BOX_NO)); }
//
// Must be called before trying to apply lighting model
//
void GnuPlot::Pm3DInitLightingModel() { _Pm3D.InitLightingModel(); }

void GnuPlot::GpPm3DBlock::InitLightingModel()
{
	light[0] = cos(-SMathConst::PiDiv180*pm3d_shade.rot_x)*cos(-(SMathConst::PiDiv180*pm3d_shade.rot_z));
	light[2] = cos(-SMathConst::PiDiv180*pm3d_shade.rot_x)*sin(-(SMathConst::PiDiv180*pm3d_shade.rot_z));
	light[1] = sin(-SMathConst::PiDiv180*pm3d_shade.rot_x);
}
//
// Layer on layer of coordinate conventions
//
void GnuPlot::IlluminateOneQuadrangle(Quadrangle * q)
{
	GpCoordinate c1, c2, c3, c4;
	GpVertex vtmp;
	Map3D_XYZ(q->vertex.corners[0], &vtmp);
	c1.Pt = vtmp;
	Map3D_XYZ(q->vertex.corners[1], &vtmp);
	c2.Pt = vtmp;
	Map3D_XYZ(q->vertex.corners[2], &vtmp);
	c3.Pt = vtmp; 
	Map3D_XYZ(q->vertex.corners[3], &vtmp);
	c4.Pt = vtmp;
	q->gray = ApplyLightingModel(&c1, &c2, &c3, &c4, q->gray, _Pm3D.color_from_rgbvar);
}
// 
// Adjust current RGB color based on pm3d lighting model.
// Jan 2019: preserve alpha channel
//   This isn't quite right because specular highlights should
//   not be affected by transparency.
// 
int GnuPlot::ApplyLightingModel(GpCoordinate * v0, GpCoordinate * v1, GpCoordinate * v2, GpCoordinate * v3, double gray, bool grayIsRgb)
{
	double normal[3];
	double normal1[3];
	double reflect[3];
	double t;
	double phi;
	double psi;
	uint rgb;
	uint alpha = 0;
	rgb_color color;
	double r, g, b, tmp_r, tmp_g, tmp_b;
	double dot_prod, shade_fact, spec_fact;
	if(grayIsRgb) {
		rgb = static_cast<uint>(gray);
		r = (double)((rgb >> 16) & 0xFF) / 255.0;
		g = (double)((rgb >>  8) & 0xFF) / 255.0;
		b = (double)((rgb      ) & 0xFF) / 255.0;
		alpha = rgb & 0xff000000;
	}
	else {
		Rgb1FromGray(gray, &color);
		r = color.r;
		g = color.g;
		b = color.b;
	}
	psi = -SMathConst::PiDiv180*(_3DBlk.SurfaceRotZ);
	phi = -SMathConst::PiDiv180*(_3DBlk.SurfaceRotX);
	normal[0] = (v1->Pt.y-v0->Pt.y)*(v2->Pt.z-v0->Pt.z) * _3DBlk.Scale3D.y * _3DBlk.Scale3D.z - (v1->Pt.z-v0->Pt.z)*(v2->Pt.y-v0->Pt.y) * _3DBlk.Scale3D.y * _3DBlk.Scale3D.z;
	normal[1] = (v1->Pt.z-v0->Pt.z)*(v2->Pt.x-v0->Pt.x) * _3DBlk.Scale3D.x * _3DBlk.Scale3D.z - (v1->Pt.x-v0->Pt.x)*(v2->Pt.z-v0->Pt.z) * _3DBlk.Scale3D.x * _3DBlk.Scale3D.z;
	normal[2] = (v1->Pt.x-v0->Pt.x)*(v2->Pt.y-v0->Pt.y) * _3DBlk.Scale3D.x * _3DBlk.Scale3D.y - (v1->Pt.y-v0->Pt.y)*(v2->Pt.x-v0->Pt.x) * _3DBlk.Scale3D.x * _3DBlk.Scale3D.y;
	t = sqrt(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]);
	// Trap and handle degenerate case of two identical vertices.
	// Aug 2020
	// FIXME: The problem case that revealed this problem always involved
	//   v2 == (v0 or v1) but some unknown example might instead yield
	//   v0 == v1, which is not addressed here.
	if(t < 1.e-12) {
		if(v2 == v3) // 2nd try; give up and return original color 
			return static_cast<int>(grayIsRgb ? gray : ((uchar)(r*255.0) << 16) + ((uchar)(g*255.0) << 8) + ((uchar)(b*255.0)));
		else
			return ApplyLightingModel(v0, v1, v3, v3, gray, grayIsRgb); // @recursion
	}
	normal[0] /= t;
	normal[1] /= t;
	normal[2] /= t;
	// Correct for the view angle so that the illumination is "fixed" with 
	// respect to the viewer rather than rotating with the surface.        
	if(_Pm3D.pm3d_shade.fixed) {
		normal1[0] =  cos(psi)*normal[0] -  sin(psi)*normal[1] + 0*normal[2];
		normal1[1] =  sin(psi)*normal[0] +  cos(psi)*normal[1] + 0*normal[2];
		normal1[2] =  0*normal[0] +                0*normal[1] + 1*normal[2];
		normal[0] =  1*normal1[0] +         0*normal1[1] +         0*normal1[2];
		normal[1] =  0*normal1[0] +  cos(phi)*normal1[1] -  sin(phi)*normal1[2];
		normal[2] =  0*normal1[0] +  sin(phi)*normal1[1] +  cos(phi)*normal1[2];
	}
	if(normal[2] < 0.0) {
		normal[0] *= -1.0;
		normal[1] *= -1.0;
		normal[2] *= -1.0;
	}
	dot_prod = normal[0]*_Pm3D.light[0] + normal[1]*_Pm3D.light[1] + normal[2]*_Pm3D.light[2];
	shade_fact = (dot_prod < 0) ? -dot_prod : 0;
	tmp_r = r*(_Pm3D.pm3d_shade.ambient-_Pm3D.pm3d_shade.strength+shade_fact*_Pm3D.pm3d_shade.strength);
	tmp_g = g*(_Pm3D.pm3d_shade.ambient-_Pm3D.pm3d_shade.strength+shade_fact*_Pm3D.pm3d_shade.strength);
	tmp_b = b*(_Pm3D.pm3d_shade.ambient-_Pm3D.pm3d_shade.strength+shade_fact*_Pm3D.pm3d_shade.strength);
	// Specular highlighting 
	if(_Pm3D.pm3d_shade.spec > 0.0) {
		reflect[0] = -_Pm3D.light[0]+2*dot_prod*normal[0];
		reflect[1] = -_Pm3D.light[1]+2*dot_prod*normal[1];
		reflect[2] = -_Pm3D.light[2]+2*dot_prod*normal[2];
		t = sqrt(reflect[0]*reflect[0] + reflect[1]*reflect[1] + reflect[2]*reflect[2]);
		reflect[0] /= t;
		reflect[1] /= t;
		reflect[2] /= t;
		dot_prod = -reflect[2];
		// old-style Phong equation; no need for bells or whistles 
		spec_fact = pow(fabs(dot_prod), _Pm3D.pm3d_shade.Phong);
		// White spotlight from direction phi,psi 
		if(dot_prod > 0) {
			tmp_r += _Pm3D.pm3d_shade.spec * spec_fact;
			tmp_g += _Pm3D.pm3d_shade.spec * spec_fact;
			tmp_b += _Pm3D.pm3d_shade.spec * spec_fact;
		}
		// EXPERIMENTAL: Red spotlight from underneath 
		if(dot_prod < 0 && _Pm3D.pm3d_shade.spec2 > 0) {
			tmp_r += _Pm3D.pm3d_shade.spec2 * spec_fact;
		}
	}
	tmp_r = clip_to_01(tmp_r);
	tmp_g = clip_to_01(tmp_g);
	tmp_b = clip_to_01(tmp_b);
	rgb = ((uchar)((tmp_r)*255.0) << 16) + ((uchar)((tmp_g)*255.0) << 8) + ((uchar)((tmp_b)*255.0));
	// restore alpha value if there was one 
	rgb |= alpha;
	return rgb;
}
// 
// The pm3d code works with gpdPoint data structures (double: x,y,z,color)
// term->filled_polygon want gpiPoint data (int: x,y,style).
// This routine converts from gpdPoint to gpiPoint
//
void GnuPlot::FilledPolygon(GpTermEntry * pTerm, gpdPoint * corners, int fillstyle, int nv)
{
	// For normal pm3d surfaces and tessellation the constituent polygons
	// have a small number of vertices, usually 4.
	// However generalized polygons like cartographic areas could have a
	// vastly larger number of vertices, so we allow for dynamic reallocation.
	static int max_vertices = 0; // @global
	static gpiPoint * icorners = NULL; // @global
	static gpiPoint * ocorners = NULL; // @global
	static gpdPoint * clipcorners = NULL; // @global
	if(pTerm->filled_polygon) {
		int i;
		if(nv > max_vertices) {
			max_vertices = nv;
			icorners = (gpiPoint *)SAlloc::R(icorners, (2*max_vertices) * sizeof(gpiPoint));
			ocorners = (gpiPoint *)SAlloc::R(ocorners, (2*max_vertices) * sizeof(gpiPoint));
			clipcorners = (gpdPoint *)SAlloc::R(clipcorners, (2*max_vertices) * sizeof(gpdPoint));
		}
		if((_Pm3D.pm3d.clip == PM3D_CLIP_Z) && (_Pm3D.pm3d_plot_at != PM3D_AT_BASE && _Pm3D.pm3d_plot_at != PM3D_AT_TOP)) {
			const int cfpr = ClipFilledPolygon(corners, clipcorners, nv);
			if(cfpr < 0) // All vertices out of range 
				return;
			if(cfpr > 0) { // Some got clipped 
				nv = cfpr;
				corners = clipcorners;
			}
		}
		for(i = 0; i < nv; i++) {
			double x, y;
			Map3D_XY_double(corners[i].x, corners[i].y, corners[i].z, &x, &y);
			icorners[i].x = static_cast<int>(x);
			icorners[i].y = static_cast<int>(y);
		}
		// Clip to x/y only in 2D projection. 
		if(_3DBlk.splot_map && _Pm3D.pm3d.clip == PM3D_CLIP_Z) {
			for(i = 0; i<nv; i++)
				ocorners[i] = icorners[i];
			V.ClipPolygon(ocorners, icorners, nv, &nv);
		}
		if(fillstyle)
			icorners[0].style = fillstyle;
		else if(Gg.default_fillstyle.fillstyle == FS_EMPTY)
			icorners[0].style = FS_OPAQUE;
		else
			icorners[0].style = style_from_fill(&Gg.default_fillstyle);
		pTerm->FilledPolygon_(nv, icorners);
		if(_Pm3D.pm3d.border.l_type != LT_NODRAW) {
			// LT_DEFAULT means draw border in current color 
			// FIXME: currently there is no obvious way to set LT_DEFAULT  
			if(_Pm3D.pm3d.border.l_type != LT_DEFAULT)
				TermApplyLpProperties(pTerm, &_Pm3D.pm3d.border);
			pTerm->Mov_(icorners[0].x, icorners[0].y);
			for(i = nv-1; i >= 0; i--) {
				pTerm->Vec_(icorners[i].x, icorners[i].y);
			}
		}
	}
}
// 
// Clip existing filled polygon again zmax or zmin.
// We already know this is a convex polygon with at least one vertex in range.
// The clipped polygon may have as few as 3 vertices or as many as n+2.
// Returns the new number of vertices after clipping.
// 
int GnuPlot::ClipFilledPolygon(const gpdPoint * pInpts, gpdPoint * pOutpts, int nv)
{
	int current = 0; // The vertex we are now considering 
	int next = 0;   // The next vertex 
	int nvo = 0;    // Number of vertices after clipping 
	int noutrange = 0; // Number of out-range points 
	int nover = 0;
	int nunder = 0;
	double fraction;
	double zmin = AxS[FIRST_Z_AXIS].Range.low;
	double zmax = AxS[FIRST_Z_AXIS].Range.upp;
	// classify inrange/outrange vertices 
	static int * outrange = NULL;
	static int maxvert = 0;
	if(nv > maxvert) {
		maxvert = nv;
		outrange = (int *)SAlloc::R(outrange, maxvert * sizeof(int));
	}
	for(current = 0; current < nv; current++) {
		if(inrange(pInpts[current].z, zmin, zmax)) {
			outrange[current] = 0;
		}
		else if(pInpts[current].z > zmax) {
			outrange[current] = 1;
			noutrange++;
			nover++;
		}
		else if(pInpts[current].z < zmin) {
			outrange[current] = -1;
			noutrange++;
			nunder++;
		}
	}
	// If all are in range, nothing to do 
	if(noutrange == 0)
		return 0;
	// If all vertices are out of range on the same side, draw nothing 
	if(nunder == nv || nover == nv)
		return -1;
	for(current = 0; current < nv; current++) {
		next = (current+1) % nv;
		// Current point is out of range 
		if(outrange[current]) {
			if(!outrange[next]) {
				// Current point is out-range but next point is in-range.
				// Clip line segment from current-to-next and store as new vertex.
				fraction = ((pInpts[current].z >= zmax ? zmax : zmin) - pInpts[next].z) / (pInpts[current].z - pInpts[next].z);
				pOutpts[nvo].x = pInpts[next].x + fraction * (pInpts[current].x - pInpts[next].x);
				pOutpts[nvo].y = pInpts[next].y + fraction * (pInpts[current].y - pInpts[next].y);
				pOutpts[nvo].z = pInpts[current].z >= zmax ? zmax : zmin;
				nvo++;
			}
			else if(/* Gg.ClipLines2 && */ (outrange[current] * outrange[next] < 0)) {
				// Current point and next point are out of range on opposite
				// sides of the z range.  Clip both ends of the segment.
				fraction = ((pInpts[current].z >= zmax ? zmax : zmin) - pInpts[next].z) / (pInpts[current].z - pInpts[next].z);
				pOutpts[nvo].x = pInpts[next].x + fraction * (pInpts[current].x - pInpts[next].x);
				pOutpts[nvo].y = pInpts[next].y + fraction * (pInpts[current].y - pInpts[next].y);
				pOutpts[nvo].z = pInpts[current].z >= zmax ? zmax : zmin;
				nvo++;
				fraction = ((pInpts[next].z >= zmax ? zmax : zmin) - pOutpts[nvo-1].z) / (pInpts[next].z - pOutpts[nvo-1].z);
				pOutpts[nvo].x = pOutpts[nvo-1].x + fraction * (pInpts[next].x - pOutpts[nvo-1].x);
				pOutpts[nvo].y = pOutpts[nvo-1].y + fraction * (pInpts[next].y - pOutpts[nvo-1].y);
				pOutpts[nvo].z = pInpts[next].z >= zmax ? zmax : zmin;
				nvo++;
			}
			// Current point is in range 
		}
		else {
			pOutpts[nvo++] = pInpts[current];
			if(outrange[next]) {
				// Current point is in-range but next point is out-range.
				// Clip line segment from current-to-next and store as new vertex.
				fraction = ((pInpts[next].z >= zmax ? zmax : zmin) - pInpts[current].z) / (pInpts[next].z - pInpts[current].z);
				pOutpts[nvo].x = pInpts[current].x + fraction * (pInpts[next].x - pInpts[current].x);
				pOutpts[nvo].y = pInpts[current].y + fraction * (pInpts[next].y - pInpts[current].y);
				pOutpts[nvo].z = pInpts[next].z >= zmax ? zmax : zmin;
				nvo++;
			}
		}
	}
	// this is not supposed to happen, but ... 
	if(nvo <= 1)
		return -1;
	return nvo;
}
// 
// EXPERIMENTAL
// returns 1 for top of pm3d surface towards the viewer
//   -1 for bottom of pm3d surface towards the viewer
// NB: the ordering of the quadrangle vertices depends on the scan direction.
//   In the case of depth ordering, the user does not have good control over this.
// 
int GnuPlot::Pm3DSide(const GpCoordinate * p0, const GpCoordinate * p1, const GpCoordinate * p2)
{
	GpVertex v[3];
	double u0, u1, v0, v1;
	// Apply current view rotation to corners of this quadrangle 
	Map3D_XYZ(p0->Pt, &v[0]);
	Map3D_XYZ(p1->Pt, &v[1]);
	Map3D_XYZ(p2->Pt, &v[2]);
	// projection of two adjacent edges 
	u0 = v[1].x - v[0].x;
	u1 = v[1].y - v[0].y;
	v0 = v[2].x - v[0].x;
	v1 = v[2].y - v[0].y;
	// cross-product 
	return sgn(u0*v1 - u1*v0);
}
// 
// Returns a pointer into the list of polygon vertices.
// If necessary reallocates the entire list to ensure there is enough
// room for the requested number of vertices.
// 
gpdPoint * GnuPlot::GpPm3DBlock::GetPolygon(int size)
{
	// Check size of current list, allocate more space if needed 
	if(next_polygon + size >= polygonlistsize) {
		polygonlistsize = 2 * polygonlistsize + size;
		P_PolygonList = (gpdPoint *)SAlloc::R(P_PolygonList, polygonlistsize * sizeof(gpdPoint));
	}
	current_polygon = next_polygon;
	next_polygon = current_polygon + size;
	return &P_PolygonList[current_polygon];
}

void GnuPlot::GpPm3DBlock::FreePolygonList()
{
	ZFREE(P_PolygonList);
	current_polygon = 0;
	next_polygon = 0;
	polygonlistsize = 0;
}
