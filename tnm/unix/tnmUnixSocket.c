/*
 * tnmUnixSocket.c --
 *
 *	UNIX specific socket functions. These functions are actually
 *	wrapper functions that do nothing on UNIX systems.
 *
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmInt.h"
#include "tnmPort.h"

#include <fcntl.h>

int
TnmSocket(int domain, int type, int protocol)
{
    int s = socket(domain, type, protocol);
    if (s < 0) {
	return TNM_SOCKET_ERROR;
    }
#ifdef O_NONBLOCK
    fcntl(s, F_SETFL, O_NONBLOCK);
#endif
    return s;
}

int
TnmSocketBind(int s, struct sockaddr *name, socklen_t namelen)
{
    int e = bind(s, name, namelen);
    return (e < 0) ? TNM_SOCKET_ERROR : 0;
}

int
TnmSocketSendTo(int s, char *buf, size_t len, int flags, struct sockaddr *to, socklen_t tolen)
{
    int n = sendto(s, buf, len, flags, to, tolen);
    return (n < 0) ? TNM_SOCKET_ERROR : n;
}

int
TnmSocketRecvFrom(int s, char *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
    int n = recvfrom(s, buf, len, flags, from, fromlen);
    return (n < 0) ? TNM_SOCKET_ERROR : n;
}

int TnmSocketClose(int s)
{
    int e = close(s);
    return (e < 0) ? TNM_SOCKET_ERROR : 0;
}

void
TnmCreateSocketHandler(int sock, int mask, TnmSocketProc *proc, ClientData clientData)
{
    Tcl_CreateFileHandler(sock, mask, proc, clientData);
}

void
TnmDeleteSocketHandler(int sock)
{
    Tcl_DeleteFileHandler(sock);
}


