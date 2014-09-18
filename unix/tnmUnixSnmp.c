/*
 * tnmUnixSnmp.c --
 *
 *	This file contains all functions that handle UNIX specific
 *	functions for the SNMP engine. This is basically the code
 *	required to receive SNMP traps via the nmtrapd(8) daemon.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmUnixSnmp.c,v 1.1.1.1 2006/12/07 12:16:59 karl Exp $
 */

#include "tnmSnmp.h"

extern int hexdump;		/* flag that controls hexdump */

/*
 * The default filename where we will find the nmtrapd binary. This
 * is normally overwritten in the Makefile.
 */

#ifndef NMTRAPD
#define NMTRAPD "/usr/local/bin/nmtrapd"
#endif

/*
 * The following variable holds the channel used to access
 * the pipe to the nmtrapd process.
 */

static Tcl_Channel channel = NULL;

/*
 * The following variable holds the TCP channel which is used to
 * talk to the nmtrapd daemon.
 */

static Tcl_Channel trap_channel = NULL;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int
ForkDaemon		_ANSI_ARGS_((Tcl_Interp *interp));

static void
TrapProc		_ANSI_ARGS_((ClientData clientData, int mask));

static int
TrapRecv		_ANSI_ARGS_((Tcl_Interp *interp, 
				     u_char *packet, int *packetlen, 
				     struct sockaddr_in *from));

/*
 *----------------------------------------------------------------------
 *
 * ForkDaemon --
 *
 *	This procedure starts the trap forwarder daemon named
 *	nmtrapd.
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
ForkDaemon(interp)
    Tcl_Interp *interp;
{
    int argc = 1;
    char *argv[2];

    argv[0] = getenv("TNM_NMTRAPD");
    if (! argv[0]) {
	argv[0] = NMTRAPD;
    }
    argv[1] = NULL;

    channel = Tcl_OpenCommandChannel(interp, argc, argv, 0);
    return channel ? TCL_OK : TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpNmtrapdOpen --
 *
 *	This procedure creates a channel to receive trap messages.
 *	Since traps are send to a privileged port, we start the nmtrapd
 *	trap multiplexer and connect to it via a TCP connection.
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
TnmSnmpNmtrapdOpen(interp)
    Tcl_Interp *interp;
{
    int i;
    
    if (trap_channel) {
	Tcl_RegisterChannel((Tcl_Interp *) NULL, trap_channel);
	return TCL_OK;
    }
    
    if (! trap_channel) {
	trap_channel = Tcl_OpenTcpClient(interp, 1702, "localhost", 0, 0, 0);
	if (! trap_channel) {
	    if (ForkDaemon(interp) != TCL_OK) {
		return TCL_ERROR;
	    }
	    for (i = 0; i < 5; i++) {
		sleep(1);
		trap_channel = Tcl_OpenTcpClient(interp, 1702, "localhost",
						 0, 0, 0);
		if (trap_channel) break;
	    }
	}
	if (! trap_channel) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "cannot connect to nmtrapd: ",
			     Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
    }

    if (Tcl_SetChannelOption(interp, trap_channel,
			     "-translation", "binary") != TCL_OK) {
	(void) Tcl_Close((Tcl_Interp *) NULL, trap_channel);
	return TCL_ERROR;
    }
    
    Tcl_RegisterChannel((Tcl_Interp *) NULL, trap_channel);
    Tcl_CreateChannelHandler(trap_channel, TCL_READABLE,
			     TrapProc, (ClientData) interp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpNmtrapdClose --
 *
 *	This procedure closes the socket for incoming traps.
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
TnmSnmpNmtrapdClose()
{
    if (trap_channel) {
	Tcl_UnregisterChannel((Tcl_Interp *) NULL, trap_channel);
	trap_channel = NULL;
	Tcl_ReapDetachedProcs();
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TrapRecv --
 *
 *	This procedure reads from the trap daemon to process incoming
 *	trap messages.
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
TrapRecv(interp, packet, packetlen, from)
    Tcl_Interp *interp;
    u_char *packet;
    int *packetlen;
    struct sockaddr_in *from;
{
    int len, rlen;
    char c, version, unused;

    if (Tcl_Read(trap_channel, (char *) &version, 1) != 1) {
	goto errorExit;
    }
    if (Tcl_Read(trap_channel, (char *) &unused, 1) != 1) {
	goto errorExit;
    }
    if (Tcl_Read(trap_channel, (char *) &from->sin_port, 2) != 2) {
	goto errorExit;
    }
    if (Tcl_Read(trap_channel, (char *) &from->sin_addr.s_addr, 4) != 4) {
	goto errorExit;
    }
    if (Tcl_Read(trap_channel, (char *) &len, 4) != 4) {
	goto errorExit;
    }
    len = ntohl(len);
    rlen = len < *packetlen ? len : *packetlen;
    if (Tcl_Read(trap_channel, (char *) packet, rlen) <= 0) {
	goto errorExit;
    }

    /*
     * Eat up any remaining data-bytes.
     */

    while (len > *packetlen) {
	if (Tcl_Read(trap_channel, &c, 1) != 1) {
	    goto errorExit;
	}
	len--;
    }

    *packetlen = rlen;

    if (hexdump) {
	TnmSnmpDumpPacket(packet, *packetlen, from, NULL);
    }

    /* 
     * Finally, make sure that the socket address belongs to the 
     * INET address family.
     */

    from->sin_family = AF_INET;
    return TCL_OK;

 errorExit:
    TnmSnmpNmtrapdClose();
    Tcl_SetResult(interp, "lost connection to nmtrapd daemon", TCL_STATIC);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TrapProc --
 *
 *	This procedure is called from the event dispatcher whenever
 *	a trap message is received.
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
TrapProc(clientData, mask)
    ClientData clientData;
    int mask;
{
    Tcl_Interp *interp = (Tcl_Interp *) clientData;
    u_char packet[TNM_SNMP_MAXSIZE];
    int code, packetlen = TNM_SNMP_MAXSIZE;
    struct sockaddr_in from;

    Tcl_ResetResult(interp);
    code = TrapRecv(interp, packet, &packetlen, &from);
    if (code != TCL_OK) return;

    code = TnmSnmpDecode(interp, packet, packetlen, &from, NULL, NULL,
			 NULL, NULL);
    if (code == TCL_ERROR) {
	Tcl_AddErrorInfo(interp, "\n    (snmp trap event)");
	Tcl_BackgroundError(interp);
    }
    if (code == TCL_CONTINUE && hexdump) {
	TnmWriteMessage(Tcl_GetStringResult(interp));
	TnmWriteMessage("\n");
    }
}
