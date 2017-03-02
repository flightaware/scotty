/*
 * tnmNetdb.c --
 *
 *	This file contains the source of the netdb command that
 *	provides access to local network configuration information.
 *	It is basically just a wrapper around the C interface defined 
 *	in netdb.h. This implementation is supposed to be thread-safe.
 *
 * Copyright (c) 1995-1996 Technical University of Braunschweig.
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

#include <rpc/rpc.h>

/*
 * Some machines have no rpcent structure. Here is definition that
 * seems to work in this cases.
 */

#ifndef HAVE_RPCENT
struct rpcent {
    char *r_name;	/* name of server for this rpc program */
    char **r_aliases;	/* alias list */
    int r_number;	/* rpc program number */
};
#endif

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
LookupFailed		(Tcl_Interp *interp, Tcl_Obj *objPtr);

static int
GetIpAddr		(Tcl_Interp *interp, Tcl_Obj *objPtr,
				     unsigned long *addr);
static int
GetIpMask		(Tcl_Interp *interp, Tcl_Obj *objPtr,
				     unsigned long *mask);
static int
NetdbHosts		(Tcl_Interp *interp, 
				     int objc, Tcl_Obj *CONST objv[]);
static int
NetdbIp			(Tcl_Interp *interp, 
				     int objc, Tcl_Obj *CONST objv[]);
static int
NetdbNetworks		(Tcl_Interp *interp, 
				     int objc, Tcl_Obj *CONST objv[]);
static int
NetdbProtocols		(Tcl_Interp *interp, 
				     int objc, Tcl_Obj *CONST objv[]);
static int
NetdbServices		(Tcl_Interp *interp, 
				     int objc, Tcl_Obj *CONST objv[]);
static int
NetdbSunrpcs		(Tcl_Interp *interp, 
				     int objc, Tcl_Obj *CONST objv[]);


/*
 *----------------------------------------------------------------------
 *
 * LookupFailed --
 *
 *	This procedure handles errors by leaving an error message
 *	in the Tcl interpreter.
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
LookupFailed(Tcl_Interp *interp, Tcl_Obj *objPtr)
{
    Tcl_AppendResult(interp, "can not lookup \"",
		     Tcl_GetStringFromObj(objPtr, NULL),
		     "\"", (char *) NULL);
}

/*
 *----------------------------------------------------------------------
 *
 * GetIpAddr --
 *
 *	This procedure converts a string containing a network address
 *	into an unsigned long value.
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
GetIpAddr(Tcl_Interp *interp, Tcl_Obj *objPtr, unsigned long *addr)
{
    unsigned long m;
    char *arg = Tcl_GetStringFromObj(objPtr, NULL);

    if (TnmValidateIpAddress(interp, arg) != TCL_OK) {
    errorExit:
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			       "invalid IP address \"",
			       arg, "\"", (char *) NULL);
	return TCL_ERROR;
    }

    m = inet_addr(arg);
    if ((m == -1) && (strcmp(arg, "255.255.255.255") != 0)) {
	goto errorExit;
    }

    *addr = ntohl(m);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * GetIpMask --
 *
 *	This procedure converts a string containing a network mask
 *	into an unsigned long value.
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
GetIpMask(Tcl_Interp *interp, Tcl_Obj *objPtr, unsigned long *mask)
{
    unsigned long m;
    char *arg = Tcl_GetStringFromObj(objPtr, NULL);

    if (TnmValidateIpAddress(interp, arg) != TCL_OK) {
    errorExit:
	Tcl_ResetResult(interp);
	Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
			       "invalid IP address mask \"",
			       arg, "\"", (char *) NULL);
	return TCL_ERROR;
    }

    m = inet_addr(arg);
    if ((m == -1) && (strcmp(arg, "255.255.255.255") != 0)) {
	goto errorExit;
    }

    *mask = ntohl(m);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NetdbHosts --
 *
 *	This procedure is invoked to process the "netdb hosts" command.
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

static int
NetdbHosts(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    struct sockaddr_in addr;
    char *name;
    int result;

    enum commands { cmdAddress, cmdAliases, cmdName } cmd;

    static CONST char *cmdTable[] = {
	"address", "aliases", "name", (char *) NULL
    };

    /*
     * First, process the "netdb hosts" command option.
     */

    if (objc == 2) {
#ifdef HAVE_GETHOSTENT
	struct hostent *host;
	struct in_addr *paddr;
	Tcl_Obj *listPtr, *elemPtr;

	listPtr = Tcl_GetObjResult(interp);
	sethostent(0);
	while ((host = gethostent())) {	
	    paddr = (struct in_addr *) *host->h_addr_list++;
	    elemPtr = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(interp, elemPtr,
				     Tcl_NewStringObj(host->h_name, -1));
	    Tcl_ListObjAppendElement(interp, elemPtr,
				     Tcl_NewStringObj(inet_ntoa(*paddr), -1));
	    Tcl_ListObjAppendElement(interp, listPtr, elemPtr);
	}
	endhostent();
#endif
	return TCL_OK;
    }

    /*
     * Process any queries for the "netdb hosts" command.
     */

    result = Tcl_GetIndexFromObj(interp, objv[2], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }
    
    switch (cmd) {
    case cmdAliases:
    case cmdName:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 3, objv, "address");
	    return TCL_ERROR;
	}
	name = Tcl_GetStringFromObj(objv[3], NULL);
	if (TnmValidateIpAddress(interp, name) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (TnmSetIPAddress(interp, name, &addr) != TCL_OK) {
	    return TCL_ERROR;
	}
	name = TnmGetIPName(interp, &addr);
	if (! name) {
	    return TCL_ERROR;
	}
	if (cmd == cmdName) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), name, -1);
	} else {
	    int i;
	    Tcl_Obj *listPtr = Tcl_GetObjResult(interp);
	    struct hostent *host = gethostbyaddr((char *) &addr.sin_addr, 
						 4, AF_INET);
	    if (! host) {
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
				       "unknown address \"", 
				       Tcl_GetStringFromObj(objv[3], NULL),
				       "\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    for (i = 0; host->h_aliases[i]; i++) {
		Tcl_ListObjAppendElement(interp, listPtr, 
				 Tcl_NewStringObj(host->h_aliases[i], -1));
	    }
	}
	break;
    case cmdAddress:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 3, objv, "name");
	    return TCL_ERROR;
	}
	name = Tcl_GetStringFromObj(objv[3], NULL);
	if (TnmValidateIpHostName(interp, name) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (TnmSetIPAddress(interp, name, &addr) != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_SetStringObj(Tcl_GetObjResult(interp), 
			 inet_ntoa(addr.sin_addr), -1);
	break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NetdbIp --
 *
 *	This procedure is invoked to process the "netdb ip" command.
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

static int
NetdbIp(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    int result;

    enum commands {
	cmdApply, cmdBroadcast, cmdClass, cmdCompare, cmdRange
    } cmd;

    static CONST char *cmdTable[] = {
	"apply", "broadcast", "class", "compare", "range", (char *) NULL
    };

    if (objc == 2) {
	Tcl_WrongNumArgs(interp, 2, objv, "option arg");
	return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[2], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }

    switch (cmd) {
    case cmdApply: {
	unsigned long addr, mask;
	struct in_addr ipaddr;
	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 3, objv, "address mask");
	    return TCL_ERROR;
	}
	if (GetIpAddr(interp, objv[3], &addr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (GetIpMask(interp, objv[4], &mask) != TCL_OK) {
	    return TCL_ERROR;
	}
	ipaddr.s_addr = htonl(addr & mask);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), inet_ntoa(ipaddr), -1);
	break;
    }
    case cmdBroadcast: {
	unsigned long addr, mask;
	struct in_addr ipaddr;
	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 3, objv, "address mask");
	    return TCL_ERROR;
	}
	if (GetIpAddr(interp, objv[3], &addr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (GetIpMask(interp, objv[4], &mask) != TCL_OK) {
	    return TCL_ERROR;
	}
	ipaddr.s_addr = htonl(addr | ~mask);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), inet_ntoa(ipaddr), -1);
	break;
    }

    case cmdClass: {
	unsigned long addr;
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 3, objv, "address");
	    return TCL_ERROR;
	}
	if (GetIpAddr(interp, objv[3], &addr) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((addr >> 24) == 127) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), "loopback", -1);
	} else if (IN_CLASSA(addr)) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), "A", 1);
	} else if (IN_CLASSB(addr)) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), "B", 1);
	} else if (IN_CLASSC(addr)) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), "C", 1);
	} else if (IN_CLASSD(addr)) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), "D", 1);
	} else {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), "unknown IP class", -1);
	    return TCL_ERROR;
	}
	break;

    }

    case cmdCompare: {
	unsigned long maskA, maskB;
	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 3, objv, "mask mask");
	    return TCL_ERROR;
	}
	if (GetIpMask(interp, objv[3], &maskA) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (GetIpMask(interp, objv[4], &maskB) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (maskA < maskB) {
	    Tcl_SetIntObj(Tcl_GetObjResult(interp), -1);
	} else if (maskA > maskB) {
	    Tcl_SetIntObj(Tcl_GetObjResult(interp), 1);
	} else {
	    Tcl_SetIntObj(Tcl_GetObjResult(interp), 0);
	}
	break;
    }

    case cmdRange: {
	unsigned long net, mask;
	struct in_addr ipaddr;
	Tcl_Obj *listPtr;
	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 3, objv, "address mask");
	    return TCL_ERROR;
	}
	if (GetIpAddr(interp, objv[3], &net) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (GetIpMask(interp, objv[4], &mask) != TCL_OK) {
	    return TCL_ERROR;
	}
	listPtr = Tcl_GetObjResult(interp);
	for (ipaddr.s_addr = net + 1;
	     ipaddr.s_addr < net + ~mask; ipaddr.s_addr++) {
	    ipaddr.s_addr = htonl(ipaddr.s_addr);
	    Tcl_ListObjAppendElement(interp, listPtr, 
				     Tcl_NewStringObj(inet_ntoa(ipaddr), -1));
	    ipaddr.s_addr = ntohl(ipaddr.s_addr);
	}
	break;
    }
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NetdbNetworks --
 *
 *	This procedure is invoked to process the "netdb networks" command.
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

static int
NetdbNetworks(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    struct netent *net;
    unsigned long addr;
    struct in_addr ipaddr;
    int result;

    enum commands { cmdAddress, cmdAliases, cmdName } cmd;

    static CONST char *cmdTable[] = {
	"address", "aliases", "name", (char *) NULL
    };

    /*
     * First, process the "netdb networks" command option.
     */

    if (objc == 2) {
#ifdef HAVE_GETNETENT
	Tcl_Obj *listPtr, *elemPtr;

	listPtr = Tcl_GetObjResult(interp);
	setnetent(0);
	while ((net = getnetent())) {
	    while (net->n_net && ! ((net->n_net >> 24) & 0xff)) {
		net->n_net <<= 8;
	    }
	    ipaddr.s_addr = htonl(net->n_net);
	    elemPtr = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(interp, elemPtr,
				     Tcl_NewStringObj(net->n_name, -1));
	    Tcl_ListObjAppendElement(interp, elemPtr,
				     Tcl_NewStringObj(inet_ntoa(ipaddr), -1));
	    Tcl_ListObjAppendElement(interp, listPtr, elemPtr);
	}
	endnetent();
#endif
	return TCL_OK;
    }

    /*
     * Process any queries for the "netdb networks" command.
     */

    result = Tcl_GetIndexFromObj(interp, objv[2], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }
    
    switch (cmd) {
    case cmdName:
    case cmdAliases:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 3, objv, "address");
	    return TCL_ERROR;
	}
	if (GetIpAddr(interp, objv[3], &addr) != TCL_OK) {
	    return TCL_ERROR;
	}
	/* GetIpAddr() returns a host-format value: */
	/* addr = htonl(addr); */
	while (addr && ! (addr & 0xff)) {
	    addr >>= 8;
	}
	net = getnetbyaddr(addr, AF_INET);
	if (! net) {
	    LookupFailed(interp, objv[3]);
	    return TCL_ERROR;
	}
	if (cmd == cmdName) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), net->n_name, -1);
	} else {
	    int i;
	    Tcl_Obj *listPtr = Tcl_GetObjResult(interp);
	    for (i = 0; net->n_aliases[i]; i++) {
		Tcl_ListObjAppendElement(interp, listPtr, 
				 Tcl_NewStringObj(net->n_aliases[i], -1));
	    }
	}
	break;
    case cmdAddress:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 3, objv, "name");
	    return TCL_ERROR;
	}
	net = getnetbyname(Tcl_GetStringFromObj(objv[3], NULL));
	if (! net) {
	    LookupFailed(interp, objv[3]);
	    return TCL_ERROR;
	}
	while (net->n_net && ! ((net->n_net >> 24) & 0xff)) {
	    net->n_net <<= 8;
	}
	ipaddr.s_addr = net->n_net;
	Tcl_SetStringObj(Tcl_GetObjResult(interp), inet_ntoa(ipaddr), -1);
	break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NetdbProtocols --
 *
 *	This procedure is invoked to process the "netdb protocols" command.
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

static int
NetdbProtocols(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    struct protoent *proto;
    int result, num;

    enum commands { cmdAliases, cmdName, cmdNumber } cmd;

    static CONST char *cmdTable[] = {
	"aliases", "name", "number", (char *) NULL
    };

    /*
     * First, process the "netdb protocols" command option.
     */

    if (objc == 2) {
#ifdef HAVE_GETPROTOENT
	Tcl_Obj *listPtr, *elemPtr;

	listPtr = Tcl_GetObjResult(interp);
	setprotoent(0);
	while ((proto = getprotoent())) {	
	    elemPtr = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(interp, elemPtr,
				     Tcl_NewStringObj(proto->p_name, -1));
	    Tcl_ListObjAppendElement(interp, elemPtr,
				     Tcl_NewIntObj(proto->p_proto));
	    Tcl_ListObjAppendElement(interp, listPtr, elemPtr);
	}
	endprotoent();
#endif
	return TCL_OK;
    }
    
    /*
     * Process any queries for the "netdb protocols" command.
     */

    result = Tcl_GetIndexFromObj(interp, objv[2], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }
    
    switch (cmd) {
    case cmdName:
    case cmdAliases:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 3, objv, "number");
	    return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(interp, objv[3], &num) != TCL_OK) {
	    return TCL_ERROR;
	}
	if ((proto = getprotobynumber(num)) == NULL) {
	    LookupFailed(interp, objv[3]);
	    return TCL_ERROR;
	}
	if (cmd == cmdName) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), proto->p_name, -1);
	} else {
	    int i;
	    Tcl_Obj *listPtr = Tcl_GetObjResult(interp);
	    for (i = 0; proto->p_aliases[i]; i++) {
		Tcl_ListObjAppendElement(interp, listPtr, 
				 Tcl_NewStringObj(proto->p_aliases[i], -1));
	    }
	}
	break;
    case cmdNumber:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 3, objv, "name");
	    return TCL_ERROR;
	}
	if ((proto = getprotobyname(Tcl_GetStringFromObj(objv[3], NULL)))
	    == NULL) {
	    LookupFailed(interp, objv[3]);
	    return TCL_ERROR;
	}
	Tcl_SetIntObj(Tcl_GetObjResult(interp), proto->p_proto);
	break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NetdbServices --
 *
 *	This procedure is invoked to process the "netdb services" command.
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

static int
NetdbServices(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    struct sockaddr_in addr;
    int result, port;
    char *name;

    enum commands { cmdAliases, cmdName, cmdNumber } cmd;

    static CONST char *cmdTable[] = {
	"aliases", "name", "number", (char *) NULL
    };

    /*
     * First, process the "netdb services" command option.
     */

    if (objc == 2) {
#ifdef HAVE_GETSERVENT
	struct servent *serv;
	Tcl_Obj *listPtr, *elemPtr;

	listPtr = Tcl_GetObjResult(interp);
	setservent(0);
	while ((serv = getservent())) {
	    /*
	     * Skip over all service entries that use protocols other
	     * than udp and tcp as we do not handle lookups for those
	     * protocols.
	     */
	    if (strcmp(serv->s_proto, "udp") != 0
		&& strcmp(serv->s_proto, "tcp") != 0) {
		continue;
	    }
	    elemPtr = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(interp, elemPtr,
				     Tcl_NewStringObj(serv->s_name, -1));
	    Tcl_ListObjAppendElement(interp, elemPtr,
		     Tcl_NewIntObj(ntohs((unsigned short) serv->s_port)));
	    Tcl_ListObjAppendElement(interp, elemPtr,
				     Tcl_NewStringObj(serv->s_proto, -1));
	    Tcl_ListObjAppendElement(interp, listPtr, elemPtr);
	}
	endservent();
#endif
	return TCL_OK;
    }

    /*
     * Process any queries for the "netdb services" command.
     */

    result = Tcl_GetIndexFromObj(interp, objv[2], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }

    switch (cmd) {
    case cmdAliases:
    case cmdName:
	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 3, objv, "number protocol");
	    return TCL_ERROR;
	}
	if (TnmGetUnsignedFromObj(interp, objv[3], &port) != TCL_OK) {
	    return TCL_ERROR;
	}
	addr.sin_port = htons((unsigned short) port);
	name = TnmGetIPPort(interp, Tcl_GetStringFromObj(objv[4],NULL), &addr);
	if (! name) {
	    return TCL_ERROR;
	}
	if (cmd == cmdName) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), name, -1);
	} else {
	    int i;
	    Tcl_Obj *listPtr;
	    struct servent *serv;
	    serv = getservbyport(addr.sin_port, 
				 Tcl_GetStringFromObj(objv[4], NULL));
	    if (! serv) {
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp),
				       "unknown port \"", 
				       Tcl_GetStringFromObj(objv[3], NULL),
				       " ",
				       Tcl_GetStringFromObj(objv[4], NULL),
				       "\"", (char *) NULL);
		return TCL_ERROR;
	    }
	    listPtr = Tcl_GetObjResult(interp);
	    for (i = 0; serv->s_aliases[i]; i++) {
		Tcl_ListObjAppendElement(interp, listPtr, 
				 Tcl_NewStringObj(serv->s_aliases[i], -1));
	    }
	}
	break;
    case cmdNumber:
	if (objc != 5) {
	    Tcl_WrongNumArgs(interp, 3, objv, "name protocol");
	    return TCL_ERROR;
	}
	if (TnmSetIPPort(interp, Tcl_GetStringFromObj(objv[4],NULL),
			 Tcl_GetStringFromObj(objv[3],NULL), &addr)
	    != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_SetIntObj(Tcl_GetObjResult(interp), ntohs(addr.sin_port));
	break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NetdbSunrpcs --
 *
 *	This procedure is invoked to process the "netdb sunrpcs" command.
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

static int
NetdbSunrpcs(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    struct rpcent *rpc;
    int num, result;

    enum commands { cmdAliases, cmdName, cmdNumber } cmd;

    static CONST char *cmdTable[] = {
	"aliases", "name", "number", (char *) NULL
    };

    /*
     * First, process the "netdb sunrpcs" command option.
     */

    if (objc == 2) {
#ifdef HAVE_GETRPCENT
	Tcl_Obj *listPtr, *elemPtr;

	listPtr = Tcl_GetObjResult(interp);
	setrpcent(0);
	while ((rpc = (struct rpcent *) getrpcent())) {	
	    elemPtr = Tcl_NewListObj(0, NULL);
	    Tcl_ListObjAppendElement(interp, elemPtr,
				     Tcl_NewStringObj(rpc->r_name, -1));
	    Tcl_ListObjAppendElement(interp, elemPtr,
				     Tcl_NewIntObj(rpc->r_number));
	    Tcl_ListObjAppendElement(interp, listPtr, elemPtr);
	}
	endrpcent();
#endif
	return TCL_OK;
    }

    /*
     * Process any queries for the "netdb sunrpcs" command.
     */

    result = Tcl_GetIndexFromObj(interp, objv[2], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }
    
    switch (cmd) {
    case cmdAliases:
    case cmdName:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 3, objv, "number");
	    return TCL_ERROR;
	}
	if (TnmGetUnsignedFromObj(interp, objv[3], &num) != TCL_OK) {
	    return TCL_ERROR;
	}
	rpc = (struct rpcent *) getrpcbynumber(num);
	if (rpc == NULL) {
	    LookupFailed(interp, objv[3]);
	    return TCL_ERROR;
	}
	if (cmd == cmdName) {
	    Tcl_SetStringObj(Tcl_GetObjResult(interp), rpc->r_name, -1);
	} else {
	    int i;
	    Tcl_Obj *listPtr = Tcl_GetObjResult(interp);
	    for (i = 0; rpc->r_aliases[i]; i++) {
		Tcl_ListObjAppendElement(interp, listPtr, 
				 Tcl_NewStringObj(rpc->r_aliases[i], -1));
	    }
	}
	break;
    case cmdNumber:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 3, objv, "name");
	    return TCL_ERROR;
	}
	rpc = (struct rpcent *) getrpcbyname(
	    Tcl_GetStringFromObj(objv[3],NULL));
	if (rpc == NULL) {
	    LookupFailed(interp, objv[3]);
	    return TCL_ERROR;
	}
	Tcl_SetIntObj(Tcl_GetObjResult(interp), rpc->r_number);
	break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_NetdbCmd --
 *
 *	This procedure is invoked to process the "netdb" command.
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
Tnm_NetdbObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    int result;

    enum commands {
	cmdHosts, cmdIp, cmdNetworks, cmdProtocols, cmdServices, cmdSunrpcs
    } cmd;

    static CONST char* cmdTable[] = {
	"hosts", "ip", "networks", "protocols", "services", "sunrpcs",
	(char *) NULL
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option query ?arg arg ...?");
	return TCL_ERROR;
    }

    result = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
				 "option", TCL_EXACT, (int *) &cmd);
    if (result != TCL_OK) {
	return result;
    }

    switch (cmd) {
    case cmdHosts:
	result = NetdbHosts(interp, objc, objv);;
	break;
    case cmdIp:
	result = NetdbIp(interp, objc, objv);
	break;
    case cmdNetworks:
	result = NetdbNetworks(interp, objc, objv);
	break;
    case cmdProtocols:
	result = NetdbProtocols(interp, objc, objv);
	break;
    case cmdServices:
	result = NetdbServices(interp, objc, objv);
	break;
    case cmdSunrpcs:
	result = NetdbSunrpcs(interp, objc, objv);
	break;
    }

    return result;
}

