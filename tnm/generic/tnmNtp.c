/*
 * tnmNtp.c --
 *
 *	Extend a tcl command interpreter with a command to query NTP
 *	server for timestat. This implementation is supposed to be
 *	thread-safe.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
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
 * ToDo:	* check about `more' flag.
 *		* make better error return strings.
 */

struct ntp_control {
    unsigned char mode;			/* version and mode */
    unsigned char op;			/* opcode */
    unsigned short sequence;		/* sequence # */
    unsigned short status;		/* status */
    unsigned short associd;		/* association id */
    unsigned short offset;		/* data offset */
    unsigned short len;			/* data len */
    unsigned char data[(480 + 20)];	/* data + auth */
};

/*
 * The options for the ntp command.
 */

enum options { optTimeout, optRetries };

static TnmTable ntpOptionTable[] = {
    { optTimeout,	"-timeout" },
    { optRetries,	"-retries" },
    { 0, NULL }
};

/*
 * The socket used to send and receive NTP datagrams.
 */

static int sock = -1;

/*
 * Every Tcl interpreter has an associated NtpControl record. It
 * keeps track of the default settings for this interpreter.
 */

static char tnmNtpControl[] = "tnmNtpControl";

typedef struct NtpControl {
    int retries;		/* Default number of retries. */
    int timeout;		/* Default timeout in seconds. */
} NtpControl;

/*
 * Mutex used to serialize access to static variables in this module.
 */

TCL_DECLARE_MUTEX(ntpMutex)

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
AssocDeleteProc	(ClientData clientData, Tcl_Interp *interp);

static int
NtpSocket	(Tcl_Interp *interp);

static int
NtpReady	(int s, int timeout);

static void
NtpMakePkt	(struct ntp_control *pkt, int op,
			     unsigned short assoc, unsigned short seq);
static int
NtpFetch	(Tcl_Interp *interp, struct sockaddr_in *daddr, 
			     int op, int retries, int timeo,
			     char *buf, unsigned short assoc);
static int
NtpSplit	(Tcl_Interp *interp, char *varname,
			     char *pfix, char *buf);
static int 
NtpGetPeer	(char *data, int *assoc);

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
AssocDeleteProc(ClientData clientData, Tcl_Interp *interp)
{
    NtpControl *control = (NtpControl *) clientData;

    if (control) {
	ckfree((char *) control);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * NtpSocket --
 *
 *	This procedure opens a socket that is used to send NTP requests.
 *	An error message is left in interp if we can't open the socket.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A socket is opened.
 *
 *----------------------------------------------------------------------
 */

static int
NtpSocket(Tcl_Interp *interp)
{
    struct sockaddr_in maddr;
    int code;
    
    if (sock != -1) {
	TnmSocketClose(sock);
    }

    sock = TnmSocket(AF_INET, SOCK_DGRAM, 0);
    if (sock == TNM_SOCKET_ERROR) {
	Tcl_AppendResult(interp, "could not create socket: ", 
			 Tcl_PosixError(interp), (char *) NULL);
        return TCL_ERROR;
    }

    maddr.sin_family = AF_INET;
    maddr.sin_addr.s_addr = htonl(INADDR_ANY);
    maddr.sin_port = htons(0);

    code = TnmSocketBind(sock, (struct sockaddr *) &maddr, sizeof(maddr));
    if (code == TNM_SOCKET_ERROR) {
	Tcl_AppendResult(interp, "can not bind socket: ",
			 Tcl_PosixError(interp), (char *) NULL);
	TnmSocketClose(sock);
	sock = -1;
        return TCL_ERROR;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NtpReady --
 *
 *	This procedure used to wait for an answer from an NTP server.
 *
 * Results:
 *	Return 1 if we got an answer, 0 otherwise.
 *
 * Side effects:
 *	Time passes.
 *
 *----------------------------------------------------------------------
 */

static int
NtpReady(s, timeout)
    int s, timeout;
{
    fd_set rfd;
    struct timeval tv;
    int rc;
    
    FD_ZERO(&rfd);
    FD_SET(s, &rfd);
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    
    do {
	rc = select(s + 1, &rfd, (fd_set *) 0, (fd_set *) 0, &tv);
	if (rc == -1 && errno == EINTR) {
	    continue;
	}
	if (rc == -1) {
	    perror("* select failed; reason");
	    return 0;
	}
    } while (rc < 0);
    
    return rc > 0;
}

/*
 *----------------------------------------------------------------------
 *
 * NtpMakePkt --
 *
 *	This procedure creates an NTP packet.
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
NtpMakePkt(struct ntp_control *pkt, int op, unsigned short assoc, unsigned short seq)
{
    pkt->mode = 0x18 | 6;			/* version 3 | MODE_CONTROL */
    pkt->op = op;				/* CTL_OP_... */
    pkt->sequence = htons(seq);
    pkt->status = 0;
    pkt->associd = htons(assoc);
    pkt->offset = htons(0);

    if (! assoc) {
	sprintf((char *) pkt->data, 
	      "precision,peer,system,stratum,rootdelay,rootdispersion,refid");
    } else  {
	sprintf((char *) pkt->data, 
	      "srcadr,stratum,precision,reach,valid,delay,offset,dispersion");
    }
    pkt->len = htons((unsigned short) (strlen((char *) pkt->data)));
}

/*
 *----------------------------------------------------------------------
 *
 * NtpFetch --
 *
 *	This procedure fetches the result of an op from an NTP server
 *	and appends the result to the buffer buf. An error message is
 *	left in the Tcl interpreter pointed to by interp.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Ntp packets are send and received.
 *
 *----------------------------------------------------------------------
 */

static int
NtpFetch(Tcl_Interp *interp, struct sockaddr_in *daddr, int op, int retries, int timeo, char *buf, unsigned short assoc)
{
    struct ntp_control qpkt, pkt;
    struct sockaddr_in saddr;
    int i, rc;
    socklen_t slen = sizeof(saddr);
    int timeout = (timeo * 1000) / (retries + 1);

    static unsigned short seq = 1;			/* sequence number */

    /* 
     * increment to a new sequence number: 
     */

    seq++;
    
    for (i = 0; i < retries + 1; i++) {
	NtpMakePkt(&qpkt, op, assoc, seq);		/* CTL_OP_READVAR */
	memset((char *) &pkt, 0, sizeof(pkt));
	
	rc = TnmSocketSendTo(sock, (unsigned char *)&qpkt, sizeof(qpkt), 0, 
			     (struct sockaddr *) daddr, sizeof(*daddr));
	if (rc == TNM_SOCKET_ERROR) {
	    Tcl_AppendResult(interp, "udp sendto failed: ",
			     Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
	
	while (NtpReady(sock, timeout)) {
	    memset((char *) &pkt, 0, sizeof(pkt));
	    rc = TnmSocketRecvFrom(sock, (unsigned char *) &pkt, sizeof(pkt), 0, 
				   (struct sockaddr *) &saddr, &slen);
	    if (rc == TNM_SOCKET_ERROR) {
		Tcl_AppendResult(interp, "recvfrom failed: ",
				 Tcl_PosixError(interp), (char *) NULL);
		return TCL_ERROR;
	    }

	    /*
	     * Ignore short packets < (ntp_control + 1 data byte)
	     */
	    
	    if (rc < 12 + 1) {
		continue;
	    }
	    
	    if ((pkt.op & 0x80) 
		&& saddr.sin_addr.s_addr == daddr->sin_addr.s_addr
		&& saddr.sin_port == daddr->sin_port
		&& pkt.sequence == qpkt.sequence)
	    {
		strcat(buf, (char *) pkt.data);
		return TCL_OK;
	    }
	}
    }
    
    Tcl_SetResult(interp, "no ntp response", TCL_STATIC);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * NtpSplit --
 *
 *	This procedure splits the result of an NTP query into pieces
 *	and set the Tcl array variable given by varname. An error message 
 *	is left in the Tcl interpreter pointed to by interp.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	An array variable is modified.
 *
 *----------------------------------------------------------------------
 */

static int
NtpSplit(Tcl_Interp *interp, char *varname, char *pfix, char *buf)
{
    char *d, *s, *g;
    const char *r;
    char var [256];

    for (s = buf, d = buf; *s; s++) {
	if (*s == ',') {
	    *s = '\0';
	    for (g = d; *g && (*g != '='); g++) ;
	    if (*g) {
		*g++ = '\0';
		sprintf(var, "%s.%s", pfix, d);
		r = Tcl_SetVar2(interp, varname, var, g, TCL_LEAVE_ERR_MSG);
		if (!r) return TCL_ERROR;
	    }
	    for (d = s+1; *d && isspace(*d); d++) ;
	}
    }

    if (d != s) {
	if (isspace(*--s)) *s = '\0';
	if (isspace(*--s)) *s = '\0';
	for (g = d; *g && (*g != '='); g++) ;
	if (*g) {
	    *g++ = '\0';
	    sprintf(var, "%s.%s", pfix, d);
	    r = Tcl_SetVar2(interp, varname, var, g, TCL_LEAVE_ERR_MSG);
	    if (!r) return TCL_ERROR;
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NtpGetPeer --
 *
 *	This procedure scans data for a peer=... entry.
 *
 * Results:
 *	Returns 1 if the peer entry is found, 0 otherwise. The
 *	peer is returned in assoc.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int 
NtpGetPeer(char *data, int *assoc)
{
    int i;

    for (i = 0; i < (int) strlen(data); i++) {
        if (1 == sscanf(data + i, "peer=%d,", assoc)) {
	    return 1;
	}
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_NtpObjCmd --
 *
 *	This procedure is invoked to process the "ntp" command.
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
Tnm_NtpObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    struct sockaddr_in daddr;
    int x, code, assoc;
    char data1 [1024], data2 [1024];

    int actRetries = -1;	/* actually used retries */
    int actTimeout = -1;	/* actually used timeout */

    NtpControl *control = (NtpControl *) 
	Tcl_GetAssocData(interp, tnmNtpControl, NULL);

    if (! control) {
	control = (NtpControl *) ckalloc(sizeof(NtpControl));
	control->retries = 2;
	control->timeout = 2;
	Tcl_SetAssocData(interp, tnmNtpControl, AssocDeleteProc, 
			 (ClientData) control);
    }

    if (objc < 2) {
    wrongArgs:
	Tcl_WrongNumArgs(interp, 1, objv,
			 "?-timeout t? ?-retries r? host arrayName");
	return TCL_ERROR;
    }

    /* 
     * Parse the options:
     */

    for (x = 1; x < objc; x++) {
	code = TnmGetTableKeyFromObj(interp, ntpOptionTable,
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
	case optTimeout:
	    if (x == objc-1) {
		Tcl_SetIntObj(Tcl_GetObjResult(interp), control->timeout);
		return TCL_OK;
	    }
	    code = TnmGetPositiveFromObj(interp, objv[++x], &actTimeout);
	    if (code != TCL_OK) {
	        return TCL_ERROR;
	    }
	    break;
	case optRetries:
	    if (x == objc-1) {
		Tcl_SetIntObj(Tcl_GetObjResult(interp), control->retries);
		return TCL_OK;
	    }
	    code = TnmGetUnsignedFromObj(interp, objv[++x], &actRetries);
	    if (code != TCL_OK) {
	        return TCL_ERROR;
	    }
	    break;
	}
    }

    /*
     * No arguments left? Set the default values and return.
     */

    if (x == objc) {
	if (actRetries >= 0) {
	    control->retries = actRetries;
	}
	if (actTimeout > 0) {
	    control->timeout = actTimeout;
	}
        return TCL_OK;
    }

    /*
     * Now we should have two arguments left: host and arrayName.
     */
    
    if (x != objc-2) {
	goto wrongArgs;
    }

    actRetries = actRetries < 0 ? control->retries : actRetries;
    actTimeout = actTimeout < 0 ? control->timeout : actTimeout;

    Tcl_MutexLock(&ntpMutex);
    
    if (sock < 0) {
	if (NtpSocket(interp) != TCL_OK) {
	    Tcl_MutexUnlock(&ntpMutex);
	    return TCL_ERROR;
	}
    }

    if (TnmSetIPAddress(interp, Tcl_GetStringFromObj(objv[x++], NULL),
			&daddr) != TCL_OK) {
	Tcl_MutexUnlock(&ntpMutex);
        return TCL_ERROR;
    }
    daddr.sin_port = htons(123);
    
    /*
     * CTL_OP_READVAR
     */

    data1 [0] = data2 [0] = 0;
    code = NtpFetch(interp, &daddr, 2, actRetries, actTimeout, data1, 0);
    if (code != TCL_OK) {
	Tcl_MutexUnlock(&ntpMutex);
	return TCL_ERROR;
    }
    
    /*
     * Try to get additional info: 
     */

    if (NtpGetPeer(data1, &assoc)) {
	if (NtpFetch(interp, &daddr, 2, actRetries, actTimeout, data2,
		     (unsigned short) assoc)
	    != TCL_OK) {
	    Tcl_MutexUnlock(&ntpMutex);
	    return TCL_ERROR;
	}
    }

    /*
     * Split the response and write it to the Tcl array.
     */

    code = NtpSplit(interp, Tcl_GetStringFromObj(objv[x], NULL), "sys", data1);
    if (code == TCL_OK) {
	code = NtpSplit(interp, Tcl_GetStringFromObj(objv[x], NULL), "peer", data2);
    }

    Tcl_MutexUnlock(&ntpMutex);
    return code;
}

