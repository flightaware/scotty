/*
 * tnmMibTcl.c --
 *
 *	The Tcl interface to the MIB parser.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1999 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmMibTcl.c,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

#include "tnmSnmp.h"
#include "tnmMib.h"

/*
 * A Tcl list variable which keeps the list of the MIB files loaded
 * into the Tnm extension.
 */

static Tcl_Obj *mibFilesLoaded = NULL;
Tcl_Obj *tnmMibModulesLoaded = NULL;

TCL_DECLARE_MUTEX(mibMutex)	/* To serialize access to the mib command. */
    
/*
 * Forward declarations for procedures defined later in this file:
 */

static TnmMibType*
GetMibType	_ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *objPtr));

static TnmMibNode*
GetMibNode	_ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *objPtr, 
			     TnmOid **oidPtrPtr, TnmOid *nodeOidPtr));
static int
GetMibNodeOrType _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *objPtr,
			      TnmMibType **typePtrPtr,
			      TnmMibNode **nodePtrPtr));
static TnmMibNode*
GetMibColumnNode _ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *objPtr,
			      TnmOid **oidPtrPtr, TnmOid *nodeOidPtr));
static Tcl_Obj*
GetIndexList	_ANSI_ARGS_((Tcl_Interp *interp, TnmMibNode *nodePtr,
			     TnmMibNode ***indexNodeList, int *implied));
static int
WalkTree	_ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *varName, 
			     Tcl_Obj *body, TnmMibNode* nodePtr, 
			     TnmOid *oidPtr, TnmOid *rootPtr));

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

static TnmMibType*
GetMibType(interp, objPtr)
    Tcl_Interp *interp;
    Tcl_Obj *objPtr;
{
    TnmMibType *typePtr = TnmMibFindType(Tcl_GetStringFromObj(objPtr,NULL));
    if (! typePtr) {
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			       "unknown type \"", 
			       Tcl_GetStringFromObj(objPtr, NULL),
			       "\"", (char *) NULL);
	return NULL;
    }
    return typePtr;
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

static TnmMibNode*
GetMibNode(interp, objPtr, oidPtrPtr, nodeOidPtr)
    Tcl_Interp *interp;
    Tcl_Obj *objPtr;
    TnmOid **oidPtrPtr;
    TnmOid *nodeOidPtr;
{
    TnmMibNode *nodePtr = NULL;
    TnmOid *oidPtr;

    if (oidPtrPtr) {
	*oidPtrPtr = NULL;
    }
    oidPtr = TnmGetOidFromObj(interp, objPtr);
    if (oidPtr) { 
	nodePtr = TnmMibNodeFromOid(oidPtr, nodeOidPtr);
    }
    if (! nodePtr || oidPtr->length == 0) {
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			       "unknown MIB node \"", 
			       Tcl_GetStringFromObj(objPtr, NULL),
			       "\"", (char *) NULL);
	return NULL;
    }
    if (oidPtrPtr) {
	*oidPtrPtr = oidPtr;
    }
    return nodePtr;
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
GetMibNodeOrType(interp, objPtr, typePtrPtr, nodePtrPtr)
    Tcl_Interp *interp;
    Tcl_Obj *objPtr;
    TnmMibType **typePtrPtr;
    TnmMibNode **nodePtrPtr;
{
    *nodePtrPtr = (TnmMibNode *) NULL;
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
 * GetMibColumnNode --
 *
 *	This procedure tries to convert the argument in objPtr into
 *	a MIB node pointer. The MIB node pointed to must be a column
 *	of a conceptual table.
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

static TnmMibNode*
GetMibColumnNode(interp, objPtr, oidPtrPtr, nodeOidPtr)
    Tcl_Interp *interp;
    Tcl_Obj *objPtr;
    TnmOid **oidPtrPtr;
    TnmOid *nodeOidPtr;
{
    TnmMibNode *nodePtr;
    
    nodePtr = GetMibNode(interp, objPtr, oidPtrPtr, nodeOidPtr);
    if (! nodePtr) {
	return NULL;
    }
    if (nodePtr->macro != TNM_MIB_OBJECTTYPE) {
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			       "no object type definition for \"",
			       Tcl_GetStringFromObj(objPtr, NULL),
			       "\"", (char *) NULL);
	return NULL;
    }
    if (nodePtr->syntax == ASN1_SEQUENCE
	|| nodePtr->syntax == ASN1_SEQUENCE_OF
	|| nodePtr->parentPtr == NULL
	|| nodePtr->parentPtr->syntax != ASN1_SEQUENCE) {
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			       "no columnar object type \"",
			       Tcl_GetStringFromObj(objPtr, NULL),
			       "\"", (char *) NULL);
	return NULL;
    }
    
    return nodePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * GetIndexList --
 *
 *	This procedure returns the retrieves the list of INDEX objects
 *	for the MIB node identified by nodePtr. The nodePtr might point
 *	to a "table", an "entry" or a "columnar" MIB object type.
 *
 * Results:
 *	This procedure returns a pointer to a Tcl_Obj which contains
 *	the OIDs for the index list or a NULL pointer if nodePtr does
 *	not resolve to a table object. The procedure allocates a NULL
 *	terminated array with pointers to the TnmMibNode structures
 *	for the index objects if the indexNodeList parameter is not
 *	NULL. The procedure in addition returns the IMPLIED bit in the
 *	implied parameter if it is not NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
GetIndexList(interp, nodePtr, indexNodeList, implied)
    Tcl_Interp *interp;
    TnmMibNode *nodePtr;
    TnmMibNode ***indexNodeList;
    int *implied;
{
    int i, code = TCL_OK, idxc;
    Tcl_Obj *idxObj, **idxv;

    if (nodePtr == NULL || nodePtr->parentPtr == NULL) {
	return NULL;
    }

    /*
     * Accept "table" nodes as well as "columnar" nodes.
     */

    if (nodePtr->syntax == ASN1_SEQUENCE_OF && nodePtr->childPtr) {
	nodePtr = nodePtr->childPtr;
    }
    if (nodePtr->syntax != ASN1_SEQUENCE && nodePtr->parentPtr
	&& nodePtr->parentPtr->syntax == ASN1_SEQUENCE) {
	nodePtr = nodePtr->parentPtr;
    }

    if (nodePtr->syntax != ASN1_SEQUENCE || nodePtr->index == NULL) {
	return NULL;
    }

    /*
     * Check whether we have a table augmentation. Make nodePtr
     * point to the node which really defines the table index.
     */

    if (nodePtr->augment) {
	TnmMibNode *entryNodePtr;
	entryNodePtr = TnmMibFindNode(nodePtr->index, NULL, 1);
	if (entryNodePtr && entryNodePtr->syntax == ASN1_SEQUENCE) {
	    nodePtr = entryNodePtr;
	} else {
	    Tcl_Panic("failed to resolve index for augmented table");
	}
    }

    idxObj = Tcl_NewStringObj(nodePtr->index, -1);
    if (idxObj) {
	code = Tcl_ListObjGetElements(NULL, idxObj, &idxc, &idxv);
    }
    if (! idxObj || code != TCL_OK) {
	Tcl_Panic("corrupted index list");
    }

    if (indexNodeList) {
	*indexNodeList = (TnmMibNode **) ckalloc((idxc+1) * sizeof(TnmMibNode));
	memset((char *) *indexNodeList, 0, (idxc+1) * sizeof(TnmMibNode));
    }
    
    for (i = 0; i < idxc; i++) {
	TnmMibNode *nPtr;
	nPtr = GetMibNode(interp, idxv[i], NULL, NULL);
	if (! nPtr) {
	    Tcl_Panic("can not resolve index list"); /* XXX */
	}
	TnmOidObjSetRep(idxv[i], TNM_OID_AS_OID);
	Tcl_InvalidateStringRep(idxv[i]);
	if (indexNodeList) {
	    (*indexNodeList)[i] = nPtr;
	}
    }

    if (implied) {
	*implied = nodePtr->implied;
    }

    Tcl_InvalidateStringRep(idxObj);
    return idxObj;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibLoadFile --
 *
 *	This procedure reads MIB definitions from a file and adds the
 *	objects to the internal MIB tree. This function expands ~
 *	filenames and searches $tnm(library)/site and $tnm(library)/mibs
 *	for the given filename. The frozen image of MIB definitions is 
 *	written into a machine specific directory.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	New frozen MIB files may be created.
 *
 *----------------------------------------------------------------------
 */

int
TnmMibLoadFile(interp, objPtr)
    Tcl_Interp *interp;
    Tcl_Obj *objPtr;
{
    Tcl_DString fileBuffer, frozenFileBuffer;
    CONST char *library, *cache, *arch;
    char *fileName, *frozenFileName = NULL;
    char *file = NULL, *module;
    int code = TCL_OK;
    Tcl_Obj *splitList;
    int splitListLen;
    Tcl_Obj *filePath = NULL;

    Tcl_DStringInit(&fileBuffer);
    Tcl_DStringInit(&frozenFileBuffer);

    if (! mibFilesLoaded) {
	mibFilesLoaded = Tcl_NewListObj(0, NULL);
    }
    if (! tnmMibModulesLoaded) {
	tnmMibModulesLoaded = Tcl_NewListObj(0, NULL);
    }

    fileName = Tcl_GetString(objPtr);	/* xxx this sucks ... */

    /*
     * Split the file argument into a path and the file name. Also,
     * get the Tnm library and cache paths and the architecture string.
     */

    splitList = Tcl_FSSplitPath(objPtr, &splitListLen);
    
    library = Tcl_GetVar2(interp, "tnm", "library", TCL_GLOBAL_ONLY);
    cache = Tcl_GetVar2(interp, "tnm", "cache", TCL_GLOBAL_ONLY);
    arch = Tcl_GetVar2(interp, "tnm", "arch", TCL_GLOBAL_ONLY);

#if 0
    /* 
     * Check if we can write a frozen file. Construct the path to the
     * directory where we keep frozen files. Create a machine specific
     * sub-directory if needed. The result of the following block is
     * the native frozen file name or NULL if we can't write a frozen
     * file.
     */

    if (cache != NULL && arch != NULL) {
	Tcl_Obj *path;
	Tcl_Obj *elem = NULL;
	Tcl_Obj *obj;

	path = Tcl_NewStringObj("", 0);
	Tcl_IncrRefCount(path);
	Tcl_AppendStringsToObj(path, cache, "/", arch, NULL);
	obj = Tcl_NewStringObj(path,-1);
	Tcl_IncrRefCount(obj);
	(void) TnmMkDir(interp, obj);
	Tcl_DecrRefCount(obj);

	Tcl_ListObjIndex(NULL, splitList, splitListLen-1, &elem);
	Tcl_AppendStringsToObj(path, cache, "/",
			       Tcl_GetStringFromObj(elem, NULL),
			       ".idy", NULL);
	frozenFileName = Tcl_TranslateFileName(interp,
				Tcl_GetStringFromObj(path, NULL),
			        &frozenFileBuffer);
	Tcl_DecrRefCount(path);
    }
#endif

    /* 
     * Search for the MIB file we are trying to load. First try the
     * file argument translated into the platform specific format. If
     * not found, check $tnm(library)/site and $tnm(library)/mibs.
     */

    if (Tcl_FSConvertToPathType(interp, objPtr) == TCL_ERROR) {
	code = TCL_ERROR;
	goto exit;
    }

    if (library && Tcl_FSAccess(objPtr, R_OK) != 0) {

	/*
	 * Copy the whole library path, append "site" or "mibs" and
	 * finally append the file name and path. This makes sure
	 * that relative paths like "sun/SUN-MIB" will be found in
	 * $tnm(library)/site/sun/SUN-MIB.
	 */

	filePath = Tcl_NewStringObj("", 0);
	Tcl_IncrRefCount(filePath);
	Tcl_AppendStringsToObj(filePath, library, "/site/",
			       Tcl_GetStringFromObj(objPtr, NULL), NULL);
	if (Tcl_FSConvertToPathType(interp, filePath) != TCL_OK
	    || Tcl_FSAccess(filePath, R_OK) != 0) {
	    Tcl_SetStringObj(filePath, "", 0);
	    Tcl_AppendStringsToObj(filePath, library, "/mibs/",
				   Tcl_GetStringFromObj(objPtr, NULL), NULL);
	    if (Tcl_FSConvertToPathType(interp, filePath) != TCL_OK
		|| Tcl_FSAccess(filePath, R_OK) != 0) {
		Tcl_AppendResult(interp, "couldn't open MIB file \"",
				 Tcl_GetStringFromObj(objPtr, NULL),
				 "\": ", Tcl_PosixError(interp),
				 (char *) NULL);
		code = TCL_ERROR;
		goto exit;
	    }
	}
	fileName = Tcl_GetString(filePath);
    }

    /*
     * First check whether this module is already loaded before we
     * call the parser. This helps to avoid memory leaks when loading
     * the same module multiple times.
     */

    if (fileName) {
	int i, objc, code;
	Tcl_Obj **objv;
	
	code = Tcl_ListObjGetElements(NULL, mibFilesLoaded,
				      &objc, &objv);
	if (code != TCL_OK) {
	    Tcl_Panic("currupted internal list mibFilesLoaded");
	}

	for (i = 0; i < objc; i++) {
	    char *s = Tcl_GetStringFromObj(objv[i], NULL);
	    char *t = Tcl_GetStringFromObj(objPtr, NULL);
	    if (strcmp(s, t) == 0) {
		goto exit;
	    }	
	}
    }

    /*
     * If we have the file name now, call the parser to do its job.
     */
    
    if (fileName) {
	module = TnmMibParse(fileName, frozenFileName);
	if (module == NULL) {
	    Tcl_AppendResult(interp, "couldn't parse MIB file \"",
			     fileName,"\"", (char *) NULL);
	    code = TCL_ERROR;
	} else {
	    Tcl_ListObjAppendElement(NULL, mibFilesLoaded, objPtr);
	    Tcl_ListObjAppendElement(NULL, tnmMibModulesLoaded,
				     Tcl_NewStringObj(module, -1));
	}
    } else {
	Tcl_AppendResult(interp, "couldn't open MIB file \"", file,
			 "\": ", Tcl_PosixError(interp), (char *) NULL);
	code = TCL_ERROR;
    }

 exit:
    /* 
     * Free up all the memory that we have allocated.
     */

    Tcl_DStringFree(&fileBuffer);
    Tcl_DStringFree(&frozenFileBuffer);
    if (filePath != NULL) {
	Tcl_DecrRefCount(filePath);
    }
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibLoadCore --
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

int
TnmMibLoadCore(interp)
    Tcl_Interp *interp;
{
    Tcl_Obj *listPtr, *part1Ptr, *part2Ptr, **objv;
    int i, objc;
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
	if (TnmMibLoadFile(interp, objv[i]) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    alreadyDone = 1;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibLoad --
 *
 *	This procedure reads the set of default MIB definitions and
 *	adds the objects to the internal MIB tree. The set of default
 *	MIB definitions is taken from the global Tcl variables
 *	tnm(mibs:core) and tnm(mibs).
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmMibLoad(interp)
    Tcl_Interp *interp;
{
    Tcl_Obj *listPtr, *part1Ptr, *part2Ptr, **objv;
    int i, objc;
    static int alreadyDone = 0;

    if (alreadyDone) {
	return TCL_OK;
    }

    if (TnmMibLoadCore(interp) != TCL_OK) {
	return TCL_ERROR;
    }

    part1Ptr = Tcl_NewStringObj("tnm", -1);
    part2Ptr = Tcl_NewStringObj("mibs", -1);
    listPtr = Tcl_ObjGetVar2(interp, part1Ptr, part2Ptr, TCL_GLOBAL_ONLY);
    Tcl_DecrRefCount(part1Ptr);
    Tcl_DecrRefCount(part2Ptr);
    if (! listPtr) {
	return TCL_OK;
    }
    if (Tcl_ListObjGetElements(interp, listPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }

    for (i = 0; i < objc; i++) {
	if (TnmMibLoadFile(interp, objv[i]) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    alreadyDone = 1;
    return TCL_OK;
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
WalkTree(interp, varName, body, nodePtr, oidPtr, rootPtr)
    Tcl_Interp *interp;
    Tcl_Obj *varName;
    Tcl_Obj *body;
    TnmMibNode* nodePtr;
    TnmOid *oidPtr;
    TnmOid *rootPtr;
{
    int result = TCL_OK;
    int length = TnmOidGetLength(oidPtr);

    for (; nodePtr; nodePtr = nodePtr->nextPtr) {

	TnmOidSet(oidPtr, length-1, nodePtr->subid);
	if (! TnmOidInTree(rootPtr, oidPtr)) break;

	if (!Tcl_ObjSetVar2(interp, varName, NULL, TnmNewOidObj(oidPtr), 
			    TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1)) {
	    result = TCL_ERROR;
	    goto loopDone;
	}

	result = Tcl_EvalObj(interp, body);
        if ((result == TCL_OK || result == TCL_CONTINUE) 
	    && nodePtr->childPtr) {
	    TnmOidSetLength(oidPtr, length+1);
	    result = WalkTree(interp, varName, body, nodePtr->childPtr, 
			      oidPtr, rootPtr);
	    TnmOidSetLength(oidPtr, length);
	}

	if (result != TCL_OK) {
	    if (result == TCL_CONTINUE) {
		result = TCL_OK;
	    } else if (result == TCL_BREAK) {
		goto loopDone;
	    } else if (result == TCL_ERROR) {
		char msg[100];
		sprintf(msg, "\n    (\"mib walk\" body line %d)",
			Tcl_GetErrorLine(interp));
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
 * Tnm_MibObjCmd --
 *
 *	This procedure is invoked to process the "mib" command.
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
Tnm_MibObjCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int	objc;
    Tcl_Obj *CONST objv[];
{
    TnmMibNode *nodePtr;
    TnmMibType *typePtr;
    TnmOid *oidPtr;
    Tcl_Obj *objPtr, *listPtr;
    char *result = NULL;
    static int initialized = 0;
    int code;

    enum commands {
	cmdAccess, cmdChild, cmdCompare, cmdDefval, cmdDescr, 
	cmdDisplay, cmdEnums, cmdExists, cmdFile, cmdFormat, cmdIndex,
	cmdInfo, cmdLabel, cmdLength, cmdLoad, cmdMacro,
	cmdMember, cmdModule, cmdName, cmdOid, cmdPack, cmdParent,
	cmdRange, cmdScan, cmdSize, cmdSplit, cmdStatus, cmdSubtree,
	cmdSyntax, cmdType, cmdUnpack, cmdVariables, cmdWalk
    } cmd;

    static CONST char *cmdTable[] = {
	"access", "children", "compare", "defval", "description", 
	"displayhint", "enums", "exists", "file", "format", "index",
	"info", "label", "length", "load", "macro", 
	"member", "module", "name", "oid", "pack", "parent",
	"range", "scan", "size", "split", "status", "subtree",
	"syntax", "type", "unpack", "variables", "walk",
	(char *) NULL
    };

    enum infos {
	infoAccess, infoFiles, infoMacros, infoModules, infoStatus, infoTypes
    } info;

    static CONST char *infoTable[] = {
	"access", "files", "macros", "modules", "status", "types",
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

#ifdef LIBSMI
    smiInit("scotty");
#endif

    /*
     * Auto-load the default set of MIB definitions, if not initialized
     * yet. This makes use of the global Tcl variable tnm(mibs).
     */

    Tcl_MutexLock(&mibMutex);
    if (! initialized) {
	if (TnmMibLoadCore(interp) != TCL_OK) {
	    Tcl_MutexUnlock(&mibMutex);
	    return TCL_ERROR;
	}
	if (strcmp(Tcl_GetStringFromObj(objv[1], NULL), "load") != 0) {
	    if (TnmMibLoad(interp) != TCL_OK) {
		Tcl_MutexUnlock(&mibMutex);
		return TCL_ERROR;
	    }
	}
	initialized = 1;
    }
    Tcl_MutexUnlock(&mibMutex);

    switch (cmd) {
    case cmdWalk: {
	int code;
	TnmOid nodeOid, rootOid;
	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varName node command");
	    return TCL_ERROR;
	}
	TnmOidInit(&nodeOid);
	TnmOidInit(&rootOid);
	Tcl_MutexLock(&mibMutex);
	nodePtr = GetMibNode(interp, objv[3], NULL, &nodeOid);
	Tcl_MutexUnlock(&mibMutex);
	if (! nodePtr) {
	    TnmOidFree(&nodeOid);
            return TCL_ERROR;
        }
	TnmOidCopy(&rootOid, &nodeOid);
	Tcl_MutexLock(&mibMutex);
	code = WalkTree(interp, objv[2], objv[4], nodePtr, &nodeOid, &rootOid);
	Tcl_MutexUnlock(&mibMutex);
	TnmOidFree(&nodeOid);
	TnmOidFree(&rootOid);
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
	code = GetMibNodeOrType(interp, objv[2], &typePtr, &nodePtr);
	Tcl_MutexUnlock(&mibMutex);
	if (code != TCL_OK) {
	    return TCL_ERROR;
	}
        if (typePtr) {
            objPtr = TnmMibScanValue(typePtr, typePtr->syntax, objv[3]);
	} else {
	    objPtr = TnmMibScanValue(nodePtr->typePtr,
				     (int) nodePtr->syntax, objv[3]);
	}
	Tcl_SetObjResult(interp, objPtr ? objPtr : objv[3]);
	break;

    case cmdFormat:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType value");
            return TCL_ERROR;
	}
	Tcl_MutexLock(&mibMutex);
	code = GetMibNodeOrType(interp, objv[2], &typePtr, &nodePtr);
	Tcl_MutexUnlock(&mibMutex);
	if (code != TCL_OK) {
	    return TCL_ERROR;
	}
	objPtr = NULL;
	if (typePtr) {
	    objPtr = TnmMibFormatValue(typePtr, typePtr->syntax, objv[3]);
	} else {
	    objPtr = TnmMibFormatValue(nodePtr->typePtr,
				       (int) nodePtr->syntax, objv[3]);
	}
	Tcl_SetObjResult(interp, objPtr ? objPtr : objv[3]);
	break;

    case cmdAccess:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "node ?varName?");
	    return TCL_ERROR;
	}
	nodePtr = GetMibNode(interp, objv[2], NULL, NULL);
	if (! nodePtr) {
            return TCL_ERROR;
        }
	result = TnmGetTableValue(tnmMibAccessTable,
				  (unsigned) nodePtr->access);
	if (objc == 4) {
	    if (result) {
		if (Tcl_ObjSetVar2(interp, objv[3], NULL,
			   Tcl_NewStringObj(result, -1),
			   TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1) == NULL) {
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

    case cmdChild: {
	TnmOid nodeOid;
	int len;
	if (objc < 3 || objc > 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "node ?varName?");
            return TCL_ERROR;
        }
	TnmOidInit(&nodeOid);
	nodePtr = GetMibNode(interp, objv[2], NULL, &nodeOid);
	if (! nodePtr) {
            return TCL_ERROR;
	}
	listPtr = Tcl_NewListObj(0, NULL);
	if (nodePtr->childPtr) {
	    Tcl_Obj *elemObjPtr;
	    int index = TnmOidGetLength(&nodeOid);
	    TnmOidAppend(&nodeOid, 0);
	    for (nodePtr = nodePtr->childPtr;
		 nodePtr;
		 nodePtr = nodePtr->nextPtr) {
		TnmOidSet(&nodeOid, index, nodePtr->subid);
		elemObjPtr = TnmNewOidObj(&nodeOid);
		TnmOidObjSetRep(elemObjPtr, TnmOidObjGetRep(objv[2]));
		Tcl_InvalidateStringRep(elemObjPtr);
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	    }
	}
	if (objc == 4) {
	    if (Tcl_ObjSetVar2(interp, objv[3], NULL, listPtr,
			       TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1) == NULL) {
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
	nodePtr = GetMibNode(interp, objv[2], NULL, NULL);
	if (! nodePtr) {
            return TCL_ERROR;
        }
	if (nodePtr->macro == TNM_MIB_OBJECTTYPE && nodePtr->index 
	    && nodePtr->syntax != ASN1_SEQUENCE_OF 
	    && nodePtr->syntax != ASN1_SEQUENCE) {
	    result = nodePtr->index;
	}
	if (objc == 4) {
	    if (result) {
		if (Tcl_ObjSetVar2(interp, objv[3], NULL,
			   Tcl_NewStringObj(result, -1),
			   TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1) == NULL) {
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
	Tcl_MutexLock(&mibMutex);
	code = GetMibNodeOrType(interp, objv[2], &typePtr, &nodePtr);
	Tcl_MutexUnlock(&mibMutex);
	if (code != TCL_OK) {
	    return TCL_ERROR;
	}
	if (typePtr) {
	    result = TnmMibGetString(typePtr->fileName, typePtr->fileOffset);
	} else {
	    result = TnmMibGetString(nodePtr->fileName, nodePtr->fileOffset);
	}
	if (objc == 4) {
	    if (result) {
		if (Tcl_ObjSetVar2(interp, objv[3], NULL,
			   Tcl_NewStringObj(result, -1),
			   TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1) == NULL) {
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

    case cmdDisplay:
	if (objc < 3 || objc > 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "type ?varName?");
            return TCL_ERROR;
        }
	typePtr = GetMibType(interp, objv[2]);
	if (! typePtr) {
            return TCL_ERROR;
        }
	if (objc == 4) {
	    if (typePtr->displayHint) {
		if (Tcl_ObjSetVar2(interp, objv[3], NULL,
			   Tcl_NewStringObj(typePtr->displayHint, -1),
			   TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1) == NULL) {
		    return TCL_ERROR;
		}
		
	    }
	    Tcl_SetBooleanObj(Tcl_GetObjResult(interp),
			      typePtr->displayHint != NULL);
	} else {
	    if (typePtr->displayHint) {
		Tcl_SetStringObj(Tcl_GetObjResult(interp), 
				 typePtr->displayHint, -1);
	    }
	}
	break;

    case cmdEnums:
	if (objc < 3 || objc > 4) {
            Tcl_WrongNumArgs(interp, 2, objv, "type ?varName?");
            return TCL_ERROR;
        }
	typePtr = GetMibType(interp, objv[2]);
	if (! typePtr) {
	    return TCL_ERROR;
	}
	listPtr = Tcl_NewListObj(0, NULL);
	if (typePtr->restKind == TNM_MIB_REST_ENUMS) {
	    Tcl_Obj *elemObjPtr;
            TnmMibRest *rPtr;
            for (rPtr = typePtr->restList; rPtr; rPtr = rPtr->nextPtr) {
		elemObjPtr = Tcl_NewStringObj(rPtr->rest.intEnum.enumLabel,-1);
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		elemObjPtr = Tcl_NewIntObj(rPtr->rest.intEnum.enumValue);
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	    }
	}
	if (objc == 4) {
	    if (Tcl_ObjSetVar2(interp, objv[3], NULL, listPtr,
			       TCL_LEAVE_ERR_MSG | TCL_PARSE_PART1) == NULL) {
		return TCL_ERROR;
	    }
	    Tcl_SetBooleanObj(Tcl_GetObjResult(interp),
			      typePtr->restKind == TNM_MIB_REST_ENUMS);
	} else {
	    Tcl_SetObjResult(interp, listPtr);
	}
	break;

    case cmdExists:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	code = GetMibNodeOrType(interp, objv[2], &typePtr, &nodePtr);
	Tcl_SetBooleanObj(Tcl_GetObjResult(interp), code == TCL_OK);
	break;

    case cmdFile:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &typePtr, &nodePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	result = typePtr ? typePtr->fileName : nodePtr->fileName;
	if (result) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), result, -1);
	}
        break;

    case cmdIndex:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "node");
	    return TCL_ERROR;
	}
	nodePtr = GetMibNode(interp, objv[2], NULL, NULL);
	if (! nodePtr) {
	    return TCL_ERROR;
	}
	objPtr = GetIndexList(interp, nodePtr, NULL, NULL);
	if (objPtr) {
	    Tcl_SetObjResult(interp, objPtr);
	}
	break;

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
	    TnmListFromTable(tnmMibAccessTable, listPtr, pattern);
	    break;
	case infoFiles:
	    TnmListFromList(mibFilesLoaded, listPtr, pattern);
	    break;
	case infoMacros:
	    TnmListFromTable(tnmMibMacroTable, listPtr, pattern);
	    break;
	case infoModules:
	    TnmListFromList(tnmMibModulesLoaded, listPtr, pattern);
	    break;
	case infoStatus:
	    TnmListFromTable(tnmMibStatusTable, listPtr, pattern);
	    break;
	case infoTypes:
	    TnmMibListTypes(pattern, listPtr);
	    break;
	}
	break;
    }

    case cmdLabel:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &typePtr, &nodePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
        if (typePtr) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), typePtr->name, -1);
	} else {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), nodePtr->label, -1);
	}
        break;

    case cmdLoad:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "file");
	    return TCL_ERROR;
	}
	return TnmMibLoadFile(interp, objv[2]);

    case cmdMacro:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &typePtr, &nodePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	result = TnmGetTableValue(tnmMibMacroTable,
		  (unsigned) (typePtr ? typePtr->macro : nodePtr->macro));
	if (result) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), result, -1);
	}
        break;

    case cmdMember:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "node");
	    return TCL_ERROR;
	}
	nodePtr = GetMibNode(interp, objv[2], NULL, NULL);
	if (! nodePtr) {
	    return TCL_ERROR;
	}
	if ((nodePtr->macro == TNM_MIB_OBJECTGROUP ||
	     nodePtr->macro == TNM_MIB_NOTIFICATIONGROUP) && nodePtr->index) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), nodePtr->index, -1);
	}
	break;

    case cmdModule:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &typePtr, &nodePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	result = typePtr ? typePtr->moduleName : nodePtr->moduleName;
	if (result) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), result, -1);
	}
        break;

    case cmdName: {
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "node");
            return TCL_ERROR;
        }
	nodePtr = GetMibNode(interp, objv[2], NULL, NULL);
	if (! nodePtr) {
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
    }

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
	TnmOid nodeOid;
	int i;
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "node");
            return TCL_ERROR;
        }
	TnmOidInit(&nodeOid);
	nodePtr = GetMibNode(interp, objv[2], NULL, &nodeOid);
	if (! nodePtr) {
	    TnmOidFree(&nodeOid);
	    return TCL_ERROR;
	}
	if (nodePtr->parentPtr && nodePtr->parentPtr->label) {
	    objPtr = Tcl_GetObjResult(interp);
	    oidPtr = TnmGetOidFromObj(interp, objPtr);
	    for (i = 0; i < TnmOidGetLength(&nodeOid) - 1; i++) {
		TnmOidAppend(oidPtr, TnmOidGet(&nodeOid, i));
	    }
	    TnmOidObjSetRep(objPtr, TnmOidObjGetRep(objv[2]));
	    Tcl_InvalidateStringRep(objPtr);
	}
	TnmOidFree(&nodeOid);
        break;
    }

    case cmdRange:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "type");
            return TCL_ERROR;
        }
	typePtr = GetMibType(interp, objv[2]);
	if (! typePtr) {
	    return TCL_ERROR;
	}
	if (typePtr->restKind == TNM_MIB_REST_RANGE) {
	    Tcl_Obj *listPtr, *elemObjPtr;
            TnmMibRest *rPtr;
	    listPtr = Tcl_GetObjResult(interp);
            for (rPtr = typePtr->restList; rPtr; rPtr = rPtr->nextPtr) {
		elemObjPtr = Tcl_NewIntObj(rPtr->rest.intRange.min);
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		elemObjPtr = Tcl_NewIntObj(rPtr->rest.intRange.max);
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	    }
	}
	break;

    case cmdSize:
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "type");
            return TCL_ERROR;
        }
	typePtr = GetMibType(interp, objv[2]);
	if (! typePtr) {
	    return TCL_ERROR;
	}
	if (typePtr->restKind == TNM_MIB_REST_SIZE) {
	    Tcl_Obj *listPtr, *elemObjPtr;
            TnmMibRest *rPtr;
	    listPtr = Tcl_GetObjResult(interp);
            for (rPtr = typePtr->restList; rPtr; rPtr = rPtr->nextPtr) {
		elemObjPtr = Tcl_NewIntObj(rPtr->rest.intRange.min);
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
		elemObjPtr = Tcl_NewIntObj(rPtr->rest.intRange.max);
		Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	    }
	}
	break;

    case cmdSplit: {
	TnmOid nodeOid;
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "oid");
            return TCL_ERROR;
        }
	TnmOidInit(&nodeOid);
	nodePtr = GetMibNode(interp, objv[2], &oidPtr, &nodeOid);
	if (! nodePtr) {
	    TnmOidFree(&nodeOid);
            return TCL_ERROR;
	}
	if (nodePtr->macro == TNM_MIB_OBJECTTYPE) {
	    int i;
	    TnmOid base, inst;
	    
	    TnmOidInit(&inst);
	    TnmOidInit(&base);
	    for (i = 0; i < TnmOidGetLength(oidPtr); i++) {
		if (i < TnmOidGetLength(&nodeOid)) {
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
	TnmOidFree(&nodeOid);
	break;
    }

    case cmdStatus: {
	if (objc != 3) {
            Tcl_WrongNumArgs(interp, 2, objv, "nodeOrType");
            return TCL_ERROR;
        }
	if (GetMibNodeOrType(interp, objv[2], &typePtr, &nodePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (typePtr) {
	    result = TnmGetTableValue(tnmMibStatusTable,
				      (unsigned) typePtr->status);
	} else {
	    result = TnmGetTableValue(tnmMibStatusTable,
				      (unsigned) nodePtr->status);
	}
	if (result) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), result, -1);
	}
        break;
	
    }

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
	if (GetMibNodeOrType(interp, objv[2], &typePtr, &nodePtr) != TCL_OK) {
	    return TCL_ERROR;
	}
        if (typePtr) {
	    result = TnmGetTableValue(tnmSnmpTypeTable, (unsigned) typePtr->syntax);
	} else {
	    if (nodePtr->macro == TNM_MIB_OBJECTTYPE) {
		if (nodePtr->typePtr && nodePtr->typePtr->name) {
		    switch (nodePtr->typePtr->macro) {
		    case TNM_MIB_OBJECTTYPE:
		    case TNM_MIB_TEXTUALCONVENTION:
		    case TNM_MIB_TYPE_ASSIGNMENT:
			result = TnmGetTableValue(tnmSnmpTypeTable, 
					  (unsigned) nodePtr->typePtr->syntax);
			break;
		    default:
			result = nodePtr->typePtr->name;
			break;
		    }
		} else {
		    result = TnmGetTableValue(tnmSnmpTypeTable,
					      (unsigned) nodePtr->syntax);
		}
	    }
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
	nodePtr = GetMibNode(interp, objv[2], NULL, NULL);
	if (! nodePtr) {
            return TCL_ERROR;
	}
	if (nodePtr->macro == TNM_MIB_OBJECTTYPE) {
	    if (nodePtr->typePtr) {
		if (nodePtr->typePtr->moduleName) {
		    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
					   nodePtr->typePtr->moduleName,
					   "!", (char *) NULL);
		}
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
				       nodePtr->typePtr->name, (char *) NULL);
	    } else {
		result = TnmGetTableValue(tnmSnmpTypeTable,
					  (unsigned) nodePtr->syntax);
		Tcl_SetStringObj(Tcl_GetObjResult(interp), result, 
				 result ? -1 : 0);
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

	code = TnmMibPack(interp, &nodeOid, objc-3, (Tcl_Obj **) objv+3,
			  implied, indexNodeList);
	if (code == TCL_OK) {
	    Tcl_SetObjResult(interp, TnmNewOidObj(&nodeOid));
	}
	ckfree((char *) indexNodeList);
	TnmOidFree(&nodeOid);
	Tcl_DecrRefCount(indexList);
	return code;
    }

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
	Tcl_DecrRefCount(indexList);
	return code;
    }
    
    case cmdVariables:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "node");
	    return TCL_ERROR;
	}
	nodePtr = GetMibNode(interp, objv[2], NULL, NULL);
	if (! nodePtr) {
	    return TCL_ERROR;
	}
	if ((nodePtr->macro == TNM_MIB_TRAPTYPE ||
	     nodePtr->macro == TNM_MIB_NOTIFICATIONTYPE) && nodePtr->index) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), nodePtr->index, -1);
	}
	break;
    }
    
    return TCL_OK;
}
    
