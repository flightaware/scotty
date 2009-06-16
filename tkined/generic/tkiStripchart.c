/*
 * tkiStripchart.c --
 *
 *	This file implements stripchart items for canvas widgets.
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

/* #define HAVE_LABEL */

/*
 * The structure below defines the record for each stripchart item.
 */

typedef struct StripchartItem {
    Tk_Item header;             /* Generic stuff that's the same for all
                                 * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_Canvas canvas;           /* Canvas containing item. Needed for
				 * parsing stripchart values. */
    Tcl_Interp *interp;		/* Interpreter handle. */
    int numPoints;              /* Number of points in stripchart polygon. */
    double *coordPtr;           /* Pointer to malloc-ed array containing
                                 * x- and y-coords of all points in polygon.
                                 * X-coords are even-valued indices, y-coords
                                 * are corresponding odd-valued indices. */
    double *shiftPtr;           /* Pointer to malloc-ed array containing 
				 * the y-coords of all points in polygon. */
    int scale;                  /* Actual scale of the stripchart. */
    double scaleValue;          /* Every scalevalue we scale up. */
    int jump;                   /* Number of pixels jumped with a 
				 * jumpscroll. */
    int numValues;              /* Number of values that are actually 
				 * stored in shiftPtr. */
    XColor *stripColor;         /* Color for stripchart polygon. */
    GC fillGC;                  /* Graphics context for filling stripchart 
				 * polygon. */
    XColor *striplineColor;     /* Color of stripline. */
    GC striplineGC;             /* Graphics context for the stripline. */
    double bbox[4];             /* Coordinates of bounding box for rectangle
                                 * (x1, y1, x2, y2). */
    XColor *rectColor;          /* Background color. */
    GC rectGC;                  /* Graphics context for rectangular area. */
    XColor *outlineColor;       /* Color of rectangular outline. */
    GC outlineGC;               /* Graphics context for outline. */
    XColor *scalelineColor;     /* Color of scaleline. */
    int scalelineStyle;         /* Style of scaleline, 0 means LineSolid, 
				 * each other positive value means the
				 * length of dashes using LineOnOffDash. */
    GC scalelineGC;             /* Graphics context for scaleline. */
#ifdef HAVE_LABEL
    char *text;			/* Text for item. */
    XFontStruct *fontPtr;	/* Font for drawing text. */
    GC textGC;                  /* GC for the text describing the new 
				 * scalevalue. */
    XColor *textColor;          /* Color for the text describing the new 
				 * scalevalue. */
    unsigned int textWidth;	/* */
    unsigned int textHeight;	/* */
#endif
    int selected;		/* True if the item is selected. */
} StripchartItem;


/*
 * Prototypes for procedures defined in this file:
 */

static void             ComputeStripchartBbox _ANSI_ARGS_((
                            Tk_Canvas canvas, StripchartItem *stripPtr));
static int              ConfigureStripchart _ANSI_ARGS_((Tcl_Interp *interp,
                            Tk_Canvas canvas, Tk_Item *itemPtr, int objc,
                            Tcl_Obj *CONST objv[], int flags));
static int              CreateStripchart _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr,
			    int objc, Tcl_Obj *CONST objv[]));
static void             DeleteStripchart _ANSI_ARGS_((Tk_Canvas canvas,
                            Tk_Item *itemPtr, Display *display));
static void             DisplayStripchart _ANSI_ARGS_((Tk_Canvas canvas,
                            Tk_Item *itemPtr, Display *display, Drawable dst,
			    int x, int y, int width, int height));
static void             FillStripchart _ANSI_ARGS_((Tk_Canvas canvas, 
			    Tk_Item *itemPtr, double *coordPtr, 
			    Drawable drawable));
static int              ParseStripchartValues _ANSI_ARGS_((ClientData 
			    clientData, Tcl_Interp *interp, Tk_Window tkwin, 
			    char *value, char *recordPtr, int offset));
static char *           PrintStripchartValues _ANSI_ARGS_((ClientData 
			    clientData, Tk_Window tkwin, char *recordPtr, 
			    int offset, Tcl_FreeProc **freeProcPtr));
static void             ScaleStripchart _ANSI_ARGS_((Tk_Canvas canvas,
                            Tk_Item *itemPtr, double originX, double originY,
                            double scaleX, double scaleY));
static int              StripchartCoords _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, 
                            int objc, Tcl_Obj *CONST objv[]));
static int              StripchartToArea _ANSI_ARGS_((Tk_Canvas canvas,
                            Tk_Item *itemPtr, double *rectPtr));
static double           StripchartToPoint _ANSI_ARGS_((Tk_Canvas canvas,
                            Tk_Item *itemPtr, double *pointPtr));
static int              StripchartToPostscript _ANSI_ARGS_((Tcl_Interp *interp,
                            Tk_Canvas canvas, Tk_Item *itemPtr, int prepass));
static int              StripchartValues _ANSI_ARGS_ ((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, 
                            int argc, char **argv));
static void             TranslateStripchart _ANSI_ARGS_((Tk_Canvas canvas,
                            Tk_Item *itemPtr, double deltaX, double deltaY));

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

static Tk_CustomOption tagsOption = {NULL, NULL,
     (ClientData) NULL
};
static Tk_CustomOption valueOption = {ParseStripchartValues,
        PrintStripchartValues, (ClientData) NULL
};

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_COLOR, "-background", (char *) NULL, (char *) NULL,
        (char *) NULL, Tk_Offset(StripchartItem, rectColor), 
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-fill", (char *) NULL, (char *) NULL,
	 "black", Tk_Offset(StripchartItem, stripColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_INT, "-jumpscroll", (char *) NULL, (char *) NULL,
	"5", Tk_Offset(StripchartItem, jump), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_COLOR, "-outline", "outlineColor", (char *) NULL,
        "black", Tk_Offset(StripchartItem, outlineColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-scaleline", (char *) NULL, (char *) NULL,
        "black", Tk_Offset(StripchartItem, scalelineColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_INT, "-scalelinestyle", (char *) NULL, (char *) NULL,
	"4", Tk_Offset(StripchartItem, scalelineStyle),
        TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_DOUBLE, "-scalevalue", (char *) NULL, (char *) NULL,
	"100.0", Tk_Offset(StripchartItem, scaleValue), 
	TK_CONFIG_DONT_SET_DEFAULT},  
    {TK_CONFIG_COLOR, "-stripline", (char *) NULL, (char *) NULL,
        "", Tk_Offset(StripchartItem, striplineColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
        (char *) NULL, 0, TK_CONFIG_NULL_OK, &tagsOption},
    {TK_CONFIG_CUSTOM, "-values", (char *) NULL, (char *) NULL,
        (char *) NULL, 0, TK_CONFIG_DONT_SET_DEFAULT, &valueOption},
#ifdef HAVE_LABEL
    {TK_CONFIG_STRING, "-text", (char *) NULL, (char *) NULL,
        "", Tk_Offset(StripchartItem, text), TK_CONFIG_NULL_OK},
    {TK_CONFIG_FONT, "-font", (char *) NULL, (char *) NULL,
        "-Adobe-Helvetica-Bold-R-Normal--*-120-*",
        Tk_Offset(StripchartItem, fontPtr), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-textColor", (char *) NULL, (char *) NULL,
        "black", Tk_Offset(StripchartItem, textColor), TK_CONFIG_NULL_OK},
#endif
    {TK_CONFIG_BOOLEAN, "-selected", (char *) NULL, (char *) NULL,
	"0", Tk_Offset(StripchartItem, selected), TK_CONFIG_NULL_OK},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
        (char *) NULL, 0, 0}
};



/*
 * The structures below defines the stripchart item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_ItemType TkStripchartType = {
    "stripchart",                       /* name */
    sizeof(StripchartItem),             /* itemSize */
    CreateStripchart,                   /* createProc */
    configSpecs,                        /* configSpecs */
    ConfigureStripchart,                /* configureProc */
    StripchartCoords,                   /* coordProc */
    DeleteStripchart,                   /* deleteProc */
    DisplayStripchart,                  /* displayProc */
    TK_CONFIG_OBJS,			/* flags */
    StripchartToPoint,                  /* pointProc */
    StripchartToArea,                   /* areaProc */
    StripchartToPostscript,             /* postscriptProc */
    ScaleStripchart,                    /* scaleProc */
    TranslateStripchart,                /* translateProc */
    (Tk_ItemIndexProc *) NULL,          /* indexProc */
    (Tk_ItemCursorProc *) NULL,         /* icursorProc */
    (Tk_ItemSelectionProc *) NULL,      /* selectionProc */
    (Tk_ItemInsertProc *) NULL,         /* insertProc */
    (Tk_ItemDCharsProc *) NULL,         /* dTextProc */
    (Tk_ItemType *) NULL                /* nextPtr */
};

/*
 * The definition below determines how large are static arrays
 * used to hold spline points (splines larger than this have to
 * have their arrays malloc-ed).
 */

#define MAX_STATIC_POINTS 200

/*
 *--------------------------------------------------------------
 *
 * CreateStripchart --
 *
 *	This procedure is invoked to create a new stripchart item in
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
 *	A new stripchart item is created.
 *
 *--------------------------------------------------------------
 */

static int
CreateStripchart(interp, canvas, itemPtr, objc, objv)
    Tcl_Interp *interp;			/* Interpreter for error reporting. */
    Tk_Canvas canvas;			/* Canvas to hold new item. */
    Tk_Item *itemPtr;			/* Record to hold new item;  header
					 * has been initialized by caller. */
    int objc;				/* Number of arguments in objv. */
    Tcl_Obj *CONST objv[];		/* Arguments describing stripchart. */
{
    register StripchartItem *stripPtr = (StripchartItem *) itemPtr;
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

    stripPtr->canvas = canvas;
    stripPtr->interp = interp;
    stripPtr->numPoints = 0;
    stripPtr->coordPtr = NULL;
    stripPtr->shiftPtr = NULL;
    stripPtr->scale = 1;
    stripPtr->scaleValue = 100.0;
    stripPtr->jump = 5;
    stripPtr->numValues = 0;
    stripPtr->stripColor = NULL;
    stripPtr->fillGC = None;
    stripPtr->striplineColor = NULL;
    stripPtr->striplineGC = None;
    stripPtr->rectColor = NULL;
    stripPtr->rectGC = None;
    stripPtr->outlineColor = NULL;
    stripPtr->outlineGC = None;
    stripPtr->scalelineColor = NULL;
    stripPtr->scalelineStyle = 4;
    stripPtr->scalelineGC = None;
    stripPtr->selected = 0;
#ifdef HAVE_LABEL
    stripPtr->text = NULL;
    stripPtr->fontPtr = NULL;
    stripPtr->textGC = None;
    stripPtr->textColor = NULL;
#endif

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

    if (StripchartCoords(interp, canvas, itemPtr, i, objv) != TCL_OK) {
	goto error;
    }

    if (ConfigureStripchart(interp, canvas, itemPtr, objc-i, objv+i, 0)
	   == TCL_OK) {
	return TCL_OK;
    }

    error:
    DeleteStripchart(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * StripchartCoords --
 *
 *	This procedure is invoked to process the "coords" widget
 *	command on stripcharts.  See the user documentation for details
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
StripchartCoords(interp, canvas, itemPtr, objc, objv)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item whose coordinates are to be
					 * read or modified. */
    int objc;				/* Number of coordinates supplied in
					 * objv. */
    Tcl_Obj *CONST objv[];		/* Array of coordinates: x1, y1,
					 * x2, y2, ... */
{
    register StripchartItem *stripPtr = (StripchartItem *) itemPtr;
    int i;

    if (objc == 0) {
	Tcl_Obj *objPtr = Tcl_NewObj();
	for (i = 0; i < 4; i++) {
	    Tcl_ListObjAppendElement(interp, objPtr,
				     Tcl_NewDoubleObj(stripPtr->bbox[i]));
	}
	Tcl_SetObjResult(interp, objPtr);
	return TCL_OK;
    } else if (objc != 4) {
	Tcl_AppendResult(interp, "wrong # args:  should be \"",
		Tk_PathName(Tk_CanvasTkwin(canvas)), 
		"\" coords tagOrId x1 y1 x2 y2", (char *) NULL);
	return TCL_ERROR;
    }  else {
	if ((Tk_CanvasGetCoordFromObj(interp, canvas, objv[0],
				      &stripPtr->bbox[0]) != TCL_OK)
	    || (Tk_CanvasGetCoordFromObj(interp, canvas, objv[1],
					 &stripPtr->bbox[1]) != TCL_OK)
	    || (Tk_CanvasGetCoordFromObj(interp, canvas, objv[2],
					 &stripPtr->bbox[2]) != TCL_OK)
	    || (Tk_CanvasGetCoordFromObj(interp, canvas, objv[3],
					 &stripPtr->bbox[3]) != TCL_OK)) {
	    return TCL_ERROR;
	}

    }

    ComputeStripchartBbox(canvas, stripPtr);   
    StripchartValues(interp, canvas, itemPtr, 0, (char **) NULL);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ConfigureStripchart --
 *
 *      This procedure is invoked to configure various aspects
 *      of a stripchart item such as its background color.
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
ConfigureStripchart(interp, canvas, itemPtr, objc, objv, flags)
    Tcl_Interp *interp;         /* Interpreter for error reporting. */
    Tk_Canvas canvas;           /* Canvas containing itemPtr. */
    Tk_Item *itemPtr;           /* Stripchart item to reconfigure. */
    int objc;                   /* Number of elements in objv.  */
    Tcl_Obj *CONST objv[];
    /* Arguments describing things to configure. */
    int flags;                  /* Flags to pass to Tk_ConfigureWidget. */
{
    register StripchartItem *stripPtr = (StripchartItem *) itemPtr;
    XGCValues gcValues;
    GC newGC;
    unsigned long mask;
    Tk_Window tkwin = Tk_CanvasTkwin(canvas);
    Display *display = Tk_Display(Tk_CanvasTkwin(canvas));

    if (Tk_ConfigureWidget(interp, tkwin, configSpecs, objc, (char **) objv,
		   (char *) stripPtr, flags|TK_CONFIG_OBJS) != TCL_OK) {
	return TCL_ERROR;
    }

    if (stripPtr->scaleValue <= 0) {
	stripPtr->scaleValue = 100;
	Tcl_AppendResult(interp, 
			 "wrong scalevalue: should be positiv", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * A few of the options require additional processing, such as
     * graphics contexts.
     */

    if (stripPtr->stripColor == NULL) {
	newGC = None;
    } else {
	gcValues.foreground = stripPtr->stripColor->pixel;
	mask = GCForeground;
	newGC = Tk_GetGC(tkwin, mask, &gcValues);
    }
    if (stripPtr->fillGC != None) {
	Tk_FreeGC(display, stripPtr->fillGC);
    }
    stripPtr->fillGC = newGC;

    if (stripPtr->striplineColor == NULL) {
	newGC = None;
    } else {
	gcValues.foreground = stripPtr->striplineColor->pixel;
	mask = GCForeground;
	newGC = Tk_GetGC(Tk_CanvasTkwin(canvas), mask, &gcValues);
    }
    if (stripPtr->striplineGC != None) {
	Tk_FreeGC(display, stripPtr->striplineGC);
    }
    stripPtr->striplineGC = newGC;

    if (stripPtr->outlineColor == NULL) {
	newGC = None;
    } else {
	gcValues.foreground = stripPtr->outlineColor->pixel;
	gcValues.cap_style = CapProjecting;
	mask = GCForeground|GCCapStyle;
	newGC = Tk_GetGC(Tk_CanvasTkwin(canvas), mask, &gcValues);
    }
    if (stripPtr->outlineGC != None) {
	Tk_FreeGC(display, stripPtr->outlineGC);
    }
    stripPtr->outlineGC = newGC;
    
    if (stripPtr->rectColor == NULL) {
	newGC = None;
    } else {
	gcValues.foreground = stripPtr->rectColor->pixel;
	mask = GCForeground;
	newGC = Tk_GetGC(Tk_CanvasTkwin(canvas), mask, &gcValues);
    }
    if (stripPtr->rectGC != None) {
	Tk_FreeGC(display, stripPtr->rectGC);
    }
    stripPtr->rectGC = newGC;


    if (stripPtr->scalelineColor == NULL) {
	newGC = None;
    } else {
	gcValues.foreground = stripPtr->scalelineColor->pixel;
	mask = GCForeground;
	if (stripPtr->scalelineStyle < 0) {
	    stripPtr->scalelineStyle = 0;
	}
	gcValues.line_style = LineSolid;
	if (stripPtr->scalelineStyle > 0) {
	    gcValues.line_style = LineOnOffDash;
	    gcValues.dashes = (char) stripPtr->scalelineStyle;
	    mask |= GCLineStyle|GCDashList;
	}
	newGC = Tk_GetGC(Tk_CanvasTkwin(canvas), mask, &gcValues);
    }
    if (stripPtr->scalelineGC != None) {
	Tk_FreeGC(display, stripPtr->scalelineGC);
    }
    stripPtr->scalelineGC = newGC;

#ifdef HAVE_LABEL
    if (stripPtr->textColor == NULL) {
	newGC = None;
    } else {
	gcValues.foreground = stripPtr->textColor->pixel;
	mask = GCForeground;
	newGC = Tk_GetGC(Tk_CanvasTkwin(canvas), mask, &gcValues);
    }
    if (stripPtr->textGC != None) {
	Tk_FreeGC(display, stripPtr->textGC);
    }
    stripPtr->textGC = newGC;
#endif

    ComputeStripchartBbox(canvas, stripPtr);
    StripchartValues(interp, canvas, itemPtr, 0, (char **) NULL);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DeleteStripchart --
 *
 *	This procedure is called to clean up the data structure
 *	associated with a stripchart item.
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
DeleteStripchart(canvas, itemPtr, display)
    Tk_Canvas canvas;			/* Info about overall canvas widget. */
    Tk_Item *itemPtr;			/* Item that is being deleted. */
    Display *display;			/* Display containing window for
                                         * canvas. */
{
    register StripchartItem *stripPtr = (StripchartItem *) itemPtr;

    if (stripPtr->coordPtr != NULL) {
	ckfree((char *) stripPtr->coordPtr);
    }
    if (stripPtr->shiftPtr != NULL) {
	ckfree((char *) stripPtr->shiftPtr);
    }
    if (stripPtr->stripColor != NULL) {
	Tk_FreeColor(stripPtr->stripColor);
    }
    if (stripPtr->fillGC != None) {
	Tk_FreeGC(display, stripPtr->fillGC);
    }
    if (stripPtr->striplineColor != NULL) {
	Tk_FreeColor(stripPtr->striplineColor);
    }
    if (stripPtr->striplineGC != None) {
	Tk_FreeGC(display, stripPtr->striplineGC);
    }
    if (stripPtr->rectColor != NULL) {
	Tk_FreeColor(stripPtr->rectColor);
    }
    if (stripPtr->rectGC != None) {
	Tk_FreeGC(display, stripPtr->rectGC);
    }
    if (stripPtr->outlineColor != NULL) {
	Tk_FreeColor(stripPtr->outlineColor);
    }
    if (stripPtr->outlineGC != None) {
        Tk_FreeGC(display, stripPtr->outlineGC);
    } 
    if (stripPtr->scalelineColor != NULL) {
	Tk_FreeColor(stripPtr->scalelineColor);
    }
    if (stripPtr->scalelineGC != None) {
	Tk_FreeGC(display, stripPtr->scalelineGC);
    }
#ifdef HAVE_LABEL
    if (stripPtr->text) {
        ckfree(stripPtr->text);
    }
    if (stripPtr->fontPtr != NULL) {
	Tk_FreeFontStruct(stripPtr->fontPtr);
    }
    if (stripPtr->textGC != None) {
        Tk_FreeGC(display, stripPtr->textGC);
    }
    if (stripPtr->textColor != NULL) {
        Tk_FreeColor(stripPtr->textColor);
    }
#endif
}

/*
 *--------------------------------------------------------------
 *
 * ComputeStripchartBbox --
 *
 *	This procedure is invoked to compute the bounding box of
 *	all the pixels that may be drawn as part of a stripchart.
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
ComputeStripchartBbox(canvas, stripPtr)
    register Tk_Canvas canvas;		/* Canvas that contains item. */
    StripchartItem *stripPtr;		/* Item whose bbox is to be
					 * recomputed. */
{
    /*
     * Make sure that the first coordinates are the lowest ones.
     */

    if (stripPtr->bbox[1] > stripPtr->bbox[3]) {
        double tmp;
        tmp = stripPtr->bbox[3];
        stripPtr->bbox[3] = stripPtr->bbox[1];
        stripPtr->bbox[1] = tmp;
    }
    if (stripPtr->bbox[0] > stripPtr->bbox[2]) {
        double tmp;
        tmp = stripPtr->bbox[2];
        stripPtr->bbox[2] = stripPtr->bbox[0];
        stripPtr->bbox[0] = tmp;
    }

    stripPtr->header.x1 = (int) stripPtr->bbox[0] - 1;
    stripPtr->header.y1 = (int) stripPtr->bbox[1] - 1;
    stripPtr->header.x2 = (int) stripPtr->bbox[2] + 1.5;
    stripPtr->header.y2 = (int) stripPtr->bbox[3] + 1.5;

#ifdef HAVE_LABEL
    if (stripPtr->fontPtr && stripPtr->text) {
      TkComputeTextGeometry(stripPtr->fontPtr, stripPtr->text, 
			    strlen(stripPtr->text),  -1, 
			    &(stripPtr->textWidth),
			    &(stripPtr->textHeight));
    } else {
      stripPtr->textWidth = stripPtr->textHeight = 0;
    }

    stripPtr->header.x1 -= 3;
    stripPtr->header.y1 -= 3;
    stripPtr->header.x2 += 3;
    stripPtr->header.y2 += 3;
#endif
}

/*
 *--------------------------------------------------------------
 *
 * FillStripchart --
 *
 *	This procedure is invoked to convert a stripchart to screen
 *	coordinates and display it using a particular GC.
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
FillStripchart(canvas, itemPtr, coordPtr, drawable)
    register Tk_Canvas canvas;		/* Canvas whose coordinate system
					 * is to be used for drawing. */
    Tk_Item *itemPtr;                   /* Item to fill. */
    double *coordPtr;			/* Array of coordinates for polygon:
					 * x1, y1, x2, y2, .... */
    Drawable drawable;			/* Pixmap or window in which to draw
					 * stripchart. */
{
    register StripchartItem *stripPtr = (StripchartItem *) itemPtr;

    XPoint staticPoints[MAX_STATIC_POINTS];
    XPoint *pointPtr;
    register XPoint *pPtr;
    int i;

    /*
     * Build up an array of points in screen coordinates.  Use a
     * static array unless the stripchart has an enormous number of points;
     * in this case, dynamically allocate an array.
     */

    if (stripPtr->numValues+2 <= MAX_STATIC_POINTS) {
	pointPtr = staticPoints;
    } else {
	pointPtr = (XPoint *) ckalloc((unsigned) 
			((stripPtr->numValues+2) * sizeof(XPoint)));
    }

    for (i = 0, pPtr = pointPtr; i < stripPtr->numValues+2; i += 1, 
	 coordPtr += 2, pPtr++) {
	Tk_CanvasDrawableCoords(canvas, coordPtr[0], coordPtr[1], 
				&pPtr->x, &pPtr->y);
    }

    /*
     * Display stripchart, then free up stripchart storage if it was 
     * dynamically allocated.
     */

    if (stripPtr->fillGC != NULL) {
	XFillPolygon(Tk_Display(Tk_CanvasTkwin(canvas)), drawable, 
		     stripPtr->fillGC, pointPtr, stripPtr->numValues+2, 
		     Complex, CoordModeOrigin);
    }
    if (stripPtr->striplineGC != None) {
	XDrawLines(Tk_Display(Tk_CanvasTkwin(canvas)), drawable, 
                   stripPtr->striplineGC, pointPtr, stripPtr->numValues+2,
		   CoordModeOrigin);
    }

    if (pointPtr != staticPoints) {
	ckfree((char *) pointPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * DisplayStripchart --
 *
 *	This procedure is invoked to draw a stripchart item in a given
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
DisplayStripchart(canvas, itemPtr, display, drawable, x, y, width, height)
    register Tk_Canvas canvas;		/* Canvas that contains item. */
    Tk_Item *itemPtr;			/* Item to be displayed. */
    Display *display;                   /* Display on which to draw item. */  
    Drawable drawable;			/* Pixmap or window in which to draw
					 * item. */
    int x, y, width, height;            /* Describes region of canvas that
                                         * must be redisplayed (not used). */
{
    register StripchartItem *stripPtr = (StripchartItem *) itemPtr;
    int i;
    short x1, y1, x2, y2;

    /*
     * Compute the screen coordinates of the bounding box for the item.
     * Make sure that the bbox is at least one pixel large, since some
     * X servers will die if it isn't.
     */

    Tk_CanvasDrawableCoords(canvas, stripPtr->bbox[0], stripPtr->bbox[1],
			    &x1, &y1);
    Tk_CanvasDrawableCoords(canvas, stripPtr->bbox[2], stripPtr->bbox[3], 
			    &x2, &y2);

    if (x2 <= x1) {
	x2 = x1+1;
    }
    if (y2 <= y1) {
	y2 = y1+1;
    }

    /* Display the background of the stripchart bounding box */

    if (stripPtr->rectGC != None) {
	XFillRectangle(display, drawable, stripPtr->rectGC,
		       x1+1, y1+1, 
		       (unsigned int) (x2-x1-1), 
		       (unsigned int) (y2-y1-1));
    }
		   
    FillStripchart(canvas, itemPtr, stripPtr->coordPtr, drawable);
    
    /*
     * Display the scalelines. Make sure that we only display lines
     * that are actually displayable. Skip the first line to get 
     * around rounding errors.
     */

    if (stripPtr->scalelineGC != NULL){
	if (stripPtr->scale > 1) {
	    int lines = stripPtr->scale;
	    if (lines > (y2-y1)) lines = y2-y1;
	    for (i = 1; i < lines; i++) {
		XDrawLine(display, drawable, stripPtr->scalelineGC,
			  x1,   y2 - i*(y2-y1)/lines, 
			  x2-1, y2 - i*(y2-y1)/lines);
	    }
	}
    }
    
    /* 
     * Display the border of the bounding box. 
     */
    
    if (stripPtr->outlineGC != None) {
	XDrawRectangle(display, drawable, stripPtr->outlineGC,
		       x1, y1, (unsigned) (x2-x1), (unsigned) (y2-y1));
	if (stripPtr->selected) {
	    TkiMarkRectangle(display, drawable, stripPtr->outlineGC,
			     x1, y1, x2, y2);
	}
    }

#ifdef HAVE_LABEL
    if (stripPtr->textGC != None) {
	int x = (stripPtr->bbox[0] + stripPtr->bbox[2] / 2)
	  - (stripPtr->textWidth/2);
	int y = stripPtr->bbox[3] + stripPtr->textHeight;
	short drawableX, drawableY;

	Tk_CanvasDrawableCoords(canvas, x, y, &drawableX, &drawableY);
	TkDisplayChars(display, drawable, stripPtr->textGC,
		       stripPtr->fontPtr, stripPtr->text,
		       strlen(stripPtr->text),
		       drawableX, drawableY, 0, 0);
    }
#endif
}

/*
 *--------------------------------------------------------------
 *
 * StripchartToPoint --
 *
 *	Computes the distance from a given point to a given
 *	stripchart, in canvas units.
 *
 * Results:
 *	The return value is 0 if the point whose x and y coordinates
 *	are pointPtr[0] and pointPtr[1] is inside the stripchart.  If the
 *	point isn't inside the stripchart then the return value is the
 *	distance from the point to the stripchart.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static double
StripchartToPoint(canvas, itemPtr, pointPtr)
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item to check against point. */
    double *pointPtr;		/* Pointer to x and y coordinates. */
{
    register StripchartItem *stripPtr = (StripchartItem *) itemPtr;
    double xDiff, yDiff, x1, y1, x2, y2, inc;

    /*
     * Generate a new larger rectangle that includes the border
     * width, if there is one.
     */

    x1 = stripPtr->bbox[0];
    y1 = stripPtr->bbox[1];
    x2 = stripPtr->bbox[2];
    y2 = stripPtr->bbox[3];
    if (stripPtr->outlineGC != None) {
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
 * StripchartToArea --
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
StripchartToArea(canvas, itemPtr, areaPtr)
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item to check against stripchart. */
    double *areaPtr;		/* Pointer to array of four coordinates
				 * (x1, y1, x2, y2) describing rectangular
				 * area.  */
{
    register StripchartItem *stripPtr = (StripchartItem *) itemPtr;
    double halfWidth;

    halfWidth = 0.5;
    if (stripPtr->outlineGC == None) {
	halfWidth = 0.0;
    }

    if ((areaPtr[2] <= (stripPtr->bbox[0] - halfWidth))
	    || (areaPtr[0] >= (stripPtr->bbox[2] + halfWidth))
	    || (areaPtr[3] <= (stripPtr->bbox[1] - halfWidth))
	    || (areaPtr[1] >= (stripPtr->bbox[3] + halfWidth))) {
	return -1;
    }
    if ((areaPtr[0] <= (stripPtr->bbox[0] - halfWidth))
	    && (areaPtr[1] <= (stripPtr->bbox[1] - halfWidth))
	    && (areaPtr[2] >= (stripPtr->bbox[2] + halfWidth))
	    && (areaPtr[3] >= (stripPtr->bbox[3] + halfWidth))) {
	return 1;
    }
    return 0;
}

/*
 *--------------------------------------------------------------
 *
 * ScaleStripchart --
 *
 *	This procedure is invoked to rescale a stripchart item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The stripchart referred to by itemPtr is rescaled so that the
 *	following transformation is applied to all point
 *	coordinates:
 *		x' = originX + scaleX*(x-originX)
 *		y' = originY + scaleY*(y-originY)
 *
 *--------------------------------------------------------------
 */

static void
ScaleStripchart(canvas, itemPtr, originX, originY, scaleX, scaleY)
    Tk_Canvas canvas;			/* Canvas containing stripchart. */
    Tk_Item *itemPtr;			/* Stripchart to be scaled. */
    double originX, originY;		/* Origin about which to scale rect. */
    double scaleX;			/* Amount to scale in X direction. */
    double scaleY;			/* Amount to scale in Y direction. */
{
    StripchartItem *stripPtr = (StripchartItem *) itemPtr;
    register double *coordPtr;
    int i;

    stripPtr->bbox[0] = originX + scaleX*(stripPtr->bbox[0] - originX);
    stripPtr->bbox[1] = originY + scaleY*(stripPtr->bbox[1] - originY);
    stripPtr->bbox[2] = originX + scaleX*(stripPtr->bbox[2] - originX);
    stripPtr->bbox[3] = originY + scaleY*(stripPtr->bbox[3] - originY);

    for (i = 0, coordPtr = stripPtr->coordPtr; i < stripPtr->numPoints;
	    i++, coordPtr += 2) {
	coordPtr[1] = originY + scaleY*(coordPtr[1] - originY);
    }
    ComputeStripchartBbox(canvas, stripPtr);
    StripchartValues(stripPtr->interp, canvas, itemPtr, 0, (char **) NULL);
}

/*
 *--------------------------------------------------------------
 *
 * TranslateStripchart --
 *
 *	This procedure is called to move a stripchart by a given
 *	amount.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The position of the stripchart is offset by (xDelta, yDelta),
 *	and the bounding box is updated in the generic part of the
 *	item structure.
 *
 *--------------------------------------------------------------
 */

static void
TranslateStripchart(canvas, itemPtr, deltaX, deltaY)
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item that is being moved. */
    double deltaX, deltaY;		/* Amount by which item is to be
					 * moved. */
{
    register StripchartItem *stripPtr = (StripchartItem *) itemPtr;
    register double *coordPtr;
    int i;

    stripPtr->bbox[0] += deltaX;
    stripPtr->bbox[1] += deltaY;
    stripPtr->bbox[2] += deltaX;
    stripPtr->bbox[3] += deltaY;

    for (i = 0, coordPtr = stripPtr->coordPtr; i < stripPtr->numPoints;
	    i++, coordPtr += 2) {
	*coordPtr += deltaX;
	coordPtr[1] += deltaY;
    }
    ComputeStripchartBbox(canvas, stripPtr);
}

/*
 *--------------------------------------------------------------
 *
 * ParseStripchartValues --
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
ParseStripchartValues(clientData, interp, tkwin, value, recordPtr, offset)
    ClientData clientData;      /* Not used. */
    Tcl_Interp *interp;         /* Used for error reporting. */
    Tk_Window tkwin;            /* Not used. */
    char *value;                /* Textual specification of stripchart 
				 * values. */
    char *recordPtr;            /* Pointer to item record in which to
                                 * store value information. */
    int offset;                 /* Not used. */
{
    StripchartItem *stripPtr = (StripchartItem *) recordPtr;
    int argc;
    char **argv = NULL;

    if (Tcl_SplitList(interp, value, &argc, &argv) != TCL_OK) {
        syntaxError:
        Tcl_ResetResult(interp);
        Tcl_AppendResult(interp, "bad stripchart value \"", value,
			 "\": must be list with 0 ore more numbers", 
			 (char *) NULL);
        if (argv != NULL) {
            ckfree((char *) argv);
        }
        return TCL_ERROR;
    }

    if (StripchartValues(interp, stripPtr->canvas, (Tk_Item *) stripPtr, 
			 argc, argv) != TCL_OK) {
	goto syntaxError;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * StripchartValues --
 *
 *      This procedure is invoked to parse new values in
 *      the stripchart.
 *
 * Results:
 *      Returns TCL_OK or TCL_ERROR.
 *
 * Side effects:
 *      The members of *itemPtr that deals with values and information
 *      about the stripchart polygon are updated.
 *
 *--------------------------------------------------------------
 */

static int
StripchartValues(interp, canvas, itemPtr, argc, argv) 
    Tcl_Interp *interp;			/* */
    Tk_Canvas canvas;  			/* Canvas containing item. */
    Tk_Item *itemPtr;                   /* Item whose coordinates are to be
                                         * read or modified. */
    int argc;                           /* Number of coordinates supplied in
                                         * argv. */
    char **argv;                        /* Array of coordinates: x1, y1,
                                         * x2, y2, ... */
{    
    register StripchartItem *stripPtr = (StripchartItem *) itemPtr;
    int i, j;
    short x1, x2, y1, y2;
    int numPoints, numCoords, width, height, jump;
    double max, scaleValue;
    double *savePtr = NULL;

    Tk_CanvasDrawableCoords(canvas, stripPtr->bbox[0], stripPtr->bbox[1],
			    &x1, &y1);
    Tk_CanvasDrawableCoords(canvas, stripPtr->bbox[2], stripPtr->bbox[3], 
			    &x2, &y2);
    height = (y2 == y1) ? 0 : y2-y1-1;
    width = (x2 == x1) ? 0 : x2-x1-1;
    numPoints = width+2;
    numCoords = 2*numPoints;
    
    /* 
     * If we enlarge the bounding box, we have to allocate
     * more space for the point arrays.
     */
    
    if (stripPtr->numPoints < numPoints) {
	if (stripPtr->shiftPtr != NULL) {
	    savePtr = stripPtr->shiftPtr;
	}
	stripPtr->shiftPtr = (double *) ckalloc((unsigned)
						(sizeof(double) * (width)));
	for (i = 0; i < width; i++) {
	    stripPtr->shiftPtr[i] = 0;
	}
	for (i = 0; i < stripPtr->numValues; i++) {
	    stripPtr->shiftPtr[i] = savePtr[i];
	}
	if (savePtr != NULL) {
	    ckfree ((char *) savePtr);
	}
	
	if (stripPtr->coordPtr != NULL) {
	    ckfree((char *) stripPtr->coordPtr);
	}
	stripPtr->coordPtr = (double *) ckalloc((unsigned)
						(sizeof(double) * numCoords));
	stripPtr->numPoints = numPoints;
    } 
    /* The bounding box gets smaller. */
    
    else if (stripPtr->numPoints > numPoints) {
	if (stripPtr->numValues > width-1) {
	    for (i = 0; i < width; i++) {
		stripPtr->shiftPtr[i] = 
			stripPtr->shiftPtr[i+stripPtr->numValues-width+1];
	    }
	    stripPtr->numValues = width-1;
	}
	stripPtr->numPoints = numPoints;
    }
    
    /* There are new polygon coordinates. */

    if (stripPtr->jump <= 0) {
	stripPtr->jump = 1;
    }
    for (i = 0; i < argc; i++) {
	
	/* Check if we have to jump. */

	jump = stripPtr->jump;
	if (width <= stripPtr->jump) {
	    jump = 1;
	}
	if (stripPtr->numValues == width-1) {
	    for (j = 0; j < width-jump; j++) {
		stripPtr->shiftPtr[j] = 
			stripPtr->shiftPtr[j+jump];
	    }
	    stripPtr->numValues -= jump;
	    for (j = width-jump; j < width-1; j++) {
		stripPtr->shiftPtr[j] = 0;
	    }
	}

	if (Tk_CanvasGetCoord(interp, canvas, argv[i],
		&stripPtr->shiftPtr[stripPtr->numValues]) != TCL_OK) {
	    return TCL_ERROR;
	}
	stripPtr->numValues++;
    }
    
    if (argv) ckfree((char *) argv);

    /* Clip negative values. */

    for (i = 0; i < stripPtr->numValues; i++) {
	if (stripPtr->shiftPtr[i] < 0) {
	    stripPtr->shiftPtr[i] = 0;
	}
    }
    
    /* Pass the coordinates into coordPtr. */
    
    stripPtr->coordPtr[1] = 0;
    for (i = 2; i < numCoords-2; i+=2) {
	stripPtr->coordPtr[i] = i/2;
	stripPtr->coordPtr[i+1] = stripPtr->shiftPtr[i/2-1];
    }
    stripPtr->coordPtr[numCoords-2] = width;
    stripPtr->coordPtr[numCoords-1] = 0;
    
    /* Adapt the x-coordinates to the actual bounding box. */
    
    for (i = 2; i < numCoords; i+=2) {
	stripPtr->coordPtr[i] += stripPtr->bbox[0];
     }
    stripPtr->coordPtr[0] = stripPtr->bbox[0];
    
    /* 
     * Now we have to check if the actual scale is right. 
     */
    
    /* Find the maximal y-value of the polygon. */
    
    max = 0.0;
    for (i = 0; i < stripPtr->numValues; i++) {
	if (stripPtr->shiftPtr[i] > max)
		max = stripPtr->shiftPtr[i];
    }
    
    /* Find the right scale. */
    
    scaleValue = stripPtr->scaleValue;

    stripPtr->scale = 1;
    if (max/stripPtr->scale > scaleValue) {
	stripPtr->scale = (int) (max / scaleValue) + 1;
    }
    
    /* 
     * Adapt the y-coordinates to the actual scale and to the actual
     * bounding box. 
     */

    for (i = 0; i < numCoords; i+=2) {
	stripPtr->coordPtr[i+1] = 
		-stripPtr->coordPtr[i+1]/scaleValue * 
			(height-1)/stripPtr->scale + stripPtr->bbox[3]-1;
	if (stripPtr->coordPtr[i+1] == stripPtr->bbox[3]-1) {
	    stripPtr->coordPtr[i+1] += 1;
	}
    }
            
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * PrintStripchartValues --
 *
 *      This procedure is a callback invoked by the configuration
 *      code to return a printable value describing a stripchart value.
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
PrintStripchartValues(clientData, tkwin, recordPtr, offset, freeProcPtr)
    ClientData clientData;      /* Not used. */
    Tk_Window tkwin;            /* Window associated with stripPtr's 
				 * widget. */
    char *recordPtr;            /* Pointer to item record containing current
                                 * value information. */
    int offset;                 /* Not used. */
    Tcl_FreeProc **freeProcPtr; /* Store address of procedure to call to
                                 * free string here. */
{
    StripchartItem *stripPtr = (StripchartItem *) recordPtr;
    Tcl_DString buffer;
    char tmp[TCL_DOUBLE_SPACE];
    char *p;
    int i;

    Tcl_DStringInit(&buffer);

    for (i = 0; i < stripPtr->numValues; i++) {
	Tcl_PrintDouble(stripPtr->interp, stripPtr->shiftPtr[i], tmp);
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
 * StripchartToPostscript --
 *
 *	This procedure is called to generate Postscript for
 *	stripchart items.
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
StripchartToPostscript(interp, canvas, itemPtr, prepass)
    Tcl_Interp *interp;                 /* Leave Postscript or error message
                                         * here. */
    Tk_Canvas canvas;			/* Information about overall canvas. */
    Tk_Item *itemPtr;			/* Item for which Postscript is
					 * wanted. */
    int prepass;                        /* 1 means this is a prepass to
                                         * collect font information;  0 means
                                         * final Postscript is being created. */
{
    register StripchartItem *stripPtr = (StripchartItem *) itemPtr;
    char pathCmd1[500], pathCmd2[500], string[100];
    double y1, y2;
    int dash, i;

    y1 = Tk_CanvasPsY(canvas, stripPtr->bbox[1]);
    y2 = Tk_CanvasPsY(canvas, stripPtr->bbox[3]);

    /*
     * Generate a string that creates a path for the stripchart.
     * This is the only part of the procedure's code that is type-
     * specific.
     */

    sprintf(pathCmd1, "%.15g %.15g moveto %.15g 0 rlineto 0 %.15g rlineto %.15g 0 rlineto closepath\n",
	    stripPtr->bbox[0], y1,
	    stripPtr->bbox[2]-stripPtr->bbox[0]-1, y2-y1,
	    stripPtr->bbox[0]-stripPtr->bbox[2]+1);
    
    /*
     * First draw the filled area of the stripchart.
     */

    if (stripPtr->rectColor != NULL) {
        Tcl_AppendResult(interp, pathCmd1, (char *) NULL);
        if (Tk_CanvasPsColor(interp, canvas, stripPtr->rectColor)
                != TCL_OK) {
            return TCL_ERROR;
        }
      	Tcl_AppendResult(interp, "fill\n", (char *) NULL);
    }


    /* 
     * Now draw the stripchart polygon, if there is one.
     */

    /*
     * Generate a path for the polygon's outline.
     */

    Tk_CanvasPsPath(interp, canvas, stripPtr->coordPtr, stripPtr->numPoints);

    /*
     * Fill the area of the polygon.
     */

    if (stripPtr->stripColor != NULL) {
	if (Tk_CanvasPsColor(interp, canvas, stripPtr->stripColor) 
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_AppendResult(interp, "eofill\n", (char *) NULL);
    }

    /*
     * Draw the stripline.
     */

    if (stripPtr->striplineColor != NULL) {
	Tk_CanvasPsPath(interp, canvas, stripPtr->coordPtr, 
			stripPtr->numPoints);
	
	if (Tk_CanvasPsColor(interp, canvas, stripPtr->striplineColor)
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_AppendResult(interp, "stroke\n", (char *) NULL);
    }

    /*
     * Now draw the scalelines.
     */

    /* Generate a path for the scaleline. */

    for (i = 1; i < stripPtr->scale; i++) {
	y1 = Tk_CanvasPsY(canvas, (stripPtr->bbox[3]-1-i*
	         (stripPtr->bbox[3]-stripPtr->bbox[1]-2)/stripPtr->scale));

	sprintf(pathCmd2, "%.15g %.15g moveto %.15g 0 rlineto\n",
		stripPtr->bbox[0], y1, 
		stripPtr->bbox[2]-stripPtr->bbox[0]);

	if (stripPtr->scalelineColor != NULL) {
	    Tcl_AppendResult(interp, pathCmd2, (char *) NULL);
	    if (stripPtr->scalelineStyle > 0) {
		dash = stripPtr->scalelineStyle;
		sprintf(string,
			" 0 setlinejoin 2 setlinecap [%d] 0 setdash\n", dash);
	    } else {
		sprintf(string, " 0 setlinejoin 2 setlinecap [] 0 setdash\n");
	    }
	    Tcl_AppendResult(interp, string, (char *) NULL);
	    
	    if (Tk_CanvasPsColor(interp, canvas, stripPtr->scalelineColor)
                != TCL_OK) {
		return TCL_ERROR;
	    }
	    Tcl_AppendResult(interp, "stroke\n", (char *) NULL);
	    Tcl_AppendResult(interp, "[] 0 setdash\n", (char *) NULL);
	}
    }

    /*
     * Now draw the rectangular outline, if there is one.
     */

    if (stripPtr->outlineColor != NULL) {
        Tcl_AppendResult(interp, pathCmd1, (char *) NULL);
        Tcl_AppendResult(interp,
                " 0 setlinejoin 2 setlinecap [] 0 setdash\n", (char *) NULL);
	if (Tk_CanvasPsColor(interp, canvas, stripPtr->outlineColor)
                != TCL_OK) {
            return TCL_ERROR;
        }
        Tcl_AppendResult(interp, "stroke\n", (char *) NULL);
    }

    return TCL_OK;
}

void
Tki_StripchartInit ()
{
    tagsOption.parseProc = Tk_CanvasTagsParseProc;
    tagsOption.printProc = Tk_CanvasTagsPrintProc;

    Tk_CreateItemType((Tk_ItemType *) &TkStripchartType);
}

