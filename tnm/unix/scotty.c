/*
 * scotty.c --
 *
 *	This is scotty, a simple event-driven Tcl interpreter with 
 *	some special commands to get network management information
 *	about TCP/IP networks. 
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2001 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

/*
 * leg20170227: redone completely using current facilities available
 * in Tcl.
 */

#include <tcl.h>
#include "tnm.h"

/*
 *----------------------------------------------------------------------
 *
 * EventLoop --
 *
 *	This procedure is the main event loop. Tcl_DoOneEvent()
 *      favours timer events over file events. This can create
 *      problems if e.g. async. SNMP requests become timed out before
 *      the answer is processed. We therefore check for a pending file
 *      event inside of the while loop.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Time passes and events are processed.
 *
 *----------------------------------------------------------------------
 */

static void
EventLoop(void)
{
    while (Tcl_DoOneEvent(TCL_ALL_EVENTS)) {
        Tcl_DoOneEvent(TCL_DONT_WAIT|TCL_FILE_EVENTS);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppInit(Tcl_Interp *interp)
{
    if ((Tcl_Init)(interp) == TCL_ERROR) {
        return TCL_ERROR;
    }
    
    if (Tcl_InitStubs(interp, "8.1", 0) == NULL) {
	return TCL_ERROR;
    }
    if (Tcl_PkgRequire(interp, "Tcl", "8.1", 0) == NULL) {
	return TCL_ERROR;
    }
    
    if (Tcl_PkgRequire(interp, "Tnm", TNM_VERSION, 1) == NULL) {
	if (Tcl_StringMatch(Tcl_GetStringResult(interp), "*can't find package*")) {
	    Tcl_AppendResult(interp, "\n",
   "This usually means that you have to define the TCLLIBPATH environment\n",
   "variable to point to the tnm library directory or you have to include\n",
   "the path to the tnm library directory in Tcl's auto_path variable.",
			     (char *) NULL);
	}
        return TCL_ERROR;
    }

#ifdef DJGPP
    Tcl_SetVar(interp, "tcl_rcFileName", "~/tclsh.rc", TCL_GLOBAL_ONLY);
#else
    Tcl_SetVar(interp, "tcl_rcFileName", "~/.tclshrc", TCL_GLOBAL_ONLY);
#endif
    return TCL_OK;

    Tcl_SetMainLoop(EventLoop);
}

int
main(argc, argv)
    int argc;			/* Number of command-line arguments. */
    char **argv;		/* Values of command-line arguments. */
{
    /*
     * The following #if block allows you to change the AppInit
     * function by using a #define of TCL_LOCAL_APPINIT instead
     * of rewriting this entire file.  The #if checks for that
     * #define and uses Tcl_AppInit if it doesn't exist.
     */

#ifndef TCL_LOCAL_APPINIT
#define TCL_LOCAL_APPINIT Tcl_AppInit    
#endif
    extern int TCL_LOCAL_APPINIT (Tcl_Interp *interp);

    /*
     * The following #if block allows you to change how Tcl finds the startup
     * script, prime the library or encoding paths, fiddle with the argv,
     * etc., without needing to rewrite Tcl_Main()
     */

#ifdef TCL_LOCAL_MAIN_HOOK
    extern int TCL_LOCAL_MAIN_HOOK (int *argc, char ***argv);
#endif

#ifdef TCL_LOCAL_MAIN_HOOK
    TCL_LOCAL_MAIN_HOOK(&argc, &argv);
#endif

    Tcl_Main(argc, argv, TCL_LOCAL_APPINIT);

    return 0;			/* Needed only to prevent compiler warning. */
}
