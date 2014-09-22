/*
 * tnmMapUtil.c --
 *
 *	This file contains functions that are used to simplify the
 *	implementation of map item types. In fact, most of the
 *	item's functionality is implemented here. This implementation
 *	is supposed to be thread-safe.
 *
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997      Gaertner Datensysteme.
 * Copyright (c) 1997-1999 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmMapUtil.c,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $
 */

#include "tnmInt.h"
#include "tnmPort.h"
#include "tnmMap.h"

static TnmTable cmdTable[] = {
    { TNM_ITEM_CMD_ATTRIBUTE,	"attribute" },
    { TNM_ITEM_CMD_BIND,	"bind" },
    { TNM_ITEM_CMD_CGET,	"cget" },
    { TNM_ITEM_CMD_CONFIG,	"configure" },
    { TNM_ITEM_CMD_DESTROY,	"destroy" },
    { TNM_ITEM_CMD_DUMP,	"dump" },
    { TNM_ITEM_CMD_HEALTH,	"health" },
    { TNM_ITEM_CMD_INFO,	"info" },
    { TNM_ITEM_CMD_MAP,		"map" },
    { TNM_ITEM_CMD_MOVE,	"move" },
    { TNM_ITEM_CMD_MSG,		"message" },
    { TNM_ITEM_CMD_RAISE,	"raise" },
    { TNM_ITEM_CMD_TYPE,	"type" },
    { 0, NULL }
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static Tcl_Obj*
GetOption	(Tcl_Interp *interp, ClientData object, 
			     int option);
static int
SetOption	(Tcl_Interp *interp, ClientData object, 
			     int option, Tcl_Obj *objPtr);

static TnmConfig config = {
    NULL,
    SetOption,
    GetOption
};


/*
 *----------------------------------------------------------------------
 *
 * TnmMapItemConfigure --
 *
 *	This procedure is invoked to process item type specific 
 *	configuration options.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	The item is modified.
 *
 *---------------------------------------------------------------------- 
 */

int
TnmMapItemConfigure(TnmMapItem *itemPtr, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    config.optionTable = itemPtr->typePtr->configTable;
    return TnmSetConfig(interp, &config, (ClientData) itemPtr, objc, objv);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMapItemObjCmd --
 *
 *	This procedure is invoked to process item command options that
 *	are shared by most item types. The list of legal command
 *	options is controlled by a bit field in the item type
 *	structure.
 *
 * Results:
 *	A standard Tcl result. The value TCL_CONTINUE is returned if
 *	the command was not processed so that the type specific code
 *	can take whatever action is required.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------- 
 */

int
TnmMapItemObjCmd(TnmMapItem *itemPtr, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    int i, cmd, result = TCL_OK;
    char *pattern = NULL;
    ClientData *elementPtr;
    TnmMapEvent *eventPtr;
    TnmMapBind *bindPtr;
    TnmMapMsg *msgPtr;
    Tcl_Obj *listPtr;

    enum infos { 
	infoBindings, infoEvents, infoLinks, infoMember, infoMsgs 
    } info;

    static CONST char *infoTable[] = {
	"bindings", "events", "links", "member", "messages", (char *) NULL
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    /*
     * Check for the standard commands if they are mentioned in
     * the commonCmds field of the item type at hand.
     */

    cmd = TnmGetTableKey(cmdTable, Tcl_GetStringFromObj(objv[1], NULL));
    switch (cmd & itemPtr->typePtr->commonCmds) {

    case TNM_ITEM_CMD_CGET:
	config.optionTable = itemPtr->typePtr->configTable;
	result = TnmGetConfig(interp, &config, (ClientData) itemPtr, 
			      objc, objv);
	break;

    case TNM_ITEM_CMD_CONFIG:
	result = TnmMapItemConfigure(itemPtr, interp, objc, objv);
	if (result == TCL_OK && objc > 2) {
	    TnmMapCreateEvent(TNM_MAP_CONFIGURE_EVENT, itemPtr, NULL);
	}
	break;

    case TNM_ITEM_CMD_MAP:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    return TCL_ERROR;
	}

	Tcl_SetStringObj(Tcl_GetObjResult(interp),
		 Tcl_GetCommandName(interp, itemPtr->mapPtr->token), -1);
	break;

    case TNM_ITEM_CMD_MOVE:
	if (objc != 2 && objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?x y?");
	    return TCL_ERROR;
	}
	if (objc == 4) {
	    int x, y;
	    if (Tcl_GetIntFromObj(interp, objv[2], &x) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (Tcl_GetIntFromObj(interp, objv[3], &y) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (! itemPtr->mapPtr->loading) {
		Tcl_GetTime(&itemPtr->mtime);
	    }
	    itemPtr->x += x;
	    itemPtr->y += y;
	    if (itemPtr->typePtr->moveProc) {
		(itemPtr->typePtr->moveProc) (interp, itemPtr, x, y);
	    }
	    TnmMapCreateEvent(TNM_MAP_MOVE_EVENT, itemPtr, NULL);
	}
        listPtr = Tcl_GetObjResult(interp);
	Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(itemPtr->x));
	Tcl_ListObjAppendElement(interp, listPtr, Tcl_NewIntObj(itemPtr->y));
	break;

    case TNM_ITEM_CMD_TYPE:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_SetStringObj(Tcl_GetObjResult(interp), itemPtr->typePtr->name, -1);
	break;

    case TNM_ITEM_CMD_ATTRIBUTE:
	if (objc < 2 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?name ?value??");
	    return TCL_ERROR;
	}
	switch (objc) {
	case 2:
	    TnmAttrList(&itemPtr->attributes, interp);
	    break;
	case 3:
	    result = TnmAttrSet(&itemPtr->attributes, interp, 
				Tcl_GetStringFromObj(objv[2], NULL), NULL);
	    break;
	case 4:
	    TnmAttrSet(&itemPtr->attributes, interp, 
		       Tcl_GetStringFromObj(objv[2], NULL),
		       Tcl_GetStringFromObj(objv[3], NULL));
	    if (! itemPtr->mapPtr->loading) {
		Tcl_GetTime(&itemPtr->mtime);
	    }
	    TnmMapCreateEvent(TNM_MAP_ATTRIBUTE_EVENT, itemPtr, 
			      Tcl_GetStringFromObj(objv[2], NULL));
	    break;
	}
	break;

    case TNM_ITEM_CMD_DUMP:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    return TCL_ERROR;
	}
	if (itemPtr->typePtr->dumpProc) {
	    (itemPtr->typePtr->dumpProc) (interp, itemPtr);
	} else {
	    TnmMapItemDump(itemPtr, interp);
	}
	break;

    case TNM_ITEM_CMD_DESTROY:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    return TCL_ERROR;
	}
	result = Tcl_DeleteCommandFromToken(interp, itemPtr->token);
	break;

    case TNM_ITEM_CMD_BIND:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "pattern script");
            return TCL_ERROR;
	}
	bindPtr = TnmMapUserBinding(itemPtr->mapPtr, itemPtr, 
				    Tcl_GetStringFromObj(objv[2], NULL),
				    Tcl_GetStringFromObj(objv[3], NULL));
	result = (bindPtr) ? TCL_OK : TCL_ERROR;
	break;

    case TNM_ITEM_CMD_RAISE:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "event ?argument?");
	    return TCL_ERROR;
	}
	eventPtr = TnmMapCreateUserEvent(itemPtr->mapPtr, itemPtr, 
				 Tcl_GetStringFromObj(objv[2], NULL),
		   (objc == 4) ? Tcl_GetStringFromObj(objv[3], NULL) : NULL);
	if (eventPtr) {
	    TnmMapRaiseEvent(eventPtr);
	}
	break;

    case TNM_ITEM_CMD_HEALTH:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_SetIntObj(Tcl_GetObjResult(interp), itemPtr->health / 1000);
	return TCL_OK;

    case TNM_ITEM_CMD_MSG:
	return TnmMapMsgCmd(interp, itemPtr->mapPtr, itemPtr, objc, objv);

    case TNM_ITEM_CMD_INFO:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "subject ?pattern?");
            return TCL_ERROR;
	}
	result = Tcl_GetIndexFromObj(interp, objv[2], infoTable, 
				     "option", TCL_EXACT, (int *) &info);
	if (result != TCL_OK) {
	    break;
	}
	pattern = (objc == 4) ? Tcl_GetStringFromObj(objv[3], NULL) : NULL;
	listPtr = Tcl_GetObjResult(interp);
	switch (info) {
	case infoMsgs:
	    for (msgPtr = itemPtr->msgList; msgPtr; msgPtr = msgPtr->nextPtr) {
		if (pattern &&
		    !Tcl_StringMatch(Tcl_GetStringFromObj(msgPtr->tag, NULL),
				     pattern)) {
		    continue;
		}
		if (msgPtr->token) {
		    CONST char *cmdName;
		    cmdName = Tcl_GetCommandName(interp, msgPtr->token);
		    Tcl_ListObjAppendElement(interp, listPtr, 
					     Tcl_NewStringObj(cmdName, -1));
		}
	    }
	    break;
	case infoEvents:
	    for (eventPtr = itemPtr->eventList; 
		 eventPtr; eventPtr = eventPtr->nextPtr) {
		if (pattern && !Tcl_StringMatch(eventPtr->eventName, 
						pattern)) {
		    continue;
		}
		if (eventPtr->token) {
		    CONST char *cmdName;
		    cmdName = Tcl_GetCommandName(interp, eventPtr->token);
		    Tcl_ListObjAppendElement(interp, listPtr, 
					     Tcl_NewStringObj(cmdName, -1));
		}
	    }
	    break;
	case infoBindings:
	    for (bindPtr = itemPtr->bindList; 
		 bindPtr; bindPtr = bindPtr->nextPtr) {
		if (pattern && !Tcl_StringMatch(bindPtr->pattern, pattern)) {
		    continue;
		}
		if (bindPtr->type == TNM_MAP_USER_EVENT) {
		    CONST char *cmdName;
		    cmdName = Tcl_GetCommandName(interp, bindPtr->token);
		    Tcl_ListObjAppendElement(interp, listPtr, 
					     Tcl_NewStringObj(cmdName, -1));
		}
	    }
	    break;
	case infoLinks:
	    elementPtr = TnmVectorElements(&itemPtr->linkedItems);
	    for (i = 0; elementPtr[i]; i++) {
		TnmMapItem *elemPtr = (TnmMapItem *) elementPtr[i];
		CONST char *cmdName;
		cmdName = Tcl_GetCommandName(interp, elemPtr->token);
		Tcl_ListObjAppendElement(interp, listPtr, 
					 Tcl_NewStringObj(cmdName, -1));
	    }
	    break;
	case infoMember:
	    elementPtr = TnmVectorElements(&itemPtr->memberItems);
	    for (i = 0; elementPtr[i]; i++) {
                TnmMapItem *elemPtr = (TnmMapItem *) elementPtr[i];
		CONST char *cmdName;
		cmdName = Tcl_GetCommandName(interp, elemPtr->token);
		Tcl_ListObjAppendElement(interp, listPtr, 
					 Tcl_NewStringObj(cmdName, -1));
            }
	    break;
	}
	return TCL_OK;

    default:
	result = TCL_CONTINUE;
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMapItemCmdList --
 *
 *	This procedure is invoked to retrieve a comma separated list of
 *	commands understood by an item. This is usually used to build
 *	up an error response if the item command proc detects an unknown
 *	command option.
 *
 * Results:
 *	The list of default commands is appended to the result buffer
 *	of the Tcl interpreter.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------- */

void
TnmMapItemCmdList(TnmMapItem *itemPtr, Tcl_Interp *interp)
{
    TnmTable *elemPtr, *newCmdTable;
    int i;

    /*
     * We make a copy of the command table for this specific type
     * so that we can use the generic Tnm function to create the
     * list of strings in the command table.
     */

    newCmdTable = (TnmTable *) ckalloc(sizeof(cmdTable));
    memset((char *) newCmdTable, 0, sizeof(cmdTable));

    for (i = 0, elemPtr = cmdTable; elemPtr->value; elemPtr++) {
        if (elemPtr->key & itemPtr->typePtr->commonCmds) {
	    newCmdTable[i++] = *elemPtr;
        }
    }

    Tcl_AppendResult(interp, TnmGetTableValues(newCmdTable), (char *) NULL);
    ckfree((char *) newCmdTable);
}

/*
 *----------------------------------------------------------------------
 *
 * GetOption --
 *
 *	This procedure retrieves the value of a item option.
 *
 * Results:
 *	A pointer to the value formatted as a string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
GetOption(Tcl_Interp *interp, ClientData object, int option)
{
    TnmMapItem *itemPtr = (TnmMapItem *) object;
    Tcl_Obj *objPtr = NULL;

    switch (option) {
    case TNM_ITEM_OPT_COLOR:
	objPtr = itemPtr->color;
	break;
    case TNM_ITEM_OPT_FONT:
	objPtr = itemPtr->font;
	break;
    case TNM_ITEM_OPT_ICON:
	objPtr = itemPtr->icon;
	break;
    case TNM_ITEM_OPT_NAME:
	objPtr = itemPtr->name;
	break;
    case TNM_ITEM_OPT_ADDRESS:
	objPtr = Tcl_NewStringObj(itemPtr->address, -1);
	break;
    case TNM_ITEM_OPT_PRIO:
	objPtr = Tcl_NewIntObj(itemPtr->priority);
	break;
    case TNM_ITEM_OPT_SRC:
	if (itemPtr->srcPtr) {
	    objPtr = Tcl_NewStringObj(
		Tcl_GetCommandName(interp, itemPtr->srcPtr->token), -1);
	} else {
	    objPtr = Tcl_NewObj();
	}
	break;
    case TNM_ITEM_OPT_DST:
	if (itemPtr->dstPtr) {
	    objPtr = Tcl_NewStringObj(
		Tcl_GetCommandName(interp, itemPtr->dstPtr->token), -1);
	} else {
	    objPtr = Tcl_NewObj();
	}
	break;
    case TNM_ITEM_OPT_CTIME:
	objPtr = TnmNewUnsigned32Obj((TnmUnsigned32) itemPtr->ctime.sec);
	break;
    case TNM_ITEM_OPT_MTIME:
	objPtr = TnmNewUnsigned32Obj((TnmUnsigned32) itemPtr->mtime.sec);
	break;
    case TNM_ITEM_OPT_EXPIRE:
	objPtr = Tcl_NewIntObj(itemPtr->expire);
	break;
    case TNM_ITEM_OPT_PATH:
	objPtr = itemPtr->path;
	break;
    case TNM_ITEM_OPT_TAGS:
	objPtr = itemPtr->tagList;
	break;
    case TNM_ITEM_OPT_STORE:
	objPtr = itemPtr->storeList;
	break;
    case TNM_ITEM_OPT_PARENT:
	if (itemPtr->parent) {
	    objPtr = Tcl_NewStringObj(
		Tcl_GetCommandName(interp, itemPtr->parent->token), -1);
	} else {
	    objPtr = Tcl_NewObj();
	}
	break;
    }

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * SetOption --
 *
 *	This procedure modifies a single option of an item.
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
SetOption(Tcl_Interp *interp, ClientData object, int option, Tcl_Obj *objPtr)
{
    TnmMapItem *itemPtr = (TnmMapItem *) object;
    TnmMapItem *linkPtr, *iPtr;
    int code, intValue, len;
    char *val;

    switch (option) {
    case TNM_ITEM_OPT_COLOR:
	if (itemPtr->color) {
	    Tcl_DecrRefCount(itemPtr->color);
	}
	itemPtr->color = objPtr;
	Tcl_IncrRefCount(itemPtr->color);
	break;
    case TNM_ITEM_OPT_FONT:
	if (itemPtr->font) {
	    Tcl_DecrRefCount(itemPtr->font);
	}
	itemPtr->font = objPtr;
	Tcl_IncrRefCount(itemPtr->font);
	break;
    case TNM_ITEM_OPT_ICON:
	if (itemPtr->icon) {
	    Tcl_DecrRefCount(itemPtr->icon);
	}
	itemPtr->icon = objPtr;
	Tcl_IncrRefCount(itemPtr->icon);
	break;
    case TNM_ITEM_OPT_NAME:
	if (itemPtr->name) {
            Tcl_DecrRefCount(itemPtr->name);
        }
	itemPtr->name = objPtr;
	Tcl_IncrRefCount(itemPtr->name);
	if (! itemPtr->mapPtr->loading) {
	    Tcl_GetTime(&itemPtr->mtime);
	}
	break;
    case TNM_ITEM_OPT_ADDRESS:
	if (itemPtr->address) {
            ckfree(itemPtr->address);
        }
 	val = Tcl_GetStringFromObj(objPtr, &len);
 	itemPtr->address = len ? ckstrdup(val) : NULL;
	if (! itemPtr->mapPtr->loading) {
	    Tcl_GetTime(&itemPtr->mtime);
	}
	break;
    case TNM_ITEM_OPT_PRIO:
	code = TnmGetUnsignedFromObj(interp, objPtr, &intValue);
	if (code != TCL_OK) {
	    return TCL_ERROR;
	}
	if (intValue > 100) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, 
		     "expected integer in the range 0..100 but got \"",
		     Tcl_GetStringFromObj(objPtr, NULL), "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	itemPtr->priority = intValue;
	break;
    case TNM_ITEM_OPT_SRC:
	linkPtr = TnmMapFindItem(interp, itemPtr->mapPtr, 
				 Tcl_GetStringFromObj(objPtr, NULL));
	if (! linkPtr) {
	    return TCL_ERROR;
	}
	if (linkPtr->typePtr != &tnmNetworkType
	    && linkPtr->typePtr != &tnmPortType) {
	    Tcl_SetResult(interp, 
			  "-src value must be a network or a port item",
			  TCL_STATIC);
	    return TCL_ERROR;
	}
	if (itemPtr->srcPtr) {
	    TnmVectorDelete(&(itemPtr->srcPtr->linkedItems), 
			    (ClientData) itemPtr);
	}
	TnmVectorAdd(&(linkPtr->linkedItems), (ClientData) itemPtr);
	itemPtr->srcPtr = linkPtr;
	if (! itemPtr->mapPtr->loading) {
	    Tcl_GetTime(&itemPtr->mtime);
	}
	break;
    case TNM_ITEM_OPT_DST:
	linkPtr = TnmMapFindItem(interp, itemPtr->mapPtr, 
				 Tcl_GetStringFromObj(objPtr, NULL));
	if (! linkPtr) {
	    return TCL_ERROR;
	}
	if (linkPtr->typePtr != &tnmNetworkType
	    && linkPtr->typePtr != &tnmPortType) {
	    Tcl_SetResult(interp, 
			  "-dst value must be a network or a port item",
			  TCL_STATIC);
	    return TCL_ERROR;
	}
	if (itemPtr->dstPtr) {
	    TnmVectorDelete(&(itemPtr->dstPtr->linkedItems), 
			    (ClientData) itemPtr);
	}
	TnmVectorAdd(&(linkPtr->linkedItems), (ClientData) itemPtr);
	itemPtr->dstPtr = linkPtr;
	if (! itemPtr->mapPtr->loading) {
	    Tcl_GetTime(&itemPtr->mtime);
	}
	break;
    case TNM_ITEM_OPT_CTIME:
	code = TnmGetUnsignedFromObj(interp, objPtr, &intValue);
        if (code != TCL_OK) {
            return TCL_ERROR;
        }
	if (! itemPtr->mapPtr->loading) {
	    Tcl_AppendResult(interp, "the creation time-stamp is readonly",
                             (char *) NULL);
            return TCL_ERROR;
	}
	itemPtr->ctime.sec = intValue;
	break;
    case TNM_ITEM_OPT_MTIME:
	code = TnmGetUnsignedFromObj(interp, objPtr, &intValue);
        if (code != TCL_OK) {
            return TCL_ERROR;
        }
	if (! itemPtr->mapPtr->loading) {
	    Tcl_AppendResult(interp, "the modification time-stamp is readonly",
                             (char *) NULL);
            return TCL_ERROR;
	}
	itemPtr->mtime.sec = intValue;
	break;
    case TNM_ITEM_OPT_EXPIRE:
	code = TnmGetUnsignedFromObj(interp, objPtr, &intValue);
	if (code != TCL_OK) {
            return TCL_ERROR;
	}
	itemPtr->expire = intValue;
	break;
    case TNM_ITEM_OPT_PATH:
	if (itemPtr->path) {
	    Tcl_DecrRefCount(itemPtr->path);
	}
	itemPtr->path = objPtr;
	Tcl_IncrRefCount(objPtr);
	break;
    case TNM_ITEM_OPT_TAGS:
	if (itemPtr->tagList) {
	    Tcl_DecrRefCount(itemPtr->tagList);
	}
	itemPtr->tagList = objPtr;
	Tcl_IncrRefCount(itemPtr->tagList);
        if (! itemPtr->mapPtr->loading) {
	    Tcl_GetTime(&itemPtr->mtime);
	}
	break;
    case TNM_ITEM_OPT_STORE:
	if (itemPtr->storeList) {
	    Tcl_DecrRefCount(itemPtr->storeList);
	}
	itemPtr->storeList = objPtr;
	Tcl_IncrRefCount(itemPtr->storeList);
	break;
    case TNM_ITEM_OPT_PARENT:
	val = Tcl_GetStringFromObj(objPtr, &len);
	if (len == 0) {
	    if (itemPtr->parent) {
		TnmVectorDelete(&(itemPtr->parent->memberItems),
				(ClientData) itemPtr);
	    }
	    itemPtr->parent = NULL;
	    break;
	}
	linkPtr = TnmMapFindItem(interp, itemPtr->mapPtr, val);
	if (! linkPtr) {
	    return TCL_ERROR;
	}
	/*
	 * Make sure the new parent is of an acceptable type.
	 */
	if (linkPtr->typePtr != itemPtr->typePtr->parentType) {
	    Tcl_AppendResult(interp, "\"", val, "\" is not a ",
			     itemPtr->typePtr->parentType->name, " item",
			     (char *) NULL);
	    return TCL_ERROR;
	}
	/*
	 * Make sure that we do not create loops.
	 */
	for (iPtr = linkPtr; iPtr; iPtr = iPtr->parent) {
	    if (iPtr == itemPtr) {
		Tcl_AppendResult(interp, "\"", 
				 Tcl_GetCommandName(interp, itemPtr->token),
				 "\" is not allowed to be a member of \"",
				 val, "\" (loop detected)", (char *) NULL);
		return TCL_ERROR;
	    }
	}
	if (itemPtr->parent) {
	    TnmVectorDelete(&(itemPtr->parent->memberItems), 
			    (ClientData) itemPtr);
	}
	TnmVectorAdd(&(linkPtr->memberItems), (ClientData) itemPtr);
	itemPtr->parent = linkPtr;
        if (! itemPtr->mapPtr->loading) {
	    Tcl_GetTime(&itemPtr->mtime);
	}
	break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMapItemDump --
 *
 *	This procedure created a dump of the item that contains all
 *	the standard attributes and configuration option understood
 *	by the item type at hand.
 *
 * Results:
 *	The script is returned in the Tcl interpreter result buffer.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TnmMapItemDump(TnmMapItem *itemPtr, Tcl_Interp *interp)
{
    Tcl_DString ds;
    TnmTable *elemPtr;
    char *varName, buf[256];
    CONST char *name = Tcl_GetCommandName(interp, itemPtr->token);
    config.optionTable = itemPtr->typePtr->configTable;

    Tcl_DStringInit(&ds);
    varName = ckalloc(strlen(name)+2);
    strcpy(varName, "$");
    strcat(varName, name);

    Tcl_DStringAppend(&ds, "set ", -1);
    Tcl_DStringAppend(&ds, name, -1);
    Tcl_DStringAppend(&ds, " [$map create ", -1);
    Tcl_DStringAppend(&ds, itemPtr->typePtr->name, -1);
    if (config.optionTable) {
	Tcl_DStringAppend(&ds, " ", -1);
        TnmSetConfig(interp, &config, (ClientData) itemPtr, 0, NULL);
	Tcl_DStringAppend(&ds, Tcl_GetStringFromObj(Tcl_GetObjResult(interp), 
						    NULL), -1);
	Tcl_ResetResult(interp);
    }
    Tcl_DStringAppend(&ds, "]\n", 2);

    /*
     * Dump the default command settings understood by this
     * item type.
     */

    for (elemPtr = cmdTable; elemPtr->value; elemPtr++) {
	if (elemPtr->key & itemPtr->typePtr->commonCmds) {
	    switch (elemPtr->key) {
	    case TNM_ITEM_CMD_MAP:
		continue;
	    case TNM_ITEM_CMD_MOVE:
		if (itemPtr->x != 0 || itemPtr->y != 0) {
		    Tcl_DStringAppend(&ds, varName, -1);
		    Tcl_DStringAppend(&ds, " move ", -1);
		    sprintf(buf, "%d %d\n", itemPtr->x, itemPtr->y);
		    Tcl_DStringAppend(&ds, buf, -1);
		}
		break;
	    case TNM_ITEM_CMD_TYPE:
		continue;
	    case TNM_ITEM_CMD_ATTRIBUTE:
		TnmAttrDump(&itemPtr->attributes, varName, &ds);
		break;
	    case TNM_ITEM_CMD_DUMP:
		continue;
	    }
	}
    }

    ckfree(varName);
    Tcl_DStringResult(interp, &ds);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMapFindItem --
 *
 *	This procedure converts an item name into an item pointer.
 *	It makes sure that only item names that belong to the given
 *	map are accepted.
 *
 * Results:
 *	A pointer to the TnmMapItem structure or NULL if there is
 *	no item with this name in the given map.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmMapItem*
TnmMapFindItem(Tcl_Interp *interp, TnmMap *mapPtr, char *name)
{
    Tcl_CmdInfo info;
    TnmMapItem *itemPtr;
    int code;

    code = Tcl_GetCommandInfo(interp, name, &info);
    if (! code) {
    unknownItem:
	Tcl_AppendResult(interp, "unknown item \"", name, "\"",
			 (char *) NULL);
	return NULL;
    }

    /*
     * Check whether the objClientData value is actually in the list
     * of items for this map. We have to do this search to make sure
     * that we do not use a objClientData from another Tcl command as
     * a pointer to a TnmMapItem structure.
     */

    for (itemPtr = mapPtr->itemList; itemPtr; itemPtr = itemPtr->nextPtr) {
	if (itemPtr == (TnmMapItem *) info.objClientData) break;
    }
    if (! itemPtr) {
	goto unknownItem;
    }

    return itemPtr;
}

