/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2003 University of Southern California
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is University of Southern
 * California.
 *
 * Contributor(s):
 *	Carl D. Worth <cworth@cworth.org>
 */

#include "cairoint.h"

typedef struct cairo_path_bounder {
    cairo_point_t current_point;
    cairo_bool_t has_initial_point;
    cairo_bool_t has_point;

    cairo_box_t extents;
} cairo_path_bounder_t;

static void
_cairo_path_bounder_init (cairo_path_bounder_t *bounder)
{
    bounder->has_initial_point = FALSE;
    bounder->has_point = FALSE;
}

static void
_cairo_path_bounder_fini (cairo_path_bounder_t *bounder)
{
    bounder->has_initial_point = FALSE;
    bounder->has_point = FALSE;
}

static void
_cairo_path_bounder_add_point (cairo_path_bounder_t *bounder,
			       const cairo_point_t *point)
{
    if (bounder->has_point) {
	if (point->x < bounder->extents.p1.x)
	    bounder->extents.p1.x = point->x;

	if (point->y < bounder->extents.p1.y)
	    bounder->extents.p1.y = point->y;

	if (point->x > bounder->extents.p2.x)
	    bounder->extents.p2.x = point->x;

	if (point->y > bounder->extents.p2.y)
	    bounder->extents.p2.y = point->y;
    } else {
	bounder->extents.p1.x = point->x;
	bounder->extents.p1.y = point->y;
	bounder->extents.p2.x = point->x;
	bounder->extents.p2.y = point->y;

	bounder->has_point = TRUE;
    }
}

static cairo_status_t
_cairo_path_bounder_move_to (void *closure,
			     const cairo_point_t *point)
{
    cairo_path_bounder_t *bounder = closure;

    bounder->current_point = *point;
    bounder->has_initial_point = TRUE;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_line_to (void *closure,
			     const cairo_point_t *point)
{
    cairo_path_bounder_t *bounder = closure;

    if (bounder->has_initial_point) {
	_cairo_path_bounder_add_point (bounder, &bounder->current_point);
	bounder->has_initial_point = FALSE;
    }

    _cairo_path_bounder_add_point (bounder, point);
    bounder->current_point = *point;

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_curve_to (void *closure,
			      const cairo_point_t *b,
			      const cairo_point_t *c,
			      const cairo_point_t *d)
{
    cairo_path_bounder_t *bounder = closure;

    /* If the bbox of the control points is entirely inside, then we
     * do not need to further evaluate the spline.
     */
    if (! bounder->has_point ||
	b->x < bounder->extents.p1.x || b->x > bounder->extents.p2.x ||
	b->y < bounder->extents.p1.y || b->y > bounder->extents.p2.y ||
	c->x < bounder->extents.p1.x || c->x > bounder->extents.p2.x ||
	c->y < bounder->extents.p1.y || c->y > bounder->extents.p2.y ||
	d->x < bounder->extents.p1.x || d->x > bounder->extents.p2.x ||
	d->y < bounder->extents.p1.y || d->y > bounder->extents.p2.y)
    {
	return _cairo_spline_bound (_cairo_path_bounder_line_to, bounder,
				    &bounder->current_point, b, c, d);
    }
    else
    {
	/* All control points are within the current extents. */
	return CAIRO_STATUS_SUCCESS;
    }
}

static cairo_status_t
_cairo_path_bounder_curve_to_cp (void *closure,
				 const cairo_point_t *b,
				 const cairo_point_t *c,
				 const cairo_point_t *d)
{
    cairo_path_bounder_t *bounder = closure;

    if (bounder->has_initial_point) {
	_cairo_path_bounder_add_point (bounder, &bounder->current_point);
	bounder->has_initial_point = FALSE;
    }

    _cairo_path_bounder_add_point (bounder, b);
    _cairo_path_bounder_add_point (bounder, c);
    _cairo_path_bounder_add_point (bounder, d);

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
_cairo_path_bounder_close_path (void *closure)
{
    return CAIRO_STATUS_SUCCESS;
}

/* This computes the extents of all the points in the path, not those of
 * the damage area (i.e it does not consider winding and it only inspects
 * the control points of the curves, not the flattened path).
 */
void
_cairo_path_fixed_approximate_clip_extents (cairo_path_fixed_t *path,
					    cairo_rectangle_int_t *extents)
{
    cairo_path_bounder_t bounder;
    cairo_status_t status;

    _cairo_path_bounder_init (&bounder);

    status = _cairo_path_fixed_interpret (path, CAIRO_DIRECTION_FORWARD,
					  _cairo_path_bounder_move_to,
					  _cairo_path_bounder_line_to,
					  _cairo_path_bounder_curve_to_cp,
					  _cairo_path_bounder_close_path,
					  &bounder);
    assert (status == CAIRO_STATUS_SUCCESS);

    if (bounder.has_point) {
	_cairo_box_round_to_rectangle (&bounder.extents, extents);
    } else {
	extents->x = extents->y = 0;
	extents->width = extents->height = 0;
    }

    _cairo_path_bounder_fini (&bounder);
}

/* A slightly better approximation than above - we actually decompose the
 * Bezier, but we continue to ignore winding.
 */
void
_cairo_path_fixed_approximate_fill_extents (cairo_path_fixed_t *path,
					    cairo_rectangle_int_t *extents)
{
    cairo_path_bounder_t bounder;
    cairo_status_t status;

    _cairo_path_bounder_init (&bounder);

    status = _cairo_path_fixed_interpret (path, CAIRO_DIRECTION_FORWARD,
					  _cairo_path_bounder_move_to,
					  _cairo_path_bounder_line_to,
					  _cairo_path_bounder_curve_to,
					  _cairo_path_bounder_close_path,
					  &bounder);
    assert (status == CAIRO_STATUS_SUCCESS);

    if (bounder.has_point) {
	_cairo_box_round_to_rectangle (&bounder.extents, extents);
    } else {
	extents->x = extents->y = 0;
	extents->width = extents->height = 0;
    }

    _cairo_path_bounder_fini (&bounder);
}

/* Adjusts the fill extents (above) by the device-space pen.  */
void
_cairo_path_fixed_approximate_stroke_extents (cairo_path_fixed_t *path,
					      cairo_stroke_style_t *style,
					      const cairo_matrix_t *ctm,
					      cairo_rectangle_int_t *extents)
{
    cairo_path_bounder_t bounder;
    cairo_status_t status;

    _cairo_path_bounder_init (&bounder);

    status = _cairo_path_fixed_interpret (path, CAIRO_DIRECTION_FORWARD,
					  _cairo_path_bounder_move_to,
					  _cairo_path_bounder_line_to,
					  _cairo_path_bounder_curve_to,
					  _cairo_path_bounder_close_path,
					  &bounder);
    assert (status == CAIRO_STATUS_SUCCESS);

    if (bounder.has_point) {
	double dx, dy;

	_cairo_stroke_style_max_distance_from_path (style, ctm, &dx, &dy);

	bounder.extents.p1.x -= _cairo_fixed_from_double (dx);
	bounder.extents.p2.x += _cairo_fixed_from_double (dx);
	bounder.extents.p1.y -= _cairo_fixed_from_double (dy);
	bounder.extents.p2.y += _cairo_fixed_from_double (dy);

	_cairo_box_round_to_rectangle (&bounder.extents, extents);
    } else {
	extents->x = extents->y = 0;
	extents->width = extents->height = 0;
    }

    _cairo_path_bounder_fini (&bounder);
}

void
_cairo_path_fixed_bounds (cairo_path_fixed_t *path,
			  double *x1, double *y1,
			  double *x2, double *y2)
{
    cairo_path_bounder_t bounder;
    cairo_status_t status;

    _cairo_path_bounder_init (&bounder);

    status = _cairo_path_fixed_interpret (path, CAIRO_DIRECTION_FORWARD,
					  _cairo_path_bounder_move_to,
					  _cairo_path_bounder_line_to,
					  _cairo_path_bounder_curve_to,
					  _cairo_path_bounder_close_path,
					  &bounder);
    assert (status == CAIRO_STATUS_SUCCESS);

    if (bounder.has_point) {
	*x1 = _cairo_fixed_to_double (bounder.extents.p1.x);
	*y1 = _cairo_fixed_to_double (bounder.extents.p1.y);
	*x2 = _cairo_fixed_to_double (bounder.extents.p2.x);
	*y2 = _cairo_fixed_to_double (bounder.extents.p2.y);
    } else {
	*x1 = 0.0;
	*y1 = 0.0;
	*x2 = 0.0;
	*y2 = 0.0;
    }

    _cairo_path_bounder_fini (&bounder);
}
