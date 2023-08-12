/*
 * Copyright © 2000 SuSE, Inc.
 * Copyright © 2007 Red Hat, Inc.
 * Copyright © 2000 Keith Packard, member of The XFree86 Project, Inc.
 *  2005 Lars Knoll & Zack Rusin, Trolltech
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */
#include "cairoint.h"
#pragma hdrstop
#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

static FORCEINLINE double coordinates_to_parameter(double x, double y, double angle)
{
	double t = atan2(y, x) + angle;
	while(t < 0)
		t += SMathConst::Pi2;
	while(t >= SMathConst::Pi2)
		t -= SMathConst::Pi2;
	return 1 - t * (1 / SMathConst::Pi2); // Scale t to [0, 1] and make rotation CCW
}

static uint32 * conical_get_scanline_narrow(pixman_iter_t * iter, const uint32 * mask)
{
	pixman_image_t * image = iter->image;
	int x = iter->x;
	int y = iter->y;
	int width = iter->width;
	uint32 * buffer = iter->buffer;
	gradient_t * gradient = (gradient_t*)image;
	conical_gradient_t * conical = (conical_gradient_t*)image;
	uint32 * end = buffer + width;
	pixman_gradient_walker_t walker;
	boolint affine = TRUE;
	double cx = 1.0;
	double cy = 0.0;
	double cz = 0.0;
	double rx = x + 0.5;
	double ry = y + 0.5;
	double rz = 1.0;
	_pixman_gradient_walker_init(&walker, gradient, image->common.repeat);
	if(image->common.transform) {
		pixman_vector_t v;
		/* reference point is the center of the pixel */
		v.vector[0] = pixman_int_to_fixed(x) + pixman_fixed_1 / 2;
		v.vector[1] = pixman_int_to_fixed(y) + pixman_fixed_1 / 2;
		v.vector[2] = pixman_fixed_1;

		if(!pixman_transform_point_3d(image->common.transform, &v))
			return iter->buffer;

		cx = image->common.transform->matrix[0][0] / 65536.0;
		cy = image->common.transform->matrix[1][0] / 65536.0;
		cz = image->common.transform->matrix[2][0] / 65536.0;
		rx = v.vector[0] / 65536.0;
		ry = v.vector[1] / 65536.0;
		rz = v.vector[2] / 65536.0;
		affine = image->common.transform->matrix[2][0] == 0 && v.vector[2] == pixman_fixed_1;
	}
	if(affine) {
		rx -= conical->center.x / 65536.;
		ry -= conical->center.y / 65536.;
		while(buffer < end) {
			if(!mask || *mask++) {
				double t = coordinates_to_parameter(rx, ry, conical->angle);
				*buffer = _pixman_gradient_walker_pixel(&walker, (pixman_fixed_48_16_t)pixman_double_to_fixed(t));
			}
			++buffer;
			rx += cx;
			ry += cy;
		}
	}
	else {
		while(buffer < end) {
			double x, y;
			if(!mask || *mask++) {
				double t;
				if(rz != 0) {
					x = rx / rz;
					y = ry / rz;
				}
				else {
					x = y = 0.;
				}
				x -= conical->center.x / 65536.0;
				y -= conical->center.y / 65536.0;
				t = coordinates_to_parameter(x, y, conical->angle);
				*buffer = _pixman_gradient_walker_pixel(&walker, (pixman_fixed_48_16_t)pixman_double_to_fixed(t));
			}
			++buffer;

			rx += cx;
			ry += cy;
			rz += cz;
		}
	}
	iter->y++;
	return iter->buffer;
}

static uint32 * conical_get_scanline_wide(pixman_iter_t * iter, const uint32 * mask)
{
	uint32 * buffer = conical_get_scanline_narrow(iter, NULL);
	pixman_expand_to_float((argb_t *)buffer, buffer, PIXMAN_a8r8g8b8, iter->width);
	return buffer;
}

void _pixman_conical_gradient_iter_init(pixman_image_t * image, pixman_iter_t * iter)
{
	if(iter->iter_flags & ITER_NARROW)
		iter->get_scanline = conical_get_scanline_narrow;
	else
		iter->get_scanline = conical_get_scanline_wide;
}

PIXMAN_EXPORT pixman_image_t * pixman_image_create_conical_gradient(const pixman_point_fixed_t *  center, pixman_fixed_t angle, const pixman_gradient_stop_t * stops, int n_stops)
{
	pixman_image_t * image = _pixman_image_allocate();
	if(image) {
		conical_gradient_t * conical = &image->conical;
		if(!_pixman_init_gradient(&conical->common, stops, n_stops)) {
			ZFREE(image);
		}
		else {
			angle = MOD(angle, pixman_int_to_fixed(360));
			image->type = CONICAL;
			conical->center = *center;
			conical->angle = (pixman_fixed_to_double(angle) / 180.0) * SMathConst::Pi;
		}
	}
	return image;
}
