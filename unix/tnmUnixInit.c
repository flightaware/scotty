/*
 * tnmUnixInit.c --
 *
 *	This file contains the UNIX specific entry point.
 *
 * Copyright (c) 1996      Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmInt.h"
#include "tnmPort.h"

#define BIND_8_COMPAT

#include <arpa/nameser.h>
#include <resolv.h>

/*
 * Forward declarations for procedures defined later in this file:
 */

static char*
FindPath		_ANSI_ARGS_((Tcl_Interp *interp, char *path,
				     char *name, char* version));
static void
FindProc		_ANSI_ARGS_((Tcl_Interp *interp, char *name,
				     char *version));
EXTERN int
Tnm_Init		_ANSI_ARGS_((Tcl_Interp *interp));

EXTERN int
Tnm_SafeInit		_ANSI_ARGS_((Tcl_Interp *interp));


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
FindPath(interp, path, name, version)
    Tcl_Interp *interp;
    char *path;
    char *name;
    char *version;
{
    char *pkgPath;
    int code, largc, i;
    char **largv;
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
 * FindProc --
 *
 *	This procedure searches for an executable with a given
 *	name and version. It sets an entry in the global tnm
 *	array which contains the full path to the executable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A global Tcl variable is created.
 *
 *----------------------------------------------------------------------
 */

static void
FindProc(interp, name, version)
    Tcl_Interp *interp;
    char *name;
    char *version;
{
    char *token, *path;
    Tcl_DString ds;
    int found = 0;

    path = getenv("PATH");
    if (! path) {
	return;
    }

    path = ckstrdup(path);
    Tcl_DStringInit(&ds);

    token = strtok(path, ":");
    while (token) {
	Tcl_DStringAppend(&ds, token, -1);
	Tcl_DStringAppend(&ds, "/", -1);
	Tcl_DStringAppend(&ds, name, -1);
	Tcl_DStringAppend(&ds, version, -1);
	if (access(Tcl_DStringValue(&ds), R_OK | X_OK) == 0) {
	    found = 1;
	    break;
	}
	Tcl_DStringFree(&ds);
	token = strtok(NULL, ":");
    }

    if (found) {
	Tcl_SetVar2(interp, "tnm", name, 
		    Tcl_DStringValue(&ds), TCL_GLOBAL_ONLY);
    }

    Tcl_DStringFree(&ds);
    ckfree(path);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmInitPath --
 *
 *	This procedure is called to determine the installation path
 *	of the Tnm extension on this system.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Initializes the global Tcl variable tnm(library).
 *
 *----------------------------------------------------------------------
 */

void
TnmInitPath(interp)
    Tcl_Interp *interp;
{
    char *path, *version;

    path = getenv("TNM_LIBRARY");
    if (! path) {
	path = FindPath(interp, TNMLIB, "tnm", TNM_VERSION);
    }
    Tcl_SetVar2(interp, "tnm", "library", path, TCL_GLOBAL_ONLY);

    /*
     * Also initialize the tkined(library) variable which is used
     * by tnmIned.c to locate Tkined specific files. This should
     * be removed once Tkined gets integrated into Tnm.
     */

    path = getenv("TKINED_LIBRARY");
    if (! path) {
	path = FindPath(interp, TKINEDLIB, "tkined", TKI_VERSION);
    }
    Tcl_SetVar2(interp, "tkined", "library", path, TCL_GLOBAL_ONLY);

    /*
     * Locate tclsh and wish so that we can start additional
     * scripts with the correct path name.
     */

    version = Tcl_GetVar(interp, "tcl_version", TCL_GLOBAL_ONLY);
    if (version) {
	FindProc(interp, "tclsh", version);
    }
    version = Tcl_GetVar(interp, "tk_version", TCL_GLOBAL_ONLY);
    if (version) {
	FindProc(interp, "wish", version);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmInitDns --
 *
 *	This procedure is called to initialize the DNS resolver and to
 *	save the domain name in the global tnm(domain) Tcl variable.
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
TnmInitDns(interp)
    Tcl_Interp *interp;
{
    char domain[MAXDNAME], *p;

    res_init();
    _res.options |= RES_RECURSE | RES_DNSRCH | RES_DEFNAMES | RES_AAONLY;

    /*
     * Remove any trailing dots or white spaces and save the
     * result in tnm(domain).
     */

    strcpy(domain, _res.defdname);
    p = domain + strlen(domain) - 1;
    while ((*p == '.' || isspace(*p)) && p > domain) {
	*p-- = '\0';
    }
    Tcl_SetVar2(interp, "tnm", "domain", domain, TCL_GLOBAL_ONLY);
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_Init --
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
Tnm_Init(interp)
    Tcl_Interp *interp;
{
    int code;

    code = TnmInit(interp, 0);
    if (code != TCL_OK) {
	return code;
    }

    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_SafeInit --
 *
 *	This procedure is the UNIX entry point for safe Tcl
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
Tnm_SafeInit(interp)
    Tcl_Interp *interp;
{
    return TnmInit(interp, 1);
}
