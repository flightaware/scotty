/*
 * tnmSnmpTcl.c --
 *
 *	This module implements the Tcl command interface for the SNMP
 *	protocol engine. The Tcl command interface is based on the
 *	concepts of SNMPv2. The SNMP engine translates SNMPv2 requests
 *	automatically into SNMPv1 requests if necessary.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmSnmpTcl.c,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if !defined(Tcl_GetErrorLine)
#define Tcl_GetErrorLine(interp) (interp->errorLine)
#endif


#include "tnmSnmp.h"
#include "tnmMib.h"

/*
 * The global variable TnmSnmp list contains all existing
 * session handles.
 */

TnmSnmp *tnmSnmpList = NULL;

int hexdump = 0;

/*
 * Every Tcl interpreter has an associated SnmpControl record. It
 * keeps track of the aliases known by the interpreter. The name
 * tnmSnmpControl is used to get/set the SnmpControl record.
 */

static char tnmSnmpControl[] = "tnmSnmpControl";

typedef struct SnmpControl {
    Tcl_HashTable aliasTable;	/* The hash table with SNMP aliases. */
} SnmpControl;

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
AssocDeleteProc	(ClientData clientData, Tcl_Interp *interp);

static void
DeleteProc	(ClientData clientdata);

static void
PduInit		(TnmSnmpPdu *pduPtr, TnmSnmp *session, int type);

static void
PduFree		(TnmSnmpPdu *pduPtr);

static Tcl_Obj*
GetOption	(Tcl_Interp *interp, ClientData object, 
			     int option);
static int
SetOption	(Tcl_Interp *interp, ClientData object, 
			     int option, Tcl_Obj *objPtr);
static int
BindEvent	(Tcl_Interp *interp, TnmSnmp *session,
			     Tcl_Obj *eventPtr, Tcl_Obj *script);
static int
FindSessions	(Tcl_Interp *interp, 
			     int objc, Tcl_Obj *CONST objv[]);
static int
GeneratorCmd	(ClientData	clientData, Tcl_Interp *interp,
			     int objc, Tcl_Obj *CONST objv[]);
static int
ListenerCmd	(ClientData	clientData, Tcl_Interp *interp,
			     int objc, Tcl_Obj *CONST objv[]);
static int
NotifierCmd	(ClientData	clientData, Tcl_Interp *interp,
			     int objc, Tcl_Obj *CONST objv[]);
static int
ResponderCmd	(ClientData	clientData, Tcl_Interp *interp,
			     int objc, Tcl_Obj *CONST objv[]);
static int 
WaitSession	(Tcl_Interp *interp, TnmSnmp *session, int id);

static void
ResponseProc	(TnmSnmp *session, TnmSnmpPdu *pdu, 
			     ClientData clientData);
static int
Notify		(Tcl_Interp *interp, TnmSnmp *session, int type,
			     Tcl_Obj *oid, Tcl_Obj *vbList, Tcl_Obj *script);
static int
Request		(Tcl_Interp *interp, TnmSnmp *session, int type,
			     int n, int m, Tcl_Obj *vbList, Tcl_Obj *cmd);
static Tcl_Obj*
WalkCheck	(int oidListLen, Tcl_Obj **oidListElems, 
			     int vbListLen, Tcl_Obj **vbListElems);
static void
AsyncWalkProc	(TnmSnmp *session, TnmSnmpPdu *pdu, 
			     ClientData clientData);
static int
AsyncWalk	(Tcl_Interp *interp, TnmSnmp *session,
			     Tcl_Obj *oidList, Tcl_Obj *tclCmd);
static int
SyncWalk	(Tcl_Interp *interp, TnmSnmp *session,
			     Tcl_Obj *varName, Tcl_Obj *oidList, 
			     Tcl_Obj *tclCmd);
static int
Delta		(Tcl_Interp *interp, Tcl_Obj *vbl1,
			     Tcl_Obj *vbl2);
static int
Extract		(Tcl_Interp *interp, int what, Tcl_Obj *objPtr,
			     Tcl_Obj *indexObjPtr);

#if 0
static int
ExpandTable	(Tcl_Interp *interp, 
			     char *tList, Tcl_DString *dst);
static int
ExpandScalars	(Tcl_Interp *interp, 
			     char *sList, Tcl_DString *dst);
static int
Table		(Tcl_Interp *interp, TnmSnmp *session,
			     char *table, char *arrayName);
static int
Scalars		(Tcl_Interp *interp, TnmSnmp *session,
			     char *group, char *arrayName);
static void
ScalarSetVar	(Tcl_Interp *interp, char *vbl,
			     char *varName, Tcl_DString *result);
#endif

/*
 * The options used to configure snmp sessions. There are separate
 * tables for each session type. Additional tables control the
 * events processed for the different session types.
 */

enum options {
    optAddress, optPort, optVersion, optAlias, optTags, optEnterprise,
    optCommunity, optUser, optContext, optEngineID, optSecurityLevel,
    optAuthKey, optPrivKey, optAuthPassWord, optPrivPassWord,
#ifdef TNM_SNMPv2U
    optPassword,
#endif
    optTransport, optTimeout, optRetries, optWindow, optDelay,
#ifdef TNM_SNMP_BENCH
    optRtt, optSendSize, optRecvSize
#endif
};

static TnmTable generatorOptionTable[] = {
    { optAddress,	"-address" },
    { optPort,		"-port" },
    { optVersion,	"-version" },
    { optCommunity,	"-community" },
    { optUser,		"-user" },
    { optContext,	"-context" },
    { optEngineID,	"-engineID" },
    { optAuthKey,	"-authKey" },
    { optPrivKey,	"-privKey" },
    { optAuthPassWord,	"-authPassWord" },
    { optPrivPassWord,	"-privPassWord" },
    { optSecurityLevel,	"-security" },
#ifdef TNM_SNMPv2U
    { optPassword,	"-password" },
#endif
    { optAlias,		"-alias" },
    { optTransport,	"-transport" },
    { optTimeout,	"-timeout" },
    { optRetries,	"-retries" },
    { optWindow,	"-window" },
    { optDelay,		"-delay" },
    { optTags,		"-tags" },
#ifdef TNM_SNMP_BENCH
    { optRtt,		"-rtt" },
    { optSendSize,	"-sendSize" },
    { optRecvSize,	"-recvSize" },
#endif
    { 0, NULL }
};

static TnmConfig generatorConfig = {
    generatorOptionTable,
    SetOption,
    GetOption
};

static TnmTable generatorEventTable[] = {
    { TNM_SNMP_SEND_EVENT,	"send" },
    { TNM_SNMP_RECV_EVENT,	"recv" },
    { 0, NULL }
};

static TnmTable responderOptionTable[] = {
    { optPort,		"-port" },
    { optVersion,	"-version" },
    { optCommunity,	"-community" },
    { optUser,		"-user" },
    { optContext,	"-context" },
    { optEngineID,	"-engineID" },
    { optAuthKey,	"-authKey" },
    { optPrivKey,	"-privKey" },
    { optAuthPassWord,	"-authPassWord" },
    { optPrivPassWord,	"-privPassWord" },
    { optSecurityLevel,	"-security" },
#ifdef TNM_SNMPv2U
    { optPassword,	"-password" },
#endif
    { optAlias,		"-alias" },
    { optTransport,	"-transport" },
    { optTimeout,	"-timeout" },
    { optRetries,	"-retries" },
    { optWindow,	"-window" },
    { optDelay,		"-delay" },
    { optTags,		"-tags" },
    { 0, NULL }
};

static TnmConfig responderConfig = {
    responderOptionTable,
    SetOption,
    GetOption
};

static TnmTable responderEventTable[] = {
    { TNM_SNMP_SEND_EVENT,	"send" },
    { TNM_SNMP_RECV_EVENT,	"recv" },
    { TNM_SNMP_BEGIN_EVENT,	"begin" },
    { TNM_SNMP_END_EVENT,	"end" },
    { 0, NULL }
};

static TnmTable notifierOptionTable[] = {
    { optAddress,	"-address" },
    { optPort,		"-port" },
    { optVersion,	"-version" },
    { optCommunity,	"-community" },
    { optUser,		"-user" },
    { optContext,	"-context" },
    { optEngineID,	"-engineID" },
    { optAuthKey,	"-authKey" },
    { optPrivKey,	"-privKey" },
    { optAuthPassWord,	"-authPassWord" },
    { optPrivPassWord,	"-privPassWord" },
    { optSecurityLevel,	"-security" },
#ifdef TNM_SNMPv2U
    { optPassword,	"-password" },
#endif
    { optAlias,		"-alias" },
    { optTransport,	"-transport" },
    { optTimeout,	"-timeout" },
    { optRetries,	"-retries" },
    { optWindow,	"-window" },
    { optDelay,		"-delay" },
    { optTags,		"-tags" },
    { optEnterprise,	"-enterprise" },
    { 0, NULL }
};

static TnmConfig notifierConfig = {
    notifierOptionTable,
    SetOption,
    GetOption
};

static TnmTable notifierEventTable[] = {
    { TNM_SNMP_SEND_EVENT,	"send" },
    { TNM_SNMP_RECV_EVENT,	"recv" },
    { 0, NULL }
};

static TnmTable listenerOptionTable[] = {
    { optPort,		"-port" },
    { optVersion,	"-version" },
    { optCommunity,	"-community" },
    { optUser,		"-user" },
    { optContext,	"-context" },
    { optEngineID,	"-engineID" },
    { optAuthKey,	"-authKey" },
    { optPrivKey,	"-privKey" },
    { optAuthPassWord,	"-authPassWord" },
    { optPrivPassWord,	"-privPassWord" },
    { optSecurityLevel,	"-security" },
#ifdef TNM_SNMPv2U
    { optPassword,	"-password" },
#endif
    { optAlias,		"-alias" },
    { optTransport,	"-transport" },
    { optTags,		"-tags" },
    { 0, NULL }
};

static TnmConfig listenerConfig = {
    listenerOptionTable,
    SetOption,
    GetOption
};

static TnmTable listenerEventTable[] = {
    { TNM_SNMP_SEND_EVENT,	"send" },
    { TNM_SNMP_RECV_EVENT,	"recv" },
    { TNM_SNMP_TRAP_EVENT,	"trap" },
    { TNM_SNMP_INFORM_EVENT,	"inform" },
    { 0, NULL }
};

/*
 * The following structure describes a Tcl command that should be
 * evaluated once we receive a response for a SNMP request.
 */

typedef struct AsyncToken {
    Tcl_Interp *interp;
    Tcl_Obj *tclCmd;
    Tcl_Obj *oidList;
} AsyncToken;


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
    SnmpControl *control = (SnmpControl *) clientData;
    Tcl_HashEntry *entryPtr;
    Tcl_HashSearch search;

    /*
     * Note, we do not care about snmp objects since Tcl first
     * removes all commands before calling this delete procedure.
     * Remove all the entries in the alias hash table to free the
     * allocated storage space.
     */

    if (control) {
	do {
	    entryPtr = Tcl_FirstHashEntry(&control->aliasTable, &search);
	    if (entryPtr) {
		ckfree((char *) Tcl_GetHashValue(entryPtr));
		Tcl_DeleteHashEntry(entryPtr);
	    }
	} while (entryPtr);
	Tcl_DeleteHashTable(&control->aliasTable);
	ckfree((char *) control);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteProc --
 *
 *	This procedure is invoked when a session handle is deleted.
 *	It frees the associated session structure and de-installs all
 *	pending events. If it is the last session, we also close the
 *	socket for manager communication.
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
DeleteProc(ClientData clientData)
{
    TnmSnmp **sPtrPtr, *session = (TnmSnmp *) clientData;

    sPtrPtr = &tnmSnmpList;
    while (*sPtrPtr && (*sPtrPtr) != session) {
	sPtrPtr = &(*sPtrPtr)->nextPtr;
    }

    if (*sPtrPtr) {
	(*sPtrPtr) = session->nextPtr;
    }

    TnmSnmpDeleteSession(session);

    if (tnmSnmpList == NULL) {
	TnmSnmpManagerClose();
    }
}

/*
 *----------------------------------------------------------------------
 *
 * PduInit --
 *
 *	This procedure initializes an SNMP PDU.
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
PduInit(TnmSnmpPdu *pduPtr, TnmSnmp *session, int type)
{
    pduPtr->addr = session->maddr;
    pduPtr->type = type;
    pduPtr->requestId = TnmSnmpGetRequestId();
    pduPtr->errorStatus = TNM_SNMP_NOERROR;
    pduPtr->errorIndex = 0;    
    pduPtr->trapOID = NULL;
    Tcl_DStringInit(&pduPtr->varbind);

#ifdef TNM_SNMP_BENCH
    memset((char *) &session->stats, 0, sizeof(session->stats));
#endif
}

/*
 *----------------------------------------------------------------------
 *
 * PduFree --
 *
 *	This procedure frees all data associated with an SNMP PDU.
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
PduFree(TnmSnmpPdu *pduPtr)
{
    if (pduPtr->trapOID) ckfree(pduPtr->trapOID);
    Tcl_DStringFree(&pduPtr->varbind);
}

/*
 *----------------------------------------------------------------------
 *
 * GetOption --
 *
 *	This procedure retrieves the value of a session option.
 *
 * Results:
 *	A pointer to the value formatted as a string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
GetOption(Tcl_Interp *interp, ClientData object, int option)
{
    TnmSnmp *session = (TnmSnmp *) object;

    switch ((enum options) option) {
    case optAddress:
	return Tcl_NewStringObj(inet_ntoa(session->maddr.sin_addr), -1);
    case optPort:
	return Tcl_NewIntObj(ntohs(session->maddr.sin_port));
    case optVersion:
	return Tcl_NewStringObj(TnmGetTableValue(tnmSnmpVersionTable, 
					 (unsigned) session->version), -1);
    case optCommunity:
	if (session->version!=TNM_SNMPv1 && session->version!=TNM_SNMPv2C) {
	    return NULL;
	}
	return session->community;
    case optUser:
	if (! TNM_SNMP_USER(session)) {
	    return NULL;
	}
	return session->user;
    case optContext:
	if (! TNM_SNMP_USER(session)) {
	    return NULL;
	}
	return session->context;
#ifdef TNM_SNMPv2U
    case optPassword:
	if (! TNM_SNMP_USER(session)) {
	    return NULL;
	}
	return Tcl_NewStringObj(session->password, -1);
#endif
    case optEngineID:
	if (! TNM_SNMP_USER(session)) {
            return NULL;
        }
	return session->engineID;
    case optAuthKey:
	if (! TNM_SNMP_USER(session)) {
            return NULL;
        }
	return session->usmAuthKey;
    case optPrivKey:
	if (! TNM_SNMP_USER(session)) {
            return NULL;
        }
	return session->usmPrivKey;
    case optAuthPassWord:
	if (! TNM_SNMP_USER(session)) {
            return NULL;
        }
	return session->authPassWord;
    case optPrivPassWord:
	if (! TNM_SNMP_USER(session)) {
            return NULL;
        }
	return session->privPassWord;
    case optSecurityLevel:
	if (! TNM_SNMP_USER(session)) {
            return NULL;
        }
	return Tcl_NewStringObj(TnmGetTableValue(tnmSnmpSecurityLevelTable, 
				 (unsigned) session->securityLevel), -1);
    case optAlias:
	return NULL;
    case optTransport:
	return Tcl_NewStringObj(TnmGetTableValue(tnmSnmpDomainTable, 
					 (unsigned) session->domain), -1);
    case optTimeout:
	if (session->domain != TNM_SNMP_UDP_DOMAIN) return NULL;
	return Tcl_NewIntObj(session->timeout);
    case optRetries:
	if (session->domain != TNM_SNMP_UDP_DOMAIN) return NULL;
	return Tcl_NewIntObj(session->retries);
    case optWindow:
	if (session->domain != TNM_SNMP_UDP_DOMAIN) return NULL;
	return Tcl_NewIntObj(session->window);
    case optDelay:
	if (session->domain != TNM_SNMP_UDP_DOMAIN) return NULL;
	return Tcl_NewIntObj(session->delay);
    case optTags:
	return session->tagList;
    case optEnterprise:
	return Tcl_NewStringObj(TnmOidToString(&session->enterpriseOid), -1);
#ifdef TNM_SNMP_BENCH
    case optRtt:
	return Tcl_NewIntObj(
	(session->stats.recvTime.sec - session->stats.sendTime.sec) * 1000000
	+ (session->stats.recvTime.usec - session->stats.sendTime.usec));
    case optSendSize:
	return Tcl_NewIntObj(session->stats.sendSize);
    case optRecvSize:
	return Tcl_NewIntObj(session->stats.recvSize);
#endif
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * SetOption --
 *
 *	This procedure modifies a single option of a session object.
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
    TnmSnmp *session = (TnmSnmp *) object;
    int num, len;
#ifdef TNM_SNMPv2U
    char *val;
#endif

    SnmpControl *control = (SnmpControl *) 
	Tcl_GetAssocData(interp, tnmSnmpControl, NULL);

    switch ((enum options) option) {
    case optAddress:
	return TnmSetIPAddress(interp, Tcl_GetStringFromObj(objPtr, NULL),
			       &session->maddr);
    case optPort:
	return TnmSetIPPort(interp, "udp", Tcl_GetStringFromObj(objPtr, NULL),
			    &session->maddr);
    case optVersion:
	num = TnmGetTableKeyFromObj(interp, tnmSnmpVersionTable, 
				    objPtr, "SNMP version");
	if (num == -1) {
	    return TCL_ERROR;
	}
	session->version = num;
	return TCL_OK;
    case optCommunity:
#ifdef TNM_SNMPv2U
	if (session->version == TNM_SNMPv2U) {
	    session->version = TNM_SNMPv1;
	}
#endif
	if (session->version == TNM_SNMPv3) {
	    session->version = TNM_SNMPv1;
	}
	Tcl_DecrRefCount(session->community);
	session->community = objPtr;
	Tcl_IncrRefCount(objPtr);
	return TCL_OK;
    case optUser:
#ifdef TNM_SNMPv2U
	session->version = TNM_SNMPv2U;
#endif
	session->version = TNM_SNMPv3;
	(void) Tcl_GetStringFromObj(objPtr, &len);
#ifdef TNM_SNMPv2U
	if (len > USEC_MAX_USER) {
	    Tcl_SetResult(interp, "user name too long", TCL_STATIC);
	    return TCL_ERROR;
	}
#endif
	/*
	 * Check the length restrictions for the user name defined
	 * in RFC 2274, section 2.4.
	 */
	if (len < 1 || len > 32) {
	    Tcl_SetResult(interp,
		  "the user name length must be between 1 and 32 octets",
			  TCL_STATIC);
	    return TCL_ERROR;
	}
	Tcl_DecrRefCount(session->user);
	session->user = objPtr;
	Tcl_IncrRefCount(objPtr);
	return TCL_OK;
#ifdef TNM_SNMPv2U
    case optPassword:
	session->version = TNM_SNMPv2U;
	val = Tcl_GetStringFromObj(objPtr, &len);
	if (len == 0) {
	    session->qos &= ~ USEC_QOS_AUTH;
	} else {

	    session->password = ckstrdup(val);
	    session->qos |= USEC_QOS_AUTH;
	}
	return TCL_OK;
#endif
    case optAuthPassWord:
	/*
	 * RFC 2274 section 11.2 requires that passwords are at least
	 * 8 characters long.
	 */
	(void) Tcl_GetStringFromObj(objPtr, &len);
	if (len < 8) {
	    Tcl_SetResult(interp,
			  "the password must be at least 8 characters long",
			  TCL_STATIC);
	    return TCL_ERROR;
	}
	session->version = TNM_SNMPv3;
	if (session->authPassWord) {
	    Tcl_DecrRefCount(session->authPassWord);
	    session->authPassWord = NULL;
	}
	session->authPassWord = objPtr;
	Tcl_IncrRefCount(objPtr);
	return TCL_OK;
    case optPrivPassWord:
	/*
	 * RFC 2274 section 11.2 requires that passwords are at least
	 * 8 characters long.
	 */
	(void) Tcl_GetStringFromObj(objPtr, &len);
	if (len < 8) {
	    Tcl_SetResult(interp,
			  "the password must be at least 8 characters long",
			  TCL_STATIC);
	    return TCL_ERROR;
	}
	session->version = TNM_SNMPv3;
	if (session->privPassWord) {
	    Tcl_DecrRefCount(session->privPassWord);
	    session->privPassWord = NULL;
	}
	session->privPassWord = objPtr;
	Tcl_IncrRefCount(objPtr);
	return TCL_OK;
    case optContext:
#ifdef TNM_SNMPv2U
	session->version = TNM_SNMPv2U;
#endif
	session->version = TNM_SNMPv3;
	(void) Tcl_GetStringFromObj(objPtr, &len);
#ifdef TNM_SNMPv2U
	if (len > USEC_MAX_CONTEXT) {
	    Tcl_SetResult(interp, "context name too long", TCL_STATIC);
	    return TCL_ERROR;
	}
#endif
	Tcl_DecrRefCount(session->context);
	session->context = objPtr;
	Tcl_IncrRefCount(objPtr);
	return TCL_OK;
    case optEngineID:
	if (Tcl_ConvertToType(interp, objPtr, &tnmOctetStringType) != TCL_OK) {
	    return TCL_ERROR;
	} 
#ifdef TNM_SNMPv2U
        session->version = TNM_SNMPv2U;
#endif
        session->version = TNM_SNMPv3;
	Tcl_DecrRefCount(session->engineID);
	session->engineID = objPtr;
        Tcl_IncrRefCount(objPtr);
        return TCL_OK;
    case optAuthKey:
        session->version = TNM_SNMPv3;
	if (session->usmAuthKey) {
	    Tcl_DecrRefCount(session->usmAuthKey);
	}
	if (session->authPassWord) {
	    Tcl_DecrRefCount(session->authPassWord);
	    session->authPassWord = NULL;
	}
	session->usmAuthKey = objPtr;
        Tcl_IncrRefCount(objPtr);
        return TCL_OK;
    case optPrivKey:
        session->version = TNM_SNMPv3;
	if (session->usmPrivKey) {
	    Tcl_DecrRefCount(session->usmPrivKey);
	}
	if (session->privPassWord) {
	    Tcl_DecrRefCount(session->privPassWord);
	    session->privPassWord = NULL;
	}
	session->usmPrivKey = objPtr;
        Tcl_IncrRefCount(objPtr);
        return TCL_OK;
    case optSecurityLevel:
	num = TnmGetTableKeyFromObj(interp, tnmSnmpSecurityLevelTable, 
				    objPtr, "SNMP security level");
	if (num == -1) {
	    return TCL_ERROR;
	}
	session->securityLevel = num;
	return TCL_OK;
    case optAlias: {
	Tcl_HashEntry *entryPtr;
	int code;
	char *alias;
	Tcl_DString dst;
	entryPtr = Tcl_FindHashEntry(&control->aliasTable, 
				     Tcl_GetStringFromObj(objPtr, NULL));
	if (! entryPtr) {
	    Tcl_AppendResult(interp, "unknown alias \"",
			     Tcl_GetStringFromObj(objPtr, NULL),
			     "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	alias = (char *) Tcl_GetHashValue(entryPtr);
	if (! alias) {
	    Tcl_SetResult(interp, "alias loop detected", TCL_STATIC);
	    return TCL_ERROR;
	}
	Tcl_SetHashValue(entryPtr, NULL);
	Tcl_DStringInit(&dst);
	Tcl_DStringAppend(&dst, 
			  Tcl_GetCommandName(interp, session->token), -1);
	Tcl_DStringAppend(&dst, " configure ", -1);
	Tcl_DStringAppend(&dst, alias, -1);
	code = Tcl_Eval(interp, Tcl_DStringValue(&dst));
	Tcl_SetHashValue(entryPtr, alias);
	Tcl_DStringFree(&dst);
	Tcl_ResetResult(interp);
	return code;
    }
    case optTransport:
	num = TnmGetTableKeyFromObj(interp, tnmSnmpDomainTable, 
				    objPtr, "SNMP transport domain");
	if (num == -1) {
	    return TCL_ERROR;
	}
	session->domain = num;
	return TCL_OK;
    case optTimeout:
	if (TnmGetPositiveFromObj(interp, objPtr, &num) != TCL_OK) {
	    return TCL_ERROR;
	}
	session->timeout = num;
	return TCL_OK;
    case optRetries:
	if (TnmGetUnsignedFromObj(interp, objPtr, &num) != TCL_OK) {
	    return TCL_ERROR;
	}
	session->retries = num;
	return TCL_OK;
    case optWindow:
	if (TnmGetUnsignedFromObj(interp, objPtr, &num) != TCL_OK) {
	    return TCL_ERROR;
	}
	session->window = num;
	return TCL_OK;
    case optDelay:
	if (TnmGetUnsignedFromObj(interp, objPtr, &num) != TCL_OK) {
	    return TCL_ERROR;
	}
	session->delay = num;
	return TCL_OK;
    case optTags:
	if (session->tagList) {
	    Tcl_DecrRefCount(session->tagList);
	}
	session->tagList = objPtr;
	Tcl_IncrRefCount(session->tagList);
	return TCL_OK;
    case optEnterprise: {
	TnmOid *oidPtr;
	oidPtr = TnmGetOidFromObj(interp, objPtr);
	if (! oidPtr) {
	    return TCL_ERROR;
	}
	TnmOidCopy(&session->enterpriseOid, oidPtr);
	return TCL_OK;
    }
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * BindEvent --
 *
 *	This procedure is invoked to process the "bind" command
 *	option of the snmp session commands.
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
BindEvent(Tcl_Interp *interp, TnmSnmp *session, Tcl_Obj *eventObjPtr, Tcl_Obj *script)
{
    TnmSnmpBinding *bindPtr;
    TnmTable *tablePtr = NULL;
    int event;
    
    /*
     * Check the allowed event types for the various session types.
     */

    switch (session->type) {
    case TNM_SNMP_GENERATOR:
	tablePtr = generatorEventTable;
	break;
    case TNM_SNMP_RESPONDER:
	tablePtr = responderEventTable;
	break;
    case TNM_SNMP_NOTIFIER:
	tablePtr = notifierEventTable;
	break;
    case TNM_SNMP_LISTENER:
	tablePtr = listenerEventTable;
	break;
    }
    
    event = TnmGetTableKey(tablePtr, Tcl_GetStringFromObj(eventObjPtr, NULL));

    if (event < 0) {
	Tcl_AppendResult(interp, "unknown event \"", 
			 Tcl_GetStringFromObj(eventObjPtr, NULL), 
			 "\": must be ", TnmGetTableValues(tablePtr),
			 (char *) NULL);
	return TCL_ERROR;
    }
    
    bindPtr = session->bindPtr;
    while (bindPtr && bindPtr->event != event) {
	bindPtr = bindPtr->nextPtr;
    }
    
    if (! script) {
	if (bindPtr) {
	    Tcl_SetResult(interp, bindPtr->command, TCL_STATIC);
	}
    } else {
	if (! bindPtr) {
	    bindPtr = (TnmSnmpBinding *) ckalloc(sizeof(TnmSnmpBinding));
	    memset((char *) bindPtr, 0, sizeof(TnmSnmpBinding));
	    bindPtr->event = event;
	    bindPtr->nextPtr = session->bindPtr;
	    session->bindPtr = bindPtr;
	}
	if (bindPtr->command) {
	    ckfree (bindPtr->command);
	}
	bindPtr->command = ckstrdup(Tcl_GetStringFromObj(script, NULL));
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FindSessions --
 *
 *	This procedure is invoked to process the "find" command
 *	option of the snmp command.
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
FindSessions(Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    int i, result, type = 0, version = 0;
    TnmSnmp *session;
    Tcl_Obj *listPtr, *patList = NULL;
    struct sockaddr_in addr;

    enum options { 
	optAddress, optPort, optTags, optType, optVersion
    } option;

    static CONST char *optionTable[] = {
	"-address", "-port", "-tags", "-type", "-version", (char *) NULL
    };

    if (objc % 2) {
	Tcl_WrongNumArgs(interp, 2, objv, "?option value? ?option value? ...");
	return TCL_ERROR;
    }

    for (i = 2; i < objc; i++) {
	result = Tcl_GetIndexFromObj(interp, objv[i++], optionTable, 
				     "option", TCL_EXACT, (int *) &option);
	if (result != TCL_OK) {
	    return result;
	}
	switch (option) {
	case optAddress:
	    result = TnmSetIPAddress(interp, 
				  Tcl_GetStringFromObj(objv[i], NULL), &addr);
	    if (result != TCL_OK) {
		return TCL_ERROR;
	    }
	    break;
	case optPort:
	    result = TnmSetIPPort(interp, "udp", 
				  Tcl_GetStringFromObj(objv[i], NULL), &addr);
	    if (result != TCL_OK) {
		return TCL_ERROR;
	    }
	    break;
	case optTags:
	    patList = objv[i];
	    break;
	case optType:
	    type = TnmGetTableKeyFromObj(interp, tnmSnmpApplTable, 
					 objv[i], "SNMP application type");
	    if (type == -1) {
		return TCL_ERROR;
	    }
	    break;
	case optVersion:
	    version = TnmGetTableKeyFromObj(interp, tnmSnmpVersionTable,
					    objv[i], "SNMP version");
	    if (version == -1) {
		return TCL_ERROR;
	    }
	    break;
	}
    }

    listPtr = Tcl_GetObjResult(interp);
    for (session = tnmSnmpList; session; session = session->nextPtr) {
	if (type && session->type != type) continue;
	if (version && session->version != version) continue;
	if (patList) {
	    int match = TnmMatchTags(interp, session->tagList, patList);
	    if (match < 0) {
		return TCL_ERROR;
	    }
	    if (! match) continue;
	}
	if (session->interp == interp) {
	    CONST char *cmdName = Tcl_GetCommandName(interp, session->token);
	    Tcl_Obj *elemObjPtr = Tcl_NewStringObj(cmdName, -1);
	    Tcl_ListObjAppendElement(interp, listPtr, elemObjPtr);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tnm_SnmpCmd --
 *
 *	This procedure is invoked to process the "snmp" command.
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
Tnm_SnmpObjCmd(ClientData clientData, Tcl_Interp *interp, int	objc, Tcl_Obj *CONST objv[])
{
    static int initialized = 0;
    static unsigned nextId = 0;

    TnmSnmp *session;
    int code, result = TCL_OK;
    Tcl_Obj *listPtr;
    char *name, *pattern;

    SnmpControl *control = (SnmpControl *) 
	Tcl_GetAssocData(interp, tnmSnmpControl, NULL);

    enum commands { 
	cmdAlias,
#if 0
	cmdArray,
#endif
	cmdDelta, cmdExpand, cmdFind, cmdGenerator, cmdInfo,
	cmdListener, cmdNotifier, cmdOid, cmdResponder,
	cmdType, cmdValue, cmdWait, cmdWatch 
    } cmd;

    static CONST char *cmdTable[] = {
	"alias",
#if 0
	"array",
#endif
	"delta", "expand", "find", "generator", "info",
	"listener", "notifier", "oid", "responder",
	"type", "value", "wait", "watch",
	(char *) NULL
    };

    enum infos { 
	infoDomains, infoErrors, infoExceptions, infoPDUs, infoSecurity,
	infoTypes, infoVersions 
    } info;

    static CONST char *infoTable[] = {
	"domains", "errors", "exceptions", "pdus", "security",
	"types", "versions", (char *) NULL
    };

    if (! control) {
	control = (SnmpControl *) ckalloc(sizeof(SnmpControl));
	memset((char *) control, 0, sizeof(SnmpControl));
	Tcl_InitHashTable(&control->aliasTable, TCL_STRING_KEYS);
	Tcl_SetAssocData(interp, tnmSnmpControl, AssocDeleteProc, 
			 (ClientData) control);
    }

    if (! initialized) {
	TnmSnmpSysUpTime();
	memset((char *) &tnmSnmpStats, 0, sizeof(TnmSnmpStats));
	srand((unsigned int) (time(NULL) * getpid()));
	initialized = 1;
    }

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    code = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
			       "option", TCL_EXACT, (int *) &cmd);
    if (code != TCL_OK) {
	return code;
    }

    switch (cmd) {
    case cmdAlias: {
        Tcl_HashEntry *entryPtr;
        if (objc == 2) {
	    Tcl_HashSearch search;
	    entryPtr = Tcl_FirstHashEntry(&control->aliasTable, &search);
	    while (entryPtr) {
	        Tcl_AppendElement(interp,
			  Tcl_GetHashKey(&control->aliasTable, entryPtr));
	        entryPtr = Tcl_NextHashEntry(&search);
	    }
	} else if (objc == 3) {
	    char *name = Tcl_GetStringFromObj(objv[2], NULL);
	    entryPtr = Tcl_FindHashEntry(&control->aliasTable, name);
	    if (entryPtr) {
	        Tcl_SetResult(interp, (char *) Tcl_GetHashValue(entryPtr),
			      TCL_STATIC);
	    }
	} else if (objc == 4) {
	    char *name = Tcl_GetStringFromObj(objv[2], NULL);
	    char *value = Tcl_GetStringFromObj(objv[3], NULL);
	    int isNew;
	    entryPtr = Tcl_CreateHashEntry(&control->aliasTable, name, &isNew);
	    if (!isNew) {
		ckfree((char *) Tcl_GetHashValue(entryPtr));
	    }
	    if (*value == '\0') {
		Tcl_DeleteHashEntry(entryPtr);
	    } else {
		Tcl_SetHashValue(entryPtr, ckstrdup(value));
	    }
	} else {
	    Tcl_WrongNumArgs(interp, 2, objv, "?agent? ?config?");
	    result = TCL_ERROR;
	    break;
	}
	break;
    }

#if 0
    case cmdArray: {
	Tcl_DString ds;
	Tcl_DStringInit(&ds);
	if (TnmMibLoad(interp) != TCL_OK) {
            result = TCL_ERROR;
            break;
        }
        if (argc != 5) {
            TnmWrongNumArgs(interp, 2, argv, "set arrayName varBindList");
            result = TCL_ERROR;
            break;
        }
	Tcl_DStringGetResult(interp, &ds);
	ScalarSetVar(interp, argv[4], argv[3], &ds);
	Tcl_DStringResult(interp, &ds);
	break;
    }
#endif

    case cmdDelta:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varBindList1 varBindList2");
	    result = TCL_ERROR;
	    break;
	}
	result = Delta(interp, objv[2], objv[3]);
	break;

    case cmdExpand:
        if (objc != 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varBindList");
	    result = TCL_ERROR;
	    break;
	}
	if (TnmMibLoad(interp) != TCL_OK) {
	    result = TCL_ERROR;
	    break;
	}
#if 0
	listPtr = TnmSnmpNorm(interp, objv[2], 0);
#else
	listPtr = TnmSnmpNorm(interp, objv[2], 
			      TNM_SNMP_NORM_OID | TNM_SNMP_NORM_INT);
#endif
	if (! listPtr) {
            result = TCL_ERROR;
        } else {
            Tcl_SetObjResult(interp, listPtr);
            result = TCL_OK;
        }
        break;

    case cmdFind:
	result = FindSessions(interp, objc, objv);
	break;

    case cmdGenerator:

	/* 
	 * Initialize the SNMP manager module by opening a socket for
	 * manager communication. Populate the MIB module with the
	 * set of default MIB definitions.
	 */

	if (TnmMibLoad(interp) != TCL_OK) {
	    result = TCL_ERROR;
	    break;
	}
	if (TnmSnmpManagerOpen(interp) != TCL_OK) {
	    result = TCL_ERROR;
	    break;
	}
	
	session = TnmSnmpCreateSession(interp, TNM_SNMP_GENERATOR);
	session->config = &generatorConfig;
	result = TnmSetConfig(interp, session->config,
			      (ClientData) session, objc, objv);
	if (result != TCL_OK) {
	    TnmSnmpDeleteSession(session);
	    break;
	}
#ifdef TNM_SNMPv2U
	if (session->version == TNM_SNMPv2U && session->qos & USEC_QOS_AUTH) {
	    TnmSnmpUsecGetAgentID(session);
	}
#endif
	TnmSnmpComputeKeys(session);

	session->nextPtr = tnmSnmpList;
	tnmSnmpList = session;

	/*
	 * Finally create a Tcl command for this session.
	 */

	name = TnmGetHandle(interp, "snmp", &nextId);
	session->token = Tcl_CreateObjCommand(interp, name, GeneratorCmd,
			  (ClientData) session, DeleteProc);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), name, -1);
	break;

    case cmdInfo:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "subject ?pattern?");
	    result = TCL_ERROR;
            break;
	}
	code = Tcl_GetIndexFromObj(interp, objv[2], infoTable, 
				   "option", TCL_EXACT, (int *) &info);
	if (code != TCL_OK) {
	    result = TCL_ERROR;
	    break;
	}
	pattern = (objc == 4) ? Tcl_GetStringFromObj(objv[3], NULL) : NULL;
	listPtr = Tcl_GetObjResult(interp);
	switch (info) {
	case infoDomains:
	    TnmListFromTable(tnmSnmpDomainTable, listPtr, pattern);
	    break;
	case infoErrors:
	    TnmListFromTable(tnmSnmpErrorTable, listPtr, pattern);
	    break;
	case infoExceptions:
	    TnmListFromTable(tnmSnmpExceptionTable, listPtr, pattern);
	    break;
	case infoPDUs:
	    TnmListFromTable(tnmSnmpPDUTable, listPtr, pattern);
	    break;
	case infoSecurity:
	    TnmListFromTable(tnmSnmpSecurityLevelTable, listPtr, pattern);
	    break;
	case infoTypes:
	    TnmListFromTable(tnmSnmpTypeTable, listPtr, pattern);
	    break;
	case infoVersions:
	    TnmListFromTable(tnmSnmpVersionTable, listPtr, pattern);
	    break;
	}
	break;

    case cmdListener:
	if (TnmMibLoad(interp) != TCL_OK) {
	    result = TCL_ERROR;
	    break;
	}
	/* 
	 * Initialize the SNMP manager module for sending responses to
	 * inform messages.
	 */
	if (TnmSnmpManagerOpen(interp) != TCL_OK) {
	    result = TCL_ERROR;
	    break;
	}
	session = TnmSnmpCreateSession(interp, TNM_SNMP_LISTENER);
	session->config = &listenerConfig;
	result = TnmSetConfig(interp, session->config,
			      (ClientData) session, objc, objv);
	if (result != TCL_OK) {
	    TnmSnmpDeleteSession(session);
	    break;
	}
	Tcl_ResetResult(interp);
	if (TnmSnmpListenerOpen(interp, session) != TCL_OK) {
	    TnmSnmpDeleteSession(session);
	    break;
	}
	
#ifdef TNM_SNMPv2U
	if (session->version == TNM_SNMPv2U && session->qos & USEC_QOS_AUTH) {
	    TnmSnmpUsecGetAgentID(session);
	}
#endif
	TnmSnmpComputeKeys(session);

	session->nextPtr = tnmSnmpList;
	tnmSnmpList = session;

	/*
	 * Finally create a Tcl command for this session.
	 */
	
	name = TnmGetHandle(interp, "snmp", &nextId);
	session->token = Tcl_CreateObjCommand(interp, name, ListenerCmd,
			  (ClientData) session, DeleteProc);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), name, -1);
	break;

    case cmdNotifier:
	if (TnmMibLoad(interp) != TCL_OK) {
	    result = TCL_ERROR;
	    break;
	}
	if (TnmSnmpManagerOpen(interp) != TCL_OK) {
	    result = TCL_ERROR;
	    break;
	}

	session = TnmSnmpCreateSession(interp, TNM_SNMP_NOTIFIER);
	session->config = &notifierConfig;
	result = TnmSetConfig(interp, session->config,
			      (ClientData) session, objc, objv);
	if (result != TCL_OK) {
	    TnmSnmpDeleteSession(session);
	    break;
	}
#ifdef TNM_SNMPv2U
	if (session->version == TNM_SNMPv2U && session->qos & USEC_QOS_AUTH) {
	    TnmSnmpUsecGetAgentID(session);
	}
#endif
	TnmSnmpComputeKeys(session);

	session->nextPtr = tnmSnmpList;
	tnmSnmpList = session;

	/*
	 * Finally create a Tcl command for this session.
	 */
	
	name = TnmGetHandle(interp, "snmp", &nextId);
	session->token = Tcl_CreateObjCommand(interp, name, NotifierCmd,
			  (ClientData) session, DeleteProc);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), name, -1);
	break;

    case cmdOid:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varBindList ?index?");
	    result = TCL_ERROR;
	    break;
	}
	if (TnmMibLoad(interp) != TCL_OK) {
	    result = TCL_ERROR;
	    break;
	}
	result = Extract(interp, 0, objv[2], objc == 4 ? objv[3] : NULL);
	break;

    case cmdResponder:
	if (TnmMibLoad(interp) != TCL_OK) {
	    result = TCL_ERROR;
	    break;
	}
	session = TnmSnmpCreateSession(interp, TNM_SNMP_RESPONDER);
	session->config = &responderConfig;
	result = TnmSetConfig(interp, session->config, 
			      (ClientData) session, objc, objv);
	if (result == TCL_OK) {
	    Tcl_Obj *resultPtr = Tcl_GetObjResult(interp);
	    Tcl_IncrRefCount(resultPtr);
	    Tcl_ResetResult(interp);
	    result = TnmSnmpAgentInit(interp, session);
	    if (result == TCL_OK) {
		Tcl_SetObjResult(interp, resultPtr);
	    }
	    Tcl_DecrRefCount(resultPtr);
	}
	if (result != TCL_OK) {
	    TnmSnmpDeleteSession(session);
	    break;
	}
#ifdef TNM_SNMPv2U
	if (session->version == TNM_SNMPv2U && session->qos & USEC_QOS_AUTH) {
	    TnmSnmpUsecGetAgentID(session);
	}
#endif
	TnmSnmpComputeKeys(session);

	session->nextPtr = tnmSnmpList;
	tnmSnmpList = session;

	/*
	 * Finally create a Tcl command for this session.
	 */
	
	name = TnmGetHandle(interp, "snmp", &nextId);
	session->token = Tcl_CreateObjCommand(interp, name, ResponderCmd,
			  (ClientData) session, DeleteProc);
	Tcl_SetStringObj(Tcl_GetObjResult(interp), name, -1);
	break;

    case cmdType:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varBindList ?index?");
	    result = TCL_ERROR;
	    break;
	}
	if (TnmMibLoad(interp) != TCL_OK) {
	    result = TCL_ERROR;
	    break;
	}
	result = Extract(interp, 1, objv[2], objc == 4 ? objv[3] : NULL);
	break;
	
    case cmdValue:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varBindList ?index?");
	    result = TCL_ERROR;
	    break;
	}
	if (TnmMibLoad(interp) != TCL_OK) {
	    result = TCL_ERROR;
	    break;
	}
	result = Extract(interp, 2, objv[2], objc == 4 ? objv[3] : NULL);
	break;

    case cmdWait:
        if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    result = TCL_ERROR;
	    break;
	}
      repeat:
	for (session = tnmSnmpList; session; session = session->nextPtr) {
	    if (TnmSnmpQueueRequest(session, NULL)) {
		Tcl_DoOneEvent(0);
		goto repeat;
	    }
	}
	break;

    case cmdWatch:
	if (objc > 3) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?bool?");
            result = TCL_ERROR;
	    break;
        }
	if (objc == 3) {
	    result = Tcl_GetBooleanFromObj(interp, objv[2], &hexdump);
	    if (result != TCL_OK) {
		break;
	    }
	}
	Tcl_SetBooleanObj(Tcl_GetObjResult(interp), hexdump);
	break;
    }

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * GeneratorCmd --
 *
 *	This procedure is invoked to process a manager command.
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
GeneratorCmd(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *CONST objv[])
{
    TnmSnmp *session = (TnmSnmp *) clientData;
    int code, nonReps, maxReps;

    enum commands {
	cmdBind, cmdCget, cmdConfigure, cmdDestroy, cmdGet, cmdGetBulk, cmdGetNext, 
#ifdef ASN1_SNMP_GETRANGE
	cmdGetRange, 
#endif
	cmdSet, cmdWait, cmdWalk
    } cmd;

    static CONST char *cmdTable[] = {
	"bind", "cget", "configure", "destroy", "get", "getbulk", "getnext",
#ifdef ASN1_SNMP_GETRANGE
 	"getrange", 
#endif
	"set", "wait", "walk", (char *) NULL
    };

    if (objc < 2) {
 	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
	return TCL_ERROR;
    }

    code = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
 			       "option", TCL_EXACT, (int *) &cmd);
    if (code != TCL_OK) {
 	return code;
    }

    switch (cmd) {
    case cmdCget:
	return TnmGetConfig(interp, session->config,
			    (ClientData) session, objc, objv);

    case cmdConfigure:

	/*
	 * This call to WaitSession() is needed to ensure that a 
	 * configuration change does not affect queued requests.
	 */

	Tcl_Preserve((ClientData) session);
	WaitSession(interp, session, 0);
 	code = TnmSetConfig(interp, session->config,
			    (ClientData) session, objc, objv);
	if (code != TCL_OK) {
	    Tcl_Release((ClientData) session);
	    return TCL_ERROR;
	}
#ifdef TNM_SNMPv2U
	if (session->version == TNM_SNMPv2U && session->qos & USEC_QOS_AUTH) {
	    TnmSnmpUsecGetAgentID(session);
	}
#endif
	TnmSnmpComputeKeys(session);

	if (session->domain == TNM_SNMP_TCP_DOMAIN) {
	    Tcl_Channel c;
	    Tcl_Obj *savedResult;

	    savedResult = Tcl_GetObjResult(interp);
	    Tcl_IncrRefCount(savedResult);
	    Tcl_ResetResult(interp);
	    c = Tcl_OpenTcpClient(interp,
				  ntohs(session->maddr.sin_port),
				  inet_ntoa(session->maddr.sin_addr),
				  0, 0, 0);
	    if (! c) {
		Tcl_Release((ClientData) session);
		Tcl_DecrRefCount(savedResult);
		session->domain = TNM_SNMP_UDP_DOMAIN;
		return TCL_ERROR;
	    }
	    Tcl_SetObjResult(interp, savedResult);
	    Tcl_DecrRefCount(savedResult);
	}

	Tcl_Release((ClientData) session);
	return TCL_OK;

    case cmdDestroy:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_DeleteCommandFromToken(interp, session->token);
	return TCL_OK;

    case cmdGet:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varBindList ?script?");
	    return TCL_ERROR;
	}
	return Request(interp, session, ASN1_SNMP_GET, 0, 0,
		       objv[2], (objc == 4) ? objv[3] : NULL);

    case cmdGetNext:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varBindList ?script?");
	    return TCL_ERROR;
	}
	return Request(interp, session, ASN1_SNMP_GETNEXT, 0, 0, 
		       objv[2], (objc == 4) ? objv[3] : NULL);

    case cmdGetBulk:
	if (objc < 5 || objc > 6) {
	    Tcl_WrongNumArgs(interp, 2, objv, 
		    "nonRepeaters maxRepetitions varBindList ?script?");
	    return TCL_ERROR;
	}
	if (TnmGetUnsignedFromObj(interp, objv[2], &nonReps) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (TnmGetPositiveFromObj(interp, objv[3], &maxReps) != TCL_OK) {
	    return TCL_ERROR;
	}
	return Request(interp, session, ASN1_SNMP_GETBULK, nonReps, maxReps,
		       objv[4], (objc == 6) ? objv[5] : NULL);

#ifdef ASN1_SNMP_GETRANGE
    case cmdGetRange:
	if (objc < 5 || objc > 6) {
	    Tcl_WrongNumArgs(interp, 2, objv, 
		    "nonRepeaters maxRepetitions varBindList ?script?");
	    return TCL_ERROR;
	}
	if (TnmGetUnsignedFromObj(interp, objv[2], &nonReps) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (TnmGetPositiveFromObj(interp, objv[3], &maxReps) != TCL_OK) {
	    return TCL_ERROR;
	}
	return Request(interp, session, ASN1_SNMP_GETRANGE, nonReps, maxReps,
		       objv[4], (objc == 6) ? objv[5] : NULL);
#endif

    case cmdSet:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "varBindList ?script?");
	    return TCL_ERROR;
	}
	return Request(interp, session, ASN1_SNMP_SET, 0, 0,
		       objv[2], (objc == 4) ? objv[3] : NULL);

    case cmdWait:
	if (objc == 2) {
	    return WaitSession(interp, session, 0);
	} else if (objc == 3) {
	    int request;
	    if (Tcl_GetIntFromObj(interp, objv[2], &request) != TCL_OK) {
		return TCL_ERROR;
	    }
	    return WaitSession(interp, session, request);
	}
	Tcl_WrongNumArgs(interp, 2, objv, "?request?");
	return TCL_ERROR;

    case cmdWalk:
	if (objc < 4 || objc > 5) {
	    Tcl_WrongNumArgs(interp, 2, objv, "?varName? varBindList script");
	    return TCL_ERROR;
	}
	return (objc == 4)
	    ? AsyncWalk(interp, session, objv[2], objv[3])
	    : SyncWalk(interp, session, objv[2], objv[3], objv[4]);

    case cmdBind:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "event ?command?");
	    return TCL_ERROR;
        }
	return BindEvent(interp, session, objv[2],
			 (objc == 4) ? objv[3] : NULL);
    }

    return TCL_OK;

#if 0
    /*
     * All commands handled below need a well configured session.
     * Check if we have one and return an error if something is
     * still incomplete.
     */

    if (Configured(interp, session) != TCL_OK) {
	return TCL_ERROR;
    }

    switch (cmd) {
    case cmdTbl:
	if (argc != 4) {
	    TnmWrongNumArgs(interp, 2, argv, "table arrayName");
	    return TCL_ERROR;
	}
	return Table(interp, session, argv[2], argv[3]);

    case cmdScalars:
	if (argc != 4) {
	    TnmWrongNumArgs(interp, 2, argv, "group arrayName");
	    return TCL_ERROR;
	}
	return Scalars(interp, session, argv[2], argv[3]);
    }
#endif

}

/*
 *----------------------------------------------------------------------
 *
 * ListenerCmd --
 *
 *	This procedure is invoked to process a listener command.
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
ListenerCmd(ClientData clientData, Tcl_Interp *interp, int	objc, Tcl_Obj *CONST objv[])
{
    TnmSnmp *session = (TnmSnmp *) clientData;
    int code;

    enum commands {
	cmdBind, cmdCget, cmdConfigure, cmdDestroy, cmdWait
    } cmd;

    static CONST char *cmdTable[] = {
	"bind", "cget", "configure", "destroy", "wait",
	(char *) NULL
    };
    
    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
        return TCL_ERROR;
    }

    code = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
			       "option", TCL_EXACT, (int *) &cmd);
    if (code != TCL_OK) {
	return code;
    }

    switch (cmd) {
    case cmdCget:
	return TnmGetConfig(interp, session->config,
			    (ClientData) session, objc, objv);

    case cmdConfigure:

	/*
	 * This call to WaitSession() is needed to ensure that a 
	 * configuration change does not affect queued requests.
	 */

	Tcl_Preserve((ClientData) session);
	WaitSession(interp, session, 0);
 	code = TnmSetConfig(interp, session->config,
			    (ClientData) session, objc, objv);
	if (code != TCL_OK) {
	    Tcl_Release((ClientData) session);
	    return TCL_ERROR;
	}
#ifdef TNM_SNMPv2U
	if (session->version == TNM_SNMPv2U && session->qos & USEC_QOS_AUTH) {
	    TnmSnmpUsecGetAgentID(session);
	}
#endif
	TnmSnmpComputeKeys(session);

	Tcl_Release((ClientData) session);
	return TCL_OK;

    case cmdDestroy:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_DeleteCommandFromToken(interp, session->token);
	return TCL_OK;

    case cmdWait:
	if (objc == 2) {
	    return WaitSession(interp, session, 0);
	} else if (objc == 3) {
	    int request;
	    if (Tcl_GetIntFromObj(interp, objv[2], &request) != TCL_OK) {
		return TCL_ERROR;
	    }
	    return WaitSession(interp, session, request);
	}
	Tcl_WrongNumArgs(interp, 2, objv, "?request?");
	return TCL_ERROR;

    case cmdBind:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "event ?command?");
	    return TCL_ERROR;
        }
	return BindEvent(interp, session, objv[2],
			 (objc == 4) ? objv[3] : NULL);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * NotifierCmd --
 *
 *	This procedure is invoked to process a notifier command.
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
NotifierCmd(ClientData clientData, Tcl_Interp *interp, int	objc, Tcl_Obj *CONST objv[])
{
    TnmSnmp *session = (TnmSnmp *) clientData;
    int code;

    enum commands {
	cmdBind, cmdCget, cmdConfigure, cmdDestroy, cmdInform, cmdTrap, cmdWait
    } cmd;

    static CONST char *cmdTable[] = {
	"bind", "cget", "configure", "destroy", "inform", "trap", "wait", 
	(char *) NULL
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
        return TCL_ERROR;
    }

    code = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
			       "option", TCL_EXACT, (int *) &cmd);
    if (code != TCL_OK) {
	return code;
    }

    switch (cmd) {
    case cmdCget:
	return TnmGetConfig(interp, session->config,
			    (ClientData) session, objc, objv);

    case cmdConfigure:

	/*
	 * This call to WaitSession() is needed to ensure that a 
	 * configuration change does not affect queued requests.
	 */

	Tcl_Preserve((ClientData) session);
	WaitSession(interp, session, 0);
 	code = TnmSetConfig(interp, session->config,
			    (ClientData) session, objc, objv);
	if (code != TCL_OK) {
	    Tcl_Release((ClientData) session);
	    return TCL_ERROR;
	}
#ifdef TNM_SNMPv2U
	if (session->version == TNM_SNMPv2U && session->qos & USEC_QOS_AUTH) {
	    TnmSnmpUsecGetAgentID(session);
	}
#endif
	TnmSnmpComputeKeys(session);

	Tcl_Release((ClientData) session);
	return TCL_OK;

    case cmdDestroy:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_DeleteCommandFromToken(interp, session->token);
	return TCL_OK;

    case cmdInform:
	if (objc < 4 || objc > 5) {
            Tcl_WrongNumArgs(interp, 2, objv,
			     "snmpTrapOID varBindList ?script?");
            return TCL_ERROR;
        }
	return Notify(interp, session, ASN1_SNMP_INFORM, objv[2],
		      objv[3], (objc == 5) ? objv[4] : NULL);

    case cmdTrap:
	if (objc != 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "snmpTrapOID varBindList");
	    return TCL_ERROR;
	}
	return Notify(interp, session, ASN1_SNMP_TRAP2, objv[2],
		      objv[3], NULL);

    case cmdWait:
	if (objc == 2) {
	    return WaitSession(interp, session, 0);
	} else if (objc == 3) {
	    int request;
	    if (Tcl_GetIntFromObj(interp, objv[2], &request) != TCL_OK) {
		return TCL_ERROR;
	    }
	    return WaitSession(interp, session, request);
	}
	Tcl_WrongNumArgs(interp, 2, objv, "?request?");
	return TCL_ERROR;

    case cmdBind:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "event ?command?");
	    return TCL_ERROR;
        }
	return BindEvent(interp, session, objv[2],
			 (objc == 4) ? objv[3] : NULL);
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ResponderCmd --
 *
 *	This procedure is invoked to process a agent command.
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
ResponderCmd(ClientData clientData, Tcl_Interp *interp, int	objc, Tcl_Obj *CONST objv[])
{
    TnmSnmp *session = (TnmSnmp *) clientData;
    int code;

    enum commands {
	cmdBind, cmdCget, cmdConfigure, cmdDestroy, cmdInstance
    } cmd;

    static CONST char *cmdTable[] = {
	"bind", "cget", "configure", "destroy", "instance",
	(char *) NULL
    };

    if (objc < 2) {
	Tcl_WrongNumArgs(interp, 1, objv, "option ?arg arg ...?");
        return TCL_ERROR;
    }

    code = Tcl_GetIndexFromObj(interp, objv[1], cmdTable, 
			       "option", TCL_EXACT, (int *) &cmd);
    if (code != TCL_OK) {
	return code;
    }

    switch (cmd) {
    case cmdCget:
	return TnmGetConfig(interp, session->config,
			    (ClientData) session, objc, objv);

    case cmdConfigure:

	/*
	 * This call to WaitSession() is needed to ensure that a 
	 * configuration change does not affect queued requests.
	 */

	Tcl_Preserve((ClientData) session);
	WaitSession(interp, session, 0);
 	code = TnmSetConfig(interp, session->config,
			    (ClientData) session, objc, objv);
	if (code == TCL_OK) {
	    code = TnmSnmpAgentInit(interp, session);
	}
	if (code != TCL_OK) {
	    Tcl_Release((ClientData) session);
	    return TCL_ERROR;
	}
#ifdef TNM_SNMPv2U
	if (session->version == TNM_SNMPv2U && session->qos & USEC_QOS_AUTH) {
	    TnmSnmpUsecGetAgentID(session);
	}
#endif
	TnmSnmpComputeKeys(session);

	Tcl_Release((ClientData) session);
	break;

    case cmdDestroy:
	if (objc != 2) {
	    Tcl_WrongNumArgs(interp, 2, objv, (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_DeleteCommandFromToken(interp, session->token);
	break;

    case cmdBind:
	if (objc < 3 || objc > 4) {
	    Tcl_WrongNumArgs(interp, 2, objv, "event ?command?");
	    return TCL_ERROR;
        }
	return BindEvent(interp, session, objv[2],
			 (objc == 4) ? objv[3] : NULL);
	break;

    case cmdInstance:
	if (objc < 4 || objc > 5) {
	    Tcl_WrongNumArgs(interp, 2, objv, "oid varName ?default?");
	    return TCL_ERROR;
	}
	code = TnmSnmpCreateNode(session->interp, 
				 Tcl_GetStringFromObj(objv[2], NULL), 
				 Tcl_GetStringFromObj(objv[3], NULL),
		    (objc > 4) ? Tcl_GetStringFromObj(objv[4], NULL) : "");
	if (code != TCL_OK) {
	    return code;
	}
	break;
    }

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WaitSession --
 *
 *	This procedure processes events until either the list of
 *	outstanding requests is empty or until the given request
 *	is no longer in the list of outstanding requests.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Tcl events are processed which can cause arbitrary side effects.
 *
 *----------------------------------------------------------------------
 */

static int
WaitSession(Tcl_Interp *interp, TnmSnmp *session, int request)
{
    CONST char *cmdName;
    char *name;

    cmdName = Tcl_GetCommandName(interp, session->token);
    if (! cmdName) {
	return TCL_OK;
    }

    /*
     * Do not use the session pointer! We have to search for the
     * session name after each single event because the session
     * may be deleted as a side effect of the event.
     */

    name = ckstrdup(cmdName);
  repeat:
    for (session = tnmSnmpList; session; session = session->nextPtr) {
	CONST char *thisName = Tcl_GetCommandName(interp, session->token);
	if (strcmp(thisName, name) != 0) continue;
	if (! request) {
	    if (TnmSnmpQueueRequest(session, NULL)) {
		Tcl_DoOneEvent(0);
		goto repeat;
	    }
	} else {
	    if (TnmSnmpFindRequest(request)) {
		Tcl_DoOneEvent(0);
		goto repeat;
	    }
	}
    }
    
    ckfree(name);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ResponseProc --
 *
 *	This procedure is called once we have received the response
 *	for an asynchronous SNMP request. It evaluates a Tcl script.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arbitrary side effects since commands are evaluated.
 *
 *----------------------------------------------------------------------
 */

static void
ResponseProc(TnmSnmp *session, TnmSnmpPdu *pdu, ClientData clientData)
{
    AsyncToken *atPtr = (AsyncToken *) clientData;
    TnmSnmpEvalCallback(atPtr->interp, session, pdu, 
			Tcl_GetStringFromObj(atPtr->tclCmd, NULL),
			NULL, NULL, NULL, NULL);
    Tcl_DecrRefCount(atPtr->tclCmd);
    ckfree((char *) atPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Notify --
 *
 *	This procedure sends out a notification (trap or inform).
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
Notify(Tcl_Interp *interp, TnmSnmp *session, int type, Tcl_Obj *oid, Tcl_Obj *vbl, Tcl_Obj *script)
{
    TnmSnmpPdu pdu;
    char *trapOid;
    
    PduInit(&pdu, session, type);
    trapOid = Tcl_GetStringFromObj(oid, NULL);
    if (TnmIsOid(trapOid)) {
	pdu.trapOID = ckstrdup(trapOid);
    } else {
	char *tmp = TnmMibGetOid(trapOid);
	if (! tmp) {
	    Tcl_AppendResult(interp, "unknown notification \"", 
			     trapOid, "\"", (char *) NULL);
	    PduFree(&pdu);
	    return TCL_ERROR;
	}
	pdu.trapOID = ckstrdup(tmp);
    }
    Tcl_DStringAppend(&pdu.varbind, Tcl_GetStringFromObj(vbl, NULL), -1);
    if (TnmSnmpEncode(interp, session, &pdu, NULL, NULL) != TCL_OK) {
	PduFree(&pdu);
	return TCL_ERROR;
    }
    PduFree(&pdu);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Request --
 *
 *	This procedure creates a pdu structure and calls TnmSnmpEncode
 *	to send the packet to the destination.
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
Request(Tcl_Interp *interp, TnmSnmp *session, int type, int non, int max, Tcl_Obj *vbList, Tcl_Obj *cmdObj)
{
    TnmSnmpPdu pdu;
    int code = TCL_OK;
    char *vbl = Tcl_GetStringFromObj(vbList, NULL);
    char *cmd = cmdObj ? Tcl_GetStringFromObj(cmdObj, NULL) : NULL;

    PduInit(&pdu, session, type);
    if (type == ASN1_SNMP_GETBULK) {
	pdu.errorStatus = non > 0 ? non : 0;
	pdu.errorIndex = max > 0 ? max : 0;
    }
    Tcl_DStringAppend(&pdu.varbind, vbl, -1);

    if (cmd) {
	AsyncToken *atPtr = (AsyncToken *) ckalloc(sizeof(AsyncToken));
	atPtr->interp = interp;
	atPtr->tclCmd = cmdObj;
	Tcl_IncrRefCount(atPtr->tclCmd);
	atPtr->oidList = NULL;
	code = TnmSnmpEncode(interp, session, &pdu, 
			     ResponseProc, (ClientData) atPtr);
	if (code != TCL_OK) {
	    Tcl_DecrRefCount(atPtr->tclCmd);
	    ckfree((char *) atPtr);
	}
    } else {
	code = TnmSnmpEncode(interp, session, &pdu, NULL, NULL);
    }

    PduFree(&pdu);
    return code;
}

/*
 *----------------------------------------------------------------------
 *
 * WalkCheck --
 *
 *	This procedure is used to implement MIB walks. It takes
 *	a list of object identifier values and a varbind list as
 *	input and checks whether the varbinds are contained in
 *	the subtrees defined by the object identifier values. This
 *	procedure also checks whether a syntax value of a varbind
 *	matches the endOfMibView exception.
 *
 * Results:
 *	A pointer to the varbind list or NULL if the varbind list
 *	elements are not contained in the subtrees defined by the
 *	object identifier list.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tcl_Obj*
WalkCheck(int oidListLen, Tcl_Obj **oidListElems, int vbListLen, Tcl_Obj **vbListElems)
{
    int i, code;
    Tcl_Obj *objPtr;

    /*
     * Check if all the object identifiers are in the subtree.
     */
    
    for (i = 0; i < oidListLen; i++) {
	code = Tcl_ListObjIndex(NULL, vbListElems[i], 0, &objPtr);
	if (code != TCL_OK || !objPtr) {
	    Tcl_Panic("WalkCheck: no object identifier in varbind list");
	}
	code = TnmOidInTree(TnmGetOidFromObj(NULL, oidListElems[i]),
			    TnmGetOidFromObj(NULL, objPtr));
	if (! code) {
	    return NULL;
	}
    }

    /*
     * Check if we got an endOfMibView exception.
     */

    for (i = 0; i < oidListLen; i++) {
	code = Tcl_ListObjIndex(NULL, vbListElems[i], 1, &objPtr);
	if (code != TCL_OK || !objPtr) {
	    Tcl_Panic("WalkCheck: no syntax in varbind list");
	}
	code = TnmGetTableKey(tnmSnmpExceptionTable,
			      Tcl_GetStringFromObj(objPtr, NULL));
	if (code == ASN1_END_OF_MIB_VIEW) {
	    return NULL;
	}
    }

    objPtr = Tcl_NewListObj(oidListLen, vbListElems);
    return objPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * AsyncWalkProc --
 *
 *	This procedure is called once we have received the response
 *	during an asynchronous SNMP walk. It evaluates a Tcl script
 *	and starts another SNMP getnext if we did not reach the end
 *	of the MIB view.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arbitrary side effects since commands are evaluated.
 *
 *----------------------------------------------------------------------
 */

static void
AsyncWalkProc(TnmSnmp *session, TnmSnmpPdu *pdu, ClientData clientData)
{
    AsyncToken *atPtr = (AsyncToken *) clientData;
    Tcl_Interp *interp = atPtr->interp;
    Tcl_Obj *vbList, *newList, **vbListElems, **oidListElems;
    int vbListLen, oidListLen;

#if 0
    if (pdu->errorStatus == TNM_SNMP_NOSUCHNAME) {
	pdu->errorStatus = TNM_SNMP_ENDOFWALK;
	TnmSnmpEvalCallback(interp, session, pdu, 
			    Tcl_GetStringFromObj(atPtr->tclCmd, NULL),
			    NULL, NULL, NULL, NULL);
	goto done;
    }
#endif

    if (pdu->errorStatus != TNM_SNMP_NOERROR) {
	TnmSnmpEvalCallback(interp, session, pdu, 
			    Tcl_GetStringFromObj(atPtr->tclCmd, NULL),
			    NULL, NULL, NULL, NULL);
	goto done;
    }

    vbList = Tcl_NewStringObj(Tcl_DStringValue(&pdu->varbind), 
			      Tcl_DStringLength(&pdu->varbind));
    
    if (Tcl_ListObjGetElements(interp, atPtr->oidList,
			       &oidListLen, &oidListElems) != TCL_OK) {
	Tcl_Panic("AsyncWalkProc: failed to split object identifier list");
    }
    
    if (Tcl_ListObjGetElements(interp, vbList,
			       &vbListLen, &vbListElems) != TCL_OK) {
	Tcl_Panic("AsyncWalkProc: failed to split varbind list");
    }
    
    newList = WalkCheck(oidListLen, oidListElems, vbListLen, vbListElems);
    Tcl_DecrRefCount(vbList);
    if (! newList) {
	pdu->errorStatus = TNM_SNMP_ENDOFWALK;
	Tcl_DStringFree(&pdu->varbind);
	TnmSnmpEvalCallback(interp, session, pdu, 
			    Tcl_GetStringFromObj(atPtr->tclCmd, NULL),
			    NULL, NULL, NULL, NULL);
	goto done;
    }
    TnmSnmpEvalCallback(interp, session, pdu, 
			Tcl_GetStringFromObj(atPtr->tclCmd, NULL),
			NULL, NULL, NULL, NULL);
    pdu->type = ASN1_SNMP_GETNEXT;
    pdu->requestId = TnmSnmpGetRequestId();
    (void) TnmSnmpEncode(interp, session, pdu, AsyncWalkProc, 
			 (ClientData) atPtr);
    Tcl_DecrRefCount(newList);
    return;

done:
    Tcl_DecrRefCount(atPtr->tclCmd);
    Tcl_DecrRefCount(atPtr->oidList);
    ckfree((char *) atPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * AsyncWalk --
 *
 *	This procedure walks a MIB tree. It evaluates the given Tcl
 *	command foreach varbind retrieved using getbulk requests.
 *	First, all variables contained in the list argument are
 *	converted to their OIDs. Then we start an asynchronous loop
 *	using gebulk requests until we get an error or until one
 *	returned variable starts with an OID not being a valid prefix.
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
AsyncWalk(Tcl_Interp *interp, TnmSnmp *session, Tcl_Obj *oidList, Tcl_Obj *tclCmd)
{
    TnmSnmpPdu pdu;
    int i, result;
    AsyncToken *atPtr;

    int oidListLen;
    Tcl_Obj **oidListElems;
    
    /*
     * Make sure our argument is a valid Tcl list where every 
     * element in the list is a valid object identifier.
     */

    result = Tcl_ListObjGetElements(interp, oidList,
				    &oidListLen, &oidListElems);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    if (oidListLen == 0) {
	return TCL_OK;
    }

    for (i = 0; i < oidListLen; i++) {
	TnmOid *oidPtr = TnmGetOidFromObj(interp, oidListElems[i]);
	if (! oidPtr) {
	    return TCL_ERROR;
	}
    }

    /*
     * The structure where we keep all information about this
     * asynchronous walk.
     */

    atPtr = (AsyncToken *) ckalloc(sizeof(AsyncToken));
    atPtr->interp = interp;
    atPtr->tclCmd = tclCmd;
    Tcl_IncrRefCount(atPtr->tclCmd);
    atPtr->oidList = oidList;
    Tcl_IncrRefCount(atPtr->oidList);

    PduInit(&pdu, session, ASN1_SNMP_GETNEXT);
    Tcl_DStringAppend(&pdu.varbind, Tcl_GetStringFromObj(oidList, NULL), -1);
    result = TnmSnmpEncode(interp, session, &pdu, 
			   AsyncWalkProc, (ClientData) atPtr);
    if (result != TCL_OK) {
	Tcl_DecrRefCount(atPtr->tclCmd);
	Tcl_DecrRefCount(atPtr->oidList);
	ckfree((char *) atPtr);
    }
    PduFree(&pdu);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * SyncWalk --
 *
 *	This procedure walks a MIB tree. It evaluates the given Tcl
 *	command foreach varbind retrieved using getbulk requests.
 *	First, all variables contained in the list argument are
 *	converted to their OIDs. Then we loop using gebulk requests
 *	until we get an error or until one returned variable starts
 *	with an OID not being a valid prefix.
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
SyncWalk(Tcl_Interp *interp, TnmSnmp *session, Tcl_Obj *varName, Tcl_Obj *oidList, Tcl_Obj *tclCmd)
{
    int i, j, result;
    TnmSnmpPdu pdu;
    int numRepeaters = 0;
    int oidListLen, vbListLen;
    Tcl_Obj **oidListElems, **vbListElems, *vbList;

    /* 
     * Set warpFactor and warpLimit variables defined below control
     * how we increate the numRepeaters parameter while walking the
     * MIB tree. We currently use the sequence 4 8 12 16 20 ... 48.
     *
     * The warpFactor defines the amount used to increase the value on
     * numRepeaters in every round. The warpLimit defines an upper
     * limit for numRepeaters. Note, the code below reduces the upper
     * limit whenever a response is received with contains an
     * "incomplete row".
     *
     * Some measurements show some real interesting effects. If you
     * increase the warp factor too much, you will risk timeouts
     * because the agent might need a lot of time to build the
     * response. If the agent does not cache response packets, you
     * will get very bad performance once the agents input queue fills
     * up with retries. Therefore, we have limited the `warp' factor to
     * 48 which works well with scotty's default parameters on an
     * Ethernet. (This number should not be hard coded but perhaps it
     * is better so because everyone should read this comment before
     * playing with these parameters.)
     */

    int warpLimit = 48;
    int warpFactor = 4;
    
    /*
     * Make sure our argument is a valid Tcl list where every 
     * element in the list is a valid object identifier.
     */

    result = Tcl_ListObjGetElements(interp, oidList,
				    &oidListLen, &oidListElems);
    if (result != TCL_OK) {
	return TCL_ERROR;
    }
    if (oidListLen == 0) {
	return TCL_OK;
    }

    PduInit(&pdu, session, ASN1_SNMP_GETBULK);

    for (i = 0; i < oidListLen; i++) {
	TnmOid *oidPtr = TnmGetOidFromObj(interp, oidListElems[i]);
	if (! oidPtr) {
	    PduFree(&pdu);
	    return TCL_ERROR;
	}
	Tcl_DStringAppendElement(&pdu.varbind, TnmOidToString(oidPtr));
    }

    while (1) {

	pdu.type        = ASN1_SNMP_GETBULK;
	pdu.requestId   = TnmSnmpGetRequestId();

	if (numRepeaters < warpLimit ) {
	    numRepeaters += warpFactor;
	}

	pdu.errorStatus = 0;
	pdu.errorIndex  = (numRepeaters / oidListLen > 0) 
	    ? numRepeaters / oidListLen : 1;

	result = TnmSnmpEncode(interp, session, &pdu, NULL, NULL);
	vbList = Tcl_GetObjResult(interp);
	if (result == TCL_ERROR && pdu.errorStatus == TNM_SNMP_NOSUCHNAME) {
	    result = TCL_OK;
	    break;
	}
	if (result != TCL_OK) {
            break;
        }
	
	result = Tcl_ListObjGetElements(interp, vbList,
					&vbListLen, &vbListElems);
	if (result != TCL_OK) {
	    break;
	}

	if (vbListLen < oidListLen) {
	    Tcl_SetResult(interp, "response with wrong # of varbinds",
			  TCL_STATIC);
	    result = TCL_ERROR;
	    break;
	}
	if (vbListLen % oidListLen) {
	    /*
	     * This case is legal according to RFC 1905 when an agent
	     * truncates the varbind list due to message size
	     * restrictions.  We handle this case by ignoring the
	     * trailing varbinds. Otherwise our walk would get out of
	     * sync. 
	     */
	    vbListLen -= vbListLen % oidListLen;
	    if (warpLimit > 0) {
		numRepeaters -= warpFactor;
		warpLimit = numRepeaters;
	    }
	}

	Tcl_IncrRefCount(vbList);
	for (j = 0; j < vbListLen / oidListLen; j++) {

	    Tcl_Obj *newList;

	    newList = WalkCheck(oidListLen, oidListElems, oidListLen,
			vbListElems + (j * oidListLen));
	    if (! newList) {
		Tcl_DecrRefCount(vbList);
		goto loopDone;
	    }

	    PduFree(&pdu);
	    Tcl_DStringAppend(&pdu.varbind, 
			      Tcl_GetStringFromObj(newList, NULL), -1);

	    if (Tcl_ObjSetVar2(interp, varName, (Tcl_Obj *) NULL,
			       newList, TCL_LEAVE_ERR_MSG) == NULL) {
		result = TCL_ERROR;
		Tcl_DecrRefCount(vbList);
		Tcl_DecrRefCount(newList);
		goto loopDone;
	    }

	    result = Tcl_EvalObj(interp, tclCmd);
	    if (result != TCL_OK) {
		if (result == TCL_CONTINUE) {
		    result = TCL_OK;
		} else if (result == TCL_BREAK) {
		    result = TCL_OK;
		    Tcl_DecrRefCount(vbList);
		    goto loopDone;
		} else if (result == TCL_ERROR) {
		    char msg[100];
		    sprintf(msg, "\n    (\"%s walk\" body line %d)",
			    Tcl_GetCommandName(interp, session->token),
			    Tcl_GetErrorLine(interp));
		    Tcl_AddErrorInfo(interp, msg);
		    Tcl_DecrRefCount(vbList);
		    goto loopDone;
		} else {
		    Tcl_DecrRefCount(vbList);
		    goto loopDone;
		}
	    }
	}
	Tcl_DecrRefCount(vbList);
    }

  loopDone:
    PduFree(&pdu);
    if (result == TCL_OK) {
	Tcl_ResetResult(interp);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * Delta --
 *
 *	This procedure takes two varbind lists and computes delta
 *	values for each TimeTicks, Counter32 and Counter64 value.
 *	The varbind lists must have the same structure, otherwise
 *	a Tcl error will be raised.
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
Delta(interp, obj1Ptr, obj2Ptr)
    Tcl_Interp *interp;
    Tcl_Obj *obj1Ptr;
    Tcl_Obj *obj2Ptr;
{
    Tcl_Obj *vbl1Ptr = NULL, *vbl2Ptr = NULL;
    int i, objc1, objc2;
    Tcl_Obj **objv1, **objv2;
    
    /*
     * The following Tcl_Objs are allocated once and reused whenever
     * we need to expand a varbind list containing object identifiers
     * without any value or type elements.
     */

    static Tcl_Obj *deltaType = NULL;
    static Tcl_Obj *delta32Type = NULL;
    static Tcl_Obj *delta64Type = NULL;
    static Tcl_Obj *deltaTicksType = NULL;
    static Tcl_Obj *emptyList = NULL;

    if (! deltaType) {
	deltaType = Tcl_NewStringObj("Delta", 5);
	Tcl_IncrRefCount(deltaType);
    }
    if (! delta32Type) {
	delta32Type = Tcl_NewStringObj("Delta32", 7);
	Tcl_IncrRefCount(delta32Type);
    }
    if (! delta64Type) {
	delta64Type = Tcl_NewStringObj("Delta64", 7);
	Tcl_IncrRefCount(delta64Type);
    }
    if (! deltaTicksType) {
	deltaTicksType = Tcl_NewStringObj("DeltaTicks", 10);
	Tcl_IncrRefCount(deltaTicksType);
    }
    if (! emptyList) {
	emptyList = Tcl_NewListObj(0, NULL);
	Tcl_IncrRefCount(emptyList);
    }

    /*
     * Normalize the arguments into varbind lists.
     */

    vbl1Ptr = TnmSnmpNorm(interp, obj1Ptr, 0);
    if (! vbl1Ptr) {
	goto errorExit;
    }
    vbl2Ptr = TnmSnmpNorm(interp, obj2Ptr, 0);
    if (! vbl2Ptr) {
	goto errorExit;
    }

    (void) Tcl_ListObjGetElements(interp, vbl1Ptr, &objc1, &objv1);
    (void) Tcl_ListObjGetElements(interp, vbl2Ptr, &objc2, &objv2);
	       
    if (objc1 != objc2) {
    mismatch:
	Tcl_SetResult(interp, "varbind lists do not match", TCL_STATIC);
	goto errorExit;
    }

    for (i = 0; i < objc1; i++) {
	int dummy, type1, type2;
	Tcl_Obj **vbv1, **vbv2, *listPtr;
	TnmOid *oid1Ptr, *oid2Ptr;
	
	(void) Tcl_ListObjGetElements(interp, objv1[i], &dummy, &vbv1);
	(void) Tcl_ListObjGetElements(interp, objv2[i], &dummy, &vbv2);
	oid1Ptr = TnmGetOidFromObj(interp, vbv1[0]);
	oid2Ptr = TnmGetOidFromObj(interp, vbv2[0]);
	if (TnmOidCompare(oid1Ptr, oid2Ptr) != 0) {
	    goto mismatch;
	}

	listPtr = Tcl_NewListObj(1, vbv1);
	
	type1 = TnmGetTableKeyFromObj(NULL, tnmSnmpTypeTable, vbv1[1], NULL);
	type2 = TnmGetTableKeyFromObj(NULL, tnmSnmpTypeTable, vbv2[1], NULL);
	if (type1 != type2) {
	    Tcl_DecrRefCount(listPtr);
	    goto mismatch;
	}

	switch (type1) {
	case ASN1_TIMETICKS:
	    Tcl_ListObjAppendElement(interp, listPtr, deltaTicksType);
	    break;
	case ASN1_COUNTER32:
	    Tcl_ListObjAppendElement(interp, listPtr, delta32Type);
	    break;
	case ASN1_COUNTER64:
	    Tcl_ListObjAppendElement(interp, listPtr, delta64Type);
	    break;
	default:
	    Tcl_ListObjAppendElement(interp, listPtr, deltaType);
	    break;
	}

	switch (type1) {
	case ASN1_TIMETICKS:
	case ASN1_COUNTER32: {
	    TnmUnsigned32 u1, u2;
	    (void) TnmGetUnsigned32FromObj(NULL, vbv1[2], &u1);
	    (void) TnmGetUnsigned32FromObj(NULL, vbv2[2], &u2);
	    Tcl_ListObjAppendElement(interp, listPtr,
				     TnmNewUnsigned32Obj(u2 - u1));
	    break;
	}
	case ASN1_COUNTER64: {
	    TnmUnsigned64 u1, u2;
	    (void) TnmGetUnsigned64FromObj(NULL, vbv1[2], &u1);
	    (void) TnmGetUnsigned64FromObj(NULL, vbv2[2], &u2);
	    Tcl_ListObjAppendElement(interp, listPtr,
				     TnmNewUnsigned64Obj(u2 - u1));
	    break;
	}
	default:
	    Tcl_ListObjAppendElement(interp, listPtr, emptyList);
	    break;
	}

	Tcl_ListObjAppendElement(interp, Tcl_GetObjResult(interp), listPtr);
    }

    Tcl_DecrRefCount(vbl1Ptr);
    Tcl_DecrRefCount(vbl2Ptr);
    return TCL_OK;

 errorExit:
    if (vbl1Ptr) Tcl_DecrRefCount(vbl1Ptr);
    if (vbl2Ptr) Tcl_DecrRefCount(vbl2Ptr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Extract --
 *
 *	This procedure extracts th OIDs, types or values out of a given
 *	varbind list.
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
Extract(Tcl_Interp *interp, int what, Tcl_Obj *objPtr, Tcl_Obj *indexObjPtr)
{
    Tcl_Obj *listPtr;
    Tcl_Obj **objv, **vbv;
    int i, objc, vbc, index = -1;

    if (what < 0 || what > 2) {
	Tcl_Panic("illegal selection value passed to Extract()");
    }

    if (indexObjPtr) {
	if (Tcl_GetIntFromObj(interp, indexObjPtr, &index) == TCL_OK) {
	    if (index < 0) {
		index = 0;
	    }
	} else {	    
	    char *s = Tcl_GetStringFromObj(indexObjPtr, NULL);
	    if (strcmp(s, "end") == 0) {
		index = -2;
	    } else {
		return TCL_ERROR;
	    }
	}
    }

    if (Tcl_ListObjGetElements(interp, objPtr, &objc, &objv) != TCL_OK) {
	return TCL_ERROR;
    }

    if (index > objc - 1 || index == -2) {
	index = objc - 1;
    }

    listPtr = Tcl_GetObjResult(interp);
    for (i = 0; i < objc; i++) {
	if (index < 0 || index == i) {
	    if (Tcl_ListObjGetElements(interp, objv[i],
				       &vbc, &vbv) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (vbc == 3) {
		if (index == i) {
		    Tcl_SetObjResult(interp, vbv[what]);
		    break;
		} else {
		    (void) Tcl_ListObjAppendElement(NULL, listPtr, vbv[what]);
		}
	    }
	}
    }

    return TCL_OK;
}
#if 0

/*
 *----------------------------------------------------------------------
 *
 * ExpandTable --
 *
 *	This procedure expands the list of table variables or a single
 *	table name into a list of MIB instances.
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
ExpandTable(Tcl_Interp *interp, char *tList, Tcl_DString *dst)
{
    int i, argc, code;
    char **argv = NULL;
    TnmMibNode *nodePtr, *entryPtr = NULL, *tablePtr = NULL;
    
    code = Tcl_SplitList(interp, tList, &argc, &argv);
    if (code != TCL_OK) {
	return TCL_ERROR;
    }

    for (i = 0; i < argc; i++) {

	/*
	 * Lookup the given object.
	 */

	nodePtr = TnmMibFindNode(argv[i], NULL, 0);
	if (! nodePtr) {
	    Tcl_AppendResult(interp, "unknown mib table \"", argv[i], "\"",
			     (char *) NULL);
	    ckfree((char *) argv);
	    return TCL_ERROR;
	}

	/*
	 * Locate the entry (SEQUENCE) that contains this object.
	 */

	switch (nodePtr->syntax) {
	  case ASN1_SEQUENCE:
	    entryPtr = nodePtr;
	    break;
	  case ASN1_SEQUENCE_OF: 
	    if (nodePtr->childPtr) {
		entryPtr = nodePtr->childPtr;
	    }
	    break;
	  default:
	    if (nodePtr->parentPtr && nodePtr->childPtr == NULL
		&& nodePtr->parentPtr->syntax == ASN1_SEQUENCE) {
		entryPtr = nodePtr->parentPtr;
	    } else {
	    unknownTable:
		Tcl_AppendResult(interp, "not a table \"", argv[i], "\"",
				 (char *) NULL);
		ckfree((char *) argv);
		return TCL_ERROR;
	    }
	}

	/*
	 * Check whether all objects belong to the same table.
	 */

	if (entryPtr == NULL || entryPtr->parentPtr == NULL) {
	    goto unknownTable;
	}

	if (tablePtr == NULL) {
	    tablePtr = entryPtr->parentPtr;
	}
	if (tablePtr != entryPtr->parentPtr) {
	    Tcl_AppendResult(interp, "instances not in the same table",
			     (char *) NULL);
	    ckfree((char *) argv);
	    return TCL_ERROR;
	}

	/*
	 * Now add the nodes to the list. Expand SEQUENCE nodes to
	 * include all child nodes. Check the access mode which must
	 * allow at least read access.
	 */

	if (nodePtr == entryPtr || nodePtr == tablePtr) {
	    TnmMibNode *nPtr;
	    for (nPtr = entryPtr->childPtr; nPtr; nPtr=nPtr->nextPtr) {
		if (nPtr->access != TNM_MIB_NOACCESS) {
		    Tcl_DStringAppendElement(dst, nPtr->label);
		}
	    }
	} else {
	    if (nodePtr->access != TNM_MIB_NOACCESS) {
		Tcl_DStringAppendElement(dst, nodePtr->label);
	    }
	}
    }

    ckfree((char *) argv);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Table --
 *
 *	This procedure retrieves a conceptual SNMP table and stores
 *	the values in a Tcl array.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Tcl variables are modified.
 *
 *----------------------------------------------------------------------
 */

static int
Table(Tcl_Interp *interp, TnmSnmp *session, char *table, char *arrayName)
{
    int i, largc, code;
    TnmSnmpPdu _pdu, *pdu = &_pdu;
    Tcl_DString varList;
    char **largv;
    
    /*
     * A special hack to make sure that the given array name 
     * is actually known as an array.
     */

    Tcl_SetVar2(interp, arrayName, "foo", "", 0);
    Tcl_UnsetVar(interp, arrayName, 0);

    /*
     * Initialize the PDU.
     */

    pdu->addr        = session->maddr;
    pdu->type        = ASN1_SNMP_GETBULK;
    pdu->requestId   = TnmSnmpGetRequestId();
    pdu->errorStatus = TNM_SNMP_NOERROR;
    pdu->errorIndex  = 0;    
    pdu->trapOID     = NULL;
    Tcl_DStringInit(&pdu->varbind);
    Tcl_DStringInit(&varList);

    /*
     * Expand the given table list to create the complete getnext varbind.
     */

    code = ExpandTable(interp, table, &varList);
    if (code != TCL_OK) {
        return TCL_ERROR;
    }

    if (Tcl_DStringLength(&varList) == 0) {
	return TCL_OK;
    }

    /*
     *
     */

    code = Tcl_SplitList(interp, Tcl_DStringValue(&varList),
			 &largc, &largv);
    if (code != TCL_OK) {
	Tcl_DStringFree(&varList);
	return TCL_ERROR;
    }

    for (i = 0; i < largc; i++) {
	TnmWriteMessage(largv[i]);
	TnmWriteMessage("\n");
    }

    ckfree((char *) largv);
    Tcl_DStringFree(&varList);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ExpandScalars --
 *
 *	This procedure expands the list of scalar or group names
 *	into a list of MIB instances.
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
ExpandScalars(Tcl_Interp *interp, char *sList, Tcl_DString *dst)
{
    int argc, code, i;
    char **argv = NULL;
    TnmMibNode *nodePtr;
    TnmOid oid;

    code = Tcl_SplitList(interp, sList, &argc, &argv);
    if (code != TCL_OK) {
	return TCL_ERROR;
    }

    TnmOidInit(&oid);
    for (i = 0; i < argc; i++) {

	nodePtr = TnmMibFindNode(argv[i], NULL, 0);
	if (nodePtr == NULL) {
	    Tcl_AppendResult(interp, "unknown mib object \"", argv[i], "\"",
			     (char *) NULL);
	    ckfree((char *) argv);
	    return TCL_ERROR;
	}

	/*
	 * Skip the node if it is a table or an entry node.
	 */

	if (nodePtr->syntax == ASN1_SEQUENCE 
	    || nodePtr->syntax == ASN1_SEQUENCE_OF) {
	    continue;
	}

	/*
	 * Try to expand to child nodes if the node has child nodes, 
	 * Ignore all nodes which itsef have childs and which are
	 * not accessible.
	 */

	if (nodePtr->childPtr) {
	    for (nodePtr = nodePtr->childPtr; 
		 nodePtr; nodePtr=nodePtr->nextPtr) {
		if (nodePtr->access == TNM_MIB_NOACCESS || nodePtr->childPtr) {
		    continue;
		}
		TnmMibNodeToOid(nodePtr, &oid);
		TnmOidAppend(&oid, 0);
		Tcl_DStringAppendElement(dst, TnmOidToString(&oid));
		TnmOidFree(&oid);
	    }

	} else if (nodePtr->access != TNM_MIB_NOACCESS) {
	    TnmMibNodeToOid(nodePtr, &oid);
	    TnmOidAppend(&oid, 0);
	    Tcl_DStringAppendElement(dst, TnmOidToString(&oid));
	    TnmOidFree(&oid);

	} else {
	    Tcl_AppendResult(interp, "object \"", argv[0],
			     "\" not accessible", (char *) NULL);
	    ckfree((char *) argv);
	    return TCL_ERROR;
	}
    }

    ckfree((char *) argv);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Scalars --
 *
 *	This procedure extracts the variables and values contained in
 *	the varbindlist vbl and set corresponding array Tcl variables.
 *	The list of array names modified is appended to result.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Tcl variables are modified.
 *
 *----------------------------------------------------------------------
 */

static int
Scalars(Tcl_Interp *interp, TnmSnmp *session, char *group, char *arrayName)
{
    int i, largc, code;
    TnmSnmpPdu _pdu, *pdu = &_pdu;
    Tcl_DString varList;
    Tcl_DString result;
    char **largv;
    
    /*
     * A special hack to make sure that the given array name 
     * is actually known as an array.
     */

    Tcl_SetVar2(interp, arrayName, "foo", "", 0);
    Tcl_UnsetVar(interp, arrayName, 0);

    /*
     * Initialize the PDU.
     */

    pdu->addr        = session->maddr;
    pdu->type        = ASN1_SNMP_GET;
    pdu->requestId   = TnmSnmpGetRequestId();
    pdu->errorStatus = TNM_SNMP_NOERROR;
    pdu->errorIndex  = 0;    
    pdu->trapOID     = NULL;
    Tcl_DStringInit(&pdu->varbind);
    Tcl_DStringInit(&varList);
    Tcl_DStringInit(&result);

    /*
     * Expand the given scalar list to create the complete get varbind.
     */

    code = ExpandScalars(interp, group, &varList);
    if (code != TCL_OK) {
        return TCL_ERROR;
    }

    if (Tcl_DStringLength(&varList) == 0) {
	return TCL_OK;
    }

    /*
     * First try to retrieve all variables in one get request. This
     * may fail because the PDU causes a tooBig error or the agent
     * responds to missing variables with a noSuchName error.
     */

    Tcl_DStringAppend(&pdu->varbind, Tcl_DStringValue(&varList),
		      Tcl_DStringLength(&varList));
    code = TnmSnmpEncode(interp, session, pdu, NULL, NULL);
    if (code == TCL_OK) {	
	ScalarSetVar(interp, interp->result, arrayName, &result);
	Tcl_DStringFree(&varList);
	Tcl_DStringResult(interp, &result);
	return TCL_OK;
    }

    /*
     * Stop if we got no response since the agent is not
     * talking to us. This saves some time-outs.
     */

    if (strcmp(interp->result, "noResponse") == 0) {
	return TCL_ERROR;
    }

    /*
     * If we had no success, try every single varbind with one
     * request. Ignore errors so we just collect existing variables.
     */

    code = Tcl_SplitList(interp, Tcl_DStringValue(&varList),
			 &largc, &largv);
    if (code != TCL_OK) {
	Tcl_DStringFree(&varList);
	return TCL_ERROR;
    }

    for (i = 0; i < largc; i++) {

	pdu->type        = ASN1_SNMP_GET;
	pdu->requestId   = TnmSnmpGetRequestId();
	pdu->errorStatus = TNM_SNMP_NOERROR;
	pdu->errorIndex  = 0;    
	Tcl_DStringInit(&pdu->varbind);
	Tcl_DStringAppend(&pdu->varbind, largv[i], -1);

	code = TnmSnmpEncode(interp, session, pdu, NULL, NULL);
	if (code != TCL_OK) {
	    continue;
	}

	ScalarSetVar(interp, interp->result, arrayName, &result);
    }
    ckfree((char *) largv);
    Tcl_DStringFree(&varList);
    Tcl_DStringResult(interp, &result);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * ScalarSetVar --
 *
 *	This procedure extracts the variables and values contained in
 *	the varbindlist vbl and set corresponding array Tcl variables.
 *	The list of array names modified is appended to result.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tcl variables are modified.
 *
 *----------------------------------------------------------------------
 */

static void
ScalarSetVar(Tcl_Interp *interp, char *vbl, char *varName, Tcl_DString *result)
{
    int i, code;
    char *name;
    TnmSnmpVarBind *vbPtr;
    TnmMibNode *nodePtr;
    TnmVector vector;

    TnmVectorInit(&vector);
    code = TnmSnmpVarBindFromString(interp, &vector, vbl);
    if (code != TCL_OK) {
	return;
    }
    
    for (i = 0; i < TnmVectorSize(&vector); i++) {
	vbPtr = (TnmSnmpVarBind *) TnmVectorGet(&vector, i);
	if (TnmSnmpException(vbPtr->syntax)) {
	    continue;
	}
	name = TnmOidToString(&vbPtr->oid);
	nodePtr = TnmMibFindNode(name, NULL, 0);
	if (nodePtr) {
	    name = nodePtr->label;
	}
	Tcl_SetVar2(interp, varName, name, TnmSnmpValueToString(vbPtr), 0);
	Tcl_DStringAppendElement(result, name);
    }

    TnmSnmpVarBindFree(&vector);
    TnmVectorFree(&vector);
}
#endif

