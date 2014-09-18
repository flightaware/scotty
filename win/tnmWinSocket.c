/*
 * tnmWinSocket.c --
 *
 *	Windows specific socket functions. These functions call the
 *	socket functions as defined in the Windows Socket API version
 *	1.1. Error codes are moved from the Windows Socket library
 *	to the Tcl library.
 *
 * Copyright (c) 1996-1997 University of Twente. 
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmInt.h"
#include "tnmPort.h"

#ifndef USE_TCL_STUBS
	/*
 * TclPro does not seem to have tclIntPlatDecls.h.
 */ 
	EXTERN void TclWinConvertWSAError (DWORD errCode);
#else
	/*
	 * This internal header is needed for TclWinConvertWSAError() :-(
	 */
	#include <TclPort.h>
#endif 
 
/*
 * The following structure is used to keep track of Tcl channels
 * opened on sockets for the purpose of getting events from the Tcl
 * channel driver.
 */

typedef struct SocketHandler {
    int sd;
    Tcl_Channel channel;
    TnmSocketProc *proc;
    ClientData clientData;
    struct SocketHandler *nextPtr;
} SocketHandler;

static SocketHandler *socketHandlerList = NULL;

int
TnmSocket(domain, type, protocol)
    int domain;
    int type;
    int protocol;
{
    int s = socket(domain, type, protocol);
    if (s == INVALID_SOCKET) {
	TclWinConvertWSAError(WSAGetLastError());
	return TNM_SOCKET_ERROR;
    }
    return s;
}

int
TnmSocketBind(s, name, namelen)
    int s;
    struct sockaddr *name;
    int namelen;
{
    int e = bind(s, name, namelen);
    if (e == SOCKET_ERROR) {
	TclWinConvertWSAError(WSAGetLastError());
    }
    return (e == SOCKET_ERROR) ? TNM_SOCKET_ERROR : 0;
}

int
TnmSocketSendTo(s, buf, len, flags, to, tolen)
    int s;
    char *buf;
    int len;
    int flags;
    struct sockaddr *to;
    int tolen;
{
    int n = sendto(s, buf, len, flags, to, tolen);
    if (n == SOCKET_ERROR) {
	TclWinConvertWSAError(WSAGetLastError());
    }
    return (n == SOCKET_ERROR) ? TNM_SOCKET_ERROR : n;
}

int
TnmSocketRecvFrom(s, buf, len, flags, from, fromlen)
    int s;
    char *buf;
    int len;
    int flags;
    struct sockaddr *from;
    int *fromlen;
{
    int n = recvfrom(s, buf, len, flags, from, fromlen);

    /*
     * At least the Windows NT 3.51 Windows Socket implementation
     * does not allow to use recvfrom on a connected socket. This
     * is against the Windows Socket specification 1.1. Anyway, we
     * hack around this problem by calling recv() and getpeername()
     * if we got an WSAEISCONN error.  */

    if (n == SOCKET_ERROR && WSAGetLastError() == WSAEISCONN) {
	n = getpeername(s, from, fromlen);
	if (n != SOCKET_ERROR) {
	    n = recv(s, buf, len, flags);
	}
    }

    if (n == SOCKET_ERROR) {
	TclWinConvertWSAError(WSAGetLastError());
    }
    return (n == SOCKET_ERROR) ? TNM_SOCKET_ERROR : n;
}

int TnmSocketClose(s)
    int s;
{
    int e = closesocket(s);
    if (e == INVALID_SOCKET) {
	TclWinConvertWSAError(WSAGetLastError());
    }
    return (e == INVALID_SOCKET) ? TNM_SOCKET_ERROR : 0;
}

void
TnmCreateSocketHandler(sock, mask, proc, clientData)
    int sock;
    int mask;
    TnmSocketProc *proc;
    ClientData clientData;
{
    SocketHandler *shPtr;

    shPtr = (SocketHandler *) ckalloc(sizeof(SocketHandler));
    shPtr->sd = sock;
    shPtr->channel = Tcl_MakeTcpClientChannel((ClientData) sock);
    shPtr->proc = proc;
    shPtr->clientData = clientData;
    Tcl_RegisterChannel((Tcl_Interp *) NULL, shPtr->channel);
    Tcl_CreateChannelHandler(shPtr->channel, mask, proc, clientData);
    shPtr->nextPtr = socketHandlerList;
    socketHandlerList = shPtr;
}

void
TnmDeleteSocketHandler(sock)
    int sock;
{
    SocketHandler **shPtrPtr;

    for (shPtrPtr = &socketHandlerList; *shPtrPtr; ) {
	if ((*shPtrPtr)->sd == sock) {
	    Tcl_DeleteChannelHandler((*shPtrPtr)->channel,
				     (*shPtrPtr)->proc,
				     (*shPtrPtr)->clientData);
	    Tcl_UnregisterChannel((Tcl_Interp *) NULL, (*shPtrPtr)->channel);
	    *shPtrPtr = (*shPtrPtr)->nextPtr;
	} else {
	  shPtrPtr = &(*shPtrPtr)->nextPtr;
	}
    }
}
