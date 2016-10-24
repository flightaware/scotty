/*
 * tkiMisc.c --
 *
 *	Some utility functions that are used in other places.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tkined.h"
#include "tkiPort.h"

/*
 * A general purpose sprintf buffer. See tkined.h for its intended use.
 */

char *buffer;

/*
 * Make sure our general purpose sprintf buffer is large enough
 * to hold at least size bytes. It will never be smaller than
 * 1024 bytes.
 */

void
buffersize(size_t size)
{
    static size_t buffer_size = 0;

    if (size < 1024) size = 1024;

    if (buffer_size == 0) {
	buffer = ckalloc(size);
    } else {
	if (size > buffer_size) {
	    buffer = ckrealloc (buffer, size);
	}
    }

    buffer_size = size;
}


/*
 *----------------------------------------------------------------------
 *
 * FixPath --
 *
 *	This procedure converts a Windows path into the Tcl 
 *	representation by converting "\\" to "/".
 *
 * Results:
 *	A pointer to the fixed path.
 *
 * Side effects:
 *	The path is modified.
 *
 *----------------------------------------------------------------------
 */

static char *
FixPath(char *path)
{
    char *s;

    for (s = path; *s; s++) {
	if (*s == '\\') {
	    *s = '/';
	}
    }
    return path;
}

/* 
 * Find a file searching in some likely places. Return the
 * absolute platform specific filename or NULL if not found.
 */

char* 
findfile (Tcl_Interp *interp, char *name)
{
    int i;
    const char *library;
    char *file;
    static Tcl_DString *dsPtr = NULL;
    static char *dirs[] = { "/bitmaps/", "/site/", "/apps/", "/", NULL };

    if (! dsPtr) {
	dsPtr = (Tcl_DString *) ckalloc(sizeof(Tcl_DString));
	Tcl_DStringInit(dsPtr);
    }

    file = Tcl_TranslateFileName(interp, name, dsPtr);
    if (file && access(file, R_OK) == 0) {
	Tcl_ResetResult(interp);
	return FixPath(file);
    }

    buffersize(strlen(name)+20);
    strcpy(buffer, "~/.tkined/");
    strcat(buffer, name);
    file = Tcl_TranslateFileName(interp, buffer, dsPtr);
    if (file && access(file, R_OK) == 0) {
	Tcl_ResetResult(interp);
	return FixPath(file);
    }

    library = Tcl_GetVar2(interp, "tkined", "library", TCL_GLOBAL_ONLY);
    if (! library) {
	Tcl_ResetResult(interp);
	return (char *) NULL;
    }
    
    buffersize(strlen(library)+strlen(name)+20);
    for (i = 0; dirs[i]; i++) {
	strcpy(buffer, library);
	strcat(buffer, dirs[i]);
	strcat(buffer, name);
	file = Tcl_TranslateFileName(interp, buffer, dsPtr);
	if (file && access(file, R_OK) == 0) {
	    Tcl_ResetResult(interp);
	    return FixPath(file);
	}
    }

    return (char *) NULL;
}

/*
 * Convert a type id to the appropriate string.
 */

char* 
type_to_string (unsigned type)
{
    char *result;

    switch (type) {
        case TKINED_NODE:        result = "NODE"; break;
	case TKINED_GROUP:       result = "GROUP"; break;
	case TKINED_NETWORK:     result = "NETWORK"; break;
	case TKINED_LINK:        result = "LINK"; break;
	case TKINED_TEXT:        result = "TEXT"; break;
	case TKINED_IMAGE:       result = "IMAGE"; break;
	case TKINED_INTERPRETER: result = "INTERPRETER"; break;
	case TKINED_MENU:        result = "MENU"; break;
	case TKINED_LOG:         result = "LOG"; break;
	case TKINED_REFERENCE:   result = "REFERENCE"; break;
	case TKINED_STRIPCHART:  result = "STRIPCHART"; break;
	case TKINED_BARCHART:    result = "BARCHART"; break;
	case TKINED_GRAPH:	 result = "GRAPH"; break;
	case TKINED_HTML:	 result = "HTML"; break;
	case TKINED_DATA:	 result = "DATA"; break;
	case TKINED_EVENT:	 result = "EVENT"; break;
	default:                 result = "";
    }

    return result;
}

unsigned 
string_to_type (const char *str)
{
    unsigned type = TKINED_NONE;

    if (str != NULL) {
	if      ((*str == 'N') && (strcmp(str, "NODE") == 0))
		type = TKINED_NODE;
	else if ((*str == 'G') && (strcmp(str, "GROUP") == 0))
		type = TKINED_GROUP;
	else if ((*str == 'N') && (strcmp(str, "NETWORK") == 0))
		type = TKINED_NETWORK;
	else if ((*str == 'L') && (strcmp(str, "LINK") == 0))
		type = TKINED_LINK;
	else if ((*str == 'T') && (strcmp(str, "TEXT") == 0))
		type = TKINED_TEXT;
	else if ((*str == 'I') && (strcmp(str, "IMAGE") == 0))
		type = TKINED_IMAGE;
	else if ((*str == 'I') && (strcmp(str, "INTERPRETER") == 0))
		type = TKINED_INTERPRETER;
	else if ((*str == 'M') && (strcmp(str, "MENU") == 0))
		type = TKINED_MENU;
	else if ((*str == 'L') && (strcmp(str, "LOG") == 0))
		type = TKINED_LOG;
	else if ((*str == 'R') && (strcmp(str, "REFERENCE") == 0))
		type = TKINED_REFERENCE;
	else if ((*str == 'S') && (strcmp(str, "STRIPCHART") == 0))
		type = TKINED_STRIPCHART;
	else if ((*str == 'B') && (strcmp(str, "BARCHART") == 0))
		type = TKINED_BARCHART;
#ifdef HAVE_BLT
	else if ((*str == 'G') && (strcmp(str, "GRAPH") == 0))
		type = TKINED_GRAPH;
#else
	else if ((*str == 'G') && (strcmp(str, "GRAPH") == 0))
		type = TKINED_STRIPCHART;
#endif
	else if ((*str == 'H') && (strcmp(str, "HTML") == 0))
		type = TKINED_HTML;
	else if ((*str == 'D') && (strcmp(str, "DATA") == 0))
		type = TKINED_DATA;
	else if ((*str == 'E') && (strcmp(str, "EVENT") == 0))
		type = TKINED_EVENT;
   }

    return type;
}

/*
 * Delete the item from the list stored in slist.
 */

void
ldelete (Tcl_Interp *interp, char *slist, char *item)
{
    int largc, i;
    const char **largv;

    if (item == NULL) return;

    if (Tcl_SplitList (interp, slist, &largc, &largv) != TCL_OK) {
	Tcl_ResetResult (interp);
	return;
    }

    *slist = 0;
    for (i = 0; i < largc; i++) {
        if ((item[0] != largv[i][0]) || (strcmp (item, largv[i]) != 0)) {
	    strcat (slist, largv[i]);
	    strcat (slist, " ");
        }
    }
    ckfree ((char*) largv);

    i = strlen (slist) - 1;
    if (slist[i] == ' ') slist[i] = '\0';
}

/*
 * Append an item to the list stored in slist.
 */

void
lappend (char **slist, char *item)
{
    *slist = ckrealloc (*slist, strlen (*slist) + strlen(item) + 2);
    if (**slist != '\0') strcat (*slist, " ");
    strcat (*slist, item);
}


/*
 * Return a copy of the input string with all newlines replaced by
 * "\n". The result string is malloced and must be freed by the caller.
 */

char*
ckstrdupnn (char *s)
{
    char *p, *t, *r;
    size_t n = 2;
    
    for (p = s; *p != 0; p++) {
	if (*p == '\n') n++;
    }

    n += (p - s);
    r = ckalloc (n);

    for (t = r, p = s; *p != 0; p++, t++) {
	if (*p == '\n') {
	    *t++ = '\\'; 
	    *t = 'n';
	} else {
	    *t = *p;
	}
    }
    *t = 0;
    
    return r;
}

