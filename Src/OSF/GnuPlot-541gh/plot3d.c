// GNUPLOT - plot3d.c 
// Copyright 1986 - 1993, 1998, 2004   Thomas Williams, Colin Kelley
//
#include <gnuplot.h>
#pragma hdrstop

enum splot_component {
	SP_FUNCTION, 
	SP_DATAFILE, 
	SP_DATABLOCK, 
	SP_KEYENTRY, 
	SP_VOXELGRID
};

static void sp_replace(GpSurfacePoints * sp, int num_samp_1, int num_iso_1, int num_samp_2, int num_iso_2);
static iso_curve * iso_alloc(int num);
static void iso_extend(struct iso_curve * ip, int num);
static void iso_free(struct iso_curve * ip);

// helper functions for grid_nongrid_data() 
static double splines_kernel(double h);
static double qnorm(double dist_x, double dist_y, int q);
static double pythag(double dx, double dy);
// helper function to detect empty data sets 
static void count_3dpoints(GpSurfacePoints * plot, int * nt, int * ni, int * nu);
// 
// sp_alloc() allocates a GpSurfacePoints structure that can hold 'num_iso_1'
// iso-curves with 'num_samp_2' samples and 'num_iso_2' iso-curves with
// 'num_samp_1' samples.
// If, however num_iso_2 or num_samp_1 is zero no iso curves are allocated.
// 
GpSurfacePoints * GnuPlot::SpAlloc(int num_samp_1, int num_iso_1, int num_samp_2, int num_iso_2)
{
	lp_style_type default_lp_properties; // = DEFAULT_LP_STYLE_TYPE;
	GpSurfacePoints * sp = (GpSurfacePoints *)SAlloc::M(sizeof(*sp));
	memzero(sp, sizeof(GpSurfacePoints));
	// Initialize various fields 
	sp->lp_properties = default_lp_properties;
	sp->fill_properties = Gg.default_fillstyle;
	if(sp->fill_properties.fillstyle == FS_EMPTY)
		sp->fill_properties.fillstyle = FS_SOLID;
	default_arrow_style(&(sp->arrow_properties));
	if(num_iso_2 > 0 && num_samp_1 > 0) {
		int i;
		for(i = 0; i < num_iso_1; i++) {
			iso_curve * icrv = iso_alloc(num_samp_2);
			icrv->next = sp->iso_crvs;
			sp->iso_crvs = icrv;
		}
		for(i = 0; i < num_iso_2; i++) {
			iso_curve * icrv = iso_alloc(num_samp_1);
			icrv->next = sp->iso_crvs;
			sp->iso_crvs = icrv;
		}
	}
	return (sp);
}
// 
// sp_replace() updates a GpSurfacePoints structure so it can hold 'num_iso_1'
// iso-curves with 'num_samp_2' samples and 'num_iso_2' iso-curves with
// 'num_samp_1' samples.
// If, however num_iso_2 or num_samp_1 is zero no iso curves are allocated.
// 
static void sp_replace(GpSurfacePoints * sp, int num_samp_1, int num_iso_1, int num_samp_2, int num_iso_2)
{
	for(iso_curve * icrvs = sp->iso_crvs; icrvs;) {
		iso_curve * icrv = icrvs;
		icrvs = icrvs->next;
		iso_free(icrv);
	}
	sp->iso_crvs = NULL;
	if(num_iso_2 > 0 && num_samp_1 > 0) {
		int i;
		for(i = 0; i < num_iso_1; i++) {
			iso_curve * icrv = iso_alloc(num_samp_2);
			icrv->next = sp->iso_crvs;
			sp->iso_crvs = icrv;
		}
		for(i = 0; i < num_iso_2; i++) {
			iso_curve * icrv = iso_alloc(num_samp_1);
			icrv->next = sp->iso_crvs;
			sp->iso_crvs = icrv;
		}
	}
	else
		sp->iso_crvs = NULL;
}
/*
 * sp_free() releases any memory which was previously malloc()'d to hold
 *   surface points.
 */
/* HBB 20000506: don't risk stack havoc by recursion, use iterative list
 * cleanup instead */
void sp_free(GpSurfacePoints * sp)
{
	while(sp) {
		GpSurfacePoints * next = sp->next_sp;
		SAlloc::F(sp->title);
		ZFREE(sp->title_position);
		while(sp->contours) {
			gnuplot_contours * next_cntrs = sp->contours->next;
			SAlloc::F(sp->contours->coords);
			FREEANDASSIGN(sp->contours, next_cntrs);
		}
		while(sp->iso_crvs) {
			iso_curve * next_icrvs = sp->iso_crvs->next;
			iso_free(sp->iso_crvs);
			sp->iso_crvs = next_icrvs;
		}
		free_labels(sp->labels);
		sp->labels = NULL;
		FREEANDASSIGN(sp, next);
	}
}
// 
// iso_alloc() allocates a iso_curve structure that can hold 'num' points.
// 
struct iso_curve * iso_alloc(int num)                   
{
	iso_curve * ip = (iso_curve *)SAlloc::M(sizeof(iso_curve));
	ip->p_max = (num >= 0 ? num : 0);
	ip->p_count = 0;
	if(num > 0) {
		ip->points = (GpCoordinate *)SAlloc::M(num * sizeof(GpCoordinate));
		memzero(ip->points, num * sizeof(GpCoordinate));
	}
	else
		ip->points = (GpCoordinate *)NULL;
	ip->next = NULL;
	return (ip);
}
// 
// iso_extend() reallocates a iso_curve structure to hold "num" points. This will either expand or shrink the storage.
// 
void iso_extend(struct iso_curve * ip, int num)
{
	if(num != ip->p_max) {
		if(num > 0) {
			ip->points = (GpCoordinate *)SAlloc::R(ip->points, num * sizeof(GpCoordinate));
			if(num > ip->p_max)
				memzero(&(ip->points[ip->p_max]), (num - ip->p_max) * sizeof(GpCoordinate));
			ip->p_max = num;
		}
		else {
			ZFREE(ip->points);
			ip->p_max = 0;
		}
	}
}
//
// iso_free() releases any memory which was previously malloc()'d to hold iso curve points.
//
void iso_free(iso_curve * ip)
{
	if(ip) {
		SAlloc::F(ip->points);
		SAlloc::F(ip);
	}
}

//void plot3drequest()
void GnuPlot::Plot3DRequest(GpTermEntry * pTerm)
{
	// 
	// in the parametric case we would say splot [u= -Pi:Pi] [v= 0:2*Pi] [-1:1]
	// [-1:1] [-1:1] sin(v)*cos(u),sin(v)*cos(u),sin(u) in the non-parametric
	// case we would say only splot [x= -2:2] [y= -5:5] sin(x)*cos(y)
	// 
	int    dummy_token0 = -1;
	int    dummy_token1 = -1;
	/*AXIS_INDEX*/int axis;
	/*AXIS_INDEX*/int u_axis;
	/*AXIS_INDEX*/int v_axis;
	Gg.Is3DPlot = true;
	if(Gg.Parametric && sstreq(_Pb.set_dummy_var[0], "t")) {
		strcpy(_Pb.set_dummy_var[0], "u");
		strcpy(_Pb.set_dummy_var[1], "v");
	}
	// initialize the arrays from the 'set' scalars 
	AxS[FIRST_X_AXIS].Init(FALSE);
	AxS[FIRST_Y_AXIS].Init(FALSE);
	AxS[FIRST_Z_AXIS].Init(TRUE);
	AxS[U_AXIS].Init(FALSE);
	AxS[V_AXIS].Init(FALSE);
	AxS[COLOR_AXIS].Init(TRUE);
	// Always be prepared to restore the autoscaled values on "refresh"
	// Dima Kogan April 2018
	// FIXME: Could we fold this into axis_init?
	for(axis = (AXIS_INDEX)0; axis < NUMBER_OF_MAIN_VISIBLE_AXES; axis++) {
		GpAxis * this_axis = &AxS[axis];
		if(this_axis->set_autoscale != AUTOSCALE_NONE)
			this_axis->range_flags |= RANGE_WRITEBACK;
	}
	// Nonlinear mapping of x or y via linkage to a hidden primary axis. 
	// The user set autoscale for the visible axis; apply it also to the hidden axis. 
	for(axis = (AXIS_INDEX)0; axis < NUMBER_OF_MAIN_VISIBLE_AXES; axis++) {
		if(axis != SAMPLE_AXIS) {
			GpAxis * secondary = &AxS[axis];
			if(secondary->linked_to_primary && secondary->linked_to_primary->index == -secondary->index) {
				GpAxis * primary = secondary->linked_to_primary;
				primary->set_autoscale = secondary->set_autoscale;
				primary->Init(true);
			}
		}
	}
	if(!pTerm)               /* unknown */
		IntErrorCurToken("use 'set term' to set terminal type first");
	// Range limits for the entire plot are optional but must be given	
	// in a fixed order. The keyword 'sample' terminates range parsing.	
	u_axis = (Gg.Parametric ? U_AXIS : FIRST_X_AXIS);
	v_axis = (Gg.Parametric ? V_AXIS : FIRST_Y_AXIS);
	dummy_token0 = ParseRange((AXIS_INDEX)u_axis);
	dummy_token1 = ParseRange((AXIS_INDEX)v_axis);
	if(Gg.Parametric) {
		ParseRange(FIRST_X_AXIS);
		ParseRange(FIRST_Y_AXIS);
	}
	ParseRange(FIRST_Z_AXIS);
	CheckAxisReversed(FIRST_X_AXIS);
	CheckAxisReversed(FIRST_Y_AXIS);
	CheckAxisReversed(FIRST_Z_AXIS);
	if(Pgm.EqualsCur("sample") && Pgm.EqualsNext("["))
		Pgm.Shift();
	// Clear out any tick labels read from data files in previous plot 
	for(axis = (AXIS_INDEX)0; axis<AXIS_ARRAY_SIZE; axis++) {
		t_ticdef * ticdef = &AxS[axis].ticdef;
		if(ticdef->def.user)
			ticdef->def.user = prune_dataticks(ticdef->def.user);
		if(!ticdef->def.user && ticdef->type == TIC_USER)
			ticdef->type = TIC_COMPUTED;
	}
	// use the default dummy variable unless changed 
	if(dummy_token0 > 0)
		Pgm.CopyStr(_Pb.c_dummy_var[0], dummy_token0, MAX_ID_LEN);
	else
		strcpy(_Pb.c_dummy_var[0], _Pb.set_dummy_var[0]);
	if(dummy_token1 > 0)
		Pgm.CopyStr(_Pb.c_dummy_var[1], dummy_token1, MAX_ID_LEN);
	else
		strcpy(_Pb.c_dummy_var[1], _Pb.set_dummy_var[1]);
	// In "set view map" mode the x2 and y2 axes are legal 
	// but must be linked to the respective primary axis. 
	if(_3DBlk.splot_map) {
		if((AxS[SECOND_X_AXIS].ticmode && !AxS[SECOND_X_AXIS].linked_to_primary) || (AxS[SECOND_Y_AXIS].ticmode && !AxS[SECOND_Y_AXIS].linked_to_primary))
			IntError(NO_CARET, "Secondary axis must be linked to primary axis in order to draw tics");
	}
	Eval3DPlots(pTerm);
}
// 
// Helper function for refresh command.  Reexamine each data point and update the
// flags for INRANGE/OUTRANGE/UNDEFINED based on the current limits for that axis.
// Normally the axis limits are already known at this point. But if the user has
// forced "set autoscale" since the previous plot or refresh, we need to reset the
// axis limits and try to approximate the full auto-scaling behaviour.
// 
//void refresh_3dbounds(GpTermEntry * pTerm, GpSurfacePoints * pFirstPlot, int nplots)
void GnuPlot::Refresh3DBounds(GpTermEntry * pTerm, GpSurfacePoints * pFirstPlot, int nplots)
{
	const GpSurfacePoints * this_plot = pFirstPlot;
	for(int iplot = 0; iplot < nplots; iplot++, this_plot = this_plot->next_sp) {
		GpAxis * x_axis = &AxS[FIRST_X_AXIS];
		GpAxis * y_axis = &AxS[FIRST_Y_AXIS];
		GpAxis * z_axis = &AxS[FIRST_Z_AXIS];
		iso_curve * this_curve;
		// IMAGE clipping is done elsewhere, so we don't need INRANGE/OUTRANGE checks 
		if(oneof3(this_plot->plot_style, IMAGE, RGBIMAGE, RGBA_IMAGE)) {
			if(x_axis->set_autoscale)
				ProcessImage(pTerm, this_plot, IMG_UPDATE_AXES);
			continue;
		}
		for(this_curve = this_plot->iso_crvs; this_curve; this_curve = this_curve->next) {
			// VECTOR plots consume two iso_crvs structures, one for heads and one for tails.
			// Only the first one has the true point count; the second one claims zero.
			// FIXME: Maybe we should change this?
			const int n_points = (this_plot->plot_style == VECTOR) ? this_plot->iso_crvs->p_count : this_curve->p_count;
			for(int i = 0; i < n_points; i++) {
				GpCoordinate * point = &this_curve->points[i];
				if(point->type == UNDEFINED)
					continue;
				else
					point->type = INRANGE;
				// If the state has been set to autoscale since the last plot,
				// mark everything INRANGE and re-evaluate the axis limits now.
				// Otherwise test INRANGE/OUTRANGE against previous axis limits.
				// 
				// This autoscaling logic is parallel to that in refresh_bounds() in plot2d.c
				if(!this_plot->noautoscale) {
					x_axis->AutoscaleOnePoint(point->Pt.x);
					y_axis->AutoscaleOnePoint(point->Pt.y);
				}
				if(!x_axis->InRange(point->Pt.x) || !y_axis->InRange(point->Pt.y)) {
					point->type = OUTRANGE;
					continue;
				}
				if(!this_plot->noautoscale) {
					z_axis->AutoscaleOnePoint(point->Pt.z);
				}
				if(!z_axis->InRange(point->Pt.z)) {
					point->type = OUTRANGE;
					continue;
				}
			}
		}
	}
	// handle 'reverse' ranges 
	CheckAxisRange(FIRST_X_AXIS);
	CheckAxisRange(FIRST_Y_AXIS);
	CheckAxisRange(FIRST_Z_AXIS);
	// Make sure the bounds are reasonable, and tweak them if they aren't 
	AxisCheckedExtendEmptyRange(FIRST_X_AXIS, NULL);
	AxisCheckedExtendEmptyRange(FIRST_Y_AXIS, NULL);
	AxisCheckedExtendEmptyRange(FIRST_Z_AXIS, NULL);
}

static double splines_kernel(double h)
{
	return (h > 0.0) ? (h * h * log(h)) : 0.0;
}
// 
// PKJ:
// This function has been hived off out of the original grid_nongrid_data().
// No changes have been made, but variables only needed locally have moved
// out of grid_nongrid_data() into this function. 
// 
void GnuPlot::ThinPlateSplinesSetup(const iso_curve * pOldIsoCrvs, double ** ppXX, int * pNumPoints)
{
	int i, j, k;
	double * xx, * yy, * zz, * b, ** K, d;
	int * indx;
	const iso_curve * oicrv;
	int numpoints = 0;
	for(oicrv = pOldIsoCrvs; oicrv; oicrv = oicrv->next) {
		numpoints += oicrv->p_count;
	}
	xx = (double *)SAlloc::M(sizeof(xx[0]) * (numpoints + 3) * (numpoints + 8));
	// the memory needed is not really (n+3)*(n+8) for now, but might be if I take into account errors ... 
	K = (double **)SAlloc::M(sizeof(K[0]) * (numpoints + 3));
	yy = xx + numpoints;
	zz = yy + numpoints;
	b = zz + numpoints;
	// HBB 20010424: Count actual input points without the UNDEFINED ones, as we copy them 
	numpoints = 0;
	for(oicrv = pOldIsoCrvs; oicrv; oicrv = oicrv->next) {
		const GpCoordinate * opoints = oicrv->points;
		for(k = 0; k < oicrv->p_count; k++, opoints++) {
			// HBB 20010424: avoid crashing for undefined input 
			if(opoints->type != UNDEFINED) {
				xx[numpoints] = opoints->Pt.x;
				yy[numpoints] = opoints->Pt.y;
				zz[numpoints] = opoints->Pt.z;
				numpoints++;
			}
		}
	}
	for(i = 0; i < numpoints + 3; i++) {
		K[i] = b + (numpoints + 3) * (i + 1);
	}
	for(i = 0; i < numpoints; i++) {
		for(j = i + 1; j < numpoints; j++) {
			double dx = xx[i] - xx[j], dy = yy[i] - yy[j];
			K[i][j] = K[j][i] = -splines_kernel(sqrt(dx * dx + dy * dy));
		}
		K[i][i] = 0.0; /* here will come the weights for errors */
		b[i] = zz[i];
	}
	for(i = 0; i < numpoints; i++) {
		K[i][numpoints] = K[numpoints][i] = 1.0;
		K[i][numpoints + 1] = K[numpoints + 1][i] = xx[i];
		K[i][numpoints + 2] = K[numpoints + 2][i] = yy[i];
	}
	b[numpoints] = 0.0;
	b[numpoints + 1] = 0.0;
	b[numpoints + 2] = 0.0;
	K[numpoints][numpoints] = 0.0;
	K[numpoints][numpoints + 1] = 0.0;
	K[numpoints][numpoints + 2] = 0.0;
	K[numpoints + 1][numpoints] = 0.0;
	K[numpoints + 1][numpoints + 1] = 0.0;
	K[numpoints + 1][numpoints + 2] = 0.0;
	K[numpoints + 2][numpoints] = 0.0;
	K[numpoints + 2][numpoints + 1] = 0.0;
	K[numpoints + 2][numpoints + 2] = 0.0;
	indx = (int *)SAlloc::M(sizeof(indx[0]) * (numpoints + 3));
	// actually, K is *not* positive definite, but
	// has only non zero real eigenvalues -> we can use an lu_decomp safely 
	LuDecomp(K, numpoints + 3, indx, &d);
	lu_backsubst(K, numpoints + 3, indx, b);
	SAlloc::F(K);
	SAlloc::F(indx);
	*ppXX = xx;
	*pNumPoints = numpoints;
}

static double qnorm(double dist_x, double dist_y, int q)
{
	double dist = 0.0;
#if (1)
	switch(q) {
		case 1: dist = pythag(dist_x, dist_y); break;
		case 2: dist = dist_x*dist_x + dist_y*dist_y; break;
		default: dist = pow(pythag(dist_x, dist_y), q); break;
	}
#else   /* old code (versions 3.5 - 5.2) */
	switch(q) {
		case 1:
		    dist = dist_x + dist_y;
		    break;
		case 2:
		    dist = dist_x * dist_x + dist_y * dist_y;
		    break;
		case 4:
		    dist = dist_x * dist_x + dist_y * dist_y;
		    dist *= dist;
		    break;
		case 8:
		    dist = dist_x * dist_x + dist_y * dist_y;
		    dist *= dist;
		    dist *= dist;
		    break;
		case 16:
		    dist = dist_x * dist_x + dist_y * dist_y;
		    dist *= dist;
		    dist *= dist;
		    dist *= dist;
		    break;
		default:
		    dist = pow(dist_x, (double)q) + pow(dist_y, (double)q);
		    break;
	}
#endif
	return dist;
}
//
// This is from Numerical Recipes in C, 2nd ed, p70 
//
static double pythag(double dx, double dy)
{
	const double x = fabs(dx);
	const double y = fabs(dy);
	if(x > y) {
		return x*sqrt(1.0 + (y*y)/(x*x));
	}
	if(y==0.0) {
		return 0.0;
	}
	return y*sqrt(1.0 + (x*x)/(y*y));
}

void GnuPlot::GridNonGridData(GpSurfacePoints * pPlot)
{
	int i, j, k;
	double x, y, z, w, dx, dy, xmin, xmax, ymin, ymax;
	double c;
	coord_type dummy_type;
	iso_curve * old_iso_crvs = pPlot->iso_crvs;
	iso_curve * icrv, * oicrv, * oicrvs;
	// these are only needed for thin_plate_splines 
	double * xx = NULL, * yy = NULL, * zz = NULL, * b = NULL;
	int numpoints = 0;
	// Version 5.3 - allow gridding of separate color column so long as
	// we don't need to generate splines for it
	if(pPlot->pm3d_color_from_column && _Plt.dgrid3d_mode == DGRID3D_SPLINES)
		IntError(NO_CARET, "Spline gridding of a separate color column is not implemented");
	// Compute XY bounding box on the original data. */
	// FIXME HBB 20010424: Does this make any sense? Shouldn't we just
	// use whatever the x and y ranges have been found to be, and
	// that's that? The largest difference this is going to make is if
	// we plot a datafile that doesn't span the whole x/y range
	// used. Do we want a dgrid3d over the actual data rectangle, or
	// over the xrange/yrange area? 
	xmin = xmax = old_iso_crvs->points[0].Pt.x;
	ymin = ymax = old_iso_crvs->points[0].Pt.y;
	for(icrv = old_iso_crvs; icrv; icrv = icrv->next) {
		const GpCoordinate * points = icrv->points;
		for(i = 0; i < icrv->p_count; i++, points++) {
			// HBB 20010424: avoid crashing for undefined input 
			if(points->type != UNDEFINED) {
				const coordval _x = points->Pt.x;
				const coordval _y = points->Pt.y;
				SETMIN(xmin, _x);
				SETMAX(xmax, _x);
				SETMIN(ymin, _y);
				SETMAX(ymax, _y);
			}
		}
	}
	dx = (xmax - xmin) / (_Plt.dgrid3d_col_fineness - 1);
	dy = (ymax - ymin) / (_Plt.dgrid3d_row_fineness - 1);
	// Create the new grid structure, and compute the low pass filtering from non grid to grid structure.
	pPlot->iso_crvs = NULL;
	pPlot->num_iso_read = _Plt.dgrid3d_col_fineness;
	pPlot->has_grid_topology = TRUE;
	if(_Plt.dgrid3d_mode == DGRID3D_SPLINES) {
		ThinPlateSplinesSetup(old_iso_crvs, &xx, &numpoints);
		yy = xx + numpoints;
		zz = yy + numpoints;
		b  = zz + numpoints;
	}
	for(i = 0, x = xmin; i < _Plt.dgrid3d_col_fineness; i++, x += dx) {
		icrv = iso_alloc(_Plt.dgrid3d_row_fineness + 1);
		icrv->p_count = _Plt.dgrid3d_row_fineness;
		icrv->next = pPlot->iso_crvs;
		pPlot->iso_crvs = icrv;
		GpCoordinate * points = icrv->points;
		for(j = 0, y = ymin; j < _Plt.dgrid3d_row_fineness; j++, y += dy, points++) {
			z = w = 0.0;
			c = 0.0;
			// as soon as ->type is changed to UNDEFINED, break out of two inner loops! 
			points->type = INRANGE;
			if(_Plt.dgrid3d_mode == DGRID3D_SPLINES) {
				z = b[numpoints];
				for(k = 0; k < numpoints; k++) {
					double dx = xx[k] - x, dy = yy[k] - y;
					z = z - b[k] * splines_kernel(sqrt(dx * dx + dy * dy));
				}
				z = z + b[numpoints + 1] * x + b[numpoints + 2] * y;
			}
			else { // everything, except splines 
				for(oicrv = old_iso_crvs; oicrv; oicrv = oicrv->next) {
					const GpCoordinate * opoints = oicrv->points;
					for(k = 0; k < oicrv->p_count; k++, opoints++) {
						if(_Plt.dgrid3d_mode == DGRID3D_QNORM) {
							double dist = qnorm(fabs(opoints->Pt.x - x), fabs(opoints->Pt.y - y), _Plt.dgrid3d_norm_value);
							if(dist == 0.0) {
								// HBB 981209:  flag the first undefined z and return 
								points->type = UNDEFINED;
								z = opoints->Pt.z;
								c = opoints->CRD_COLOR;
								w = 1.0;
								break; /* out of inner loop */
							}
							else {
								z += opoints->Pt.z / dist;
								c += opoints->CRD_COLOR / dist;
								w += 1.0/dist;
							}
						}
						else { /* ALL else: not spline, not qnorm! */
							double weight = 0.0;
							double dist = pythag((opoints->Pt.x-x)/_Plt.dgrid3d_x_scale, (opoints->Pt.y-y)/_Plt.dgrid3d_y_scale);
							if(_Plt.dgrid3d_mode == DGRID3D_GAUSS) {
								weight = exp(-dist*dist);
							}
							else if(_Plt.dgrid3d_mode == DGRID3D_CAUCHY) {
								weight = 1.0/(1.0 + dist*dist );
							}
							else if(_Plt.dgrid3d_mode == DGRID3D_EXP) {
								weight = exp(-dist);
							}
							else if(_Plt.dgrid3d_mode == DGRID3D_BOX) {
								weight = (dist<1.0) ? 1.0 : 0.0;
							}
							else if(_Plt.dgrid3d_mode == DGRID3D_HANN) {
								if(dist < 1.0)
									weight = 0.5*(1+cos(SMathConst::Pi*dist));
							}
							z += opoints->Pt.z * weight;
							c += opoints->CRD_COLOR * weight;
							w += weight;
						}
					}
					// PKJ: I think this is only relevant for qnorm 
					if(points->type != INRANGE)
						break; /* out of the second-inner loop as well ... */
				}
			} /* endif( dgrid3d_mode == DGRID3D_SPLINES ) */
			// Now that we've escaped the loops safely, we know that we
			// do have a good value in z and w, so we can proceed just as
			// if nothing had happened at all. Nice, isn't it? 
			points->type = INRANGE;
			points->Pt.x = x;
			points->Pt.y = y;
			// Honor requested x and y limits 
			// Historical note: This code was not in 4.0 or 4.2. It imperfectly 
			// restores the clipping behaviour of version 3.7 and earlier. 
			if((x < AxS[AxS.Idx_X].Range.low && !(AxS[AxS.Idx_X].autoscale & AUTOSCALE_MIN)) || 
				(x > AxS[AxS.Idx_X].Range.upp && !(AxS[AxS.Idx_X].autoscale & AUTOSCALE_MAX)) || 
				(y < AxS[AxS.Idx_Y].Range.low && !(AxS[AxS.Idx_Y].autoscale & AUTOSCALE_MIN)) || 
				(y > AxS[AxS.Idx_Y].Range.upp && !(AxS[AxS.Idx_Y].autoscale & AUTOSCALE_MAX)))
				points->type = OUTRANGE;

			if(_Plt.dgrid3d_mode != DGRID3D_SPLINES && !_Plt.dgrid3d_kdensity) {
				z = z / w;
				c = c / w;
			}
			STORE_AND_UPDATE_RANGE(points->Pt.z, z, points->type, AxS.Idx_Z, pPlot->noautoscale, continue);
			if(!pPlot->pm3d_color_from_column)
				c = z;
			dummy_type = points->type;
			STORE_AND_UPDATE_RANGE(points->CRD_COLOR, c, dummy_type, COLOR_AXIS, pPlot->noautoscale, continue);
		}
	}
	SAlloc::F(xx); // safe to call free on NULL pointer if splines not used 
	// Delete the old non grid data. 
	for(oicrvs = old_iso_crvs; oicrvs;) {
		oicrv = oicrvs;
		oicrvs = oicrvs->next;
		iso_free(oicrv);
	}
}
// 
// Get 3D data from file, and store into this_plot data
// structure. Takes care of 'set mapping' and 'set dgrid3d'.
// 
// Notice: this_plot->token is end of datafile spec, before title etc
// will be moved past title etc after we return 
// 
int GnuPlot::Get3DData(GpTermEntry * pTerm, GpSurfacePoints * pPlot)
{
	int xdatum = 0;
	int ydatum = 0;
	int j;
	int pt_in_iso_crv = 0;
	iso_curve * this_iso;
	int retval = 0;
	double v[MAXDATACOLS];
	// Initialize the space that will hold input data values 
	memzero(v, sizeof(v));
	if(_Plt.mapping3d == MAP3D_CARTESIAN) {
		// do this check only, if we have PM3D / PM3D-COLUMN not compiled in 
		if(_Df.df_no_use_specs == 2)
			IntError(pPlot->token, "Need 1 or 3 columns for cartesian data");
		// HBB NEW 20060427: if there's only one, explicit using
		// column, it's z data.  df_axis[] has to reflect that, so
		// df_readline() will expect time/date input. 
		if(_Df.df_no_use_specs == 1)
			_Df.df_axis[0] = FIRST_Z_AXIS;
	}
	else {
		if(_Df.df_no_use_specs == 1)
			IntError(pPlot->token, "Need 2 or 3 columns for polar data");
	}
	pPlot->num_iso_read = 0;
	pPlot->has_grid_topology = TRUE;
	pPlot->pm3d_color_from_column = FALSE;
	// we ought to keep old memory - most likely case
	// is a replot, so it will probably exactly fit into
	// memory already allocated ?
	if(pPlot->iso_crvs) {
		for(iso_curve * icrvs = pPlot->iso_crvs; icrvs;) {
			iso_curve * icrv = icrvs;
			icrvs = icrvs->next;
			iso_free(icrv);
		}
		pPlot->iso_crvs = NULL;
	}
	// data file is already open 
	if(_Df.df_matrix)
		pPlot->has_grid_topology = TRUE;
	{
		/*{{{  read surface from text file */
		iso_curve * local_this_iso = iso_alloc(Gg.Samples1);
		GpCoordinate * cp;
		GpCoordinate * cphead = NULL; /* Only for VECTOR plots */
		double x, y, z;
		double xlow = 0, xhigh = 0;
		double xtail, ytail, ztail;
		double weight = 1.0;
		double zlow = VERYLARGE, zhigh = -VERYLARGE;
		double color = VERYLARGE;
		int pm3d_color_from_column = FALSE;
#define color_from_column(x) pm3d_color_from_column = x
		if(pPlot->plot_style == LABELPOINTS)
			ExpectString(4);
		if(pPlot->plot_style == VECTOR) {
			local_this_iso->next = iso_alloc(Gg.Samples1);
			local_this_iso->next->p_count = 0;
		}
		if(pPlot->plot_style == POLYGONS) {
			pPlot->has_grid_topology = false;
			_Pm3D.track_pm3d_quadrangles = true;
		}
		// If the user has set an explicit locale for numeric input, apply it 
		// here so that it affects data fields read from the input file.      
		set_numeric_locale();
		_Df.df_warn_on_missing_columnheader = true; // Initial state 
		while((retval = DfReadLine(_Df, v, MAXDATACOLS)) != DF_EOF) {
			j = retval;
			if(j == 0) // not blank line, but df_readline couldn't parse it 
				IntWarn(NO_CARET, "Bad data on line %d of file %s", _Df.df_line_number, NZOR(_Df.df_filename, ""));
			if(j == DF_SECOND_BLANK)
				break; // two blank lines 
			if(j == DF_FIRST_BLANK) {
				/* Images are in a sense similar to isocurves.
				 * However, the routine for images is written to
				 * compute the two dimensions of coordinates by
				 * examining the data alone.  That way it can be used
				 * in the 2D plots, for which there is no isoline
				 * record.  So, toss out isoline information for
				 * images.
				 */
				if(oneof3(pPlot->plot_style, IMAGE, RGBIMAGE, RGBA_IMAGE))
					continue;
				if(pPlot->plot_style == VECTOR)
					continue;
				// one blank line 
				if(pt_in_iso_crv == 0) {
					if(xdatum == 0)
						continue;
					pt_in_iso_crv = xdatum;
				}
				if(xdatum > 0) {
					local_this_iso->p_count = xdatum;
					local_this_iso->next = pPlot->iso_crvs;
					pPlot->iso_crvs = local_this_iso;
					pPlot->num_iso_read++;
					if(xdatum != pt_in_iso_crv)
						pPlot->has_grid_topology = FALSE;
					local_this_iso = iso_alloc(pt_in_iso_crv);
					xdatum = 0;
					ydatum++;
				}
				continue;
			}
			else if(j == DF_FOUND_KEY_TITLE) {
				// only the shared part of the 2D and 3D headers is used 
				DfSetKeyTitle((curve_points *)pPlot);
				continue;
			}
			else if(j == DF_KEY_TITLE_MISSING) {
				fprintf(stderr, "get_data: key title not found in requested column\n");
				continue;
			}
			else if(j == DF_COLUMN_HEADERS) {
				continue;
			}
			// its a data point or undefined 
			if(xdatum >= local_this_iso->p_max) {
				// overflow about to occur. Extend size of points[]
				// array. Double the size, and add 1000 points, to
				// avoid needlessly small steps. 
				iso_extend(local_this_iso, xdatum + xdatum + 1000);
				if(pPlot->plot_style == VECTOR) {
					iso_extend(local_this_iso->next, xdatum + xdatum + 1000);
					local_this_iso->next->p_count = 0;
				}
			}
			cp = local_this_iso->points + xdatum;
			if(pPlot->plot_style == VECTOR) {
				if(j < 6) {
					cp->type = UNDEFINED;
					continue;
				}
				cphead = local_this_iso->next->points + xdatum;
			}
			if(j == DF_UNDEFINED || j == DF_MISSING) {
				cp->type = UNDEFINED;
				/* Version 5.5 - Since version 5 2D plot modes store all available
				 * info even if one of the requested columns is missing or undefined.
				 * Now we extend this behavior to 3D processing also.
				 * prior to May 2020 the next line was
				 *    goto come_here_if_undefined;
				 */
				j = _Df.df_no_use_specs;
			}
			if(j == DF_COMPLEX_VALUE) {
				cp->type = UNDEFINED;
				_Plt.Plot3D_NComplexValues++;
				goto come_here_if_undefined;
			}
			cp->type = INRANGE; // unless we find out different 
			// EAM Oct 2004 - Substantially rework this section 
			// now that there are many more plot types.         
			x = y = z = 0.0;
			xtail = ytail = ztail = 0.0;
			// The x, y, z coordinates depend on the mapping type 
			switch(_Plt.mapping3d) {
				case MAP3D_CARTESIAN:
				    if(j == 1) {
					    x = xdatum;
					    y = ydatum;
					    z = v[0];
					    j = 3;
					    break;
				    }
				    if(j == 2) {
					    if(PM3DSURFACE != pPlot->plot_style)
						    IntError(pPlot->token, "2 columns only possible with explicit pm3d style (line %d)", _Df.df_line_number);
					    x = xdatum;
					    y = ydatum;
					    z = v[0];
					    color_from_column(TRUE);
					    color = v[1];
					    j = 3;
					    break;
				    }
				    // Assume everybody agrees that x,y,z are the first three specs 
				    if(j >= 3) {
					    x = v[0];
					    y = v[1];
					    z = v[2];
					    break;
				    }
				    break;
				case MAP3D_SPHERICAL:
				    if(j < 2)
					    IntError(pPlot->token, "Need 2 or 3 columns");
				    if(j < 3) {
					    v[2] = 1; /* default radius */
					    j = 3;
				    }
				    // Convert to radians. 
				    v[0] *= Gg.ang2rad;
				    v[1] *= Gg.ang2rad;
				    x = v[2] * cos(v[0]) * cos(v[1]);
				    y = v[2] * sin(v[0]) * cos(v[1]);
				    z = v[2] * sin(v[1]);
				    break;
				case MAP3D_CYLINDRICAL:
				    if(j < 2)
					    IntError(pPlot->token, "Need 2 or 3 columns");
				    if(j < 3) {
					    v[2] = 1; /* default radius */
					    j = 3;
				    }
				    // Convert to radians. 
				    v[0] *= Gg.ang2rad;
				    x = v[2] * cos(v[0]);
				    y = v[2] * sin(v[0]);
				    z = v[1];
				    break;
				default:
				    IntError(NO_CARET, "Internal error: Unknown mapping type");
				    return retval;
			}
			if(j < _Df.df_no_use_specs)
				IntError(pPlot->token, "Wrong number of columns in input data - line %d", _Df.df_line_number);
			// Work-around for hidden3d, which otherwise would use the 
			// color of the vector midpoint rather than the endpoint. 
			if(pPlot->plot_style == IMPULSES) {
				if(pPlot->lp_properties.pm3d_color.type == TC_Z) {
					color = z;
					color_from_column(TRUE);
				}
			}
			// After the first three columns it gets messy because 
			// different plot styles assume different contents in the columns 
			if(( pPlot->plot_style == POINTSTYLE || pPlot->plot_style == LINESPOINTS)) {
				int varcol = 3;
				coordval var_char = 0;
				if(pPlot->lp_properties.PtSize == PTSZ_VARIABLE)
					cp->CRD_PTSIZE = v[varcol++];
				if(pPlot->lp_properties.PtType == PT_VARIABLE) {
					if(isnan(v[varcol]) && _Df.df_tokens[varcol]) {
						strnzcpy((char *)(&var_char), _Df.df_tokens[varcol], sizeof(coordval));
						truncate_to_one_utf8_char((char *)(&var_char));
						cp->CRD_PTCHAR = var_char;
					}
					cp->CRD_PTTYPE = v[varcol++];
				}
				if(j < varcol)
					IntError(NO_CARET, "Not enough input columns");
				else if(j == varcol) {
					color = z;
					color_from_column(FALSE);
				}
				else {
					color = v[varcol];
					color_from_column(TRUE);
				}
			}
			else if(pPlot->plot_style == LABELPOINTS) {
				if(_3DBlk.draw_contour && !pPlot->opt_out_of_contours) {
					; // j is 3 for some reason 
				}
				else {
					int varcol = 4;
					cp->CRD_ROTATE = pPlot->labels->rotate;
					cp->CRD_PTSIZE = pPlot->labels->lp_properties.PtSize;
					cp->CRD_PTTYPE = pPlot->labels->lp_properties.PtType;
					if(cp->CRD_PTSIZE == PTSZ_VARIABLE)
						cp->CRD_PTSIZE = v[varcol++];
					if(cp->CRD_PTTYPE == PT_VARIABLE)
						cp->CRD_PTTYPE = v[varcol++];
					if(j < varcol)
						IntError(NO_CARET, "Not enough input columns");
					if(j == varcol) {
						/* 4th column holds label text rather than color */
						color = z;
						color_from_column(FALSE);
					}
					else {
						color = v[varcol];
						color_from_column(TRUE);
					}
				}
			}
			else if(pPlot->plot_style == CIRCLES) {
				int varcol = 3;
				// require 4th column = radius 
				cp->CRD_PTSIZE = v[varcol++];
				if(j < varcol)
					IntError(NO_CARET, "Not enough input columns");
				if(j == varcol)
					color = pPlot->fill_properties.border_color.lt;
				else
					color = v[varcol];
				color_from_column(TRUE);
			}
			else if(pPlot->plot_style == VECTOR) {
				// We already enforced that j >= 6 
				xtail = x + v[3];
				ytail = y + v[4];
				ztail = z + v[5];
				if(j >= 7) {
					color = v[6];
					color_from_column(TRUE);
				}
				else {
					color = z;
					color_from_column(FALSE);
				}
			}
			else if(pPlot->plot_style == ZERRORFILL) {
				if(j == 4) {
					zlow = v[2] - v[3];
					zhigh = v[2] + v[3];
				}
				else if(j == 5) {
					zlow = v[3];
					zhigh = v[4];
				}
				else {
					IntError(NO_CARET, "this plot style wants 4 or 5 input columns");
				}
				color_from_column(FALSE);
				_Pm3D.track_pm3d_quadrangles = TRUE;
			}
			else if(pPlot->plot_style == BOXES) {
				// Pop last using value to use as variable color 
				if(pPlot->fill_properties.border_color.type == TC_RGB && pPlot->fill_properties.border_color.value < 0) {
					color_from_column(TRUE);
					color = v[--j];
				}
				else if(pPlot->fill_properties.border_color.type == TC_Z && j >= 4) {
					rgb255_color rgbcolor;
					Rgb255MaxColorsFromGray(Cb2Gray(v[--j]), &rgbcolor);
					uint rgb = (uint)rgbcolor.r << 16 | (uint)rgbcolor.g << 8 | (uint)rgbcolor.b;
					color_from_column(TRUE);
					color = rgb;
				}
				else if(pPlot->fill_properties.border_color.type == TC_COLORMAP && j >= 4) {
					double gray = Map2Gray(v[--j], pPlot->lp_properties.P_Colormap);
					color_from_column(TRUE);
					color = rgb_from_colormap(gray, pPlot->lp_properties.P_Colormap);
				}
				else if(pPlot->lp_properties.l_type == LT_COLORFROMCOLUMN) {
					lp_style_type lptmp;
					LoadLineType(pTerm, &lptmp, (int)v[--j]);
					color_from_column(TRUE);
					color = RgbFromColorspec(&lptmp.pm3d_color);
				}
				if(j >= 4) {
					xlow = x - v[3]/2.;
					xhigh = x + v[3]/2.;
				}
				else if(AxS.__X().log) {
					double width = (V.BoxWidth > 0.0) ? V.BoxWidth : 0.1;
					xlow = x * pow(AxS.__X().base, -width/2.0);
					xhigh = x * pow(AxS.__X().base, width/2.0);
				}
				else {
					double width = (V.BoxWidth > 0.0) ? V.BoxWidth : 1.0;
					xlow = x - width/2.0;
					xhigh = x + width/2.0;
				}
				_Pm3D.track_pm3d_quadrangles = true;
#ifdef BOXERROR_3D
			}
			else if(pPlot->plot_style == BOXERROR) {
				/* This style only makes sense in xz projection, as a way
				 * to produce horizontal bar charts with error bars.
				 * 4 column:	x (y) z zerror
				 * 5 column:	x (y) z zlow zhigh
				 */
				// Optional variable color in last column 
				if((pPlot->lp_properties.pm3d_color.type == TC_RGB && pPlot->lp_properties.pm3d_color.value < 0) || pPlot->lp_properties.l_type == LT_COLORFROMCOLUMN) {
					color_from_column(TRUE);
					color = v[--j];
				}
				if(j == 4) {
					zlow = v[2] - v[3];
					zhigh = v[2] + v[3];
				}
				else if(j == 5) {
					zlow = v[3];
					zhigh = v[4];
				}
				else {
					IntError(NO_CARET, "this plot style wants 4 or 5 input columns");
				}
#endif /* BOXERROR_3D */
			}
			else if(pPlot->plot_style == LINES && pPlot->plot_smooth == SMOOTH_ACSPLINES) {
				if(j >= 4)
					weight = v[3]; /* spline weights */
				else
					weight = 1.0; /* default weight */
			}
			else if(pPlot->plot_style == POLYGONS) {
				if(j >= 4) {
					if(pPlot->lp_properties.l_type == LT_COLORFROMCOLUMN) {
						lp_style_type lptmp;
						LoadLineType(GPT.P_Term, &lptmp, (int)(v[3]));
						color = RgbFromColorspec(&lptmp.pm3d_color);
						color_from_column(TRUE);
					}
					if(pPlot->fill_properties.border_color.type == TC_RGB && pPlot->fill_properties.border_color.value < 0) {
						color = v[3];
						color_from_column(TRUE);
					}
				}
			}
			else { /* all other plot styles */
				if(j >= 4) {
					color = v[3];
					color_from_column(TRUE);
				}
			}
#undef color_from_column
			// The STORE_AND_UPDATE_RANGE macro cannot use "continue" as
			// an action statement because it is wrapped in a loop.
			// I regard this as correct goto use
			cp->type = INRANGE;
			STORE_AND_UPDATE_RANGE(cp->Pt.x, x, cp->type, AxS.Idx_X, pPlot->noautoscale, goto come_here_if_undefined);
			STORE_AND_UPDATE_RANGE(cp->Pt.y, y, cp->type, AxS.Idx_Y, pPlot->noautoscale, goto come_here_if_undefined);
			if(pPlot->plot_style == VECTOR) {
				cphead->type = INRANGE;
				STORE_AND_UPDATE_RANGE(cphead->Pt.x, xtail, cphead->type, AxS.Idx_X, pPlot->noautoscale, goto come_here_if_undefined);
				STORE_AND_UPDATE_RANGE(cphead->Pt.y, ytail, cphead->type, AxS.Idx_Y, pPlot->noautoscale, goto come_here_if_undefined);
			}
			if(_Plt.dgrid3d) {
				// No point in auto-scaling before we re-grid the data 
				cp->Pt.z = z;
				cp->CRD_COLOR = (pm3d_color_from_column) ? color : z;
			}
			else {
				coord_type dummy_type;
				// Without this,  z=Nan or z=Inf or DF_MISSING fails to set
				// CRD_COLOR at all, since the z test bails to a goto.
				if(pPlot->plot_style == IMAGE) {
					dummy_type = INRANGE;
					store_and_update_range(&cp->CRD_COLOR, (pm3d_color_from_column) ? color : z, &dummy_type, &AxS.__CB(), pPlot->noautoscale);
					// Rejecting z because x or y is OUTRANGE causes lost pixels
					// and possilbe non-rectangular image arrays, which then fail.
					// Better to mark all pixels INRANGE and clip them later.
					cp->type = INRANGE;
				}
				// Version 5: cp->z=0 in the UNDEF_ACTION recovers what	version 4 did 
				STORE_AND_UPDATE_RANGE(cp->Pt.z, z, cp->type, AxS.Idx_Z, pPlot->noautoscale, cp->Pt.z = 0.0; goto come_here_if_undefined);
				if(pPlot->plot_style == ZERRORFILL ||  pPlot->plot_style == BOXERROR) {
					STORE_AND_UPDATE_RANGE(cp->CRD_ZLOW, zlow, cp->type, AxS.Idx_Z, pPlot->noautoscale, goto come_here_if_undefined);
					STORE_AND_UPDATE_RANGE(cp->CRD_ZHIGH, zhigh, cp->type, AxS.Idx_Z, pPlot->noautoscale, goto come_here_if_undefined);
				}
				if(pPlot->plot_style == LINES && pPlot->plot_smooth == SMOOTH_ACSPLINES) {
					cp->ylow = weight;
				}
				if(pPlot->plot_style == BOXES) {
					STORE_AND_UPDATE_RANGE(cp->xlow, xlow, cp->type, AxS.Idx_X, pPlot->noautoscale, goto come_here_if_undefined);
					STORE_AND_UPDATE_RANGE(cp->xhigh, xhigh, cp->type, AxS.Idx_X, pPlot->noautoscale, goto come_here_if_undefined);
					// autoscale on y without losing color information stored in CRD_COLOR 
					if(_Plt.boxdepth > 0) {
						double dummy;
						STORE_AND_UPDATE_RANGE(dummy, y - _Plt.boxdepth, cp->type, AxS.Idx_Y, pPlot->noautoscale, {});
						STORE_AND_UPDATE_RANGE(dummy, y + _Plt.boxdepth, cp->type, AxS.Idx_Y, pPlot->noautoscale, {});
					}
					/* We converted linetype colors (lc variable) to RGB colors on input.
					 * Most plot styles do not do this.
					 * When we later call check3d_for_variable_color it needs to know this.
					 */
					if(pPlot->lp_properties.l_type == LT_COLORFROMCOLUMN) {
						pPlot->lp_properties.pm3d_color.type = TC_RGB;
						pPlot->lp_properties.pm3d_color.value = -1.0;
					}
				}
				if(pPlot->plot_style == VECTOR)
					STORE_AND_UPDATE_RANGE(cphead->Pt.z, ztail, cphead->type, AxS.Idx_Z, pPlot->noautoscale, goto come_here_if_undefined);
				if(pm3d_color_from_column) {
					if(pPlot->plot_style == VECTOR)
						cphead->CRD_COLOR = color;
				}
				else {
					color = z;
				}
				dummy_type = cp->type;
				STORE_AND_UPDATE_RANGE(cp->CRD_COLOR, color, dummy_type, COLOR_AXIS, pPlot->noautoscale, goto come_here_if_undefined);
			}
			// At this point we have stored the point coordinates. Now we need to copy 
			// x,y,z into the text_label structure and add the actual text string.     
			if(pPlot->plot_style == LABELPOINTS) {
				// Aug 2018: only store INRANGE labels. This is a CHANGE. 
				if(cp->type == INRANGE)
					StoreLabel(pTerm, pPlot->labels, cp, xdatum, _Df.df_tokens[3], color);
			}
			if(pPlot->plot_style == RGBIMAGE || pPlot->plot_style == RGBA_IMAGE) {
				// We will autoscale the RGB components to  a total range [0:255]
				// so we don't need to do any fancy scaling here.
				// If there is only one column of image data, it must be 32-bit ARGB 
				if(j==4) {
					uint argb = (uint)v[3];
					v[3] = (argb >> 16) & 0xff;
					v[4] = (argb >> 8) & 0xff;
					v[5] = (argb) & 0xff;
					// The alpha channel convention is unfortunate 
					v[6] = 255 - (uint)((argb >> 24) & 0xff);
				}
				cp->CRD_R = v[3];
				cp->CRD_G = v[4];
				cp->CRD_B = v[5];
				cp->CRD_A = v[6]; /* Alpha channel */
			}
come_here_if_undefined:
			/* some may complain, but I regard this as the correct use of goto */
			++xdatum;
		}               /* end of whileloop - end of surface */
		/* We are finished reading user input; return to C locale for internal use */
		reset_numeric_locale();
		if(pm3d_color_from_column) {
			pPlot->pm3d_color_from_column = pm3d_color_from_column;
		}
		if(xdatum > 0) {
			pPlot->num_iso_read++; /* Update last iso. */
			local_this_iso->p_count = xdatum;
			// If this is a VECTOR plot then iso->next is already 
			// occupied by the vector tail coordinates.           
			if(pPlot->plot_style != VECTOR)
				local_this_iso->next = pPlot->iso_crvs;
			pPlot->iso_crvs = local_this_iso;
			if(xdatum != pt_in_iso_crv)
				pPlot->has_grid_topology = FALSE;
		}
		else { /* Free last allocation */
			if(pPlot->plot_style == VECTOR)
				iso_free(local_this_iso->next);
			iso_free(local_this_iso);
		}
		/*}}} */
	}
	if(_Plt.dgrid3d && pPlot->num_iso_read > 0)
		GridNonGridData(pPlot);
	if(pPlot->num_iso_read <= 1)
		pPlot->has_grid_topology = FALSE;
	if(pPlot->has_grid_topology && !_3DBlk.hidden3d && (_3DBlk.implicit_surface || pPlot->plot_style == SURFACEGRID)) {
		iso_curve * new_icrvs = NULL;
		int num_new_iso = pPlot->iso_crvs->p_count;
		int len_new_iso = pPlot->num_iso_read;
		int i;
		// Now we need to set the other direction (pseudo) isolines. 
		for(i = 0; i < num_new_iso; i++) {
			iso_curve * new_icrv = iso_alloc(len_new_iso);
			new_icrv->p_count = len_new_iso;
			for(j = 0, this_iso = pPlot->iso_crvs; this_iso; j++, this_iso = this_iso->next) {
				// copy whole point struct to get type too.
				// wasteful for windows, with padding 
				// more efficient would be extra pointer to same struct 
				new_icrv->points[j] = this_iso->points[i];
			}
			new_icrv->next = new_icrvs;
			new_icrvs = new_icrv;
		}
		// Append the new iso curves after the read ones. 
		for(this_iso = pPlot->iso_crvs; this_iso->next; this_iso = this_iso->next)
			;
		this_iso->next = new_icrvs;
	}
	// Deferred evaluation of plot title now that we know column headers 
	ReevaluatePlotTitle((curve_points *)pPlot);
	return retval;
}
// 
// HBB 20000501: code isolated from eval_3dplots(), where practically
// identical code occurred twice, for direct and crossing isolines,
// respectively.  The latter only are done for in non-hidden3d mode. 
// 
void GnuPlot::CalculateSetOfIsoLines(AXIS_INDEX valueAxIdx, bool cross, iso_curve ** ppThisIso, AXIS_INDEX iso_axis,
	double isoMin, double isoStep, int numIsoToUse, AXIS_INDEX samAxIdx, double samMin, double samStep, int numSamToUse)
{
	int i;
	GpCoordinate * points = (*ppThisIso)->points;
	int do_update_color = (!Gg.Parametric || (Gg.Parametric && valueAxIdx == FIRST_Z_AXIS));
	for(int j = 0; j < numIsoToUse; j++) {
		double iso = isoMin + j * isoStep;
		double isotemp;
		if(AxS[iso_axis].IsNonLinear())
			isotemp = iso = EvalLinkFunction(&AxS[iso_axis], iso);
		else
			isotemp = iso;
		Gcomplex(&_Plt.Plot3D_Func.dummy_values[cross ? 0 : 1], isotemp, 0.0);
		for(i = 0; i < numSamToUse; i++) {
			double sam = samMin + i * samStep;
			GpValue a;
			double temp;
			if(AxS[samAxIdx].IsNonLinear())
				sam = EvalLinkFunction(&AxS[samAxIdx], sam);
			temp = sam;
			Gcomplex(&_Plt.Plot3D_Func.dummy_values[cross ? 1 : 0], temp, 0.0);
			if(cross)
				points[i].Pt.SetXY(iso, sam);
			else
				points[i].Pt.SetXY(sam, iso);
			EvaluateAt(_Plt.Plot3D_Func.at, &a);
			if(Ev.IsUndefined_) {
				points[i].type = UNDEFINED;
				continue;
			}
			if(fabs(Imag(&a)) > Gg.Zero && !isnan(Real(&a))) {
				points[i].type = UNDEFINED;
				_Plt.Plot3D_NComplexValues++;
				continue;
			}
			temp = Real(&a);
			points[i].type = INRANGE;
			STORE_AND_UPDATE_RANGE(points[i].Pt.z, temp, points[i].type, valueAxIdx, FALSE, NOOP);
			if(do_update_color) {
				coord_type dummy_type = points[i].type;
				STORE_AND_UPDATE_RANGE(points[i].CRD_COLOR, temp, dummy_type, COLOR_AXIS, FALSE, NOOP);
			}
		}
		(*ppThisIso)->p_count = numSamToUse;
		*ppThisIso = (*ppThisIso)->next;
		points = (*ppThisIso) ? (*ppThisIso)->points : NULL;
	}
}
// 
// This parses the splot command after any range specifications. To support
// autoscaling on the x/z axis, we want any data files to define the x/y
// range, then to plot any functions using that range. We thus parse the
// input twice, once to pick up the data files, and again to pick up the
// functions. Definitions are processed twice, but that won't hurt.
// div - okay, it doesn't hurt, but every time an option as added for
// datafiles, code to parse it has to be added here. Change so that
// we store starting-token in the plot structure.
// 
void GnuPlot::Eval3DPlots(GpTermEntry * pTerm)
{
	int i;
	GpSurfacePoints ** tp_3d_ptr;
	int    start_token = 0, end_token;
	bool   eof_during_iteration = FALSE; /* set when for [n=start:*] hits NODATA */
	int    begin_token;
	bool   some_data_files = FALSE, some_functions = FALSE;
	bool   was_definition = FALSE;
	int    df_return = 0;
	int    plot_num = 0;
	int    line_num = 0; // default line type 
	// part number of parametric function triplet: 0 = z, 1 = y, 2 = x 
	int    crnt_param = 0;
	char * xtitle;
	char * ytitle;
	legend_key * key = &Gg.KeyT;
	char orig_dummy_u_var[MAX_ID_LEN+1], orig_dummy_v_var[MAX_ID_LEN+1];
	// Free memory from previous splot.
	// If there is an error within this function, the memory is left allocated,
	// since we cannot call sp_free if the list is incomplete
	if(_Plt.first_3dplot && _Plt.plot3d_num > 0)
		sp_free(_Plt.first_3dplot);
	_Plt.plot3d_num = 0;
	_Plt.first_3dplot = NULL;
	AxS.Idx_X = FIRST_X_AXIS;
	AxS.Idx_Y = FIRST_Y_AXIS;
	AxS.Idx_Z = FIRST_Z_AXIS;
	tp_3d_ptr = &_Plt.first_3dplot;
	// Assume that the input data can be re-read later 
	Gg.VolatileData = false;
	// Track complex values so that we can warn about trying to plot a complex-valued function directly.
	_Plt.Plot3D_NComplexValues = 0;
	// Normally we only need to initialize pm3d quadrangles if pm3d mode is active
	// but there are a few special cases that use them outside of pm3d mode.
	_Pm3D.track_pm3d_quadrangles = Pm3DObjects();
	// Explicit ranges in the splot command may temporarily rename dummy variables 
	strcpy(orig_dummy_u_var, _Pb.c_dummy_var[0]);
	strcpy(orig_dummy_v_var, _Pb.c_dummy_var[1]);
	xtitle = NULL;
	ytitle = NULL;
	begin_token = Pgm.GetCurTokenIdx();
/*** First Pass: Read through data files ***/
	//
	// This pass serves to set the x/yranges and to parse the command, as
	// well as filling in everything except the function data. That is done
	// after the x/yrange is defined.
	//
	_Pb.plot_iterator = CheckForIteration();
	_Plt.last_iteration_in_first_pass = INT_MAX;
	while(TRUE) {
		// Forgive trailing comma on a multi-element plot command 
		if(Pgm.EndOfCommand()) {
			if(plot_num == 0)
				IntErrorCurToken("function to plot expected");
			break;
		}
		if(crnt_param == 0 && !was_definition)
			start_token = Pgm.GetCurTokenIdx();
		if(IsDefinition(Pgm.GetCurTokenIdx())) {
			Define();
			if(Pgm.EqualsCur(","))
				Pgm.Shift();
			was_definition = TRUE;
			continue;
		}
		else {
			int specs = -1;
			GpSurfacePoints * p_plot_/*this_plot*/;
			splot_component this_component;
			const char * name_str;
			bool duplication = FALSE;
			bool set_title = FALSE, set_with = FALSE;
			bool set_lpstyle = FALSE;
			bool checked_once = FALSE;
			bool set_labelstyle = FALSE;
			bool set_fillstyle = FALSE;
			bool set_fillcolor = FALSE;
			t_colorspec fillcolor = DEFAULT_COLORSPEC;
			int u_sample_range_token, v_sample_range_token;
			GpValue original_value_u, original_value_v;
			if(!was_definition && (!Gg.Parametric || crnt_param == 0))
				start_token = Pgm.GetCurTokenIdx();
			was_definition = FALSE;
			// Check for sampling range[s]
			// Note: we must allow both for '+', which uses SAMPLE_AXIS, and '++', which uses U_AXIS and V_AXIS
			u_sample_range_token = ParseRange(SAMPLE_AXIS);
			if(u_sample_range_token != 0) {
				AxS[U_AXIS].Range.low = AxS[SAMPLE_AXIS].Range.low;
				AxS[U_AXIS].Range.upp = AxS[SAMPLE_AXIS].Range.upp;
				AxS[U_AXIS].autoscale = AxS[SAMPLE_AXIS].autoscale;
				AxS[U_AXIS].SAMPLE_INTERVAL = AxS[SAMPLE_AXIS].SAMPLE_INTERVAL;
			}
			v_sample_range_token = ParseRange(V_AXIS);
			if(u_sample_range_token > 0)
				AxS[SAMPLE_AXIS].range_flags |= RANGE_SAMPLED;
			if(u_sample_range_token > 0 && AxS[U_AXIS].SAMPLE_INTERVAL != 0)
				AxS[U_AXIS].range_flags |= RANGE_SAMPLED;
			if(v_sample_range_token > 0 && AxS[V_AXIS].SAMPLE_INTERVAL != 0)
				AxS[V_AXIS].range_flags |= RANGE_SAMPLED;
			// Allow replacement of the dummy variables in a function 
			if(u_sample_range_token > 0)
				Pgm.CopyStr(_Pb.c_dummy_var[0], u_sample_range_token, MAX_ID_LEN);
			else if(u_sample_range_token < 0)
				strcpy(_Pb.c_dummy_var[0], _Pb.set_dummy_var[0]);
			else
				strcpy(_Pb.c_dummy_var[0], orig_dummy_u_var);
			if(v_sample_range_token > 0)
				Pgm.CopyStr(_Pb.c_dummy_var[1], v_sample_range_token, MAX_ID_LEN);
			else if(v_sample_range_token < 0)
				strcpy(_Pb.c_dummy_var[1], _Pb.set_dummy_var[1]);
			else
				strcpy(_Pb.c_dummy_var[1], orig_dummy_v_var);
			// Determine whether this plot component is a
			//   function (name_str == NULL)
			//   data file (name_str is "filename")
			//   datablock (name_str is "$datablock")
			//   voxel grid (name_str is "$gridname")
			//   key entry (keyword 'keyentry')
			Pgm.dummy_func = &_Plt.Plot3D_Func;
			name_str = StringOrExpress(NULL);
			Pgm.dummy_func = NULL;
			if(Pgm.EqualsCur("keyentry"))
				this_component = SP_KEYENTRY;
			else if(!name_str)
				this_component = SP_FUNCTION;
			else if(*name_str == '$' && GetVGridByName(name_str))
				this_component = SP_VOXELGRID;
			else if(*name_str == '$')
				this_component = SP_DATABLOCK;
			else
				this_component = SP_DATAFILE;
			switch(this_component) {
				case SP_DATAFILE:
				case SP_DATABLOCK:
				    // data file or datablock to plot 
				    if(Gg.Parametric && crnt_param != 0)
					    IntErrorCurToken("previous parametric function not fully specified");
				    if(!some_data_files) {
					    if(AxS[FIRST_X_AXIS].autoscale & AUTOSCALE_MIN) {
						    AxS[FIRST_X_AXIS].Range.low = VERYLARGE;
					    }
					    if(AxS[FIRST_X_AXIS].autoscale & AUTOSCALE_MAX) {
						    AxS[FIRST_X_AXIS].Range.upp = -VERYLARGE;
					    }
					    if(AxS[FIRST_Y_AXIS].autoscale & AUTOSCALE_MIN) {
						    AxS[FIRST_Y_AXIS].Range.low = VERYLARGE;
					    }
					    if(AxS[FIRST_Y_AXIS].autoscale & AUTOSCALE_MAX) {
						    AxS[FIRST_Y_AXIS].Range.upp = -VERYLARGE;
					    }
					    some_data_files = TRUE;
				    }
				    if(*tp_3d_ptr)
					    p_plot_ = *tp_3d_ptr;
				    else { // no memory malloc()'d there yet 
					    // Allocate enough isosamples and samples 
					    p_plot_ = SpAlloc(0, 0, 0, 0);
					    *tp_3d_ptr = p_plot_;
				    }
				    p_plot_->plot_type = DATA3D;
				    p_plot_->plot_style = Gg.data_style;
				    eof_during_iteration = FALSE;
				    // FIXME: additional fields may need to be reset 
				    p_plot_->opt_out_of_hidden3d = FALSE;
				    p_plot_->title_is_suppressed = FALSE;
				    // Mechanism for deferred evaluation of plot title 
				    free_at(_Df.df_plot_title_at);
				    _Df.df_plot_title_at = NULL;
				    DfSetPlotMode(MODE_SPLOT);
				    specs = DfOpen(_Df, name_str, MAXDATACOLS, (curve_points *)p_plot_);
				    if(_Df.df_matrix)
					    p_plot_->has_grid_topology = true;
				    // Store pointers to the named variables used for sampling 
					p_plot_->sample_var  = (u_sample_range_token > 0) ? AddUdv(u_sample_range_token) : Ev.AddUdvByName(_Pb.c_dummy_var[0]);
					p_plot_->sample_var2 = (v_sample_range_token > 0) ? AddUdv(v_sample_range_token) : Ev.AddUdvByName(_Pb.c_dummy_var[1]);
				    // Save prior values of u, v so we can restore later 
				    original_value_u = p_plot_->sample_var->udv_value;
				    original_value_v = p_plot_->sample_var2->udv_value;
				    p_plot_->token = end_token = Pgm.GetPrevTokenIdx(); // for capture to key 
				    p_plot_->iteration = _Pb.plot_iterator ? _Pb.plot_iterator->iteration : 0; // FIXME: Is this really needed? 
				    // p_plot_->token is temporary, for errors in get_3ddata() 
				    // NB: df_axis is used only for timedate data and 3D cbticlabels 
				    if(specs < 3) {
					    if(AxS[FIRST_X_AXIS].datatype == DT_TIMEDATE)
						    IntErrorCurToken("Need full using spec for x time data");
					    if(AxS[FIRST_Y_AXIS].datatype == DT_TIMEDATE)
						    IntErrorCurToken("Need full using spec for y time data");
				    }
				    _Df.df_axis[0] = FIRST_X_AXIS;
				    _Df.df_axis[1] = FIRST_Y_AXIS;
				    _Df.df_axis[2] = FIRST_Z_AXIS;
				    break;
				case SP_KEYENTRY:
				    Pgm.Shift();
				    plot_num++;
				    if(*tp_3d_ptr)
					    p_plot_ = *tp_3d_ptr;
				    else { // no memory malloc()'d there yet 
					    // Allocate enough isosamples and samples 
					    p_plot_ = SpAlloc(0, 0, 0, 0);
					    *tp_3d_ptr = p_plot_;
				    }
				    p_plot_->plot_type = KEYENTRY;
				    p_plot_->plot_style = LABELPOINTS;
				    p_plot_->token = end_token = Pgm.GetPrevTokenIdx();
				    break;
				case SP_FUNCTION:
				    plot_num++;
				    if(Gg.Parametric) {
					    // Rotate between x/y/z axes 
					    // +2 same as -1, but beats -ve problem 
					    crnt_param = (crnt_param + 2) % 3;
				    }
				    if(*tp_3d_ptr) {
					    p_plot_ = *tp_3d_ptr;
					    if(!_3DBlk.hidden3d)
						    sp_replace(p_plot_, Gg.Samples1, Gg.IsoSamples1, Gg.Samples2, Gg.IsoSamples2);
					    else
						    sp_replace(p_plot_, Gg.IsoSamples1, 0, 0, Gg.IsoSamples2);
				    }
				    else { // no memory malloc()'d there yet 
					    // Allocate enough isosamples and samples 
					    if(!_3DBlk.hidden3d)
						    p_plot_ = SpAlloc(Gg.Samples1, Gg.IsoSamples1, Gg.Samples2, Gg.IsoSamples2);
					    else
						    p_plot_ = SpAlloc(Gg.IsoSamples1, 0, 0, Gg.IsoSamples2);
					    *tp_3d_ptr = p_plot_;
				    }
				    p_plot_->plot_type = FUNC3D;
				    p_plot_->has_grid_topology = TRUE;
				    p_plot_->plot_style = Gg.func_style;
				    p_plot_->num_iso_read = Gg.IsoSamples2;
				    // FIXME: additional fields may need to be reset 
				    p_plot_->opt_out_of_hidden3d = FALSE;
				    p_plot_->title_is_suppressed = FALSE;
				    // ignore it for now 
				    some_functions = TRUE;
				    end_token = Pgm.GetPrevTokenIdx();
				    break;
				case SP_VOXELGRID:
				    plot_num++;
				    if(*tp_3d_ptr)
					    p_plot_ = *tp_3d_ptr;
				    else {
					    // No actual space is needed since we will not store data 
					    p_plot_ = SpAlloc(0, 0, 0, 0);
					    *tp_3d_ptr = p_plot_;
				    }
				    p_plot_->vgrid = GetVGridByName(name_str)->udv_value.v.vgrid;
				    p_plot_->plot_type = VOXELDATA;
				    p_plot_->opt_out_of_hidden3d = TRUE;
				    p_plot_->token = end_token = Pgm.GetPrevTokenIdx();
				    break;
				default:
				    IntError(Pgm.GetPrevTokenIdx(), "unrecognized data source");
				    break;
			}
			ZFREE(p_plot_->title); // clear current title, if it exists 
			// default line and point types 
			p_plot_->lp_properties.l_type = line_num;
			p_plot_->lp_properties.PtType = line_num;
			p_plot_->lp_properties.d_type = line_num;
			// user may prefer explicit line styles 
			p_plot_->hidden3d_top_linetype = line_num;
			if(Gg.PreferLineStyles)
				LpUseProperties(pTerm, &p_plot_->lp_properties, line_num+1);
			else
				LoadLineType(pTerm, &p_plot_->lp_properties, line_num+1);
			// pm 25.11.2001 allow any order of options 
			while(!Pgm.EndOfCommand() || !checked_once) {
				int save_token = Pgm.GetCurTokenIdx();
				// Allow this plot not to affect autoscaling 
				if(Pgm.AlmostEqualsCur("noauto$scale")) {
					Pgm.Shift();
					p_plot_->noautoscale = TRUE;
					continue;
				}
				// deal with title 
				ParsePlotTitle(reinterpret_cast<curve_points *>(p_plot_), xtitle, ytitle, &set_title);
				if(save_token != Pgm.GetCurTokenIdx())
					continue;
				// EXPERIMENTAL smoothing options for splot with lines 
				if(Pgm.EqualsCur("smooth")) {
					Pgm.Shift();
					if(Pgm.AlmostEqualsCur("c$splines") || Pgm.EqualsCur("path")) {
						Pgm.Shift();
						p_plot_->plot_smooth = SMOOTH_CSPLINES;
					}
					else if(Pgm.AlmostEqualsCur("acs$plines")) {
						Pgm.Shift();
						p_plot_->plot_smooth = SMOOTH_ACSPLINES;
					}
					else
						IntErrorCurToken("only cspline or acsplines possible here");
					p_plot_->plot_style = LINES;
					continue;
				}
				// deal with style 
				if(Pgm.AlmostEqualsCur("w$ith")) {
					if(set_with) {
						duplication = TRUE;
						break;
					}
					p_plot_->plot_style = GetStyle();
					if((p_plot_->plot_type == FUNC3D) && ((p_plot_->plot_style & PLOT_STYLE_HAS_ERRORBAR) || 
						(p_plot_->plot_style == LABELPOINTS && !_3DBlk.draw_contour) || (p_plot_->plot_style == VECTOR))) {
						IntWarn(Pgm.GetPrevTokenIdx(), "This style cannot be used to plot a surface defined by a function");
						p_plot_->plot_style = POINTSTYLE;
						p_plot_->plot_type = NODATA;
					}
					if(oneof3(p_plot_->plot_style, IMAGE, RGBA_IMAGE, RGBIMAGE)) {
						if(p_plot_->plot_type == FUNC3D)
							IntError(Pgm.GetPrevTokenIdx(), "a function cannot be plotted as an image");
						else
							GetImageOptions(&p_plot_->image_properties);
					}
					if((p_plot_->plot_style | Gg.data_style) & PM3DSURFACE) {
						if(Pgm.EqualsCur("at")) {
							// option 'with pm3d [at ...]' is explicitly specified 
							Pgm.Shift();
							if(GetPm3DAtOption(&p_plot_->pm3d_where[0]))
								return; /* error */
						}
					}
					if(p_plot_->plot_type == VOXELDATA && oneof2(p_plot_->plot_style, DOTS, POINTSTYLE)) {
						p_plot_->iso_level = 0.0;
						if(Pgm.EqualsCur("above")) {
							Pgm.Shift();
							p_plot_->iso_level = RealExpression();
						}
					}
					if(p_plot_->plot_style == ISOSURFACE) {
						if(p_plot_->plot_type == VOXELDATA) {
							if(Pgm.EqualsCur("level")) {
								Pgm.Shift();
								p_plot_->iso_level = RealExpression();
							}
							else
								IntErrorCurToken("isosurface requires a level");
						}
						else
							IntErrorCurToken("isosurface requires a voxel grid");
						p_plot_->pm3d_color_from_column = FALSE;
						_Pm3D.track_pm3d_quadrangles = TRUE;
					}
					set_with = TRUE;
					continue;
				}
				// Hidden3D code by default includes points, labels and vectors
				// in the hidden3d processing. Check here if this particular
				// plot wants to be excluded.
				if(Pgm.AlmostEqualsCur("nohidden$3d")) {
					Pgm.Shift();
					p_plot_->opt_out_of_hidden3d = TRUE;
					continue;
				}
				// "set contour" is global.  Allow individual plots to opt out 
				if(Pgm.AlmostEqualsCur("nocon$tours")) {
					Pgm.Shift();
					p_plot_->opt_out_of_contours = TRUE;
					continue;
				}
				// "set surface" is global.  Allow individual plots to opt out 
				if(Pgm.AlmostEqualsCur("nosur$face")) {
					Pgm.Shift();
					p_plot_->opt_out_of_surface = TRUE;
					continue;
				}
				// Most plot styles accept line and point properties but do not
				// want font or text properties.
				if(p_plot_->plot_style == VECTOR) {
					int stored_token = Pgm.GetCurTokenIdx();
					if(!checked_once) {
						default_arrow_style(&p_plot_->arrow_properties);
						LoadLineType(pTerm, &p_plot_->arrow_properties.lp_properties, line_num+1);
						checked_once = TRUE;
					}
					ArrowParse(&p_plot_->arrow_properties, TRUE);
					if(stored_token != Pgm.GetCurTokenIdx()) {
						if(set_lpstyle) {
							duplication = TRUE;
							break;
						}
						else {
							set_lpstyle = TRUE;
							p_plot_->lp_properties = p_plot_->arrow_properties.lp_properties;
							continue;
						}
					}
				}
				if(p_plot_->plot_style == PM3DSURFACE) {
					// both previous and subsequent line properties override pm3d default border 
					int stored_token = Pgm.GetCurTokenIdx();
					if(!set_lpstyle)
						p_plot_->lp_properties = _Pm3D.pm3d.border;
					LpParse(pTerm, &p_plot_->lp_properties, LP_ADHOC, FALSE);
					if(stored_token != Pgm.GetCurTokenIdx()) {
						set_lpstyle = TRUE;
						continue;
					}
				}
				if(p_plot_->plot_style != LABELPOINTS) {
					int stored_token = Pgm.GetCurTokenIdx();
					lp_style_type lp; // = DEFAULT_LP_STYLE_TYPE;
					int new_lt = 0;
					lp.l_type = line_num;
					lp.PtType = line_num;
					lp.d_type = line_num;
					// user may prefer explicit line styles 
					if(Gg.PreferLineStyles)
						LpUseProperties(pTerm, &lp, line_num+1);
					else
						LoadLineType(pTerm, &lp, line_num+1);
					new_lt = LpParse(pTerm, &lp, LP_ADHOC, p_plot_->plot_style & PLOT_STYLE_HAS_POINT);
					checked_once = TRUE;
					if(stored_token != Pgm.GetCurTokenIdx()) {
						if(set_lpstyle) {
							duplication = TRUE;
							break;
						}
						else {
							p_plot_->lp_properties = lp;
							set_lpstyle = TRUE;
							if(new_lt)
								p_plot_->hidden3d_top_linetype = new_lt - 1;
							if(p_plot_->lp_properties.PtType != PT_CHARACTER)
								continue;
						}
					}
				}
				// Labels can have font and text property info as plot options
				// In any case we must allocate one instance of the text style
				// that all labels in the plot will share.
				if((p_plot_->plot_style == LABELPOINTS) || (p_plot_->plot_style & PLOT_STYLE_HAS_POINT && p_plot_->lp_properties.PtType == PT_CHARACTER)) {
					int stored_token = Pgm.GetCurTokenIdx();
					if(!p_plot_->labels) {
						p_plot_->labels = new_text_label(-1);
						p_plot_->labels->pos = CENTRE;
						p_plot_->labels->layer = LAYER_PLOTLABELS;
					}
					ParseLabelOptions(p_plot_->labels, 3);
					if(_3DBlk.draw_contour)
						LoadContourLabelOptions(pTerm, p_plot_->labels);
					checked_once = TRUE;
					if(stored_token != Pgm.GetCurTokenIdx()) {
						if(set_labelstyle) {
							duplication = TRUE;
							break;
						}
						else {
							set_labelstyle = TRUE;
							continue;
						}
					}
					else if(p_plot_->lp_properties.PtType == PT_CHARACTER) {
						if(Pgm.EqualsCur(","))
							break;
						else
							continue;
					}
				}
				// Some plots have a fill style as well 
				if((p_plot_->plot_style & PLOT_STYLE_HAS_FILL) && !set_fillstyle) {
					int stored_token = Pgm.GetCurTokenIdx();
					if(Pgm.EqualsCur("fs") || Pgm.AlmostEqualsCur("fill$style")) {
						p_plot_->fill_properties.fillstyle = Gg.default_fillstyle.fillstyle;
						p_plot_->fill_properties.filldensity = Gg.default_fillstyle.filldensity;
						p_plot_->fill_properties.fillpattern = 1;
						p_plot_->fill_properties.border_color = p_plot_->lp_properties.pm3d_color;
						ParseFillStyle(&p_plot_->fill_properties);
						set_fillstyle = TRUE;
					}
					if(p_plot_->plot_style == PM3DSURFACE)
						p_plot_->fill_properties.border_color.type = TC_DEFAULT;
					if(Pgm.EqualsCur("fc") || Pgm.AlmostEqualsCur("fillc$olor")) {
						ParseColorSpec(&fillcolor, TC_RGB);
						set_fillstyle = TRUE;
						set_fillcolor = TRUE;
					}
					if(stored_token != Pgm.GetCurTokenIdx())
						continue;
				}
				break; // unknown option 
			}
			if(duplication)
				IntErrorCurToken("duplicated or contradicting arguments in plot options");
			if(p_plot_->plot_style == TABLESTYLE)
				IntError(NO_CARET, "use `plot with table` rather than `splot with table`");
			if(p_plot_->plot_style == PARALLELPLOT)
				IntError(NO_CARET, "plot style parallelaxes not supported in 3D");
			// set default values for title if this has not been specified 
			p_plot_->title_is_automated = FALSE;
			if(!set_title) {
				p_plot_->title_no_enhanced = TRUE; /* filename or function cannot be enhanced */
				if(key->auto_titles == FILENAME_KEYTITLES) {
					Pgm.MCapture(&(p_plot_->title), start_token, end_token);
					if(crnt_param == 2)
						xtitle = p_plot_->title;
					else if(crnt_param == 1)
						ytitle = p_plot_->title;
					p_plot_->title_is_automated = TRUE;
				}
				else {
					ASSIGN_PTR(xtitle, '\0');
					ASSIGN_PTR(ytitle, '\0');
				}
			}
			// No line/point style given. As lp_parse also supplies
			// the defaults for linewidth and pointsize, call it now to define them. 
			if(!set_lpstyle) {
				if(p_plot_->plot_style == VECTOR) {
					p_plot_->arrow_properties.lp_properties.l_type = line_num;
					ArrowParse(&p_plot_->arrow_properties, TRUE);
					p_plot_->lp_properties = p_plot_->arrow_properties.lp_properties;
				}
				else if(p_plot_->plot_style == PM3DSURFACE) {
					// Use default pm3d border unless we see explicit line properties 
					p_plot_->lp_properties = _Pm3D.pm3d.border;
					LpParse(pTerm, &p_plot_->lp_properties, LP_ADHOC, FALSE);
				}
				else {
					int new_lt = 0;
					p_plot_->lp_properties.l_type = line_num;
					p_plot_->lp_properties.l_width = 1.0;
					p_plot_->lp_properties.PtType = line_num;
					p_plot_->lp_properties.d_type = line_num;
					p_plot_->lp_properties.PtSize = Gg.PointSize;
					// user may prefer explicit line styles 
					if(Gg.PreferLineStyles)
						LpUseProperties(pTerm, &p_plot_->lp_properties, line_num+1);
					else
						LoadLineType(pTerm, &p_plot_->lp_properties, line_num+1);
					new_lt = LpParse(pTerm, &p_plot_->lp_properties, LP_ADHOC, p_plot_->plot_style & PLOT_STYLE_HAS_POINT);
					p_plot_->hidden3d_top_linetype = new_lt ? (new_lt - 1) : line_num;
				}
			}
			// If this plot style uses a fillstyle and we saw an explicit
			// fill color, save it in lp_properties now.
			if((p_plot_->plot_style & PLOT_STYLE_HAS_FILL) && set_fillcolor) {
#ifdef BOXERROR_3D
				if(p_plot_->plot_style == BOXERROR)
					p_plot_->lp_properties.pm3d_color = fillcolor;
				else
#endif
				p_plot_->fill_properties.border_color = fillcolor;
			}
			// No fillcolor given; use the line color for fill also 
			if(((p_plot_->plot_style & PLOT_STYLE_HAS_FILL) && !set_fillstyle) && !oneof2(p_plot_->plot_style, PM3DSURFACE, BOXERROR))
				p_plot_->fill_properties.border_color = p_plot_->lp_properties.pm3d_color;
			// Some low-level routines expect to find the pointflag attribute
			// in lp_properties (they don't have access to the full header). 
			if(p_plot_->plot_style & PLOT_STYLE_HAS_POINT)
				p_plot_->lp_properties.flags |= LP_SHOW_POINTS;
			// Rule out incompatible line/point/style options 
			if(p_plot_->plot_type == FUNC3D) {
				if((p_plot_->plot_style & PLOT_STYLE_HAS_POINT) && (p_plot_->lp_properties.PtSize == PTSZ_VARIABLE))
					p_plot_->lp_properties.PtSize = 1.0;
			}
			// FIXME: Leaving an explicit font in the label style for contour 
			// labels causes a double-free segfault.  Clear it preemptively.  
			if(p_plot_->plot_style == LABELPOINTS && (_3DBlk.draw_contour && !p_plot_->opt_out_of_contours)) {
				ZFREE(p_plot_->labels->font);
			}
			// don't increment the default line/point properties if p_plot_ is an EXPLICIT pm3d surface plot 
			if(crnt_param == 0 && !oneof4(p_plot_->plot_style, PM3DSURFACE, IMAGE, RGBIMAGE, RGBA_IMAGE)) { // same as above, for an (rgb)image plot 
				line_num++;
				if(_3DBlk.draw_contour != CONTOUR_NONE)
					line_num++;
				// This reserves a second color for the back of a hidden3d surface 
				if(_3DBlk.hidden3d && _Plt.hiddenBacksideLinetypeOffset != 0)
					line_num++;
			}
			//
			// now get the data... having to think hard here...
			// first time through, we fill in p_plot_. For second
			// surface in file, we have to allocate another surface
			// struct. BUT we may allocate this store only to
			// find that it is merely some blank lines at end of file
			// tp_3d_ptr is still pointing at next field of prev. plot,
			// before :    prev_or_first -> p_plot_ -> possible_preallocated_store
			//   tp_3d_ptr--^
			// after  :    prev_or_first -> first -> second -> last -> possibly_more_store
			//   tp_3d_ptr ----^
			// if file is empty, tp_3d_ptr is not moved. p_plot_ continues
			// to point at allocated storage, but that will be reused later
			//
			assert(p_plot_ == *tp_3d_ptr);
			if(p_plot_->plot_type == DATA3D) {
				//{{{  read data 
				// pointer to the plot of the first dataset (surface) in the file 
				GpSurfacePoints * first_dataset = p_plot_;
				int this_token = p_plot_->token;
				// Error check to handle missing or unreadable file 
				if(specs == DF_EOF) {
					// FIXME: plot2d does ++line_num here; needed in 3D also? 
					p_plot_->plot_type = NODATA;
					goto SKIPPED_EMPTY_FILE;
				}
				do {
					p_plot_ = *tp_3d_ptr;
					assert(p_plot_);
					// dont move tp_3d_ptr until we are sure we have read a surface
					//
					// used by get_3ddata() 
					p_plot_->token = this_token;
					df_return = Get3DData(pTerm, p_plot_);
					// for second pass 
					p_plot_->token = Pgm.GetCurTokenIdx();
					p_plot_->iteration = _Pb.plot_iterator ? _Pb.plot_iterator->iteration : 0;
					if(p_plot_->num_iso_read == 0)
						p_plot_->plot_type = NODATA;
					// Sep 2017 - Check for all points bad or out of range  
					// (normally harmless but must not cause infinite loop) 
					if(forever_iteration(_Pb.plot_iterator)) {
						int ntotal, ninrange, nundefined;
						count_3dpoints(p_plot_, &ntotal, &ninrange, &nundefined);
						if(ninrange == 0) {
							p_plot_->plot_type = NODATA;
							goto SKIPPED_EMPTY_FILE;
						}
					}
					// okay, we have read a surface 
					plot_num++;
					tp_3d_ptr = &(p_plot_->next_sp);
					if(df_return == DF_EOF)
						break;
					// there might be another surface so allocate
					// and prepare another surface structure
					// This does no harm if in fact there are
					// no more surfaces to read
					if((p_plot_ = *tp_3d_ptr) != NULL) {
						ZFREE(p_plot_->title);
					}
					else {
						// Allocate enough isosamples and samples 
						p_plot_ = *tp_3d_ptr = SpAlloc(0, 0, 0, 0);
					}
					p_plot_->plot_type = DATA3D;
					p_plot_->iteration = _Pb.plot_iterator ? _Pb.plot_iterator->iteration : 0;
					p_plot_->plot_style = first_dataset->plot_style;
					p_plot_->lp_properties = first_dataset->lp_properties;
					p_plot_->fill_properties = first_dataset->fill_properties;
					p_plot_->arrow_properties = first_dataset->arrow_properties;
					if((p_plot_->plot_style == LABELPOINTS) || (p_plot_->plot_style & PLOT_STYLE_HAS_POINT && p_plot_->lp_properties.PtType == PT_CHARACTER)) {
						p_plot_->labels = new_text_label(-1);
						*(p_plot_->labels) = *(first_dataset->labels);
						p_plot_->labels->next = NULL;
					}
					strcpy(p_plot_->pm3d_where, first_dataset->pm3d_where);
				} while(df_return != DF_EOF);
				DfClose(_Df);
				// Plot-type specific range-fiddling 
				if(!AxS[FIRST_Z_AXIS].log && (p_plot_->plot_style == IMPULSES || p_plot_->plot_style == BOXES)) {
					if(AxS[FIRST_Z_AXIS].autoscale & AUTOSCALE_MIN) {
						SETMIN(AxS[FIRST_Z_AXIS].Range.low, 0.0);
					}
					if(AxS[FIRST_Z_AXIS].autoscale & AUTOSCALE_MAX) {
						SETMAX(AxS[FIRST_Z_AXIS].Range.upp, 0.0);
					}
				}
				/*}}} */
			}
			else if(oneof3(p_plot_->plot_type, FUNC3D, KEYENTRY, NODATA)) {
				tp_3d_ptr = &p_plot_->next_sp;
				p_plot_->token = Pgm.GetCurTokenIdx(); /* store for second pass */
				p_plot_->iteration = _Pb.plot_iterator ? _Pb.plot_iterator->iteration : 0;
			}
			else if(p_plot_->plot_type == VOXELDATA) {
				// voxel data in an active vgrid must already be present 
				tp_3d_ptr = &p_plot_->next_sp;
				p_plot_->token = Pgm.GetCurTokenIdx(); /* store for second pass */
				p_plot_->iteration = _Pb.plot_iterator ? _Pb.plot_iterator->iteration : 0;
				// FIXME: I worry that vxrange autoscales xrange and xrange autoscales vxrange 
				AxS[FIRST_X_AXIS].AutoscaleOnePoint(p_plot_->vgrid->vxmin);
				AxS[FIRST_X_AXIS].AutoscaleOnePoint(p_plot_->vgrid->vxmax);
				AxS[FIRST_Y_AXIS].AutoscaleOnePoint(p_plot_->vgrid->vymin);
				AxS[FIRST_Y_AXIS].AutoscaleOnePoint(p_plot_->vgrid->vymax);
				AxS[FIRST_Z_AXIS].AutoscaleOnePoint(p_plot_->vgrid->vzmin);
				AxS[FIRST_Z_AXIS].AutoscaleOnePoint(p_plot_->vgrid->vzmax);
			}
			else {
				IntError(NO_CARET, "unexpected plot_type %d at plot3d:%d\n", p_plot_->plot_type, __LINE__);
			}
			if(p_plot_->plot_type == DATA3D && p_plot_->plot_style == LINES && p_plot_->plot_smooth != SMOOTH_NONE) {
				Gen3DSplines(p_plot_);
				Refresh3DBounds(pTerm, p_plot_, 1);
			}
SKIPPED_EMPTY_FILE:
			if(empty_iteration(_Pb.plot_iterator) && p_plot_)
				p_plot_->plot_type = NODATA;
			if(forever_iteration(_Pb.plot_iterator) && !p_plot_)
				IntError(NO_CARET, "unbounded iteration in something other than a data plot");
			else if(forever_iteration(_Pb.plot_iterator) && (p_plot_->plot_type == NODATA))
				eof_during_iteration = TRUE;
			else if(forever_iteration(_Pb.plot_iterator) && (p_plot_->plot_type != DATA3D))
				IntError(NO_CARET, "unbounded iteration in something other than a data plot");
			else if(forever_iteration(_Pb.plot_iterator))
				_Plt.last_iteration_in_first_pass = _Pb.plot_iterator->iteration_current;
			// restore original value of sample variables 
			// FIXME: somehow p_plot_ has changed since we saved sample_var! 
			if(name_str && p_plot_->sample_var) {
				p_plot_->sample_var->udv_value = original_value_u;
				p_plot_->sample_var2->udv_value = original_value_v;
			}
		} /* !IsDefinition() : end of scope of p_plot_ */
		if(crnt_param != 0) {
			if(Pgm.EqualsCur(",")) {
				Pgm.Shift();
				continue;
			}
			else
				break;
		}
		/* Iterate-over-plot mechanisms */
		if(eof_during_iteration) {
			FPRINTF((stderr, "eof during iteration current %d\n", _Pb.plot_iterator->iteration_current));
			FPRINTF((stderr, "    last_iteration_in_first_pass %d\n", _Plt.last_iteration_in_first_pass));
			eof_during_iteration = FALSE;
		}
		else if(NextIteration(_Pb.plot_iterator)) {
			Pgm.SetTokenIdx(start_token);
			continue;
		}
		_Pb.plot_iterator = cleanup_iteration(_Pb.plot_iterator);
		if(Pgm.EqualsCur(",")) {
			Pgm.Shift();
			_Pb.plot_iterator = CheckForIteration();
			if(forever_iteration(_Pb.plot_iterator))
				if(_Plt.last_iteration_in_first_pass != INT_MAX)
					IntWarn(NO_CARET, "splot does not support multiple unbounded iterations");
		}
		else
			break;
	} // while(TRUE), ie first pass 
	if(Gg.Parametric && crnt_param != 0)
		IntError(NO_CARET, "parametric function not fully specified");
/*** Second Pass: Evaluate the functions ***/
	//
	// Everything is defined now, except the function data. We expect no
	// syntax errors, etc, since the above parsed it all. This makes the code
	// below simpler. If AxS[FIRST_Y_AXIS].autoscale, the yrange may still change.
	// - eh ?  - z can still change.  x/y/z can change if we are parametric ??
	//
	if(some_functions) {
		// I've changed the controlled variable in fn plots to u_min etc since
		// it's easier for me to think parametric - 'normal' plot is after all
		// a special case. I was confused about x_min being both minimum of
		// x values found, and starting value for fn plots.
		double u_min, u_max, u_step, v_min, v_max, v_step;
		double u_isostep, v_isostep;
		AXIS_INDEX u_axis, v_axis;
		GpSurfacePoints * this_plot;
		// Make these point out the right 'u' and 'v' axis. In
		// non-parametric mode, x is used as u, and y as v 
		u_axis = Gg.Parametric ? U_AXIS : FIRST_X_AXIS;
		v_axis = Gg.Parametric ? V_AXIS : FIRST_Y_AXIS;
		if(!Gg.Parametric) {
			// Autoscaling tracked the visible axis coordinates.	
			// For nonlinear axes we must transform the limits back to the primary axis 
			UpdatePrimaryAxisRange(&AxS[FIRST_X_AXIS]);
			UpdatePrimaryAxisRange(&AxS[FIRST_Y_AXIS]);
			// Check that xmin -> xmax is not too small.
			// Give error if xrange badly set from missing datafile error.
			// Parametric fn can still set ranges.
			// If there are no fns, we'll report it later as 'nothing to plot'.
			AxisCheckedExtendEmptyRange(FIRST_X_AXIS, "x range is invalid");
			AxisCheckedExtendEmptyRange(FIRST_Y_AXIS, "y range is invalid");
		}
		if(Gg.Parametric && !some_data_files) {
			// parametric fn can still change x/y range 
			if(AxS[FIRST_X_AXIS].autoscale & AUTOSCALE_MIN)
				AxS[FIRST_X_AXIS].Range.low = VERYLARGE;
			if(AxS[FIRST_X_AXIS].autoscale & AUTOSCALE_MAX)
				AxS[FIRST_X_AXIS].Range.upp = -VERYLARGE;
			if(AxS[FIRST_Y_AXIS].autoscale & AUTOSCALE_MIN)
				AxS[FIRST_Y_AXIS].Range.low = VERYLARGE;
			if(AxS[FIRST_Y_AXIS].autoscale & AUTOSCALE_MAX)
				AxS[FIRST_Y_AXIS].Range.upp = -VERYLARGE;
		}
		/*{{{  figure ranges, restricting logscale limits to be positive */
		u_min = AxisLogValueChecked(u_axis, AxS[u_axis].Range.low, "x range");
		u_max = AxisLogValueChecked(u_axis, AxS[u_axis].Range.upp, "x range");
		if(AxS[u_axis].IsNonLinear()) {
			u_min = AxS[u_axis].linked_to_primary->Range.low;
			u_max = AxS[u_axis].linked_to_primary->Range.upp;
		}
		v_min = AxisLogValueChecked(v_axis, AxS[v_axis].Range.low, "y range");
		v_max = AxisLogValueChecked(v_axis, AxS[v_axis].Range.upp, "y range");
		if(AxS[v_axis].IsNonLinear()) {
			v_min = AxS[v_axis].linked_to_primary->Range.low;
			v_max = AxS[v_axis].linked_to_primary->Range.upp;
		}
		/*}}} */
		if(Gg.Samples1 < 2 || Gg.Samples2 < 2 || Gg.IsoSamples1 < 2 || Gg.IsoSamples2 < 2) {
			IntError(NO_CARET, "samples or iso_samples < 2. Must be at least 2.");
		}
		// start over 
		this_plot = _Plt.first_3dplot;
		Pgm.SetTokenIdx(begin_token);
		_Pb.plot_iterator = CheckForIteration();
		// We kept track of the last productive iteration in the first pass 
		if(forever_iteration(_Pb.plot_iterator))
			_Pb.plot_iterator->iteration_end = _Plt.last_iteration_in_first_pass;
		if(_3DBlk.hidden3d) {
			u_step = (u_max - u_min) / (Gg.IsoSamples1 - 1);
			v_step = (v_max - v_min) / (Gg.IsoSamples2 - 1);
		}
		else {
			u_step = (u_max - u_min) / (Gg.Samples1 - 1);
			v_step = (v_max - v_min) / (Gg.Samples2 - 1);
		}
		u_isostep = (u_max - u_min) / (Gg.IsoSamples1 - 1);
		v_isostep = (v_max - v_min) / (Gg.IsoSamples2 - 1);
		// Read through functions 
		while(TRUE) {
			if(crnt_param == 0 && !was_definition)
				start_token = Pgm.GetCurTokenIdx();
			if(IsDefinition(Pgm.GetCurTokenIdx())) {
				Define();
				if(Pgm.EqualsCur(","))
					Pgm.Shift();
				was_definition = TRUE;
				continue;
			}
			else {
				struct at_type * at_ptr;
				const char * name_str;
				was_definition = FALSE;
				// Forgive trailing comma on a multi-element plot command 
				if(Pgm.EndOfCommand() || this_plot == NULL) {
					IntWarnCurToken("ignoring trailing comma in plot command");
					break;
				}
				// Check for a sampling range
				// Currently we only support sampling of pseudofiles '+' and '++'.
				// This loop is for functions only, so the sampling range is ignored.
				ParseRange(U_AXIS);
				ParseRange(V_AXIS);
				Pgm.dummy_func = &_Plt.Plot3D_Func;
				name_str = StringOrExpress(&at_ptr);
				if(Pgm.EqualsCur("keyentry")) {
				}
				else if(!name_str) {   /* func to plot */
					/*{{{  evaluate function */
					iso_curve * this_iso = this_plot->iso_crvs;
					int num_sam_to_use, num_iso_to_use;
					// crnt_param is used as the axis number.  As the
					// axis array indices are ordered z, y, x, we have
					// to count *backwards*, starting starting at 2,
					// to properly store away contents to x, y and
					// z. The following little gimmick does that. 
					if(Gg.Parametric)
						crnt_param = (crnt_param + 2) % 3;
					_Plt.Plot3D_Func.at = at_ptr;
					num_iso_to_use = Gg.IsoSamples2;
					num_sam_to_use = _3DBlk.hidden3d ? Gg.IsoSamples1 : Gg.Samples1;
					CalculateSetOfIsoLines((AXIS_INDEX)crnt_param, FALSE, &this_iso, v_axis, v_min, v_isostep, num_iso_to_use, u_axis, u_min, u_step, num_sam_to_use);
					if(!_3DBlk.hidden3d) {
						num_iso_to_use = Gg.IsoSamples1;
						num_sam_to_use = Gg.Samples2;
						CalculateSetOfIsoLines((AXIS_INDEX)crnt_param, TRUE, &this_iso, u_axis, u_min, u_isostep, num_iso_to_use, v_axis, v_min, v_step, num_sam_to_use);
					}
					/*}}} */
				} /* end of ITS A FUNCTION TO PLOT */
				// we saved it from first pass 
				Pgm.SetTokenIdx(this_plot->token);
				// we may have seen this one data file in multiple iterations 
				i = this_plot->iteration;
				do {
					this_plot = this_plot->next_sp;
				} while(this_plot && this_plot->token == Pgm.GetCurTokenIdx() && this_plot->iteration == i);
			} // !IsDefinition 
			// Iterate-over-plot mechanism 
			if(crnt_param == 0 && NextIteration(_Pb.plot_iterator)) {
				Pgm.SetTokenIdx(start_token);
				continue;
			}
			if(crnt_param == 0)
				_Pb.plot_iterator = cleanup_iteration(_Pb.plot_iterator);
			if(Pgm.EqualsCur(",")) {
				Pgm.Shift();
				if(crnt_param == 0)
					_Pb.plot_iterator = CheckForIteration();
				if(forever_iteration(_Pb.plot_iterator))
					_Pb.plot_iterator->iteration_end = _Plt.last_iteration_in_first_pass;
			}
			else
				break;
		}               /* while(TRUE) */
		if(Gg.Parametric) {
			// Now actually fix the plot triplets to be single plots. 
			Parametric3DFixUp(_Plt.first_3dplot, &plot_num);
		}
	}                       /* some functions */
	/* if first_3dplot is NULL, we have no functions or data at all.
	 * This can happen if you type "splot x=5", since x=5 is a variable assignment.
	 */
	if(plot_num == 0 || !_Plt.first_3dplot) {
		IntErrorCurToken("no functions or data to plot");
	}
	// Is this too severe? 
	if(_Plt.Plot3D_NComplexValues > 3)
		IntWarn(NO_CARET, "Did you try to plot a complex-valued function?");
	if(AxS[FIRST_X_AXIS].IsNonLinear()) {
		// Transfer observed data or function ranges back to primary axes 
		UpdatePrimaryAxisRange(&AxS[FIRST_X_AXIS]);
		ExtendPrimaryTicRange(&AxS[FIRST_X_AXIS]);
		AxisCheckEmptyNonLinear(&AxS[FIRST_X_AXIS]);
	}
	else {
		AxisCheckedExtendEmptyRange(FIRST_X_AXIS, "All points x value undefined");
		CheckAxisRange(FIRST_X_AXIS);
	}
	if(AxS[FIRST_Y_AXIS].IsNonLinear()) {
		// Transfer observed data or function ranges back to primary axes 
		UpdatePrimaryAxisRange(&AxS[FIRST_Y_AXIS]);
		ExtendPrimaryTicRange(&AxS[FIRST_Y_AXIS]);
		AxisCheckEmptyNonLinear(&AxS[FIRST_Y_AXIS]);
	}
	else {
		AxisCheckedExtendEmptyRange(FIRST_Y_AXIS, "All points y value undefined");
		CheckAxisRange(FIRST_Y_AXIS);
	}
	if(AxS[FIRST_Z_AXIS].IsNonLinear()) {
		UpdatePrimaryAxisRange(&AxS[FIRST_Z_AXIS]);
		ExtendPrimaryTicRange(&AxS[FIRST_Z_AXIS]);
	}
	else {
		AxisCheckedExtendEmptyRange(FIRST_Z_AXIS, _3DBlk.splot_map ? NULL : "All points z value undefined");
		CheckAxisRange(FIRST_Z_AXIS);
	}
	SetupTics(&AxS[FIRST_X_AXIS], 20);
	SetupTics(&AxS[FIRST_Y_AXIS], 20);
	SetupTics(&AxS[FIRST_Z_AXIS], 20);
	if(_3DBlk.splot_map) {
		SetupTics(&AxS[SECOND_X_AXIS], 20);
		SetupTics(&AxS[SECOND_Y_AXIS], 20);
	}
	SetPlotWithPalette(plot_num, MODE_SPLOT);
	if(IsPlotWithPalette()) {
		SetCbMinMax();
		AxisCheckedExtendEmptyRange(COLOR_AXIS, "All points of colorbox value undefined");
		SetupTics(&AxS[COLOR_AXIS], 20);
	}
	if(!plot_num || !_Plt.first_3dplot) {
		IntErrorCurToken("no functions or data to plot");
	}
	// Creates contours if contours are to be plotted as well. 
	if(_3DBlk.draw_contour) {
		GpSurfacePoints * this_plot;
		for(this_plot = _Plt.first_3dplot, i = 0; i < plot_num; this_plot = this_plot->next_sp, i++) {
			if(this_plot->contours) {
				for(gnuplot_contours * cntrs = this_plot->contours; cntrs;) {
					gnuplot_contours * cntr = cntrs;
					cntrs = cntrs->next;
					SAlloc::F(cntr->coords);
					SAlloc::F(cntr);
				}
				this_plot->contours = NULL;
			}
			// Make sure this one can be contoured. 
			if(oneof4(this_plot->plot_style, VECTOR, IMAGE, RGBIMAGE, RGBA_IMAGE))
				continue;
			if(this_plot->plot_type == NODATA)
				continue;
			if(this_plot->plot_type == KEYENTRY)
				continue;
			// Allow individual surfaces to opt out of contouring 
			if(this_plot->opt_out_of_contours)
				continue;
			if(!this_plot->has_grid_topology)
				IntWarn(NO_CARET, "Cannot contour non grid data. Please use \"set dgrid3d\".");
			else if(this_plot->plot_type == DATA3D)
				this_plot->contours = Contour(this_plot->num_iso_read, this_plot->iso_crvs);
			else
				this_plot->contours = Contour(Gg.IsoSamples2, this_plot->iso_crvs);
		}
	} // draw_contour 
	// Images don't fit the grid model.  (The image data correspond
	// to pixel centers.)  To make image work in hidden 3D, add
	// another non-visible phantom surface of only four points
	// outlining the image.  Opt out of hidden3d for the {RGB}IMAGE
	// to avoid processing large amounts of data.
	if(_3DBlk.hidden3d && plot_num) {
		GpSurfacePoints * this_plot = _Plt.first_3dplot;
		do {
			if(oneof2(this_plot->plot_style, IMAGE, RGBIMAGE) && (this_plot->image_properties.nrows > 0 && this_plot->image_properties.ncols > 0) && !(this_plot->opt_out_of_hidden3d)) {
				GpSurfacePoints * new_plot = SpAlloc(2, 0, 0, 2);
				// Construct valid 2 x 2 parallelogram. 
				new_plot->num_iso_read = 2;
				new_plot->iso_crvs->p_count = 2;
				new_plot->iso_crvs->next->p_count = 2;
				new_plot->next_sp = this_plot->next_sp;
				this_plot->next_sp = new_plot;
				// Set up hidden3d behavior, no visible lines but opaque to items behind the parallelogram. 
				new_plot->plot_style = SURFACEGRID;
				new_plot->opt_out_of_surface = TRUE;
				new_plot->opt_out_of_contours = TRUE;
				new_plot->has_grid_topology = TRUE;
				new_plot->hidden3d_top_linetype = LT_NODRAW;
				new_plot->plot_type = DATA3D;
				new_plot->opt_out_of_hidden3d = FALSE;
				// Compute the geometry of the phantom 
				ProcessImage(pTerm, this_plot, IMG_UPDATE_CORNERS);
				// Advance over the phantom 
				plot_num++;
				this_plot = this_plot->next_sp;
			}
			this_plot = this_plot->next_sp;
		} while(this_plot);
	}
	// if we get here, all went well, so record the line for replot 
	if(Pgm.plot_token != -1) {
		// note that m_capture also frees the old replot_line 
		Pgm.MCapture(&Pgm.replot_line, Pgm.plot_token, Pgm.GetPrevTokenIdx());
		Pgm.plot_token = -1;
		Ev.FillGpValString("GPVAL_LAST_PLOT", Pgm.replot_line);
	}
	// record that all went well 
	_Plt.plot3d_num = plot_num;
	// perform the plot 
	if(Tab.Mode) {
		Print3DTable(plot_num);
	}
	else {
		Do3DPlot(pTerm, _Plt.first_3dplot, plot_num, NORMAL_REPLOT);
		/* after do_3dplot(), AxS[].min and .max
		 * contain the plotting range actually used (rounded
		 * to tic marks, not only the min/max data values)
		 * --> save them now for writeback if requested
		 */
		SaveWritebackAllAxes();
		// Mark these plots as safe for quick refresh 
		SET_REFRESH_OK(E_REFRESH_OK_3D, plot_num);
	}
	UpdateGpvalVariables(pTerm, 1); // update GPVAL_ variables available to user 
}
/*
 * The hardest part of this routine is collapsing the FUNC plot types in the
 * list (which are guaranteed to occur in (x,y,z) triplets while preserving
 * the non-FUNC type plots intact.  This means we have to work our way
 * through various lists.  Examples (hand checked):
 * start_plot:F1->F2->F3->NULL ==> F3->NULL
 * start_plot:F1->F2->F3->F4->F5->F6->NULL ==> F3->F6->NULL
 * start_plot:F1->F2->F3->D1->D2->F4->F5->F6->D3->NULL ==>
 * F3->D1->D2->F6->D3->NULL
 *
 * x and y ranges now fixed in eval_3dplots
 */
void GnuPlot::Parametric3DFixUp(GpSurfacePoints * pStartPlot, int * pPlotNum)
{
	GpSurfacePoints * xp, * new_list, * free_list = NULL;
	GpSurfacePoints ** last_pointer = &new_list;
	int i, surface;
	/*
	 * Ok, go through all the plots and move FUNC3D types together.  Note:
	 * this originally was written to look for a NULL next pointer, but
	 * gnuplot wants to be sticky in grabbing memory and the right number of
	 * items in the plot list is controlled by the plot_num variable.
	 *
	 * Since gnuplot wants to do this sticky business, a free_list of
	 * GpSurfacePoints is kept and then tagged onto the end of the plot list
	 * as this seems more in the spirit of the original memory behavior than
	 * simply freeing the memory.  I'm personally not convinced this sort of
	 * concern is worth it since the time spent computing points seems to
	 * dominate any garbage collecting that might be saved here...
	 */
	new_list = xp = pStartPlot;
	for(surface = 0; surface < *pPlotNum; surface++) {
		if(xp->plot_type == FUNC3D) {
			GpSurfacePoints * yp = xp->next_sp;
			GpSurfacePoints * zp = yp->next_sp;
			/* Here's a FUNC3D parametric function defined as three parts.
			 * Go through all the points and assign the x's and y's from xp and
			 * yp to zp. min/max already done
			 */
			iso_curve * xicrvs = xp->iso_crvs;
			iso_curve * yicrvs = yp->iso_crvs;
			iso_curve * zicrvs = zp->iso_crvs;
			(*pPlotNum) -= 2;
			assert(INRANGE < OUTRANGE && OUTRANGE < UNDEFINED);
			while(zicrvs) {
				GpCoordinate * xpoints = xicrvs->points;
				GpCoordinate * ypoints = yicrvs->points;
				GpCoordinate * zpoints = zicrvs->points;
				for(i = 0; i < zicrvs->p_count; ++i) {
					zpoints[i].Pt.SetXY(xpoints[i].Pt.z, ypoints[i].Pt.z);
					SETMAX(zpoints[i].type, xpoints[i].type);
					SETMAX(zpoints[i].type, ypoints[i].type);
				}
				xicrvs = xicrvs->next;
				yicrvs = yicrvs->next;
				zicrvs = zicrvs->next;
			}
			/* add xp and yp to head of free list */
			assert(xp->next_sp == yp);
			yp->next_sp = free_list;
			free_list = xp;
			// add zp to tail of new_list 
			*last_pointer = zp;
			last_pointer = &(zp->next_sp);
			xp = zp->next_sp;
		}
		else {          /* its a data plot */
			assert(*last_pointer == xp); /* think this is true ! */
			last_pointer = &(xp->next_sp);
			xp = xp->next_sp;
		}
	}
	// Ok, append free list and write first_plot 
	*last_pointer = free_list;
	_Plt.first_3dplot = new_list;
}

void GnuPlot::LoadContourLabelOptions(GpTermEntry * pTerm, text_label * pContourLabel)
{
	lp_style_type * lp = &pContourLabel->lp_properties;
	lp->p_interval = _3DBlk.clabel_interval;
	LpParse(pTerm, lp, LP_ADHOC, TRUE);
}
//
// Count the number of data points but do nothing with them 
//
static void count_3dpoints(GpSurfacePoints * plot, int * ntotal, int * ninrange, int * nundefined)
{
	iso_curve * icrvs = plot->iso_crvs;
	*ntotal = *ninrange = *nundefined = 0;
	while(icrvs) {
		GpCoordinate * point;
		for(int i = 0; i < icrvs->p_count; i++) {
			point = &(icrvs->points[i]);
			(*ntotal)++;
			if(point->type == INRANGE)
				(*ninrange)++;
			if(point->type == UNDEFINED)
				(*nundefined)++;
		}
		icrvs = icrvs->next;
	}
}
