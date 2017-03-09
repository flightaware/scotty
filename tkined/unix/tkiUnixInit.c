/*
 * tkiUnixInit.c --
 *
 *	This file contains the UNIX specific entry point.
 *
 * Copyright (c) 1996      Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "tkined.h"
#include "tkiPort.h"

/*
 * Forward declarations for procedures defined later in this file:
 */

static char*
FindPath		(Tcl_Interp *interp, char *path,
				     char *name, char* version);
EXTERN int
Tkined_Init		(Tcl_Interp *interp);


/*
 *----------------------------------------------------------------------
 *
 * FindPath --
 *
 *	This procedure searches for the path where an extension
 *	is installed.
 *
 * Results:
 *	A pointer to a string containing the "best guess".
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char*
FindPath(Tcl_Interp *interp, char *path, char *name, char *version)
{
    const char *pkgPath;
    int code, largc, i;
    const char **largv;
    Tcl_DString ds;
    
    if (access(path, R_OK | X_OK) == 0) {
	return path;
    }

    /*
     * Try to locate the installation directory by reading
     * the tcl_pkgPath variable.
     */
    
    pkgPath = Tcl_GetVar(interp, "tcl_pkgPath", TCL_GLOBAL_ONLY);
    if (! pkgPath) {
	return path;
    }

    code = Tcl_SplitList(interp, pkgPath, &largc, &largv);
    if (code != TCL_OK) {
	return path;
    }

    Tcl_DStringInit(&ds);
    for (i = 0; i < largc; i++) {
	Tcl_DStringAppend(&ds, largv[i], -1);
	Tcl_DStringAppend(&ds, "/", 1);
	Tcl_DStringAppend(&ds, name, -1);
	Tcl_DStringAppend(&ds, version, -1);
	if (access(Tcl_DStringValue(&ds), R_OK | X_OK) == 0) {
	    path = ckstrdup(Tcl_DStringValue(&ds));
	    Tcl_DStringFree(&ds);
	    break;
	}
	Tcl_DStringFree(&ds);
    }
    ckfree((char *) largv);

    return path;
}

/*
 *----------------------------------------------------------------------
 *
 * TkiInitPath --
 *
 *	This procedure is called to determine the installation path
 *	of the Tkined editor on this system.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Initializes the global Tcl variable tkined(library).
 *
 *----------------------------------------------------------------------
 */

void
TkiInitPath(Tcl_Interp *interp)
{
    char *path;

    path = getenv("TKINED_LIBRARY");
    if (! path) {
	path = FindPath(interp, TKINEDLIB, "tkined", TKI_VERSION);
    }
    Tcl_SetVar2(interp, "tkined", "library", path, TCL_GLOBAL_ONLY);
}

/*
 *----------------------------------------------------------------------
 *
 * Tkined_Init --
 *
 *	This procedure is the UNIX entry point for trusted Tcl
 *	interpreters.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Tcl variables are created.
 *
 *----------------------------------------------------------------------
 */

int
Tkined_Init(Tcl_Interp *interp)
{
    return TkiInit(interp);
}


