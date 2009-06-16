/*
 * tnmIcmp.c --
 *
 *	Extend a Tcl interpreter with an icmp command. This
 *	module depends only the platform independent part.
 *	This implementation is supposed to be thread-safe.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1999 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmInt.h"
#include "tnmPort.h"

/*
 * Every Tcl interpreter has an associated IcmpControl record. It
 * keeps track of the default settings for this interpreter.
 */

static char tnmIcmpControl[] = "tnmIcmpControl";

typedef struct IcmpControl {
    int retries;		/* Default number of retries. */
    int timeout;		/* Default timeout in seconds. */
    int size;			/* Default size of the ICMP packet. */
    int delay;			/* Default delay between ICMP packets. */
    int window;			/* Default window of active ICMP packets. */
} IcmpControl;

/*
 * The options for the icmp command.
 */

enum options {
    optDelay, optRetries, optSize, optTimeout, optWindow
};

static TnmTable icmpOptionTable[] = {
    { optDelay,		"-delay" },
    { optRetries,	"-retries" },
    { optSize,		"-size" },
    { optTimeout,	"-timeout" },
    { optWindow,	"-window" },
    { 0, NULL }
};

/*
 * Mutex used to serialize access to static variables in this module.
 */

TCL_DECLARE_MUTEX(icmpMutex)

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
AssocDeleteProc	_ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp));

static int
IcmpRequest	_ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *hosts, 
			     TnmIcmpRequest *icmpPtr));

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
    IcmpControl *control = (IcmpControl *) clientData;

    if (control) {
	ckfree((char *) control);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * IcmpRequest --
 *
 *	This procedure is called to process a single ICMP request.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
IcmpRequest(interp, hosts, icmpPtr)
    Tcl_Interp *interp;
    Tcl_Obj *hosts;
    TnmIcmpRequest *icmpPtr;
{
    int i, code, objc;
    struct sockaddr_in addr;
    static unsigned int lastTid = 1;
    Tcl_Obj *listPtr, **objv;
    
    code = Tcl_ListObjGetElements(interp, hosts, &objc, &objv);
    if (code != TCL_OK) {
	return TCL_ERROR;
    }

    icmpPtr->numTargets = objc;
    icmpPtr->targets = (TnmIcmpTarget *) ckalloc(objc*sizeof(TnmIcmpTarget));
    memset((char *) icmpPtr->targets, 0, objc * sizeof(TnmIcmpTarget));

    for (i = 0; i < icmpPtr->numTargets; i++) {
	TnmIcmpTarget *targetPtr = &(icmpPtr->targets[i]);
	code = TnmSetIPAddress(interp, 
			       Tcl_GetStringFromObj(objv[i], NULL), &addr);
	if (code != TCL_OK) {
	    ckfree((char *) icmpPtr->targets);
	    return TCL_ERROR;
	}
	Tcl_MutexLock(&icmpMutex);
	targetPtr->tid = lastTid++;
	Tcl_MutexUnlock(&icmpMutex);
	targetPtr->dst = addr.sin_addr;
	targetPtr->res = addr.sin_addr;
	targetPtr->res.s_addr = 0;
    }

    code = TnmIcmp(interp, icmpPtr);
    if (code != TCL_OK) {
	ckfree((char *) icmpPtr->targets);
	return TCL_ERROR;
    }

    Tcl_ResetResult(interp);
    listPtr = Tcl_GetObjResult(interp);
    Tcl_SetStringObj(listPtr, NULL, 0);

    for (i = 0; i < icmpPtr->numTargets; i++) {
	TnmIcmpTarget *targetPtr = &(icmpPtr->targets[i]);
	switch (icmpPtr->type) {
	case TNM_ICMP_TYPE_ECHO:
	case TNM_ICMP_TYPE_MASK:
	case TNM_ICMP_TYPE_TIMESTAMP:
	    Tcl_ListObjAppendElement(interp, listPtr, objv[i]);
	    break;
	case TNM_ICMP_TYPE_TRACE:
	    if (icmpPtr->flags & TNM_ICMP_FLAG_LASTHOP 
		&& targetPtr->flags & TNM_ICMP_FLAG_LASTHOP) {
		Tcl_ListObjAppendElement(interp, listPtr,
			 Tcl_NewStringObj(inet_ntoa(targetPtr->dst), -1));
	    } else {
		Tcl_ListObjAppendElement(interp, listPtr,
			 Tcl_NewStringObj(inet_ntoa(targetPtr->res), -1));
	    }
	    break;
	}
	if (targetPtr->status == TNM_ICMP_STATUS_NOERROR) {
	    switch (icmpPtr->type) {
	    case TNM_ICMP_TYPE_ECHO:
	    case TNM_ICMP_TYPE_TRACE:
	    case TNM_ICMP_TYPE_TIMESTAMP:
#if 0 /* return ms as float instead of int for usec resolution */
	        /* This is to be discussed: if we get ping-times below
		   1 ms reported as 0 ms, we silently adjust this. */
		Tcl_ListObjAppendElement(interp, listPtr, 
			 Tcl_NewLongObj(targetPtr->u.rtt 
					? (long) targetPtr->u.rtt : 1));
#else
		Tcl_ListObjAppendElement(interp, listPtr, 
			 Tcl_NewDoubleObj((double)(targetPtr->u.rtt / 1000.0)));
#endif
		break;
	    case TNM_ICMP_TYPE_MASK: {
		struct in_addr ipaddr;
		ipaddr.s_addr = htonl(targetPtr->u.mask);
		Tcl_ListObjAppendElement(interp, listPtr,
				 Tcl_NewStringObj(inet_ntoa(ipaddr), -1));
		break;
		}
	    }
	} else {
	    Tcl_ListObjAppendElement(interp, listPtr,
				     Tcl_NewStringObj(NULL, 0));
	}
    }
    
    ckfree((char *) icmpPtr->targets);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_IcmpObjCmd --
 *
 *	This procedure is invoked to process the "icmp" command.
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
Tnm_IcmpObjCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST objv[];
{
    int actTimeout = -1;	/* actually used timeout */
    int actRetries = -1;	/* actually used retries */
    int actSize = -1;		/* actually used size */
    int actDelay = -1;		/* actually used delay */
    int actWindow = -1;		/* actually used window size */

    int type = 0;		/* the request type */
    int ttl = -1;		/* the time to live field */
    int flags = 0;		/* the flags for this request */
    int x, code;

    enum commands { 
	cmdEcho, cmdMask, cmdTimestamp, cmdTrace, cmdTtl
    } cmd;

    static CONST char *cmdTable[] = {
	"echo", "mask", "timestamp", "trace", "ttl", (char *) NULL
    };

    TnmIcmpRequest *icmpPtr;

    IcmpControl *control = (IcmpControl *) 
	Tcl_GetAssocData(interp, tnmIcmpControl, NULL);

    if (! control) {
	control = (IcmpControl *) ckalloc(sizeof(IcmpControl));
	control->retries = 2;
	control->timeout = 5;
	control->size = 64;
	control->delay = 0;
	control->window = 10;
	Tcl_SetAssocData(interp, tnmIcmpControl, AssocDeleteProc, 
			 (ClientData) control);
    }

    if (objc == 1) {
      icmpWrongArgs:
	Tcl_WrongNumArgs(interp, 1, objv, "?-retries n? ?-timeout n? ?-size n? ?-delay n? ?-window size? option ?arg? hosts");
	return TCL_ERROR;
    }

    /*
     * Parse the options.
     */

    for (x = 1; x < objc;) {
	code = TnmGetTableKeyFromObj(interp, icmpOptionTable,
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
	x++;
	switch ((enum options) code) {
	case optDelay:
	    if (x == objc) {
                Tcl_SetObjResult(interp, Tcl_NewIntObj(control->delay));
		return TCL_OK;
	    }
	    if (TnmGetIntRangeFromObj(interp, objv[x],
				      0, 255, &actDelay) != TCL_OK) {
		return TCL_ERROR;
            }
            x++;
	    break;
	case optRetries:
	    if (x == objc) {
		Tcl_SetObjResult(interp, Tcl_NewIntObj(control->retries));
		return TCL_OK;
	    }
	    if (TnmGetUnsignedFromObj(interp, objv[x], 
				      &actRetries) != TCL_OK) {
                return TCL_ERROR;
	    }
	    x++;
	    break;
	case optSize:
	    if (x == objc) {
		Tcl_SetObjResult(interp, Tcl_NewIntObj(control->size));
		return TCL_OK;
	    }
	    if (TnmGetIntRangeFromObj(interp, objv[x],
				      44, 65515, &actSize) != TCL_OK) {
                return TCL_ERROR;
            }
	    x++;
	    break;
	case optTimeout:
	    if (x == objc) {
                Tcl_SetObjResult(interp, Tcl_NewIntObj(control->timeout));
                return TCL_OK;
            }
	    if (TnmGetPositiveFromObj(interp, objv[x],
				      &actTimeout) != TCL_OK) {
                return TCL_ERROR;
            }
	    x++;
	    break;
	case optWindow:
	    if (x == objc) {
                Tcl_SetObjResult(interp, Tcl_NewIntObj(control->window));
		return TCL_OK;
	    }
	    if (TnmGetIntRangeFromObj(interp, objv[x],
				      0, 65535, &actWindow) != TCL_OK) {
		return TCL_ERROR;
            }
            x++;
	    break;
	}
    } 

    /*
     * No arguments left? Set the default values and return.
     */

    if (objc == x) {
        if (actRetries >= 0) {
            control->retries = actRetries;
        }
        if (actTimeout > 0) {
            control->timeout = actTimeout;
        }
	if (actSize > 0) {
	    control->size = actSize;
	}
	if (actDelay >= 0) {
	    control->delay = actDelay;
	}
	if (actWindow >= 0) {
	    control->window = actWindow;
	}
        return TCL_OK;
    }

    /*
     * Now we should have at least two arguments left!
     */

    if (objc < 2) {
	goto icmpWrongArgs;
    }

    actRetries = actRetries < 0 ? control->retries : actRetries;
    actTimeout = actTimeout < 0 ? control->timeout : actTimeout;
    actSize  = actSize  < 0 ? control->size  : actSize;
    actDelay = actDelay < 0 ? control->delay : actDelay;
    actWindow = actWindow < 0 ? control->window : actWindow;

    /*
     * Get the query type.
     */

    code = Tcl_GetIndexFromObj(interp, objv[x], cmdTable,
                               "option", TCL_EXACT, (int *) &cmd);
    if (code != TCL_OK) {
        return code;
    }

    switch (cmd) {
    case cmdEcho:
	type = TNM_ICMP_TYPE_ECHO;
	break;
    case cmdMask:
	type = TNM_ICMP_TYPE_MASK;
	break;
    case cmdTimestamp:
	type = TNM_ICMP_TYPE_TIMESTAMP;
	break;
    case cmdTtl:
	type = TNM_ICMP_TYPE_TRACE;
	x++;
	if (objc - x < 2) {
	    goto icmpWrongArgs;
	}
	if (TnmGetIntRangeFromObj(interp, objv[x], 
				  1, 255, &ttl) != TCL_OK) {
            return TCL_ERROR;
        }
	break;
    case cmdTrace:
	type = TNM_ICMP_TYPE_TRACE;
	flags |= TNM_ICMP_FLAG_LASTHOP;
	x++;
	if (objc - x < 2) {
            goto icmpWrongArgs;
        }
	if (TnmGetIntRangeFromObj(interp, objv[x], 
				  1, 255, &ttl) != TCL_OK) {
            return TCL_ERROR;
        }
	break;
    }
    x++;

    /*
     * There should be one argument left which contains the list
     * of target IP asdresses. 
     */

    if (objc - x != 1) {
	goto icmpWrongArgs;
    }

    /*
     * Create and initialize an ICMP request structure.
     */

    icmpPtr = (TnmIcmpRequest *) ckalloc(sizeof(TnmIcmpRequest));
    memset((char *) icmpPtr, 0, sizeof(TnmIcmpRequest));
    
    icmpPtr->type = type;
    icmpPtr->ttl = ttl;
    icmpPtr->timeout = actTimeout;
    icmpPtr->retries = actRetries;
    icmpPtr->delay = actDelay;
    icmpPtr->size = actSize;
    icmpPtr->window = actWindow;
    icmpPtr->flags = flags;

    code = IcmpRequest(interp, objv[objc-1], icmpPtr);
    ckfree((char *) icmpPtr);
    return code;
}
