/*
 * tnmUnixIcmp.c --
 *
 *	Make an ICMP request to a list of IP addresses. The UNIX
 *	implementation is based on nmicmpd, a set uid root program
 *	which is used to access raw sockets. This module implements
 *	the communication between the Tnm extension and nmicmpd.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmInt.h"
#include "tnmPort.h"

/*
 * The default filename where we will find the nmicmpd binary. This
 * is normally overwritten in the Makefile.
 */

#ifndef NMICMPD
#define NMICMPD "/usr/local/bin/nmicmpd"
#endif

/*
 * The following variable holds the channel used to talk 
 * to the nmicmpd process.
 */

static Tcl_Channel channel = NULL;

/*
 * The following structure is used to talk to the nmicmpd daemon. See
 * the nmicmpd(8) man page for a description of this message format.
 */

#define ICMP_MSG_VERSION	00
#define ICMP_MSG_REQUEST_SIZE	20
#define ICMP_MSG_RESPONSE_SIZE	16

typedef struct IcmpMsg {
    u_char version;		/* The protocol version. */
    u_char type;		/* The requested ICMP operation. */
    u_char status;		/* Status information. */
    u_char flags;		/* Flags exchanged with the daemon. */
    unsigned int tid;		/* The unique transaction identifier. */
    struct in_addr addr;	/* The target IPv4 address. */
    union {
	struct {
	    u_char ttl;		/* The ttl field of a request. */
	    u_char timeout;	/* The timeout field of a request. */
	    u_char retries;	/* The retries of a request. */
	    u_char delay;	/* The delay of a request. */
	} c;
	unsigned int data;	/* The data value of a response. */
    } u;
    unsigned short size;	/* The requested ICMP message size. */
    unsigned short window;	/* The window size for this request. */
} IcmpMsg;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int
ForkDaemon	(Tcl_Interp *interp);

static void
KillDaemon	(ClientData clientData);


/*
 *----------------------------------------------------------------------
 *
 * ForkDaemon --
 *
 *	This procedure is invoked to fork a nmicmpd process and to set
 *	up a channel to talk to the nmicmpd process.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A new process and a new pipe is created.
 *
 *----------------------------------------------------------------------
 */

static int
ForkDaemon(Tcl_Interp *interp)
{
    int argc = 1;
    char *argv[2];

    argv[0] = getenv("TNM_NMICMPD");
    if (! argv[0]) {
	argv[0] = NMICMPD;
    }
    argv[1] = NULL;

    channel = Tcl_OpenCommandChannel(interp, argc, argv, TCL_STDIN|TCL_STDOUT);
    if (! channel) {
	return TCL_ERROR;
    }

    Tcl_CreateExitHandler(KillDaemon, (ClientData) NULL);

    Tcl_SetChannelOption(interp, channel, "-translation", "binary");
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * KillDaemon --
 *
 *	This procedure is invoked to terminate a running nmicmpd
 *	process by closing the channel to it.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A new process and a new pipe is created.
 *
 *----------------------------------------------------------------------
 */

static void
KillDaemon(ClientData clientData)
{
    if (channel) {
	Tcl_Close((Tcl_Interp *) NULL, channel);
	channel = NULL;
	Tcl_DeleteExitHandler(KillDaemon, (ClientData) NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmIcmp --
 *
 *	This procedure is the platform specific entry point for
 *	sending ICMP requests.
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
TnmIcmp(Tcl_Interp *interp, TnmIcmpRequest *icmpPtr)
{
    int i, j, rc, code = TCL_OK;
    IcmpMsg icmpMsg;

    /*
     * Start nmicmpd if not done yet.
     */

    if (channel == NULL) {
	if (ForkDaemon(interp) != TCL_OK) {
	    return TCL_ERROR;
	}
    }

    /*
     * Start by sending all requests to the nmicmpd daemon.
     */

    for (i = 0; i < icmpPtr->numTargets; i++) {
	TnmIcmpTarget *targetPtr = &(icmpPtr->targets[i]);
	icmpMsg.version = ICMP_MSG_VERSION;
	icmpMsg.type = icmpPtr->type;
	icmpMsg.status = TNM_ICMP_STATUS_NOERROR;
	icmpMsg.flags = 0;	
	icmpMsg.tid = htonl(targetPtr->tid);
	icmpMsg.addr = targetPtr->dst;
	icmpMsg.u.c.ttl = 0;
	if (icmpMsg.type == TNM_ICMP_TYPE_TRACE) {
	    icmpMsg.u.c.ttl = icmpPtr->ttl;
	}
	icmpMsg.u.c.timeout = icmpPtr->timeout;
	icmpMsg.u.c.retries = icmpPtr->retries;
	icmpMsg.u.c.delay = icmpPtr->delay;
	icmpMsg.size = htons((unsigned short) icmpPtr->size);
	icmpMsg.window = htons((unsigned short) icmpPtr->window);
	rc = Tcl_Write(channel, (char *) &icmpMsg, ICMP_MSG_REQUEST_SIZE);
	if (rc > 0) {
	    if (Tcl_Flush(channel) != TCL_OK) {
		rc = -1;
	    }
	}
#if 0
	{
	    char s[255];
	    TnmHexEnc((char *) &icmpMsg, ICMP_MSG_REQUEST_SIZE, s);
	    strcat(s, "\n");
	    TnmWriteMessage(s);
	}
#endif
	if (rc < 0) {
	    Tcl_AppendResult(interp, "nmicmpd: ", Tcl_PosixError(interp),
			     (char *) NULL);
	    KillDaemon((ClientData) NULL);
	    return TCL_ERROR;
	}
    }

    /*
     * Collect the answers from the nmicmpd daemon.
     */

    code = TCL_OK;
    for (j = 0; j < icmpPtr->numTargets; j++) {
	rc = Tcl_Read(channel, (char *) &icmpMsg, ICMP_MSG_RESPONSE_SIZE);
	if (rc != ICMP_MSG_RESPONSE_SIZE) {
	    Tcl_AppendResult(interp, "nmicmpd: ", Tcl_PosixError(interp),
			     (char *) NULL);
	    KillDaemon((ClientData) NULL);
	    return TCL_ERROR;
	}
#if 0
	{
	    char s[255];
	    TnmHexEnc((char *) &icmpMsg, rc, s);
	    strcat(s, "\n");
	    TnmWriteMessage(s);
	}
#endif
	if (icmpMsg.status == TNM_ICMP_STATUS_GENERROR) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "nmicmpd: failed to send ICMP message",
			     (char *) NULL);
	    code = TCL_ERROR;
	    break;
	}
	if (code == TCL_OK) {
	    for (i = 0; i < icmpPtr->numTargets; i++) {
		TnmIcmpTarget *targetPtr = &(icmpPtr->targets[i]);
		if (htonl(targetPtr->tid) == icmpMsg.tid) {
		    targetPtr->res = icmpMsg.addr;
		    switch (icmpMsg.type) {
		    case TNM_ICMP_TYPE_ECHO:
		    case TNM_ICMP_TYPE_TRACE:
			targetPtr->u.rtt = ntohl(icmpMsg.u.data);
			break;
		    case TNM_ICMP_TYPE_MASK:
			targetPtr->u.mask = ntohl(icmpMsg.u.data);
			break;
		    case TNM_ICMP_TYPE_TIMESTAMP:
			targetPtr->u.tdiff = ntohl(icmpMsg.u.data);
			break;
		    }
		    targetPtr->status = icmpMsg.status;
		    targetPtr->flags = (icmpPtr->flags & icmpMsg.flags);
		    break;
		}
	    }
	}
    }

    return code;
}


