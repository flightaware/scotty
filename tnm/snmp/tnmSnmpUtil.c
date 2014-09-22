/*
 * tnmSnmpUtil.c --
 *
 *	This file contains some utilities to manipulate request lists
 *	that keep track of outstanding requests. It also contains a
 *	wrapper around the MD5 digest algorithm and some code to handle
 *	SNMP bindings.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmSnmpUtil.c,v 1.2 2008/02/13 16:44:05 karl Exp $
 */

#include "tnmSnmp.h"
#include "tnmMib.h"
#include "tnmMD5.h"

/* 
 * Flag that controls hexdump. See the watch command for its use.
 */

extern int hexdump;

/*
 * The queue of active and waiting asynchronous requests.
 */

static TnmSnmpRequest *queueHead = NULL;

/*
 * The following tables are used to map SNMP version numbers,
 * application types, SNMP errors to strings.
 */

TnmTable tnmSnmpVersionTable[] = {
    { TNM_SNMPv1,	"SNMPv1" },
    { TNM_SNMPv2C,	"SNMPv2c" },
#ifdef TNM_SNMPv2U
    { TNM_SNMPv2U,	"SNMPv2u" },
#endif
    { TNM_SNMPv3,	"SNMPv3" },
    { 0, NULL }
};

TnmTable tnmSnmpApplTable[] = {
    { TNM_SNMP_GENERATOR,	"generator" },
    { TNM_SNMP_RESPONDER,	"responder" },
    { TNM_SNMP_NOTIFIER,	"notifier" },
    { TNM_SNMP_LISTENER,	"listener" },
    { 0, NULL }
};

TnmTable tnmSnmpDomainTable[] = {
    { TNM_SNMP_UDP_DOMAIN,	"udp" },
    { TNM_SNMP_TCP_DOMAIN,	"tcp" },
    { 0, NULL }
};

/*
 * The table of SNMP error codes is based on RFC 1905, section 3.
 * It also includes some error codes and names that are specific
 * to the Tnm extension (e.g. noResponse).
 */

TnmTable tnmSnmpErrorTable[] =
{
    { TNM_SNMP_NOERROR,			"noError" },
    { TNM_SNMP_TOOBIG,			"tooBig" },
    { TNM_SNMP_NOSUCHNAME,		"noSuchName" },
    { TNM_SNMP_BADVALUE,		"badValue" },
    { TNM_SNMP_READONLY,		"readOnly" },
    { TNM_SNMP_GENERR,			"genErr" },
    { TNM_SNMP_NOACCESS,		"noAccess" },
    { TNM_SNMP_WRONGTYPE,		"wrongType" },
    { TNM_SNMP_WRONGLENGTH,		"wrongLength" },
    { TNM_SNMP_WRONGENCODING,		"wrongEncoding" },
    { TNM_SNMP_WRONGVALUE,		"wrongValue" },
    { TNM_SNMP_NOCREATION,		"noCreation" },
    { TNM_SNMP_INCONSISTENTVALUE,	"inconsistentValue" },
    { TNM_SNMP_RESOURCEUNAVAILABLE,	"resourceUnavailable" },
    { TNM_SNMP_COMMITFAILED,		"commitFailed" },
    { TNM_SNMP_UNDOFAILED,		"undoFailed" },
    { TNM_SNMP_AUTHORIZATIONERROR,	"authorizationError" },
    { TNM_SNMP_NOTWRITABLE,		"notWritable" },
    { TNM_SNMP_INCONSISTENTNAME,	"inconsistentName" },
    { TNM_SNMP_ENDOFWALK,		"endOfWalk" },
    { TNM_SNMP_NORESPONSE,		"noResponse" },
    { 0, NULL }
};

/*
 * The following table is used to convert event names to event token.
 */

TnmTable tnmSnmpEventTable[] =
{
    { TNM_SNMP_GET_EVENT,	"get" },
    { TNM_SNMP_SET_EVENT,	"set" },
    { TNM_SNMP_CREATE_EVENT,	"create" },
    { TNM_SNMP_TRAP_EVENT,	"trap" },
    { TNM_SNMP_INFORM_EVENT,	"inform" },
    { TNM_SNMP_CHECK_EVENT,	"check" },
    { TNM_SNMP_COMMIT_EVENT,	"commit" },
    { TNM_SNMP_ROLLBACK_EVENT,	"rollback" },
    { TNM_SNMP_BEGIN_EVENT,	"begin" },
    { TNM_SNMP_END_EVENT,	"end" },
    { TNM_SNMP_SEND_EVENT,	"send" },
    { TNM_SNMP_RECV_EVENT,	"recv" },
    { 0, NULL }
};

#ifdef TNM_SNMPv2U
/*
 * The following structures and procedures are used to keep a list of
 * keys that were computed with the USEC password to key algorithm.
 * This list is needed so that identical session handles don't suffer
 * from repeated slow computations.
 */

typedef struct KeyCacheElem {
    char *password;
    u_char agentID[USEC_MAX_AGENTID];
    u_char authKey[TNM_MD5_SIZE];
    struct KeyCacheElem *nextPtr;
} KeyCacheElem;

static KeyCacheElem *firstKeyCacheElem = NULL;

/*
 * The following structure is used to keep a cache of known agentID,
 * agentBoots and agentTime values. New session handles are initialized
 * using this cache to avoid repeated report PDUs.
 */

typedef struct AgentIDCacheElem {
    struct sockaddr_in addr;
    u_char agentID[USEC_MAX_AGENTID];
    u_int agentBoots;
    u_int agentTime;
    struct AgentIDCacheElem *nextPtr;
} AgentIDCacheElem;

static AgentIDCacheElem *firstAgentIDCacheElem = NULL;
#endif

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
SessionDestroyProc	(char *memPtr);

static void
RequestDestroyProc	(char *memPtr);

#ifdef TNM_SNMPv2U
static int
FindAuthKey		(TnmSnmp *session);

static void
SaveAuthKey		(TnmSnmp *session);

static void 
MakeAuthKey		(TnmSnmp *session);

static int
FindAgentID		(TnmSnmp *session);

static void
SaveAgentID		(TnmSnmp *session);
#endif


/*
 *----------------------------------------------------------------------
 *
 * SessionDestroyProc --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a request at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the request is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
SessionDestroyProc(char *memPtr)
{
    TnmSnmp *session = (TnmSnmp *) memPtr;
    
    Tcl_DecrRefCount(session->community);
    Tcl_DecrRefCount(session->context);
    Tcl_DecrRefCount(session->user);
    Tcl_DecrRefCount(session->engineID);
    if (session->usmAuthKey) {
	Tcl_DecrRefCount(session->usmAuthKey);
    }
    if (session->usmPrivKey) {
	Tcl_DecrRefCount(session->usmPrivKey);
    }
    if (session->authPassWord) {
	Tcl_DecrRefCount(session->authPassWord);
    }
    if (session->privPassWord) {
	Tcl_DecrRefCount(session->privPassWord);
    }
    if (session->tagList) {
	Tcl_DecrRefCount(session->tagList);
    }
    
    while (session->bindPtr) {
	TnmSnmpBinding *bindPtr = session->bindPtr;	
	session->bindPtr = bindPtr->nextPtr;
	if (bindPtr->command) {
	    ckfree(bindPtr->command);
	}
	ckfree((char *) bindPtr);
    }

    if (session->type == TNM_SNMP_LISTENER) {
	TnmSnmpListenerClose(session);
    }
    if (session->type == TNM_SNMP_RESPONDER) {
	TnmSnmpResponderClose(session);
    }
    
    ckfree((char *) session);
}

/*
 *----------------------------------------------------------------------
 *
 * RequestDestroyProc --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a request at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the request is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
RequestDestroyProc(char *memPtr)
{
    TnmSnmpRequest *request = (TnmSnmpRequest *) memPtr;

    ckfree((char *) request);
}
#ifdef TNM_SNMPv2U

/*
 *----------------------------------------------------------------------
 *
 * FindAuthKey --
 *
 *	This procedure searches for an already computed key in the 
 *	list of cached authentication keys.
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
FindAuthKey(TnmSnmp *session)
{
    KeyCacheElem *keyPtr;
    
    for (keyPtr = firstKeyCacheElem; keyPtr; keyPtr = keyPtr->nextPtr) {
	if ((strcmp(session->password, keyPtr->password) == 0) 
	    && (memcmp(session->agentID, keyPtr->agentID, 
		       USEC_MAX_AGENTID) == 0)) {
	    memcpy(session->authKey, keyPtr->authKey, TNM_MD5_SIZE);
	    return TCL_OK;
	}
    }

    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * SaveAuthKey --
 *
 *	This procedure adds a new computed key to the internal
 *	list of cached authentication keys.
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
SaveAuthKey(TnmSnmp *session)
{
    KeyCacheElem *keyPtr;
    
    keyPtr = (KeyCacheElem *) ckalloc(sizeof(KeyCacheElem));
    keyPtr->password = ckstrdup(session->password);
    memcpy(keyPtr->agentID, session->agentID, USEC_MAX_AGENTID);
    memcpy(keyPtr->authKey, session->authKey, TNM_MD5_SIZE);
    keyPtr->nextPtr = firstKeyCacheElem;
    firstKeyCacheElem = keyPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * MakeAuthKey --
 *
 *	This procedure converts a 0 terminated password string into 
 *	a 16 byte MD5 key. This is a slighly modified version taken 
 *	from RFC 1910. We keep a cache of all computed passwords to 
 *	make repeated lookups faster.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new authentication key is computed and stored in the
 *	SNMP session structure.
 *
 *----------------------------------------------------------------------
 */

static void 
MakeAuthKey(TnmSnmp *session)
{
    MD5_CTX MD;
    u_char *cp, password_buf[64];
    u_long password_index = 0;
    u_long count = 0, i;
    int found, valid = 0, passwordlen = strlen((char *) session->password);

    /*
     * We simply return if we do not have a password or if the 
     * agentID is zero (which is an initialized agentID value.
     */

    for (i = 0; i < USEC_MAX_AGENTID; i++) {
	if (session->agentID[i] != 0) {
	    valid++;
	    break;
	}
    }
    if (! valid || session->password == NULL) {
	return;
    }

    found = FindAuthKey(session);
    if (found != TCL_OK) {

	TnmMD5Init(&MD);   /* initialize MD5 */

	/* loop until we've done 1 Megabyte */
	while (count < 1048576) {
	    cp = password_buf;
	    for(i = 0; i < 64; i++) {
		*cp++ = session->password[password_index++ % passwordlen];
		/*
		 * Take the next byte of the password, wrapping to the
		 * beginning of the password as necessary.
		 */
	    }
	    
	    TnmMD5Update(&MD, password_buf, 64);
	    
	    /*
	     * 1048576 is divisible by 64, so the last MDupdate will be
	     * aligned as well.
	     */
	    count += 64;
	}
	
	TnmMD5Final(password_buf, &MD);
	memcpy(password_buf+TNM_MD5_SIZE, (char *) session->agentID, 
	       USEC_MAX_AGENTID);
	memcpy(password_buf+TNM_MD5_SIZE+USEC_MAX_AGENTID, password_buf, 
	       TNM_MD5_SIZE);
	TnmMD5Init(&MD);   /* initialize MD5 */
	TnmMD5Update(&MD, password_buf, 
		      TNM_MD5_SIZE+USEC_MAX_AGENTID+TNM_MD5_SIZE);
	TnmMD5Final(session->authKey, &MD);
	SaveAuthKey(session);
    }

    if (hexdump) {
	fprintf(stderr, "MD5 key: ");
	for (i = 0; i < TNM_MD5_SIZE; i++) {
	    fprintf(stderr, "%02x ", session->authKey[i]);
	}
	fprintf(stderr, "\n");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FindAgentID --
 *
 *	This procedure searches for an already known agentID in the list
 *	of cached agentIDs.
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
FindAgentID(TnmSnmp *session)
{
    AgentIDCacheElem *idPtr;
    
    for (idPtr = firstAgentIDCacheElem; idPtr; idPtr = idPtr->nextPtr) {
	if (memcmp(&session->maddr, &idPtr->addr, 
		   sizeof(struct sockaddr_in)) == 0) {
	    memcpy(session->agentID, idPtr->agentID, USEC_MAX_AGENTID);
	    session->agentBoots = idPtr->agentBoots;
	    session->agentTime = idPtr->agentTime;
	    return TCL_OK;
	}
    }

    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * SaveAgentID --
 *
 *	This procedure adds a new agentID to the internal list of 
 *	cached agentIDs. It also caches the agentBoots and agentTime
 *	values.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
SaveAgentID(TnmSnmp *session)
{
    AgentIDCacheElem *idPtr;
    
    for (idPtr = firstAgentIDCacheElem; idPtr; idPtr = idPtr->nextPtr) {
	if (memcmp(&session->maddr, &idPtr->addr,
		   sizeof(struct sockaddr_in)) == 0) {
	    memcpy(idPtr->agentID, session->agentID, USEC_MAX_AGENTID);
	    idPtr->agentBoots = session->agentBoots;
	    idPtr->agentTime = session->agentTime;
	    return;
	}
    }

    idPtr = (AgentIDCacheElem *) ckalloc(sizeof(AgentIDCacheElem));
    memcpy(&idPtr->addr, &session->maddr, sizeof(struct sockaddr_in));
    memcpy(idPtr->agentID, session->agentID, USEC_MAX_AGENTID);
    idPtr->agentBoots = session->agentBoots;
    idPtr->agentTime = session->agentTime;
    idPtr->nextPtr = firstAgentIDCacheElem;
    firstAgentIDCacheElem = idPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpUsecSetAgentID --
 *
 *	This procedure re-computes localized authentication keys and
 *	should be called whenever the agentID of a session is changed.
 *	It also caches agentBoots and agentTime and hence it should
 *	also be called when these two parameters change.
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
TnmSnmpUsecSetAgentID(TnmSnmp *session)
{
    if (session->qos & USEC_QOS_AUTH && session->password) {
	MakeAuthKey(session);
    }
    SaveAgentID(session);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpUsecGetAgentID --
 *
 *	This procedure tries to find an already known agentID for the
 *	SNMP session. It uses the internal cache of agentIDs. The 
 *	authentication key is re-computed if an agentID is found.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new authentication key and new agentID, agentBoots and agentTime
 *	values are stored in the SNMP session structure.
 *
 *----------------------------------------------------------------------
 */

void
TnmSnmpUsecGetAgentID(TnmSnmp *session)
{
    int found;

    found = FindAgentID(session);
    if (found == TCL_OK) {
	if (session->qos & USEC_QOS_AUTH) {
	    MakeAuthKey(session);
	}
    }
}

#endif

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpEvalCallback --
 *
 *	This procedure evaluates a Tcl callback. The command string is
 *	modified according to the % escapes before evaluation.  The
 *	list of supported escapes is %R = request id, %S = session
 *	name, %E = error status, %I = error index, %V = varbindlist
 *	and %A the agent address. There are three more escapes for
 *	instance bindings: %o = object identifier of instance, %i =
 *	instance identifier, %v = value, %p = previous value during
 *	set processing.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Tcl commands are evaluated which can have all kind of effects.
 *
 *----------------------------------------------------------------------
 */

int
TnmSnmpEvalCallback(Tcl_Interp *interp, TnmSnmp *session, TnmSnmpPdu *pdu, char *cmd, char *instance, char *oid, char *value, char *last)
{
    char buf[20];
    int	code;
    Tcl_DString tclCmd;
    char *startPtr, *scanPtr, *name;

    Tcl_DStringInit(&tclCmd);
    startPtr = cmd;
    for (scanPtr = startPtr; *scanPtr != '\0'; scanPtr++) {
	if (*scanPtr != '%') {
	    continue;
	}
	Tcl_DStringAppend(&tclCmd, startPtr, scanPtr - startPtr);
	scanPtr++;
	startPtr = scanPtr + 1;
	switch (*scanPtr) {
	  case 'R':  
	    sprintf(buf, "%d", pdu->requestId);
	    Tcl_DStringAppend(&tclCmd, buf, -1);
	    break;
	  case 'S':
	    if (session && session->interp && session->token) {
		Tcl_DStringAppend(&tclCmd, 
		  Tcl_GetCommandName(session->interp, session->token), -1);
	    }
	    break;
	  case 'V':
	    Tcl_DStringAppend(&tclCmd, Tcl_DStringValue(&pdu->varbind), -1);
	    break;
	  case 'E':
	    name = TnmGetTableValue(tnmSnmpErrorTable, (unsigned) pdu->errorStatus);
	    if (name == NULL) {
		name = "unknown";
	    }
	    Tcl_DStringAppend(&tclCmd, name, -1);
	    break;
	  case 'I':
	    sprintf(buf, "%d", pdu->errorIndex - 1);
	    Tcl_DStringAppend(&tclCmd, buf, -1);
	    break;
	  case 'A':
	    Tcl_DStringAppend(&tclCmd, inet_ntoa(pdu->addr.sin_addr), -1);
	    break;
	  case 'P':
	    sprintf(buf, "%u", ntohs((unsigned short) pdu->addr.sin_port));
	    Tcl_DStringAppend(&tclCmd, buf, -1);
	    break;
#ifdef TNM_SNMPv3
	  case 'C':
	    if (pdu->context && pdu->contextLength) {
		Tcl_DStringAppend(&tclCmd, pdu->context, pdu->contextLength);
	    }
	    break;
	  case 'G':
	    if (pdu->engineID && pdu->engineIDLength) {
		Tcl_DStringAppend(&tclCmd, pdu->engineID, pdu->engineIDLength);
	    }
	    break;
#endif
	  case 'T':
	    name = TnmGetTableValue(tnmSnmpPDUTable, (unsigned) pdu->type);
	    if (name == NULL) {
		name = "unknown";
	    }
	    Tcl_DStringAppend(&tclCmd, name, -1);
            break;
	  case 'o':
	    if (instance) {
		Tcl_DStringAppend(&tclCmd, instance, -1);
	    }
	    break;
	  case 'i':
	    if (oid) {
		Tcl_DStringAppend(&tclCmd, oid, -1);
	    }
	    break;
	  case 'v':
	    if (value) {
		Tcl_DStringAppend(&tclCmd, value, -1);
	    }
	    break;
	  case 'p':
	    if (last) {
		Tcl_DStringAppend(&tclCmd, last, -1);
	    }
	    break;
	  case '%':
	    Tcl_DStringAppend(&tclCmd, "%", -1);
	    break;
	  default:
	    sprintf(buf, "%%%c", *scanPtr);
	    Tcl_DStringAppend(&tclCmd, buf, -1);
	}
    }
    Tcl_DStringAppend(&tclCmd, startPtr, scanPtr - startPtr);
    
    /*
     * Now evaluate the callback function and issue a background
     * error if the callback fails for some reason. Return the
     * original error message and code to the caller.
     */
    
    Tcl_AllowExceptions(interp);
    code = Tcl_GlobalEval(interp, Tcl_DStringValue(&tclCmd));
    Tcl_DStringFree(&tclCmd);

    /*
     * Call the usual error handling proc if we have evaluated
     * a binding not bound to a specific instance. Bindings 
     * bound to an instance are usually called during PDU 
     * processing where it is important to get the error message
     * back.
     */

    if (code == TCL_ERROR && oid == NULL) {
	char *errorMsg = ckstrdup(Tcl_GetStringResult(interp));
	Tcl_AddErrorInfo(interp, "\n    (snmp callback)");
	Tcl_BackgroundError(interp);
	Tcl_SetResult(interp, errorMsg, TCL_DYNAMIC);
    }
    
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpEvalBinding --
 *
 *	This procedure checks for events that are not bound to an
 *	instance, such as TNM_SNMP_BEGIN_EVENT and TNM_SNMP_END_EVENT.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Tcl commands are evaluated which can have all kind of effects.
 *
 *----------------------------------------------------------------------
 */

int
TnmSnmpEvalBinding(Tcl_Interp *interp, TnmSnmp *session, TnmSnmpPdu *pdu, int event)
{
    int code = TCL_OK;
    TnmSnmpBinding *bindPtr = session->bindPtr;
    
    while (bindPtr) {
	if (bindPtr->event == event) break;
	bindPtr = bindPtr->nextPtr;
    }

    if (bindPtr && bindPtr->command) {
	Tcl_Preserve((ClientData) session);
	code = TnmSnmpEvalCallback(interp, session, pdu, bindPtr->command,
				   NULL, NULL, NULL, NULL);
	Tcl_Release((ClientData) session);
    }

    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpDumpPDU --
 *
 *	This procedure dumps the contents of a pdu to standard output. 
 *	This is just a debugging aid.
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
TnmSnmpDumpPDU(Tcl_Interp *interp, TnmSnmpPdu *pdu)
{
    if (hexdump) {

        int i, code, argc;
	const char **argv;
	char *name, *status;
	char buffer[80];
	Tcl_DString dst;
	Tcl_Channel channel;

	Tcl_DStringInit(&dst);

	name = TnmGetTableValue(tnmSnmpPDUTable, (unsigned) pdu->type);
	if (name == NULL) {
	    name = "(unknown PDU type)";
	}

	status = TnmGetTableValue(tnmSnmpErrorTable, (unsigned) pdu->errorStatus);
	if (status == NULL) {
	    status = "(unknown error code)";
	}
	
	if (pdu->type == ASN1_SNMP_GETBULK) {
	    sprintf(buffer, "%s %d non-repeaters %d max-repetitions %d\n", 
		    name, pdu->requestId,
		    pdu->errorStatus, pdu->errorIndex);
	} else if (pdu->type == ASN1_SNMP_TRAP1) {
	    sprintf(buffer, "%s\n", name);
	} else if (pdu->errorStatus == TNM_SNMP_NOERROR) {
	    sprintf(buffer, "%s %d %s\n", name, pdu->requestId, status);
	} else {
	    sprintf(buffer, "%s %d %s at %d\n", 
		    name, pdu->requestId, status, pdu->errorIndex);
	}

	Tcl_DStringAppend(&dst, buffer, -1);

	code = Tcl_SplitList(interp, Tcl_DStringValue(&pdu->varbind), 
			     &argc, &argv);
	if (code == TCL_OK) {
	    for (i = 0; i < argc; i++) {
		sprintf(buffer, "%4d.\t", i+1);
		Tcl_DStringAppend(&dst, buffer, -1);
		Tcl_DStringAppend(&dst, argv[i], -1);
		Tcl_DStringAppend(&dst, "\n", -1);
	    }
	    ckfree((char *) argv);
	}
	Tcl_ResetResult(interp);

	channel = Tcl_GetStdChannel(TCL_STDOUT);
	if (channel) {
	    Tcl_Write(channel,
		      Tcl_DStringValue(&dst), Tcl_DStringLength(&dst));
	}
	Tcl_DStringFree(&dst);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpMD5Digest --
 *
 *	This procedure computes the message digest of the given packet.
 *	It is based on the MD5 implementation of RFC 1321. We compute a
 *	keyed MD5 digest if the key parameter is not a NULL pointer.
 *
 * Results:
 *	The digest is written into the digest array.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TnmSnmpMD5Digest(u_char *packet, int length, u_char *key, u_char *digest)
{
    MD5_CTX MD;

    TnmMD5Init(&MD);   /* initialize MD5 */
    TnmMD5Update(&MD, (char *) packet, length);
    if (key) {
	TnmMD5Update(&MD, (char *) key, TNM_MD5_SIZE);
    }
    TnmMD5Final(digest, &MD);

    if (hexdump) {
	int i;
	if (key) {
	    fprintf(stderr, "MD5    key: ");
	    for (i = 0; i < TNM_MD5_SIZE; i++) {
		fprintf(stderr, "%02x ", key[i]);
	    }
	    fprintf(stdout, "\n");
	}
	fprintf(stderr, "MD5 digest: ");
	for (i = 0; i < TNM_MD5_SIZE; i++) {
	    fprintf(stderr, "%02x ", digest[i]);
	}
	fprintf(stderr, "\n");
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpCreateSession --
 *
 *	This procedure allocates and initializes a TnmSnmp 
 *	structure.
 *
 * Results:
 *	A pointer to the new TnmSnmp structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmSnmp*
TnmSnmpCreateSession(Tcl_Interp *interp, char type)
{
    TnmSnmp *session;
    const char *user;

    session = (TnmSnmp *) ckalloc(sizeof(TnmSnmp));
    memset((char *) session, 0, sizeof(TnmSnmp));

    session->interp = interp;
    session->maddr.sin_family = AF_INET;
    if (type == TNM_SNMP_GENERATOR || type == TNM_SNMP_NOTIFIER) {
       session->maddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else {
       session->maddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }
    if (type == TNM_SNMP_LISTENER || type == TNM_SNMP_NOTIFIER) {
	session->maddr.sin_port = htons((unsigned short) TNM_SNMP_TRAPPORT);
    } else {
	session->maddr.sin_port = htons((unsigned short) TNM_SNMP_PORT);
    }
    session->version = TNM_SNMPv1;
    session->domain = TNM_SNMP_UDP_DOMAIN;
    session->type = type;
    session->community = Tcl_NewStringObj("public", 6);
    Tcl_IncrRefCount(session->community);
    session->context = Tcl_NewStringObj("", 0);
    Tcl_IncrRefCount(session->context);

    user = Tcl_GetVar2(interp, "tnm", "user", TCL_GLOBAL_ONLY);
    if (! user) {
	user = "initial";
    }
    session->user = Tcl_NewStringObj(user, (int) strlen(user));
    Tcl_IncrRefCount(session->user);
    session->engineID = Tcl_NewStringObj("", 0);
    Tcl_IncrRefCount(session->engineID);
    session->maxSize = TNM_SNMP_MAXSIZE;
    session->securityLevel = TNM_SNMP_AUTH_NONE | TNM_SNMP_PRIV_NONE;
    session->maxSize = TNM_SNMP_MAXSIZE;
    session->authPassWord = Tcl_NewStringObj("public", 6);
    Tcl_IncrRefCount(session->authPassWord);
    session->privPassWord = Tcl_NewStringObj("private", 6);
    Tcl_IncrRefCount(session->privPassWord);
    session->retries = TNM_SNMP_RETRIES;
    session->timeout = TNM_SNMP_TIMEOUT;
    session->window  = TNM_SNMP_WINDOW;
    session->delay   = TNM_SNMP_DELAY;
    session->tagList = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(session->tagList);

    TnmOidInit(&session->enterpriseOid);
    TnmOidFromString(&session->enterpriseOid, "1.3.6.1.4.1.1575");

    return session;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpDeleteSession --
 *
 *	This procedure frees the memory allocated by a TnmSnmp
 *	and all it's associated structures.
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
TnmSnmpDeleteSession(TnmSnmp *session)
{
    TnmSnmpRequest **rPtrPtr;

    if (! session) return;

    rPtrPtr = &queueHead;
    while (*rPtrPtr) {
	if ((*rPtrPtr)->session == session) {
	    TnmSnmpRequest *request = *rPtrPtr;
	    *rPtrPtr = (*rPtrPtr)->nextPtr;
	    if (request->timer) {
	        Tcl_DeleteTimerHandler(request->timer);
	    }
	    Tcl_EventuallyFree((ClientData) request, RequestDestroyProc);
	} else {
	    rPtrPtr = &(*rPtrPtr)->nextPtr;
	}
    }

    Tcl_EventuallyFree((ClientData) session, SessionDestroyProc);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpCreateRequest --
 *
 *	This procedure creates an entry in the request list and
 *	saves this packet together with it's callback function.
 *	The callback is executed when the response packet is
 *	received from the agent.
 *
 * Results:
 *	A pointer to the new TnmSnmpRequest structure.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmSnmpRequest*
TnmSnmpCreateRequest(int id, u_char *packet, int packetlen, TnmSnmpRequestProc *proc, ClientData clientData, Tcl_Interp *interp)
{
    TnmSnmpRequest *request;

    /*
     * Allocate a TnmSnmpRequest structure together with some space to
     * hold the encoded packet. Allocating this in one ckalloc call 
     * simplifies and thus speeds up memory management.
     */

    request = (TnmSnmpRequest *) ckalloc(sizeof(TnmSnmpRequest) + packetlen);
    memset((char *) request, 0, sizeof(TnmSnmpRequest));
    request->packet = (u_char *) request + sizeof(TnmSnmpRequest);
    request->id = id;
    memcpy(request->packet, packet, (size_t) packetlen);
    request->packetlen = packetlen;
    request->proc = proc;
    request->clientData = clientData;
    request->interp = interp;
    return request;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpFindRequest --
 *
 *	This procedure scans through the request lists of all open
 *	sessions and tries to find the request for a given request
 *	id.
 *
 * Results:
 *	A pointer to the request structure or NULL if the request
 *	id is not in the request list.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmSnmpRequest*
TnmSnmpFindRequest(int id)
{
    TnmSnmpRequest *rPtr;

    for (rPtr = queueHead; rPtr; rPtr = rPtr->nextPtr) {
        if (rPtr->id == id) break;
    }
    return rPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpQueueRequest --
 *
 *	This procedure queues a request into the wait queue or checks 
 *	if queued requests should be activated. The queue is processed
 *	in FIFO order with the following constraints:
 *
 *	1. The number of active requests per session is smaller than
 *	   the window size of this session.
 *
 *	2. The total number of active requests is smaller than the
 *	   window size of this session.
 *
 *	The second rule makes sure that you can't flood a network by 
 *	e.g. creating thousand sessions all with a small window size
 *	sending one request. If the parameter which specifies the
 *	new request is NULL, only queue processing will take place.
 *
 * Results:
 *	The number of requests queued for this SNMP session.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmSnmpQueueRequest(TnmSnmp *session, TnmSnmpRequest *request)
{
    int waiting = 0, active = 0;
    TnmSnmpRequest *rPtr, *lastPtr = NULL;

    /*
     * Walk through the queue and count active and waiting requests.
     * Keep a pointer to the last request in the queue.
     */

    for (rPtr = queueHead; rPtr; rPtr = rPtr->nextPtr) {
	if (rPtr->sends) {
	    active++;
	} else {
	    waiting++;
	}
	if (request) {
	    lastPtr = rPtr;
	}
    }

    /*
     * Append the new request (if we have one).
     */

    if (request) {
	request->session = session;
	session->waiting++;
	waiting++;
	if (! queueHead) {
	    queueHead = request;
	} else {
	    lastPtr->nextPtr = request;
	}
    }

    /*
     * Try to activate new requests if there are some waiting and
     * if the total number of active requests is smaller than the
     * window of the current session.
     */

    for (rPtr = queueHead; rPtr && waiting; rPtr = rPtr->nextPtr) {
        if (session->window && active >= session->window) break;
	if (! rPtr->sends && (rPtr->session->active < rPtr->session->window 
			      || rPtr->session->window == 0)) {
	    TnmSnmpTimeoutProc((ClientData) rPtr);
	    active++;
	    waiting--;
	    rPtr->session->active++;
	    rPtr->session->waiting--;
	}
    }

    return (session->active + session->waiting);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpDeleteRequest --
 *
 *	This procedure deletes a request from the list of known 
 *	requests. This will also free all resources and event
 *	handlers for this request.
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
TnmSnmpDeleteRequest(TnmSnmpRequest *request)
{
    TnmSnmpRequest *rPtr, **rPtrPtr;
    TnmSnmp *session;

    /*
     * Check whether the request still exists. It may have been
     * removed because the session for this request has been 
     * destroyed during callback processing.
     */

    for (rPtr = queueHead; rPtr; rPtr = rPtr->nextPtr) {
	if (rPtr == request) break;
    }
    if (! rPtr) return;
    
    /* 
     * Check whether the session is still in the session list.
     * We sometimes get called when the session has already been
     * destroyed as a side effect of evaluating callbacks.
     */
    
    for (session = tnmSnmpList; session; session = session->nextPtr) {
	if (session == request->session) break;
    }

    if (session) {
	if (request->sends) {
	    session->active--;
	} else {
	    session->waiting--;
	}
    }
    
    /*
     * Remove the request from the list of outstanding requests.
     * and free the resources allocated for this request.
     */

    rPtrPtr = &queueHead;
    while (*rPtrPtr && *rPtrPtr != request) {
	rPtrPtr = &(*rPtrPtr)->nextPtr;
    }
    if (*rPtrPtr) {
	*rPtrPtr = request->nextPtr;
	if (request->timer) {
	    Tcl_DeleteTimerHandler(request->timer);
	    request->timer = NULL;
	}
	Tcl_EventuallyFree((ClientData) request, RequestDestroyProc);
    }

    /*
     * Update the request queue. This will activate async requests
     * that have been queued because of the window size.
     */
     
    if (session) {
	TnmSnmpQueueRequest(session, NULL);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpGetRequestId --
 *
 *	This procedure generates an unused request identifier.
 *
 * Results:
 *	The request identifier.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmSnmpGetRequestId()
{
    int id;
    TnmSnmpRequest *rPtr = queueHead;

    do {
	id = rand();
	for (rPtr = queueHead; rPtr && rPtr->id != id; rPtr = rPtr->nextPtr) {
	    /* empty body */
	}
    } while (rPtr);

    return id;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_SnmpSplitVBList --
 *
 *	This procedure splits a list of Tcl lists containing
 *	varbinds (again Tcl lists) into an array of SNMP_VarBind
 *	structures.
 *
 * Results:
 *	The standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tnm_SnmpSplitVBList(Tcl_Interp *interp, char *list, int *varBindSizePtr, SNMP_VarBind **varBindPtrPtr)
{
    int code, vblc, i;
    const char **vblv;
    int varBindSize;
    SNMP_VarBind *varBindPtr;

    code = Tcl_SplitList(interp, list, &vblc, &vblv);
    if (code != TCL_OK) {
        return TCL_ERROR;
    }

    /*
     * Allocate space for the varbind table. Note, we could reuse space
     * allocated from previous runs to avoid all the malloc and free
     * operations. For now, we go the simple way.
     */

    varBindSize = vblc;
    varBindPtr = (SNMP_VarBind *) ckalloc(varBindSize * sizeof(SNMP_VarBind));
    memset((char *) varBindPtr, 0, varBindSize * sizeof(SNMP_VarBind));

    for (i = 0; i < varBindSize; i++) {
        int vbc;
        char **vbv;

        code = Tcl_SplitList(interp, vblv[i], &vbc, (const char ***)&vbv);
	if (code != TCL_OK) {
	    Tnm_SnmpFreeVBList(varBindSize, varBindPtr);
	    ckfree((char *) vblv);
	    return TCL_ERROR;
	}
	if (vbc > 0) {
	    varBindPtr[i].soid = vbv[0];
	    if (vbc > 1) {
		varBindPtr[i].syntax = vbv[1];
		if (vbc > 2) {
		    varBindPtr[i].value = vbv[2];
		}
	    }
	}
	varBindPtr[i].freePtr = (char *) vbv;
    }

    *varBindSizePtr = varBindSize;
    *varBindPtrPtr = varBindPtr;
    ckfree((char *) vblv);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_SnmpMergeVBList --
 *
 *	This procedure merges the contents of a SNMP_VarBind
 *	structure into a Tcl list of Tcl lists.
 *
 * Results:
 *	A pointer to a malloced buffer.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char*
Tnm_SnmpMergeVBList(int varBindSize, SNMP_VarBind *varBindPtr)
{
    static Tcl_DString list;
    int i;

    Tcl_DStringInit(&list);

    for (i = 0; i < varBindSize; i++) {
        Tcl_DStringStartSublist(&list);
	Tcl_DStringAppendElement(&list, 
			     varBindPtr[i].soid ? varBindPtr[i].soid : "");
	Tcl_DStringAppendElement(&list, 
			     varBindPtr[i].syntax ? varBindPtr[i].syntax : "");
	Tcl_DStringAppendElement(&list, 
			     varBindPtr[i].value ? varBindPtr[i].value : "");
	Tcl_DStringEndSublist(&list);
    }

    return ckstrdup(Tcl_DStringValue(&list));
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_SnmpFreeVBList --
 *
 *	This procedure frees the array of SNMP_VarBind structures
 *	and all the varbinds stored in the array.
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
Tnm_SnmpFreeVBList(int varBindSize, SNMP_VarBind *varBindPtr)
{
    int i;
    
    for (i = 0; i < varBindSize; i++) {
	if (varBindPtr[i].freePtr) {
	    ckfree(varBindPtr[i].freePtr);
	}
    }

    ckfree((char *) varBindPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpNorm --
 *
 *	This procedure normalizes a Tcl list into a normalized
 *	Tcl SNMP varbind list. It expands missing types and values
 *	and it converts names to OIDs.
 *
 * Results:
 *	A pointer to a Tcl_Obj containing the normalized list
 *	or NULL if the conversion failed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Obj*
TnmSnmpNorm(Tcl_Interp *interp, Tcl_Obj *objPtr, int flags)
{
    int i, code, objc;
    Tcl_Obj **objv;
    Tcl_Obj *vbListPtr = NULL;

    /*
     * The following Tcl_Objs are allocated once and reused whenever
     * we need to expand a varbind list containing object identifiers
     * without any value or type elements.
     */

    static Tcl_Obj *nullType = NULL;
    static Tcl_Obj *zeroValue = NULL;
    static Tcl_Obj *nullValue = NULL;

    if (! nullType) {
	nullType = Tcl_NewStringObj("NULL", 4);
	Tcl_IncrRefCount(nullType);
    }
    if (! zeroValue) {
	zeroValue = Tcl_NewIntObj(0);
	Tcl_IncrRefCount(zeroValue);
    }
    if (! nullValue) {
	nullValue = Tcl_NewStringObj(NULL, 0);
	Tcl_IncrRefCount(nullValue);
    }

    /*
     * Split the varbind list into a list of varbinds. Create a
     * new Tcl list to hold the expanded varbind list.
     */

    code = Tcl_ListObjGetElements(interp, objPtr, &objc, &objv);
    if (code != TCL_OK) {
	goto errorExit;
    }

    vbListPtr = Tcl_NewListObj(0, NULL);

    for (i = 0; i < objc; i++) {
	int vbc, type;
	Tcl_Obj **vbv, *vbPtr;
	TnmOid* oidPtr;
	Tcl_Obj *oidObjPtr, *typeObjPtr, *valueObjPtr;
	TnmMibNode *nodePtr = NULL;

	/*
	 * Create a new varbind element in the expanded result list
	 * for each varbind.
	 */

	vbPtr = Tcl_NewListObj(0, NULL);
	Tcl_ListObjAppendElement(interp, vbListPtr, vbPtr);

	code = Tcl_ListObjGetElements(interp, objv[i], &vbc, &vbv);
	if (code != TCL_OK) {
	    goto errorExit;
	}

	/*
	 * Get the object identifier value from the first list
	 * element. Check the number of list elements and assign
	 * them to the oid, type and value variables.
	 */

	switch (vbc) {
	case 1:
	    oidObjPtr = vbv[0];
	    typeObjPtr = nullType;
	    valueObjPtr = nullValue;
	    break;
	case 2:
	    oidObjPtr = vbv[0];
	    typeObjPtr = NULL;
	    valueObjPtr = vbv[1];
	    break;
	case 3:
	    oidObjPtr = vbv[0];
	    typeObjPtr = vbv[1];
	    valueObjPtr = vbv[2];
	    break;
	default: {
		char msg[80];
		sprintf(msg, "illegal number of elements in varbind %d", i);
		Tcl_ResetResult(interp);
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), 
				       msg, (char *) NULL);
		goto errorExit;
	    }
	}

	/*
	 * Check/resolve the object identifier and assign it to the
	 * result list. Make sure to make a deep copy if the object
	 * identifier value is shared since the string representation
	 * must be invalidated to ensure that hexadecimal
	 * sub-identifier are converted into decimal sub-identifier.
	 */

	oidPtr = TnmGetOidFromObj(interp, oidObjPtr);
	if (! oidPtr) {
	    goto errorExit;
	}
	if (Tcl_IsShared(oidObjPtr)) {
	    oidObjPtr = Tcl_DuplicateObj(oidObjPtr);
	}
	TnmOidObjSetRep(oidObjPtr, TNM_OID_AS_OID);
	Tcl_InvalidateStringRep(oidObjPtr);
	Tcl_ListObjAppendElement(interp, vbPtr, oidObjPtr);

	/* 
	 * Lookup the type in the MIB if there is no type given in the
	 * varbind element.
	 */

	if (! typeObjPtr) {
	    int syntax;
	    nodePtr = TnmMibNodeFromOid(oidPtr, NULL);
	    if (! nodePtr) {
		char msg[80];
		sprintf(msg, "failed to lookup the type for varbind %d", i);
		Tcl_ResetResult(interp);
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), 
				       msg, (char *) NULL);
		goto errorExit;
	    }
	    syntax = (nodePtr->typePtr && nodePtr->typePtr->name)
		? nodePtr->typePtr->syntax : nodePtr->syntax;

	    typeObjPtr = Tcl_NewStringObj(
		TnmGetTableValue(tnmSnmpTypeTable, (unsigned) syntax), -1);
	}

	type = TnmGetTableKeyFromObj(NULL, tnmSnmpTypeTable, 
				     typeObjPtr, NULL);
	if (type == -1) {
	    type = TnmGetTableKeyFromObj(NULL, tnmSnmpExceptionTable,
					 typeObjPtr, NULL);
	    if (type == -1) {
		char msg[80];
	    invalidType:
		sprintf(msg, "illegal type in varbind %d", i);
		Tcl_ResetResult(interp);
		Tcl_AppendStringsToObj(Tcl_GetObjResult(interp), 
				       msg, (char *) NULL);
		goto errorExit;
	    }
	}

	Tcl_ListObjAppendElement(interp, vbPtr, typeObjPtr);

	/*
	 * Check the value and perform any conversions needed to
	 * convert the value into the base type representation.
	 */

	switch (type) {
	case ASN1_INTEGER: {
	    long longValue;
	    code = Tcl_GetLongFromObj(interp, valueObjPtr, &longValue);
	    if (code != TCL_OK) {
		if (! nodePtr) {
		    nodePtr = TnmMibNodeFromOid(oidPtr, NULL);
		}
		if (nodePtr) {
		    Tcl_Obj *value;
		    value = TnmMibScanValue(nodePtr->typePtr, nodePtr->syntax, 
					    valueObjPtr);
		    if (! value) {
			goto errorExit;
		    }
		    Tcl_ResetResult(interp);
		    code = Tcl_GetLongFromObj(interp, value, &longValue);
		}
		if (code != TCL_OK) {
		    goto errorExit;
		}
		valueObjPtr = Tcl_NewLongObj(longValue);
	    }
	    if (flags & TNM_SNMP_NORM_INT) {
		if (! nodePtr) {
		    nodePtr = TnmMibNodeFromOid(oidPtr, NULL);
		}
		if (nodePtr && nodePtr->typePtr) {
		    Tcl_Obj *newPtr;
		    newPtr = TnmMibFormatValue(nodePtr->typePtr,
					       nodePtr->syntax,
					       valueObjPtr);
		    if (newPtr) {
			valueObjPtr = newPtr;
		    }
		}
	    }
	    break;
	}
	case ASN1_COUNTER32:
	case ASN1_GAUGE32:
	case ASN1_TIMETICKS: {
	    TnmUnsigned32 u;
	    code = TnmGetUnsigned32FromObj(interp, valueObjPtr, &u);
	    if (code != TCL_OK) {
		goto errorExit;
	    }
	    break;
	}
	case ASN1_COUNTER64: {
	    TnmUnsigned64 u;
	    code = TnmGetUnsigned64FromObj(interp, valueObjPtr, &u);
	    if (code != TCL_OK) {
		goto errorExit;
	    }
	    break;
	}
	case ASN1_IPADDRESS: {
            if (TnmGetIpAddressFromObj(interp, valueObjPtr) == NULL) {
		goto errorExit;
	    }
	    Tcl_InvalidateStringRep(valueObjPtr);
	    break;
	}
	case ASN1_OBJECT_IDENTIFIER:
	    if (! TnmGetOidFromObj(interp, valueObjPtr)) {
		goto errorExit;
	    }
	    if (Tcl_IsShared(valueObjPtr)) {
		valueObjPtr = Tcl_DuplicateObj(valueObjPtr);
	    }
	    if (flags & TNM_SNMP_NORM_OID) {
		TnmOidObjSetRep(valueObjPtr, TNM_OID_AS_NAME);
	    } else {
		TnmOidObjSetRep(valueObjPtr, TNM_OID_AS_OID);
	    }
	    Tcl_InvalidateStringRep(valueObjPtr);
	    break;
	case ASN1_OCTET_STRING: {
	    int len;
	    if (! nodePtr) {
		nodePtr = TnmMibNodeFromOid(oidPtr, NULL);
	    }
	    if (nodePtr) {
		Tcl_Obj *scan;
		scan = TnmMibScanValue(nodePtr->typePtr, nodePtr->syntax, 
				      valueObjPtr);
		if (scan) {
		    valueObjPtr = scan;
		}
	    }
	    if (TnmGetOctetStringFromObj(interp, valueObjPtr, &len) == NULL) {
		goto errorExit;
	    }
	    Tcl_InvalidateStringRep(valueObjPtr);
	    break;
	}
	case ASN1_NULL:
	    valueObjPtr = nullValue;
	    break;
	default:
	    goto invalidType;
	}
	
	Tcl_ListObjAppendElement(interp, vbPtr, valueObjPtr);
    }

    return vbListPtr;

 errorExit:
    if (vbListPtr) {
	Tcl_DecrRefCount(vbListPtr);
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpSysUpTime --
 *
 *	This procedure returns the uptime of this SNMP enitity (agent) 
 *	in hundreds of seconds. Should be initialized when registering 
 *	the SNMP extension.
 *
 * Results:
 *	The uptime in hundreds of seconds.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmSnmpSysUpTime()
{
    static Tcl_Time bootTime = { 0, 0 };
    Tcl_Time currentTime;
    int delta = 0;

    Tcl_GetTime(&currentTime);
    if (bootTime.sec == 0 && bootTime.usec == 0) {
	bootTime = currentTime;
    } else {
	delta = (currentTime.sec - bootTime.sec) * 100
	    + (currentTime.usec - bootTime.usec) / 10000;
    }
    return delta;
}

