/*
 * tnmUtil.c --
 *
 *	Utility functions used by various modules of the Tnm extension.
 *	This implementation is supposed to be thread-safe.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1999 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tnmInt.h"
#include "tnmPort.h"

/*
 * Mutex used to serialize access to static variables in this module.
 */

TCL_DECLARE_MUTEX(utilMutex)


/*
 *----------------------------------------------------------------------
 *
 * TnmGetTableValue --
 *
 *	This procedure searches for a key in a given table and 
 *	returns the corresponding value.
 *
 * Results:
 *	Returns a pointer to the static value or NULL if the key
 *	is not contained in the table.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TnmGetTableValue(TnmTable *table, unsigned key)
{
    TnmTable *elemPtr;

    if (table) {
	for (elemPtr = table; elemPtr->value; elemPtr++) {
	    if (elemPtr->key == key) {
		return elemPtr->value;
	    }
	}
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetTableKey --
 *
 *	This procedure searches for a value for a given table and
 *	returns the corresponding key.
 *
 * Results:
 *	Returns the key code or -1 if the string does not contain
 *	the given value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmGetTableKey(TnmTable *table, const char *value)
{
    TnmTable *elemPtr;

    if (table) {
	for (elemPtr = table; elemPtr->value; elemPtr++) {
	    if (strcmp(value, elemPtr->value) == 0) {
		return elemPtr->key;
	    }
	}
    }
    
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetTableValues --
 *
 *	This procedure returns a list of all values in a TnmTable.
 *	The list is contained in a string where each element is
 *	separated by a ", " substring.
 *
 * Results:
 *	Returns a pointer to the static string which is overwritten
 *	by the next call to this procedure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TnmGetTableValues(TnmTable *table)
{
    static char *buffer = NULL;
    static size_t size = 0;
    TnmTable *elemPtr;
    size_t cnt = 8;
    char *p;

    Tcl_MutexLock(&utilMutex);

    if (buffer == NULL) {
	size = 256;
	buffer = ckalloc(size);
    }

    /*
     * First count the number of bytes that we need to build 
     * the result string and make sure that the buffer is long
     * enough to hold the result.
     */

    if (table) {
	for (elemPtr = table; elemPtr->value; elemPtr++) {
	    cnt += strlen(elemPtr->value) + 2;
	}
    }

    if (cnt > size) {
	size = cnt;
	buffer = ckrealloc(buffer, size);
    }

    /*
     * Build the result string.
     */
    
    p = buffer;
    if (table) {
	for (elemPtr = table; elemPtr->value; elemPtr++) {
	    char *s = elemPtr->value;
	    if (p != buffer) {
		TnmTable *nextPtr = elemPtr + 1;
		*p++ = ',';
		*p++ = ' ';
		if (nextPtr->value == NULL) {
		    *p++ = 'o';
		    *p++ = 'r';
		    *p++ = ' ';
		}
	    }
	    while (*s) {
		*p++ = *s++;
	    }
	}
    }
    *p = '\0';

    Tcl_MutexUnlock(&utilMutex);
    
    return buffer;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmListFromTable --
 *
 *	This procedure returns a Tcl list object of all values in a
 *	TnmTable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The listPtr argument is converted into a Tcl list.
 *
 *----------------------------------------------------------------------
 */

void
TnmListFromTable(TnmTable *table, Tcl_Obj *listPtr, char *pattern)
{
    for (; table->value; table++) {
	if (pattern && !Tcl_StringMatch(table->value, pattern)) {
	    continue;
	}	
	Tcl_ListObjAppendElement((Tcl_Interp *) NULL, listPtr,
				 Tcl_NewStringObj(table->value, -1));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetTableKeyFromObj --
 *
 *	This procedure searches for the string representation of a
 *	Tcl_Obj in a given table and returns the corresponding key.
 *	An error message is left in the interpreter if the string
 *	can not be a found.
 *
 * Results:
 *	Returns the key code or -1 if the object does not contain
 *	the given value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmGetTableKeyFromObj(Tcl_Interp *interp, TnmTable *table, Tcl_Obj *objPtr, char *what)
{
    char *name;
    int value;

    name = Tcl_GetStringFromObj(objPtr, NULL);
    value = TnmGetTableKey(table, name);
    if (value == -1 && interp) {
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), "unknown ",
			       what, " \"", name, "\": should be ",
			       TnmGetTableValues(table), (char *) NULL);
    }
    return value;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmListFromList --
 *
 *	This procedure returns a Tcl list object of all values in a
 *	given list that match a given pattern.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The listPtr argument is converted into a Tcl list.
 *
 *----------------------------------------------------------------------
 */

void
TnmListFromList(Tcl_Obj *objPtr, Tcl_Obj *listPtr, char *pattern)
{
    int i, objc, code;
    Tcl_Obj **objv;

    code = Tcl_ListObjGetElements(NULL, objPtr, &objc, &objv);
    if (code != TCL_OK) return;
    
    for (i = 0; i < objc; i++) {
	char *s = Tcl_GetStringFromObj(objv[i], NULL);
	if (pattern && !Tcl_StringMatch(s, pattern)) {
	    continue;
	}	
	Tcl_ListObjAppendElement((Tcl_Interp *) NULL, listPtr, objv[i]);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSetConfig --
 *
 *	This procedure allows to configure various aspects of an
 *	object. It is usually used to implement a configure object
 *	command.
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
TnmSetConfig(Tcl_Interp *interp, TnmConfig *config, ClientData object, int objc, Tcl_Obj *const objv[])
{
    int i, option, code;
    TnmTable *elemPtr;
    Tcl_Obj *listPtr;
    Tcl_Obj *objPtr;

    if (objc % 2) {
	Tcl_WrongNumArgs(interp, 2, objv, "?option value? ?option value? ...");
	return TCL_ERROR;
    }

    /*
     * First scan through the list of options to make sure that
     * we don't run on an unknown option later when we have 
     * already modified the object.
     */

    for (i = 2; i < objc; i += 2) {
	option = TnmGetTableKeyFromObj(interp, config->optionTable,
				       objv[i], "option");
	if (option < 0) {
	    return TCL_ERROR;
	}
    }

    /*
     * Now call the function to actually modify the object. Note,
     * this version does not rollback changes so an object might
     * end up in a half modified state.
     */
	
    for (i = 2; i < objc; i += 2) {
	option = TnmGetTableKeyFromObj(interp, config->optionTable,
				       objv[i], "option");
	code = (config->setOption)(interp, object, option, objv[i+1]);
	if (code != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    /*
     * Create a new list which contains all the configuration
     * options and their current values.
     */

    listPtr = Tcl_GetObjResult(interp);
    for (elemPtr = config->optionTable; elemPtr->value; elemPtr++) {
	objPtr = (config->getOption)(interp, object, (int) elemPtr->key);
	if (objPtr) {
	    Tcl_ListObjAppendElement(interp, listPtr, 
				     Tcl_NewStringObj(elemPtr->value, -1));
	    Tcl_ListObjAppendElement(interp, listPtr, objPtr);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetConfig --
 *
 *	This procedure retrieves the value of a configuration option
 *	of an object. It is usually used to process the cget object
 *	command.
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
TnmGetConfig(Tcl_Interp *interp, TnmConfig *config, ClientData object, int objc, Tcl_Obj *const objv[])
{
    int option;
    Tcl_Obj *objPtr;

    if (objc != 3) {
	Tcl_WrongNumArgs(interp, 2, objv, "option");
	return TCL_ERROR;
    }

    option = TnmGetTableKeyFromObj(interp, config->optionTable, 
				   objv[2], "option");
    if (option < 0) {
	return TCL_ERROR;
    }

    objPtr = (config->getOption)(interp, object, option);
    if (! objPtr) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "invalid option \"", 
			 Tcl_GetStringFromObj(objv[2], NULL), "\"", 
			 (char *) NULL);
	return TCL_ERROR;
    }

    Tcl_SetObjResult(interp, objPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmVectorInit --
 *
 *	This procedure initializes a Tnm vector.
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
TnmVectorInit(TnmVector *vPtr)
{
    vPtr->elements = vPtr->staticSpace;
    vPtr->size = 0;
    vPtr->spaceAvl = TNM_VECTOR_STATIC_SIZE;
    memset((char *) vPtr->staticSpace, 0, 
	   (vPtr->spaceAvl + 1) * sizeof(ClientData));
}

/*
 *----------------------------------------------------------------------
 *
 * TnmVectorFree --
 *
 *	This procedure frees all resources allocated for a Tnm vector
 *	and re-initializes the Tnm vector.
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
TnmVectorFree(TnmVector *vPtr)
{
    if (vPtr->elements != vPtr->staticSpace) {
	ckfree((char *) vPtr->elements);
    }
    vPtr->elements = vPtr->staticSpace;
    vPtr->size = 0;
    vPtr->spaceAvl = TNM_VECTOR_STATIC_SIZE;
    memset((char *) vPtr->staticSpace, 0, 
	   (vPtr->spaceAvl + 1) * sizeof(ClientData));
}

/*
 *----------------------------------------------------------------------
 *
 * TnmVectorAdd --
 *
 *	This procedure adds a ClientData element to a given vector.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The vector might grow in size.
 *
 *----------------------------------------------------------------------
 */

void
TnmVectorAdd(TnmVector *vPtr, ClientData clientData)
{
    int i;
    size_t size;
    ClientData *dynamicSpace;

    if (vPtr->size == vPtr->spaceAvl) {
	vPtr->spaceAvl += TNM_VECTOR_STATIC_SIZE;
	size = (vPtr->spaceAvl + 1) * sizeof(ClientData);
	dynamicSpace = (ClientData *) ckalloc(size);
	memset((char *) dynamicSpace, 0, size);
	for (i = 0; i < vPtr->size; i++) {
	    dynamicSpace[i] = vPtr->elements[i];
	}
	if (vPtr->elements != vPtr->staticSpace) {
	    ckfree((char *) vPtr->elements);
	}
	vPtr->elements = dynamicSpace;
    }
    vPtr->elements[vPtr->size++] = clientData;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmVectorDelete --
 *
 *	This procedure deletes a ClientData element from a given vector.
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
TnmVectorDelete(TnmVector *vPtr, ClientData clientData)
{
    int i, found = 0;

    /* fprintf(stderr, "TnmVectorDelete()...\n"); */

    for (i = 0; i < vPtr->size; i++) {
	found = (clientData == vPtr->elements[i]);
	if (found) break;
    }

    for (; found && i < vPtr->size; i++) {
	vPtr->elements[i] = vPtr->elements[i+1];
	/* fprintf(stderr, "moving...\n"); */
    }

    if (found) vPtr->size--;
}
#if 0

/*
 *----------------------------------------------------------------------
 *
 * TnmGetUnsigned --
 *
 *	This procedure converts a string into an unsigned integer 
 *	value. This is simply Tcl_GetInt() with an additional check.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmGetUnsigned(Tcl_Interp *interp, char *string, int *intPtr)
{
    int code;

    code = Tcl_GetInt(interp, string, intPtr);
    if (code != TCL_OK || *intPtr < 0) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "expected unsigned integer but got \"",
			 string, "\"", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TnmGetUnsignedFromObj --
 *
 *	This procedure converts an object into an unsigned integer 
 *	value. This is simply Tcl_GetIntFromObj() with an additional
 *	check.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmGetUnsignedFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int *intPtr)
{
    int code;

    code = Tcl_GetIntFromObj(interp, objPtr, intPtr);
    if (code != TCL_OK || *intPtr < 0) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "expected unsigned integer but got \"",
			 Tcl_GetStringFromObj(objPtr, NULL), "\"", 
			 (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}
#if 0

/*
 *----------------------------------------------------------------------
 *
 * TnmGetPositive --
 *
 *	This procedure converts a string into a positive integer 
 *	value. This is simply Tcl_GetInt() with an additional check.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmGetPositive(Tcl_Interp *interp, char *string, int *intPtr)
{
    int code;

    code = Tcl_GetInt(interp, string, intPtr);
    if (code != TCL_OK || *intPtr < 1) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "expected positive integer but got \"",
			 string, "\"", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TnmGetPositiveFromObj --
 *
 *	This procedure converts a string into a positive integer 
 *	value. This is simply Tcl_GetIntFromObj() with an additional
 *	check.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmGetPositiveFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int *intPtr)
{
    int code;

    code = Tcl_GetIntFromObj(interp, objPtr, intPtr);
    if (code != TCL_OK || *intPtr < 1) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "expected positive integer but got \"",
			 Tcl_GetStringFromObj(objPtr, NULL), "\"",
			 (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetIntRangeFromObj --
 *
 *	This procedure converts a string into an integer value and 
 *	checks whether the value is in the given range. This is simply
 *	Tcl_GetIntFromObj() with an additional check.
 *
 * Results:
 *	A standard TCL result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmGetIntRangeFromObj(Tcl_Interp *interp, Tcl_Obj *objPtr, int min, int max, int *intPtr)
{
    int code;
    char buffer[40];

    code = Tcl_GetIntFromObj(interp, objPtr, intPtr);
    if (code != TCL_OK || *intPtr < min || *intPtr > max) {
	Tcl_ResetResult(interp);
	sprintf(buffer, "%d and %d", min, max);
	Tcl_AppendResult(interp, "expected integer between ", buffer,
		 " but got \"", Tcl_GetStringFromObj(objPtr, NULL), "\"",
		 (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}
#if 0

/*
 *----------------------------------------------------------------------
 *
 * TnmPrintCounter64 --
 *
 *	This procedure converts a 64 bit counter value stored in two
 *	32 bit numbers into a string. This is tricky code borrowed
 *	from Marshall Rose.
 *
 * Results:
 *	The printed number is returned in static space.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char*
TnmPrintCounter64(u_int high, u_int low)
{
    double d;
    int j, carry;
    char *cpf, *cpl, last[6];
    static char first[30];

    Tcl_MutexLock(&utilMutex);
    
    if (high == 0) {
	sprintf(first, "%u", low);
	Tcl_MutexUnlock(&utilMutex);
	return first;
    }

    d = high * 4294967296.0;	/* 2^32 */
    if (high <= 0x1fffff) { 
	d += low;
	sprintf(first, "%.0f", d);
	Tcl_MutexUnlock(&utilMutex);
	return first;
    }

    d += (low & 0xfffff000);
    sprintf(first, "%.0f", d);
    sprintf(last, "%5.5d", low & 0xfff);
    for (carry = 0, cpf = first+strlen(first)-1, cpl = last+4;
	 cpl >= last;
	 cpf--, cpl--) {
	j = carry + (*cpf - '0') + (*cpl - '0');
	if (j > 9) {
	    j -= 10;
	    carry = 1;
	} else {
	    carry = 0;
	}
	*cpf = j + '0';
    }
    Tcl_MutexUnlock(&utilMutex);
    return first;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TnmSetIPAddress --
 *
 *	This procedure retrieves the network address for the given
 *	host name or address. The argument is validated to ensure that
 *	only legal IP address and host names are accepted. This
 *	procedure maintains a cache of successful name lookups to
 *	reduce the overall DNS overhead.
 *
 * Results:
 *	A standard TCL result. This procedure leaves an error message 
 *	in interp->result if interp is not NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmSetIPAddress(Tcl_Interp *interp, const char *host, struct sockaddr_in *addr)
{
    static Tcl_HashTable *hostTable = NULL;
    Tcl_HashEntry *hostEntry;
    struct hostent *hp = NULL;
    int code, type;

#define TNM_IP_HOST_NAME 1
#define TNM_IP_HOST_ADDRESS 2

    Tcl_MutexLock(&utilMutex);

    if (hostTable == NULL) {
	hostTable = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(hostTable, TCL_STRING_KEYS);
    }
    addr->sin_family = AF_INET;

    /*
     * First check whether we got a host name, an IP address or
     * something else completely different.
     */

    type = TNM_IP_HOST_NAME;
    code = TnmValidateIpHostName(NULL, host);
    if (code != TCL_OK) {
	code = TnmValidateIpAddress(NULL, host);
	if (code != TCL_OK) {
	    if (interp) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "illegal IP address or name \"",
				 host, "\"", (char *) NULL);
	    }
	    Tcl_MutexUnlock(&utilMutex);
	    return TCL_ERROR;
	}
	type = TNM_IP_HOST_ADDRESS;
    }

    /*
     * Convert the IP address into the internal format. Note that
     * inet_addr can't handle the valid address 255.255.255.255.
     */

    if (type == TNM_IP_HOST_ADDRESS) {
	int hostaddr = inet_addr(host);
	if (hostaddr == -1 && strcmp(host, "255.255.255.255") != 0) {
	    if (interp) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "invalid IP address \"", 
				 host, "\"", (char *) NULL);
	    }
	    Tcl_MutexUnlock(&utilMutex);
	    return TCL_ERROR;
	}
	memcpy((char *) &addr->sin_addr, (char *) &hostaddr, 4);
	Tcl_MutexUnlock(&utilMutex);
	return TCL_OK;
    }

    /*
     * Try to convert the name into an IP address. First check
     * whether this name is already known in our address cache.
     * If not, try to resolve the name and add an entry to the
     * cache if successful. Otherwise return an error.
     */

    if (type == TNM_IP_HOST_NAME) {
	struct sockaddr_in *caddr;
	int isnew;

	hostEntry = Tcl_FindHashEntry(hostTable, host);
	if (hostEntry) {
	    caddr = (struct sockaddr_in *) Tcl_GetHashValue(hostEntry);
	    addr->sin_addr.s_addr = caddr->sin_addr.s_addr;
	    Tcl_MutexUnlock(&utilMutex);
	    return TCL_OK;
	}

	hp = gethostbyname(host);
	if (! hp) {
	    if (interp) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "unknown IP host name \"", 
				 host, "\"", (char *) NULL);
	    }
	    Tcl_MutexUnlock(&utilMutex);
	    return TCL_ERROR;
	}

	memcpy((char *) &addr->sin_addr,
	       (char *) hp->h_addr, (size_t) hp->h_length);
	caddr = (struct sockaddr_in *) ckalloc(sizeof(struct sockaddr_in));
	*caddr = *addr;
	hostEntry = Tcl_CreateHashEntry(hostTable, host, &isnew);
	Tcl_SetHashValue(hostEntry, (ClientData) caddr);
	Tcl_MutexUnlock(&utilMutex);
	return TCL_OK;
    }

    /*
     * We should not reach here.
     */

    Tcl_MutexUnlock(&utilMutex);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetIPName --
 *
 *	This procedure retrieves the network name for the given
 *	network address. It maintains a cache of the last name lookups
 *	to reduce overhead.
 *
 * Results:
 *	A pointer to a static string containing the name or NULL
 *	if the name could not be found. An error message is left
 *	in the interpreter if interp is not NULL
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TnmGetIPName(Tcl_Interp *interp, struct sockaddr_in *addr)
{
    static Tcl_HashTable *hostTable = NULL;
    Tcl_HashEntry *hostEntry;
    struct hostent *host;

    Tcl_MutexLock(&utilMutex);

    if (hostTable == NULL) {
	hostTable = (Tcl_HashTable *) ckalloc(sizeof(Tcl_HashTable));
	Tcl_InitHashTable(hostTable, TCL_ONE_WORD_KEYS);
    }

    hostEntry = Tcl_FindHashEntry(hostTable, (char *) addr->sin_addr.s_addr);
    if (hostEntry) {
	Tcl_MutexUnlock(&utilMutex);
	return (char *) Tcl_GetHashValue(hostEntry);
    }
    
    host = gethostbyaddr((char *) &addr->sin_addr, 4, AF_INET);
    if (host) {
	int isnew;
	char *name = ckstrdup(host->h_name);
	hostEntry = Tcl_CreateHashEntry(hostTable,
					(char *) addr->sin_addr.s_addr,
					&isnew);
	Tcl_SetHashValue(hostEntry, (ClientData) name);
	Tcl_MutexUnlock(&utilMutex);
	return name;
    }

    if (interp) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "unknown IP address \"", 
			 inet_ntoa(addr->sin_addr), "\"", (char *) NULL);
    }
    Tcl_MutexUnlock(&utilMutex);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSetIPPort --
 *
 *	This procedure interprets a string value as an IP port number
 *	and writes the port into the socket address structure.
 *
 * Results:
 *	A standard TCL result. This procedure leaves an error message 
 *	in interp->result if interp is not NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmSetIPPort(Tcl_Interp *interp, char *protocol, char *port, struct sockaddr_in* addr)
{
    char *p = port;
    int isnum = 1;

    if (strcmp(protocol, "udp") != 0 && strcmp(protocol, "tcp") != 0) {
	if (interp) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "unknown IP protocol \"", 
			     protocol, "\"", (char *) NULL);
	}
	return TCL_ERROR;
    }

    while (*p) {
	if (!isdigit(*p++)) isnum = 0;
    }

    if (isnum) {
	int number = atoi(port);
	if (number >= 0) {
	    addr->sin_port = htons((unsigned short) number);
	    return TCL_OK;
	}
    } else {
	struct servent *servent = getservbyname(port, protocol);
	if (servent) {
	    addr->sin_port = servent->s_port;
	    return TCL_OK;
	}
    }

    if (interp) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "unknown ", protocol, " port \"", 
			 port, "\"", (char *) NULL);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetIPPort --
 *
 *	This procedure retrieves the service name for the given
 *	IP port.
 *
 * Results:
 *	A pointer to a static string containing the name or NULL
 *	if the name could not be found. An error message is left
 *	in the interpreter if interp is not NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TnmGetIPPort(Tcl_Interp *interp, char *protocol, struct sockaddr_in *addr)
{
    struct servent *serv;

    if (strcmp(protocol, "udp") != 0 && strcmp(protocol, "tcp") != 0) {
	if (interp) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "unknown IP protocol \"", 
			     protocol, "\"", (char *) NULL);
	}
	return NULL;
    }

    serv = getservbyport(addr->sin_port, protocol);
    if (! serv) {
	char buffer[20];
	sprintf(buffer, "%d", ntohs(addr->sin_port));
	if (interp) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "unknown ", protocol, " port \"", 
			     buffer, "\"", (char *) NULL);
	}
	return NULL;
    }

    return serv->s_name;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmValidateIpHostName --
 *
 *	This procedure should be called to validate IP host names.
 *	Some resolver libraries do not check host names very well
 *	which might yield security problems. Some of these resolver
 *	libraries even return random junk if you call gethostbyname()
 *	with an empty string. This procedure checks the rules defined
 *	in RFC 952 and 1123 since we can't rely on the operating
 *	system.
 *
 * Results:
 *	A standard Tcl result. An error message is left in the Tcl
 *	interpreter if it is not NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmValidateIpHostName(Tcl_Interp *interp, const char *name)
{
    const char *p = name;
    char last = ' ';
    int dots = 0, alpha = 0;

    /*
     * A host name must start with one of the characters [a-zA-Z0-9]
     * and continue with characters from the set [-.a-zA-Z0-9] and
     * must not end with a '-'. Names that only contain
     * digits and three dots are also not allowed.
     *
     * NOTE: a hostname is allowed to end with a dot, which the previous version of this code
     * explicitly disallowed.
     */

    if (! isalnum(*p)) {
	goto error;
    }

    while (isalnum(*p) || *p == '-' || *p == '.') {
	if (*p == '.') dots++;
	if (isalpha(*p)) alpha++;
	last = *p++;
    }

    if (*p == '\0' && (isalnum(last) || last == '.') && (alpha || dots != 3)) {
	return TCL_OK;
    }

 error:
    if (interp) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "illegal IP host name \"",
			 name, "\"", (char *) NULL);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmValidateIpAddress --
 *
 *	This procedure should be called to validate IP addresses.  An
 *	IP address is accepted as valid if and only if it consists of
 *	a string with the format [0-9]+.[0-9]+.[0-9]+.[0-9]+ where
 *	each number is in the range [0-255].
 *
 *	(Note, this check might change if we start to support IPv6.)
 *
 * Results:
 *	A standard Tcl result. An error message is left in the Tcl
 *	interpreter if it is not NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmValidateIpAddress(Tcl_Interp *interp, const char *address)
{
    const char *p = address;
    unsigned dots, a;

    for (dots = 0, a = 0; isdigit(*p) || *p == '.'; p++) {
	if (*p == '.') {
	    dots++, a = 0;
	} else {
	    a = 10 * a + *p - '0';
	}
	if (dots > 3 || a > 255) {
	    goto error;
	}
    }

    if (*p == '\0' && dots == 3) {
	return TCL_OK;
    }

 error:
    if (interp) {
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "illegal IP address \"",
			 address, "\"", (char *) NULL);
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmWriteMessage --
 *
 *	This procedure writes a message to the error channel. This 
 *	should only be used in situations where there is not better
 *	way to handle the run-time error in Tcl.
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
TnmWriteMessage(const char *msg)
{
    Tcl_Channel channel;

    channel = Tcl_GetStdChannel(TCL_STDERR);
    if (channel) {
	Tcl_Write(channel, msg, -1);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmAttrClear --
 *
 *	This procedure clears a Tcl hash table containing attributes.
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
TnmAttrClear(Tcl_HashTable *tablePtr)
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;

    entryPtr = Tcl_FirstHashEntry(tablePtr, &search);
    while (entryPtr != NULL) {
        ckfree((char *) Tcl_GetHashValue(entryPtr));
        entryPtr = Tcl_NextHashEntry(&search);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmAttrList --
 *
 *	This procedure list all attribute names in a Tcl hash table.
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
TnmAttrList(Tcl_HashTable *tablePtr, Tcl_Interp *interp)
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    Tcl_Obj *listPtr, *elemObjPtr;

    listPtr = Tcl_GetObjResult(interp);
    entryPtr = Tcl_FirstHashEntry(tablePtr, &search);
    while (entryPtr) {
	elemObjPtr = Tcl_NewStringObj(Tcl_GetHashKey(tablePtr, entryPtr), -1);
	Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	entryPtr = Tcl_NextHashEntry(&search);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmAttrSet --
 *
 *	This procedure is used to get or set an attribute. It verifies
 *	that attribute names only consist of valid characters.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmAttrSet(Tcl_HashTable *tablePtr, Tcl_Interp *interp, char *name, char *value)
{
    Tcl_HashEntry *entryPtr;
    int isNew;
    char *p;

    entryPtr = Tcl_FindHashEntry(tablePtr, name);

    if (value) {

	/*
	 * Check the character set of the name to make sure that
	 * we do not run into quoting hell problems just because
	 * people put funny characters into the names.
	 */

	for (p = name; *p; p++) {
	    if (!isalnum(*p) && *p != ':') {
		Tcl_SetResult(interp, "illegal character in attribute name",
			      TCL_STATIC);
		return TCL_ERROR;
	    }
	}

	if (! entryPtr) {
	    entryPtr = Tcl_CreateHashEntry(tablePtr, name, &isNew);
	} else {
	    ckfree((char *) Tcl_GetHashValue(entryPtr));
	}
	if (*value) {
	    Tcl_SetHashValue(entryPtr, ckstrdup(value));
	} else {
	    Tcl_DeleteHashEntry(entryPtr);
	    entryPtr = NULL;
	}
    }

    if (entryPtr) {
	Tcl_SetResult(interp, (char *) Tcl_GetHashValue(entryPtr), TCL_STATIC);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmAttrDump --
 *
 *	This procedure is used to dump persistent attributes into
 *	a dynamic string. Attributes with lower case characters are
 *	silently ignored.
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
TnmAttrDump(Tcl_HashTable *tablePtr, char *name, Tcl_DString *dsPtr)
{
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;
    char *key, *value;
    
    entryPtr = Tcl_FirstHashEntry(tablePtr, &search);
    while (entryPtr != NULL) {
	key = Tcl_GetHashKey(tablePtr, entryPtr);
	value = (char *) Tcl_GetHashValue(entryPtr);
	if (isupper(*key) || *key == ':') {
	    Tcl_DStringAppend(dsPtr, name, -1);
	    Tcl_DStringAppend(dsPtr, " attribute ", -1);
	    Tcl_DStringAppendElement(dsPtr, key);
	    Tcl_DStringAppendElement(dsPtr, value);
	    Tcl_DStringAppend(dsPtr, "\n", 1);
	}
	entryPtr = Tcl_NextHashEntry(&search);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmHexEnc --
 *
 *	This procedure converts the binary buffer s with len n into
 *	human readable format by encoding all bytes in hex digits
 *	seperated by a colon (1A:2B:3D).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The result is returned in d (with a trailing 0 character).
 *
 *----------------------------------------------------------------------
 */

void
TnmHexEnc(char *s, int n, char *d)
{
    while (n-- > 0) {
	char c = *s++;
	int c1 = (c & 0xf0) >> 4;
	int c2 = c & 0x0f;
	if ((c1 += '0') > '9') c1 += 7;
	if ((c2 += '0') > '9') c2 += 7;
	*d++ = c1, *d++ = c2;
	if (n > 0) {
	    *d++ = ':';
	}
    }
    *d = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmHexDec --
 *
 *	This procedure converts the human readable hex string s into
 *	the corresponding binary sequence of octets. The octets are
 *	written into the buffer d.
 *
 * Results:
 *	The length of the binary buffer or -1 in case of an error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmHexDec(s, d, n)
    const char *s;
    char *d;
    int *n;
{
    int v;
    char c;

    *n = 0;
    while (s[0] && s[1]) {
	c = *s++ & 0xff;
	if (! isxdigit(c)) return -1;
	v = c >= 'a' ?  c - 87 : (c >= 'A' ? c - 55 : c - 48);
	c = *s++ & 0xff;
	if (! isxdigit(c)) return -1;
	v = (v << 4) + (c >= 'a' ?  c - 87 : (c >= 'A' ? c - 55 : c - 48));
	*d++ = v;
	(*n)++;
	if (*s == ':') {
	    s++;
	}
    }
    if (s[0]) {
	return -1;
    }

    return *n;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmGetHandle --
 *
 *	This procedure creates a handle for an object which is 
 *	represented by a Tcl command. This procedure ensures that
 *	the name is currently unused.
 *
 * Results:
 *	A pointer to the handle string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char*
TnmGetHandle(Tcl_Interp *interp, char *prefix, unsigned *id)
{
    static char buffer[40];
    Tcl_CmdInfo cmdInfo;

    Tcl_MutexLock(&utilMutex);
    do {
	memset(buffer, 0, sizeof(buffer));
	strncpy(buffer, prefix, 20);
	sprintf(buffer+strlen(buffer), "%u", (*id)++);
    } while (Tcl_GetCommandInfo(interp, buffer, &cmdInfo));
    Tcl_MutexUnlock(&utilMutex);

    return buffer;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMatchTags --
 *
 *	This procedure matches a tag list against a list of pattern.
 *
 * Results:
 *	1 if one of the pattern matches at least one tag in the tag
 *	list and 0 otherwise. The value -1 is returned if one of the
 *	arguments is not a valid Tcl list.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmMatchTags(Tcl_Interp *interp, Tcl_Obj *tagListObj, Tcl_Obj *patternListObj)
{
    int i, j, code, tagLen, patLen;
    Tcl_Obj **tagPtrs, **patPtrs;
    int match = 0;

    code = Tcl_ListObjGetElements(interp, tagListObj, &tagLen, &tagPtrs);
    if (code != TCL_OK) {
	return -1;
    }
    code = Tcl_ListObjGetElements(interp, patternListObj, &patLen, &patPtrs);
    if (code != TCL_OK) {
	return -1;
    }

    for (i = 0; i < patLen; i++) {
	match = 0;
	for (j = 0; j < tagLen && !match; j++) {
	    match = Tcl_StringMatch(Tcl_GetStringFromObj(tagPtrs[j], NULL),
				    Tcl_GetStringFromObj(patPtrs[i], NULL));
	}
	if (! match) {
	    return 0;
	}
    }

    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMkDir --
 *
 *	This procedure creates a path of directories. This is largely
 *	based on the implementation of the Tcl mkdir command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Creates directories in the file system.
 *
 *----------------------------------------------------------------------
 */

int
TnmMkDir(Tcl_Interp *interp, Tcl_Obj *obj)
{
    int result = TCL_OK, j, pobjc;
    Tcl_Obj *split = NULL;
    Tcl_Obj *target = NULL;
    Tcl_Obj *errfile = NULL;
    Tcl_StatBuf statBuf;

    if (Tcl_FSConvertToPathType(interp, obj) != TCL_OK) {
	result = TCL_ERROR;
	goto done;
    }

    split = Tcl_FSSplitPath(obj, &pobjc);
    if (pobjc == 0) {
	errno = ENOENT;
	errfile = obj;
	goto done;
    }

    for (j = 0; j < pobjc; j++) {

	target = Tcl_FSJoinPath(split, j + 1);
	Tcl_IncrRefCount(target);
	
	/*
	 * Call TnmStat() so that if target is a symlink that points
	 * to a directory we will create subdirectories in that
	 * directory.
	 */

	if (Tcl_FSStat(target, &statBuf) == 0) {
	    if (!S_ISDIR(statBuf.st_mode)) {
		errno = EEXIST;
		errfile = target;
		goto done;
	    }
	} else if ((errno != ENOENT)
		   || (Tcl_FSCreateDirectory(target) != TCL_OK)) {
	    errfile = target;
	    goto done;
	}
	/* Forget about this sub-path */
	Tcl_DecrRefCount(target);
	target = NULL;
    }
    Tcl_DecrRefCount(split);
    split = NULL;

 done:
    if (errfile != NULL) {
	Tcl_AppendResult(interp, "can't create directory \"",
		 Tcl_GetString(errfile), "\": ", Tcl_PosixError(interp),
			 (char *) NULL);
	result = TCL_ERROR;
    }
    if (split != NULL) {
	Tcl_DecrRefCount(split);
    }
    if (target != NULL) {
	Tcl_DecrRefCount(target);
    }
    
    return result;
}

