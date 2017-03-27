/*
 * tnmDns.c --
 *
 *	Extend a tcl command interpreter with a dns command to query
 *	the Internet domain name service. This implementation is
 *	supposed to be thread-safe.
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

#define BIND_8_COMPAT

#include <arpa/nameser.h>
#include <resolv.h>

/*
 * enable debug output of resover routine DnsDoQuery(), DnsHaveQuery()
 * with #define DEBUG_RESOLV
 */

/*
 * Max # of returned hostnames or IP addresses:
 */

#define MAXRESULT	30

/*
 * The default Internet name server port.
 */

#ifndef NAMESERVER_PORT
#define NAMESERVER_PORT	53
#endif

/*
 * Query and reply structure.
 */

typedef struct {
    HEADER qb1;
    char qb2[PACKETSZ];
} querybuf;

/*
 * Selfmade reply structure (private use only).
 */

typedef struct {
    int type;			/* T_A, T_SOA, T_HINFO, T_MX */
    int n;			/* # of results stored */
    union {
	struct in_addr addr[MAXRESULT]; 
	char str[MAXRESULT][256];
    } u;
} a_res;

/*
 * Every Tcl interpreter has an associated DnsControl record. It
 * keeps track of the default settings for this interpreter.
 */

static char tnmDnsControl[] = "tnmDnsControl";

typedef struct DnsControl {
    int retries;		/* Default number of retries. */
    int timeout;		/* Default timeout in seconds. */
    short nscount;		/* Number of name servers. */
    struct sockaddr_in		/* List of default name server */
    nsaddr_list[MAXNS];		/* addresses. */
} DnsControl;

/*
 * The options for the dns command.
 */

enum options { optTimeout, optRetries, optServer };

static TnmTable dnsOptionTable[] = {
    { optTimeout,	"-timeout" },
    { optRetries,	"-retries" },
    { optServer,	"-server" },
    { 0, NULL }
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
AssocDeleteProc	(ClientData clientData, Tcl_Interp *interp);

static void
DnsInit		(DnsControl *control);

static int
DnsGetHostName	(Tcl_Interp *interp, const char *hname);

static void
DnsDoQuery	(char *query_string, int query_type, 
			     a_res *query_result);
static void
DnsHaveQuery	(const char *query_string, int query_type,
			     a_res *query_result, int depth);
static int 
DnsA		(Tcl_Interp *interp, char *hname);

static int
DnsPtr		(Tcl_Interp *interp, const char *ip);

#if 0
static void
DnsCleanHinfo	(char *str);
#endif

static int
DnsHinfo	(Tcl_Interp *interp, const char *hname);

static int 
DnsMx		(Tcl_Interp *interp, const char *hname);

static int 
DnsSoa		(Tcl_Interp *interp, const char *hname);

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
    DnsControl *control = (DnsControl *) clientData;

    if (control) {
	ckfree((char *) control);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DnsInit --
 *
 *	This procedure initializes the resolver parameters.
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
DnsInit(DnsControl *control)
{
    int i;

    _res.retrans = control->timeout;
    _res.retry = control->retries + 1;
    _res.nscount = control->nscount;
    for (i = 0; i < control->nscount; i++) {
	_res.nsaddr_list[i] = control->nsaddr_list[i];
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DnsGetHostName --
 *
 *	This procedure converts the given IP address into the
 *	corresponding host name.
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
DnsGetHostName(Tcl_Interp *interp, const char *hname)
{
    int rc;
    
    rc = DnsPtr(interp, hname);
    if (rc != TCL_OK) {
	Tcl_ResetResult(interp);
        Tcl_AppendResult(interp, "cannot reverse lookup \"",
			 hname, "\"", (char *) NULL);
	return TCL_ERROR;
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DnsDoQuery --
 *
 *	This procedure sends a single DNS query and extracts the
 *	result from the DNS response.
 *
 * Results:
 *	The result is returned in the query_result parameter.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
DnsDoQuery(char *query_string, int query_type, a_res *query_result)
{
    querybuf query, answer;
    char buf[512], lbuf[512], auth_buf[512];
    int i, qlen, alen, llen, nscount, len, nsents, ancount;
    short type, rdlen;
    /* class and ttl are unused, they are just needed for skipping
     *  over the respective field */
    short class;
    long ttl;
    querybuf *q;
    u_char *ptr;
    u_char *eom;

    /*
     * Initialize the query_result. Note, query_result->u.str[0]
     * contains an error-description if we had an error.
     */

#ifdef DEBUG_RESOLV
    fprintf(stderr, "DnsDoQuery: query_type=%d %s\n",
	    query_type, query_string);
#endif

    query_result->type = -1;
    query_result->n = 0;
    for (i = 0; i < sizeof(querybuf); i++) {
	((char *) &query)[i] = ((char *) &answer)[i] = 0;
    }

    /*
     * res_mkquery(op, dname, class, type, data, datalen, newrr, buf, buflen) 
     */
	
    qlen = res_mkquery(QUERY, query_string, C_IN, query_type, 
		       (u_char *) 0, 0, 0,
		       (u_char *) &query, sizeof(query));
    if (qlen <= 0) {
	query_result->n = -1;
	sprintf(query_result->u.str[0], "cannot make query '%s'", query_string);
	return;
    }

    /*
     * res_send(msg, msglen, answer, anslen)
     */

    alen = res_send((u_char *) &query, qlen, 
		    (u_char *) &answer, sizeof (answer));
    if (alen <= 0) {
	query_result->n = -1;
	sprintf (query_result->u.str[0], "cannot send query '%s'; error %d", 
		 query_string, h_errno);
	return;
    }

    /*
     * If there are nameserver entries, only these are for authorative
     * answers of interest:
     */
    
    q = &answer;
    /* TODO: nsents is never used, later we look up q->qb1.nscount again
     *  and store it in nscount, eventually
     */
    nsents = ntohs((unsigned short) q->qb1.nscount);

    if (q->qb1.rcode != 0) {
	if (q->qb1.rcode == 1)
	    strcpy(query_result->u.str[0], "format error");
	else if (q->qb1.rcode == 2)
	    strcpy(query_result->u.str[0], "server failure");
	else if (q->qb1.rcode == 3)
	    strcpy(query_result->u.str[0], "non existent domain");
	else if (q->qb1.rcode == 4)
	    strcpy(query_result->u.str[0], "not implemented");
	else if (q->qb1.rcode == 5)
	    strcpy(query_result->u.str[0], "query refused");
	else
	    sprintf(query_result->u.str[0], "unknown error %d", q->qb1.rcode);
	query_result->type = query_type;
	query_result->n = -1;
	return;
    }

#ifdef DEBUG_RESOLV
    fprintf(stderr, "ancount=%d, nscount=%d, arcount=%d\n",
	    ntohs((unsigned short)q->qb1.ancount),
	    ntohs((unsigned short)q->qb1.nscount),
	    ntohs((unsigned short)q->qb1.arcount));
#endif
    
    ancount = nscount = ntohs((unsigned short) q->qb1.ancount);
    if (! nscount) {
	nscount = ntohs((unsigned short) q->qb1.nscount);
    }
    if (! nscount) {
	nscount = ntohs((unsigned short) q->qb1.arcount);
    }

    /* prevent falling through to authoritative section in some
     *  cases */
    if (!ancount) {
	switch (query_type) {
	case T_CNAME:
	    strcpy(query_result->u.str[0], "no CNAME record");
	    query_result->n = -1;
	    return;
	case T_HINFO:
	    strcpy(query_result->u.str[0], "no HINFO record");
	    query_result->n = -1;
	    return;
	case T_TXT:
	    strcpy(query_result->u.str[0], "no TXT record");
	    query_result->n = -1;
	    return;
	}
    }
    
    /*
     * give some help (seems to be needed for very ole sun-code... 
     */

    eom = (u_char *) &answer + alen;
    *eom++ = 0;
    
    ptr = (u_char *) q->qb2;
    
    /*
     * Skip over question section: [ QNAME , QTYPE , QCLASS ]
     */

    if (q->qb1.qdcount > 0) {
	int rc = dn_skipname(ptr, eom);
	ptr += rc + QFIXEDSZ;
    }

    /*
     * Now we have left a section with:
     *	Answering RR's
     *	Authority RR's
     *	Additional RR's
     */

    /* fprintf(stderr, "** nscount=%d\n", nscount); */

    /* TODO: there is no check, if ptr overruns the buf in between.
     * In fact we run into the authority section in some cases,
     * e.g. HINFO w/o result.
     */
    strcpy(query_result->u.str[0], "no answer");
    for ( ; nscount; nscount--) {

	/*
	 * Every RR looks like: [ NAME, TYPE, CLASS, TTL, RDLENGTH, RDATA ]
	 */

	/*
	 * dn_expand(msg, msglen, comp_dn, exp_dn, length)
	 */
	
	llen = dn_expand((u_char *) q, eom, ptr, lbuf, sizeof(lbuf));
	if (llen < 0) {
	    strcpy(query_result->u.str[0], "dn_expand() of NAME failed");
	    query_result->n = -1;
	    return;
	}
	ptr += llen;

	/*
	 * Fetch TYPE, CLASS, TTL, RDLENGTH:
	 */

	GETSHORT(type, ptr);

#ifdef DEBUG_RESOLV
	fprintf(stderr, "nscount=%d, type=%d\n", nscount, type);
#endif
	  
	/* class and ttl are unused, we just need them to skip over the respective field */
	GETSHORT(class, ptr);
	(void)class;
	GETLONG(ttl, ptr);
	(void)ttl;
	
	GETSHORT(rdlen, ptr);

	if (type == T_NS) {
	    
	    len = dn_expand((u_char *) q, eom, ptr, buf, sizeof(buf));
	    if (len < 0) {
	        strcpy(query_result->u.str[0], "dn_expand() of T_NS RDATA failed");
	        query_result->n = -1;
		return;
	    }
	    ptr += len;

	    if (query_result->type == T_NS || query_result->type == -1) {
		query_result->type = T_NS;
		strcpy(query_result->u.str[query_result->n++], buf);
	    }

	} else if (query_type == T_CNAME && type == T_CNAME) {
	    
	    len = dn_expand((u_char *) q, eom, ptr, buf, sizeof(buf));
	    if (len < 0) {
	        strcpy(query_result->u.str[0], "dn_expand() of T_CNAME RDATA failed");
	        query_result->n = -1;
		return;
	    }
	    ptr += len;

	    if (query_result->type == T_CNAME || query_result->type == -1) {
		query_result->type = T_CNAME;
		strcpy(query_result->u.str[query_result->n++], buf);
	    }

	} else if (type == T_TXT) {
	    
	    len = *ptr++;
		
	    if (query_result->type == T_TXT || query_result->type == -1) {
		query_result->type = T_TXT;
		strncpy(query_result->u.str[query_result->n], (char *) ptr, len);
		query_result->u.str[query_result->n++][len] = '\0';
	    }
	    ptr += len;
		
	} else if (type == T_A) {

	    unsigned long x;
	    GETLONG (x, ptr);
	    if (strcmp(query_string, lbuf) == 0
		|| query_result->type == T_A || query_result->type == -1) {
		query_result->type = T_A;
		query_result->u.addr[query_result->n++].s_addr = ntohl(x);
	    }

	} else if (type == T_SOA) {

	    /*
	     * SOA rdata format is:
	     * [ MNAME, RNAME, SERIAL, REFRESH, RETRY, EXPIRE, MINIMUM ]
	     */

	    len = dn_expand((u_char*) q, eom, ptr, auth_buf, sizeof(auth_buf));
	    if (len < 0) {
	        strcpy(query_result->u.str[0], "dn_expand() of T_SOA MNAME failed");
	        query_result->n = -1;
		return;
	    }
	    ptr += len;
			
	    len = dn_expand((u_char *) q, eom, ptr, buf, sizeof(buf));
	    if (len < 0) {
	        strcpy(query_result->u.str[0], "dn_expand() of T_SOA RNAME failed");
	        query_result->n = -1;
		return;
	    }
	    ptr += len;
			
	    /*
	     * Skip to the end of this rr:
	     */

	    /* TODO: 32/64 pointer arithmetic ok? */
	    ptr += 5 * 4;

	    /* TODO: why check for T_SOA? */
	    if (query_result->type == T_SOA || query_result->type == -1) {
		query_result->type = T_SOA;
		strcpy(query_result->u.str[query_result->n++], auth_buf);
	    }
	    
	} else if (type == T_HINFO) {
	    /* two <character-string>s */
	    if (query_result->type == T_HINFO || query_result->type == -1) {		
		query_result->type = T_HINFO;
		for (i = 2; i; i--) {
		    len = *ptr++;
		    strncpy(query_result->u.str[query_result->n], (char *) ptr, len);
		    query_result->u.str[query_result->n++][len] = '\0';
		    ptr += len;
	        }
	    }
#if 0
	    /* leg20170307: ditched this.  It's not understandable
	      TODO: why len from dn_expand() but then pointer arithmetic with rdlen?
	     * TODO: why a for loop with exactly one iteration when we
	     * need both: CPU and OS string?
	     */
	    for (i = 0; i <= 1; i++) {		/* XXX: ??? */
	        len = dn_expand((u_char *) q, eom, ptr, buf, sizeof(buf));
			if (len < 0) {
	            strcpy(query_result->u.str[0], "dn_expand() of T_HINFO failed");
	            query_result->n = -1;
		    return;
		}
		/* TODO: len/rdlen ptr += rdlen;
		 */
		ptr +=len;

		if (query_result->type == T_HINFO 
		    || query_result->type == -1) {
		    query_result->type = T_HINFO;
		    strcpy(query_result->u.str[query_result->n++], buf);
		}
	    }
#endif
	    
	} else if (type == T_PTR) {

	    len = dn_expand((u_char *) q, eom, ptr, buf, sizeof(buf));
	    if (len < 0) {
	        strcpy(query_result->u.str[0], "dn_expand() of T_PTR failed");
	        query_result->n = -1;
		return;
	    }
	    /* TODO: len/rdlen
	       ptr += rdlen;
	    */
	    ptr += len;
	    
	    if (query_result->type == T_PTR || query_result->type == -1) {
		query_result->type = T_PTR;
		strcpy(query_result->u.str[query_result->n++], buf);
	    }

	} else if (type == T_MX) {

	    unsigned prio;
	    GETSHORT (prio, ptr);
	    
	    len = dn_expand((u_char *) q, eom, ptr, buf, sizeof(buf));
	    if (len < 0) {
	        strcpy(query_result->u.str[0], "dn_expand() of T_MX failed");
		query_result->n = -1;
		return;
	    }
	    ptr += len;

	    if (query_result->type == T_MX || query_result->type == -1) {
		query_result->type = T_MX;
		sprintf(query_result->u.str[query_result->n++], 
			"%s %d", buf, prio);
	    }

	} else {
	    /* don't use this record, skip it */
	    ptr += rdlen;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DnsHaveQuery --
 *
 *	This procedure perfoms a DNS query.
 *
 * Results:
 *	The result is returned in the query_result parameter. If
 *	query_result->n < 0, then the first string contains the 
 *	error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
DnsHaveQuery(const char *query_string, int query_type, a_res *query_result, int depth)
{
    int i;
    a_res res;
    char tmp[256];
    
    query_result->type = -1;
    query_result->n = 0;
    
    if (depth > 1) {
	return;
    }

    int len = strlen(query_string);
    char last = len ? query_string[len - 1] : '\0';

    /*
     * Loop through every domain suffix:
     */
    
    for (i = -1; i < MAXDNSRCH + 1; i++) {

        if (i == -1) {
	    strcpy(tmp, query_string);
	} else if (! _res.dnsrch[i]) {
	    break;
	} else if (last == '.') {
	    break;
	} else {
	    sprintf(tmp, "%s.%s", query_string, _res.dnsrch[i]);
	}
	
	DnsDoQuery(tmp, query_type, &res);
#ifdef DEBUG_RESOLV
	fprintf(stderr, "DnsDoQuery1: res.type=%d, res.n=%d\n", res.type, res.n);
#endif
	if (res.type == query_type && res.n > 0) {
	    *query_result = res;
	    return;
	}

	/*
	 * Check ptr and soa's not recursive:
	 */

	if (query_type == T_SOA || query_type == T_PTR
	    || query_type == T_TXT || query_type == T_HINFO
	    || query_type == T_CNAME) {
	    *query_result = res;
	    return;
	}
    }
    
    /*
     * Seems to be unsuccessful: look for any answer.
     */

    for (i = -1; i < MAXDNSRCH + 1; i++) {

	if (i == -1) {
	    strcpy(tmp, query_string);
	} else if (! _res.dnsrch[i]) {
	    break;
	} else if (last == '.') {
	    break;
	} else {
	    sprintf(tmp, "%s.%s", query_string, _res.dnsrch[i]);
	}
	
	DnsDoQuery(tmp, query_type, &res);
#ifdef DEBUG_RESOLV
	fprintf(stderr, "DnsDoQuery2: res.type=%d, res.n=%d\n", res.type, res.n);
#endif

	if (res.n > 0) {
	    *query_result = res;
	    return;
	}
    }
    
    if (res.n <= 0) {
	*query_result = res;
	return;
    }
    
    return;

#if 0
    /*
     * Here we could ask other hosts ...
     * (but still wrong and not used)
     */
    
    if (res.type == T_SOA) {
	a_res tmpres;
	DnsDoQuery(res.u.str[0], T_A, &tmpres);
	if (tmpres.type != T_A || tmpres.n <= 0) {
	    return;
	}
	_res.nsaddr.sin_addr = tmpres.u.addr[0];
	_res.nscount = 1;
	DnsHaveQuery(query_string, query_type, query_result, depth + 1);
	return;
    }
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * DnsA --
 *
 *	This procedure retrieves the address record (A) for the 
 *	given DNS name.
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
DnsA(Tcl_Interp *interp, char *hname)
{
    a_res res;
    int i;

    if (TnmValidateIpAddress(NULL, hname) == TCL_OK) {
        if (DnsPtr(interp, hname) == TCL_OK) {
	    Tcl_SetResult(interp, hname, TCL_VOLATILE);
	    return TCL_OK;
	} else {
	    return TCL_ERROR;
	}
    }

    if (TnmValidateIpHostName(interp, hname) != TCL_OK) {
	return TCL_ERROR;
    }

    DnsHaveQuery(hname, T_A, &res, 0);

    if (res.n < 0 || res.type != T_A) {
        Tcl_SetResult(interp, res.u.str[0], TCL_VOLATILE);
        return TCL_ERROR;
    }
  
    for (i = 0; i < res.n; i++) {
	Tcl_AppendElement(interp, inet_ntoa(res.u.addr[i]));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DnsPtr --
 *
 *	This procedure retrieves the PTR record for the given DNS 
 *	name. The PTR record contains the fully qualified DNS name.
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
DnsPtr(Tcl_Interp *interp, const char *ip)
{
    a_res res;
    int i, a, b, c, d;
    char tmp[128];

    if (TnmValidateIpAddress(interp, ip) != TCL_OK) {
	return TCL_ERROR;
    }

    if (4 != sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d)) {
	Tcl_AppendResult(interp, "invalid IP address \"", 
			 ip, "\"", (char *) NULL);
	return TCL_ERROR;
    }

    sprintf(tmp, "%d.%d.%d.%d.in-addr.arpa", d, c, b, a);
    DnsHaveQuery(tmp, T_PTR, &res, 0);

    if (res.n < 0 || res.type != T_PTR) {
        Tcl_SetResult(interp, res.u.str[0], TCL_VOLATILE);
        return TCL_ERROR;
    }

    for (i = 0; i < res.n; i++) {
        Tcl_AppendElement(interp, res.u.str[i]);
    }
    return TCL_OK;
}

#if 0
/*
 *----------------------------------------------------------------------
 *
 * DnsCleanHinfo --
 *
 *	This procedure removes all backslash characters from the
 *	given string.
 *
 * Results:
 *	The given string is modified.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
DnsCleanHinfo(char *str)
{
    char *ptr;
    
    while (str && *str) {
	if (*str == '\\') {
	    for (ptr = str; *ptr; ptr++)
		*ptr = *(ptr + 1);
	}
	str++;
    }
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * DnsHinfo --
 *
 *	This procedure retrieves the host info record (HINFO) for
 *	the given DNS hname.
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
DnsHinfo(Tcl_Interp *interp, const char *hname)
{
    a_res res;

    /*
     * If we get a numerical address, convert to a name first. 
     */

    if (TnmValidateIpAddress(NULL, hname) == TCL_OK) {
	if (DnsGetHostName(interp, hname) != TCL_OK) {
	    return TCL_ERROR;
	}
	hname = Tcl_GetStringResult(interp);
    }

    if (TnmValidateIpHostName(interp, hname) != TCL_OK) {
	return TCL_ERROR;
    }

    DnsHaveQuery(hname, T_HINFO, &res, 0);
    Tcl_ResetResult(interp);

    if (res.n < 0 || res.type != T_HINFO) {
        Tcl_SetResult(interp, res.u.str[0], TCL_VOLATILE);
	return TCL_ERROR;
    }
    if (res.n < 2) {
        Tcl_SetResult(interp, "not enough strings in HINFO record", TCL_STATIC);
	return TCL_ERROR;
    }

    Tcl_AppendElement(interp, res.u.str[0]);
    Tcl_AppendElement(interp, res.u.str[1]);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DnsCname --
 *
 *	This procedure retrieves the Cname records for
 *	the DNS domain name given by hname.
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
DnsCname(Tcl_Interp *interp, const char *hname)
{
    a_res res;
    int i;

    /*
     * If we get a numerical address, convert to a name first. 
     */

    if (TnmValidateIpAddress(NULL, hname) == TCL_OK) {
	if (DnsGetHostName(interp, hname) != TCL_OK) {
	    return TCL_ERROR;
	}
	hname = Tcl_GetStringResult(interp);
    }

    if (TnmValidateIpHostName(interp, hname) != TCL_OK) {
	return TCL_ERROR;
    }

    DnsHaveQuery(hname, T_CNAME, &res, 0);
    Tcl_ResetResult(interp);

    if (res.n < 0 || res.type != T_CNAME) {
        Tcl_SetResult(interp, res.u.str[0], TCL_VOLATILE);
	return TCL_ERROR;
    }

    for (i = 0; i < res.n; i++) {
        Tcl_AppendElement(interp, res.u.str[i]);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DnsMx --
 *
 *	This procedure retrieves the mail exchanger record (MX) for
 *	the DNS domain name given by hname.
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
DnsMx(Tcl_Interp *interp, const char *hname)
{
    a_res res;
    int i;

    /*
     * If we get a numerical address, convert to a name first. 
     */

    if (TnmValidateIpAddress(NULL, hname) == TCL_OK) {
	if (DnsGetHostName(interp, hname) != TCL_OK) {
	    return TCL_ERROR;
	}
	hname = Tcl_GetStringResult(interp);
    }

    if (TnmValidateIpHostName(interp, hname) != TCL_OK) {
	return TCL_ERROR;
    }

    DnsHaveQuery(hname, T_MX, &res, 0);
    Tcl_ResetResult(interp);

    if (res.n < 0 || res.type != T_MX) {
        Tcl_SetResult(interp, res.u.str[0], TCL_VOLATILE);
	return TCL_ERROR;
    }

    for (i = 0; i < res.n; i++) {
        Tcl_AppendElement(interp, res.u.str[i]);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DnsNs --
 *
 *	This procedure retrieves the DNS server records (NS) for
 *	the DNS domain name given by hname.
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
DnsNs(Tcl_Interp *interp, const char *hname)
{
    a_res res;
    int i;

    /*
     * If we get a numerical address, convert to a name first. 
     */

    if (TnmValidateIpAddress(NULL, hname) == TCL_OK) {
	if (DnsGetHostName(interp, hname) != TCL_OK) {
	    return TCL_ERROR;
	}
	hname = Tcl_GetStringResult(interp);
    }

    if (TnmValidateIpHostName(interp, hname) != TCL_OK) {
	return TCL_ERROR;
    }

    DnsHaveQuery(hname, T_NS, &res, 0);
    Tcl_ResetResult(interp);

    if (res.n < 0 || res.type != T_NS) {
        Tcl_SetResult(interp, res.u.str[0], TCL_VOLATILE);
	return TCL_ERROR;
    }

    for (i = 0; i < res.n; i++) {
        Tcl_AppendElement(interp, res.u.str[i]);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DnsTxt --
 *
 *	This procedure retrieves the TXT records for
 *	the DNS domain name given by hname.
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
DnsTxt(Tcl_Interp *interp, const char *hname)
{
    a_res res;
    int i;

    /*
     * If we get a numerical address, convert to a name first. 
     */

    if (TnmValidateIpAddress(NULL, hname) == TCL_OK) {
	if (DnsGetHostName(interp, hname) != TCL_OK) {
	    return TCL_ERROR;
	}
	hname = Tcl_GetStringResult(interp);
    }

    if (TnmValidateIpHostName(interp, hname) != TCL_OK) {
	return TCL_ERROR;
    }

    DnsHaveQuery(hname, T_TXT, &res, 0);
    Tcl_ResetResult(interp);

    if (res.n < 0 || res.type != T_TXT) {
        Tcl_SetResult(interp, res.u.str[0], TCL_VOLATILE);
	return TCL_ERROR;
    }

    for (i = 0; i < res.n; i++) {
        Tcl_AppendElement(interp, res.u.str[i]);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DnsSoa --
 *
 *	This procedure retrieves the start of authority (SOA) for
 *	the DNS domain given by hname.
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
DnsSoa(Tcl_Interp *interp, const char *hname)
{
    a_res res;
    int i;

    /*
     * If we get a numerical address, convert to a name first. 
     * I guess, this makes absolutly no sense for a soa. :-)
     */

    if (TnmValidateIpAddress(NULL, hname) == TCL_OK) {
	if (DnsGetHostName(interp, hname) != TCL_OK) {
	    return TCL_ERROR;
	}	
	hname = Tcl_GetStringResult(interp);
    }

    if (TnmValidateIpHostName(interp, hname) != TCL_OK) {
	return TCL_ERROR;
    }

    DnsHaveQuery(hname, T_SOA, &res, 0);
    Tcl_ResetResult(interp);

    if (res.n < 0 || res.type != T_SOA) {
        Tcl_SetResult(interp, res.u.str[0], TCL_VOLATILE);
	return TCL_ERROR;
    }

    for (i = 0; i < res.n; i++) {
        Tcl_AppendElement(interp, res.u.str[i]);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_DnsObjCmd --
 *
 *	This procedure is invoked to process the "dns" command.
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
Tnm_DnsObjCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    int x, i, code;
    char *arg;
    DnsControl dnsParams;		/* Actually used DNS parameters. */

    DnsControl *control = (DnsControl *) 
	Tcl_GetAssocData(interp, tnmDnsControl, NULL);

    enum commands {
	cmdAddress, cmdCname, cmdHinfo, cmdMx, cmdName, cmdNs, cmdSoa, cmdTxt
    } cmd;

    static CONST char *cmdTable[] = {
	"address", "cname", "hinfo", "mx", "name", "ns", "soa", "txt", (char *) NULL	
    };

    if (! control) {
	control = (DnsControl *) ckalloc(sizeof(DnsControl));

	/*
	 * Copy the current settings into the control record so that
	 * we can store this configuration for each interpreter.
	 */

	control->retries = 2;
	control->timeout = 2;
	control->nscount = _res.nscount;
	for (i = 0; i < _res.nscount; i++) {
	    control->nsaddr_list[i] = _res.nsaddr_list[i];
	}
	if (control->nscount == 0
	    || (control->nscount == 1
		&& control->nsaddr_list[0].sin_addr.s_addr 
		== htonl(INADDR_ANY))) {
	    control->nscount = 1;
	    control->nsaddr_list[0].sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	}
	Tcl_SetAssocData(interp, tnmDnsControl, AssocDeleteProc, 
			 (ClientData) control);
	DnsInit(control);
    }

    dnsParams.retries = -1;
    dnsParams.timeout = -1;
    dnsParams.nscount = -1;
    for (i = 0; i < MAXNS; i++) {
#ifdef HAVE_SA_LEN
	dnsParams.nsaddr_list[i].sin_len = sizeof(struct sockaddr_in);
#endif
	dnsParams.nsaddr_list[i].sin_family = AF_INET;
	dnsParams.nsaddr_list[i].sin_addr.s_addr = htonl(INADDR_ANY);
	dnsParams.nsaddr_list[i].sin_port = htons(NAMESERVER_PORT);
    }

    if (objc < 2) {
      wrongArgs:
	Tcl_WrongNumArgs(interp, 1, objv,
			 "?-timeout t? ?-retries r? ?-server hosts? option arg");
	return TCL_ERROR;
    }
    
    /* 
     * Parse the options:
     */

    for (x = 1; x < objc; x++) {
	code = TnmGetTableKeyFromObj(interp, dnsOptionTable,
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
	    code = TnmGetPositiveFromObj(interp, objv[++x],
					 &dnsParams.timeout);
	    if (code != TCL_OK) {
	        return TCL_ERROR;
	    }
	    break;
	case optRetries:
	    if (x == objc-1) {
		Tcl_SetIntObj(Tcl_GetObjResult(interp), control->retries);
		return TCL_OK;
	    }
	    code = TnmGetUnsignedFromObj(interp, objv[++x],
					 &dnsParams.retries);
	    if (code != TCL_OK) {
	        return TCL_ERROR;
	    }
	    break;
	case optServer: {
	    int elemc;
	    Tcl_Obj **elemv;
	    if (x == objc-1) {
		for (i = 0; i < control->nscount; i++) {
		    Tcl_AppendElement(interp, 
				  inet_ntoa(control->nsaddr_list[i].sin_addr));
		}
		return TCL_OK;
	    }
	    code = Tcl_ListObjGetElements(interp, objv[++x], &elemc, &elemv);
	    if (code != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (elemc > MAXNS) {
		Tcl_SetResult(interp, 
		      "number of DNS server addresses exceeds resolver limit",
			      TCL_STATIC);
		return TCL_ERROR;
	    }
	    if (elemc == 0) {
		Tcl_SetResult(interp, 
			      "at least one DNS server address required",
			      TCL_STATIC);
		return TCL_ERROR;
	    }
	    dnsParams.nscount = elemc;
	    for (i = 0; i < dnsParams.nscount; i++) {
		code = TnmSetIPAddress(interp,
				       Tcl_GetStringFromObj(elemv[i], NULL),
				       &dnsParams.nsaddr_list[i]);
		if (code != TCL_OK) {
		    return TCL_ERROR;
		}
	    }
	    break;
	    }
	}
    }

    if (x == objc) {
	if (dnsParams.retries >= 0) {
            control->retries = dnsParams.retries;
        }
        if (dnsParams.timeout > 0) {
            control->timeout = dnsParams.timeout;
        }
	if (dnsParams.nscount > 0) {
	    control->nscount = dnsParams.nscount;
	    for (i = 0; i < dnsParams.nscount; i++) {
		control->nsaddr_list[i] = dnsParams.nsaddr_list[i];
	    }
	}
        return TCL_OK;
    }

    if (x != objc-2) {
        goto wrongArgs;
    }

    if (dnsParams.timeout < 0) {
	dnsParams.timeout = control->timeout;
    }
    if (dnsParams.retries < 0) {
	dnsParams.retries = control->retries;
    }
    if (dnsParams.nscount < 0) {
	dnsParams.nscount = control->nscount;
	for (i = 0; i < control->nscount; i++) {
	    dnsParams.nsaddr_list[i] = control->nsaddr_list[i];
	}
    }
    DnsInit(&dnsParams);

    /*
     * Get the query type and do the DNS lookup.
     */

    code = Tcl_GetIndexFromObj(interp, objv[x], cmdTable,
                               "option", TCL_EXACT, (int *) &cmd);
    if (code != TCL_OK) {
        return code;
    }

    arg = Tcl_GetStringFromObj(objv[objc-1], NULL);
    switch (cmd) {
    case cmdAddress:
	return DnsA(interp, arg);
    case cmdCname:
	return DnsCname(interp, arg);
    case cmdHinfo:
	return DnsHinfo(interp, arg);
    case cmdMx:
	return DnsMx(interp, arg);
    case cmdNs:
	return DnsNs(interp, arg);
    case cmdName:
	return DnsPtr(interp, arg);
    case cmdSoa:
	return DnsSoa(interp, arg);
    case cmdTxt:
	return DnsTxt(interp, arg);
    }

    return TCL_OK;
}

