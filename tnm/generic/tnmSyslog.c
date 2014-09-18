/*
 * tnmSyslog.c --
 *
 *	A command to access the system logging facility. This
 *	implementation is supposed to be thread-safe and more
 *	or less follows the model in RFC 3164 although the
 *	implementation maps this to the local logging facility
 *	(which might not be based on the syslog protocol on some
 *	platforms).
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1999 Technical University of Braunschweig.
 * Copyright (c) 2003	   International University Bremen.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmInt.h"
#include "tnmPort.h"

static TnmTable tnmLogTable[] = {
    { TNM_LOG_EMERG,	"emergency" },
    { TNM_LOG_ALERT,	"alert" },
    { TNM_LOG_CRIT,	"critical" },
    { TNM_LOG_ERR,	"error" },
    { TNM_LOG_WARNING,	"warning" },
    { TNM_LOG_NOTICE,	"notice" },
    { TNM_LOG_INFO,	"info" },
    { TNM_LOG_DEBUG,	"debug" },
    { 0, NULL }
};

static TnmTable tnmFacilityTable[] = {
    { TNM_LOG_KERN,	"kernel" },
    { TNM_LOG_USER,	"user" },
    { TNM_LOG_MAIL,	"mail" },
    { TNM_LOG_DAEMON,	"daemon" },
    { TNM_LOG_AUTH,	"auth" },
    { TNM_LOG_SYSLOG,	"syslog" },
    { TNM_LOG_LPR,	"lpr" },
    { TNM_LOG_NEWS,	"news" },
    { TNM_LOG_UUCP,	"uucp" },
    { TNM_LOG_CRON,	"cron" },
    { TNM_LOG_AUTHPRIV,	"authpriv" },
    { TNM_LOG_FTP,	"ftp" },
    { TNM_LOG_NTP,	"ntp" },
    { TNM_LOG_AUDITPRIV,"audit" },
    { TNM_LOG_ALERTPRIV,"alert" },
    { TNM_LOG_CRONPRIV,	"cronpriv" },
    
    { TNM_LOG_LOCAL0,	"local0" },
    { TNM_LOG_LOCAL1,	"local1" },
    { TNM_LOG_LOCAL2,	"local2" },
    { TNM_LOG_LOCAL3,	"local3" },
    { TNM_LOG_LOCAL4,	"local4" },
    { TNM_LOG_LOCAL5,	"local5" },
    { TNM_LOG_LOCAL6,	"local6" },
    { TNM_LOG_LOCAL7,	"local7" },
    { 0, NULL }
};

/*
 * Every Tcl interpreter has an associated DnsControl record. It
 * keeps track of the default settings for this interpreter.
 */

static char tnmSyslogControl[] = "tnmSyslogControl";

typedef struct SyslogControl {
    char *ident;		/* Identification of the event source. */
    int facility;		/* Loging facility. */
} SyslogControl;

/*
 * The options for the syslog command.
 */

enum options { optIdent, optFacility };

static TnmTable syslogOptionTable[] = {
    { optIdent,		"-ident" },
    { optFacility,	"-facility" },
    { 0, NULL }
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
AssocDeleteProc	(ClientData clientData, Tcl_Interp *interp);


/*
 *----------------------------------------------------------------------
 *
 * AssocDeleteProc --
 *
 *	This procedure is called when a Tcl interpreter gets destroyed
 *	so that we can clean up the data associated with this interpreter.
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
AssocDeleteProc(clientData, interp)
    ClientData clientData;
    Tcl_Interp *interp;
{
    SyslogControl *control = (SyslogControl *) clientData;

    if (control) {
	if (control->ident) {
	    ckfree(control->ident);
	}
	ckfree((char *) control);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_SyslogObjCmd --
 *
 *	This procedure is invoked to process the "syslog" command.
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
Tnm_SyslogObjCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST objv[];
{
    int x, level, code;
    char *ident = NULL;
    int facility = -1;

    SyslogControl *control = (SyslogControl *)
	Tcl_GetAssocData(interp, tnmSyslogControl, NULL);

    if (! control) {
	control = (SyslogControl *) ckalloc(sizeof(SyslogControl));
	control->ident = ckstrdup("scotty");
	control->facility = TNM_LOG_LOCAL0;
	Tcl_SetAssocData(interp, tnmSyslogControl, AssocDeleteProc, 
			 (ClientData) control);
    }

    if (objc < 2) {
    wrongArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "?-ident string? level message");
	return TCL_ERROR;
    }

    for (x = 1; x < objc; x++) {
	code = TnmGetTableKeyFromObj(interp, syslogOptionTable,
				     objv[x], "option");
	if (code == -1) {
	    char *option = Tcl_GetStringFromObj(objv[x], NULL);
	    if (*option == '-') {
		return TCL_ERROR;
	    } else {
		Tcl_ResetResult(interp);
		break;
	    }
	}
	switch ((enum options) code) {
	case optIdent:
	    if (x == objc-1) {
		Tcl_SetResult(interp, control->ident, TCL_STATIC);
		return TCL_OK;
	    }
	    ident = Tcl_GetStringFromObj(objv[++x], NULL);
	    break;
	case optFacility:
	    if (x == objc-1) {
	        Tcl_SetResult(interp,
			      TnmGetTableValue(tnmFacilityTable,
					       control->facility), NULL);
		return TCL_OK;
	    }
	    code = TnmGetTableKeyFromObj(interp, tnmFacilityTable,
					 objv[++x], NULL);
	    if (code == -1) {
		return TCL_ERROR;
	    }
	    facility = code;
	    break;
	}
    }

    if (x == objc) {
	if (ident) {
	    if (control->ident) {
		ckfree(control->ident);
	    }
	    control->ident = ckstrdup(ident);
	}
	if (facility > -1) {
	    control->facility = facility;
	}
	return TCL_OK;
    }

    if (x != objc-2) {
	goto wrongArgs;
    }

    if (! ident) {
	ident = control->ident;
    }

    if (facility == -1) {
	facility = control->facility;
    }

    level = TnmGetTableKeyFromObj(interp, tnmLogTable, objv[x], "level");
    if (level < 0) {
	return TCL_ERROR;
    }

    code = TnmWriteLogMessage(ident, level, facility,
			      Tcl_GetStringFromObj(objv[++x], NULL));
    if (code != 0) {
	Tcl_SetResult(interp, "error while accessing system logging facility",
		      TCL_STATIC);
	return TCL_ERROR;
    }

    return TCL_OK;
}
