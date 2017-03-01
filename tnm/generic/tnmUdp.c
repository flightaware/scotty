/*
 * tnmUdp.c --
 *
 *	This is the implementation of the udp command that allows
 *	to send and receive udp datagrams. This implementation is
 *	supposed to be thread-safe.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2003 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmInt.h"
#include "tnmPort.h"

/*
 * A structure to describe an open UDP socket.
 */

typedef struct Udp {
    int sock;			/* The socket we are using.	       */
    int connected:1;		/* Flag set if connected.	       */
    int nameChanged:1;		/* Flag set if the name changed.       */
    int addrConfigured:1;       /* Flag set if (remote) address configured */
    int portConfigured:1;       /* Flag set if (remote) port configured */
    struct sockaddr_in remote;  /* Configured remote address/port */
    struct sockaddr_in name;	/* Name of the local socket.           */
    struct sockaddr_in peer;	/* Name of the peer.		       */
    Tcl_Obj *readCmd;		/* Command to execute if readable.     */
    Tcl_Obj *writeCmd;		/* Command to execute if writeable.    */
    Tcl_Obj *tagList;		/* The tags associated with the socket. */
    Tcl_Command token;		/* The command token used by Tcl.      */
    Tcl_Interp *interp;		/* The interpreter owning this socket. */
    struct Udp *nextPtr;	/* Next socket in our list of sockets. */
} Udp;

/*
 * Every Tcl interpreter has an associated UdpControl record. It keeps
 * track of the list of udp sockets known by the interpreter. The name
 * tnmUdpControl is used to get/set the UdpControl record.
 */

static char tnmUdpControl[] = "tnmUdpControl";

typedef struct UdpControl {
    Udp *udpList;		/* The socket list for this interpreter. */
} UdpControl;

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
AssocDeleteProc	(ClientData clientData, Tcl_Interp *interp);

static void
DeleteProc	(ClientData clientData);

static void
DestroyProc	(char *memPtr);

static void
UdpEventProc	(ClientData clientData, int mask);

static int
UdpCreate	(Tcl_Interp *interp, int objc,
			     Tcl_Obj *CONST objv[]);
static int
UdpConnect	(Tcl_Interp *interp, Udp *udpPtr, int objc,
			     Tcl_Obj *CONST objv[]);
static int
UdpSend		(Tcl_Interp *interp, Udp *udpPtr, int objc,
			     Tcl_Obj *CONST objv[]);
static int
UdpReceive	(Tcl_Interp *interp, Udp *udpPtr, int objc,
			     Tcl_Obj *CONST objv[]);
#ifdef HAVE_MULTICAST
static int
UdpJoin		(Tcl_Interp *interp, Udp *udpPtr, int objc,
			     Tcl_Obj *CONST objv[]);
#endif

#if 0
static int
UdpMulticast	(Tcl_Interp *interp, int objc,
			     Tcl_Obj *CONST objv[]));
#endif

static Tcl_Obj*
GetOption	(Tcl_Interp *interp, ClientData object, 
			     int option);
static int
SetOption	(Tcl_Interp *interp, ClientData object, 
			     int option, Tcl_Obj *objPtr);
static int
UdpObjCmd	(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *CONST objv[]);
static int
UdpFind		(Tcl_Interp *interp,
			     int objc, Tcl_Obj *CONST objv[]);

/*
 * The options used to configure udp socket objects.
 */

enum options {
    optAddress, optPort, optMyAddress, optMyPort,
    optReadCmd, optWriteCmd, optTags
#ifdef HAVE_MULTICAST
    , optTtl
#endif
};

static TnmTable optionTable[] = {
    { optAddress,	"-address" },
    { optPort,		"-port" },
    { optMyAddress,	"-myaddress" },
    { optMyPort,	"-myport" },
    { optReadCmd,	"-read" },
    { optWriteCmd,	"-write" },
    { optTags,		"-tags" },
#ifdef HAVE_MULTICAST
    { optTtl,		"-ttl" },
#endif
    { 0, NULL }
};

static TnmConfig config = {
    optionTable,
    SetOption,
    GetOption
};

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
    UdpControl *control = (UdpControl *) clientData;

    /*
     * Note, we do not care about udp objects since Tcl first
     * removes all commands before calling this delete procedure.
     */

    if (control) {
	ckfree((char *) control);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteProc --
 *
 *	This procedure removes the udp socket from the list of udp
 *	sockets and releases all memory associated with a udp socket
 *	object.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A udp socket is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteProc(ClientData clientData)
{
    Udp **udpPtrPtr;
    Udp *udpPtr = (Udp *) clientData;
    UdpControl *control = (UdpControl *)
	Tcl_GetAssocData(udpPtr->interp, tnmUdpControl, NULL);

    /*
     * First, update the list of all known udp sockets.
     */

    udpPtrPtr = &control->udpList;
    while (*udpPtrPtr && (*udpPtrPtr) != udpPtr) {
	udpPtrPtr = &(*udpPtrPtr)->nextPtr;
    }

    if (*udpPtrPtr) {
	(*udpPtrPtr) = udpPtr->nextPtr;
    }

    Tcl_EventuallyFree((ClientData) udpPtr, DestroyProc);
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyProc --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a udp socket at a safe
 *	time (when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the udp socket is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyProc(char *memPtr)
{
    Udp *udpPtr = (Udp *) memPtr;

    TnmDeleteSocketHandler(udpPtr->sock);
    TnmSocketClose(udpPtr->sock);
    if (udpPtr->readCmd) {
	Tcl_DecrRefCount(udpPtr->readCmd);
    }
    if (udpPtr->writeCmd) {
	Tcl_DecrRefCount(udpPtr->writeCmd);
    }
    if (udpPtr->tagList) {
	Tcl_DecrRefCount(udpPtr->tagList);
    }
    ckfree((char *) udpPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * UdpEventProc --
 *
 *	This procedure is invoked by the event dispatcher if the udp
 *	socket is readable or writable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arbitrary Tcl commands are evaluated.
 *
 *----------------------------------------------------------------------
 */

static void
UdpEventProc(ClientData clientData, int mask)
{
    Udp *udpPtr = (Udp *) clientData;
    Tcl_Interp *interp = udpPtr->interp;
    Tcl_Obj *cmd = NULL;
    int length, code;
    Tcl_DString tclCmd;
    char *startPtr, *scanPtr;
    char buf[20];
    
    (void) Tcl_GetStringFromObj(udpPtr->readCmd, &length);
    if (mask == TCL_READABLE && length) {
	cmd = udpPtr->readCmd;
    }

    (void) Tcl_GetStringFromObj(udpPtr->writeCmd, &length);
    if (mask == TCL_WRITABLE && length) {
	cmd = udpPtr->writeCmd;
    }

    if (cmd) {
	Tcl_DStringInit(&tclCmd);
	startPtr = Tcl_GetStringFromObj(cmd, NULL);
	
	for (scanPtr = startPtr; *scanPtr != '\0'; scanPtr++) {
	    if (*scanPtr != '%') {
		continue;
	    }
	    Tcl_DStringAppend(&tclCmd, startPtr, scanPtr - startPtr);
	    scanPtr++;
	    startPtr = scanPtr + 1;
	    switch (*scanPtr) {
	    case 'U':
		Tcl_DStringAppend(&tclCmd,
				  Tcl_GetCommandName(interp, 
						     udpPtr->token), -1);
		break;
	    default:
		sprintf(buf, "%%%c", *scanPtr);
		Tcl_DStringAppend(&tclCmd, buf, -1);
	    }
	}
	Tcl_DStringAppend(&tclCmd, startPtr, scanPtr - startPtr);
    
	Tcl_Preserve((ClientData) interp);
	Tcl_AllowExceptions(interp);
	code = Tcl_GlobalEval(interp, Tcl_DStringValue(&tclCmd));
	Tcl_DStringFree(&tclCmd);
	
	if (code == TCL_ERROR) {
	    Tcl_AddErrorInfo(interp,
		"\n    (script bound to udp socket - binding deleted)");
	    Tcl_BackgroundError(interp);
	    TnmDeleteSocketHandler(udpPtr->sock);
	}
	Tcl_Release((ClientData) interp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UdpCreate --
 *
 *	This procedure is invoked to open a UDP socket. It basically
 *	implements the "udp create" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
UdpCreate(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    static unsigned nextId = 0;
    int code;
    Udp *udpPtr, *p;
    char *cmdName;
    socklen_t len;
    UdpControl *control = (UdpControl *) 
	Tcl_GetAssocData(interp, tnmUdpControl, NULL);

    udpPtr = (Udp *) ckalloc(sizeof(Udp));
    memset((char *) udpPtr, 0, sizeof(Udp));
    udpPtr->readCmd = Tcl_NewStringObj(NULL, 0);
    Tcl_IncrRefCount(udpPtr->readCmd);
    udpPtr->writeCmd = Tcl_NewStringObj(NULL, 0);
    Tcl_IncrRefCount(udpPtr->writeCmd);
    udpPtr->tagList = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(udpPtr->tagList);
    udpPtr->interp = interp;

    udpPtr->sock = TnmSocket(PF_INET, SOCK_DGRAM, 0);
    if (udpPtr->sock == TNM_SOCKET_ERROR) {
	Tcl_AppendResult(interp, "could not create socket: ", 
			 Tcl_PosixError(interp), (char *) NULL);
	Tcl_EventuallyFree((ClientData) udpPtr, DestroyProc);
	return TCL_ERROR;
    }

#ifdef SO_REUSEADDR
    {
	int one = 1;
	setsockopt(udpPtr->sock, SOL_SOCKET, SO_REUSEADDR,
		   (char *) &one, sizeof(one));
    }
#endif

    len = sizeof(udpPtr->name);
    (void) getsockname(udpPtr->sock, (struct sockaddr *) &udpPtr->name, &len);
    
    len = sizeof(udpPtr->peer);
    (void) getpeername(udpPtr->sock, (struct sockaddr *) &udpPtr->peer, &len);

    code = TnmSetConfig(interp, &config, (ClientData) udpPtr, objc, objv);
    if (code != TCL_OK) {
	Tcl_EventuallyFree((ClientData) udpPtr, DestroyProc);
        return TCL_ERROR;
    }

    if (udpPtr->nameChanged) {
	udpPtr->nameChanged = 0;
	code = TnmSocketBind(udpPtr->sock,
			     (struct sockaddr *) &udpPtr->name,
			     sizeof(udpPtr->name));
	if (code == TNM_SOCKET_ERROR) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "can not bind socket: ",
			     Tcl_PosixError(interp), (char *) NULL);
	    Tcl_EventuallyFree((ClientData) udpPtr, DestroyProc);
	    return TCL_ERROR;
	}
    }

    /*
     * Put the new udp socket into the udp socket list. We add it at
     * the end to preserve the order in which the udp sockets were
     * created.
     */

    if (control->udpList == NULL) {
        control->udpList = udpPtr;
    } else {
        for (p = control->udpList; p->nextPtr != NULL; p = p->nextPtr) ;
	p->nextPtr = udpPtr;
    }

    cmdName = TnmGetHandle(interp, "udp", &nextId);
    udpPtr->token = Tcl_CreateObjCommand(interp, cmdName, UdpObjCmd,
					 (ClientData) udpPtr, DeleteProc);
    Tcl_SetResult(interp, cmdName, TCL_STATIC);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UdpConnect --
 *
 *	This procedure is invoked to connect a UDP socket. It basically
 *	implements the "udp# connect" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
UdpConnect(Tcl_Interp *interp, Udp *udpPtr, int objc, Tcl_Obj *CONST objv[])
{
    struct sockaddr_in name;
    char *host, *port;
    socklen_t len;

    if (objc != 4) {
	Tcl_WrongNumArgs(interp, 2, objv, "host port");
        return TCL_ERROR;
    }

    host = Tcl_GetStringFromObj(objv[2], NULL);
    if (TnmSetIPAddress(interp, host, &name) != TCL_OK) {
	return TCL_ERROR;
    }

    port = Tcl_GetStringFromObj(objv[3], NULL);
    if (TnmSetIPPort(interp, "udp", port, &name) != TCL_OK) {
        return TCL_ERROR;
    }

    if (connect(udpPtr->sock, (struct sockaddr *) &name, sizeof(name)) < 0) {
	Tcl_AppendResult(interp, "can not connect to host \"", host,
			 "\" using port \"", port, "\": ", 
			 Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }

    len = sizeof(udpPtr->peer);
    (void) getpeername(udpPtr->sock, (struct sockaddr *) &udpPtr->peer, &len);

    udpPtr->connected = 1;
    udpPtr->addrConfigured = 0;
    udpPtr->portConfigured = 0;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UdpSend --
 *
 *	This procedure is invoked to send a message using a UDP socket.
 *	It basically implements the "udp# send" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
UdpSend(Tcl_Interp *interp, Udp *udpPtr, int objc, Tcl_Obj *CONST objv[])
{
    struct sockaddr_in name;
    struct sockaddr_in *to;
    int msgArg, len;
    char *bytes;
    char *host, *port;
    char addr[INET_ADDRSTRLEN];
    char p[6];

    // Not used but valgrind complains
    memset(name.sin_zero, 0, sizeof(name.sin_zero));

    switch (objc) {
    case 3: /* either we have a configured remote address, or we are connected */
	if (!udpPtr->connected && !(udpPtr->addrConfigured && udpPtr->portConfigured)) {
		Tcl_WrongNumArgs(interp, 2, objv,
				 "host port string."
				 " UDP endpoint must either be connected, or"
				 " -address and -port configured.");
		/* Tcl_AppendResult(interp, "\nconnected:", */
		/* 		 (udpPtr->connected)?"yes":"no", */
		/* 		 " addrConfigured: ", */
		/* 		 (udpPtr->addrConfigured)?"yes":"no", */
		/* 		 " portConfigured: ", */
		/* 		 (udpPtr->portConfigured)?"yes":"no", */
		/* 		 (char *) NULL); */
		return TCL_ERROR;
	    }
	break;
	
    case 5: /* we must not be connected */
	if (udpPtr->connected) {
	    Tcl_WrongNumArgs(interp, 2, objv,
			     "string. UDP endpoint is in connected state.");
	    return TCL_ERROR;
	}
	host = Tcl_GetStringFromObj(objv[2], NULL);
	port = Tcl_GetStringFromObj(objv[3], NULL);
	if (TnmSetIPAddress(interp, host, &name) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (TnmSetIPPort(interp, "udp", port, &name) != TCL_OK) {
	    return TCL_ERROR;
	}
	to = &name;
	msgArg = 4;
	break;
	
    default:
	Tcl_WrongNumArgs(interp, 2, objv,
			 "string | host port string.");
	return TCL_ERROR;
    }

    switch (objc) {

    case 3:
	if (udpPtr->connected) {
	    bytes = Tcl_GetByteArrayFromObj(objv[2], &len);
	    if (send(udpPtr->sock, bytes, len, 0) < 0) {
		Tcl_AppendResult(interp, "udp send failed: ", 
				 Tcl_PosixError(interp), (char *) NULL);
		return TCL_ERROR;
	    } else {
		return TCL_OK;
	    }
	}
	to = &udpPtr->remote;
	msgArg = 2;
	/* prep remote info in case we fail */
	host = addr;
	inet_ntop(AF_INET, &to->sin_addr, host, INET_ADDRSTRLEN);
	port = p;
	snprintf(port, sizeof(p), "%d", htons(to->sin_port));
	/* fall through */
	
    case 5:

	bytes = Tcl_GetByteArrayFromObj(objv[msgArg], &len);
	len = TnmSocketSendTo(udpPtr->sock, bytes, len, 0, 
			      (struct sockaddr *) to, sizeof(*to));
	if (len == TNM_SOCKET_ERROR) {
	    Tcl_AppendResult(interp, "udp send to host \"", host, 
			     "\" port \"", port, "\" failed: ",
			     Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
    }    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UdpReceive --
 *
 *	This procedure is invoked to receive a message from a UDP
 *	socket. It basically implements the "udp# receive" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
UdpReceive(Tcl_Interp *interp, Udp *udpPtr, int objc, Tcl_Obj *CONST objv[])
{
    char msg[65535];
    socklen_t clen, len;
    struct sockaddr_in client;
    Tcl_Obj *objPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	return TCL_ERROR;
    }

    clen = sizeof(client);
    len = TnmSocketRecvFrom(udpPtr->sock, msg, sizeof(msg), 0, 
			    (struct sockaddr *) &client, &clen);
    if (len == TNM_SOCKET_ERROR) {
	Tcl_AppendResult(interp, "receive failed on \"",
			 Tcl_GetCommandName(interp, udpPtr->token), "\": ", 
			 Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }

    objPtr = Tcl_GetObjResult(interp);

    Tcl_ListObjAppendElement(interp, objPtr,
			     TnmNewIpAddressObj(&client.sin_addr));
    Tcl_ListObjAppendElement(interp, objPtr,
			     Tcl_NewIntObj((int) ntohs(client.sin_port)));
    Tcl_ListObjAppendElement(interp, objPtr,
			     Tcl_NewByteArrayObj(msg, len));

    return TCL_OK;
}

#ifdef HAVE_MULTICAST
/*
 *----------------------------------------------------------------------
 *
 * UdpJoin --
 *
 *	This procedure is invoked to join a multicast channel.
 *	It basically implements the "udp# join" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
UdpJoin(Tcl_Interp *interp, Udp *udpPtr, int objc, Tcl_Obj *CONST objv[])
{
    struct sockaddr_in name;
    struct ip_mreq mreq;
    char *s;
    
    if (objc < 4 || objc > 5) {
	Tcl_WrongNumArgs(interp, 2, objv, "group port ?interface?");
	return TCL_ERROR;
    }

    s = Tcl_GetStringFromObj(objv[2], NULL);
    if (TnmSetIPAddress(interp, s, &name) != TCL_OK) {
	return TCL_ERROR;
    }

    s = Tcl_GetStringFromObj(objv[3], NULL);
    if (TnmSetIPPort(interp, "udp", s, &name) != TCL_OK) {
	return TCL_ERROR;
    }

    mreq.imr_multiaddr.s_addr = name.sin_addr.s_addr;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if ((mreq.imr_multiaddr.s_addr == -1 ||
	 !IN_MULTICAST(ntohl(mreq.imr_multiaddr.s_addr)))) {
	Tcl_AppendResult(interp, "bad multicast group address \"",
			 s, "\"", (char *) NULL);
	return TCL_ERROR;
    }

    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (objc == 5) {
	struct sockaddr_in ifaddr;
	if (TnmSetIPAddress(interp,
			    Tcl_GetStringFromObj(objv[4], NULL),
			    &ifaddr) != TCL_OK) {
	    return TCL_ERROR;
	}
	mreq.imr_interface.s_addr = ifaddr.sin_addr.s_addr;
    }

    if (setsockopt(udpPtr->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		   (char*) &mreq, sizeof(mreq)) == -1) {
	Tcl_AppendResult(interp, "adding multicast group membership failed: ",
			 Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }

    return TCL_OK;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * GetOption --
 *
 *	This procedure retrieves the value of a udp socket option.
 *
 * Results:
 *	A pointer to the value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
GetOption(Tcl_Interp *interp, ClientData object, int option)
{
    Udp *udpPtr = (Udp *) object;
#ifdef HAVE_MULTICAST
    unsigned char ttl;
    int len;
#endif
 
    switch ((enum options) option) {
    case optAddress:
	return TnmNewIpAddressObj(&udpPtr->remote.sin_addr);
    case optPort:
	return Tcl_NewIntObj((int) ntohs(udpPtr->remote.sin_port));
    case optMyAddress:
	return TnmNewIpAddressObj(&udpPtr->name.sin_addr);
    case optMyPort:
	return Tcl_NewIntObj((int) ntohs(udpPtr->name.sin_port));
    case optReadCmd:
 	return udpPtr->readCmd;
    case optWriteCmd:
	return udpPtr->writeCmd;
    case optTags:
	return udpPtr->tagList;
#ifdef HAVE_MULTICAST
    case optTtl:
	len = sizeof(ttl);
	if (getsockopt(udpPtr->sock, IPPROTO_IP, IP_MULTICAST_TTL,
		       (char*) &ttl, &len) == -1) {
	    return NULL;
	}
	return Tcl_NewIntObj(ttl);
#endif
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * SetOption --
 *
 *	This procedure modifies a single option of a udp socket
 *	object.
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
SetOption(Tcl_Interp *interp, ClientData object, int option, Tcl_Obj *objPtr)
{
    int mask, length;
    Udp *udpPtr = (Udp *) object;
#ifdef HAVE_MULTICAST
    int value;
    unsigned char ttl;
#endif

    switch ((enum options) option) {
    case optAddress:
	if (TnmSetIPAddress(interp, Tcl_GetStringFromObj(objPtr, NULL),
			    &udpPtr->remote) != TCL_OK) {
	    return TCL_ERROR;
	}
	udpPtr->addrConfigured = 1;
	if (udpPtr->portConfigured) {
	    udpPtr->connected = 0;
	}
	break;
    case optPort:
	if (TnmSetIPPort(interp, "udp", Tcl_GetStringFromObj(objPtr, NULL),
			 &udpPtr->remote) != TCL_OK) {
	    return TCL_ERROR;
	}
	udpPtr->portConfigured = 1;
	if (udpPtr->addrConfigured) {
	    udpPtr->connected = 0;
	}
	break;
    case optMyAddress:
	if (TnmSetIPAddress(interp, Tcl_GetStringFromObj(objPtr, NULL),
			    &udpPtr->name) != TCL_OK) {
	    return TCL_ERROR;
	}
	udpPtr->nameChanged = 1;
	break;
    case optMyPort:
	if (TnmSetIPPort(interp, "udp", Tcl_GetStringFromObj(objPtr, NULL),
			 &udpPtr->name) != TCL_OK) {
	    return TCL_ERROR;
	}
	udpPtr->nameChanged = 1;
	break;
    case optReadCmd:
	if (udpPtr->readCmd) {
	    Tcl_DecrRefCount(udpPtr->readCmd);
	}
	udpPtr->readCmd = objPtr;
	Tcl_IncrRefCount(udpPtr->readCmd);
	break;
    case optWriteCmd:
	if (udpPtr->writeCmd) {
	    Tcl_DecrRefCount(udpPtr->writeCmd);
	}
	udpPtr->writeCmd = objPtr;
	Tcl_IncrRefCount(udpPtr->writeCmd);
	break;
    case optTags:
	Tcl_DecrRefCount(udpPtr->tagList);
	udpPtr->tagList = objPtr;
	Tcl_IncrRefCount(udpPtr->tagList);
	break;
#ifdef HAVE_MULTICAST
    case optTtl:
	if (TnmGetIntRangeFromObj(interp, objPtr, 0, 255, &value) != TCL_OK) {
	    return TCL_ERROR;
	}
	ttl = value;
	if (setsockopt(udpPtr->sock, IPPROTO_IP, IP_MULTICAST_TTL, 
		       (char*) &ttl, sizeof(ttl)) == -1) {
	    Tcl_AppendResult(interp, "can't set multicast ttl: ",
			     Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
	break;
#endif
    }

    /*
     * Check whether we have to update the event bindings.
     */

    mask = 0;
    (void) Tcl_GetStringFromObj(udpPtr->readCmd, &length);
    if (length) {
	mask |= TCL_READABLE;
    }
    (void) Tcl_GetStringFromObj(udpPtr->writeCmd, &length);
    if (length) {
	mask |= TCL_WRITABLE;
    }
    
    TnmDeleteSocketHandler(udpPtr->sock);
    if (mask) {
	TnmCreateSocketHandler(udpPtr->sock, mask,
			       UdpEventProc, (ClientData) udpPtr);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UdpInfo --
 *
 *	This procedure returns info about the state of the UDP socket:
 *      remote address/port and connected state.
 *	It basically implements the "udp# info" command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
UdpInfo(Tcl_Interp *interp, Udp *udpPtr, int objc, Tcl_Obj *CONST objv[])
{
    Tcl_Obj *objPtr;

    if (objc != 2) {
	Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	return TCL_ERROR;
    }
    
    /* return {connect 0|1 address a.b.c.d port e}  */
    
    objPtr = Tcl_GetObjResult(interp);

    Tcl_ListObjAppendElement(interp, objPtr,
			     Tcl_NewStringObj("connected",-1));
    Tcl_ListObjAppendElement(interp, objPtr,
			     Tcl_NewIntObj(udpPtr->connected));

    Tcl_ListObjAppendElement(interp, objPtr,
			     Tcl_NewStringObj("address",-1));
    Tcl_ListObjAppendElement(interp, objPtr,
			     TnmNewIpAddressObj(&udpPtr->peer.sin_addr));

    Tcl_ListObjAppendElement(interp, objPtr,
			     Tcl_NewStringObj("port",-1));
    Tcl_ListObjAppendElement(interp, objPtr,
			     Tcl_NewIntObj((int) ntohs(udpPtr->peer.sin_port)));
   
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * UdpObjCmd --
 *
 *	This procedure implements the udp object command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
UdpObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    int code, result;
    Udp *udpPtr = (Udp *) clientData;

    enum commands {
	cmdCget, cmdConfigure, cmdConnect, cmdDestroy, cmdInfo, cmdReceive, cmdSend,
#ifdef HAVE_MULTICAST
	, cmdJoin
#endif
    } cmd;

    static CONST char *cmdTable[] = {
	"cget", "configure", "connect", "destroy", "info", "receive", "send",
#ifdef HAVE_MULTICAST
	"join",
#endif
	(char *) NULL
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?args?");
        return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }

    Tcl_Preserve((ClientData) udpPtr);

    switch (cmd) {

    case cmdConfigure:
	result = TnmSetConfig(interp, &config, (ClientData) udpPtr, 
			      objc, objv);
	if (udpPtr->nameChanged) {
	    udpPtr->nameChanged = 0;
	    code = TnmSocketBind(udpPtr->sock,
				 (struct sockaddr *) &udpPtr->name,
				 sizeof(udpPtr->name));
	    if (code == TNM_SOCKET_ERROR) {
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "can not bind socket: ",
				 Tcl_PosixError(interp), (char *) NULL);
		return TCL_ERROR;
	    }
	}
	break;

    case cmdCget:
	result = TnmGetConfig(interp, &config, (ClientData) udpPtr, 
			      objc, objv);
	break;

    case cmdConnect:
        result = UdpConnect(interp, udpPtr, objc, objv);
	break;

    case cmdDestroy:
        if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    result = TCL_ERROR;
	    break;
	}
        result = Tcl_DeleteCommandFromToken(interp, udpPtr->token);
	break;

    case cmdInfo:
        result = UdpInfo(interp, udpPtr, objc, objv);
	break;

    case cmdReceive:
	result = UdpReceive(interp, udpPtr, objc, objv);
	break;

    case cmdSend:
	result = UdpSend(interp, udpPtr, objc, objv);
	break;

#ifdef HAVE_MULTICAST
    case cmdJoin:
	result = UdpJoin(interp, udpPtr, objc, objv);
	break;
#endif
    }

    Tcl_Release((ClientData) udpPtr);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * UdpFind --
 *
 *	This procedure is invoked to process the "find" command
 *	option of the udp command.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

static int
UdpFind(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    int i, result;
    Udp *udpPtr;
    Tcl_Obj *listPtr, *patList = NULL;
    UdpControl *control = (UdpControl *) 
	Tcl_GetAssocData(interp, tnmUdpControl, NULL);

    enum options { optTags } option;

    static CONST char *optionTable[] = {
        "-tags", (char *) NULL
    };

    if (objc % 2) {
	Tcl_WrongNumArgs(interp, 2, objv, "?option value? ?option value? ...");
	return TCL_ERROR;
    }

    for (i = 2; i < objc; i++) {
	result = Tcl_GetIndexFromObj(interp, objv[i++], optionTable, 
				     "option", TCL_EXACT, (int *) &option);
	if (result != TCL_OK) {
	    return TCL_ERROR;
	}
	switch (option) {
	case optTags:
	    patList = objv[i];
	    break;
	}
    }

    listPtr = Tcl_GetObjResult(interp);
    for (udpPtr = control->udpList; udpPtr; udpPtr = udpPtr->nextPtr) {
	if (patList) {
	    int match = TnmMatchTags(interp, udpPtr->tagList, patList);
	    if (match < 0) {
		return TCL_ERROR;
	    }
	    if (! match) continue;
	}
	{
	    CONST char *name = Tcl_GetCommandName(interp, udpPtr->token);
	    Tcl_Obj *elemObjPtr = Tcl_NewStringObj(name, -1);
	    Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_UdpObjCmd --
 *
 *	This procedure is invoked to process the "udp" command.
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
Tnm_UdpObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    int cmd, result = TCL_OK;
    UdpControl *control = (UdpControl *) 
	Tcl_GetAssocData(interp, tnmUdpControl, NULL);

    enum commands { 
	cmdCreate, cmdFind
    };

    static CONST char *cmdTable[] = {
	"create", "find",
	(char *) NULL
    };

    if (! control) {
	control = (UdpControl *) ckalloc(sizeof(UdpControl));
	memset((char *) control, 0, sizeof(UdpControl));
	Tcl_SetAssocData(interp, tnmUdpControl, AssocDeleteProc, 
			 (ClientData) control);
    }

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }

    switch ((enum commands) cmd) { 
    case cmdCreate:
	result = UdpCreate(interp, objc, objv);
	break;
    case cmdFind:
	result = UdpFind(interp, objc, objv);
	break;
    }

    return result;
}

/*
 * Local Variables:
 * compile-command: "make -k -C ../../unix"
 * c-basic-offset: 4
 * End:
 */

