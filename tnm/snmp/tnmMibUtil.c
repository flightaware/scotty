/*
 * tnmMibUtil.c --
 *
 *	Utility functions to search in the MIB tree.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmMibUtil.c,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

#include "tnmSnmp.h"
#include "tnmMib.h"

/*
 * Global variables of the MIB extension:
 */

char *tnmMibFileName = NULL;		/* Current MIB file name loaded.   */
char *tnmMibModuleName = NULL;		/* Current MIB module name loaded. */
TnmMibNode *tnmMibTree = NULL;		/* The root of the MIB tree.	   */
TnmMibType *tnmMibTypeList = NULL;	/* List of textual conventions.	   */
TnmMibType *tnmMibTypeSaveMark = NULL;	/* The first already saved	   */
					/* element of tnmMibTypeList.	   */

TnmTable tnmMibAccessTable[] = {
    { TNM_MIB_NOACCESS,   "not-accessible" },
    { TNM_MIB_READONLY,   "read-only" },
    { TNM_MIB_READCREATE, "read-create" },
    { TNM_MIB_READWRITE,  "read-write" },
    { TNM_MIB_FORNOTIFY,  "accessible-for-notify" },
    { 0, NULL }
};

TnmTable tnmMibMacroTable[] = {
    { TNM_MIB_OBJECTTYPE,        "OBJECT-TYPE" },
    { TNM_MIB_OBJECTIDENTITY,    "OBJECT-IDENTITY" },
    { TNM_MIB_MODULEIDENTITY,    "MODULE-IDENTITY" },
    { TNM_MIB_NOTIFICATIONTYPE,  "NOTIFICATION-TYPE" },
    { TNM_MIB_TRAPTYPE,          "TRAP-TYPE" },
    { TNM_MIB_OBJECTGROUP,       "OBJECT-GROUP" },
    { TNM_MIB_NOTIFICATIONGROUP, "NOTIFICATION-GROUP" },
    { TNM_MIB_COMPLIANCE,        "MODULE-COMPLIANCE" },
    { TNM_MIB_CAPABILITIES,      "AGENT-CAPABILITIES" },
    { TNM_MIB_TEXTUALCONVENTION, "TEXTUAL-CONVENTION" },
    { TNM_MIB_TYPE_ASSIGNMENT,	 "TYPE-ASSIGNEMENT" },
    { TNM_MIB_VALUE_ASSIGNEMENT, "VALUE-ASSIGNEMENT" },
    { 0, NULL }
};

TnmTable tnmMibStatusTable[] = {
    { TNM_MIB_CURRENT,		"current" },
    { TNM_MIB_DEPRECATED,	"deprecated" },
    { TNM_MIB_OBSOLETE,		"obsolete" },
    { 0, NULL }
};

/*
 * A private buffer that is used to assemble object identifier in 
 * dottet notation.
 */

static char oidBuffer[TNM_OID_MAX_SIZE * 8];

/*
 * Forward declarations for procedures defined later in this file:
 */

static Tcl_Obj*
FormatOctetTC		(Tcl_Obj *val, char *fmt);

static Tcl_Obj*
FormatIntTC		(Tcl_Obj *val, char *fmt);

static Tcl_Obj*
ScanOctetTC		(Tcl_Obj *val, char *fmt);

static Tcl_Obj*
ScanIntTC		(Tcl_Obj *val, char *fmt);

static void
GetMibPath		(TnmMibNode *nodePtr, char *soid);

static void
FormatUnsigned		(unsigned u, char *s);


/*
 *----------------------------------------------------------------------
 *
 * FormatUnsigned --
 *
 *	This procedure formats the unsigned value in u into an 
 *	unsigned ascii string s. We avoid sprintf because this is 
 *	too slow.
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
FormatUnsigned(u_int u, char *s)
{
    if (u < 10) {
	*s++ = '0' + u;
    } else {
	u_int t=10;
	char c = '0'+ (u % 10);
	u /= 10;
	while (u / t) t *= 10;
	while (t /= 10) *s++ = '0'+ (u / t) % 10;
	*s++ = c;
    }
    *s = '\0';
}

/*
 *----------------------------------------------------------------------
 *
 * GetMibPath --
 *
 *	This procedure writes the path to the given by nodePtr into
 *	the given character string. This is done recursively using the 
 *	pointers to the parent node.
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
GetMibPath(TnmMibNode *nodePtr, char *s)
{
    if (! nodePtr) return;
    if (nodePtr->parentPtr) {
	GetMibPath(nodePtr->parentPtr, s);
	while (*s) s++;
	*s++ = '.';
    }
    FormatUnsigned(nodePtr->subid, s);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibNodeToOid --
 *
 *	This procedure retrieves the object identifier of the node 
 *	given by nodePtr. This is done recursively by going up to
 *	the root and afterwards assembling the object identifier.
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
TnmMibNodeToOid(TnmMibNode *nodePtr, TnmOid *oidPtr)
{
    if (! nodePtr) {
	TnmOidFree(oidPtr);
    }

    if (nodePtr->parentPtr) {
	TnmMibNodeToOid(nodePtr->parentPtr, oidPtr);
    }
    TnmOidAppend(oidPtr, nodePtr->subid);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibGetOid --
 *
 *	This procedure returns the oid that belongs to label.
 *
 * Results:
 *	A pointer to a static string containing the object identifier
 *	in dotted notation or NULL if there is no such MIB node.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char*
TnmMibGetOid(const char *label)
{
    char *expanded = TnmHexToOid(label);
    TnmMibNode *nodePtr;
    int offset = -1;

    if (expanded) label = expanded;
    nodePtr = TnmMibFindNode(label, &offset, 0);
    if (nodePtr) {
	if (TnmIsOid(label)) {
	  return (char *) label;
	}
	GetMibPath(nodePtr, oidBuffer);
	if (offset > 0) {
	    strcat(oidBuffer, label+offset);
	}
	return oidBuffer;
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibGetName --
 *
 *	This procedure returns the name that belongs to a descriptor.
 *	This is the reverse operation to TnmMibGetOid().
 *
 * Results:
 *	A pointer to a static string containing the object descriptor
 *	or NULL if there is no such MIB node.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char*
TnmMibGetName(char *label, int exact)
{
    char *expanded = TnmHexToOid(label);
    TnmMibNode *nodePtr;
    int offset = -1;
    
    if (expanded) label = expanded;
    nodePtr = TnmMibFindNode(label, &offset, exact);
    if (nodePtr) {
	if (offset > 0) {
	    strcpy(oidBuffer, nodePtr->label);
	    strcat(oidBuffer, label+offset);
	    return oidBuffer;
	} else {
	    return nodePtr->label;
	}
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibGetString --
 *
 * 	This procedure reads the next quoted string from the given
 *	file starting at the given file offset. White spaces following
 *	newline characters are removed.
 *
 * Results:
 *	A pointer to a static string containing the string or NULL
 *	if the description does not exist or is not accessible.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char*
TnmMibGetString(char *fileName, int fileOffset)
{
    static Tcl_DString *result = NULL;
    FILE *fp;
    int ch, indent = 0;
    
    if (result == NULL) {
	result = (Tcl_DString *) ckalloc(sizeof(Tcl_DString));
	Tcl_DStringInit(result);
    } else {
	Tcl_DStringFree(result);
    }

    /*
     * Ignore bogus arguments, open the file and seek to the beginning
     * of the quoted string (this allows some fuzz in the offset
     * value).
     */
    
    if (fileName == NULL || fileOffset <= 0) {
	return NULL;
    }

    fp = fopen(fileName, "rb");
    if (fp == NULL) {
	perror(fileName);
	return NULL;
    }
    if (fseek(fp, fileOffset, 0) < 0) {
	perror(fileName);
	return NULL;
    }
    while ((ch = getc(fp)) != EOF) {
	if (ch == '"') break;
    }

    /*
     * Copy the string into the buffer until we find the terminating
     * quote character. Ignore all the white spaces at the beginning
     * of a new line so that the resulting format is independent from
     * indentation in the MIB file. Calculate the first indentation
     * and strip away this many white spaces to preserve intended
     * lines. Newlines only separated by white space characters are
     * also preserved to allow for empty lines.
     */

    for (ch = getc(fp); ch != EOF && ch != '"'; ch = getc(fp)) {
	char c = ch;
	Tcl_DStringAppend(result, &c, 1);
	if (ch == '\n') {
	    int n = 0;
	    while ((ch = getc(fp)) != EOF) {
		if (ch == '\n') {
		    Tcl_DStringAppend(result, "\n", 1);
		    n = 0;
		    continue;
		}
		if (!isspace(ch)) break;
		if (++n == indent) break;
	    }
	    if (! indent && n) indent = n + 1;
	    if (ch == EOF || ch == '"') break;
	    c = ch;
	    Tcl_DStringAppend(result, &c, 1);
	}
    }

    fclose(fp);
    return Tcl_DStringValue(result);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibGetBaseSyntax --
 *
 *	This procedure returns the transfer syntax actually used to 
 *	encode a value. This may be different from the syntax defined
 *	in the OBJECT-TYPE macro as textual conventions are based on 
 *	a set of primitive types. Note: We have sometimes more than 
 *	one MIB syntax tag for an ASN.1 type, so we must make sure to 
 *	find the right syntax tag.
 *
 * Results:
 *	The ASN.1 transfer syntax or ASN1_OTHER if the lookup
 *	fails for some reason.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmMibGetBaseSyntax(const char *name)
{
    int syntax = ASN1_OTHER;
    TnmMibNode *nodePtr = TnmMibFindNode(name, NULL, 0);

    if (nodePtr) {
	if (nodePtr->typePtr && nodePtr->typePtr->name) {
	    syntax = nodePtr->typePtr->syntax;
	} else {
	    syntax = nodePtr->syntax;
	}
    }

    return syntax;
}

/*
 *----------------------------------------------------------------------
 *
 * FormatOctetTC --
 *
 *	This procedure formats the octet string value according to the 
 *	display hint stored in fmt.
 *
 * Results:
 *	The pointer to a static string containing the formatted value
 *	or NULL if there is no such MIB node.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
FormatOctetTC(Tcl_Obj *val, char *fmt)
{
    int i = 0, len, pfx, have_pfx;			/* counter prefix */
    char *last_fmt;			/* save ptr to last seen fmt */
    char *bytes;
    Tcl_Obj *objPtr;

    /* 
     * Perform some sanity checks and get the octets to be formatted.
     */

    bytes = TnmGetOctetStringFromObj(NULL, val, &len);
    if (! fmt || ! bytes) {
	return NULL;
    }

    if (strcmp(fmt, "1x:") == 0) {
	Tcl_InvalidateStringRep(val);
	return NULL;
    }

    objPtr = Tcl_NewStringObj(NULL, 0);

    while (*fmt && i < len) {

        last_fmt = fmt;		/* save for loops */

	have_pfx = pfx = 0;	/* scan prefix: */
	while (*fmt && isdigit((int) *fmt)) {
	    pfx = pfx * 10 + *fmt - '0', have_pfx = 1, fmt++;
	}
	if (! have_pfx) {
	    pfx = 1;
	}

	switch (*fmt) {
	case 'a': {
	    int k, n;
	    n = (pfx < (len-i)) ? pfx : len-i;
	    for (k = i; k < n; k++) {
		if (! isascii((int) bytes[k])) {
		    Tcl_DecrRefCount(objPtr);
		    return NULL;
		}
	    }
	    Tcl_AppendToObj(objPtr, bytes+i, n);
	    i += n;
	    break;
	}
	case 't': {
	    /* XXX */
	    Tcl_DecrRefCount(objPtr);
	    return NULL;
	}
	case 'b':
	case 'd':
	case 'o':
	case 'x': {

	    char buf[80];
	    long vv = 0;
	    int xlen = pfx * 2;

	    /* collect octets to format */
	    
	    while (pfx > 0 && i < len) {
		vv = vv * 256 + (bytes[i] & 0xff);
		i++;
		pfx--;
	    }
	    
	    switch (*fmt) {
	    case 'd':
	        sprintf(buf, "%ld", vv);
		break;
	    case 'o':
		sprintf(buf, "%lo", vv);
		break;
	    case 'x':
		sprintf(buf, "%.*lX", xlen, vv);
		break;
	    case 'b': {
	        int i, j; 
		for (i = (sizeof(int) * 8 - 1); i >= 0
			 && ! (vv & (1 << i)); i--);
		for (j = 0; i >= 0; i--, j++) {
		    buf[j] = vv & (1 << i) ? '1' : '0';
		}
		buf[j] = 0;
		break;
	    }
	    }
	    Tcl_AppendToObj(objPtr, buf, (int) strlen(buf));
	    break;
	}
	default:
	    Tcl_DecrRefCount(objPtr);
	    return NULL;
	}
	fmt++;

	/*
	 * Check for a separator and repeat with last format if
	 * data is still available.
	 */

	if (*fmt && ! isdigit((int) *fmt) && *fmt != '*') {
	    if (i < len) {
		Tcl_AppendToObj(objPtr, fmt, 1);
	    }
	    fmt++;
	}

	if (! *fmt && (i < len)) {
	    fmt = last_fmt;
	}
    }

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * FormatIntTC --
 *
 *	This procedure formats the integer value according to the 
 *	display hint stored in fmt.
 *
 * Results:
 *
 *	The pointer to a Tcl_Obj which has the correct representation
 *	or NULL if there was no need to perform a conversion or the
 *	conversion could not be made due to an error condition.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
FormatIntTC(Tcl_Obj *val, char *fmt)
{
    long value;
    Tcl_Obj *objPtr = NULL;
    int i = 0, j = 0, dpt = -1, sign = 0;
    char *s, *d;
    int slen;
    char buffer[80];

    /* 
     * Perform some sanity checks and get the integer value.
     */

    if (! fmt || (Tcl_GetLongFromObj(NULL, val, &value) != TCL_OK)) {
	return NULL;
    }

    switch (fmt[0]) {
    case 'd':
	if (! fmt[1]) {
	    Tcl_InvalidateStringRep(val);
	    return NULL;
	}
	if (fmt[1] != '-') break;		/* invalid format string */
	if (isdigit((int) fmt[2])) {
	    for (dpt = 0, i = 0; isdigit((int) fmt[2+i]); i++) {
		dpt = dpt * 10 + fmt[2+i] - '0';
	    }
	}
	if (fmt[2+i]) break;			/* invalid format string */
	objPtr = Tcl_NewStringObj(NULL, 0);
	s = Tcl_GetStringFromObj(val, &slen);
	if (s[0] == '-') {
	    sign = 1;
	    s++, slen--;
	}
	if (dpt >= slen) {
	    Tcl_SetObjLength(objPtr, slen + (dpt - slen) + 2 + sign);
	    d = Tcl_GetStringFromObj(objPtr, NULL);
	    if (sign) *d++ = '-';
	    *d++ = '0', *d++ = '.';
	    for (i = 0; i < dpt - slen; i++) {
		*d++ = '0';
	    }
	    strcpy(d, s);
	} else {
	    Tcl_SetObjLength(objPtr, slen + 1 + sign);
	    d = Tcl_GetStringFromObj(objPtr, NULL);
	    if (sign) *d++ = '-';
	    for (i = 0; i < slen - dpt; i++) {
		*d++ = s[i];
	    }
	    *d++ = '.';
	    for (; i < slen; i++) {
		*d++ = s[i];
	    }
	    *d = 0;
	}
	break;
    case 'x':
	if (fmt[1]) break;			/* invalid format string */
	sprintf(buffer,
		(value < 0) ? "-%lx" : "%lx",
		(value < 0) ? (unsigned long) -1 * value : value);
	objPtr = Tcl_NewStringObj(buffer, (int) strlen(buffer));
	break;
    case 'o':
	if (fmt[1]) break;			/* invalid format string */
	sprintf(buffer,
		(value < 0) ? "-%lo" : "%lo",
		(value < 0) ? (unsigned long) -1 * value : value);
	objPtr = Tcl_NewStringObj(buffer, (int) strlen(buffer));
	break;
    case 'b':
	if (fmt[1]) break;			/* invalid format string */
	if (value < 0) {
	    buffer[j++] = '-';
	    value *= -1;
	}
	for (i = (sizeof(long) * 8 - 1); i > 0 && ! (value & (1 << i)); i--);
	for (; i >= 0; i--, j++) {
	    buffer[j] = value & (1 << i) ? '1' : '0';
	}
	buffer[j] = 0;
	objPtr = Tcl_NewStringObj(buffer, (int) strlen(buffer));
	break;
    default:
	break;
    }

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ScanOctetTC --
 *
 *	This procedure scans a string using the display hint stored in
 *	fmt and returns the underlying OCTET STRING value.
 *
 * Results:
 *	A pointer to a Tcl_Obj which contains the octet string value
 *	or NULL if there is no need to perform a conversion or the
 *	converion could not be made due to an error condition.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
ScanOctetTC(Tcl_Obj *val, char *fmt)
{
    int i = 0, valid = 0, len, pfx, have_pfx;		/* counter prefix */
    char *last_fmt;			/* save ptr to last seen fmt */
    char *string;
    Tcl_Obj *objPtr;
    long vv;

    /*
     * Perform some sanity checks and get the string to be scanned.
     */

    string = Tcl_GetStringFromObj(val, &len);
    if (! fmt || ! string) {
        return NULL;
    }

    if (strcmp(fmt, "1x:") == 0) {
	objPtr = Tcl_DuplicateObj(val);
	if (Tcl_ConvertToType((Tcl_Interp *) NULL,
			      objPtr, &tnmOctetStringType) != TCL_OK) {
	    Tcl_DecrRefCount(objPtr);
	    return NULL;
	}
	return objPtr;
    }

    /*
     * We use a string object here and we convert it later to an
     * octet string object. This allows us to use the functions
     * already defined for string objects.
     */
    
    objPtr = Tcl_NewStringObj(NULL, 0);
    
    while (*fmt && i < len) {

	last_fmt = fmt;		/* save for loops */

	have_pfx = pfx = 0;	/* scan prefix */
	while (*fmt && isdigit((int) *fmt)) {
	    pfx = pfx * 10 + *fmt - '0', have_pfx = 1, fmt++;
	}
	if (! have_pfx) {
	    pfx = 1;
	}

	valid = 0;
	switch (*fmt) {
	case 'a':
            if (pfx < (len-i)) {
		Tcl_AppendToObj(objPtr, string+i, pfx);
		i += pfx;
	    } else {
		Tcl_AppendToObj(objPtr, string+i, len-i);
		i = len;
	    }
	    break;
	case 'b':
	    for (vv = 0; string[i] == '0' || string[i] == '1'; i++) {
		valid = 1;
		vv = (vv << 1) | (string[i] - '0');
	    }
	    break;
	case 'd':
	    valid = (1 == sscanf(string+i, "%ld", &vv));
	    if (valid) {
		while (isdigit((int) string[i])) i++;
	    }
	    break;
	case 'o':
	    valid = (1 == sscanf(string+i, "%lo", &vv));
	    if (valid) {
		while (string[i] >= '0' && string[i] <= '7') i++;
	    }
	    break;
	case 'x':
	    valid = (1 == sscanf(string+i, "%lx", &vv));
	    if (valid) {
		while (isxdigit((int) string[i])) i++;
	    }
	    break;
	default:
	    Tcl_DecrRefCount(objPtr);
	    return NULL;
	}
	fmt++;

	if (valid) {
	    while (pfx > 0) {
		char c = (char) (vv >> ((pfx - 1) * 8));
		Tcl_AppendToObj(objPtr, &c, 1);
		pfx--;
	    }
	}

	/*
	 * Check for a separator and repeat with the last format
	 * if there is still data available.
	 */
	if (*fmt != '*') {
	  /* skip over format seperator: */
	  if (*fmt && ! isdigit((int) *fmt)) {
	    fmt++;
	  }
	  /* skip over data seperator: */
	  if (i < len && ! isdigit((int) string[i])) {
	    i++;
	  }
	}

	if (! *fmt && (i < len)) {
	    fmt = last_fmt;
	}
    }

    /*
     * Copy the string over into an octet string object.
     */

    if (objPtr) {
	Tcl_Obj *newPtr;
	string = Tcl_GetStringFromObj(objPtr, &len);
	newPtr = TnmNewOctetStringObj(string, len);
	Tcl_DecrRefCount(objPtr);
	objPtr = newPtr;
    }
    
    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * ScanIntTC --
 *
 *	This procedure scans a string using the display hint stored in
 *	fmt and returns the underlying INTEGER value.
 *
 * Results:
 *	A pointer to a Tcl_Obj which contains the integer value
 *	or NULL if there was no need to perform a conversion or the
 *	conversion could not be made due to an error condition.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
ScanIntTC(Tcl_Obj *val, char *fmt)
{
    int i = 0, dpt = -1, sign = 0, frac = -1;
    Tcl_Obj *objPtr = NULL;
    char *string;
    long value;

    /* 
     * Perform some sanity checks and get the string value.
     */

    if (! fmt) {
        return NULL;
    }
    string = Tcl_GetStringFromObj(val, NULL);

    switch (fmt[0]) {
    case 'd':
	if (! fmt[1]) {
	    Tcl_InvalidateStringRep(val);
	    return NULL;
	}
	if (fmt[1] != '-') break;		/* invalid format string */
	if (isdigit((int) fmt[2])) {
	    for (dpt = 0, i = 0; isdigit((int) fmt[2+i]); i++) {
		dpt = dpt * 10 + fmt[2+i] - '0';
	    }
	}
	if (fmt[2+i]) break;			/* invalid format string */
	if (*string == '-') {
	    sign = 1;
	    string++;
	}
	for (value = 0; isdigit((int) *string) || *string == '.'; string++) {
	    if (*string == '.') {
		if (frac >= 0) break;
		frac = 0;
		continue;
	    }
	    value = value * 10 + *string - '0';
	    if (frac >= 0) {
		frac++;
	    }
	}
	if (! *string) {
	    for (; frac < dpt; frac++) value *= 10;
	    for (; frac > dpt; frac--) value /= 10;
	    objPtr = Tcl_NewLongObj(sign ? -1 * value : value);
	}
	break;
    case 'x':
	if (fmt[1]) break;			/* invalid format string */
	if (sscanf(string, "%lx", &value) == 1) {
	    objPtr = Tcl_NewLongObj(value);
	}
	break;
    case 'o':
	if (fmt[1]) break;			/* invalid format string */
	if (sscanf(string, "%lo", &value) == 1) {
	    objPtr = Tcl_NewLongObj(value);
	}
	break;
    case 'b':
	if (fmt[1]) break;			/* invalid format string */
	if (*string == '-') {
	    sign = 1;
	    string++;
	}
	for (value = 0; *string == '0' || *string == '1'; string++) {
	    value = (value << 1) | (*string - '0');
	}
	objPtr = Tcl_NewLongObj(sign ? -1 * value : value);
	break;
    default:
	break;
    }

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibFormatValue --
 *
 *	This procedure converts a value from its base representation
 *	into the format defined by textual conventions or enumerations.
 *
 * Results:
 *	The pointer to a Tcl_Obj containing the formatted value or
 *	NULL if no conversion applies.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
TnmMibFormatValue(TnmMibType *typePtr, int syntax, Tcl_Obj *value)
{
    Tcl_Obj *objPtr = NULL;
    
    if (typePtr) {

	/*
	 * Check if we have an enumeration for this value. Otherwise,
	 * try to apply a display hint, if we have one.
	 */

	if (typePtr->restKind == TNM_MIB_REST_ENUMS) {
	    TnmMibRest *rPtr;
	    long ival;

	    switch (syntax) {
	    case ASN1_INTEGER:
		if (Tcl_GetLongFromObj(NULL, value, &ival) != TCL_OK) {
		    Tcl_Panic("illegal value for integer enumeration");
		}
		for (rPtr = typePtr->restList; rPtr; rPtr = rPtr->nextPtr) {
		    if (rPtr->rest.intEnum.enumValue == ival) {
			objPtr = Tcl_NewStringObj(rPtr->rest.intEnum.enumLabel,-1);
		    }
		}
		break;
	    case ASN1_OCTET_STRING:
		/* xxx need to format bits here */
		break;
	    }
	}
	
	if (typePtr->restKind != TNM_MIB_REST_ENUMS && typePtr->displayHint) {
	    switch (syntax) {
	    case ASN1_OCTET_STRING:
		objPtr = FormatOctetTC(value, typePtr->displayHint);
		break;
	    case ASN1_INTEGER:
		objPtr = FormatIntTC(value, typePtr->displayHint);
		break;
	    }
	}
    }

    /*
     * Finally, apply some special conversions for some well-known
     * SNMP base types.
     */

    if (syntax == ASN1_OBJECT_IDENTIFIER) {
	if (Tcl_ConvertToType(NULL, value, &tnmOidType) == TCL_OK) {
	    objPtr = Tcl_DuplicateObj(value);
	    TnmOidObjSetRep(objPtr, TNM_OID_AS_NAME);
	    Tcl_InvalidateStringRep(objPtr);
	}
    }

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibFormat --
 *
 *	This procedure converts a value from its base representation
 *	into the format defined by textual conventions or enumerations.
 *
 * Results:
 *	A pointer to a Tcl_Obj containing the formatted value or NULL
 *	if there is no such MIB node.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
TnmMibFormat(char *name, int exact, char *value)
{
    TnmMibNode *nodePtr;
    Tcl_Obj *src, *dst;

    nodePtr = TnmMibFindNode(name, NULL, exact);
    if (! nodePtr) {
	return NULL;
    }
   
    if ((nodePtr->macro != TNM_MIB_OBJECTTYPE) &&
	!(nodePtr->macro == TNM_MIB_VALUE_ASSIGNEMENT && !nodePtr->childPtr)) {
	return NULL;
    }
	    
    src = Tcl_NewStringObj(value, -1);
    dst = TnmMibFormatValue(nodePtr->typePtr, (int) nodePtr->syntax, src);
    Tcl_DecrRefCount(src);

    return dst ? dst : Tcl_NewStringObj(value,-1);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibScanValue --
 *
 *	This procedure converts a value into its base representation
 *	by applying textual conventions or by converting enumerations
 *	into simple numbers.
 *
 * Results:
 *	The pointer to a Tcl_Obj containing the scanned value or
 *	NULL if no conversion applies.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
TnmMibScanValue(TnmMibType *typePtr, int syntax, Tcl_Obj *value)
{
    Tcl_Obj *objPtr = NULL;
    
    if (typePtr) {
	
	/*
	 * Check if we have an enumeration for this value. Otherwise,
	 * try to apply a display hint.
	 */
	
	if (typePtr->restKind == TNM_MIB_REST_ENUMS) {
	    TnmMibRest *rPtr;
	    char *s = Tcl_GetStringFromObj(value, NULL);
	    for (rPtr = typePtr->restList; rPtr; rPtr = rPtr->nextPtr) {
		if (strcmp(rPtr->rest.intEnum.enumLabel, s) == 0) {
		    return Tcl_NewIntObj(rPtr->rest.intEnum.enumValue);
		}
	    }
	}
	    
	if (typePtr->displayHint) {
	    switch (syntax) {
	    case ASN1_OCTET_STRING:
		objPtr = ScanOctetTC(value, typePtr->displayHint);
		break;
	    case ASN1_INTEGER:
		objPtr = ScanIntTC(value, typePtr->displayHint);
		break;
	    }
	}
    }

    if (syntax == ASN1_OBJECT_IDENTIFIER) {
	if (Tcl_ConvertToType(NULL, value, &tnmOidType) == TCL_OK) {
	    objPtr = Tcl_DuplicateObj(value);
	    TnmOidObjSetRep(objPtr, TNM_OID_AS_OID);
	    Tcl_InvalidateStringRep(objPtr);
	}
    }

    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibScan --
 *
 *	This procedure converts a value into its base representation
 *	by applying textual conventions or by converting enumerations
 *	into simple numbers.
 *
 * Results:
 *	The pointer to a static string containing the scanned value
 *	or NULL if there is no such MIB node.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char*
TnmMibScan(const char *name, int exact, const char *value)
{
    TnmMibNode *nodePtr = TnmMibFindNode(name, NULL, exact);
    static Tcl_Obj *objPtr = NULL;
    Tcl_Obj *newObjPtr;

    if (!objPtr) {
	objPtr = Tcl_NewStringObj(value, -1);
    }
    
    if (nodePtr) {
	Tcl_SetStringObj(objPtr, value, -1);
	newObjPtr = TnmMibScanValue(nodePtr->typePtr,
				    (int) nodePtr->syntax, objPtr);
	if (newObjPtr) {
	    Tcl_SetStringObj(objPtr, Tcl_GetString(newObjPtr), -1);
	    Tcl_DecrRefCount(newObjPtr);
	    return Tcl_GetString(objPtr);
	}
    }

    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibUnpack --
 *
 *	This procedure unpacks the values encoded in an instance
 *	identifier.
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
TnmMibUnpack(Tcl_Interp *interp, TnmOid *oidPtr, int offset, int implied, TnmMibNode **indexNodeList)
{
    int i, j, syntax, oidLength;
    Tcl_Obj *listObjPtr, *value, *fmtValue;
    struct in_addr ipaddr;

    oidLength = TnmOidGetLength(oidPtr);
    listObjPtr = Tcl_GetObjResult(interp);
	
    for (i = 0; indexNodeList[i]; i++) {
	
	syntax = indexNodeList[i]->typePtr
	    ? indexNodeList[i]->typePtr->syntax : indexNodeList[i]->syntax;
	
	value = NULL;
	
	switch (syntax) {
	case ASN1_INTEGER:
	    if (! offset) break;
	    value = Tcl_NewLongObj((long) TnmOidGet(oidPtr, oidLength-offset));
	    fmtValue = TnmMibFormatValue(indexNodeList[i]->typePtr,
					 (int) indexNodeList[i]->syntax,
					 value);
	    if (fmtValue) {
		Tcl_DecrRefCount(value);
		value = fmtValue;
	    }
	    offset--;
	    break;
	case ASN1_TIMETICKS:
	case ASN1_GAUGE32:
	    if (! offset) break;
	    value = TnmNewUnsigned32Obj(TnmOidGet(oidPtr, oidLength - offset));
	    offset--;
	    break;
	case ASN1_IPADDRESS:
	    if (offset < 4) break;
	    for (j = 0; j < 4; j++) {
		ipaddr.s_addr <<= 8;
		ipaddr.s_addr |= TnmOidGet(oidPtr, oidLength - offset) & 0xff;
		offset--;
	    }
	    ipaddr.s_addr = ntohl(ipaddr.s_addr);
	    value = TnmNewIpAddressObj(&ipaddr);
	    break;
	case ASN1_OBJECT_IDENTIFIER: {
	    int len = -1;
	    TnmOid oid;
	    
	    /*
	     * Set the implied flag if the data type has a fixed size.
	     * This may not work correctly if the type is derived from
	     * another type (which the SMIv2 says is illegal anyway).
	     */
	    
	    if (indexNodeList[i]->typePtr
		&& indexNodeList[i]->typePtr->restKind == TNM_MIB_REST_SIZE) {
		if (indexNodeList[i]->typePtr->restList) {
		    if (indexNodeList[i]->typePtr->restList->rest.unsRange.min
			== indexNodeList[i]->typePtr->restList->rest.unsRange.max
			&& ! indexNodeList[i]->typePtr->restList->nextPtr) {
			implied = 1;
		    }
		}
	    }
	    
	    if (implied && indexNodeList[i+1] == NULL) {
		len = offset;
	    } else if (offset) {
		len = TnmOidGet(oidPtr, oidLength - offset);
		offset--;
	    }
	    if (len < 0 || len > offset || len > TNM_OID_MAX_SIZE) break;
	    
	    TnmOidInit(&oid);
	    for (j = 0; len; len--, offset--, j++) {
		TnmOidAppend(&oid, TnmOidGet(oidPtr, oidLength - offset));
	    }
	    value = TnmNewOidObj(&oid);
	    TnmOidObjSetRep(value, TNM_OID_AS_NAME);
	    break;
	}
	case ASN1_OCTET_STRING: {
	    int len = -1;
	    char bytes[TNM_OID_MAX_SIZE];

	    /*
	     * Set the implied flag if the data type has a fixed size.
	     * This may not work correctly if the type is derived from
	     * another type (which the SMIv2 says is illegal anyway).
	     */
	    
	    if (indexNodeList[i]->typePtr
		&& indexNodeList[i]->typePtr->restKind == TNM_MIB_REST_SIZE) {
		if (indexNodeList[i]->typePtr->restList) {
		    if (indexNodeList[i]->typePtr->restList->rest.unsRange.min
			== indexNodeList[i]->typePtr->restList->rest.unsRange.max
			&& ! indexNodeList[i]->typePtr->restList->nextPtr) {
			implied = 1;
		    }
		}
	    }
	    
	    if (implied && indexNodeList[i+1] == NULL) {
		len = offset;
	    } else if (offset) {
		len = TnmOidGet(oidPtr, oidLength - offset);
		offset--;
	    }
	    if (len < 0 || len > offset || len > TNM_OID_MAX_SIZE) break;
	    
	    for (j = 0; len; len--, offset--, j++) {
		bytes[j] = TnmOidGet(oidPtr, oidLength - offset) & 0xff;
	    }
	    bytes[j] = 0;
	    value = TnmNewOctetStringObj(bytes, j);
	    fmtValue = TnmMibFormatValue(indexNodeList[i]->typePtr,
					 (int) indexNodeList[i]->syntax,
					 value);
	    if (fmtValue) {
		Tcl_DecrRefCount(value);
		value = fmtValue;
	    }
	    break;
	}
	default:
	    Tcl_Panic("can not decode index type");
	    break;
	}
	
	if (value) {
	    Tcl_ListObjAppendElement(interp, listObjPtr, value);
	} else {
	    Tcl_SetResult(interp,
			  "illegal length of the instance identifier",
			  TCL_STATIC);
	    return TCL_ERROR;
	}
    }
    
    if (offset) {
	Tcl_SetResult(interp,
		      "trailing subidentifier in the instance identifier",
		      TCL_STATIC);
	return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibPack --
 *
 *	This procedure packs the values into an instance identifier.
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
TnmMibPack(Tcl_Interp *interp, TnmOid *oidPtr, int objc, Tcl_Obj **objv, int implied, TnmMibNode **indexNodeList)
{
    int i, j, len, syntax, code;
    long int32Value;
    TnmUnsigned32 u32Value;
    struct in_addr* ipValue;
    TnmOid *oidValue;
    char *octetValue;
    Tcl_Obj *newPtr, *valPtr;
    unsigned long addr;
    
    for (i = 0; indexNodeList[i] && i < objc; i++) {
	
	syntax = indexNodeList[i]->typePtr
	    ? indexNodeList[i]->typePtr->syntax : indexNodeList[i]->syntax;

	code = TnmMibGetValue(syntax, objv[i], indexNodeList[i]->typePtr,
			      &newPtr);
	if (code != TCL_OK) {
	    Tcl_AppendResult(interp, "invalid value \"",
			     Tcl_GetStringFromObj(objv[i], NULL),
			     "\" for index element \"",
			     indexNodeList[i]->label, "\"",
			     (char *) NULL);
	    return TCL_ERROR;
	}

	valPtr = newPtr ? newPtr : objv[i];

	switch (syntax) {
	case ASN1_INTEGER:
	    (void) Tcl_GetLongFromObj(interp, valPtr, &int32Value);
	    TnmOidAppend(oidPtr, (unsigned) int32Value);
	    break;
	case ASN1_TIMETICKS:
	case ASN1_GAUGE32:
	    (void) TnmGetUnsigned32FromObj(interp, valPtr, &u32Value);
	    TnmOidAppend(oidPtr, u32Value);
	    break;
	case ASN1_IPADDRESS:
	    ipValue = TnmGetIpAddressFromObj(interp, valPtr);
	    addr = htonl(ipValue->s_addr);
	    TnmOidAppend(oidPtr, addr >> 24 & 0xff);
	    TnmOidAppend(oidPtr, addr >> 16 & 0xff);
	    TnmOidAppend(oidPtr, addr >> 8 & 0xff);
	    TnmOidAppend(oidPtr, addr & 0xff);
	    break;
	case ASN1_OBJECT_IDENTIFIER:
	    oidValue = TnmGetOidFromObj(interp, valPtr);
	    len = TnmOidGetLength(oidValue);

	    /*
	     * Set the implied flag if the data type has a fixed size.
	     * This may not work correctly if the type is derived from
	     * another type (which the SMIv2 says is illegal anyway).
	     */
	    
	    if (indexNodeList[i]->typePtr
		&& indexNodeList[i]->typePtr->restKind == TNM_MIB_REST_SIZE) {
		if (indexNodeList[i]->typePtr->restList) {
		    if (indexNodeList[i]->typePtr->restList->rest.unsRange.min
			== indexNodeList[i]->typePtr->restList->rest.unsRange.max
			&& ! indexNodeList[i]->typePtr->restList->nextPtr) {
			implied = 1;
		    }
		}
	    }
	    

	    if (! implied || indexNodeList[i+1] != NULL) {
		TnmOidAppend(oidPtr, (unsigned) len);
	    }
	    for (j = 0; j < len; j++) {
		TnmOidAppend(oidPtr, TnmOidGet(oidValue, j));
	    }
	    break;
	case ASN1_OCTET_STRING:
	    octetValue = TnmGetOctetStringFromObj(interp, valPtr, &len);

	    /*
	     * Set the implied flag if the data type has a fixed size.
	     * This may not work correctly if the type is derived from
	     * another type (which the SMIv2 says is illegal anyway).
	     */
	    
	    if (indexNodeList[i]->typePtr
		&& indexNodeList[i]->typePtr->restKind == TNM_MIB_REST_SIZE) {
		if (indexNodeList[i]->typePtr->restList) {
		    if (indexNodeList[i]->typePtr->restList->rest.unsRange.min
			== indexNodeList[i]->typePtr->restList->rest.unsRange.max
			&& ! indexNodeList[i]->typePtr->restList->nextPtr) {
			implied = 1;
		    }
		}
	    }
	    

	    if (! implied || indexNodeList[i+1] != NULL) {
		TnmOidAppend(oidPtr, (unsigned) len);
	    }
	    for (j = 0; j < len; j++) {
		TnmOidAppend(oidPtr, (unsigned) (octetValue[j] & 0xff));
	    }
	    break;	    
	default:
	    /* Counter32 and Counter64 do not make any sense */
	    Tcl_Panic("can not encode index type");
	    break;
	}

	if (newPtr) {
	    Tcl_DecrRefCount(newPtr);
	}
    }

    if (indexNodeList[i] || i < objc) {
	Tcl_AppendResult(interp, "number of arguments does not match",
			 " the number of index components", (char *) NULL);
	return TCL_ERROR;
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmMibGetValue --
 *
 *	This procedure extracts the value for an SNMP variable by
 *	converting the objPtr to the underlying type. The type is
 *	identified by syntax or optionally by typePtr. A new Tcl_Obj
 *	might be created during the conversion.
 *
 * Results:
 *	A standard Tcl result. A new Tcl_Obj might be returned in
 *	newPtr if newPtr is not NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmMibGetValue(int syntax, Tcl_Obj *objPtr, TnmMibType *typePtr, Tcl_Obj **newPtr)
{
    int result = TCL_OK;

    if (newPtr) {
	*newPtr = NULL;
    }
    
    switch (syntax) {
    case ASN1_INTEGER:
	result = Tcl_ConvertToType((Tcl_Interp *) NULL, objPtr,
				   Tcl_GetObjType("int"));
	if (result != TCL_OK && typePtr && newPtr) {
	    *newPtr = TnmMibScanValue(typePtr, syntax, objPtr);
	    if (newPtr) {
		result = Tcl_ConvertToType((Tcl_Interp *) NULL, *newPtr,
					   Tcl_GetObjType("int"));
		if (result != TCL_OK) {
		    Tcl_DecrRefCount(*newPtr);
		    *newPtr = NULL;
		}
	    }
	}
	break;
    case ASN1_COUNTER32:
    case ASN1_TIMETICKS:
    case ASN1_GAUGE32:
	result = Tcl_ConvertToType((Tcl_Interp *) NULL, objPtr,
				   &tnmUnsigned32Type);
	break;
    case ASN1_COUNTER64:
	result = Tcl_ConvertToType((Tcl_Interp *) NULL, objPtr,
				   &tnmUnsigned64Type);
    case ASN1_IPADDRESS:
	result = Tcl_ConvertToType((Tcl_Interp *) NULL, objPtr,
				   &tnmIpAddressType);
	break;
    case ASN1_OBJECT_IDENTIFIER:
	result = Tcl_ConvertToType((Tcl_Interp *) NULL, objPtr,
				   &tnmOidType);
	break;
    case ASN1_OCTET_STRING:
	result = Tcl_ConvertToType((Tcl_Interp *) NULL, objPtr,
				   &tnmOctetStringType);
	if (result != TCL_OK && typePtr && newPtr) {
	    *newPtr = TnmMibScanValue(typePtr, syntax, objPtr);
	    if (*newPtr) {
		result = Tcl_ConvertToType((Tcl_Interp *) NULL, *newPtr,
					   &tnmOctetStringType);
		if (result != TCL_OK) {
		    Tcl_DecrRefCount(*newPtr);
		    *newPtr = NULL;
		}
	    }
	}
	break;	    
    default:
	Tcl_Panic("can not encode index type");
	break;
    }

    return result;
}

/*
 * Local Variables:
 * compile-command: "make -k -C ../../unix"
 * End:
 */

