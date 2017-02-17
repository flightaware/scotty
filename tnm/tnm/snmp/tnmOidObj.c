/*
 * tnmOidObj.c --
 *
 *	This is the implementation of tnmOid object type for the
 *	the Tcl interpreter.
 *
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmOidObj.c,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

#include "tnmInt.h"

#ifdef HAVE_SMI_H
#include <smi.h>
#endif

#include "tnmMib.h"

/*
 * Prototypes for procedures defined later in this file:
 */

static void		
DupOidInternalRep	(Tcl_Obj *srcPtr, Tcl_Obj *copyPtr);

static void		
FreeOidInternalRep	(Tcl_Obj *objPtr);

static int
SetOidFromAny		(Tcl_Interp *interp, Tcl_Obj *objPtr);

static void
UpdateStringOfOid	(Tcl_Obj *objPtr);

/*
 * The structure below defines the tnmOid Tcl object type by means of
 * procedures that can be invoked by generic object code.
 */

Tcl_ObjType tnmOidType = {
    "tnmOid",			/* name of the type */
    FreeOidInternalRep,		/* freeIntRepProc */
    DupOidInternalRep,		/* dupIntRepProc */
    UpdateStringOfOid,		/* updateStringProc */
    SetOidFromAny		/* setFromAnyProc */
};


/*
 *----------------------------------------------------------------------
 *
 * TnmIsOid --
 *
 *	This procedure tests the given string, whether it consists 
 *	of dots and digits only.
 *
 * Results:
 *	A value != 0 is returned if the string is an object identifier
 *	in dotted notation, 0 otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmIsOid(char *string)
{
    char *c, isHex;

    for (c = string, isHex = 0; *c; c++) {
	if (*c == '.') {
	    isHex = 0;
	    if (c[1] == '0' && c[2] == 'x') {
		isHex = 1, c += 2;
	    }
	} else if (*c == ':') {
	    isHex = 1;
	} else if (isHex && ! isxdigit((int) *c)) {
	    return 0;
	} else if (! isHex && ! isdigit((int) *c)) {
	    return 0;
	}
    }
    
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmHexToOid --
 *
 *	This procedure tests whether the string contains any 
 *	hexadecimal subidentifier and returns a new string with all 
 *	hex sub-identifier expanded.
 *
 * Results:
 *	A pointer to the new expanded string of NULL if there is
 *	nothing to expand.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char*
TnmHexToOid(char *str)
{
    static char expstr[TNM_OID_MAX_SIZE * 8];
    char *p, *s;
    int convert = 0;

    if (! str) return NULL;

    /* 
     * First test if the string contains any hexadecimal subidentifier 
     * indicated by ':' or '.0x' separators that should be converted
     * to integer subidentifiers.
     */
    
    for (p = str; *p; p++) {
	if ((p[0] == ':' && isdigit(p[1]))
	    || (p[0] == '.' && p[1] == '0' && p[2] == 'x')) {
	    convert = 1;
	    break;
	}
    }

    if (! convert) {
	return NULL;
    }

    /*
     * Scan through the string and convert hexadecimal subidentifier
     * to integer subidentifier.
     */
    
    for (p = str, s = expstr; *p; ) { 
	convert = 0;
	if ((p[0] == ':' && isdigit(p[1]))
	    || (p[0] == '.' && p[1] == '0' && p[2] == 'x')) {
	    int w = 0;
	    p += (*p == ':') ? 1 : 3;
	    while (isxdigit((int) *p)) {
		char c = *p++ & 0xff;
		int v = c >= 'a' ?  c - 87 : (c >= 'A' ? c - 55 : c - 48);
		w = (w << 4) + v;
	    }
	    sprintf(s, ".%d", w);
	    while (*s != '\0') s++;
	} else {
	    *s++ = *p++;
	}
    }
    *s = '\0';
    
    return expstr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmOidInit --
 *
 *	This procedure initializes an object identifier (TnmOid).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TnmOidInit(TnmOid *oidPtr)
{
    oidPtr->elements = oidPtr->staticSpace;
    oidPtr->length = 0;
    oidPtr->spaceAvl = TNM_OID_STATIC_SIZE;
    memset((char *) oidPtr->staticSpace, 0, 
	   (oidPtr->spaceAvl) * sizeof(u_int));
}

/*
 *----------------------------------------------------------------------
 *
 * TnmOidFree --
 *
 *	This procedure frees all resources allocated for a object
 *	identifier and re-initializes the TnmOid structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TnmOidFree(TnmOid *oidPtr)
{
    if (oidPtr) {
	if (oidPtr->elements != oidPtr->staticSpace) {
	    ckfree((char *) oidPtr->elements);
	}
	oidPtr->elements = oidPtr->staticSpace;
	oidPtr->length = 0;
	oidPtr->spaceAvl = TNM_OID_STATIC_SIZE;
	memset((char *) oidPtr->staticSpace, 0,
	       (oidPtr->spaceAvl) * sizeof(u_int));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmOidSetLength --
 *
 *	This procedure frees sets the length of an object identifier
 *	to a given number. This function will allocate new memory if
 *	this is necessary in order to satisfy the requested length.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The vector of subidentifiers might be reallocated which
 *	will invalidate all pointers that refer to the object
 *	identifier elements.
 *
 *----------------------------------------------------------------------
 */

void
TnmOidSetLength(TnmOid *oidPtr, int length)
{
    if (length > oidPtr->spaceAvl) {
	int i, size;
	u_int *dynamicSpace;
	oidPtr->spaceAvl = ((length / TNM_OID_STATIC_SIZE) + 1) 
	    * TNM_OID_STATIC_SIZE;
	size = (oidPtr->spaceAvl + 1) * sizeof(u_int);
	dynamicSpace = (u_int *) ckalloc(size);
	memset((char *) dynamicSpace, 0, size);
	for (i = 0; i < length && i < oidPtr->length; i++) {
	    dynamicSpace[i] = oidPtr->elements[i];
	}
	if (oidPtr->elements != oidPtr->staticSpace) {
	    ckfree((char *) oidPtr->elements);
	}
	oidPtr->elements = dynamicSpace;
    }
    oidPtr->length = (length < 0) ? 0 : length;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmOidAppend --
 *
 *	This procedure appends a sub-identifier value to an object
 *	identifier. This function will allocate new memory if this is
 *	necessary in order to satisfy the requested length.
 *
 * Results:
 *	Returns normally TCL_OK. However, TCL_ERROR is returned if the
 *	append operation would create an object identifier value that
 *	exceeds the maximum length TNM_OID_MAX_SIZE. Note that it is
 *	safe to ignore such an overrun because the implementation
 *	supports OIDs of arbitrary size. But ignoring the maximum size
 *	might result in non SNMP conforming messages.
 *
 * Side effects:
 *	The vector of subidentifiers might be reallocated which
 *	will invalidate all pointers that refer to the object
 *	identifier elements.
 *
 *----------------------------------------------------------------------
 */

int
TnmOidAppend(TnmOid *oidPtr, u_int value)
{
    short length = TnmOidGetLength(oidPtr);
    if (length == TNM_OID_MAX_SIZE) {
	return TCL_ERROR;
    }
    TnmOidSetLength(oidPtr, length + 1);
    TnmOidSet(oidPtr, length, value);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmOidFromString --
 *
 *	This procedure converts a string with an object identifier
 *	in dotted representation into an internal object identifier.
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
TnmOidFromString(TnmOid *oidPtr, char *string)
{
    int i, len;
    char *c, isHex;

    /*
     * First handle empty strings which lead to empty object
     * identifier values. I am not sure if this interpretation
     * is useful, but it seems to be straight forward.
     */

    TnmOidFree(oidPtr);
    if (! string || ! *string) {
	return TCL_OK;
    }

    /*
     * Calculate the length of the object identifier value by counting
     * the number of dots in the string. Make sure that the TnmOid
     * pointer has an appropriate size.  Return an error if we find
     * strange characters in the string or if the object identifier
     * exceeds the maximum length.
     */

    for (c = string, len = 1, isHex = 0; *c; c++) {
	if (*c == '.') {
	    len++;
	    if (c[1] == '0' && c[2] == 'x') {
		isHex = 1, c += 2;
	    }
	} else if (*c == ':') {
	    len++;
	    isHex = 1;
	} else if (isHex && ! isxdigit((int) *c)) {
	    return TCL_ERROR;
	} else if (! isHex && ! isdigit((int) *c)) {
	    return TCL_ERROR;
	}
    }
    if (len > TNM_OID_MAX_SIZE) return TCL_ERROR;

    if (len > oidPtr->spaceAvl) {
	TnmOidSetLength(oidPtr, len);
    }

    /*
     * Finally, convert all the subidentifiers into unsigned integers
     * and store them in the object identifier.
     */

    for (i = 0, c = string, isHex = 0; *c; c++) {
	if (*c == '.') {
	    oidPtr->elements[++i] = 0;
	    isHex = 0;
	    if (c[1] == '0' && c[2] == 'x') {
		isHex = 1, c += 2;
	    }
	} else if (*c == ':') {
	    oidPtr->elements[++i] = 0;
	    isHex = 1;
	} else if (! isHex) {
	    oidPtr->elements[i] = 10 * oidPtr->elements[i] + *c - '0';
	} else {
	    char x = *c & 0xff;
	    int v = (x >= 'a') ?  x - 'a' + 10 
		: (x >= 'A' ? x - 'A' + 10 : x - '0');
	    oidPtr->elements[i] = (oidPtr->elements[i] << 4) + v;
	}
    }
    oidPtr->length = len;

    /*
     * Check the ASN.1 restrictions on object identifier values.
     * See X.690:1997 clause 8.19 for more details.
     */

    if ((oidPtr->length < 2) || (oidPtr->elements[0] > 2)) {
	TnmOidFree(oidPtr);
	return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmOidToString --
 *
 *	This procedure converts an object identifier into a string
 *	in dotted notation.
 *
 * Results:
 *	Returns the pointer to the string in static memory.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char*
TnmOidToString(TnmOid *oidPtr)
{
    int i;
    static char buf[TNM_OID_MAX_SIZE * 8];
    char *cp;

    if (oidPtr == NULL) return NULL;

    buf[0] = '\0';
    
    for (cp = buf, i = 0; i < oidPtr->length; i++) {
        if (oidPtr->elements[i] < 10) {
	    *cp++ = '0' + oidPtr->elements[i];
	} else {
	    u_int t=10;
	    char c = '0'+ (oidPtr->elements[i] % 10);
	    u_int u = oidPtr->elements[i] / 10;
	    while (u / t) t *= 10;
	    while (t /= 10) *cp++ = '0'+ (u / t) % 10;
	    *cp++ = c;
	}
	*cp++ = '.';
    }
    if (cp > buf) {
	*--cp = '\0';
    }
    
    return buf;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmOidCopy --
 *
 *	This procedure makes a copy of the src oid into dst oid.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TnmOidCopy(TnmOid *dstOidPtr, TnmOid *srcOidPtr)
{
    int i;

    TnmOidFree(dstOidPtr);
    TnmOidSetLength(dstOidPtr, TnmOidGetLength(srcOidPtr));
    for (i = 0; i < TnmOidGetLength(srcOidPtr); i++) {
	TnmOidSet(dstOidPtr, i, TnmOidGet(srcOidPtr, i));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmOidCompare --
 *
 *	This procedure compares two oids. 
 *
 * Results:
 *	Returns -1, 0 or 1, depending on whether oid1 is less than,
 *	equal to, or greater than oid2 in lexicographic order.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmOidCompare(oidPtr1, oidPtr2)
    TnmOid *oidPtr1;
    TnmOid *oidPtr2;
{
    int i;

    for (i = 0; i < oidPtr1->length && i < oidPtr2->length; i++) {
	if (oidPtr1->elements[i] < oidPtr2->elements[i]) {
	    return -1;
	}
	if (oidPtr2->elements[i] < oidPtr1->elements[i]) {
	    return 1;
	}
    }

    if (oidPtr1->length == oidPtr2->length) {
	return 0;
    }

    return (oidPtr1->length < oidPtr2->length) ? -1 : 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmOidInTree --
 *
 *	This procedure compares two oids. 
 *
 * Results:
 *	Returns 0 or 1, depending on whether oid is in the subtree
 *	defined by treePtr or not.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmOidInTree(TnmOid *treePtr, TnmOid *oidPtr)
{
    int i;

    if (oidPtr->length < treePtr->length) {
	return 0;
    }

    for (i = 0; i < treePtr->length; i++) {
	if (oidPtr->elements[i] != treePtr->elements[i]) {
	    return 0;
	}
    }

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetOidFromObj --
 *
 *	Attempt to return an TnmOid from the Tcl object "objPtr".  If
 *	the object is not already an tnmOidType, an attempt will be
 *	made to convert it to one.
 *
 * Results:
 *	The return value is a pointer to the TnmOid structure or NULL
 *	if the conversion failed. An error message is left in the
 *	interpreter, if interp is not NULL.
 *
 * Side effects:
 *	If the object is not already an tnmOidType, the conversion 
 *	will free any old internal representation.
 *
 *----------------------------------------------------------------------
 */

TnmOid*
TnmGetOidFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    int result;

    if (objPtr->typePtr != &tnmOidType) {
	result = SetOidFromAny(interp, objPtr);
	if (result != TCL_OK) {
	    return NULL;
	}
    }

    return (TnmOid *) objPtr->internalRep.twoPtrValue.ptr1;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmNewOidObj --
 *
 *	This procedure creates a new object identifier object.
 *
 * Results:
 *	The newly created object is returned. This object will have an
 *	invalid string representation. The returned object has ref count 0.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
TnmNewOidObj(TnmOid *oidPtr)
{
    Tcl_Obj *objPtr;
    TnmOid *newOidPtr;
    int i;

    objPtr = Tcl_NewObj();

    newOidPtr = (TnmOid *) Tcl_Alloc(sizeof(TnmOid));
    TnmOidInit(newOidPtr);
    for (i = 0; i < TnmOidGetLength(oidPtr); i++) {
	TnmOidAppend(newOidPtr, TnmOidGet(oidPtr, i));
    }

    objPtr->internalRep.twoPtrValue.ptr1 = (VOID *) newOidPtr;
    TnmOidObjSetRep(objPtr, TNM_OID_AS_OID);
    objPtr->typePtr = &tnmOidType;
    Tcl_InvalidateStringRep(objPtr);

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSetOidObj --
 *
 *	This procedure converts an object into an object identifier
 *	object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's old string rep, if any, is freed. Also, any old
 *	internal rep is freed.  
 *
 *----------------------------------------------------------------------
 */

void
TnmSetOidObj(Tcl_Obj *objPtr, TnmOid *oidPtr)
{
    register Tcl_ObjType *oldTypePtr = objPtr->typePtr;

    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("TnmSetOidObj called with shared object");
    }

    Tcl_InvalidateStringRep(objPtr);
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }
    
    objPtr->internalRep.twoPtrValue.ptr1 = (VOID *) oidPtr;
    TnmOidObjSetRep(objPtr, TNM_OID_AS_OID);
    objPtr->typePtr = &tnmOidType;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeOidInternalRep --
 *
 *	Deallocate the storage associated with a tnmOid object's
 *	internal representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees objPtr's TnmOid* internal representation and sets
 *	objPtr's internalRep.twoPtrValue.ptr1 to NULL.
 *
 *----------------------------------------------------------------------
 */

static void
FreeOidInternalRep(objPtr)
    Tcl_Obj *objPtr;		/* Object with internal rep to free. */
{
    TnmOid *oidPtr = (TnmOid *) objPtr->internalRep.twoPtrValue.ptr1;
    if (oidPtr) {
	TnmOidFree(oidPtr);
	Tcl_Free((char *) oidPtr);
    }
    objPtr->internalRep.twoPtrValue.ptr1 = NULL;
    objPtr->internalRep.twoPtrValue.ptr2 = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DupOidInternalRep --
 *
 *	Initialize the internal representation of a tnmOid Tcl_Obj to
 *	a copy of the internal representation of an existing tnmOid
 *	object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	"srcPtr"s internal rep pointer should not be NULL and we
 *	assume it is not NULL.
 *
 *----------------------------------------------------------------------
 */

static void
DupOidInternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr;		/* Object with internal rep to set. */
{
    TnmOid *oidPtr = (TnmOid *) srcPtr->internalRep.twoPtrValue.ptr1;
    TnmOid *newOidPtr;
    int i;

    newOidPtr = (TnmOid *) Tcl_Alloc(sizeof(TnmOid));
    TnmOidInit(newOidPtr);
    for (i = 0; i < TnmOidGetLength(oidPtr); i++) {
	TnmOidAppend(newOidPtr, TnmOidGet(oidPtr, i));
    }
        
    copyPtr->internalRep.twoPtrValue.ptr1 = (VOID *) newOidPtr;
    copyPtr->internalRep.twoPtrValue.ptr2
	= srcPtr->internalRep.twoPtrValue.ptr2;
    copyPtr->typePtr = &tnmOidType;
}

/*
 *----------------------------------------------------------------------
 *
 * SetOidFromAny --
 *
 *	Attempt to generate a tnmOid internal form for the Tcl object
 *	"objPtr".
 *
 * Results:
 *	The return value is TCL_OK or TCL_ERROR. If an error occurs
 *	during conversion, an error message is left in the
 *	interpreter's result unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, an tnmOid is stored as "objPtr"s internal
 *	representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetOidFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* The object to convert. */
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *string;
    TnmOid *oidPtr = NULL;
    int isOid = 0;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    string = Tcl_GetStringFromObj(objPtr, NULL);
    isOid = TnmIsOid(string);
    if (! isOid) {
	string = TnmMibGetOid(string);
	if (! string) {
	    goto errorExit;
	}
    }

    oidPtr = (TnmOid *) Tcl_Alloc(sizeof(TnmOid));
    TnmOidInit(oidPtr);
    if (TnmOidFromString(oidPtr, string) != TCL_OK) {
	goto errorExit;
    }

    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    /*
     * Free the old internalRep before setting the new one. We do this as
     * late as possible to allow the conversion code, in particular
     * Tcl_GetStringFromObj, to use that old internalRep.
     */

    objPtr->internalRep.twoPtrValue.ptr1 = (VOID *) oidPtr;
    TnmOidObjSetRep(objPtr, (isOid ? TNM_OID_AS_OID : TNM_OID_AS_NAME));
    objPtr->typePtr = &tnmOidType;
    return TCL_OK;

errorExit:
    if (interp) {
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			       "invalid object identifier \"",
			       Tcl_GetStringFromObj(objPtr, NULL), "\"",
			       (char *) NULL);
    }
    if (oidPtr) {
	Tcl_Free((char *) oidPtr);
    }
    return TCL_ERROR;
}    

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfOid --
 *
 *	Update the string representation for an tnmOid object. Note:
 *	This procedure does not invalidate an existing old string rep
 *	so storage will be lost if this has not already been done.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The object's string is set to a valid string that results from
 *	the oid-to-string conversion. This string will be empty if the
 *	list has no elements. The list internal representation should
 *	not be NULL and we assume it is not NULL.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfOid(Tcl_Obj *objPtr)
{
    TnmOid *oidPtr = (TnmOid *) objPtr->internalRep.twoPtrValue.ptr1;
    int isOid = (TnmOidObjGetRep(objPtr) == TNM_OID_AS_OID);
    char *string;

    string = TnmOidToString(oidPtr);
    if (! string) {
	return;
    }
    if (isOid) {
    returnOid:
	objPtr->length = strlen(string);
	objPtr->bytes = Tcl_Alloc(objPtr->length + 1);
	strcpy(objPtr->bytes, string);
    } else {
	TnmMibNode *nodePtr;
	int offset;
	nodePtr = TnmMibFindNode(string, &offset, 0);
	if (! nodePtr) {
	    goto returnOid;
	}
	objPtr->length = strlen(nodePtr->label);
	if (nodePtr->moduleName) {
	    objPtr->length += strlen(nodePtr->moduleName) + 2;
	}
	if (offset > 0) {
	    objPtr->length += strlen(string) - offset;
	}
	objPtr->bytes = Tcl_Alloc(objPtr->length + 1);
	if (nodePtr->moduleName) {
	    strcpy(objPtr->bytes, nodePtr->moduleName);
#if 1
	    strcat(objPtr->bytes, "::");
#else
	    strcat(objPtr->bytes, "!");
#endif
	    strcat(objPtr->bytes, nodePtr->label);
	} else {
	    strcpy(objPtr->bytes, nodePtr->label);
	}
	if (offset > 0) {
	    strcat(objPtr->bytes, string+offset);
	}
    }
}

/*
 * Local Variables:
 * compile-command: "make -k -C ../../unix"
 * End:
 */

