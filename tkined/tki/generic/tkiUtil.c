/* 
 * tkiUtils.c --
 *
 *	This file contains some utilities used to implement the
 *	application specific canvas item types.
 *
 * Copyright (c) 1995-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <tk.h>
#include "tkined.h"

void
TkiMarkRectangle(display, drawable, gc, x1, y1, x2, y2)
    Display *display;                   /* Display on which to draw item. */  
    Drawable drawable;			/* Pixmap or window in which to draw
					 * item. */
    GC gc;				/* Graphics context for filling
					 * the rectangles. */
    int x1, y1, x2, y2;			/* Describes region of rectangle that
                                         * should get selection marks. */
{
    XRectangle rects[8];
    int xm, ym, n = 4;

    x1 += 3; y1 +=3; x2 -= 3; y2 -= 3;
    xm = (x1+x2)/2; ym = (y1+y2)/2;

    rects[0].x = x1-3;
    rects[0].y = y1-3;
    rects[0].width = 2;
    rects[0].height = 2;
    rects[1].x = x2+1;
    rects[1].y = y1-3;
    rects[1].width = 2;
    rects[1].height = 2;
    rects[2].x = x1-3;
    rects[2].y = y2+1;
    rects[2].width = 2;
    rects[2].height = 2;
    rects[3].x = x2+1;
    rects[3].y = y2+1;
    rects[3].width = 2;
    rects[3].height = 2;

    if ((x2-x1) > 100) {
        rects[n].x = xm-1;
	rects[n].y = y1-3;
	rects[n].width = 2;
	rects[n].height = 2;
	n++;
	rects[n].x = xm+1;
	rects[n].y = y2+1;
	rects[n].width = 2;
	rects[n].height = 2;
	n++;
    }

    if ((y2-y1) > 100) {
        rects[n].x = x1-3;
	rects[n].y = ym-1;
	rects[n].width = 2;
	rects[n].height = 2;
	n++;
	rects[n].x = x2+1;
	rects[n].y = ym-1;
	rects[n].width = 2;
	rects[n].height = 2;
	n++;
    }

    XFillRectangles(display, drawable, gc, rects, n);
}

