/*
 * tnmObj.c --
 *
 *	This file implements some object types for the Tnm
 *	extension of the Tcl interpreter. This implementation
 *	is supposed to be thread-safe.
 *
 * Copyright (c) 1998-1999 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmObj.c,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $
 */

#include "tnmInt.h"

#if 0
TnmDeltaCounter64Obj
TnmRangeCounter64Obj

TnmDeltaUnsigned32Obj
TnmRangeUnsigned32Obj

TnmNewTAddrObj
TnmGetTAddrName
TnmSetTAddrName
TnmGetTAddrPort
TnmSetTAddrPort
TnmGetTAddrDomain
TnmSetTAddrDomain
TnmGetTAddrFromObj
#endif

/*
 * Prototypes for procedures defined later in this file:
 */

static void		
FreeUnsigned64InternalRep	(Tcl_Obj *objPtr);

static void
DupUnsigned64InternalRep	(Tcl_Obj *srcPtr,
					     Tcl_Obj *copyPtr);
static void
UpdateStringOfUnsigned64	(Tcl_Obj *objPtr);

static int
SetUnsigned64FromAny		(Tcl_Interp *interp,
					     Tcl_Obj *objPtr);
static void
DupUnsigned32InternalRep	(Tcl_Obj *srcPtr,
					     Tcl_Obj *copyPtr);
static void
UpdateStringOfUnsigned32	(Tcl_Obj *objPtr);

static int
SetUnsigned32FromAny		(Tcl_Interp *interp,
					     Tcl_Obj *objPtr);
static void		
FreeOctetStringInternalRep	(Tcl_Obj *objPtr);

static void
DupOctetStringInternalRep	(Tcl_Obj *srcPtr,
					     Tcl_Obj *copyPtr);
static void
UpdateStringOfOctetString	(Tcl_Obj *objPtr);

static int
SetOctetStringFromAny		(Tcl_Interp *interp,
					     Tcl_Obj *objPtr);
static void
DupIpAddressInternalRep		(Tcl_Obj *srcPtr,
					     Tcl_Obj *copyPtr);
static void
UpdateStringOfIpAddress		(Tcl_Obj *objPtr);

static int
SetIpAddressFromAny		(Tcl_Interp *interp,
					     Tcl_Obj *objPtr);

/*
 * The structure below defines the tnmUnsigned64 Tcl object type by
 * means of procedures that can be invoked by generic object code.
 */

Tcl_ObjType tnmUnsigned64Type = {
    "tnmUnsigned64",		/* name of the type */
    FreeUnsigned64InternalRep,	/* freeIntRepProc */
    DupUnsigned64InternalRep,	/* dupIntRepProc */
    UpdateStringOfUnsigned64,	/* updateStringProc */
    SetUnsigned64FromAny	/* setFromAnyProc */
};

/*
 * The structure below defines the tnmUnsigned32 Tcl object type by
 * means of procedures that can be invoked by generic object code.
 */

Tcl_ObjType tnmUnsigned32Type = {
    "tnmUnsigned32",		/* name of the type */
    NULL,			/* freeIntRepProc */
    DupUnsigned32InternalRep,	/* dupIntRepProc */
    UpdateStringOfUnsigned32,	/* updateStringProc */
    SetUnsigned32FromAny	/* setFromAnyProc */
};

/*
 * The structure below defines the Tnm OctetString Tcl object type by
 * means of procedures that can be invoked by generic object code.
 */

Tcl_ObjType tnmOctetStringType = {
    "tnmOctetString",		/* name of the type */
    FreeOctetStringInternalRep,	/* freeIntRepProc */
    DupOctetStringInternalRep,	/* dupIntRepProc */
    UpdateStringOfOctetString,	/* updateStringProc */
    SetOctetStringFromAny	/* setFromAnyProc */
};

/*
 * The structure below defines the Tnm IpAddress Tcl object type by
 * means of procedures that can be invoked by generic object code.
 */

Tcl_ObjType tnmIpAddressType = {
    "tnmIpAddress",		/* name of the type */
    NULL,			/* freeIntRepProc */
    DupIpAddressInternalRep,	/* dupIntRepProc */
    UpdateStringOfIpAddress,	/* updateStringProc */
    SetIpAddressFromAny		/* setFromAnyProc */
};


/*
 *----------------------------------------------------------------------
 *
 * TnmNewUnsigned64Obj --
 *
 *	This procedure creates a new unsigned 64 object.
 *
 * Results:
 *	The newly created object is returned. This object will have an
 *	invalid string representation. The returned object has ref count 0.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------- */

Tcl_Obj*
TnmNewUnsigned64Obj(TnmUnsigned64 u)
{
    Tcl_Obj *objPtr = Tcl_NewObj();
    TnmUnsigned64 *uPtr;

    uPtr = (TnmUnsigned64 *) Tcl_Alloc(sizeof(TnmUnsigned64));
    memcpy((char *) uPtr, (char *) &u, sizeof(TnmUnsigned64));
    
    objPtr->internalRep.otherValuePtr = (VOID *) uPtr;
    objPtr->typePtr = &tnmUnsigned64Type;
    Tcl_InvalidateStringRep(objPtr);
    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSetUnsigned64Obj --
 *
 *	This procedure converts an object into an unsigned 42 object.
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
TnmSetUnsigned64Obj(Tcl_Obj *objPtr, TnmUnsigned64 u)
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    TnmUnsigned64 *uPtr;

    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("TnmSetUnsigned64Obj called with shared object");
    }

    Tcl_InvalidateStringRep(objPtr);
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    uPtr = (TnmUnsigned64 *) Tcl_Alloc(sizeof(TnmUnsigned64));
    memcpy((char *) uPtr, (char *) &u, sizeof(TnmUnsigned64));
    
    objPtr->internalRep.otherValuePtr = (VOID *) uPtr;
    objPtr->typePtr = &tnmUnsigned64Type;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetUnsigned64FromObj --
 *
 *	Attempt to return an TnmUnsigned64 from the Tcl object
 *	"objPtr".  If the object is not already an tnmUnsigned64Type,
 *	an attempt will be made to convert it to one.
 *
 * Results:
 *	The return value is a pointer to the TnmUnsigned64 structure
 *	or NULL if the conversion failed. An error message is left in
 *	the interpreter, if interp is not NULL.
 *
 * Side effects:
 *	If the object is not already an tnmUnsigned64Type, the conversion 
 *	will free any old internal representation.
 *
 *----------------------------------------------------------------------
 */

int
TnmGetUnsigned64FromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, TnmUnsigned64 *uPtr)
{
    int result;

    if (objPtr->typePtr != &tnmUnsigned64Type) {
	result = SetUnsigned64FromAny(interp, objPtr);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    *uPtr = *(TnmUnsigned64 *) objPtr->internalRep.otherValuePtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeUnsigned64InternalRep --
 *
 *	Deallocate the storage associated with a 64 bit unsigned object's
 *	internal representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
FreeUnsigned64InternalRep(objPtr)
    Tcl_Obj *objPtr;		/* Object with internal rep to free. */
{
    if (objPtr->internalRep.otherValuePtr) {
	Tcl_Free((char *) objPtr->internalRep.otherValuePtr);
	objPtr->internalRep.otherValuePtr = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DupUnsigned64InternalRep --
 *
 *	Make a deep copy of a 64 bit unsigned type.
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
DupUnsigned64InternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr;		/* Object with internal rep to set. */
{
    copyPtr->internalRep.otherValuePtr
	= (VOID *) Tcl_Alloc(sizeof(TnmUnsigned64));
    memcpy((char *) copyPtr->internalRep.otherValuePtr, 
	   (char *) srcPtr->internalRep.otherValuePtr, sizeof(TnmUnsigned64));
    copyPtr->typePtr = &tnmUnsigned64Type;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfUnsigned64 --
 *
 *	Update the string representation for a 64 bit unsigned type.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfUnsigned64(Tcl_Obj *objPtr)
{
    TnmUnsigned64 *uPtr = (TnmUnsigned64 *) objPtr->internalRep.otherValuePtr;

    objPtr->bytes = Tcl_Alloc(30);
    objPtr->length = sprintf(objPtr->bytes, "%llu", *uPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * SetUnsigned64FromAny --
 *
 *	Attempt to generate a 64 unsigned internal from the
 *	Tcl object "objPtr".
 *
 * Results:
 *	The return value is TCL_OK or TCL_ERROR. If an error occurs
 *	during conversion, an error message is left in the
 *	interpreter's result unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, a 64 bit unsigned is stored as
 *	"objPtr"s internal representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetUnsigned64FromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* The object to convert. */
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *string, *p;
    TnmUnsigned64 u;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    string = Tcl_GetStringFromObj(objPtr, NULL);

    /*
     * Now parse "objPtr"s string as an unsigned long value. This code
     * is a modified version of the code found in tclObj.c. Make sure
     * that we do not accept a negative number.
     */

    for (p = string;  isspace(*p);  p++) {
	/* Empty loop body. */
    }
    if (*p == '-') {
	goto badUnsigned64;
    }
    
    /* XXX use strtoull() where available! */
    if (sscanf(p, "%llu", &u) != 1) {
    badUnsigned64:
	if (interp != NULL) {
	    /*
	     * Must copy string before resetting the result in case a caller
	     * is trying to convert the interpreter's result to an int.
	     */
	    
	    char buf[100];
	    sprintf(buf, "expected 64 bit unsigned but got \"%.50s\"", string);
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
	}
	return TCL_ERROR;
    }

    /*
     * Free the old internalRep before setting the new one. We do this as
     * late as possible to allow the conversion code, in particular
     * Tcl_GetStringFromObj, to use that old internalRep.
     */

    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.otherValuePtr = Tcl_Alloc(sizeof(TnmUnsigned64));
    *(TnmUnsigned64 *) objPtr->internalRep.otherValuePtr = u;
    objPtr->typePtr = &tnmUnsigned64Type;
    return TCL_OK;
}    

/*
 *----------------------------------------------------------------------
 *
 * TnmNewUnsigned32Obj --
 *
 *	This procedure creates a new unsigned32 object.
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
TnmNewUnsigned32Obj(TnmUnsigned32 u)
{
    Tcl_Obj *objPtr = Tcl_NewObj();

    objPtr->internalRep.longValue = (long) u;
    objPtr->typePtr = &tnmUnsigned32Type;
    Tcl_InvalidateStringRep(objPtr);

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSetUnsigned32Obj --
 *
 *	This procedure converts an object into an unsigned 32
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
TnmSetUnsigned32Obj(Tcl_Obj *objPtr, TnmUnsigned32 u)
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;

    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("TnmSetUnsigned32Obj called with shared object");
    }

    Tcl_InvalidateStringRep(objPtr);
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.longValue = (long) u;
    objPtr->typePtr = &tnmUnsigned32Type;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetUnsigned32FromObj --
 *
 *	Attempt to return an TnmUnsigned32 from the Tcl object
 *	"objPtr".  If the object is not already an tnmUnsigned32Type,
 *	an attempt will be made to convert it to one.
 *
 * Results:
 *	The return value is a pointer to the TnmUnsigned32 structure
 *	or NULL if the conversion failed. An error message is left in
 *	the interpreter, if interp is not NULL.
 *
 * Side effects:
 *	If the object is not already an tnmUnsigned32Type, the conversion 
 *	will free any old internal representation.
 *
 *----------------------------------------------------------------------
 */

int
TnmGetUnsigned32FromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, TnmUnsigned32 *uPtr)
{
    int result;

    if (objPtr->typePtr != &tnmUnsigned32Type) {
	result = SetUnsigned32FromAny(interp, objPtr);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    *uPtr = (TnmUnsigned32) objPtr->internalRep.longValue;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DupUnsigned32InternalRep --
 *
 *	Make a deep copy of a 32 bit unsigned type.
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
DupUnsigned32InternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr;		/* Object with internal rep to set. */
{
    copyPtr->internalRep.longValue = srcPtr->internalRep.longValue;
    copyPtr->typePtr = &tnmUnsigned32Type;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfUnsigned32 --
 *
 *	Update the string representation for a 32 bit unsigned type.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfUnsigned32(Tcl_Obj *objPtr)
{
    TnmUnsigned32 u = (TnmUnsigned32) objPtr->internalRep.longValue;

    objPtr->bytes = Tcl_Alloc(30);
    objPtr->length = sprintf(objPtr->bytes, "%lu", u);
}

/*
 *----------------------------------------------------------------------
 *
 * SetUnsigned32FromAny --
 *
 *	Attempt to generate a 32 unsigned internal from the
 *	Tcl object "objPtr".
 *
 * Results:
 *	The return value is TCL_OK or TCL_ERROR. If an error occurs
 *	during conversion, an error message is left in the
 *	interpreter's result unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, a 32 bit unsigned is stored as
 *	"objPtr"s internal representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetUnsigned32FromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* The object to convert. */
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *string, *end, *p;
    int length;
    TnmUnsigned32 u;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    string = Tcl_GetStringFromObj(objPtr, &length);

    /*
     * Now parse "objPtr"s string as an unsigned long value. This code
     * is a modified version of the code found in tclObj.c. Make sure
     * that we do not accept a negative number.
     */

    for (p = string;  isspace(*p);  p++) {
	/* Empty loop body. */
    }
    if (*p == '-') {
	goto badUnsigned32;
    }
    
    errno = 0;
    u = strtoul(p, &end, 0);
    if (end == string) {
    badUnsigned32:
	if (interp != NULL) {
	    /*
	     * Must copy string before resetting the result in case a caller
	     * is trying to convert the interpreter's result to an int.
	     */
	    
	    char buf[100];
	    sprintf(buf, "expected 32 bit unsigned but got \"%.50s\"", string);
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), buf, -1);
	}
	return TCL_ERROR;
    }
    if (errno == ERANGE) {
	if (interp != NULL) {
	    char *s = "unsigned value too large to represent";
	    Tcl_ResetResult(interp);
	    Tcl_AppendToObj(Tcl_GetObjResult(interp), s, -1);
	    Tcl_SetErrorCode(interp, "ARITH", "IOVERFLOW", s, (char *) NULL);
	}
	return TCL_ERROR;
    }
    
    /*
     * Make sure that the string has no garbage after the end of the int.
     */
    
    while ((end < (string+length)) && isspace(*end)) {
	end++;
    }
    if (end != (string+length)) {
	goto badUnsigned32;
    }

    /*
     * Free the old internalRep before setting the new one. We do this as
     * late as possible to allow the conversion code, in particular
     * Tcl_GetStringFromObj, to use that old internalRep.
     */

    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.longValue = (long) u;
    objPtr->typePtr = &tnmUnsigned32Type;
    return TCL_OK;
}    

/*
 *----------------------------------------------------------------------
 *
 * TnmNewOctetStringObj --
 *
 *	This procedure creates a new octet string object.
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
TnmNewOctetStringObj(char *bytes, int length)
{
    Tcl_Obj *objPtr = Tcl_NewObj();
    TnmSetOctetStringObj(objPtr, bytes, length);
    Tcl_InvalidateStringRep(objPtr);

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSetOctetStringObj --
 *
 *	This procedure converts an object into an octet string object.
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
TnmSetOctetStringObj(Tcl_Obj *objPtr, char *bytes, int length)
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    size_t size = length;

    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("TnmSetOctetStringObj called with shared object");
    }

    Tcl_InvalidateStringRep(objPtr);
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.twoPtrValue.ptr1 = Tcl_Alloc(size);
    memcpy(objPtr->internalRep.twoPtrValue.ptr1, bytes, size);
    objPtr->internalRep.twoPtrValue.ptr2 = (VOID *) size;
    objPtr->typePtr = &tnmOctetStringType;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetOctetStringFromObj --
 *
 *	Attempt to return an TnmOctetString from the Tcl object
 *	"objPtr".  If the object is not already an tnmOctetStringType,
 *	an attempt will be made to convert it to one.
 *
 * Results:
 *	The return value is a pointer to the TnmOctetString structure
 *	or NULL if the conversion failed. An error message is left in
 *	the interpreter, if interp is not NULL.
 *
 * Side effects:
 *	If the object is not already an tnmOctetStringType, the conversion 
 *	will free any old internal representation.
 *
 *----------------------------------------------------------------------
 */

char*
TnmGetOctetStringFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int *lengthPtr)
{
    int result;

    if (objPtr->typePtr != &tnmOctetStringType) {
	result = SetOctetStringFromAny(interp, objPtr);
	if (result != TCL_OK) {
	    return NULL;
	}
    }

    *lengthPtr = (int) objPtr->internalRep.twoPtrValue.ptr2;
    return (char *) objPtr->internalRep.twoPtrValue.ptr1;
}

/*
 *----------------------------------------------------------------------
 *
 * FreeOctetStringInternalRep --
 *
 *	Deallocate the storage associated with a 64 bit unsigned object's
 *	internal representation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
FreeOctetStringInternalRep(objPtr)
    Tcl_Obj *objPtr;		/* Object with internal rep to free. */
{
    if (objPtr->internalRep.twoPtrValue.ptr1) {
	Tcl_Free((char *) objPtr->internalRep.twoPtrValue.ptr1);
	objPtr->internalRep.twoPtrValue.ptr1 = NULL;
	objPtr->internalRep.twoPtrValue.ptr2 = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DupOctetStringInternalRep --
 *
 *	Make a deep copy of an octet string type.
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
DupOctetStringInternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr;		/* Object with internal rep to set. */
{
    size_t size = (int) srcPtr->internalRep.twoPtrValue.ptr2;
    char *bytes;
    
    bytes = Tcl_Alloc(size);
    memcpy(bytes, (char *) srcPtr->internalRep.twoPtrValue.ptr1, size);
    
    copyPtr->internalRep.twoPtrValue.ptr1 = (VOID *) bytes;
    copyPtr->internalRep.twoPtrValue.ptr2 = (VOID *) size;
    copyPtr->typePtr = &tnmOctetStringType;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfOctetString --
 *
 *	Update the string representation for an octet string type.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfOctetString(Tcl_Obj *objPtr)
{
    objPtr->bytes = Tcl_Alloc(
			(size_t) objPtr->internalRep.twoPtrValue.ptr2 * 3);
    TnmHexEnc(objPtr->internalRep.twoPtrValue.ptr1,
	      (int) objPtr->internalRep.twoPtrValue.ptr2,
	      objPtr->bytes);
    objPtr->length = strlen(objPtr->bytes);
}

/*
 *----------------------------------------------------------------------
 *
 * SetOctetStringFromAny --
 *
 *	Attempt to generate a octet string internal representation from
 *	the Tcl object "objPtr".
 *
 * Results:
 *	The return value is TCL_OK or TCL_ERROR. If an error occurs
 *	during conversion, an error message is left in the
 *	interpreter's result unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, an octet string is allocated and stored
 *	as "objPtr"s internal representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetOctetStringFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* The object to convert. */
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *string, *bytes;
    int length;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    string = Tcl_GetStringFromObj(objPtr, &length);

    /*
     * Convert the octet string value into a Tcl string.
     */

    bytes = Tcl_Alloc((size_t) length);
    if (TnmHexDec(string, bytes, &length) < 0) {
	if (interp != NULL) {
	    /*
	     * Must copy string before resetting the result in case a caller
	     * is trying to convert the interpreter's result to an int.
	     */
	    
	    char *tmp = ckstrdup(string);
	    Tcl_ResetResult(interp);
	    Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
				   "illegal octet string value \"",
				   tmp, "\"", (char *) NULL);
	    Tcl_Free(tmp);
	}
	Tcl_Free(bytes);
	return TCL_ERROR;
    }
    
    /*
     * Free the old internalRep before setting the new one. We do this as
     * late as possible to allow the conversion code, in particular
     * Tcl_GetStringFromObj, to use that old internalRep.
     */

    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.twoPtrValue.ptr1 = (VOID *) bytes;
    objPtr->internalRep.twoPtrValue.ptr2 = (VOID *) length;
    objPtr->typePtr = &tnmOctetStringType;
    return TCL_OK;
}    

/*
 *----------------------------------------------------------------------
 *
 * TnmNewIpAddressObj --
 *
 *	This procedure creates a IpAddress object.
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
TnmNewIpAddressObj(struct in_addr *ipaddr)
{
    Tcl_Obj *objPtr = Tcl_NewObj();
    TnmSetIpAddressObj(objPtr, ipaddr);
    Tcl_InvalidateStringRep(objPtr);

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSetIpAddressObj --
 *
 *	This procedure converts an object into an IpAddress object.
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
TnmSetIpAddressObj(Tcl_Obj *objPtr, struct in_addr *ipaddr)
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;

    if (Tcl_IsShared(objPtr)) {
	Tcl_Panic("TnmSetIpAddressObj called with shared object");
    }

    Tcl_InvalidateStringRep(objPtr);
    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.longValue = * (long *) ipaddr;
    objPtr->typePtr = &tnmIpAddressType;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetIpAddressFromObj --
 *
 *	Attempt to return an TnmOctetString from the Tcl object
 *	"objPtr".  If the object is not already an tnmIpAddressType,
 *	an attempt will be made to convert it to one.
 *
 * Results:
 *	The return value is a pointer to the in_addr structure
 *	or NULL if the conversion failed. An error message is left in
 *	the interpreter, if interp is not NULL.
 *
 * Side effects:
 *	If the object is not already an tnmIpAddressType, the conversion 
 *	will free any old internal representation.
 *
 *----------------------------------------------------------------------
 */

struct in_addr*
TnmGetIpAddressFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    int result;

    if (objPtr->typePtr != &tnmIpAddressType) {
	result = SetIpAddressFromAny(interp, objPtr);
	if (result != TCL_OK) {
	    return NULL;
	}
    }

    return (struct in_addr *) &objPtr->internalRep.longValue;
}

/*
 *----------------------------------------------------------------------
 *
 * DupIpAddressInternalRep --
 *
 *	Make a deep copy of an IpAddress type.
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
DupIpAddressInternalRep(srcPtr, copyPtr)
    Tcl_Obj *srcPtr;		/* Object with internal rep to copy. */
    Tcl_Obj *copyPtr;		/* Object with internal rep to set. */
{
    copyPtr->internalRep.longValue = srcPtr->internalRep.longValue;
    copyPtr->typePtr = &tnmIpAddressType;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateStringOfIpAddress --
 *
 *	Update the string representation for an IpAddress type.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateStringOfIpAddress(Tcl_Obj *objPtr)
{
    struct in_addr *ipaddrPtr;
    
    ipaddrPtr = (struct in_addr *) &objPtr->internalRep.longValue;
    objPtr->bytes = Tcl_Alloc(16);
    strcpy(objPtr->bytes, inet_ntoa(*ipaddrPtr));
    objPtr->length = strlen(objPtr->bytes);
}

/*
 *----------------------------------------------------------------------
 *
 * SetIpAddressFromAny --
 *
 *	Attempt to generate an IpAddress internal from the
 *	Tcl object "objPtr".
 *
 * Results:
 *	The return value is TCL_OK or TCL_ERROR. If an error occurs
 *	during conversion, an error message is left in the
 *	interpreter's result unless "interp" is NULL.
 *
 * Side effects:
 *	If no error occurs, an IpAddress is stored as
 *	"objPtr"s internal representation.
 *
 *----------------------------------------------------------------------
 */

static int
SetIpAddressFromAny(interp, objPtr)
    Tcl_Interp *interp;		/* Used for error reporting if not NULL. */
    Tcl_Obj *objPtr;		/* The object to convert. */
{
    Tcl_ObjType *oldTypePtr = objPtr->typePtr;
    char *string;
    int length;
    struct sockaddr_in inaddr;

    /*
     * Get the string representation. Make it up-to-date if necessary.
     */

    string = Tcl_GetStringFromObj(objPtr, &length);

    /*
     * Now parse "objPtr"s string as an IpAddress value.
     */

    if (TnmSetIPAddress(interp, string, &inaddr) != TCL_OK) {
	return TCL_ERROR;
    }
    
    /*
     * Free the old internalRep before setting the new one. We do this as
     * late as possible to allow the conversion code, in particular
     * Tcl_GetStringFromObj, to use that old internalRep.
     */

    if ((oldTypePtr != NULL) && (oldTypePtr->freeIntRepProc != NULL)) {
	oldTypePtr->freeIntRepProc(objPtr);
    }

    objPtr->internalRep.longValue = * (long *) &inaddr.sin_addr;
    objPtr->typePtr = &tnmIpAddressType;
    return TCL_OK;
}    


