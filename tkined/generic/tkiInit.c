/*
 * tkiInit.c --
 *
 *	This file contains only the tkined initialization.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkined.h"
#include "tkiPort.h"

#include "../bitmaps/icon.xbm"
#include "../bitmaps/noicon.xbm"
#include "../bitmaps/node.xbm"
#include "../bitmaps/group.xbm"
#include "../bitmaps/reference.xbm"
#include "../bitmaps/graph.xbm"
#include "../bitmaps/corner.xbm"
#include "../bitmaps/network.xbm"
#include "../bitmaps/link.xbm"
#include "../bitmaps/zoomin.xbm"
#include "../bitmaps/zoomout.xbm"

#include "tkiSelect.xbm"
#include "tkiResize.xbm"
#include "tkiText.xbm"
#include "tkiNode.xbm"
#include "tkiNetwork.xbm"
#include "tkiLink.xbm"
#include "tkiGroup.xbm"
#include "tkiRefer.xbm"

/*
 * Forward declarations for procedures defined later in this file:
 */

static int 
tkined_mark_box       (ClientData clientData, Tcl_Interp *interp,
				   int argc, char **argv);
static int 
tkined_mark_points    (ClientData clientData, Tcl_Interp *interp,
				   int argc, char **argv);
static void 
mark_one_item         (Tcl_Interp *interp, double x, double y, 
				   char *canvas, char *item);
static int 
blt_axes_time	      (ClientData clientData, Tcl_Interp *interp,
				   int argc, char **argv);

/*
 * Linkage to some external funtions and global variables.
 */

extern char* tkiTcl_tcl;

int tki_Debug = 0;

/* 
 * This is called to initialize this module and to register 
 * the new commands.
 */

int
TkiInit(Tcl_Interp *interp)
{
    int code;
    char *library, *tmp;

#ifdef USE_TCL_STUBS
    if (Tcl_InitStubs(interp, "8.4", 0) == NULL) {
		return TCL_ERROR;
    }
    if (Tk_InitStubs(interp, "8.4", 0) == NULL) {
		return TCL_ERROR;
    }
#endif

    buffersize(1024);

    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }

    if (Tcl_PkgRequire(interp, "Tk", TK_VERSION, 0) == NULL) {
        return TCL_ERROR;
    }

    code = Tcl_PkgProvide(interp, "Tkined", TKI_VERSION);
    if (code != TCL_OK) {
        return TCL_ERROR;
    }

    TkiInitPath(interp);
    library = Tcl_GetVar2(interp, "tkined", "library", TCL_GLOBAL_ONLY);

    strcpy(buffer, library);
    strcat(buffer, "/library");
    if (! Tcl_SetVar(interp, "auto_path", buffer, TCL_LEAVE_ERR_MSG |
		     TCL_GLOBAL_ONLY | TCL_APPEND_VALUE | TCL_LIST_ELEMENT)) {
        return TCL_ERROR;
    }
    if (! Tcl_SetVar(interp, "auto_path", "../library", TCL_LEAVE_ERR_MSG |
		     TCL_GLOBAL_ONLY | TCL_APPEND_VALUE | TCL_LIST_ELEMENT)) {
        return TCL_ERROR;
    }

    /*
     * Set up the global tkined array variable.
     */

    Tcl_SetVar2(interp, "tkined", "version", TKI_VERSION, TCL_GLOBAL_ONLY);
    sprintf(buffer, "%d", tki_Debug);
    Tcl_SetVar2(interp, "tkined", "debug", buffer, TCL_GLOBAL_ONLY);
#if 0
    Tcl_LinkVar(interp, "tkined(debug)", (char *) &tki_Debug, 
		TCL_LINK_BOOLEAN);
#endif

    /*
     * Search for a directory which allows to hold temporary files.
     * Save the directory name in the tkined(tmp) variable.
     */

    tmp = getenv("TEMP");
    if (!tmp) {
	tmp = getenv("TMP");
	if (!tmp) {
	    tmp = "/tmp";
	    if (access(tmp, W_OK) != 0) {
		tmp = ".";
	    }
	}
    }
    Tcl_SetVar2(interp, "tkined", "tmp", tmp, TCL_GLOBAL_ONLY);

    /*
     * Initialize the stripchart and barchart canvas objects.
     */

    Tki_StripchartInit ();
    Tki_BarchartInit   ();

    Tk_DefineBitmap(interp, Tk_GetUid("icon"), (char *) icon_bits,
		    icon_width, icon_height);
    Tk_DefineBitmap(interp, Tk_GetUid("noicon"), (char *) noicon_bits,
		    noicon_width, noicon_height);
    Tk_DefineBitmap(interp, Tk_GetUid("node"), (char *) node_bits,
		    node_width, node_height);
    Tk_DefineBitmap(interp, Tk_GetUid("group"), (char *) group_bits,
		    group_width, group_height);
    Tk_DefineBitmap(interp, Tk_GetUid("reference"), (char *) reference_bits,
		    reference_width, reference_height);
    Tk_DefineBitmap(interp, Tk_GetUid("graph"), (char *) graph_bits,
		    graph_width, graph_height);
    Tk_DefineBitmap(interp, Tk_GetUid("corner"), (char *) corner_bits,
		    corner_width, corner_height);
    Tk_DefineBitmap(interp, Tk_GetUid("network"), (char *) network_bits,
		    network_width, network_height);
    Tk_DefineBitmap(interp, Tk_GetUid("link"), (char *) link_bits,
		    link_width, link_height);
    Tk_DefineBitmap(interp, Tk_GetUid("zoomin"), (char *) zoomin_bits,
		    zoomin_width, zoomin_height);
    Tk_DefineBitmap(interp, Tk_GetUid("zoomout"), (char *) zoomout_bits,
		    zoomout_width, zoomout_height);

    Tk_DefineBitmap(interp, Tk_GetUid("tkiSelect"), (char *) tkiSelect_bits,
		    tkiSelect_width, tkiSelect_height);
    Tk_DefineBitmap(interp, Tk_GetUid("tkiResize"), (char *) tkiResize_bits,
		    tkiResize_width, tkiResize_height);
    Tk_DefineBitmap(interp, Tk_GetUid("tkiText"), (char *) tkiText_bits,
		    tkiText_width, tkiText_height);
    Tk_DefineBitmap(interp, Tk_GetUid("tkiNode"), (char *) tkiNode_bits,
		    tkiNode_width, tkiNode_height);
    Tk_DefineBitmap(interp, Tk_GetUid("tkiNetwork"), (char *) tkiNetwork_bits,
		    tkiNetwork_width, tkiNetwork_height);
    Tk_DefineBitmap(interp, Tk_GetUid("tkiLink"), (char *) tkiLink_bits,
		    tkiLink_width, tkiLink_height);
    Tk_DefineBitmap(interp, Tk_GetUid("tkiGroup"), (char *) tkiGroup_bits,
		    tkiGroup_width, tkiGroup_height);
    Tk_DefineBitmap(interp, Tk_GetUid("tkiRefer"), (char *) tkiRefer_bits,
		    tkiRefer_width, tkiRefer_height);

    Tcl_CreateCommand(interp, "EDITOR", Tki_CreateEditor, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);


    Tcl_CreateCommand(interp, "NODE", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "GROUP", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "NETWORK", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "LINK", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "TEXT", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "IMAGE", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "INTERPRETER", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "MENU", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "LOG", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "REFERENCE", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "STRIPCHART", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "BARCHART", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "GRAPH", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "HTML", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);
    
    Tcl_CreateCommand(interp, "DATA", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "EVENT", Tki_CreateObject, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "tkined_mark_box", tkined_mark_box, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "tkined_mark_points", tkined_mark_points, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_CreateCommand(interp, "XLocalTime", blt_axes_time, 
		      (ClientData) NULL, (Tcl_CmdDeleteProc *) NULL);

    Tcl_DStringInit(&clip);

    Tcl_InitHashTable(&tki_ObjectTable, TCL_STRING_KEYS);    

    return TCL_OK;
}

/*
 * This one is just here because we need some simple arithmetics
 * that are very slow in TCL. So I rewrote this proc in C. It just
 * takes a canvas item and puts selection marks around it.
 */

static void 
mark_one_item(interp, x, y, canvas, item)
    Tcl_Interp *interp;
    double x,y;
    char *canvas;
    char *item;
{
    sprintf(buffer, 
	    "%s create rectangle %f %f %f %f -fill black -tags mark%s", 
	    canvas, x-1, y-1, x+1, y+1, item); 

    Tcl_Eval(interp, buffer);
}

static int 
tkined_mark_points(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    int ret;
    int largc;
    char **largv;
    int i, n;
    double *x;
    double *y;

    if (argc != 3) {
	Tcl_SetResult (interp, "wrong # of args", TCL_STATIC);
	return TCL_ERROR;
    }

    ret = Tcl_VarEval (interp, argv[1], " coords ", argv[2], (char *) NULL);
    if (ret != TCL_OK) return ret;

    Tcl_SplitList (interp, interp->result, &largc, &largv);

    x = (double *) ckalloc (largc * sizeof(double));
    y = (double *) ckalloc (largc * sizeof(double));

    if (x == NULL || y == NULL) {
	ckfree ((char*) largv);
	Tcl_SetResult (interp, "setting selection marks failed", TCL_STATIC);
	return TCL_ERROR;
    }

    for (n = 0, i = 0; i < largc; i++) {
	if ((i%2) == 0) {
	    Tcl_GetDouble (interp, largv[i], &x[n]);
	} else {
	    Tcl_GetDouble (interp, largv[i], &y[n]);
	    n++;
	}
    }

    if (x[0] > x[1]) x[0] += 4;
    if (x[0] < x[1]) x[0] -= 4;
    if (y[0] > y[1]) y[0] += 4;
    if (y[0] < y[1]) y[0] -= 4;

    if (x[n-1] > x[n-2]) x[n-1] += 4;
    if (x[n-1] < x[n-2]) x[n-1] -= 4;
    if (y[n-1] > y[n-2]) y[n-1] += 4;
    if (y[n-1] < y[n-2]) y[n-1] -= 4;

    mark_one_item (interp, x[0], y[0], argv[1], argv[2]);
    mark_one_item (interp, x[n-1], y[n-1], argv[1], argv[2]);

    ckfree ((char *) x);
    ckfree ((char *) y);
    ckfree ((char*) largv);

    return TCL_OK;
}

static int 
tkined_mark_box (ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    int ret;
    int largc;
    char **largv;
    double x1, x2, y1, y2, xm, ym;

    if (argc != 3) {
	Tcl_SetResult (interp, "wrong # of args", TCL_STATIC);
	return TCL_ERROR;
    }

    ret = Tcl_VarEval (interp, argv[1], " bbox ", argv[2], (char *) NULL);
    if (ret != TCL_OK) return ret;

    Tcl_SplitList (interp, interp->result, &largc, &largv);

    Tcl_GetDouble (interp, largv[0], &x1);
    Tcl_GetDouble (interp, largv[1], &y1);
    Tcl_GetDouble (interp, largv[2], &x2);
    Tcl_GetDouble (interp, largv[3], &y2);
    x1 -= 2; x2 += 2; y1 -= 2; y2 += 2;
    xm = (x1+x2)/2;
    ym = (y1+y2)/2;

    mark_one_item (interp, x1, y1, argv[1], argv[2]);
    mark_one_item (interp, x1, y2, argv[1], argv[2]);
    mark_one_item (interp, x2, y1, argv[1], argv[2]);
    mark_one_item (interp, x2, y2, argv[1], argv[2]);
    if ((x2-x1) > 100) {
	mark_one_item (interp, xm, y1, argv[1], argv[2]);
	mark_one_item (interp, xm, y2, argv[1], argv[2]);
    }
    if ((y2-y1) > 100) {
	mark_one_item (interp, x1, ym, argv[1], argv[2]);
	mark_one_item (interp, x2, ym, argv[1], argv[2]);
    }

    ckfree ((char*) largv);

    return TCL_OK;
}

/*
 * This callback is used by the blt graph widget to map seconds since
 * 1970 into a readable hour:minutes representation using local
 * timezone.
 */

static int
blt_axes_time(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    double val;
    time_t clock;
    struct tm *ltime;

    if (argc != 3 ) return TCL_ERROR;

    if (Tcl_GetDouble(interp, argv[2], &val) != TCL_OK) return TCL_ERROR;

    clock = (time_t) val;
    ltime = localtime(&clock);
    sprintf(interp->result, "%02d:%02d", ltime->tm_hour, ltime->tm_min);

    return TCL_OK;
}


