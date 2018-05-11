/*
 * tnmSmiTcl.c --
 *
 *	The Tcl interface to the libsmi SMI parser library.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2000 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmSmiTcl.c,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

/*
 * TODO: pack/unpack, defval
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tnmInt.h"

#ifdef HAVE_SMI_H

#include <smi.h>

#include "tnmSnmp.h"
#include "tnmMib.h"

TCL_DECLARE_MUTEX(mibMutex)	/* To serialize access to the mib command. */

/*
 * String mapping tables for libsmi constants.
 */

static TnmTable smiBasetypeTable[] = {
    { SMI_BASETYPE_UNKNOWN,		"unknown"},
    { SMI_BASETYPE_INTEGER32,		"Integer32" },
    { SMI_BASETYPE_OCTETSTRING,		"OctetString" },
    { SMI_BASETYPE_OBJECTIDENTIFIER,	"ObjectIdentifier" },
    { SMI_BASETYPE_UNSIGNED32,		"Unsigned32" },
    { SMI_BASETYPE_INTEGER64,		"Integer64" },
    { SMI_BASETYPE_UNSIGNED64,		"Unsigned64" },
    { SMI_BASETYPE_FLOAT32,		"Float32" },
    { SMI_BASETYPE_FLOAT64,		"Float64" },
    { SMI_BASETYPE_FLOAT128,		"Float128" },
    { SMI_BASETYPE_ENUM,		"Enumeration" },
    { SMI_BASETYPE_BITS,		"Bits" },
    { 0, NULL }
};

static TnmTable tnmSmiNodekindTable[] = {
    { SMI_NODEKIND_UNKNOWN,		"type" },	/* oops */
    { SMI_NODEKIND_NODE,		"node" },
    { SMI_NODEKIND_SCALAR,		"scalar" },
    { SMI_NODEKIND_TABLE,		"table" },
    { SMI_NODEKIND_ROW,			"row" },
    { SMI_NODEKIND_COLUMN,		"column" },
    { SMI_NODEKIND_NOTIFICATION,	"notification" },
    { SMI_NODEKIND_GROUP,		"group" },
    { SMI_NODEKIND_COMPLIANCE,		"compliance" },
    { 0, NULL }
};

static TnmTable tnmSmiStatusTable[] = {
    { SMI_STATUS_CURRENT,	"current" },
    { SMI_STATUS_DEPRECATED,	"deprecated" },
    { SMI_STATUS_MANDATORY,	"mandatory" },
    { SMI_STATUS_OPTIONAL,	"optional" },
    { SMI_STATUS_OBSOLETE,	"obsolete" },
    { 0, NULL }
};

static TnmTable tnmSmiAccessTable[] = {
    { SMI_ACCESS_NOT_ACCESSIBLE,	"not-accessible" },
    { SMI_ACCESS_NOTIFY,		"accessible-for-notify" },
    { SMI_ACCESS_READ_ONLY,		"read-only" },
    { SMI_ACCESS_READ_WRITE,		"read-write" },
    { 0, NULL }
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static SmiType*
GetMibType	(Tcl_Interp *interp, Tcl_Obj *objPtr);

static SmiNode*
GetMibNode	(Tcl_Interp *interp, Tcl_Obj *objPtr, 
			     TnmOid **oidPtrPtr, TnmOid *nodeOidPtr);
static int
GetMibNodeOrType (Tcl_Interp *interp, Tcl_Obj *objPtr,
			      SmiType **typePtrPtr,
			      SmiNode **nodePtrPtr);
static int
GetSmiList	(Tcl_Interp *interp, SmiNode *smiNode,
			     Tcl_Obj *objPtr);
static int
WalkTree	(Tcl_Interp *interp, Tcl_Obj *varName, 
			     Tcl_Obj *body, SmiNode *smiNode, TnmOid *oidPtr);


/*
 *----------------------------------------------------------------------
 *
 * GetMibType --
 *
 *	This procedure tries to convert the argument in objPtr into
 *	a MIB type pointer. 
 *
 * Results:
 *	This procedure returns a pointer to the MIB type or a NULL
 *	pointer if the lookup failed. An error message is left in the
 *	interpreter in case of a failed lookup.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static SmiType*
GetMibType(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    SmiType *smiType;

    smiType = smiGetType(NULL, Tcl_GetString(objPtr));
    if (! smiType) {
	char *s, *p;
	SmiNode *smiNode;
	
	s = ckstrdup(Tcl_GetString(objPtr));
	if (s[strlen(s)-1] == '_') {
	    s[strlen(s)-1] = '\0';
	    p = strrchr(s, ':');
	    p++;
	    *p = tolower((int) *p);
	}
	smiNode = smiGetNode(NULL, s);
	if (smiNode) {
	    smiType = smiGetNodeType(smiNode);
	}
	ckfree(s);
    }

    if (! smiType) {
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			       "unknown type \"", 
			       Tcl_GetString(objPtr),
			       "\"", (char *) NULL);
	return NULL;
    }
    return smiType;
}

/*
 *----------------------------------------------------------------------
 *
 * GetMibNode --
 *
 *	This procedure tries to convert the argument in objPtr into
 *	a MIB node pointer. 
 *
 * Results:
 *	This procedure returns a pointer to the MIB node or a NULL
 *	pointer if the lookup failed. The object identifier of the
 *	matching node is written to nodeOidPtr, if this pointer is not
 *	NULL. An error message is left in the interpreter in case of
 *	a failed lookup.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static SmiNode*
GetMibNode(Tcl_Interp *interp, Tcl_Obj *objPtr, TnmOid **oidPtrPtr, TnmOid *nodeOidPtr)
{
    SmiNode *smiNode = NULL;
    TnmOid *oidPtr;

    if (oidPtrPtr) {
	*oidPtrPtr = NULL;
    }
    oidPtr = TnmGetOidFromObj(interp, objPtr);
    if (oidPtr) { 
	smiNode = smiGetNodeByOID(oidPtr->length, oidPtr->elements);
    }
    if (! smiNode || oidPtr->length == 0) {
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			       "unknown MIB node \"", Tcl_GetString(objPtr),
			       "\"", (char *) NULL);
	return NULL;
    }
    if (oidPtrPtr) {
	*oidPtrPtr = oidPtr;
    }
    return smiNode;
}

/*
 *----------------------------------------------------------------------
 *
 * GetMibNodeOrType --
 *
 *	This procedure tries to convert the argument in objPtr into
 *	a MIB type or a MIB node pointer.
 *
 * Results:
 *	This procedure returns TCL_OK on success. The type pointer is
 *	left in typePtrPtr and the node pointer is left in nodePtrPtr.
 *	Only one pointer will be set. The other pointer will be set
 *	to NULL. TCL_ERROR is returned if the conversion fails and an
 *	error message is left in the interpreter.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetMibNodeOrType(Tcl_Interp *interp, Tcl_Obj *objPtr, SmiType **typePtrPtr, SmiNode **nodePtrPtr)
{
    *nodePtrPtr = (SmiNode *) NULL;
    *typePtrPtr = GetMibType(interp, objPtr);
    if (*typePtrPtr) {
	return TCL_OK;
    }
    Tcl_ResetResult(interp);
    *nodePtrPtr = GetMibNode(interp, objPtr, NULL, NULL);
    if (*nodePtrPtr) {
	return TCL_OK;
    }
    Tcl_ResetResult(interp);
    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), 
			   "unknown MIB node or type \"", 
			   Tcl_GetStringFromObj(objPtr, NULL),
			   "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * GetSmiList --
 *
 *	This procedure converts a libsmi list of nodes into a Tcl
 *	list of nodes.
 *
 * Results:
 *	This procedure returns TCL_OK on success or TCL_ERROR in
 *	case the conversion failed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetSmiList(Tcl_Interp *interp, SmiNode *smiNode, Tcl_Obj *objPtr)
{
    Tcl_Obj *listPtr, *elemObjPtr;
    SmiElement *smiElem;
    SmiNode *smiElemNode;
    SmiModule *smiModule;
    int len;
    
    listPtr = Tcl_NewListObj(0, NULL);
    for (smiElem = smiGetFirstElement(smiNode);
	 smiElem;
	 smiElem = smiGetNextElement(smiElem)) {
	smiElemNode = smiGetElementNode(smiElem);
	smiModule = smiGetNodeModule(smiElemNode);
	elemObjPtr = Tcl_NewStringObj(NULL, -1);
	Tcl_AppendStringsToObj(elemObjPtr, smiModule->name,
			       "::", smiElemNode->name, (char *) NULL);
	Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
    }
    if (objPtr) {
	if (Tcl_ObjSetVar2(interp, objPtr, NULL, listPtr,
			   TCL_LEAVE_ERR_MSG) == NULL) {
	    Tcl_DecrRefCount(listPtr);
	    return TCL_ERROR;
	}
	Tcl_ListObjLength(NULL, listPtr, &len);
	Tcl_SetObjResult(interp, Tcl_NewIntObj(len));
    } else {
	Tcl_SetObjResult(interp, listPtr);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * LoadCore --
 *
 *	This procedure reads core MIB definitions and adds the objects
 *	to the internal MIB tree. The set of core MIB definitions is
 *	taken from the global Tcl variable tnm(mibs:core).
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
LoadCore(Tcl_Interp *interp)
{
    Tcl_Obj *listPtr, *part1Ptr, *part2Ptr, **objv;
    int i, objc, cnt = 0;
    static int alreadyDone = 0;

    if (alreadyDone) {
	return TCL_OK;
    }

    part1Ptr = Tcl_NewStringObj("tnm", -1);
    part2Ptr = Tcl_NewStringObj("mibs:core", -1);
    listPtr = Tcl_ObjGetVar2(interp, part1Ptr, part2Ptr, TCL_GLOBAL_ONLY);
    if (! listPtr) {
	return TCL_OK;
    }
    if (Tcl_ListObjGetElements(interp, listPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }

    for (i = 0; i < objc; i++) {
	if (! smiLoadModule(Tcl_GetString(objv[i]))) {
	    if (i == cnt) {
		Tcl_AppendResult(interp, "failed to load", (char *) NULL);
	    }
	    Tcl_AppendResult(interp, " ", Tcl_GetString(objv[i]),
			     (char *) NULL);
	} else {
	    cnt++;
	}
    }

    alreadyDone = 1;
    return (cnt == objc) ? TCL_OK : TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * WalkTree --
 *
 *	This procedure implements a recursive MIB walk. The varName 
 *	argument defines the Tcl variable used to identify the current
 *	MIB node and label identifies the root node of the sub-tree.
 *	The current position in the MIB tree is given by nodePtr.
 *
 *	The oidPtr argument is optional and only used when the label
 *	is an object identifier and not a name. In this case, we
 *	assemble the current path in the tree in the buffer pointed to
 *	by oidPtr.
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
WalkTree(Tcl_Interp *interp, Tcl_Obj *varName, Tcl_Obj *body, SmiNode* smiNode, TnmOid *oidPtr)
{
    SmiNode *childNode;
    int result = TCL_OK;
    int length = TnmOidGetLength(oidPtr);

    for (childNode = smiGetFirstChildNode(smiNode);
	 childNode;
	 childNode = smiGetNextChildNode(childNode)) {

	TnmOidSet(oidPtr, length-1, childNode->oid[childNode->oidlen-1]);
	
	if (!Tcl_ObjSetVar2(interp, varName, NULL, TnmNewOidObj(oidPtr), 
			    TCL_LEAVE_ERR_MSG)) {
	    result = TCL_ERROR;
	    goto loopDone;
	}

	result = Tcl_EvalObj(interp, body);
        if (result == TCL_OK || result == TCL_CONTINUE) {
	    SmiNode *grandChildNode = smiGetFirstChildNode(childNode);
	    if (grandChildNode) {
		TnmOidSetLength(oidPtr, length+1);
		result = WalkTree(interp, varName, body, childNode, oidPtr);
		TnmOidSetLength(oidPtr, length);
	    }
	}

	if (result != TCL_OK) {
	    if (result == TCL_CONTINUE) {
		result = TCL_OK;
	    } else if (result == TCL_BREAK) {
		goto loopDone;
	    } else if (result == TCL_ERROR) {
		char msg[100];
		sprintf(msg, "\n    (\"mib walk\" body line %d)",
			interp->errorLine);
		Tcl_AddErrorInfo(interp, msg);
		goto loopDone;
	    } else {
		goto loopDone;
	    }
	}
    }

  loopDone:
    if (result == TCL_OK) {
	Tcl_ResetResult(interp);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_SmiObjCmd --
 *
 *	This procedure is invoked to process the "smi" command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tnm_SmiObjCmd(ClientData clientData, Tcl_Interp *interp, int	objc, Tcl_Obj *const objv[])
{
    SmiNode *smiNode;
    SmiType *smiType;
    SmiModule *smiModule;
    TnmOid *oidPtr;
    Tcl_Obj *objPtr, *listPtr;
    char *result = NULL;
    static int initialized = 0;
    int code;

    enum commands {
	cmdAccess, cmdChild, cmdCompare, cmdDefval, cmdDescr, 
	cmdDisplay, cmdEnums, cmdExists, cmdFile, cmdFormat,
	cmdIndex, cmdInfo, cmdLabel, cmdLength, cmdLoad,
	cmdMacro, cmdMember, cmdModule, cmdName, cmdOid,
	cmdPack, cmdParent, cmdRange, cmdReference, cmdScan,
	cmdSize, cmdSplit, cmdStatus, cmdSubtree, cmdSyntax,
	cmdType, cmdUnits, cmdUnpack, cmdVariables, cmdWalk
    } cmd;

    static const char *cmdTable[] = {
	"access", "children", "compare", "defval", "description", 
	"displayhint", "enums", "exists", "file", "format",
	"index", "info", "label", "length", "load",
	"macro", "member", "module", "name", "oid",
	"pack", "parent", "range", "reference", "scan",
	"size", "split", "status", "subtree", "syntax",
	"type", "units", "unpack", "variables", "walk",
	(char *) NULL
    };

    enum infos {
	infoAccess, infoMacros, infoModules, infoStatus, infoTypes
    } info;

    static const char *infoTable[] = {
	"access", "macros", "modules", "status", "types",
	(char *) NULL
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    code = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
			       "option", TCL_EXACT, (int *) &cmd);
    if (code != TCL_OK) {
	return code;
    }

    /*
     * Auto-load the default set of MIB definitions, if not initialized
     * yet. This makes use of the global Tcl variable tnm(mibs).
     */

    Tcl_MutexLock(&mibMutex);
    if (! initialized) {
	int ok = smiInit("scotty");
	smiSetErrorLevel(0);
	code = LoadCore(interp); /* xxx return value XXX */
	initialized = (ok == 0) && (code == TCL_OK);
	if (code != TCL_OK) {
	    Tcl_MutexUnlock(&mibMutex);
	    return TCL_ERROR;
	}
    }
    Tcl_MutexUnlock(&mibMutex);

    switch (cmd) {

    case cmdWalk: {
	int i, code;
	TnmOid nodeOid;
	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varName node command");
	    return TCL_ERROR;
	}
	TnmOidInit(&nodeOid);
	Tcl_MutexLock(&mibMutex);
	smiNode = GetMibNode(interp, objv[3], NULL, &nodeOid);
	if (! smiNode) {
	    TnmOidFree(&nodeOid);
	    Tcl_MutexUnlock(&mibMutex);
            return TCL_ERROR;
        }
	for (i = 0; i < smiNode->oidlen; i++) {
	    TnmOidAppend(&nodeOid, smiNode->oid[i]);
	}
	TnmOidSetLength(&nodeOid, smiNode->oidlen+1);
	code = WalkTree(interp, objv[2], objv[4], smiNode, &nodeOid);
	Tcl_MutexUnlock(&mibMutex);
	TnmOidFree(&nodeOid);
	if (code != TCL_OK && code != TCL_BREAK) {
	    return TCL_ERROR;
	}
	break;
    }

    case cmdCompare: {
	TnmOid *oidPtr1, *oidPtr2;
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "oid1 oid2");
	    return TCL_ERROR;
	}
	oidPtr1 = TnmGetOidFromObj(interp, objv[2]);
	if (! oidPtr1) { 
	    return TCL_ERROR;
	}
	oidPtr2 = TnmGetOidFromObj(interp, objv[3]);
	if (! oidPtr2) { 
	    return TCL_ERROR;
	}
	Tcl_SetIntObj(Tcl_GetObjResult(interp), 
		      TnmOidCompare(oidPtr1, oidPtr2));
	break;
    }

    case cmdScan:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType value");
	    return TCL_ERROR;
	}
	Tcl_MutexLock(&mibMutex);
	code = GetMibNodeOrType(interp, objv[2], &smiType, &smiNode);
	Tcl_MutexUnlock(&mibMutex);
	if (code != TCL_OK) {
	    return TCL_ERROR;
	}
	if (smiNode) {
	    smiType = smiGetNodeType(smiNode);
	}
	if (smiType) {
#if 0
            objPtr = TnmMibScanValue2(smiType, objv[3]);
	    Tcl_SetObjResult(interp, objPtr ? objPtr : objv[3]);
#endif
	}
	break;

    case cmdFormat:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType value");
            return TCL_ERROR;
	}
	Tcl_MutexLock(&mibMutex);
	code = GetMibNodeOrType(interp, objv[2], &smiType, &smiNode);
	Tcl_MutexUnlock(&mibMutex);
	if (code != TCL_OK) {
	    return TCL_ERROR;
	}
	if (smiNode) {
	    smiType = smiGetNodeType(smiNode);
	}
	if (smiType) {
#if 0
	    objPtr = TnmMibFormatValue2(smiType, objv[3]);
	    Tcl_SetObjResult(interp, objPtr ? objPtr : objv[3]);
#endif
	}
	break;

    case cmdAccess:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "node ?varName?");
	    return TCL_ERROR;
	}
	Tcl_MutexLock(&mibMutex);
	smiNode = GetMibNode(interp, objv[2], NULL, NULL);
	Tcl_MutexUnlock(&mibMutex);
	if (! smiNode) {
            return TCL_ERROR;
        }
	result = TnmGetTableValue(tnmSmiAccessTable, smiNode->access);
	if (objc == 4) {
	    if (result) {
		if (Tcl_ObjSetVar2(interp, objv[3], NULL,
			   Tcl_NewStringObj(result, -1),
			   TCL_LEAVE_ERR_MSG) == NULL) {
		    return TCL_ERROR;
		}
	    }
	    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), result != NULL);
	} else {
	    if (result) {
		Tcl_SetStringObj(Tcl_GetObjResult(interp), result, -1);
	    }
	}
	break;

	/* XXX add mutex locks below XXX */

    case cmdChild: {
	TnmOid oid;
	SmiNode *childNode;
	Tcl_Obj *elemObjPtr;
	int i, len;
	
	if (objc < 3 || objc > 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "node ?varName?");
            return TCL_ERROR;
        }
	smiNode = GetMibNode(interp, objv[2], NULL, NULL);
	if (! smiNode) {
            return TCL_ERROR;
	}
	listPtr = Tcl_NewListObj(0, NULL);
	for(childNode = smiGetFirstChildNode(smiNode);
	    childNode;
	    childNode = smiGetNextChildNode(childNode)) {
	    TnmOidInit(&oid);
	    for (i = 0; i < childNode->oidlen; i++) {
		TnmOidAppend(&oid, childNode->oid[i]);
	    }
	    elemObjPtr = TnmNewOidObj(&oid);
	    TnmOidObjSetRep(elemObjPtr, TnmOidObjGetRep(objv[2]));
	    Tcl_InvalidateStringRep(elemObjPtr);
	    Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	    TnmOidFree(&oid);
	}
	if (objc == 4) {
	    if (Tcl_ObjSetVar2(interp, objv[3], NULL, listPtr,
			       TCL_LEAVE_ERR_MSG) == NULL) {
		return TCL_ERROR;
	    }
	    Tcl_ListObjLength(NULL, listPtr, &len);
	    Tcl_SetObjResult(interp, Tcl_NewIntObj(len));
	} else {
	    Tcl_SetObjResult(interp, listPtr);
	}
        break;
    }

    case cmdDefval:
	if (objc < 3 || objc > 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "node ?varName?");
            return TCL_ERROR;
        }
	smiNode = GetMibNode(interp, objv[2], NULL, NULL);
	if (! smiNode) {
            return TCL_ERROR;
        }

	/* xxx */
#if 0	    
	if (nodePtr->macro == TNM_MIB_OBJECTTYPE && nodePtr->index 
	    && nodePtr->syntax != ASN1_SEQUENCE_OF 
	    && nodePtr->syntax != ASN1_SEQUENCE) {
	    result = nodePtr->index;
	}
#endif
	if (objc == 4) {
	    if (result) {
		if (Tcl_ObjSetVar2(interp, objv[3], NULL,
			   Tcl_NewStringObj(result, -1),
			   TCL_LEAVE_ERR_MSG) == NULL) {
		    return TCL_ERROR;
		}
	    }
	    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), result != NULL);
	} else {
	    if (result) {
		Tcl_SetStringObj(Tcl_GetObjResult(interp), result, -1);
	    }
	}
        break;

    case cmdDescr:
	if (objc < 3 || objc > 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType ?varName?");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &smiType, &smiNode) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (smiType) {
	    result = smiType->description;
	} else {
	    result = smiNode->description;
	}
	if (objc == 4) {
	    if (result) {
		if (Tcl_ObjSetVar2(interp, objv[3], NULL,
			   Tcl_NewStringObj(result, -1),
			   TCL_LEAVE_ERR_MSG) == NULL) {
		    return TCL_ERROR;
		}
	    }
	    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), result != NULL);
	} else {
	    if (result) {
		Tcl_SetStringObj(Tcl_GetObjResult(interp), result, -1);
	    }
	}
        break;

    case cmdDisplay:
	if (objc < 3 || objc > 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "type ?varName?");
            return TCL_ERROR;
        }
	smiType = GetMibType(interp, objv[2]);
	if (! smiType) {
            return TCL_ERROR;
        }
	if (objc == 4) {
	    if (smiType->format) {
		if (Tcl_ObjSetVar2(interp, objv[3], NULL,
			   Tcl_NewStringObj(smiType->format, -1),
			   TCL_LEAVE_ERR_MSG) == NULL) {
		    return TCL_ERROR;
		}
		
	    }
	    Tcl_SetBooleanObj(Tcl_GetObjResult(interp),
			      smiType->format != NULL);
	} else {
	    if (smiType->format) {
		Tcl_SetStringObj(Tcl_GetObjResult(interp), 
				 smiType->format, -1);
	    }
	}
	break;

    case cmdEnums: {
	SmiNamedNumber *nn;
	Tcl_Obj *elemObjPtr;
	int n;

	if (objc < 3 || objc > 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "type ?varName?");
            return TCL_ERROR;
        }
	smiType = GetMibType(interp, objv[2]);
	if (! smiType) {
	    return TCL_ERROR;
	}
	listPtr = Tcl_NewListObj(0, NULL);
	for (n = 0, nn = smiGetFirstNamedNumber(smiType);
	     nn; nn = smiGetNextNamedNumber(nn), n++) {
	    elemObjPtr = Tcl_NewStringObj(nn->name, -1);
	    Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	    elemObjPtr = Tcl_NewIntObj(nn->value.value.integer32);
	    Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	}
	if (objc == 4) {
	    if (Tcl_ObjSetVar2(interp, objv[3], NULL, listPtr,
			       TCL_LEAVE_ERR_MSG) == NULL) {
		return TCL_ERROR;
	    }
	    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), n > 0);
	} else {
	    Tcl_SetObjResult(interp, listPtr);
	}
	break;
    }

    case cmdExists:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	code = GetMibNodeOrType(interp, objv[2], &smiType, &smiNode);
	Tcl_SetBooleanObj(Tcl_GetObjResult(interp), code == TCL_OK);
	break;

    case cmdFile:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &smiType, &smiNode) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (smiType) {
	    smiModule = smiGetTypeModule(smiType);
	} else {
	    smiModule = smiGetNodeModule(smiNode);
	}
	if (smiModule && smiModule->path) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), smiModule->path, -1);
	}
        break;

    case cmdIndex:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "node ?varName?");
	    return TCL_ERROR;
	}
	smiNode = GetMibNode(interp, objv[2], NULL, NULL);
	if (! smiNode) {
	    return TCL_ERROR;
	}
	code = GetSmiList(interp, smiNode, objc == 4 ? objv[3] : NULL);
	return code;

    case cmdInfo: {
	char *pattern = NULL;
	Tcl_Obj *listPtr;
	
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "subject ?pattern?");
	    return TCL_ERROR;
	}
	code = Tcl_GetIndexFromObj(interp, objv[2], infoTable, 
				   "option", TCL_EXACT, (int *) &info);
	if (code != TCL_OK) {
	    return code;
	}
	pattern = (objc == 4) ? Tcl_GetStringFromObj(objv[3], NULL) : NULL;
	listPtr = Tcl_GetObjResult(interp);
	switch (info) {
	case infoAccess:
	    TnmListFromTable(tnmSmiAccessTable, listPtr, pattern);
	    break;
	case infoMacros:
	    TnmListFromTable(tnmSmiNodekindTable, listPtr, pattern);
	    break;
	case infoModules:
	    listPtr = Tcl_GetObjResult(interp);
	    for (smiModule = smiGetFirstModule();
		 smiModule; smiModule = smiGetNextModule(smiModule)) {
		Tcl_ListObjAppendElement(interp, listPtr,
				 Tcl_NewStringObj(smiModule->name, -1));
	    }
	    break;
	case infoStatus:
	    TnmListFromTable(tnmSmiStatusTable, listPtr, pattern);
	    break;
	case infoTypes: {
	    SmiModule *smiModule;
	    SmiType *smiType;
	    Tcl_Obj *elemObjPtr;
	    listPtr = Tcl_GetObjResult(interp);
	    for (smiModule = smiGetFirstModule();
		 smiModule;
		 smiModule = smiGetNextModule(smiModule)) {
		for (smiType = smiGetFirstType(smiModule);
		     smiType;
		     smiType = smiGetNextType(smiType)) {
		    if (smiModule->name && smiType->name) {
			elemObjPtr = Tcl_NewStringObj(NULL, 0);
			Tcl_AppendStringsToObj(elemObjPtr, smiModule->name,
					       "::", smiType->name, NULL);
			Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		    }
		}
	    }
	    break;
	}
	}
	break;
    }

    case cmdLabel:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &smiType, &smiNode) != TCL_OK) {
	    return TCL_ERROR;
	}
        if (smiType) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), smiType->name, -1);
	} else {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), smiNode->name, -1);
	}
        break;

    case cmdLoad:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "file");
	    return TCL_ERROR;
	}
	if (! smiLoadModule(Tcl_GetString(objv[2]))) {
	    Tcl_AppendResult(interp, "unable to load MIB module \"",
			     Tcl_GetString(objv[2]), "\"",
			     (char *) NULL);
	    return TCL_ERROR;
	}
	break;

    case cmdMacro:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &smiType, &smiNode) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (smiNode) {
	    result = TnmGetTableValue(tnmSmiNodekindTable, smiNode->nodekind);
	} else {
	    result = "type";
	}
	if (result) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), result, -1);
	}
        break;

    case cmdMember:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "node ?varName?");
	    return TCL_ERROR;
	}
	smiNode = GetMibNode(interp, objv[2], NULL, NULL);
	if (! smiNode) {
	    return TCL_ERROR;
	}
	if (smiNode->nodekind == SMI_NODEKIND_GROUP) {
	    code = GetSmiList(interp, smiNode, objc == 4 ? objv[3] : NULL);
	}
	return code;

    case cmdModule:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &smiType, &smiNode) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (smiType) {
	    smiModule = smiGetTypeModule(smiType);
	} else {
	    smiModule = smiGetNodeModule(smiNode);
	}
	if (smiModule && smiModule->name) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), smiModule->name, -1);
	}
        break;

    case cmdName:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "node");
            return TCL_ERROR;
        }
	smiNode = GetMibNode(interp, objv[2], NULL, NULL);
	if (! smiNode) {
	    return TCL_ERROR;
	}
	objPtr = objv[2];
	if (Tcl_IsShared(objPtr)) {
	    objPtr = Tcl_DuplicateObj(objPtr);
	}
	/*
	 * Invalidate the string representation in all cases to
	 * make sure that we return a normalized unique name.
	 */
	TnmOidObjSetRep(objPtr, TNM_OID_AS_NAME);
	Tcl_InvalidateStringRep(objPtr);
	Tcl_SetObjResult(interp, objPtr);
        break;

    case cmdOid:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "node");
            return TCL_ERROR;
        }
	oidPtr = TnmGetOidFromObj(interp, objv[2]);
	if (! oidPtr) { 
	    return TCL_ERROR;
	}
	objPtr = objv[2];
	if (Tcl_IsShared(objPtr)) {
	    objPtr = Tcl_DuplicateObj(objPtr);
	}
	/*
	 * Invalidate the string representation in all cases to
	 * make sure that hexadecimal sub-identifier are converted
	 * into decimal sub-identifier.
	 */
	TnmOidObjSetRep(objPtr, TNM_OID_AS_OID);
	Tcl_InvalidateStringRep(objPtr);
        Tcl_SetObjResult(interp, objPtr);
        break;

    case cmdParent: {
	SmiNode *parentNode;
	int i;
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "node");
            return TCL_ERROR;
        }
	smiNode = GetMibNode(interp, objv[2], NULL, NULL);
	if (! smiNode) {
	    return TCL_ERROR;
	}
	parentNode = smiGetParentNode(smiNode);
	if (parentNode) {
	    objPtr = Tcl_GetObjResult(interp);
	    oidPtr = TnmGetOidFromObj(interp, objPtr);
	    for (i = 0; i < parentNode->oidlen; i++) {
		TnmOidAppend(oidPtr, parentNode->oid[i]);
	    }
	    TnmOidObjSetRep(objPtr, TnmOidObjGetRep(objv[2]));
	    Tcl_InvalidateStringRep(objPtr);
	}
        break;
    }

    case cmdRange: {
	SmiRange *range;
	Tcl_Obj *listPtr, *elemObjPtr;
	
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "type");
            return TCL_ERROR;
        }
	smiType = GetMibType(interp, objv[2]);
	if (! smiType) {
	    return TCL_ERROR;
	}
	if ((smiType->basetype == SMI_BASETYPE_INTEGER32)
	    || (smiType->basetype == SMI_BASETYPE_UNSIGNED32)
	    || (smiType->basetype == SMI_BASETYPE_INTEGER64)
	    || (smiType->basetype == SMI_BASETYPE_UNSIGNED64)) {
	    listPtr = Tcl_GetObjResult(interp);
	    for(range = smiGetFirstRange(smiType);
		range; range = smiGetNextRange(range)) {
		elemObjPtr = Tcl_NewStringObj(
		    smiRenderValue(&range->minValue, smiType,
				   SMI_RENDER_ALL), -1);
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		elemObjPtr = Tcl_NewStringObj(
		    smiRenderValue(&range->maxValue, smiType,
				   SMI_RENDER_ALL), -1);
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	    }
	}
	break;
    }

    case cmdReference:
	if (objc < 3 || objc > 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType ?varName?");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &smiType, &smiNode) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (smiType) {
	    result = smiType->reference;
	} else {
	    result = smiNode->reference;
	}
	if (objc == 4) {
	    if (result) {
		if (Tcl_ObjSetVar2(interp, objv[3], NULL,
			   Tcl_NewStringObj(result, -1),
			   TCL_LEAVE_ERR_MSG) == NULL) {
		    return TCL_ERROR;
		}
	    }
	    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), result != NULL);
	} else {
	    if (result) {
		Tcl_SetStringObj(Tcl_GetObjResult(interp), result, -1);
	    }
	}
        break;

    case cmdSize: {
	SmiRange *range;
	Tcl_Obj *listPtr, *elemObjPtr;

	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "type");
            return TCL_ERROR;
        }
	smiType = GetMibType(interp, objv[2]);
	if (! smiType) {
	    return TCL_ERROR;
	}
	if ((smiType->basetype == SMI_BASETYPE_OCTETSTRING)
	    || (smiType->basetype == SMI_BASETYPE_OBJECTIDENTIFIER)) {
	    listPtr = Tcl_GetObjResult(interp);
	    for(range = smiGetFirstRange(smiType);
		range; range = smiGetNextRange(range)) {
		elemObjPtr = Tcl_NewStringObj(
		    smiRenderValue(&range->minValue, smiType,
				   SMI_RENDER_ALL), -1);
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		elemObjPtr = Tcl_NewStringObj(
		    smiRenderValue(&range->maxValue, smiType,
				   SMI_RENDER_ALL), -1);
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	    }
	}
	break;
    }

    case cmdSplit:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "oid");
            return TCL_ERROR;
        }
	smiNode = GetMibNode(interp, objv[2], &oidPtr, NULL);
	if (! smiNode) {
            return TCL_ERROR;
	}
	if (smiNode->nodekind == SMI_NODEKIND_SCALAR
	    || smiNode->nodekind == SMI_NODEKIND_COLUMN) {
	    int i;
	    TnmOid base, inst;
	    
	    TnmOidInit(&inst);
	    TnmOidInit(&base);
	    for (i = 0; i < TnmOidGetLength(oidPtr); i++) {
		if (i < smiNode->oidlen) {
		    TnmOidAppend(&base, TnmOidGet(oidPtr, i));
		} else {
		    TnmOidAppend(&inst, TnmOidGet(oidPtr, i));
		}
	    }
	    Tcl_ListObjAppendElement(interp, Tcl_GetObjResult(interp),
				     TnmNewOidObj(&base));
	    Tcl_ListObjAppendElement(interp, Tcl_GetObjResult(interp),
				     TnmNewOidObj(&inst));
	    TnmOidFree(&base);
	    TnmOidFree(&inst);
	}
	break;

    case cmdStatus:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &smiType, &smiNode) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (smiType) {
	    result = TnmGetTableValue(tnmSmiStatusTable, smiType->status);
	} else {
	    result = TnmGetTableValue(tnmSmiStatusTable, smiNode->status);
	}
	if (result) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), result, -1);
	}
        break;

    case cmdSubtree: {
	TnmOid *oidPtr1, *oidPtr2;
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "oid1 oid2");
	    return TCL_ERROR;
	}
	oidPtr1 = TnmGetOidFromObj(interp, objv[2]);
	if (! oidPtr1) { 
	    return TCL_ERROR;
	}
	oidPtr2 = TnmGetOidFromObj(interp, objv[3]);
	if (! oidPtr2) { 
	    return TCL_ERROR;
	}
	Tcl_SetIntObj(Tcl_GetObjResult(interp), 
		      TnmOidInTree(oidPtr1, oidPtr2));
	break;
    }

    case cmdSyntax:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &smiType, &smiNode) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (smiNode) {
	    smiType = smiGetNodeType(smiNode);
	}
        if (smiType) {
	    result = TnmGetTableValue(smiBasetypeTable, smiType->basetype);
	}
	if (result) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), result, -1);
	}
        break;

    case cmdType:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "node");
            return TCL_ERROR;
        }
	smiNode = GetMibNode(interp, objv[2], NULL, NULL);
	if (! smiNode) {
            return TCL_ERROR;
	}
	if (smiNode->nodekind == SMI_NODEKIND_SCALAR
	    || smiNode->nodekind == SMI_NODEKIND_COLUMN) {
	    smiType = smiGetNodeType(smiNode);
	    if (! smiType) {
		panic("smiNode without smiType");
	    }
	    smiModule = smiGetTypeModule(smiType);
	    if (! smiModule) {
		panic("smiType without smiModule");
	    }
	    if (smiType->decl == SMI_DECL_IMPLICIT_TYPE) {
		/*
		 * Create a fake type name for implicit types.
		 */
 		char *name = ckstrdup(smiNode->name);
		name[0] = toupper(name[0]);
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
				       smiModule->name, "::",
				       name, "_", (char *) NULL);
		ckfree(name);
	    } else {
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
				       smiModule->name, "::",
				       smiType->name, (char *) NULL);
	    }
	}
	break;

    case cmdLength:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "oid");
	    return TCL_ERROR;
	}
	oidPtr = TnmGetOidFromObj(interp, objv[2]);
	if (! oidPtr) { 
	    return TCL_ERROR;
	}
	Tcl_SetIntObj(Tcl_GetObjResult(interp), TnmOidGetLength(oidPtr));
	break;

#if 0
    case cmdPack: {
	int implied;
	TnmMibNode **indexNodeList;
	Tcl_Obj *indexList;
	TnmOid nodeOid;

	if (objc < 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "oid value ?value ...?");
            return TCL_ERROR;
        }
	TnmOidInit(&nodeOid);
	nodePtr = GetMibColumnNode(interp, objv[2], &oidPtr, &nodeOid);
	if (! nodePtr) {
	    TnmOidFree(&nodeOid);
	    return TCL_ERROR;
	}
	indexList = GetIndexList(interp, nodePtr, &indexNodeList, &implied);
	if (! indexList) {
	    TnmOidFree(&nodeOid);
	    return TCL_ERROR;
	}

	code = TnmMibPack(interp, &nodeOid, objc-3,  objv+3,
			  implied, indexNodeList);
	if (code == TCL_OK) {
	    Tcl_SetObjResult(interp, TnmNewOidObj(&nodeOid));
	}
	ckfree((char *) indexNodeList);
	return code;
    }
#endif

    case cmdUnits:
	if (objc < 3 || objc > 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType ?varName?");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &smiType, &smiNode) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (smiType) {
	    result = smiType->units;
	} else {
	    result = smiNode->units;
	}
	if (objc == 4) {
	    if (result) {
		if (Tcl_ObjSetVar2(interp, objv[3], NULL,
			   Tcl_NewStringObj(result, -1),
			   TCL_LEAVE_ERR_MSG) == NULL) {
		    return TCL_ERROR;
		}
	    }
	    Tcl_SetBooleanObj(Tcl_GetObjResult(interp), result != NULL);
	} else {
	    if (result) {
		Tcl_SetStringObj(Tcl_GetObjResult(interp), result, -1);
	    }
	}
        break;

#if 0
    case cmdUnpack: {
	int implied, code;
	TnmMibNode **indexNodeList;
	Tcl_Obj *indexList;
	TnmOid nodeOid;
	
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "oid");
            return TCL_ERROR;
        }
	TnmOidInit(&nodeOid);
	nodePtr = GetMibColumnNode(interp, objv[2], &oidPtr, &nodeOid);
	if (! nodePtr) {
	    TnmOidFree(&nodeOid);
	    return TCL_ERROR;
	}
	indexList = GetIndexList(interp, nodePtr, &indexNodeList, &implied);
	if (! indexList) {
	    TnmOidFree(&nodeOid);
	    return TCL_OK;
	}

	code = TnmMibUnpack(interp, oidPtr,
		    TnmOidGetLength(oidPtr) - TnmOidGetLength(&nodeOid),
			    implied, indexNodeList);
	
	ckfree((char *) indexNodeList);
	TnmOidFree(&nodeOid);
	return code;
    }
#endif

    case cmdVariables:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "node ?varName?");
	    return TCL_ERROR;
	}
	smiNode = GetMibNode(interp, objv[2], NULL, NULL);
	if (! smiNode) {
	    return TCL_ERROR;
	}
	if (smiNode->nodekind == SMI_NODEKIND_NOTIFICATION) {
	    code = GetSmiList(interp, smiNode, objc == 4 ? objv[3] : NULL);
	}
	return code;
    }

    return TCL_OK;
}

#endif

/*
 * Local Variables:
 * compile-command: "make -k -C ../../unix"
 * End:
 */

