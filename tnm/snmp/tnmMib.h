/*
 * tnmMib.h --
 *
 *	Definitions used by the SNMP MIB parser and query interface.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmMib.h,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

#ifndef _TNMMIB
#define _TNMMIB

#include "tnmInt.h"

/*
 *----------------------------------------------------------------
 * Object identifier are represented as a vector of unsigned int.
 * The maximum length of an object identifier is 128 (section 4.1
 * of RFC 1905).
 *----------------------------------------------------------------
 */

#define TNM_OID_MAX_SIZE 128

#define TNM_OID_STATIC_SIZE 16
typedef struct TnmOid {
    u_int *elements;
    short length;
    short spaceAvl;
    u_int staticSpace[TNM_OID_STATIC_SIZE];
} TnmOid;

EXTERN void
TnmOidInit		_ANSI_ARGS_((TnmOid *oidPtr));

EXTERN void
TnmOidFree		_ANSI_ARGS_((TnmOid *oidPtr));

#define TnmOidGet(oidPtr,index)		((oidPtr)->elements[index])
#define TnmOidSet(oidPtr,index,value)	((oidPtr)->elements[index] = value)
#define TnmOidGetLength(oidPtr)		((oidPtr)->length)
#define TnmOidGetElements(oidPtr)	((oidPtr)->elements)

EXTERN void
TnmOidSetLength		_ANSI_ARGS_((TnmOid *oidPtr, int length));

EXTERN int
TnmOidAppend		_ANSI_ARGS_((TnmOid *oidPtr, u_int value));

EXTERN int
TnmOidFromString	_ANSI_ARGS_((TnmOid *oidPtr, char *string));

EXTERN char*
TnmOidToString		_ANSI_ARGS_((TnmOid *oidPtr));

EXTERN void
TnmOidCopy		_ANSI_ARGS_((TnmOid *dstOidPtr, TnmOid *srcOidPtr));

EXTERN int
TnmOidCompare		_ANSI_ARGS_((TnmOid *oidPtr1, TnmOid *oidPtr2));

EXTERN int
TnmOidInTree		_ANSI_ARGS_((TnmOid *treePtr, TnmOid *oidPtr));

EXTERN int
TnmIsOid		_ANSI_ARGS_((char *str));

EXTERN char*
TnmHexToOid		_ANSI_ARGS_((char *str));

/*
 *----------------------------------------------------------------
 * Functions for the Tcl_Obj type "tnmOidObj".
 *----------------------------------------------------------------
 */

EXTERN Tcl_ObjType tnmOidType;

EXTERN Tcl_Obj*
TnmNewOidObj		_ANSI_ARGS_((TnmOid *oidPtr));

EXTERN void
TnmSetOidObj		_ANSI_ARGS_((Tcl_Obj *objPtr, TnmOid *oidPtr));

EXTERN TnmOid*
TnmGetOidFromObj	_ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *objPtr));

#define TNM_OID_AS_OID	0x00
#define TNM_OID_AS_NAME	0x01

#define TnmOidObjIsOid(objPtr) \
	(objPtr->internalRep.twoPtrValue.ptr2 == TNM_OID_AS_OID)
#define TnmOidObjGetRep(objPtr) \
	(objPtr->internalRep.twoPtrValue.ptr2)
#define TnmOidObjSetRep(objPtr,value) \
	(objPtr->internalRep.twoPtrValue.ptr2 = (VOID *) value)

/*
 *----------------------------------------------------------------
 * The TnmMibRest structure is used to store restrictions for
 * derived SNMP types. Machine readable restrictions are sizes,
 * ranges or enumerations.
 *----------------------------------------------------------------
 */

typedef struct TnmMibRest { 
    union {
	struct {
	    int enumValue;	/* The value of an enumerated integer. */
	    char *enumLabel;	/* The name of this integer value. */
	} intEnum;
	struct {
	    int min;		/* The min value of a signed integer range. */
	    int max;		/* The max value of a signed integer range. */
	} intRange;
	struct {
	    unsigned min;	/* The min value of an unsigned range. */
	    unsigned max;	/* The max value of an unsigned range. */
	} unsRange;
    } rest;
    struct TnmMibRest *nextPtr; /* Next element in the restriction list. */
} TnmMibRest;

/*
 *----------------------------------------------------------------
 * A structure to store the Textual-Convention macros. The 
 * Textual-Convention stores a format string if displayHint 
 * is NULL. If displayHint is not NULL, the Textual-Convention 
 * stores an INTEGER enumeration.
 *----------------------------------------------------------------
 */

typedef struct TnmMibType {
    char *name;			/* The name of the MIB type. */
    char *moduleName;		/* The name of the MIB module. */
    char *fileName;		/* The file with the textual description. */
    int fileOffset;		/* Offset for the textual description. */
    short syntax;		/* The ASN.1 base syntax, e.g. INTEGER. */
    char *displayHint;		/* The display hint, eg. 2d. */
    unsigned macro:4;		/* The macro used to define this type. */
    unsigned status:4;		/* The status of this definition. */
    unsigned restKind:4;	/* The kind of restriction for this type. */
    TnmMibRest *restList;	/* The list of specific restrictions. */
    struct TnmMibType *nextPtr; /* Next TnmMibType in the list. */
} TnmMibType;

/*
 *----------------------------------------------------------------
 * The following structure is used to hold a MIB tree node in 
 * memory. Every node is linked with its parent, a list of child 
 * nodes and the next node on the current MIB level.
 *----------------------------------------------------------------
 */

typedef struct TnmMibNode {
    u_int subid;		/* This node's integer subidentifier.       */
    char *label;		/* Node's textual name (not always unique). */
    char *parentName;		/* Name of parent node during parse.        */
    char *moduleName;		/* The name of the MIB module.		    */
    char *fileName;		/* The file with the textual description.   */
    int fileOffset;		/* Offset for the textual description.      */
    unsigned syntax:16;		/* This node's object type syntax.          */
    unsigned access:4;		/* The access mode of the object.	    */
    unsigned macro:4;		/* The ASN.1 macro used for the definition. */
    unsigned status:4;		/* The status of this definition.           */
    unsigned implied:1;		/* Indicates that the last index is IMPLIED */
    unsigned augment:1;		/* Indicates an AUGMENTS condition.	    */
    char *index;		/* The list of index nodes in a table entry.*/
    TnmMibType *typePtr;	/* Optional Textual Convention.		    */
    struct TnmMibNode *parentPtr; /* The parent of this node.	            */
    struct TnmMibNode *childPtr;  /* List of child nodes.	            */
    struct TnmMibNode *nextPtr;   /* List of peer nodes.		    */
} TnmMibNode;

EXTERN Tcl_Obj *tnmMibModulesLoaded;

/*
 *----------------------------------------------------------------
 * The SMI MIB tree node access modes:
 *----------------------------------------------------------------
 */

#define TNM_MIB_NOACCESS	0
#define TNM_MIB_FORNOTIFY	1
#define TNM_MIB_READONLY	2
#define TNM_MIB_READWRITE 	3
#define TNM_MIB_READCREATE	4

EXTERN TnmTable tnmMibAccessTable[];

/*
 *----------------------------------------------------------------
 * The SMI macros used to define MIB tree nodes:
 *----------------------------------------------------------------
 */

#define TNM_MIB_OBJECTTYPE		1
#define TNM_MIB_OBJECTIDENTITY		2
#define TNM_MIB_MODULEIDENTITY		3
#define TNM_MIB_NOTIFICATIONTYPE	4
#define TNM_MIB_TRAPTYPE		5
#define TNM_MIB_OBJECTGROUP		6
#define TNM_MIB_NOTIFICATIONGROUP	7
#define TNM_MIB_COMPLIANCE		8
#define TNM_MIB_CAPABILITIES		9
#define TNM_MIB_TEXTUALCONVENTION	10
#define TNM_MIB_TYPE_ASSIGNMENT		11
#define TNM_MIB_VALUE_ASSIGNEMENT	12

EXTERN TnmTable tnmMibMacroTable[];

/*
 *----------------------------------------------------------------
 * The SMI MIB definition status codes:
 *----------------------------------------------------------------
 */

#define TNM_MIB_CURRENT		1
#define TNM_MIB_DEPRECATED	2
#define TNM_MIB_OBSOLETE	3

EXTERN TnmTable tnmMibStatusTable[];

/*
 *----------------------------------------------------------------
 * The SMI MIB subtyping restrictions:
 *----------------------------------------------------------------
 */

#define TNM_MIB_REST_NONE	0
#define TNM_MIB_REST_SIZE	1
#define TNM_MIB_REST_RANGE	2
#define TNM_MIB_REST_ENUMS	3

/*
 *----------------------------------------------------------------
 * Exported variables:
 *----------------------------------------------------------------
 */

EXTERN char *tnmMibFileName;		/* Current MIB file name loaded. */
EXTERN int tnm_MibLineNumber;		/* Current MIB file line number. */
EXTERN char *tnmMibModuleName;		/* Current MIB module name loaded. */
EXTERN TnmMibNode *tnmMibTree;		/* The root of the MIB tree. */
EXTERN TnmMibType *tnmMibTypeList;	/* List of textual conventions. */
EXTERN TnmMibType *tnmMibTypeSaveMark;	/* The first already saved */
					/* element in tnmMibTypeList. */

/*
 *----------------------------------------------------------------
 * Exported functions to access the information stored 
 * in the internal MIB tree.
 *----------------------------------------------------------------
 */

EXTERN int
TnmMibLoadFile		_ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *objPtr));

EXTERN int
TnmMibLoadCore		_ANSI_ARGS_((Tcl_Interp *interp));

EXTERN int
TnmMibLoad		_ANSI_ARGS_((Tcl_Interp *interp));

EXTERN char*
TnmMibGetString		_ANSI_ARGS_((char *fileName, int fileOffset));

EXTERN TnmMibNode*
TnmMibNodeFromOid	_ANSI_ARGS_((TnmOid *oidPtr, TnmOid *nodeOidPtr));

EXTERN void
TnmMibNodeToOid		_ANSI_ARGS_((TnmMibNode *nodePtr, TnmOid *oidPtr));

EXTERN char*
TnmMibGetOid		_ANSI_ARGS_((char *name));

EXTERN char*
TnmMibGetName		_ANSI_ARGS_((char *oid,  int exact));

EXTERN int
TnmMibGetBaseSyntax	_ANSI_ARGS_((char *name));

EXTERN TnmMibNode*
TnmMibFindNode		_ANSI_ARGS_((char *name, int *offset, int exact));

EXTERN TnmMibNode*
TnmFindMibNode		_ANSI_ARGS_((TnmOid *oidPtr, char **tailPtr));

EXTERN Tcl_Obj*
TnmMibFormat		_ANSI_ARGS_((char *name, int exact, char *arg));

EXTERN char*
TnmMibScan		_ANSI_ARGS_((char *name, int exact, char *arg));

EXTERN Tcl_Obj*
TnmMibFormatValue	_ANSI_ARGS_((TnmMibType *typePtr, int syntax, 
				     Tcl_Obj *value));
EXTERN Tcl_Obj*
TnmMibScanValue		_ANSI_ARGS_((TnmMibType *typePtr, int syntax, 
				     Tcl_Obj *value));
EXTERN int
TnmMibGetValue		_ANSI_ARGS_((int syntax, Tcl_Obj *objPtr,
				     TnmMibType *typePtr, Tcl_Obj **newPtr));
EXTERN int
TnmMibPack		_ANSI_ARGS_((Tcl_Interp *interp, TnmOid *oidPtr,
				     int objc, Tcl_Obj **objv, int implied,
				     TnmMibNode **indexNodeList));
EXTERN int
TnmMibUnpack		_ANSI_ARGS_((Tcl_Interp *interp, TnmOid *oidPtr,
				     int offset, int implied,
				     TnmMibNode **indexNodeList));
/*
 *----------------------------------------------------------------
 * Functions to read a file containing MIB definitions.
 *----------------------------------------------------------------
 */

EXTERN char*
TnmMibParse		_ANSI_ARGS_((char *file, char *frozen));

EXTERN TnmMibNode*
TnmMibReadFrozen	_ANSI_ARGS_((FILE *fp));

EXTERN void
TnmMibWriteFrozen	_ANSI_ARGS_((FILE *fp, TnmMibNode *nodePtr));

/*
 *----------------------------------------------------------------
 * Functions used by the parser or the frozen file reader to
 * build the MIB tree structure(s).
 *----------------------------------------------------------------
 */

EXTERN TnmMibNode*
TnmMibNewNode		_ANSI_ARGS_((char *label));

EXTERN int
TnmMibAddNode		_ANSI_ARGS_((TnmMibNode **rootPtr, 
				     TnmMibNode *nodePtr));
EXTERN TnmMibType*
TnmMibAddType		_ANSI_ARGS_((TnmMibType *typePtr));

EXTERN TnmMibType*
TnmMibFindType		_ANSI_ARGS_((char *name));

EXTERN void
TnmMibListTypes		_ANSI_ARGS_((char *pattern, Tcl_Obj *listPtr));

#endif /* _TNMMIB */
