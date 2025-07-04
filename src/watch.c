/* GNUPLOT - watch.c */

/*[
 * Copyright Ethan A Merritt 2019-2022
 *
 * Gnuplot license:
 *
 * Permission to use, copy, and distribute this software and its
 * documentation for any purpose with or without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.
 *
 * Permission to modify the software is granted, but not the right to
 * distribute the complete modified source code.  Modifications are to
 * be distributed as patches to the released version.  Permission to
 * distribute binaries produced by compiling modified sources is granted,
 * provided you
 *   1. distribute the corresponding source modifications from the
 *    released version in the form of a patch file along with the binaries,
 *   2. add special version identification to distinguish your version
 *    in addition to the base release version number,
 *   3. provide your name and address as the primary contact for the
 *    support of your modified version, and
 *   4. retain our contact information in regard to use of the base
 *    software.
 * Permission to distribute the released version of the source code along
 * with corresponding source modifications in the form of a patch file is
 * granted with same provisions 2 through 4 for binary distributions.
 *
 * This software is provided "as is" without express or implied warranty
 * to the extent permitted by applicable law.
 *
 * Alternative license:
 *
 * As an alternative to distributing code in this file under the gnuplot license,
 * you may instead comply with the terms below. In this case, redistribution and
 * use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.  Redistributions in binary
 * form must reproduce the above copyright notice, this list of conditions and
 * the following disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
]*/

/*
 * This file supports a "watch" variant of the "with lines" plotting style.
 * When executing a command "plot ... watch <target>" each line segment of
 * the resulting plot is checked to see if it satisfies one of the trigger
 * conditions.  If so, the [x,y] coordinates that satisfy the condition are
 * considered a "hit".
 *
 * These targets are supported:
 *	x = <value>
 *	y = <value>
 *	z = <value>
 *	func() = <value>
 *	mouse
 * z and func(x,y) are roughly equivalent in functionality, but providing
 * the function in principle allows reevaluation during interpolation
 * so that the reported target point is more accurate (not yet implemented).
 *
 * "plot FOO watch mouse" triggers when the plot crosses the current mouse
 * x position. This allows dynamic readout of the plot y values as the mouse moves.
 * It is ignored if mousing is not active in the current terminal.
 *
 * Watch hits are stored in two forms.
 *
 * (1) An array WATCH_N is created to hold the coordinates of hits that
 * trigger from watch N. A hit is generated whenever the sign of
 * (<current_value> - <target_value>) differs at the endpoints of a line segment.
 * By the intermediate value theorem there is some point on the line segment
 * at which the target value is reached.  We approximate that point by linear
 * interpolation.  If the plot is a function  plot (as opposed to a data plot) 
 * the function itself is used to further improve this approximation by iterative
 * bisection.
 * The x and y coordinates of each hit are packed into a single complex value.
 * Thus the coordinates of hit 2 from watch N may be accessed as
 *      hit_x = real(WATCH_N[2])
 *      hit_y = imag(WATCH_N[2])
 *
 * (2) The coordinates of each hit are formatted as a label and stored in
 * a list of labels attached to that plot.  The format of these labels
 * can be customized by "set style watchpoint labels <label-options>".
 *
 * Example:
 *	set xrange [-10:10]
 *	plot x**2 with lines watch x=1.2 watch y=81.0
 *	print WATCH_1
 *	    [{1.2,1.44}]
 *	print WATCH_2
 *	    [{-9.0,81.0},{9.0,81.0}]
 *      # plot function together with previously found hits
 *	plot x**2 with lines, WATCH_2 using 2:3 with points
 *	# plot function with hits labeled as they are found
 *	set style watchpoints labels
 *	plot x**2 with lines watch x=1.2 watch y=81.0
 *
 * Notes:
 * - If data smoothing options are in effect, the interpolation is done
 *   along a line segment from the smoothed curve, _not_ interpolation between
 *   two data points.
 * - WATCH_* arrays are not created for "watch mouse" hits
 * 
 */

#include "alloc.h"
#include "command.h"	/* for c_token */
#include "eval.h"
#include "gp_time.h"
#include "graphics.h"
#include "graph3d.h"
#include "mouse.h"
#include "plot2d.h"
#include "plot3d.h"
#include "watch.h"
#include "setshow.h"	/* for parse_label_options */
#include "save.h"	/* for save_label_style */

/* Exported variables */

int number_of_watches = 0;
TBOOLEAN watch_mouse_active = FALSE;
struct text_label watchpoint_labelstyle;

#define EPS 3.0E-8

#ifdef USE_WATCHPOINTS

/*
 * Local prototypes
 */

static struct text_label *mouse_hit_label(
			watch_t *target, double x, double y, double z);
static char *apply_tic_format( struct axis *axis, double hit );
static void show_watchlist( struct curve_points *this_plot, struct watch_t *watchlist );

/*
 * Default label style is the one used for "watch mouse".
 */
static struct text_label default_labelstyle = {
	.next = NULL,
	.tag = LABEL_TAG_WATCH_MOUSE,
	.place = {first_axes, first_axes, first_axes, 0.0, 0.0, 0.0},
	.pos = LEFT,
	.rotate = 0,
	.layer = LAYER_PLOTLABELS,
	.boxed = -1,
	.text = NULL,
	.font = NULL,
	.textcolor = BLACK_COLORSPEC,
	.lp_properties = DEFAULT_LP_STYLE_TYPE,
	.offset = {character, character, character, 0.0, 0.0, 0.0},
	.noenhanced = FALSE,
	.hypertext = FALSE,
	.hidden = TRUE
};
static struct position default_mouse_offset =
	{character, character, character, 0.5, 0.5, 0.0};


/*
 * Analogous to draw_clip_line() except that instead of drawing
 * we check to see if this line segment triggers a watch condition.
 */
void
watch_line(struct curve_points *plot, double x1, double y1, double z1, double x2, double y2, double z2)
{
    double hit_x, hit_y, hit_z;
    struct udvt_entry *array;
    static char array_name[12];
    watch_t *watch;
    struct value f;
    double f1, f2;
    struct value *v;

    if (polar)	/* Watchpoints not yet supported for polar plots */
	return;

    for (watch = plot->watchlist; watch != NULL; watch = watch->next) {

	switch (watch->type) {
#ifdef USE_MOUSE
	case MOUSE_PROXY_AXIS:
		{
		/* Replace watch->target with current mouse x position */
		double mouse_x, mouse_y;
		get_last_mouse_xy(&mouse_x, &mouse_y);
		if (!inrange(mouse_x, axis_array[x_axis].min, axis_array[x_axis].max))
		    continue;
		if (!inrange(mouse_x, x1, x2))
		    continue;
		/* No bisection, because function parameters may have changed
		 * between when the plot was drawn and the time of mouse interaction
		 */
		watch->target = mouse_x;
		hit_x = mouse_x;
		hit_y = y1 + (y2 - y1) * (hit_x - x1)/(x2 - x1);
		hit_z = z1 + (z2 - z1) * (hit_x - x1)/(x2 - x1);
		break;
		}
#endif
	case FIRST_X_AXIS:
		/* outrange line segment does not trigger a watch event */
		if (!inrange(watch->target, x1, x2))
		    continue;
		hit_x = watch->target;
		if (plot->plot_type == FUNC && !parametric) {
		    /* Evaluate function exactly at target x */
		    struct value a;
		    Gcomplex( &plot->plot_function.dummy_values[0], hit_x, 0 );
		    evaluate_at(plot->plot_function.at, &a);
		    hit_y = real(&a);
		} else {
		    /* Interpolate y at intersection with target x value */
		    hit_y = y1 + (y2 - y1) * (watch->target - x1)/(x2 - x1);
		}
		hit_z = z1 + (z2 - z1) * (watch->target - x1)/(x2 - x1);
		break;
	case FIRST_Y_AXIS:
		/* outrange line segment does not trigger a watch event */
		if (!inrange(watch->target, y1, y2))
		    continue;
		/* Interpolate x at intersection with target y value */
		hit_y = watch->target;
		hit_x = x1 + (x2 - x1) * (watch->target - y1)/(y2 - y1);
		if (plot->plot_type == FUNC && !parametric) {
		    /* Improve x value by successive approximation */
		    bisect_hit(plot, BISECT_MATCH, &hit_x, &hit_y, x1, x2);
		}
		hit_z = z1 + (z2 - z1) * (watch->target - y1)/(y2 - y1);
		break;
	case FIRST_Z_AXIS:
		if (!inrange(watch->target, z1, z2))
		    continue;
		/* interpolate [x,y] at intersection with z target */
		hit_x = x1 + (x2 - x1) * (watch->target - z1)/(z2 - z1);
		hit_y = y1 + (y2 - y1) * (watch->target - z1)/(z2 - z1);
		hit_z = watch->target;
		break;
	case SAMPLE_AXIS:
		{
		/* FIXME could share save/restore xyz code with mouse_label_hit() */
		struct udvt_entry *x_udv = add_udv_by_name("x");
		struct udvt_entry *y_udv = add_udv_by_name("y");
		struct udvt_entry *z_udv = add_udv_by_name("z");
		t_value original_x = x_udv->udv_value;
		t_value original_y = y_udv->udv_value;
		t_value original_z = z_udv->udv_value;

		Gcomplex(&x_udv->udv_value, x1, 0);
		Gcomplex(&y_udv->udv_value, y1, 0);
		Gcomplex(&z_udv->udv_value, z1, 0);
		evaluate_at(watch->function_at, &f);
		f1 = real(&f);

		Gcomplex(&x_udv->udv_value, x2, 0);
		Gcomplex(&y_udv->udv_value, y2, 0);
		Gcomplex(&z_udv->udv_value, z2, 0);
		evaluate_at(watch->function_at, &f);
		f2 = real(&f);

		x_udv->udv_value = original_x;
		y_udv->udv_value = original_y;
		z_udv->udv_value = original_z;

		/* outrange line segment does not trigger a watch event */
		if (!inrange(watch->target, f1, f2))
		    continue;

		/* interpolate [x,y] at intersection with target f(x,y) */
		/* FIXME: could do better by bisection with reevaluation */
		hit_x = x1 + (x2 - x1) * (watch->target - f1)/(f2 - f1);
		hit_y = y1 + (y2 - y1) * (watch->target - f1)/(f2 - f1);
		hit_z = z1 + (z2 - z1) * (watch->target - f1)/(f2 - f1);
		break;
		}
	default:
		continue;
		break;
	}

	/* The unclipped line segment spans the target watch value but it might be
	 * outside the borders of the plot.
	 * Check the coordinates against the axis ranges before accepting it.
	 */
	if (!inrange(hit_x, axis_array[FIRST_X_AXIS].min, axis_array[FIRST_X_AXIS].max))
	    continue;
	if (!inrange(hit_y, axis_array[y_axis].min, axis_array[y_axis].max))
	    continue;

	/* The bookkeeping for mouse-tracking is minimal.
	 * We accummulate hits in a list of labels and print them afterwards.
	 * No WATCH variables are set.
	 */
	if (watch->type == MOUSE_PROXY_AXIS) {
	    struct text_label *label;
	    label = mouse_hit_label(watch, hit_x, hit_y, hit_z);
	    label->next = plot->labels;
	    plot->labels = label;
	    continue;
	}

	/* For all other watch target types we update the list of hits */
	sprintf(array_name, "WATCH_%d", watch->watchno);
	array = get_udv_by_name(array_name);
	if (!array || array->udv_value.type != ARRAY)
	    int_error(NO_CARET, "%s is not an array", array_name);

	/* If a hit falls exactly on a line segment endpoint it can trigger on
	 * both of the successive segments. In that case ignore the second hit.
	 */
	v = array->udv_value.v.value_array;
	if (watch->hits > 0 && v[0].v.array_header.size > 0) {
	    if ((fabs(hit_x - v[watch->hits].v.cmplx_val.real) < EPS)
	    &&  (fabs(hit_y - v[watch->hits].v.cmplx_val.imag) < EPS))
		continue;
	}

	/* Keep a label list for non-mouse targets also but labels are displayed
	 * only if "set style watchpoints label" or requested in the plot command.
	 */
	if (TRUE) {
	    struct text_label *label;
	    label = mouse_hit_label(watch, hit_x, hit_y, hit_z);
	    label->next = plot->labels;
	    plot->labels = label;
	    label->hidden = watchpoint_labelstyle.hidden;
	    if (watch->label_at != NULL)
		label->hidden = FALSE;
	}

	/* Load hit in array WATCH_n */
	watch->hits++;

	array->udv_value.v.value_array
	    = gp_realloc(array->udv_value.v.value_array,
			    (watch->hits+1) * sizeof(t_value), NULL);
	array->udv_value.v.value_array[0].v.array_header.size = watch->hits;
	Gcomplex(&array->udv_value.v.value_array[watch->hits], hit_x, hit_y);

	/* Not always wanted; should be configurable */
	FPRINTF((stderr, "watch %d hit %d at %g %g\n", watch->watchno, watch->hits, hit_x, hit_y));

    } /* End loop over active watchs */

    return;
}

/*
 * parse watch component of plot command
 */
void
parse_watch(struct curve_points *plot)
{
    struct watch_t *new_watch, *this_watch;
    struct watch_t temp_watch;

    c_token++;

    /* Build up the watch structure here, then copy it to plot header */
    new_watch = &temp_watch;
    memset(new_watch, 0, sizeof(struct watch_t));

    if (equals(c_token, "x")) {
	if (!equals(++c_token, "="))
	    int_error(c_token, "expecting x=<value>");
	c_token++;
	new_watch->target = real_expression();
	new_watch->type = FIRST_X_AXIS;

    } else if (equals(c_token, "y")) {
	if (!equals(++c_token, "="))
	    int_error(c_token, "expecting y=<value>");
	c_token++;
	new_watch->target = real_expression();
	new_watch->type = FIRST_Y_AXIS;
    } else if (equals(c_token, "z")) {
	if (!equals(++c_token, "="))
	    int_error(c_token, "expecting z=<value>");
	c_token++;
	new_watch->target = real_expression();
	new_watch->type = FIRST_Z_AXIS;
    } else if (equals(c_token, "mouse")) {
	c_token++;
#ifdef USE_MOUSE
	/* Ignore this request if no mousing is active */
	if ((mouse_setting.on == 0) || (term->waitforinput == NULL))
#endif
	    return;
	new_watch->type = MOUSE_PROXY_AXIS;
	watch_mouse_active = TRUE;
    } else if ((get_udf_by_token(c_token) != NULL)
	   ||  (( equals(c_token,"$") && equals(c_token+2, "("))) ) {
	/* Function or functionblock */
	new_watch->function_at = perm_at();
	if (equals(c_token++, "="))
	    new_watch->target = real_expression();
	else
	    int_error(c_token, "expecting f()=<value>");
	new_watch->type = SAMPLE_AXIS;
    } else
	int_error(NO_CARET, "undefined function or unrecognized watch request");

    /* label can be generated at the time of a hit by a string-valued function */
    if (equals(c_token, "label")) {
	c_token++;
	new_watch->label_at = perm_at();
    }

    /* Watchpoints not yet supported for polar plots */
    if (polar) {
	int_warn(NO_CARET, "watchpoints ignored in polar mode");
	return;
    }

    /* New watch accepted; make a copy for the plot header */
    new_watch = gp_alloc(sizeof(struct watch_t), "new watch");
    memcpy(new_watch, &temp_watch, sizeof(struct watch_t));

    /* Watch index runs over all elements of this "plot" command */
    number_of_watches++;
    new_watch->watchno = number_of_watches;

    if (plot->watchlist == NULL)
	plot->watchlist = new_watch;
    else {
	this_watch = plot->watchlist;
	while (this_watch->next != NULL)
	    this_watch = this_watch->next;
	this_watch->next = new_watch;
    }
}

void
free_watchlist(struct watch_t *watchlist)
{
    struct watch_t *temp;

    while (watchlist) {
	temp = watchlist;
	watchlist = watchlist->next;
	if (temp->function_at)
	    free_at(temp->function_at);
	if (temp->label_at)
	    free_at(temp->label_at);
	free(temp);
    }
}

/*
 * unset_watchpoint_style - called on program start, unset and reset commands
 *	returns the styling to default
 * reset_watches - called at the start of a plot command
 *	resets count of active watches to 0
 * init_watch called at the start of each plot being watched
 *	resets hit count to zero
 */
void
unset_watchpoint_style()
{
    free(watchpoint_labelstyle.font);
    memcpy(&watchpoint_labelstyle, &default_labelstyle, sizeof(struct text_label));
    watch_mouse_active = FALSE;
}

void
reset_watches()
{
    number_of_watches = 0;
    watch_mouse_active = FALSE;
}

void
init_watch(struct curve_points *plot)
{
    watch_t *watch;
    char array_name[12];
    udvt_entry *array;

    for (watch = plot->watchlist; watch; watch = watch->next) {
	sprintf(array_name, "WATCH_%d", watch->watchno);
	array = add_udv_by_name(array_name);
	init_array(array, 0);
	watch->hits = 0;
    }
}

/*
 * Construct a label anchored at a hit from "watch mouse".
 * The idea is to accummulate the labels (there could be multiple hits)
 * in plot->labels and print them all afterwards.
 */
static struct text_label *
mouse_hit_label(watch_t *target, double x, double y, double z)
{
    char *xlabel, *ylabel;
    static char buffer[256];
    struct text_label *new_label = gp_alloc( sizeof(struct text_label), "watch label" );
    memcpy(new_label, &watchpoint_labelstyle, sizeof(struct text_label));

    switch (target->type) {
	case FIRST_X_AXIS:
			new_label->tag = LABEL_TAG_WATCH_X; break;
	case FIRST_Y_AXIS:
			new_label->tag = LABEL_TAG_WATCH_Y; break;
	case FIRST_Z_AXIS:
			new_label->tag = LABEL_TAG_WATCH_Z; break;
	case SAMPLE_AXIS:
			new_label->tag = LABEL_TAG_WATCH_FUNCTION; break;
	case MOUSE_PROXY_AXIS:
			new_label->tag = LABEL_TAG_WATCH_MOUSE;
			new_label->hidden = FALSE;
			new_label->lp_properties.flags = LP_SHOW_POINTS;
			new_label->offset = default_mouse_offset;
			break;
	default:	
			int_error(NO_CARET, "unknown watch target");
    }

    new_label->place.x = x;
    new_label->place.y = y;

    if (target->label_at) {
	/* save current user variables x,y,z
	 * load hit coordinates as x,y,z
	 * generate label from a function that may use x,y,z
	 * restore original values.
	 */
	struct value a;
	struct udvt_entry *x_udv = add_udv_by_name("x");
	struct udvt_entry *y_udv = add_udv_by_name("y");
	struct udvt_entry *z_udv = add_udv_by_name("z");
	t_value original_x = x_udv->udv_value;
	t_value original_y = y_udv->udv_value;
	t_value original_z = z_udv->udv_value;
	Gcomplex(&x_udv->udv_value, x, 0);
	Gcomplex(&y_udv->udv_value, y, 0);
	Gcomplex(&z_udv->udv_value, z, 0);

	evaluate_at(target->label_at, &a);

	x_udv->udv_value = original_x;
	y_udv->udv_value = original_y;
	z_udv->udv_value = original_z;

	if (a.type == STRING)
	    new_label->text = strdup(a.v.string_val);
	else
	    new_label->text = strdup("⬚");
	free_value(&a);
    } else {
	xlabel = strdup( apply_tic_format( &axis_array[x_axis], x));
	ylabel = strdup( apply_tic_format( &axis_array[y_axis], y));
	sprintf(buffer, "%s : %s", xlabel, ylabel);
	new_label->text = strdup(buffer);
	free(xlabel);
	free(ylabel);
    }

    return new_label;
}

/* Apply axis ticfmt to generate a partial label.
 * The caller should copy the returned string immediately,
 * since the next call to this routine will overwrite it.
 * - assumes axis log base is 10.
 * - default axis tic fmt "% h" is probably not approprite here;
 *   substitute "%.3h" in that case
 */
static char *
apply_tic_format( struct axis *axis, double hit )
{
#   define MAX_LABEL_SIZE 127
    static char buffer[MAX_LABEL_SIZE+1];

    if (axis->ticfmt == NULL)
	axis->ticfmt = copy_or_invent_formatstring(axis);

    if (axis->tictype == DT_TIMEDATE)
	gstrftime(buffer, MAX_LABEL_SIZE, axis->ticfmt, hit);
    else if (axis->tictype == DT_DMS)
	gstrdms(buffer, axis->ticfmt, hit);
    else if (is_def_format(axis->ticfmt))
	gprintf(buffer, MAX_LABEL_SIZE, "%.3h", 1.0, hit);
    else
	gprintf(buffer, MAX_LABEL_SIZE, axis->ticfmt, 1.0, hit);

    return buffer;
#   undef MAX_LABEL_SIZE
}


/*
 * "set style watchpoint"
 */
void
set_style_watchpoint()
{
    c_token++;

    /* The only settings supported at this point are related to the labels
     * added to a plot with watchpoints set.
     */
    if (almost_equals(c_token, "nolabel$s")) {
	c_token++;
	watchpoint_labelstyle.hidden = TRUE;

    } else if (almost_equals(c_token, "label$s")) {
	c_token++;
	watchpoint_labelstyle.hidden = FALSE;
	if (equals(c_token, "default")) {
	    memcpy(&watchpoint_labelstyle, &default_labelstyle, sizeof(struct text_label));
	    c_token++;
	} else {
	    parse_label_options(&watchpoint_labelstyle, 0);
	    watchpoint_labelstyle.layer = LAYER_PLOTLABELS;
	    if ((watchpoint_labelstyle.lp_properties.flags & LP_SHOW_POINTS) == 0)
		watchpoint_labelstyle.lp_properties.p_type = -1;
	}

    } else {
	int_error(c_token, "Expecting 'set style watchpoint labels <label-options>'");
    }
}

void
save_style_watchpoint(FILE *fp)
{
    if (watchpoint_labelstyle.hidden)
	fprintf(fp, "set style watchpoint nolabels\n");
    else {
	fprintf(fp, "set style watchpoint label ");
	save_label_style(fp, &watchpoint_labelstyle);
	fprintf(fp, "\n");
    }
}

void
show_style_watchpoint()
{
    fputs("\t",stderr);
    save_style_watchpoint(stderr);
}

/*
 * Walk through previous plot command and print results of watchpoints
 * to console.
 */
void
show_watchpoints()
{
    if (is_3d_plot) {
	for (struct surface_points *this_plot = first_3dplot;
		    this_plot; this_plot = this_plot->next_sp) {
	    if (!this_plot->watchlist)
		continue;
	    fprintf(stderr, "Plot title:\t%s\n", this_plot->title ? this_plot->title : "(none)");
	    show_watchlist((struct curve_points *)this_plot, this_plot->watchlist);
	}
    return;
    }

    for (struct curve_points *this_plot = first_plot;
		this_plot; this_plot = this_plot->next) {
	if (!this_plot->watchlist)
	    continue;
	fprintf(stderr, "Plot title:\t%s\n", this_plot->title ? this_plot->title : "(none)");
	show_watchlist(this_plot, this_plot->watchlist);
    }
}


static void
show_watchlist( struct curve_points *this_plot, struct watch_t *watchlist )
{
    struct watch_t *this_watch;
    struct udvt_entry *array;
    char array_name[12];
    int hits;

    /* Loop over watchpoints attached to this plot */
    for (this_watch = watchlist; this_watch; this_watch = this_watch->next) {
	const char *type;
	int i;

	if (this_watch->type == MOUSE_PROXY_AXIS) {
	    fprintf(stderr, "\tWatch %d target mouse\n", this_watch->watchno);
	    continue;
	}
	type =  this_watch->type == FIRST_X_AXIS ? "x" :
		this_watch->type == FIRST_Y_AXIS ? "y" :
		this_watch->type == FIRST_Z_AXIS ? "z" :
		this_watch->type == SAMPLE_AXIS ? "func()" : NULL;
	if (!type)
	    continue;
	fprintf(stderr, "\tWatch %d target ", this_watch->watchno);
	fprintf(stderr, "%s = %.4g ", type, this_watch->target);
	fprintf(stderr, "\t(%d hits)\n", this_watch->hits);

	/* Loop over hits stored in the corresponding array */
	sprintf(array_name, "WATCH_%d", this_watch->watchno);
	array = get_udv_by_name(array_name);
	if (!array || array->udv_value.type != ARRAY)
	    int_error(NO_CARET, "error: cannot find array %s", array_name);
	hits = array->udv_value.v.value_array[0].v.array_header.size;
	if (hits != this_watch->hits)
	    int_error(NO_CARET, "error: wrong number of hits in %s", array_name);

	for (i = 1; i <= hits; i++) {
	    double x = array->udv_value.v.value_array[i].v.cmplx_val.real;
	    double y = array->udv_value.v.value_array[i].v.cmplx_val.imag;
	    char *xlabel, *ylabel;
	    if (this_plot->plot_type == FUNC || this_plot->plot_type == DATA) {
		/* 2D plots might watch any of x1 x2 y1 y2 */
		xlabel = strdup( apply_tic_format( &axis_array[this_plot->x_axis], x));
		ylabel = strdup( apply_tic_format( &axis_array[this_plot->y_axis], y));
	    } else {
		/* 3D plots do not have a x2 or y2 axis */
		xlabel = strdup( apply_tic_format( &axis_array[FIRST_X_AXIS], x));
		ylabel = strdup( apply_tic_format( &axis_array[FIRST_Y_AXIS], y));
	    }
	    fprintf(stderr, "\t\thit %d\tx %s  y %s\n", i, xlabel, ylabel);
	    free(xlabel);
	    free(ylabel);
	}
    }
}

#endif	/* USE_WATCHPOINTS */

/*
 * The following routine is also needed by the sharpen filter.
 */

/*
 * Improve initial linear approximation of xhit by
 * golden ratio bisection of the original interval.
 */
#define PHIM1 0.618034
void
bisect_hit(struct curve_points *plot, t_bisection_target target,
	   double *xhit, double *yhit, double xlow, double xhigh)
{
    double x0, x1, x2, x3;
    double f1, f2;		/* f1 = f(x1),  f2 = f(x2) */
    struct value a;
    TBOOLEAN left;

    double epsilon = (target == BISECT_MATCH) ? EPS : 1.e-14;

    x0 = xlow;
    x3 = xhigh;
    if (fabs(xhigh - *xhit) > fabs(*xhit - xlow)) {
	x1 = *xhit;
	x2 = *xhit + (1.0-PHIM1) * (xhigh - *xhit);
    } else {
	x2 = *xhit;
	x1 = *xhit - (1.0-PHIM1) * (*xhit - xlow);
    }

    Gcomplex( &(plot->plot_function.dummy_values[0]), x1, 0 );
    evaluate_at(plot->plot_function.at, &a);
    f1 = real(&a);
    Gcomplex( &(plot->plot_function.dummy_values[0]), x2, 0 );
    evaluate_at(plot->plot_function.at, &a);
    f2 = real(&a);

    /* Iterate until convergence of estimated xhit */
    while (fabs(x3-x0) > epsilon * (fabs(x1) + fabs(x2))) {

	if (target == BISECT_MINIMIZE)
	    left = (f2 < f1);
	else if (target == BISECT_MAXIMIZE)
	    left = (f2 > f1);
	else /* minimize difference of f(x) from *yhit */
	    left = (fabs(f2 - *yhit) < fabs(f1 - *yhit));

	if (left) {
	    x0 = x1;
	    x1 = x2;
	    x2 = PHIM1*x2 + (1.0-PHIM1)*x3;
	    f1 = f2;
	    Gcomplex( &(plot->plot_function.dummy_values[0]), x2, 0 );
	    evaluate_at(plot->plot_function.at, &a);
	    f2 = real(&a);
	} else {
	    x3 = x2;
	    x2 = x1;
	    x1 = PHIM1*x1 + (1.0-PHIM1)*x0;
	    f2 = f1;
	    Gcomplex( &(plot->plot_function.dummy_values[0]), x1, 0 );
	    evaluate_at(plot->plot_function.at, &a);
	    f1 = real(&a);
	}

	if ((fabs(x1) + fabs(x2)) < epsilon)
	    break;
    }

    /* Update and return hit coordinates */
    if (target == BISECT_MINIMIZE)
	left = (f2 < f1);
    else if (target == BISECT_MAXIMIZE)
	left = (f2 > f1);
    else /* minimize difference of f(x) from *yhit */
	left = (fabs(f2 - *yhit) < fabs(f1 - *yhit));

    if (left) {
	*xhit = x2;
	*yhit = f2;
    } else {
	*xhit = x1;
	*yhit = f1;
    }
}
