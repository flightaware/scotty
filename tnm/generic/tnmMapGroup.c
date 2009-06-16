/*
 * tnmMapGroup.c --
 *
 *	This file implements group map items.
 *
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997      Gaertner Datensysteme.
 * Copyright (c) 1997-1999 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmMapGroup.c,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $
 */

#include "tnmInt.h"
#include "tnmPort.h"
#include "tnmMap.h"

/*
 * Structure to describe a group item.
 */

typedef struct TnmGroup {
    TnmMapItem header;		/* Generic stuff that's the same for all
				 * types.  MUST BE FIRST IN STRUCTURE. */
} TnmGroup;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int
GroupCmd	_ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *CONST objv[]));

/*
 * The configuration options understood by a group item.
 */

static TnmTable groupOptions[] = {
    { TNM_ITEM_OPT_ADDRESS,	"-address" },
    { TNM_ITEM_OPT_COLOR,	"-color" },
    { TNM_ITEM_OPT_CTIME,	"-ctime" },
    { TNM_ITEM_OPT_EXPIRE,	"-expire" },
    { TNM_ITEM_OPT_PARENT,	"-group" },
    { TNM_ITEM_OPT_MTIME,	"-mtime" },
    { TNM_ITEM_OPT_NAME,	"-name" },
    { TNM_ITEM_OPT_PATH,	"-path" },
    { TNM_ITEM_OPT_PRIO,	"-priority" },
    { TNM_ITEM_OPT_STORE,	"-store" },
    { TNM_ITEM_OPT_TAGS,	"-tags" },
    { 0, NULL },
};

/*
 * The structure below defines the group item type in terms of
 * procedures that can be invoked by generic item code.
 */

TnmMapItemType tnmGroupType = {
    "group",
    sizeof(TnmGroup),
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
    groupOptions,
    &tnmGroupType,
    (TnmMapItemCreateProc *) NULL,
    (TnmMapItemDeleteProc *) NULL,
    (TnmMapItemDumpProc *) NULL,
    (TnmMapItemMoveProc *) NULL,
    GroupCmd,
};

/*
 *----------------------------------------------------------------------
 *
 * GroupCmd --
 *
 *	This procedure implements the group object command.
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
GroupCmd(clientData, interp, objc, objv)
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
