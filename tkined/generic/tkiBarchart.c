/*
 * tkiBarchart.c --
 *
 *	This file implements barchart items for canvas widgets.
 *      It's based on tkRectOval.c and tkCanvPoly.c from the 
 *      tk distribution by John Ousterhout.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2001 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <tk.h>
#include "tkined.h"

/*
 * The structure below defines the record for each barchart item.
 */

typedef struct BarchartItem {
    Tk_Item header;             /* Generic stuff that's the same for all
                                 * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_Canvas canvas;           /* Canvas containing item. Needed for
                                 * parsing barchart values. */
    Tcl_Interp *interp;		/* Interpreter handle. */
    int numPoints;              /* Number of values in barchart. */
    double *valuePtr;           /* Pointer to malloc-ed array containing 
				 * the original values. */
    double *coordPtr;           /* Pointer to malloc-ed array containing
                                 * the computed coordinates. */
    int scale;                  /* Actual scale of the barchart. */
    double scaleValue;          /* Every scalevalue we scale up. */  
    double bbox[4];             /* Coordinates of bounding box for rectangle
                                 * (x1, y1, x2, y2). */
    XColor *color;              /* Color for barchart bars. */
    GC fillGC;                  /* Graphics context for filling barchart. */
    int autocolor;              /* Boolean that indicates whether autocolormode
				 * is on or off. */
    XColor *rectColor;          /* Background color of the barchart. */
    GC rectGC;                  /* Graphics context of the rectangel around 
				 * the barchart. */
    XColor *outlineColor;       /* Color of the rectangular outline 
				 * around the barchart. */
    GC outlineGC;               /* Graphics context of the outline around 
				 * the barchart. */
    XColor *barlineColor;       /* Color of the outline of the bars. */
    GC barlineGC;               /* Graphics context for outline of
				 * the bars. */
    XColor *scalelineColor;     /* Color of scaleline. */
    int scalelineStyle;         /* Style of scaleline, 0 or negativ means 
				 * LineSolid, each other positive value 
				 * means the length of dashes using 
				 * LineOnOffDash. */
    GC scalelineGC;             /* Graphics context for scaleline. */
    int selected;		/* True if the item is selected. */
} BarchartItem;

/*
 * Prototypes for procedures defined in this file:
 */

static void             ComputeBarchartBbox (
                            Tk_Canvas canvas, BarchartItem *barPtr);
static int              ConfigureBarchart (Tcl_Interp *interp,
                            Tk_Canvas canvas, Tk_Item *itemPtr, int objc,
                            Tcl_Obj *CONST objv[], int flags);
static int              CreateBarchart (Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, 
                            int objc, Tcl_Obj *CONST objv[]);
static void             DeleteBarchart (Tk_Canvas canvas,
                            Tk_Item *itemPtr, Display *display);
static void             DisplayBarchart (Tk_Canvas canvas,
                            Tk_Item *itemPtr, Display *display, Drawable dst,
			    int x, int y, int width, int height);
static int              BarchartCoords (Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr,
                            int objc, Tcl_Obj *CONST objv[]);
static int              BarchartToArea (Tk_Canvas canvas,
                            Tk_Item *itemPtr, double *rectPtr);
static double           BarchartToPoint (Tk_Canvas canvas,
                            Tk_Item *itemPtr, double *pointPtr);
static int              BarchartToPostscript (Tcl_Interp *interp,
                            Tk_Canvas canvas, Tk_Item *itemPtr, int prepass);
static int              ParseBarchartValues (ClientData 
                            clientData, Tcl_Interp *interp, Tk_Window tkwin, 
                            char *value, char *recordPtr, int offset);
static char *           PrintBarchartValues (ClientData 
                            clientData, Tk_Window tkwin, char *recordPtr, 
                            int offset, Tcl_FreeProc **freeProcPtr);
static int              BarchartValues (Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, 
                            int argc, char **argv, int numPoints);
static void             ScaleBarchart (Tk_Canvas canvas,
                            Tk_Item *itemPtr, double originX, double originY,
                            double scaleX, double scaleY);
static void             TranslateBarchart (Tk_Canvas canvas,
                            Tk_Item *itemPtr, double deltaX, double deltaY);

/*
 * Information used for parsing configuration specs:
 */

#ifdef __WIN32__
static void
dummy()
{
    Tk_CanvasTagsParseProc((ClientData) NULL, (Tcl_Interp *) NULL,
			   (Tk_Window) NULL, "", "", 0);
    Tk_CanvasTagsPrintProc((ClientData) NULL, (Tk_Window) NULL,
			   "", 0, NULL);
}
#endif

static Tk_CustomOption tagsOption = {NULL,
    NULL, (ClientData) NULL
};
static Tk_CustomOption valueOption = {
    (Tk_OptionParseProc *) ParseBarchartValues,
    (Tk_OptionPrintProc *) PrintBarchartValues,
    (ClientData) NULL
};

static Tk_ConfigSpec configSpecs[] = {  
    {TK_CONFIG_BOOLEAN, "-autocolor", (char *) NULL, (char *) NULL,
        "0", Tk_Offset(BarchartItem, autocolor), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_COLOR, "-background", (char *) NULL, (char *) NULL,
        (char *) NULL, Tk_Offset(BarchartItem, rectColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-barline", (char *) NULL, (char *) NULL,
        "black", Tk_Offset(BarchartItem, barlineColor), 
        TK_CONFIG_NULL_OK},   
    {TK_CONFIG_COLOR, "-fill", (char *) NULL, (char *) NULL,
        (char *) NULL, Tk_Offset(BarchartItem, color), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-outline", (char *) NULL, (char *) NULL,
        "black", Tk_Offset(BarchartItem, outlineColor), 
        TK_CONFIG_NULL_OK},   
    {TK_CONFIG_COLOR, "-scaleline", (char *) NULL, (char *) NULL,
        "black", Tk_Offset(BarchartItem, scalelineColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_INT, "-scalelinestyle", (char *) NULL, (char *) NULL,
        "4", Tk_Offset(BarchartItem, scalelineStyle),
        TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_DOUBLE, "-scalevalue", (char *) NULL, (char *) NULL,
        "100.0", Tk_Offset(BarchartItem, scaleValue),
        TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
        (char *) NULL, 0, TK_CONFIG_NULL_OK, &tagsOption},
    {TK_CONFIG_CUSTOM, "-values", (char *) NULL, (char *) NULL,
        (char *) NULL, 0, TK_CONFIG_DONT_SET_DEFAULT, &valueOption},
    {TK_CONFIG_BOOLEAN, "-selected", (char *) NULL, (char *) NULL,
	"0", Tk_Offset(BarchartItem, selected), TK_CONFIG_NULL_OK},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
        (char *) NULL, 0, 0}
};


/*
 * The structures below defines the barchart item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_ItemType TkBarchartType = {
    "barchart",                       /* name */
    sizeof(BarchartItem),             /* itemSize */
    CreateBarchart,                   /* createProc */
    configSpecs,                      /* configSpecs */
    ConfigureBarchart,                /* configureProc */
    BarchartCoords,                   /* coordProc */
    DeleteBarchart,                   /* deleteProc */
    DisplayBarchart,                  /* displayProc */
    TK_CONFIG_OBJS,                   /* flags */
    BarchartToPoint,                  /* pointProc */
    BarchartToArea,                   /* areaProc */
    BarchartToPostscript,             /* postscriptProc */
    ScaleBarchart,                    /* scaleProc */
    TranslateBarchart,                /* translateProc */
    (Tk_ItemIndexProc *) NULL,        /* indexProc */
    (Tk_ItemCursorProc *) NULL,       /* icursorProc */
    (Tk_ItemSelectionProc *) NULL,    /* selectionProc */
    (Tk_ItemInsertProc *) NULL,       /* insertProc */
    (Tk_ItemDCharsProc *) NULL,       /* dTextProc */
    (Tk_ItemType *) NULL              /* nextPtr */
};

/*
 * The definition below determines how large are static arrays
 * used to hold spline points (splines larger than this have to
 * have their arrays malloc-ed).
 */

#define MAX_STATIC_POINTS 50


/*
 *--------------------------------------------------------------
 *
 * CreateBarchart --
 *
 *	This procedure is invoked to create a new barchart item in
 *	a canvas.
 *
 * Results:
 *	A standard Tcl return value.  If an error occurred in
 *	creating the item, then an error message is left in
 *	canvasPtr->interp->result;  in this case itemPtr is
 *	left uninitialized, so it can be safely freed by the
 *	caller.
 *
 * Side effects:
 *	A new barchart item is created.
 *
 *--------------------------------------------------------------
 */

static int
CreateBarchart(interp, canvas, itemPtr, objc, objv)
    Tcl_Interp *interp;                 /* Interpreter for error reporting. */
    Tk_Canvas canvas;			/* Canvas to hold new item. */
    Tk_Item *itemPtr;			/* Record to hold new item;  header
					 * has been initialized by caller. */
    int objc;				/* Number of arguments in objv. */
    Tcl_Obj *CONST objv[];		/* Arguments describing barchart. */
{
    register BarchartItem *barPtr = (BarchartItem *) itemPtr;
    int i;
    
    if (objc < 4) {
	Tcl_AppendResult(interp, "wrong # args:  should be \"",
		Tk_PathName(Tk_CanvasTkwin(canvas)), "\" create ",
		itemPtr->typePtr->name,
		" x1 y1 x2 y2 ?options?", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Carry out initialization that is needed in order to clean
     * up after errors during the the remainder of this procedure.
     */
    
    barPtr->canvas = canvas;
    barPtr->interp = interp;
    barPtr->numPoints = 0;
    barPtr->coordPtr = NULL;
    barPtr->valuePtr = NULL;
    barPtr->scale = 1;
    barPtr->scaleValue = 100.0;
    barPtr->color = NULL;
    barPtr->fillGC = None;
    barPtr->autocolor = 0;
    barPtr->rectColor = NULL;
    barPtr->rectGC = None;
    barPtr->outlineColor = NULL;
    barPtr->outlineGC = None;
    barPtr->barlineColor = NULL;
    barPtr->barlineGC = None;
    barPtr->scalelineColor = NULL;
    barPtr->scalelineStyle = 4;
    barPtr->scalelineGC = None;
    barPtr->selected = 0;

    /*
     * Count the number of points and then parse them into a point
     * array.  Leading arguments are assumed to be points if they
     * start with a digit or a minus sign followed by a digit.
     */

    for (i = 4; i < objc; i++) {
	char *arg = Tcl_GetStringFromObj((Tcl_Obj *) objv[i], NULL);
	if ((!isdigit((int) arg[0])) &&
	    ((arg[0] != '-') || (!isdigit((int) arg[1])))) {
	    break;
	}
    }

    if (BarchartCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
	goto error;
    }

    if (ConfigureBarchart(interp, canvas, itemPtr, objc-i, objv+i, 0)
           == TCL_OK) {
	return TCL_OK;
    }

    error:
    DeleteBarchart(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * BarchartCoords --
 *
 *	This procedure is invoked to process the "coords" widget
 *	command on barcharts.  See the user documentation for details
 *	on what it does.
 *
 * Results:
 *	Returns TCL_OK or TCL_ERROR, and sets canvasPtr->interp->result.
 *
 * Side effects:
 *	The coordinates for the given item may be changed.
 *
 *--------------------------------------------------------------
 */

static int
BarchartCoords(interp, canvas, itemPtr, objc, objv)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item whose coordinates are to be
					 * read or modified. */
    int objc;				/* Number of coordinates supplied in
					 * objv. */
    Tcl_Obj *CONST objv[];		/* Array of coordinates: x1, y1,
					 * x2, y2, ... */
{
    register BarchartItem *barPtr = (BarchartItem *) itemPtr;
    int i;

    if (objc == 0) {
	Tcl_Obj *objPtr = Tcl_NewObj();
	for (i = 0; i < 4; i++) {
	    Tcl_ListObjAppendElement(interp, objPtr,
				     Tcl_NewDoubleObj(barPtr->bbox[i]));
	}
	Tcl_SetObjResult(interp, objPtr);
	return TCL_OK;
    } else if (objc != 4) {
	Tcl_AppendResult(interp, "wrong # args:  should be \"",
                Tk_PathName(Tk_CanvasTkwin(canvas)), 
                "\" coords tagOrId x1 y1 x2 y2",
		(char *) NULL);
	return TCL_ERROR;
    }  else {
	if ((Tk_CanvasGetCoordFromObj(interp, canvas, objv[0],
				      &barPtr->bbox[0]) != TCL_OK)
	    || (Tk_CanvasGetCoordFromObj(interp, canvas, objv[1],
					 &barPtr->bbox[1]) != TCL_OK)
	    || (Tk_CanvasGetCoordFromObj(interp, canvas, objv[2],
					 &barPtr->bbox[2]) != TCL_OK)
	    || (Tk_CanvasGetCoordFromObj(interp, canvas, objv[3],
					 &barPtr->bbox[3]) != TCL_OK)) {
	    return TCL_ERROR;
	}
    }
    ComputeBarchartBbox(canvas, barPtr);
    BarchartValues(interp, canvas, itemPtr, 0, (char **) NULL, barPtr->numPoints);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ConfigureBarchart --
 *
 *      This procedure is invoked to configure various aspects
 *      of a barchart item such as its background color.
 *
 * Results:
 *      A standard Tcl result code.  If an error occurs, then
 *      an error message is left in canvasPtr->interp->result.
 *
 * Side effects:
 *      Configuration information, such as colors, may be set 
 *      for itemPtr.
 *
 *--------------------------------------------------------------
 */

static int
ConfigureBarchart(interp, canvas, itemPtr, objc, objv, flags)
    Tcl_Interp *interp;         /* Interpreter for error reporting. */
    Tk_Canvas canvas;		/* Canvas containing itemPtr. */
    Tk_Item *itemPtr;           /* Barchart item to reconfigure. */
    int objc;                   /* Number of elements in objv.  */
    Tcl_Obj *CONST objv[];      /* Arguments describing things to configure. */
    int flags;                  /* Flags to pass to Tk_ConfigureWidget. */
{
    register BarchartItem *barPtr = (BarchartItem *) itemPtr;
    XGCValues gcValues;
    GC newGC;
    unsigned long mask;
    Tk_Window tkwin = Tk_CanvasTkwin(canvas);
    Display *display = Tk_Display(Tk_CanvasTkwin(canvas));
 
    if (Tk_ConfigureWidget(interp, tkwin, configSpecs, objc, (const char **) objv,
		   (char *) barPtr, flags|TK_CONFIG_OBJS) != TCL_OK) {
	return TCL_ERROR;
    }

     if (barPtr->scaleValue <= 0) {
	 barPtr->scaleValue = 100;
	 Tcl_AppendResult(interp, 
			"wrong scalevalue: should be positiv", (char *) NULL);
	 return TCL_ERROR;
    }

    /*
     * A few of the options require additional processing, such as
     * graphics contexts.
     */

    if (barPtr->color == NULL) {
	newGC = None;
    } else {
	gcValues.foreground = barPtr->color->pixel;
	mask = GCForeground;
	newGC = Tk_GetGC(tkwin, mask, &gcValues);
    }
    if (barPtr->fillGC != None) {
	Tk_FreeGC(display, barPtr->fillGC);
    }
    barPtr->fillGC = newGC;

    if (barPtr->rectColor == NULL) {
	newGC = None;
    } else {
	gcValues.foreground = barPtr->rectColor->pixel;
	mask = GCForeground;
	newGC = Tk_GetGC(tkwin, mask, &gcValues);
    }
    if (barPtr->rectGC != None) {
	Tk_FreeGC(display, barPtr->rectGC);
    }
    barPtr->rectGC = newGC;

    if (barPtr->outlineColor == NULL) {
	newGC = None;
    } else {
	gcValues.foreground = barPtr->outlineColor->pixel;
	gcValues.cap_style = CapProjecting;
	mask = GCForeground|GCCapStyle;
	newGC = Tk_GetGC(tkwin, mask, &gcValues);
    }
    if (barPtr->outlineGC != None) {
	Tk_FreeGC(display, barPtr->outlineGC);
    }
    barPtr->outlineGC = newGC;

    if (barPtr->barlineColor == NULL) {
	newGC = None;
    } else {
	gcValues.foreground = barPtr->barlineColor->pixel;
	gcValues.cap_style = CapProjecting;
	mask = GCForeground|GCCapStyle;
	newGC = Tk_GetGC(tkwin, mask, &gcValues);
    }
    if (barPtr->barlineGC != None) {
	Tk_FreeGC(display, barPtr->barlineGC);
    }
    barPtr->barlineGC = newGC;

    if (barPtr->scalelineColor == NULL) {
        newGC = None;
    } else {
        gcValues.foreground = barPtr->scalelineColor->pixel;
        mask = GCForeground;
        if (barPtr->scalelineStyle < 0) {
            barPtr->scalelineStyle = 0;
        }
	gcValues.line_style = LineSolid;
	if (barPtr->scalelineStyle > 0) { 
            gcValues.line_style = LineOnOffDash;
            gcValues.dashes = (char) barPtr->scalelineStyle;
            mask |= GCLineStyle|GCDashList;
        }
        newGC = Tk_GetGC(tkwin, mask, &gcValues);
    }
    if (barPtr->scalelineGC != None) {
        Tk_FreeGC(display, barPtr->scalelineGC);
    }
    barPtr->scalelineGC = newGC;
    
    ComputeBarchartBbox(canvas, barPtr);
    BarchartValues(interp, canvas, itemPtr, 0, (char **) NULL, 
		   barPtr->numPoints);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DeleteBarchart --
 *
 *	This procedure is called to clean up the data structure
 *	associated with a barchart item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources associated with itemPtr are released.
 *
 *--------------------------------------------------------------
 */

static void
DeleteBarchart(canvas, itemPtr, display)
    Tk_Canvas canvas;			/* Info about overall canvas widget. */
    Tk_Item *itemPtr;			/* Item that is being deleted. */
    Display *display;                   /* Display containing window for
                                         * canvas. */
{
    register BarchartItem *barPtr = (BarchartItem *) itemPtr;

    if (barPtr->coordPtr != NULL) {
	ckfree((char *) barPtr->coordPtr);
    }
    if (barPtr->valuePtr != NULL) {
	ckfree((char *) barPtr->valuePtr);
    }
    if (barPtr->color != NULL) {
	Tk_FreeColor(barPtr->color);
    }
    if (barPtr->fillGC != None) {
	Tk_FreeGC(display, barPtr->fillGC);
    }
    if (barPtr->rectColor != NULL) {
	Tk_FreeColor(barPtr->rectColor);
    }
    if (barPtr->rectGC != None) {
	Tk_FreeGC(display, barPtr->rectGC);
    }
    if (barPtr->outlineColor != NULL) {
	Tk_FreeColor(barPtr->outlineColor);
    }
    if (barPtr->outlineGC != None) {
	Tk_FreeGC(display, barPtr->outlineGC);
    }
    if (barPtr->barlineColor != NULL) {
	Tk_FreeColor(barPtr->barlineColor);
    }
    if (barPtr->barlineGC != None) {
        Tk_FreeGC(display, barPtr->barlineGC);
    }
    if (barPtr->scalelineColor != NULL) {
        Tk_FreeColor(barPtr->scalelineColor);
    }
    if (barPtr->scalelineGC != None) {
        Tk_FreeGC(display, barPtr->scalelineGC);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ComputeBarchartBbox --
 *
 *	This procedure is invoked to compute the bounding box of
 *	all the pixels that may be drawn as part of a barchart.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The fields x1, y1, x2, and y2 are updated in the header
 *	for itemPtr.
 *
 *--------------------------------------------------------------
 */

static void
ComputeBarchartBbox(canvas, barPtr)
    register Tk_Canvas canvas;		/* Canvas that contains item. */
    BarchartItem *barPtr;		/* Item whose bbox is to be
					 * recomputed. */
{
    /*
     * Make sure that the first coordinates are the lowest ones.
     */

    if (barPtr->bbox[1] > barPtr->bbox[3]) {
        double tmp;
        tmp = barPtr->bbox[3];
        barPtr->bbox[3] = barPtr->bbox[1];
        barPtr->bbox[1] = tmp;
    }
    if (barPtr->bbox[0] > barPtr->bbox[2]) {
        double tmp;
        tmp = barPtr->bbox[2];
        barPtr->bbox[2] = barPtr->bbox[0];
        barPtr->bbox[0] = tmp;
    }

    barPtr->header.x1 = (int) barPtr->bbox[0] - 1;
    barPtr->header.y1 = (int) barPtr->bbox[1] - 1;
    barPtr->header.x2 = (int) barPtr->bbox[2] + 0.5;
    barPtr->header.y2 = (int) barPtr->bbox[3] + 0.5;
}

/*
 *--------------------------------------------------------------
 * DisplayBarchart --
 *
 *	This procedure is invoked to draw a barchart item in a given
 *	drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	ItemPtr is drawn in drawable using the transformation
 *	information in canvasPtr.
 *
 *--------------------------------------------------------------
 */

static void
DisplayBarchart(canvas, itemPtr, display, drawable, x, y, wwidth, hheight)
    register Tk_Canvas canvas;		/* Canvas that contains item. */
    Tk_Item *itemPtr;			/* Item to be displayed. */
    Display *display;                   /* Display on which to draw item. */
    Drawable drawable;			/* Pixmap or window in which to draw
					 * item. */
    int x, y, wwidth, hheight;          /* Describes region of canvas that
                                         * must be redisplayed (not used). */
{
    register BarchartItem *barPtr = (BarchartItem *) itemPtr;
    int i;
    short x1, y1, x2, y2;
    XColor *newcolor;
    GC autocolorGC;
    unsigned int width = 0, height;
    int xpos, bloat, rem = 0;
    char *colors [6];

    colors[0] = "#afbcaf";
    colors[1] = "#afbcc9";
    colors[2] = "#d7bcc9";
    colors[3] = "#d7e4c9";
    colors[4] = "#d7e4f1";
    colors[5] = "#ffe4f1";
    /*
     * Compute the screen coordinates of the bounding box for the item.
     * Make sure that the bbox is at least one pixel large, since some
     * X servers will die if it isn't.
     */

    Tk_CanvasDrawableCoords(canvas, barPtr->bbox[0], barPtr->bbox[1],
			    &x1, &y1);
    Tk_CanvasDrawableCoords(canvas, barPtr->bbox[2], barPtr->bbox[3], 
			    &x2, &y2);

    if (x2 <= x1) {
	x2 = x1+1;
    }
    if (y2 <= y1) {
	y2 = y1+1;
    }

    /* Display the background of the barchart bounding box */

    if (barPtr->rectGC != None) {
	XFillRectangle(display, drawable, barPtr->rectGC,
		       x1, y1,
		       (unsigned int) (x2-x1), (unsigned int) (y2-y1));
    }

    /* 
     * Display the bars.
     */

    if (barPtr->numPoints > 0) {
	width = (x2-x1-1)/barPtr->numPoints;
	rem = (x2-x1-1)%barPtr->numPoints;
    }
    xpos = x1;
    bloat = 0;

    autocolorGC = XCreateGC(display, drawable, 0, NULL);

    for (i = 0; i < barPtr->numPoints; i++, xpos += width + bloat) {
	height = (unsigned int) barPtr->coordPtr[i];
	if (i >= barPtr->numPoints - rem) {
	    bloat = 1;
	}
	if (barPtr->fillGC != None) {
	    XFillRectangle(display, drawable, barPtr->fillGC,
			   xpos, (int) (y2-height-1), 
			   width + bloat, height);
	}
	
	if (barPtr->autocolor > 0 && barPtr->fillGC == None) {

	    newcolor = Tk_GetColor(barPtr->interp, Tk_CanvasTkwin(canvas),
				   Tk_GetUid(colors[i%6]));
	    
	    XSetForeground(display, autocolorGC, newcolor->pixel);
	    
	    XFillRectangle(display, drawable, autocolorGC,
			   xpos, (int) (y2-height-1), 
			   width + bloat, height);
	    
	    /* need to free it somewhere :-( */
	    /*if (newcolor != NULL) {
	      Tk_FreeColor(newcolor); 
	      }*/
	}
	
	if (barPtr->barlineGC != None) {
	    XDrawRectangle(display, drawable, barPtr->barlineGC,
			   xpos, (int) (y2-height-1), 
			   width + bloat, height);
	}
    }
    if (autocolorGC != None) {
	XFreeGC(display, autocolorGC);
    }
    
    /*
     * Display the scalelines. Make sure that we only display lines
     * that are actually displayable. Skip the first line to get 
     * around rounding errors.
     */
    
    if (barPtr->scalelineGC != None){
        if (barPtr->scale > 1) {
	    int lines = barPtr->scale;
            if (lines > (y2-y1)) lines = y2-y1;
            for (i = 1; i < lines; i++) {
                XDrawLine(display, drawable, barPtr->scalelineGC,
                          x1,   y2 - i*(y2-y1)/lines,
                          x2-1, y2 - i*(y2-y1)/lines);
            }
        }
    }

    /*
     * Display the border of the bounding box.
     */

    if (barPtr->outlineGC != None) {
	XDrawRectangle(display, drawable, barPtr->outlineGC,
		       x1, y1,
		       (unsigned int) (x2-x1-1),
		       (unsigned int) (y2-y1-1));
	if (barPtr->selected) {
	    TkiMarkRectangle(display, drawable, barPtr->outlineGC,
                             x1, y1, x2, y2);
	}
    }   
}

/*
 *--------------------------------------------------------------
 *
 * BarchartToPoint --
 *
 *	Computes the distance from a given point to a given
 *	barchart, in canvas units.
 *
 * Results:
 *	The return value is 0 if the point whose x and y coordinates
 *	are pointPtr[0] and pointPtr[1] is inside the barchart.  If the
 *	point isn't inside the barchart then the return value is the
 *	distance from the point to the barchart.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static double
BarchartToPoint(canvas, itemPtr, pointPtr)
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item to check against point. */
    double *pointPtr;		/* Pointer to x and y coordinates. */
{
    register BarchartItem *barPtr = (BarchartItem *) itemPtr;
    double xDiff, yDiff, x1, y1, x2, y2, inc;

    /*
     * Generate a new larger rectangle that includes the border
     * width, if there is one.
     */

    x1 = barPtr->bbox[0];
    y1 = barPtr->bbox[1];
    x2 = barPtr->bbox[2];
    y2 = barPtr->bbox[3];
    if (barPtr->outlineGC != None) {
	inc = 0.5;
	x1 -= inc;
	y1 -= inc;
	x2 += inc;
	y2 += inc;
    }

    /*
     * If the point is inside the rectangle, the distance is 0
     */

    if ((pointPtr[0] >= x1) && (pointPtr[0] < x2)
	        && (pointPtr[1] >= y1) && (pointPtr[1] < y2)) {
	return 0.0;
    }

    /*
     * Point is outside rectangle.
     */

    if (pointPtr[0] < x1) {
	xDiff = x1 - pointPtr[0];
    } else if (pointPtr[0] > x2)  {
	xDiff = pointPtr[0] - x2;
    } else {
	xDiff = 0;
    }

    if (pointPtr[1] < y1) {
	yDiff = y1 - pointPtr[1];
    } else if (pointPtr[1] > y2)  {
	yDiff = pointPtr[1] - y2;
    } else {
	yDiff = 0;
    }

    return hypot(xDiff, yDiff);
}

/*
 *--------------------------------------------------------------
 *
 * BarchartToArea --
 *
 *	This procedure is called to determine whether an item
 *	lies entirely inside, entirely outside, or overlapping
 *	a given rectangular area.
 *
 * Results:
 *	-1 is returned if the item is entirely outside the area
 *	given by areaPtr, 0 if it overlaps, and 1 if it is entirely
 *	inside the given area.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
BarchartToArea(canvas, itemPtr, areaPtr)
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item to check against barchart. */
    double *areaPtr;		/* Pointer to array of four coordinates
				 * (x1, y1, x2, y2) describing rectangular
				 * area.  */
{
    register BarchartItem *barPtr = (BarchartItem *) itemPtr;
    double halfWidth;

    halfWidth = 0.5;
    if (barPtr->outlineGC == None) {
	halfWidth = 0.0;
    }

    if ((areaPtr[2] <= (barPtr->bbox[0] - halfWidth))
	    || (areaPtr[0] >= (barPtr->bbox[2] + halfWidth))
	    || (areaPtr[3] <= (barPtr->bbox[1] - halfWidth))
	    || (areaPtr[1] >= (barPtr->bbox[3] + halfWidth))) {
	return -1;
    }
    if ((areaPtr[0] <= (barPtr->bbox[0] - halfWidth))
	    && (areaPtr[1] <= (barPtr->bbox[1] - halfWidth))
	    && (areaPtr[2] >= (barPtr->bbox[2] + halfWidth))
	    && (areaPtr[3] >= (barPtr->bbox[3] + halfWidth))) {
	return 1;
    }
    return 0;
}

/*
 *--------------------------------------------------------------
 *
 * ScaleBarchart --
 *
 *	This procedure is invoked to rescale a barchart item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The barchart referred to by itemPtr is rescaled so that the
 *	following transformation is applied to all point
 *	coordinates:
 *		x' = originX + scaleX*(x-originX)
 *		y' = originY + scaleY*(y-originY)
 *
 *--------------------------------------------------------------
 */

static void
ScaleBarchart(canvas, itemPtr, originX, originY, scaleX, scaleY)
    Tk_Canvas canvas;			/* Canvas containing barchart. */
    Tk_Item *itemPtr;			/* Barchart to be scaled. */
    double originX, originY;		/* Origin about which to scale rect. */
    double scaleX;			/* Amount to scale in X direction. */
    double scaleY;			/* Amount to scale in Y direction. */
{
    BarchartItem *barPtr = (BarchartItem *) itemPtr;
    register double *coordPtr;
    int i;

    barPtr->bbox[0] = originX + scaleX*(barPtr->bbox[0] - originX);
    barPtr->bbox[1] = originY + scaleY*(barPtr->bbox[1] - originY);
    barPtr->bbox[2] = originX + scaleX*(barPtr->bbox[2] - originX);
    barPtr->bbox[3] = originY + scaleY*(barPtr->bbox[3] - originY);

    for (i = 0, coordPtr = barPtr->coordPtr; i < barPtr->numPoints;
	    i++, coordPtr += 1) {
	*coordPtr = scaleY*(*coordPtr);
    }
    ComputeBarchartBbox(canvas, barPtr);
}

/*
 *--------------------------------------------------------------
 *
 * TranslateBarchart --
 *
 *	This procedure is called to move a barchart by a given
 *	amount.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The position of the barchart is offset by (xDelta, yDelta),
 *	and the bounding box is updated in the generic part of the
 *	item structure.
 *
 *--------------------------------------------------------------
 */

static void
TranslateBarchart(canvas, itemPtr, deltaX, deltaY)
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item that is being moved. */
    double deltaX, deltaY;		/* Amount by which item is to be
					 * moved. */
{
    register BarchartItem *barPtr = (BarchartItem *) itemPtr;

    barPtr->bbox[0] += deltaX;
    barPtr->bbox[1] += deltaY;
    barPtr->bbox[2] += deltaX;
    barPtr->bbox[3] += deltaY;

    ComputeBarchartBbox(canvas, barPtr);
}

/*
 *--------------------------------------------------------------
 *
 * ParseBarchartValues --
 *
 *      This procedure is called back during option parsing to
 *      parse value information.
 *
 * Results:
 *      The return value is a standard Tcl result:  TCL_OK means
 *      that the value information was parsed ok, and
 *      TCL_ERROR means it couldn't be parsed.
 *
 * Side effects:
 *      Value information in recordPtr is updated.
 *
 *--------------------------------------------------------------
 */

        /* ARGSUSED */
static int
ParseBarchartValues(clientData, interp, tkwin, value, recordPtr, offset)
    ClientData clientData;      /* Not used. */
    Tcl_Interp *interp;         /* Used for error reporting. */
    Tk_Window tkwin;            /* Not used. */
    char *value;                /* Textual specification of barchart values. */
    char *recordPtr;            /* Pointer to item record in which to
                                 * store value information. */
    int offset;                 /* Not used. */
{
    BarchartItem *barPtr = (BarchartItem *) recordPtr;
    int argc;
    char **argv = NULL;

    if (Tcl_SplitList(interp, value, &argc, (const char ***)&argv) != TCL_OK) {
        syntaxError:
        Tcl_ResetResult(interp);
        Tcl_AppendResult(interp, "bad barchart value \"", value,
                         "\": must be list with 0 ore more numbers", 
                         (char *) NULL);
        if (argv != NULL) {
            ckfree((char *) argv);
        }
        return TCL_ERROR;
    }

    if (BarchartValues(interp, barPtr->canvas, (Tk_Item *) barPtr, 
                         argc, argv, 0) != TCL_OK) {
        goto syntaxError;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * BarchartValues --
 *
 *      This procedure is invoked to parse new values in
 *      the Barchart.
 *
 * Results:
 *      Returns TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *      The members of *itemPtr that deals with values and information
 *      about the barchart polygon are updated.
 *
 *--------------------------------------------------------------
 */

static int
BarchartValues(interp, canvas, itemPtr, argc, argv, numPoints) 
    Tcl_Interp *interp;                 /* */
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;                   /* Item whose coordinates are to be
                                         * read or modified. */
    int argc;                           /* Number of coordinates supplied in
                                         * argv. */
    char **argv;                        /* Array of coordinates: x1, y1,
                                         * x2, y2, ... */
    int numPoints;                      /* Number of Values in barchart. 
					 * Only used when called by 
					 * BarchartCoords. */
{     
    register BarchartItem *barPtr = (BarchartItem *) itemPtr;
    int i, height;
    short x1, x2, y1, y2;
    double max;

    Tk_CanvasDrawableCoords(canvas, barPtr->bbox[0], barPtr->bbox[1],
                            &x1, &y1);
    Tk_CanvasDrawableCoords(canvas, barPtr->bbox[2], barPtr->bbox[3],
                            &x2, &y2);

    height = (y2 == y1) ? 0 : y2-y1-1;

    if (barPtr->numPoints < argc) {
	if (barPtr->valuePtr != NULL) {
	    ckfree((char *) barPtr->valuePtr);
	}
	barPtr->valuePtr = (double *) ckalloc((unsigned)
					      (sizeof(double) * argc));
	if (barPtr->coordPtr != NULL) {
	    ckfree((char *) barPtr->coordPtr);
	}
	barPtr->coordPtr = (double *) ckalloc((unsigned)
					      (sizeof(double) * argc));
    }
    if (numPoints == 0) {
	for (i = 0; i < barPtr->numPoints; i++) {
	    barPtr->valuePtr[i] = 0;
	    barPtr->coordPtr[i] = 0;
	}
    }
    if (argc > 0) {
	barPtr->numPoints = argc;
        
	for (i = 0; i < argc; i++) {
	    if (Tk_CanvasGetCoord(interp, canvas, argv[i], 
				 &barPtr->valuePtr[i]) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
    }

    if (argv) ckfree((char *) argv);
    
    /* 
     * Now we have to check if the actual scale is right. 
     */
    
    /* Find the maximal y-value of the polygon. */
    
    max = 0.0;
    for (i = 0; i < barPtr->numPoints; i++) {
	if (barPtr->valuePtr[i] > max) {
	    max = barPtr->valuePtr[i];
	}
    }
    
    /* Find the right scale. */
    
    barPtr->scale = 1;
    if (max/barPtr->scale > barPtr->scaleValue) {
	barPtr->scale = (int) (max / barPtr->scaleValue) + 1;
    } 
    
    /* Adapt the coordinates to the actual scale. */

    for (i = 0; i < barPtr->numPoints; i++) {
	barPtr->coordPtr[i] = barPtr->valuePtr[i]/barPtr->scaleValue * 
		height/barPtr->scale;
    }
    
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * PrintBarchartValues --
 *
 *      This procedure is a callback invoked by the configuration
 *      code to return a printable value describing a barchart value.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *--------------------------------------------------------------
 */

    /* ARGSUSED */
static char *
PrintBarchartValues(clientData, tkwin, recordPtr, offset, freeProcPtr)
    ClientData clientData;      /* Not used. */
    Tk_Window tkwin;            /* Window associated with barPtr's widget. */
    char *recordPtr;            /* Pointer to item record containing current
                                 * value information. */
    int offset;                 /* Not used. */
    Tcl_FreeProc **freeProcPtr; /* Store address of procedure to call to
                                 * free string here. */
{
    BarchartItem *barPtr = (BarchartItem *) recordPtr;
    Tcl_DString buffer;
    char tmp[TCL_DOUBLE_SPACE];
    char *p;
    int i;

    Tcl_DStringInit(&buffer);

    for (i = 0; i < barPtr->numPoints; i++) {
        Tcl_PrintDouble(barPtr->interp, barPtr->valuePtr[i], tmp);
        Tcl_DStringAppendElement(&buffer, tmp);
    }

    *freeProcPtr = TCL_DYNAMIC;
    p = ckalloc((size_t) (Tcl_DStringLength(&buffer) + 1));
    strcpy(p, buffer.string);
    Tcl_DStringFree (&buffer);
    return p;
}

/*
 *--------------------------------------------------------------
 *
 * BarchartToPostscript --
 *
 *	This procedure is called to generate Postscript for
 *	barchart items.
 *
 * Results:
 *	The return value is a standard Tcl result.  If an error
 *	occurs in generating Postscript then an error message is
 *	left in canvasPtr->interp->result, replacing whatever used
 *	to be there.  If no error occurs, then Postscript for the
 *	item is appended to the result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
BarchartToPostscript(interp, canvas, itemPtr, prepass)
    Tcl_Interp *interp;                 /* Leave Postscript or error message
                                         * here. */
    Tk_Canvas canvas;			/* Information about overall canvas. */
    Tk_Item *itemPtr;			/* Item for which Postscript is
					 * wanted. */
    int prepass;                        /* 1 means this is a prepass to
                                         * collect font information;  0 means
                                         * final Postscript is being created. */
{
    register BarchartItem *barPtr = (BarchartItem *) itemPtr;
    char pathCmd1[500], pathCmd2[400], string[100];
    double y1, y2, xpos;
    int dash, i;
    XColor *newcolor;
    int bloat, rem = 0, width = 0, height;
    char *colors[6];

    colors[0] = "#afbcaf";
    colors[1] = "#afbcc9";
    colors[2] = "#d7bcc9";
    colors[3] = "#d7e4c9";
    colors[4] = "#d7e4f1";
    colors[5] = "#ffe4f1";

    y1 = Tk_CanvasPsY(canvas, barPtr->bbox[1]);
    y2 = Tk_CanvasPsY(canvas, barPtr->bbox[3]);

    /*
     * Generate a string that creates a path for the barchart.
     * This is the only part of the procedure's code that is type-
     * specific.
     */

    sprintf(pathCmd1, "%.15g %.15g moveto %.15g 0 rlineto 0 %.15g rlineto %.15g 0 rlineto closepath\n",
                barPtr->bbox[0], y1,
                barPtr->bbox[2]-barPtr->bbox[0]-1, y2-y1,
                barPtr->bbox[0]-barPtr->bbox[2]+1);
    
    /*
     * First draw the filled background area of the barchart.
     */

    if (barPtr->rectColor != NULL) {
        Tcl_AppendResult(interp, pathCmd1, (char *) NULL);
        if (Tk_CanvasPsColor(interp, canvas, barPtr->rectColor)
                != TCL_OK) {
            return TCL_ERROR;
        }
        Tcl_AppendResult(interp, "fill\n", (char *) NULL);
    }

    /*
     * Create a path for the bars.
     */
    
    if (barPtr->numPoints > 0) {
        width = (int) (barPtr->bbox[2]-barPtr->bbox[0]-1)/barPtr->numPoints;
        rem = ((int) (barPtr->bbox[2]-barPtr->bbox[0]-1))%barPtr->numPoints;
    }
    xpos = barPtr->bbox[0];
    bloat = 0;
    
    for (i = 0; i < barPtr->numPoints; i++, xpos += width + bloat) {
	height = (unsigned int) barPtr->coordPtr[i];
	if (i >= barPtr->numPoints - rem) {
	    bloat = 1;
	}
	sprintf(pathCmd2, "%.15g %.15g moveto 0 %d rlineto %d 0 rlineto 0 %d rlineto closepath\n", 
		xpos, y2, height, width + bloat, -height);
	
	/* 
	 * Draw the inside of the bars, if necessary.
	 */
	
	/* Autocolor mode. */
	
	if (barPtr->autocolor > 0 && barPtr->fillGC == None) {
	    Tcl_AppendResult(interp, pathCmd2, (char *) NULL);
	    
	    newcolor = Tk_GetColor(interp, Tk_CanvasTkwin(canvas),
				   Tk_GetUid(colors[i%6]));
	    
	    if (Tk_CanvasPsColor(interp, canvas, newcolor) != TCL_OK) {
                return TCL_ERROR;
            }
	    
	    Tk_FreeColor(newcolor);
	    Tcl_AppendResult(interp, "fill\n", (char *) NULL);
	}

	/* Single color mode. */

	if (barPtr->color != NULL) {
	    Tcl_AppendResult(interp, pathCmd2, (char *) NULL);
	    if (Tk_CanvasPsColor(interp, canvas, barPtr->color) != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tcl_AppendResult(interp, "fill\n", (char *) NULL);
	}

	/*
	 * Draw the outline of the bars, if there is one.
	 */

	if (barPtr->barlineColor != NULL) {
	    Tcl_AppendResult(interp, pathCmd2, (char *) NULL);
	    Tcl_AppendResult(interp, "0 setlinejoin 2 setlinecap\n",
			     (char *) NULL);
	    if (Tk_CanvasPsColor(interp, canvas, barPtr->barlineColor)
                != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tcl_AppendResult(interp, "stroke\n", (char *) NULL);
	}
    }

    /*
     * Draw the scaleline, if there is one.
     */

    /*
     * Generate a path for the scaleline.
     */

    for (i = 1; i < barPtr->scale; i++) {
        y1 = Tk_CanvasPsY(canvas, (barPtr->bbox[3]-1-i*
                 (barPtr->bbox[3]-barPtr->bbox[1]-2)/barPtr->scale));

        sprintf(pathCmd2, "%.15g %.15g moveto %.15g 0 rlineto\n",
                barPtr->bbox[0], y1, 
                barPtr->bbox[2]-barPtr->bbox[0]-1);
    
        if (barPtr->scalelineColor != NULL) {
            Tcl_AppendResult(interp, pathCmd2, (char *) NULL);
            if (barPtr->scalelineStyle > 0) {
                dash = barPtr->scalelineStyle;
                sprintf(string,
                        " 0 setlinejoin 2 setlinecap [%d] 0 setdash\n", 
			dash);
            } else {
                sprintf(string, " 0 setlinejoin 2 setlinecap [] 0 setdash\n");
            }
	    Tcl_AppendResult(interp, string, (char *) NULL);
            
            if (Tk_CanvasPsColor(interp, canvas, barPtr->scalelineColor)
                != TCL_OK) {
                return TCL_ERROR;
            }
            Tcl_AppendResult(interp, "stroke\n", (char *) NULL);
	    Tcl_AppendResult(interp,  "[] 0 setdash\n", (char *) NULL);
        }
    }
    
    /*
     * Now draw the outline, if there is one.
     */
    
    if (barPtr->outlineColor != NULL) {
        Tcl_AppendResult(interp, pathCmd1, (char *) NULL);
        Tcl_AppendResult(interp, "0 setlinejoin 2 setlinecap\n", 
			 (char *) NULL);
        if (Tk_CanvasPsColor(interp, canvas, barPtr->outlineColor)
	    != TCL_OK) {
            return TCL_ERROR;
        }
        Tcl_AppendResult(interp, "stroke\n", (char *) NULL);
    }
    
    return TCL_OK;
}


void
Tki_BarchartInit ()
{
    tagsOption.parseProc = Tk_CanvasTagsParseProc;
    tagsOption.printProc = Tk_CanvasTagsPrintProc;

    Tk_CreateItemType((Tk_ItemType *) &TkBarchartType);
}


