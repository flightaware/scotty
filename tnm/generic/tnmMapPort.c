/*
 * tnmMapPort.c --
 *
 *	This file implements port map items.
 *
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997      Gaertner Datensysteme.
 * Copyright (c) 1997-1999 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmMapPort.c,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tnmInt.h"
#include "tnmPort.h"
#include "tnmMap.h"

/*
 * Structure to describe a port item.
 */

typedef struct TnmPort {
    TnmMapItem header;		/* Generic stuff that's the same for all
				 * types.  MUST BE FIRST IN STRUCTURE. */
} TnmPort;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int
PortCreateProc	(Tcl_Interp *interp, TnmMap *mapPtr,
			     TnmMapItem *itemPtr);
static int
PortCmd		(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);

/*
 * The configuration options understood by a port item.
 */

static TnmTable portOptions[] = {
    { TNM_ITEM_OPT_ADDRESS,	"-address" },
    { TNM_ITEM_OPT_COLOR,	"-color" },
    { TNM_ITEM_OPT_CTIME,	"-ctime" },
    { TNM_ITEM_OPT_EXPIRE,	"-expire" },
    { TNM_ITEM_OPT_MTIME,	"-mtime" },
    { TNM_ITEM_OPT_NAME,	"-name" },
    { TNM_ITEM_OPT_PARENT,	"-node" },
    { TNM_ITEM_OPT_PATH,	"-path" },
    { TNM_ITEM_OPT_PRIO,	"-priority" },
    { TNM_ITEM_OPT_STORE,	"-store" },
    { TNM_ITEM_OPT_TAGS,	"-tags" },
    { 0, NULL },
};

/*
 * The structure below defines the port item type in terms of
 * procedures that can be invoked by generic item code.
 */

TnmMapItemType tnmPortType = {
    "port",
    sizeof(TnmPort),
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
    portOptions,
    &tnmNodeType,
    PortCreateProc,
    (TnmMapItemDeleteProc *) NULL,
    (TnmMapItemDumpProc *) NULL,
    (TnmMapItemMoveProc *) NULL,
    PortCmd,
};

/*
 *----------------------------------------------------------------------
 *
 * PortCreateProc --
 *
 *	This procedure is called to create a new port item.
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
PortCreateProc(Tcl_Interp *interp, TnmMap *mapPtr, TnmMapItem *itemPtr)
{
    if (! itemPtr->parent) {
	Tcl_SetResult(interp, "-node option missing or empty", TCL_STATIC);
	return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PortCmd --
 *
 *	This procedure implements the port object command.
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
PortCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *const objv[])
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

