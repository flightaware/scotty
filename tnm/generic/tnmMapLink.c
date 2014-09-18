/*
 * tnmMapLink.c --
 *
 *	This file implements link map items.
 *
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997      Gaertner Datensysteme.
 * Copyright (c) 1997-1999 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmMapLink.c,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $
 */

#include "tnmInt.h"
#include "tnmPort.h"
#include "tnmMap.h"

/*
 * Structure to describe a link item.
 */

typedef struct TnmLink {
    TnmMapItem header;		/* Generic stuff that's the same for all
				 * types.  MUST BE FIRST IN STRUCTURE. */
    TnmMapItem *srcPtr;		/* Starting point of this link. */
    TnmMapItem *dstPtr;		/* End point of this link. */
} TnmLink;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int
LinkCreateProc	(Tcl_Interp *interp, TnmMap *mapPtr,
			     TnmMapItem *itemPtr);
static int
LinkCmd		(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *CONST objv[]);

/*
 * The configuration options understood by a link item.
 */

static TnmTable linkOptions[] = {
    { TNM_ITEM_OPT_ADDRESS,	"-address" },
    { TNM_ITEM_OPT_COLOR,	"-color" },
    { TNM_ITEM_OPT_CTIME,	"-ctime" },
    { TNM_ITEM_OPT_DST,		"-dst" },
    { TNM_ITEM_OPT_EXPIRE,	"-expire" },
    { TNM_ITEM_OPT_PARENT,	"-group" },
    { TNM_ITEM_OPT_MTIME,	"-mtime" },
    { TNM_ITEM_OPT_NAME,	"-name" },
    { TNM_ITEM_OPT_PATH,	"-path" },
    { TNM_ITEM_OPT_PRIO,	"-priority" },
    { TNM_ITEM_OPT_SRC,		"-src" },
    { TNM_ITEM_OPT_STORE,	"-store" },
    { TNM_ITEM_OPT_TAGS,	"-tags" },
    { 0, NULL },
};

/*
 * The structure below defines the link item type in terms of
 * procedures that can be invoked by generic item code.
 */

TnmMapItemType tnmLinkType = {
    "link",
    sizeof(TnmLink),
    0,
    TNM_ITEM_CMD_CONFIG
    | TNM_ITEM_CMD_CGET
    | TNM_ITEM_CMD_MAP
    | TNM_ITEM_CMD_TYPE
    | TNM_ITEM_CMD_DESTROY
    | TNM_ITEM_CMD_MOVE
    | TNM_ITEM_CMD_ATTRIBUTE
    | TNM_ITEM_CMD_DUMP
    | TNM_ITEM_CMD_BIND
    | TNM_ITEM_CMD_RAISE
    | TNM_ITEM_CMD_HEALTH
    | TNM_ITEM_CMD_INFO
    | TNM_ITEM_CMD_MSG,
    linkOptions,
    &tnmGroupType,
    LinkCreateProc,
    (TnmMapItemDeleteProc *) NULL,
    (TnmMapItemDumpProc *) NULL,
    (TnmMapItemMoveProc *) NULL,
    LinkCmd,
};

/*
 *----------------------------------------------------------------------
 *
 * LinkCreateProc --
 *
 *	This procedure is called to create a new link item. It is 
 *	used to set the source and destination of this link.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
LinkCreateProc(interp, mapPtr, itemPtr)
    Tcl_Interp *interp;
    TnmMap *mapPtr;
    TnmMapItem *itemPtr;
{
    if (! itemPtr->srcPtr && ! itemPtr->dstPtr) {
	Tcl_SetResult(interp, "-src and -dst option missing", TCL_STATIC);
	return TCL_ERROR;
    }

    if (! itemPtr->srcPtr) {
	Tcl_SetResult(interp, "-src option missing", TCL_STATIC);
	return TCL_ERROR;
    }

    if (! itemPtr->dstPtr) {
	Tcl_SetResult(interp, "-dst option missing", TCL_STATIC);
	return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LinkCmd --
 *
 *	This procedure implements the link object command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
LinkCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST objv[];
{
    TnmMapItem *itemPtr = (TnmMapItem *) clientData;
    int result = TnmMapItemObjCmd(itemPtr, interp, objc, objv);

    if (result == TCL_CONTINUE) {
	Tcl_AppendResult(interp, "bad option \"", 
			 Tcl_GetStringFromObj(objv[1], NULL),
			 "\": should be ", (char *) NULL);
	TnmMapItemCmdList(itemPtr, interp);
	result = TCL_ERROR;
    }

    return result;
}
