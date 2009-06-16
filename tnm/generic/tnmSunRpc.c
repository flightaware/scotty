/*
 * tnmSunRpc.c --
 *
 *	Extend a Tcl interpreter with a command to access
 *	selected Sun RPC services. This implementation is
 *	supposed to be thread-safe.
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

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpc/pmap_clnt.h>

#include "rstat.h"
#include "pcnfsd.h"

#define PCNFS_QUICK 100
#define PCNFS_SLOW 2000
#define PCNFS_UNSUPPORTED -1

/*
 * Some machines have incompatible definitions for h_addr. This
 * fix was suggested by Erik Schoenfelder.
 */

#undef h_addr
#include "ether.h"
#define h_addr h_addr_list[0]

#include "mount.h"

/*
 * Some machines have no rpcent structure. Here is definition that
 * seems to work in this cases.
 */

#ifndef HAVE_RPCENT
struct rpcent {
        char    *r_name;        /* name of server for this rpc program */
        char    **r_aliases;    /* alias list */
        int     r_number;       /* rpc program number */
};
#endif

/*
 * A structure to keep track of "connections" to etherd's.
 */

typedef struct EtherClient {
    char *name;
    CLIENT *clnt;
    int refcnt;
    etherstat estat;
    struct EtherClient *next;
} EtherClient;

static EtherClient *etherList = NULL;

/*
 * Mutex used to serialize access to static variables in this module.
 * We also protect all RPC calls since the client stubs are usually
 * not thread-safe.
 */

TCL_DECLARE_MUTEX(rpcMutex)

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
SunrpcCreateError	_ANSI_ARGS_((Tcl_Interp *interp));

static void
SunrpcError		_ANSI_ARGS_((Tcl_Interp *interp, int res));

static char*
SunrpcGetHostname	_ANSI_ARGS_((Tcl_Interp *interp, char *address));

static int 
SunrpcOpenEtherd	_ANSI_ARGS_((Tcl_Interp *interp, char *host));

static int
SunrpcCloseEtherd	_ANSI_ARGS_((Tcl_Interp *interp, char *host));

static int
SunrpcEtherd		_ANSI_ARGS_((Tcl_Interp *interp, char *host));

static int
SunrpcRstat		_ANSI_ARGS_((Tcl_Interp *interp, char *host));

static int
SunrpcInfo		_ANSI_ARGS_((Tcl_Interp *interp, char *host));

static int
SunrpcMount		_ANSI_ARGS_((Tcl_Interp *interp, char *host));

static int
SunrpcExports		_ANSI_ARGS_((Tcl_Interp *interp, char *host));

static int
SunrpcProbe		_ANSI_ARGS_((Tcl_Interp *interp, char *host, 
				     unsigned long prognum,
				     unsigned long version, 
				     unsigned protocol));
static int
PcnfsInfo		_ANSI_ARGS_((Tcl_Interp *interp, char *host,
				     char *array));
static int 
PcnfsQueue		_ANSI_ARGS_((Tcl_Interp *interp, char *host, 
				     char *printer, char *array));
static int 
PcnfsList		_ANSI_ARGS_((Tcl_Interp *interp, char *host,
				     char *array));
static int 
PcnfsStatus		_ANSI_ARGS_((Tcl_Interp *interp, char *host, 
				     char *printer, char *array));


/*
 *----------------------------------------------------------------------
 *
 * SunrpcCreateError --
 *
 *	This procedure writes the readable string of an RPC create
 *	error code into the result buffer. The error string is cleaned
 *	up a bit to make it more readable.
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
SunrpcCreateError(interp)
    Tcl_Interp *interp;
{
    char *p = clnt_spcreateerror("");
    if (strncmp(p, ": RPC: ", 7) == 0) p += 7;
    if (isspace(p[strlen(p)-1])) p[strlen(p)-1] = '\0';
    Tcl_SetResult(interp, p, TCL_STATIC);
}

/*
 *----------------------------------------------------------------------
 *
 * SunrpcError --
 *
 *	This procedure writes the readable string of the RPC error
 *	code into the result buffer. The error string is cleaned up a
 *	bit to make it more readable.
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
SunrpcError(interp, res)
    Tcl_Interp *interp;
    int res;
{
    Tcl_Obj *obj;
    char *p = clnt_sperrno(res);
    if (strncmp(p, "RPC: ", 5) == 0) p += 5;
    obj = Tcl_NewStringObj(p, -1);
    Tcl_SetObjResult(interp, obj);
    for (p = Tcl_GetString(obj); *p; p++) {
	*p = tolower(*p);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SunrpcGetHostname --
 *
 *	This procedure makes sure that the given string contains a
 *	host name and not an IP address.
 *
 * Results:
 *	A pointer to a static string containing the host name.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char* 
SunrpcGetHostname(interp, address)
    Tcl_Interp *interp;
    char *address;
{
    struct sockaddr_in addr;

    if (TnmSetIPAddress(interp, address, &addr) != TCL_OK) {
	return NULL;
    }

    return TnmGetIPName(interp, &addr);
}

/*
 *----------------------------------------------------------------------
 *
 * SunrpcOpenEtherd --
 *
 *	This procedure opens a "connection" to an etherd.
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
SunrpcOpenEtherd(interp, host)
    Tcl_Interp *interp;
    char *host;
{
    int dummy;
    CLIENT *clnt;
    EtherClient *p;
    etherstat *res;
    struct sockaddr_in _addr;
    struct sockaddr_in *addr = &_addr;
    struct timeval rpcTimeout;
    int rpcSocket = RPC_ANYSOCK;

    rpcTimeout.tv_sec = 5; rpcTimeout.tv_usec = 0;

    host = SunrpcGetHostname(interp, host);
    if (! host) {
        return TCL_ERROR;
    }

    memset((char *) addr, 0, sizeof(struct sockaddr_in));
    if (TnmSetIPAddress(interp, host, addr) != TCL_OK) {
	return TCL_ERROR;
    }

    for (p = etherList; p != NULL; p = p->next) {
	if (strcmp(host, p->name) == 0) {
	    p->refcnt++;
	    return TCL_OK;
	}
    }

    clnt = (CLIENT *) clntudp_create(addr, ETHERPROG, ETHERVERS, 
				     rpcTimeout, &rpcSocket);
    if (clnt == NULL) {
	Tcl_AppendResult(interp, "can not connect to ", host, (char *) NULL);
        return TCL_ERROR;
    }

    etherproc_on_1(&dummy, clnt);
    res = etherproc_getdata_1(&dummy, clnt);
    if (res == NULL) {
	Tcl_AppendResult(interp, "can not connect to ", host, (char *) NULL);
	return TCL_ERROR;
    }

    p = (EtherClient *) ckalloc(sizeof(EtherClient));
    memset((char *) p, 0, sizeof(EtherClient));
    p->name = ckstrdup(host);
    p->clnt = clnt;
    p->estat = *res;
    p->next = etherList;
    etherList = p;
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SunrpcCloseEtherd --
 *
 *	This procedure closes a "connection" to an etherd.
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
SunrpcCloseEtherd(interp, host)
    Tcl_Interp *interp;
    char *host;
{
    int dummy;
    EtherClient *p;
    EtherClient *q = NULL;

    host = SunrpcGetHostname(interp, host);
    if (! host) {
        return TCL_ERROR;
    }

    for (p = etherList, q = NULL; p != NULL; q = p, p = p->next) {
        if (strcmp(host, p->name) == 0) break;
    }

    if (p) {
	if (p->refcnt) {
	    p->refcnt--;
	    return TCL_OK;
	}
	etherproc_off_1(&dummy, p->clnt);
	if (q == NULL) {
	    etherList = p->next;
	} else {
	    q->next = p->next;
	}
	ckfree(p->name);
	ckfree((char *) p);
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SunrpcEtherd --
 *
 *	This procedure reads network statistics from a "connection" 
 *	to an etherd.
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
SunrpcEtherd(interp, host)
    Tcl_Interp *interp;
    char *host;
{
    int dummy, tdiff, i;
    EtherClient *p;
    etherstat *res;
    char buffer[80];

    host = SunrpcGetHostname(interp, host);
    if (! host) {
        return TCL_ERROR;
    }

    for (p = etherList; p != NULL; p = p->next) {
	if (strcmp(host, p->name) == 0) {
	    res = etherproc_getdata_1(&dummy, p->clnt);
	    if (res == NULL) {
		Tcl_AppendResult(interp, "can not connect to ", host, 
				 (char *) NULL);
		return TCL_ERROR;
	    }
	    tdiff = res->e_time.tv_useconds>p->estat.e_time.tv_useconds ?
		    (res->e_time.tv_useconds - p->estat.e_time.tv_useconds) :
	            (1000000 - p->estat.e_time.tv_useconds - res->e_time.tv_useconds);
	    tdiff = tdiff/1000 + 1000 * 
		    (res->e_time.tv_seconds - p->estat.e_time.tv_seconds);

	    sprintf(buffer,"time TimeTicks %u", tdiff);
	    Tcl_AppendElement(interp, buffer);
	    sprintf(buffer, "bytes Gauge %u", res->e_bytes - p->estat.e_bytes);
	    Tcl_AppendElement(interp, buffer);
	    sprintf(buffer, "packets Gauge %u", 
		    res->e_packets - p->estat.e_packets);
	    Tcl_AppendElement(interp, buffer);
	    sprintf(buffer, "bcast Gauge %u", res->e_bcast - p->estat.e_bcast);
	    Tcl_AppendElement(interp, buffer);

#define SUNRPC_PROTO_ND		0
#define SUNRPC_PROTO_ICMP	1
#define SUNRPC_PROTO_UDP	2
#define SUNRPC_PROTO_TCP	3
#define SUNRPC_PROTO_ARP	4
#define SUNRPC_PROTO_OTHER	5

	    sprintf(buffer, "nd Gauge %u", res->e_proto[SUNRPC_PROTO_ND] 
		    - p->estat.e_proto[SUNRPC_PROTO_ND]);
	    Tcl_AppendElement(interp, buffer);

	    sprintf(buffer, "icmp Gauge %u", res->e_proto[SUNRPC_PROTO_ICMP] 
		    - p->estat.e_proto[SUNRPC_PROTO_ICMP]);
	    Tcl_AppendElement(interp, buffer);

	    sprintf(buffer, "udp Gauge %u", res->e_proto[SUNRPC_PROTO_UDP] 
		    - p->estat.e_proto[SUNRPC_PROTO_UDP]);
	    Tcl_AppendElement(interp, buffer);

	    sprintf(buffer, "tcp Gauge %u", res->e_proto[SUNRPC_PROTO_TCP] 
		    - p->estat.e_proto[SUNRPC_PROTO_TCP]);
	    Tcl_AppendElement(interp, buffer);

	    sprintf(buffer, "arp Gauge %u", res->e_proto[SUNRPC_PROTO_ARP] 
		    - p->estat.e_proto[SUNRPC_PROTO_ARP]);
	    Tcl_AppendElement(interp, buffer);

	    sprintf(buffer, "other Gauge %u", res->e_proto[SUNRPC_PROTO_OTHER] 
		    - p->estat.e_proto[SUNRPC_PROTO_OTHER]);
	    Tcl_AppendElement(interp, buffer);

#define MINPACKETLEN 60
#define MAXPACKETLEN 1514
#define BUCKETLNTH ((MAXPACKETLEN - MINPACKETLEN + NBUCKETS - 1)/NBUCKETS)

	    for (i = 0; i < NBUCKETS; i++) {
		sprintf(buffer, "%d-%d Gauge %u", 
			MINPACKETLEN + i * BUCKETLNTH,
			MINPACKETLEN + (i + 1) * BUCKETLNTH - 1,
			res->e_size[i] - p->estat.e_size[i]);
		Tcl_AppendElement(interp, buffer);
	    }
	    
	    p->estat = *res;
	    return TCL_OK;
	}
    }

    Tcl_AppendResult(interp, "no connection to ", host, (char*) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * SunrpcRstat --
 *
 *	This procedure retrieves status information from the remote
 *	host using the rstat RPC. The result is a list of name type
 *	value triples.
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
SunrpcRstat(interp, host)
    Tcl_Interp *interp;
    char *host;
{
    struct statstime statp;
    struct timeval rpcTimeout;
    enum clnt_stat res;
    CLIENT *clnt;
    struct sockaddr_in _addr;
    struct sockaddr_in *addr = &_addr;
    int rpcSocket = RPC_ANYSOCK;
    char buffer[80];

    rpcTimeout.tv_sec = 5; rpcTimeout.tv_usec = 0;
    
    memset((char *) addr, 0, sizeof(struct sockaddr_in));
    if (TnmSetIPAddress(interp, host, addr) != TCL_OK) {
	return TCL_ERROR;
    }

    clnt = (CLIENT *) clntudp_create(addr, RSTATPROG, RSTATVERS_TIME, 
				     rpcTimeout, &rpcSocket);
    if (! clnt) {
        SunrpcCreateError(interp);
        return TCL_ERROR;
    }
    
    res = clnt_call(clnt, RSTATPROC_STATS, (xdrproc_t) xdr_void, (char*) NULL,
		    (xdrproc_t) xdr_statstime, (char *) &statp, rpcTimeout);
    clnt_destroy(clnt);
    if (res != RPC_SUCCESS) {
	SunrpcError(interp, (int) res);
	return TCL_ERROR;
    }

    sprintf(buffer,"cp_user Counter %d", statp.cp_time[0]);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer,"cp_nice Counter %d", statp.cp_time[1]);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "cp_system Counter %d", statp.cp_time[2]);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "cp_idle Counter %d", statp.cp_time[3]);
    Tcl_AppendElement(interp, buffer);

    sprintf(buffer, "dk_xfer_0 Counter %d", statp.dk_xfer[0]);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "dk_xfer_1 Counter %d", statp.dk_xfer[1]);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "dk_xfer_2 Counter %d", statp.dk_xfer[2]);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "dk_xfer_3 Counter %d", statp.dk_xfer[3]);
    Tcl_AppendElement(interp, buffer);

    sprintf(buffer, "v_pgpgin Counter %d", statp.v_pgpgin);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "v_pgpgout Counter %d", statp.v_pgpgout);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "v_pswpin Counter %d", statp.v_pswpin);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "v_pswpout Counter %d", statp.v_pswpout);
    Tcl_AppendElement(interp, buffer);

    sprintf(buffer, "v_intr Counter %d", statp.v_intr);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "v_swtch Counter %d", statp.v_swtch);
    Tcl_AppendElement(interp, buffer);

    sprintf(buffer, "if_ipackets Counter %d", statp.if_ipackets);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "if_ierrors Counter %d", statp.if_ierrors);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "if_opackets Counter %d", statp.if_opackets);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "if_oerrors Counter %d", statp.if_oerrors);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "if_collisions Counter %d", statp.if_collisions);
    Tcl_AppendElement(interp, buffer);

    sprintf(buffer, "avenrun_0 Gauge %d", statp.avenrun[0]);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "avenrun_1 Gauge %d", statp.avenrun[1]);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "avenrun_2 Gauge %d", statp.avenrun[2]);
    Tcl_AppendElement(interp, buffer);

    sprintf(buffer, "boottime TimeTicks %d", statp.boottime.tv_sec);
    Tcl_AppendElement(interp, buffer);
    sprintf(buffer, "curtime TimeTicks %d", statp.curtime.tv_sec);
    Tcl_AppendElement(interp, buffer);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SunrpcInfo --
 *
 *	This procedure retrieves the list of registered RPC services
 *	on the remote host by querying the portmapper.
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
SunrpcInfo(interp, host)
    Tcl_Interp *interp;
    char *host;
{
    struct sockaddr_in _addr;
    struct sockaddr_in *addr = &_addr;
    struct pmaplist *portmapperlist;

    memset((char *) addr, 0, sizeof(struct sockaddr_in));
    if (TnmSetIPAddress(interp, host, addr) != TCL_OK) {
	return TCL_ERROR;
    }

    portmapperlist = pmap_getmaps(addr);
    if (! portmapperlist) {
	Tcl_AppendResult(interp, "unable to contact portmapper on ", 
			 host, (char *) NULL);
	return TCL_ERROR;
    }
    while (portmapperlist) {
	int prog = portmapperlist->pml_map.pm_prog;
	struct rpcent *re = (struct rpcent *) getrpcbynumber(prog);
	Tcl_Obj *listObj = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(interp, listObj, 
			 TnmNewUnsigned32Obj(portmapperlist->pml_map.pm_prog));
	Tcl_ListObjAppendElement(interp, listObj, 
			 TnmNewUnsigned32Obj(portmapperlist->pml_map.pm_vers));
	Tcl_ListObjAppendElement(interp, listObj, Tcl_NewStringObj(
	    (portmapperlist->pml_map.pm_prot == IPPROTO_UDP)
	    ? "udp" : "tcp", -1));
	Tcl_ListObjAppendElement(interp, listObj, 
			 TnmNewUnsigned32Obj(portmapperlist->pml_map.pm_port));
	Tcl_ListObjAppendElement(interp, listObj, 
			 Tcl_NewStringObj(re ? re->r_name : "(unknown)", -1));

	Tcl_ListObjAppendElement(interp, Tcl_GetObjResult(interp), listObj);
	portmapperlist = portmapperlist->pml_next;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SunrpcMount --
 *
 *	This procedure retrieves the list of mounted NFS file
 *	systems by querying the remote mount daemon.
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
SunrpcMount(interp, host)
    Tcl_Interp *interp;
    char *host;
{
    mountlist ml = NULL;
    struct timeval rpcTimeout;
    enum clnt_stat res;
    CLIENT *clnt;
    struct sockaddr_in _addr;
    struct sockaddr_in *addr = &_addr;
    int rpcSocket = RPC_ANYSOCK;
    Tcl_DString ds;

    rpcTimeout.tv_sec = 5; rpcTimeout.tv_usec = 0;

    memset((char *) addr, 0, sizeof(struct sockaddr_in));
    if (TnmSetIPAddress(interp, host, addr) != TCL_OK) {
	return TCL_ERROR;
    }

    clnt = (CLIENT *) clnttcp_create(addr, MOUNTPROG, MOUNTVERS, 
				     &rpcSocket, 0, 0);
    if (! clnt) {
	SunrpcCreateError(interp);
	return TCL_ERROR;
    }
	
    res = clnt_call(clnt, MOUNTPROC_DUMP, (xdrproc_t) xdr_void, (char *) NULL,
		    (xdrproc_t) xdr_mountlist, (char *) &ml, rpcTimeout);
    clnt_destroy(clnt);
    if (res != RPC_SUCCESS) {
	SunrpcError(interp, (int) res);
	return TCL_ERROR;
    }

    Tcl_DStringInit(&ds);
    for (; ml; ml = ml->ml_next) {
	Tcl_DStringStartSublist(&ds);
	Tcl_DStringAppendElement(&ds, ml->ml_directory);
	Tcl_DStringAppendElement(&ds, ml->ml_hostname);
	Tcl_DStringEndSublist(&ds);
    }
    Tcl_DStringResult(interp, &ds);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SunrpcExports --
 *
 *	This procedure retrieves the list of NFS exported file
 *	systems by querying the remote mount daemon.
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
SunrpcExports(interp, host)
    Tcl_Interp *interp;
    char *host;
{
    exports ex = NULL;
    groups gr;
    struct timeval rpcTimeout;
    enum clnt_stat res;
    CLIENT *clnt;
    struct sockaddr_in _addr;
    struct sockaddr_in *addr = &_addr;
    int rpcSocket = RPC_ANYSOCK;
    Tcl_DString ds;

    rpcTimeout.tv_sec = 5; rpcTimeout.tv_usec = 0;

    memset((char *) addr, 0, sizeof(struct sockaddr_in));
    if (TnmSetIPAddress(interp, host, addr) != TCL_OK) {
	return TCL_ERROR;
    }

    clnt = (CLIENT *) clnttcp_create(addr, MOUNTPROG, MOUNTVERS, 
				     &rpcSocket, 0, 0);
    if (! clnt) {
	SunrpcCreateError(interp);
	return TCL_ERROR;
    }
	
    res = clnt_call(clnt, MOUNTPROC_EXPORT, (xdrproc_t) xdr_void, (char*) NULL,
		    (xdrproc_t) xdr_exports, (char *) &ex, rpcTimeout);
    clnt_destroy(clnt);
    if (res != RPC_SUCCESS) {
	SunrpcError(interp, (int) res);
	return TCL_ERROR;
    }

    Tcl_DStringInit(&ds);
    for (; ex; ex = ex->ex_next) {
	Tcl_DStringStartSublist(&ds);
	Tcl_DStringAppendElement(&ds, ex->ex_dir ? ex->ex_dir : "");
	Tcl_DStringStartSublist(&ds);
	for (gr = ex->ex_groups; gr; gr = gr->gr_next) {
	    Tcl_DStringAppendElement(&ds, gr->gr_name);
	}
	Tcl_DStringEndSublist(&ds);
	Tcl_DStringEndSublist(&ds);
    }
    Tcl_DStringResult(interp, &ds);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SunrpcProbe --
 *
 *	This procedure probes a registered RPC service by calling
 *	procedure 0. This should work with every well written RPC
 *	service since procedure 0 is a testing procedure. This
 *	procedure also measures the round-trip time for this call.
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
SunrpcProbe(interp, host, prognum, version, protocol)
    Tcl_Interp *interp;
    char *host;
    unsigned long prognum;
    unsigned long version;
    unsigned protocol;
{
    struct sockaddr_in _addr;
    struct sockaddr_in *addr = &_addr;
    CLIENT *clnt;
    int rpcSocket = RPC_ANYSOCK;    
    struct timeval rpcTimeout;
    enum clnt_stat res;
    u_short port;
    Tcl_Time tvs, tve;
    int ms;
    char *p;
    Tcl_Obj *obj;

    rpcTimeout.tv_sec = 5; rpcTimeout.tv_usec = 0;

    memset((char *) addr, 0, sizeof(struct sockaddr_in));
    if (TnmSetIPAddress(interp, host, addr) != TCL_OK) {
	return TCL_ERROR;
    }

    if ((protocol != IPPROTO_UDP) && (protocol != IPPROTO_TCP)) {
	Tcl_SetResult(interp, "unknown protocol", TCL_STATIC);
	return TCL_ERROR;
    }

    port = pmap_getport(addr, prognum, version, protocol);
    addr->sin_port = htons(port);
    
    if (protocol == IPPROTO_TCP) {
	clnt = (CLIENT *) clnttcp_create(addr, prognum, version, 
					 &rpcSocket, 0, 0); 
    } else {
	struct timeval wait;
	wait.tv_sec = 1; wait.tv_usec = 0;
	clnt = (CLIENT *) clntudp_create(addr, prognum, version, 
					 wait, &rpcSocket);
    }
    if (clnt == NULL) {
	SunrpcCreateError(interp);
	return TCL_ERROR;
    }

    Tcl_GetTime(&tvs);
    res = clnt_call(clnt, NULLPROC, (xdrproc_t) xdr_void, (char *) NULL, 
		    (xdrproc_t) xdr_void, (char *) NULL, rpcTimeout);
    Tcl_GetTime(&tve);

    clnt_destroy(clnt);

    ms = (tve.sec - tvs.sec) * 1000;
    ms += (tve.usec - tvs.usec) / 1000;

    obj = Tcl_NewIntObj(ms);
    Tcl_ListObjAppendElement(interp, Tcl_GetObjResult(interp), obj);

    p = clnt_sperrno(res);
    if (strncmp(p, "RPC: ", 5) == 0) p += 5;
    obj = Tcl_NewStringObj(p, -1);
    Tcl_ListObjAppendElement(interp, Tcl_GetObjResult(interp), obj);
    for (p = Tcl_GetString(obj); *p; p++) {
	*p = tolower(*p);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PcnfsInfo --
 *
 *	This procedure retrieves information about all available
 *	pcnfs services on a given server.
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
PcnfsInfo(interp, host, array)
    Tcl_Interp *interp;
    char *host;
    char *array;
{
    struct sockaddr_in _addr;
    struct sockaddr_in *addr = &_addr;
    int rpcSocket = RPC_ANYSOCK;
    struct timeval rpcTimeout;
    CLIENT *clnt;
    v2_info_args a;
    v2_info_results *res;
    int i;
    int *fp;
    char buffer[80];
    static char *procs[] = {
	"null", "info", "init", "start", "list",
	"queue", "status", "cancel", "admin",
	"requeue", "hold", "release", "map",
	"auth", "alert"
	};
    
    a.vers = "Sun Microsystems PCNFSD test subsystem V1";
    a.cm = "-";

    rpcTimeout.tv_sec = 5; rpcTimeout.tv_usec = 0;
    
    memset((char *) addr, 0, sizeof(struct sockaddr_in));
    if (TnmSetIPAddress(interp, host, addr) != TCL_OK) {
	return TCL_ERROR;
    }

    clnt = (CLIENT *) clntudp_create(addr, PCNFSDPROG, PCNFSDV2, 
				     rpcTimeout, &rpcSocket);
    if (! clnt) {
	SunrpcCreateError(interp);
	return TCL_ERROR;
    }
    
    res = pcnfsd2_info_2(&a, clnt);
    clnt_destroy(clnt);
    if (res == NULL) {
	SunrpcError(interp, RPC_FAILED);
	return TCL_ERROR;
    }

    Tcl_SetResult(interp, res->vers, TCL_VOLATILE);
    if (! array) {
        return TCL_OK;
    }
    
    fp = res->facilities.facilities_val;
    for (i = 0; i < (int) res->facilities.facilities_len; i++, fp++) {
	char *index = NULL, *value;
	if (i < sizeof(procs)/sizeof(char*)) {
	    index = procs[i];
	} else {
	    sprintf(buffer, "rpc #%d", i);
	    index = buffer;
	}
	switch (*fp) { 
	  case PCNFS_QUICK:
	    value = "fast";
            break;
	  case PCNFS_SLOW:
	    value = "slow";
            break;
	  case PCNFS_UNSUPPORTED:
	    value = "unsupported";
            break;
	  default:
	    sprintf(buffer, "%d", *fp);
	    value = buffer;
        }
	if (!Tcl_SetVar2(interp, array, index, value, TCL_LEAVE_ERR_MSG)) {
	    return TCL_ERROR;
	}
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PcnfsList --
 *
 *	This procedure retrieves the printer queue via pcnfs.
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
PcnfsQueue(interp, host, printer, array)
    Tcl_Interp *interp;
    char *host;
    char *printer;
    char *array;
{
    struct sockaddr_in _addr;
    struct sockaddr_in *addr = &_addr;
    int rpcSocket = RPC_ANYSOCK;
    struct timeval rpcTimeout;
    CLIENT *clnt;
    v2_pr_queue_results *pr_qr;
    v2_pr_queue_args pr_args;
    pr_queue_item *pr_item;
    
    pr_args.pn = printer;
    pr_args.system = host;
    pr_args.user = "doug";
    pr_args.just_mine = FALSE;
    pr_args.cm = "";

    rpcTimeout.tv_sec = 5; rpcTimeout.tv_usec = 0;
    
    memset((char *) addr, 0, sizeof(struct sockaddr_in));
    if (TnmSetIPAddress(interp, host, addr) != TCL_OK) {
	return TCL_ERROR;
    }

    clnt = (CLIENT *) clntudp_create(addr, PCNFSDPROG, PCNFSDV2, 
				     rpcTimeout, &rpcSocket);
    if (! clnt) {
	SunrpcCreateError(interp);
	return TCL_ERROR;
    }
    
    pr_qr = pcnfsd2_pr_queue_2(&pr_args, clnt);
    clnt_destroy(clnt);
    if (pr_qr == NULL) {
	SunrpcError(interp, RPC_FAILED);
	return TCL_ERROR;
    }
    
    switch (pr_qr->stat) {
      case PI_RES_NO_SUCH_PRINTER:
	Tcl_SetResult(interp, "no such printer", TCL_STATIC);
	return TCL_ERROR;
      case PI_RES_FAIL:
	Tcl_SetResult(interp, "failure contacting pcnfsd", TCL_STATIC);
	return TCL_ERROR;
      case PI_RES_OK:
	if (array) {
	    int flags = TCL_APPEND_VALUE|TCL_LIST_ELEMENT|TCL_LEAVE_ERR_MSG;
	    char pos[20];
	    for (pr_item = pr_qr->jobs; pr_item; pr_item = pr_item->pr_next) {
		sprintf(pos, "%d", pr_item->position);
		if (!Tcl_SetVar2(interp, array, pos, "id", flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, pr_item->id, flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, "size", flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, pr_item->size, flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, "status", flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, pr_item->status, flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, "system", flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, pr_item->system, flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, "user", flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, pr_item->user, flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, "file", flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, pr_item->file, flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, "comment", flags)) {
		    return TCL_ERROR;
		}
		if (!Tcl_SetVar2(interp, array, pos, pr_item->cm, flags)) {
		    return TCL_ERROR;
		}
	    }
	}
	Tcl_SetIntObj(Tcl_GetObjResult(interp), pr_qr->qlen);
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PcnfsList --
 *
 *	This procedure retrieves the list of pcnfs printers.
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
PcnfsList(interp, host, array)
    Tcl_Interp *interp;
    char *host;
    char *array;
{
    struct sockaddr_in _addr;
    struct sockaddr_in *addr = &_addr;
    int rpcSocket = RPC_ANYSOCK;
    struct timeval rpcTimeout;
    CLIENT *clnt;
    v2_pr_list_results *pr_ls;
    pr_list_item *pr_item;

    rpcTimeout.tv_sec = 5; rpcTimeout.tv_usec = 0;
    
    memset((char *) addr, 0, sizeof(struct sockaddr_in));
    if (TnmSetIPAddress(interp, host, addr) != TCL_OK) {
	return TCL_ERROR;
    }

    clnt = (CLIENT *) clntudp_create(addr, PCNFSDPROG, PCNFSDV2, 
				     rpcTimeout, &rpcSocket);
    if (! clnt) {
        SunrpcCreateError(interp);
        return TCL_ERROR;
    }
    
    pr_ls = pcnfsd2_pr_list_2(NULL, clnt);
    clnt_destroy(clnt);
    if (pr_ls == NULL) {
	SunrpcError(interp, RPC_FAILED);
	return TCL_ERROR;
    }

    for (pr_item = pr_ls->printers; pr_item; pr_item = pr_item->pr_next) {
	Tcl_AppendElement(interp, pr_item->pn);
	if (array) {
	    int flags = TCL_APPEND_VALUE|TCL_LIST_ELEMENT|TCL_LEAVE_ERR_MSG;
	    if (!Tcl_SetVar2(interp, array, 
			     pr_item->pn, "device", flags)) {
		return TCL_ERROR;
	    }
	    if (!Tcl_SetVar2(interp, array, 
			     pr_item->pn, pr_item->device, flags)) {
		return TCL_ERROR;
	    }
	    if (!Tcl_SetVar2(interp, array, 
			     pr_item->pn, "remote", flags)) {
		return TCL_ERROR;
	    }
	    if (!Tcl_SetVar2(interp, array, 
			     pr_item->pn, pr_item->remhost, flags)) {
		return TCL_ERROR;
	    }
	    if (!Tcl_SetVar2(interp, array, 
			     pr_item->pn, "comment", flags)) {
		return TCL_ERROR;
	    }
	    if (!Tcl_SetVar2(interp, array, 
			     pr_item->pn, pr_item->cm, flags)) {
		return TCL_ERROR;
	    }
	}
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * PcnfsStatus --
 *
 *	This procedure retrieves pcnfs printer status information.
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
PcnfsStatus(interp, host, printer, array)
    Tcl_Interp *interp;
    char *host;
    char *printer;
    char *array;
{
    struct sockaddr_in _addr;
    struct sockaddr_in *addr = &_addr;
    int rpcSocket = RPC_ANYSOCK;
    struct timeval rpcTimeout;
    CLIENT *clnt;
    v2_pr_status_args pr_stat;
    v2_pr_status_results *pr_sr;
    char buffer[80];
    
    pr_stat.pn = printer;
    pr_stat.cm = "";

    rpcTimeout.tv_sec = 5; rpcTimeout.tv_usec = 0;
    
    memset((char *) addr, 0, sizeof(struct sockaddr_in));
    if (TnmSetIPAddress(interp, host, addr) != TCL_OK) {
	return TCL_ERROR;
    }

    clnt = (CLIENT *) clntudp_create(addr, PCNFSDPROG, PCNFSDV2, 
				     rpcTimeout, &rpcSocket);
    if (! clnt) {
	SunrpcCreateError(interp);
	return TCL_ERROR;
    }
    
    pr_sr = pcnfsd2_pr_status_2(&pr_stat, clnt);
    clnt_destroy(clnt);
    if (pr_sr == NULL) {
	SunrpcError(interp, RPC_FAILED);
	return TCL_ERROR;
    }

    switch (pr_sr->stat) {
      case PI_RES_NO_SUCH_PRINTER:
        Tcl_SetResult(interp, "no such printer", TCL_STATIC);
	return TCL_ERROR;
      case PI_RES_FAIL:
        Tcl_SetResult(interp, "failure contacting pcnfsd", TCL_STATIC);
	return TCL_ERROR;
      case PI_RES_OK:
	if (!Tcl_SetVar2(interp, array, "available", 
			 pr_sr->avail ? "1" : "0", TCL_LEAVE_ERR_MSG)) {
	    return TCL_ERROR;
	}
	if (!Tcl_SetVar2(interp, array, "printing", 
			 pr_sr->printing ? "1" : "0", TCL_LEAVE_ERR_MSG)) {
	    return TCL_ERROR;
	}
	sprintf(buffer, "%d", pr_sr->qlen);
	if (!Tcl_SetVar2(interp, array, "queued", 
			 buffer, TCL_LEAVE_ERR_MSG)) {
	    return TCL_ERROR;
	}
	if (!Tcl_SetVar2(interp, array, "operator",
			 pr_sr->needs_operator ? "1" : "0", 
			 TCL_LEAVE_ERR_MSG)) {
	    return TCL_ERROR;
	}
	if (!Tcl_SetVar2(interp, array, "status",
			 pr_sr->status, TCL_LEAVE_ERR_MSG)) {
	    return TCL_ERROR;
	}
	if (!Tcl_SetVar2(interp, array, "comment",
			 pr_sr->cm, TCL_LEAVE_ERR_MSG)) {
	    return TCL_ERROR;
	}
        break;
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_SunrpcObjCmd --
 *
 *	This procedure is invoked to process the "sunrpc" command.
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
Tnm_SunrpcObjCmd(clientData, interp, objc, objv)
    ClientData clientData;
    Tcl_Interp *interp;
    int objc;
    Tcl_Obj *CONST objv[];
{
    int code, program, version, protocol;
    char *host;

    static TnmTable protoTable[] = {
	{ IPPROTO_TCP, "tcp" },
	{ IPPROTO_UDP, "udp" },
	{ 0,		NULL }
    };
    
    enum commands {
	cmdEther, cmdExports, cmdInfo, cmdMount, cmdPcnfs, cmdProbe, cmdStat
    } cmd;

    static CONST char *cmdTable[] = {
	"ether", "exports", "info", "mount", "pcnfs", "probe", "stat", 
	(char *) NULL
    };

    enum etherCommands {
	cmdEtherOpen, cmdEtherClose, cmdEtherStat
    } etherCmd;

    static CONST char *etherCmdTable[] = {
	"open", "close", "stat", (char *) NULL
    };

    enum pcnfsCommands {
	cmdPcnfsInfo, cmdPcnfsList, cmdPcnfsQueue, cmdPcnfsStatus
    } pcnfsCmd;

    static CONST char *pcnfsCmdTable[] = {
	"info", "list", "queue", "status", (char *) NULL
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option host ?args?");
	return TCL_ERROR;
    }

    host = Tcl_GetString(objv[2]);

    code = Tcl_GetIndexFromObj(interp, objv[1], cmdTable,
			       "option", TCL_EXACT, (int *) &cmd);
    if (code != TCL_OK) {
	return TCL_ERROR;
    }

    switch (cmd) {
    case cmdInfo:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "host");
	    return TCL_ERROR;
	}
	Tcl_MutexLock(&rpcMutex);
	code = SunrpcInfo(interp, host);
	Tcl_MutexUnlock(&rpcMutex);
	return code;

    case cmdStat:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "host");
	    return TCL_ERROR;
	}
	Tcl_MutexLock(&rpcMutex);
	code = SunrpcRstat(interp, host);
	Tcl_MutexUnlock(&rpcMutex);
	return code;

    case cmdMount:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "host");
	    return TCL_ERROR;
	}
	Tcl_MutexLock(&rpcMutex);
	code = SunrpcMount(interp, host);
	Tcl_MutexUnlock(&rpcMutex);
	return code;

    case cmdExports:
	if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "host");
	    return TCL_ERROR;
	}
	Tcl_MutexLock(&rpcMutex);
	code = SunrpcExports(interp, host);
	Tcl_MutexUnlock(&rpcMutex);
	return code;

    case cmdPcnfs:
	if (objc < 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "host option");
	    return TCL_ERROR;
	}
	
	code = Tcl_GetIndexFromObj(interp, objv[3], pcnfsCmdTable,
				   "option", TCL_EXACT, (int *) &pcnfsCmd);
	if (code != TCL_OK) {
	    return TCL_ERROR;
	}

	switch (pcnfsCmd) {
	case cmdPcnfsInfo:
	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 2, objv, "host info ?arrayName?");
		return TCL_ERROR;
	    }
	    Tcl_MutexLock(&rpcMutex);
	    code = PcnfsInfo(interp, host,
			     objc == 5 ? Tcl_GetString(objv[4]) : NULL);
	    Tcl_MutexUnlock(&rpcMutex);
	    return code;
	case cmdPcnfsList:
	    if (objc < 4 || objc > 5) {
		Tcl_WrongNumArgs(interp, 2, objv, "host list ?arrayName?");
		return TCL_ERROR;
	    }
	    Tcl_MutexLock(&rpcMutex);
	    code = PcnfsList(interp, host,
			     objc == 5 ? Tcl_GetString(objv[4]) : NULL);
	    Tcl_MutexUnlock(&rpcMutex);
	    return code;
	case cmdPcnfsQueue:
	    if (objc < 5 || objc > 6) {
		Tcl_WrongNumArgs(interp, 2, objv,
				 "host queue printer ?arrayName?");
		return TCL_ERROR;
	    }
	    Tcl_MutexLock(&rpcMutex);
	    code = PcnfsQueue(interp, host, Tcl_GetString(objv[4]),
			      objc == 6 ? Tcl_GetString(objv[5]) : NULL);
	    Tcl_MutexUnlock(&rpcMutex);
	    return code;
	case cmdPcnfsStatus:
	    if (objc != 6) {
		Tcl_WrongNumArgs(interp, 2, objv,
				 "host pcnfs printer arrayName");
		return TCL_ERROR;
	    }
	    Tcl_MutexLock(&rpcMutex);
	    code = PcnfsStatus(interp, host, Tcl_GetString(objv[4]),
			       Tcl_GetString(objv[5]));
	    Tcl_MutexUnlock(&rpcMutex);
	    return code;
	}

    case cmdProbe:
	if (objc != 6) { 
	    Tcl_WrongNumArgs(interp, 2, objv, "host program version protocol");
	    return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(interp, objv[3], &program) != TCL_OK) 
	    return TCL_ERROR;
	if (Tcl_GetIntFromObj(interp, objv[4], &version) != TCL_OK) 
	    return TCL_ERROR;

	protocol = TnmGetTableKey(protoTable, Tcl_GetString(objv[5]));
	if (protocol < 0) {
	    Tcl_AppendResult(interp, "unknown protocol \"",
			     Tcl_GetString(objv[5]), "\": should be ",
			     TnmGetTableValues(protoTable), (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_MutexLock(&rpcMutex);
	code = SunrpcProbe(interp, host, (unsigned long) program,
			   (unsigned long) version, (unsigned) protocol);
	Tcl_MutexUnlock(&rpcMutex);
	return code;

    case cmdEther:

	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "host option");
	    return TCL_ERROR;
	}
	
	code = Tcl_GetIndexFromObj(interp, objv[3], etherCmdTable,
				   "option", TCL_EXACT, (int *) &etherCmd);
	if (code != TCL_OK) {
	    return TCL_ERROR;
	}

	Tcl_MutexLock(&rpcMutex);
	switch (etherCmd) {
	case cmdEtherOpen:
	    code = SunrpcOpenEtherd(interp, host);
	    break;
	case cmdEtherClose:
	    code = SunrpcCloseEtherd(interp, host);
	    break;
	case cmdEtherStat:
	    code = SunrpcEtherd(interp, host);
	    break;
	}
	Tcl_MutexUnlock(&rpcMutex);
	return code;	
    }

    return TCL_OK;
}
