/*
 * tnmSnmpNet.c --
 *
 *	This file contains all functions that handle transport over UDP.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmSnmpNet.c,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

#include "tnmSnmp.h"

/*
 * Local variables:
 */

extern int hexdump;		/* flag that controls hexdump */

/*
 * Shared socket used for all asynchronous messages send out by this
 * manager or agent.
 */

static TnmSnmpSocket *asyncSocket = NULL;

/*
 * Shared socket used for all synchronous manager initiated 
 * interactions.
 */

static TnmSnmpSocket *syncSocket = NULL;

/*
 * The list of all shared sockets maintained in this module.
 */

TnmSnmpSocket *tnmSnmpSocketList = NULL;

/*
 * A global variable for performance measurements.
 */

#ifdef TNM_SNMP_BENCH
TnmSnmpMark tnmSnmpBenchMark;
#endif

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
ResponseProc		_ANSI_ARGS_((ClientData clientData, int mask));

static void
AgentProc		_ANSI_ARGS_((ClientData clientData, int mask));

static int
AgentRecv		_ANSI_ARGS_((Tcl_Interp *interp, TnmSnmp *session,
				     u_char *packet, int *packetlen,
				     struct sockaddr_in *from));


/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpOpen --
 *
 *	This procedure opens a shared SNMP socket. The real socket
 *	is opend only if there is not yet an open socket with the
 *	same address.
 *
 * Results:
 *	A pointer to the shared socket or NULL if the socket can't
 *	be opened. An error message is left in interp->result if
 *	interp is not a NULL pointer.
 * 
 * Side effects:
 *	A real socket might be opened.
 *
 *----------------------------------------------------------------------
 */

TnmSnmpSocket *
TnmSnmpOpen(interp, addr)
    Tcl_Interp *interp;
    struct sockaddr_in *addr;
{
    TnmSnmpSocket *sockPtr;
    struct sockaddr_in name;
    int code, socket;
    socklen_t namelen = sizeof(name);

    /*
     * First, check if we can reuse an already open socket.
     */

    for (sockPtr = tnmSnmpSocketList; sockPtr; sockPtr = sockPtr->nextPtr) {
	code = getsockname(sockPtr->sock,
			   (struct sockaddr *) &name, &namelen);
	if (code == 0 && memcmp(&name, addr, namelen) == 0) {
	    sockPtr->refCount++;
	    return sockPtr;
	}
    }

    /*
     * Create a new socket for this transport endpoint and set up
     * a Tcl filehandler to handle incoming messages.
     */

    socket = TnmSocket(AF_INET, SOCK_DGRAM, 0);
    if (socket == TNM_SOCKET_ERROR) {
	if (interp) {
	    Tcl_AppendResult(interp, "can not create socket: ",
			     Tcl_PosixError(interp), (char *) NULL);
	}
        return NULL;
    }

    code = TnmSocketBind(socket, (struct sockaddr *) addr, sizeof(*addr));
    if (code == TNM_SOCKET_ERROR) {
	if (interp) {
	    Tcl_AppendResult(interp, "can not bind socket: ",
			     Tcl_PosixError(interp), (char *) NULL);
	}
	TnmSocketClose(socket);
        return NULL;
    }

#ifdef SO_REUSEADDR
    {
        int on = 1;
	setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));
    }
#endif

    sockPtr = (TnmSnmpSocket *) ckalloc(sizeof(TnmSnmpSocket));
    memset((char *) sockPtr, 0, sizeof(TnmSnmpSocket));
    sockPtr->sock = socket;
    sockPtr->refCount = 1;
    sockPtr->nextPtr = tnmSnmpSocketList;
    tnmSnmpSocketList = sockPtr;
    return sockPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpClose --
 *
 *	This procedure closes a shared SNMP socket. The real socket
 *	is closed only if the reference count is zero. An existing
 *	Tcl file event handler is deleted.
 *
 * Results:
 *	None.
 * 
 * Side effects:
 *	A real socket might be closed.
 *
 *----------------------------------------------------------------------
 */

void
TnmSnmpClose(sockPtr)
    TnmSnmpSocket *sockPtr;
{
    TnmSnmpSocket **sockPtrPtr = &tnmSnmpSocketList;

    if (! tnmSnmpSocketList) return;

    sockPtr->refCount--;
    if (sockPtr->refCount == 0) {
	TnmDeleteSocketHandler(sockPtr->sock);
	TnmSocketClose(sockPtr->sock);
	while (*sockPtrPtr != sockPtr) {
	    sockPtrPtr = &(*sockPtrPtr)->nextPtr;
	}
	*sockPtrPtr = sockPtr->nextPtr;
	ckfree((char *) sockPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpDumpPacket --
 *
 *	This procedure prints a hex dump of a packet. Useful for
 *	debugging this code. The message given in the third 
 *	parameter should be used to identify the received packet.
 *	The fourth parameter identifies the address and port of 
 *	the sender.
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
TnmSnmpDumpPacket(packet, packetlen, from, to)
    u_char *packet;
    int packetlen;
    struct sockaddr_in *from;
    struct sockaddr_in *to;
{
    u_char *cp;
    char buf[80];
    Tcl_DString dst;
    int	len = 0;

    Tcl_DStringInit(&dst);
    if (from) {
	sprintf(buf, "[%s:%u]", 
		inet_ntoa(from->sin_addr), ntohs(from->sin_port));
	Tcl_DStringAppend(&dst, buf, -1);
    }
    Tcl_DStringAppend(&dst, " -> ", -1);
    if (to) {
	sprintf(buf, "[%s:%u]", 
		inet_ntoa(to->sin_addr), ntohs(to->sin_port));
	Tcl_DStringAppend(&dst, buf, -1);
    }
    sprintf(buf, " (%d bytes):\n", packetlen);
    Tcl_DStringAppend(&dst, buf, -1);

    for (cp = packet, len = 0; len < packetlen; cp += 16, len += 16) {
	TnmHexEnc((char *) cp, 
		  (packetlen - len > 16) ? 16 : packetlen - len, buf);
	Tcl_DStringAppend(&dst, buf, -1);
	Tcl_DStringAppend(&dst, "\n", 1);
    }
    TnmWriteMessage(Tcl_DStringValue(&dst));
    Tcl_DStringFree(&dst);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpWait --
 *
 *	This procedure waits for a specified time for an answer. It 
 *	is used to implement synchronous operations.
 *
 * Results:
 *	1 if the socket is readable, otherwise 0.
 * 
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmSnmpWait(ms, flags)
    int ms;
    int flags;
{
    struct timeval wait;
    fd_set readfds;
    int width;
    TnmSnmpSocket *snmpSocket = NULL;

    if (flags & TNM_SNMP_ASYNC) {
	snmpSocket = asyncSocket;
    }
    if (flags & TNM_SNMP_SYNC) {
	snmpSocket = syncSocket;
    }

    if (! snmpSocket) {
	return 0;
    }

    wait.tv_sec  = ms / 1000;
    wait.tv_usec = (ms % 1000) * 1000;
    width = snmpSocket->sock + 1;
    FD_ZERO(&readfds);
    FD_SET(snmpSocket->sock, &readfds);

    return select(width, &readfds, (fd_set *) NULL, (fd_set *) NULL, &wait);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpDelay --
 *
 *	This procedure enforces a small delay if the delay option
 *	of the session is set to value greater than 0.
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
TnmSnmpDelay(session)
    TnmSnmp *session;
{
    static Tcl_Time lastTimeStamp;
    Tcl_Time currentTime;
    int delta, wtime;

    if (session->delay <= 0) return;

    Tcl_GetTime(&currentTime);

    if (lastTimeStamp.sec == 0 && lastTimeStamp.usec == 0) {
	lastTimeStamp = currentTime;
	return;
    }

    delta = (currentTime.sec - lastTimeStamp.sec) * 1000 
	    + (currentTime.usec - lastTimeStamp.usec) / 1000;
    wtime = session->delay - delta;

    if (wtime <= 0) {
	lastTimeStamp = currentTime;
    } else {
	struct timeval timeout;
	timeout.tv_usec = (wtime * 1000) % 1000000;
	timeout.tv_sec = (wtime * 1000) / 1000000;
	select(0, (fd_set *) NULL, (fd_set *) NULL, (fd_set *) NULL, &timeout);
	Tcl_GetTime(&lastTimeStamp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpManagerOpen --
 *
 *	This procedure creates a socket used for normal management
 *	communication.
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
TnmSnmpManagerOpen(interp)
    Tcl_Interp *interp;
{
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    addr.sin_addr.s_addr = INADDR_ANY;

    if (! syncSocket) {
	syncSocket = TnmSnmpOpen(interp, &addr);
	if (! syncSocket) {
	    return TCL_ERROR;
	}
    }
    if (! asyncSocket) {
	asyncSocket = TnmSnmpOpen(interp, &addr);
	if (! asyncSocket) {
	    return TCL_ERROR;
	}
	TnmCreateSocketHandler(asyncSocket->sock, TCL_READABLE, 
			       ResponseProc, (ClientData) interp);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpManagerClose --
 *
 *	This procedure closes the shared manager socket.
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
TnmSnmpManagerClose()
{
    TnmSnmpClose(asyncSocket);
    asyncSocket = NULL;
    TnmSnmpClose(syncSocket);
    syncSocket = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpResponderOpen --
 *
 *	This procedure creates a socket for a command responder session
 *	on a given port. If an agent socket is already created, we close
 *	the socket and open a new one.
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
TnmSnmpResponderOpen(interp, session)
    Tcl_Interp *interp;
    TnmSnmp *session;
{
    if (session->socket) {
	TnmSnmpClose(session->socket);
    }
    session->socket = TnmSnmpOpen(interp, &session->maddr);
    if (! session->socket) {
	return TCL_ERROR;
    }
    TnmCreateSocketHandler(session->socket->sock, TCL_READABLE,
			   AgentProc, (ClientData) session);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpResponderClose --
 *
 *	This procedure closes the socket for incoming requests.
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
TnmSnmpResponderClose(session)
    TnmSnmp *session;
{
    if (session->socket) {
	TnmSnmpClose(session->socket);
    }
    session->socket = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpListenerOpen --
 *
 *	This procedure creates a socket for a notification listener
 *	on a given port. If an socket is already created, we close
 *	the socket and open a new one.
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
TnmSnmpListenerOpen(interp, session)
    Tcl_Interp *interp;
    TnmSnmp *session;
{
#ifdef _TNMUNIXPORT
    if (ntohs(session->maddr.sin_port) == TNM_SNMP_TRAPPORT) {
	return TnmSnmpNmtrapdOpen(interp);
    }
#endif
    
    if (session->socket) {
	TnmSnmpClose(session->socket);
    }
    session->socket = TnmSnmpOpen(interp, &session->maddr);
    if (! session->socket) {
	return TCL_ERROR;
    }
    TnmCreateSocketHandler(session->socket->sock, TCL_READABLE,
			   AgentProc, (ClientData) session);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpListenerClose --
 *
 *	This procedure closes the socket for incoming notifications.
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
TnmSnmpListenerClose(session)
    TnmSnmp *session;
{
#ifdef _TNMUNIXPORT
    if (ntohs(session->maddr.sin_port) == TNM_SNMP_TRAPPORT) {
	TnmSnmpNmtrapdClose();
    }
#endif
    
    if (session->socket) {
	TnmSnmpClose(session->socket);
    }
    session->socket = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpSend --
 *
 *	This procedure sends a packet to the destination address.
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
TnmSnmpSend(interp, session, packet, packetlen, to, flags)
    Tcl_Interp *interp;
    TnmSnmp *session;
    u_char *packet;
    int packetlen;
    struct sockaddr_in *to;
    int flags;
{
    int code, sock;

    if (session->domain == TNM_SNMP_TCP_DOMAIN) {
	/* 1 get suitable tcp socket */
	/* 2. send the packet */
    }

    if (! tnmSnmpSocketList) {
	Tcl_SetResult(interp, "sendto failed: no open socket", TCL_STATIC);
	return TCL_ERROR;
    }

    sock = tnmSnmpSocketList ? tnmSnmpSocketList->sock : -1;
    if (flags & TNM_SNMP_ASYNC && asyncSocket) {
	sock = asyncSocket->sock;
    }
    if (flags & TNM_SNMP_SYNC && syncSocket) {
	sock = syncSocket->sock;
    }

    code = TnmSocketSendTo(sock, (char *) packet, (size_t) packetlen, 0, 
			   (struct sockaddr *) to, sizeof(*to));

    if (code == TNM_SOCKET_ERROR) {
        Tcl_AppendResult(interp, "sendto failed: ", 
			 Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    
    tnmSnmpStats.snmpOutPkts++;

#ifdef TNM_SNMP_BENCH
    Tcl_GetTime(&tnmSnmpBenchMark.sendTime);
    tnmSnmpBenchMark.sendSize = packetlen;
#endif

    if (hexdump) {
	struct sockaddr_in name, *from = NULL;
	int namelen = sizeof(name);

	if (getsockname(sock, (struct sockaddr *) &name, &namelen) == 0) {
	    from = &name;
	}
	
	TnmSnmpDumpPacket(packet, packetlen, from, to);
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpRecv --
 *
 *	This procedure reads incoming responses from the 
 *	manager socket.
 *
 * Results:
 *	A standard Tcl result. The data and the length of the
 *	packet is stored in packet and packetlen. The address of
 *	the sender is stored in from.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmSnmpRecv(interp, packet, packetlen, from, flags)
    Tcl_Interp *interp;
    u_char *packet;
    int	*packetlen;
    struct sockaddr_in *from;
    int flags;
{
    int	sock, fromlen = sizeof(*from);

    if (! tnmSnmpSocketList) {
	Tcl_SetResult(interp, "sendto failed: no open socket", TCL_STATIC);
	return TCL_ERROR;
    }

    sock = tnmSnmpSocketList ? tnmSnmpSocketList->sock : -1;
    if (flags & TNM_SNMP_ASYNC && asyncSocket) {
	sock = asyncSocket->sock;
    }
    if (flags & TNM_SNMP_SYNC && syncSocket) {
	sock = syncSocket->sock;
    }

    *packetlen = TnmSocketRecvFrom(sock, (char *) packet, (size_t) *packetlen, 0,
				   (struct sockaddr *) from, &fromlen);

    if (*packetlen == TNM_SOCKET_ERROR) {
	Tcl_AppendResult(interp, "recvfrom failed: ",
			 Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }

#ifdef TNM_SNMP_BENCH
    Tcl_GetTime(&tnmSnmpBenchMark.recvTime);
    tnmSnmpBenchMark.recvSize = *packetlen;
#endif

    if (hexdump) {
	struct sockaddr_in name, *to = NULL;
	int namelen = sizeof(name);

	if (getsockname(sock, (struct sockaddr *) &name, &namelen) == 0) {
	    to = &name;
	}

	TnmSnmpDumpPacket(packet, *packetlen, from, to);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * AgentRecv --
 *
 *	This procedure reads from the socket to process incoming
 *	SNMP requests.
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
AgentRecv(interp, session, packet, packetlen, from)
    Tcl_Interp	*interp;
    TnmSnmp *session;
    u_char *packet;
    int *packetlen;
    struct sockaddr_in *from;
{
    int	sock = session->socket->sock, fromlen = sizeof(*from);

    if (! session->socket) {
	Tcl_SetResult(interp, "recvfrom failed: no agent socket", TCL_STATIC);
	return TCL_ERROR;
    }

    *packetlen = TnmSocketRecvFrom(sock, (char *) packet, (size_t) *packetlen, 0,
				   (struct sockaddr *) from, &fromlen);

    if (*packetlen == TNM_SOCKET_ERROR) {
	Tcl_AppendResult(interp, "recvfrom failed: ",
			 Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }

    if (hexdump) {
	struct sockaddr_in name, *to = NULL;
	int namelen = sizeof(name);

	if (getsockname(sock, (struct sockaddr *) &name, &namelen) == 0) {
	    to = &name;
	}

	TnmSnmpDumpPacket(packet, *packetlen, from, to);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpTimeoutProc --
 *
 *	This procedure is called from the event dispatcher whenever
 *	a timeout occurs so that we can retransmit packets.
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
TnmSnmpTimeoutProc(clientData)
    ClientData clientData;
{
    TnmSnmpRequest *request = (TnmSnmpRequest *) clientData;
    TnmSnmp *session = request->session;
    Tcl_Interp *interp = request->interp;

    if (request->sends < (1 + session->retries)) {
	
	/* 
	 * Reinstall TimerHandler for this request and retransmit
	 * this request (keeping the original oid).
	 */
	
#ifdef TNM_SNMPv2U
	if (session->version == TNM_SNMPv2U && session->qos & USEC_QOS_AUTH) {
	    TnmSnmpUsecAuth(session, request->packet, request->packetlen);
	}
#endif
	TnmSnmpDelay(session);
	TnmSnmpSend(interp, session, request->packet, request->packetlen, 
		    &session->maddr, TNM_SNMP_ASYNC);
#ifdef TNM_SNMP_BENCH
	if (request->stats.sendSize == 0) {
	    request->stats.sendSize = tnmSnmpBenchMark.sendSize;
	    request->stats.sendTime = tnmSnmpBenchMark.sendTime;
	}
#endif
        request->sends++;
	request->timer = Tcl_CreateTimerHandler(
			(session->timeout * 1000) / (session->retries + 1),
			TnmSnmpTimeoutProc, (ClientData) request);

    } else {

	/*
	 * # of retransmissions reached: Evaluate the callback to
	 * notify the application and delete this request. We fake
	 * an empty pdu structure to conform to the callback 
	 * conventions.
	 */
    
	TnmSnmpPdu _pdu;
	TnmSnmpPdu *pdu = &_pdu;

	memset((char *) pdu, 0, sizeof(TnmSnmpPdu));
	pdu->requestId = request->id;
	pdu->errorStatus = TNM_SNMP_NORESPONSE;
	Tcl_DStringInit(&pdu->varbind);

	Tcl_Preserve((ClientData) request);
	Tcl_Preserve((ClientData) session);
	TnmSnmpDeleteRequest(request);
	if (request->proc) {
	    (request->proc) (session, pdu, request->clientData);
	}
	Tcl_Release((ClientData) session);
	Tcl_Release((ClientData) request);
	Tcl_ResetResult(interp);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ResponseProc --
 *
 *	This procedure is called from the event dispatcher whenever
 *	a response to a management request is received.
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
ResponseProc(clientData, mask)
    ClientData	clientData;
    int mask;
{
    Tcl_Interp *interp = (Tcl_Interp *) clientData;
    u_char packet[TNM_SNMP_MAXSIZE];
    int code, packetlen = TNM_SNMP_MAXSIZE;
    struct sockaddr_in from;

    Tcl_ResetResult(interp);
    code = TnmSnmpRecv(interp, packet, &packetlen, &from, TNM_SNMP_ASYNC);
    if (code != TCL_OK) return;

    code = TnmSnmpDecode(interp, packet, packetlen, &from, 
			 NULL, NULL, NULL, NULL);
    if (code == TCL_ERROR) {
	Tcl_AddErrorInfo(interp, "\n    (snmp response event)");
	Tcl_BackgroundError(interp);
    }
    if (code == TCL_CONTINUE && hexdump) {
	TnmWriteMessage(interp->result);
	TnmWriteMessage("\n");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * AgentProc --
 *
 *	This procedure is called from the event dispatcher whenever
 *	a request for the SNMP agent entity is received.
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
AgentProc(clientData, mask)
    ClientData clientData;
    int mask;
{
    TnmSnmp *session = (TnmSnmp *) clientData;
    Tcl_Interp *interp = session->interp;
    u_char packet[TNM_SNMP_MAXSIZE];
    int code, packetlen = TNM_SNMP_MAXSIZE;
    struct sockaddr_in from;

    if (! interp) return;

    Tcl_ResetResult(interp);
    code = AgentRecv(interp, session, packet, &packetlen, &from);
    if (code != TCL_OK) return;
    
    code = TnmSnmpDecode(interp, packet, packetlen, &from, 
			 NULL, NULL, NULL, NULL);
    if (code == TCL_ERROR) {
	Tcl_AddErrorInfo(interp, "\n    (snmp agent event)");
	Tcl_BackgroundError(interp);
    }
    if (code == TCL_CONTINUE && hexdump) {
	TnmWriteMessage(interp->result);
	TnmWriteMessage("\n");
    }
}
