/*
 * tnmSnmpAgent.c --
 *
 *	This is the SNMP agent interface of scotty.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2001 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmSnmpAgent.c,v 1.2 2008/02/13 16:44:05 karl Exp $
 */

#include "tnmSnmp.h"
#include "tnmMib.h"

/*
 * The following structure is used to implement a cache that
 * is used to remember queries so that we can respond to
 * retries quickly. This is needed because side effects can
 * break the agent down if we do them for each retry.
 */

typedef struct CacheElement {
    TnmSnmp *session;
    TnmSnmpPdu request;
    TnmSnmpPdu response;
    time_t timestamp;
} CacheElement;

#define CACHE_SIZE 64
static CacheElement cache[CACHE_SIZE];

/*
 * Flags used by the SNMP set processing code to keep state information
 * about individual variables in the varbind list.
 */

#define NODE_CREATED 0x01

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
CacheInit		(void);

static TnmSnmpPdu*
CacheGet		(TnmSnmp *session, TnmSnmpPdu *pdu);

static TnmSnmpPdu*
CacheHit		(TnmSnmp *session, TnmSnmpPdu *pdu);

static char*
TraceSysUpTime		(ClientData clientData,
				     Tcl_Interp *interp,
				     char *name1, char *name2, int flags);
#ifdef TNM_SNMPv2U
static char*
TraceAgentTime		(ClientData clientData,
				     Tcl_Interp *interp,
				     char *name1, char *name2, int flags);
#endif

static char*
TraceUnsignedInt	(ClientData clientData,
				     Tcl_Interp *interp,
				     char *name1, char *name2, int flags);
static TnmSnmpNode*
FindInstance		(TnmSnmp *session, TnmOid *oidPtr);

static TnmSnmpNode*
FindNextInstance	(TnmSnmp *session, TnmOid *oidPtr);

static int
GetRequest		(Tcl_Interp *interp, TnmSnmp *session,
				     TnmSnmpPdu *request, TnmSnmpPdu *response);
static int
SetRequest		(Tcl_Interp *interp, TnmSnmp *session,
				     TnmSnmpPdu *request, TnmSnmpPdu *response);


/*
 * The global variable to keep the snmp statistics is defined here.
 */

TnmSnmpStats tnmSnmpStats;

/*
 * The following table is used to register build-in instances
 * bound to counter inside of the protocol stack.
 */

struct StatReg {
    char *name;
    unsigned int *value;
};

static struct StatReg statTable[] = {
    { "snmpInPkts.0",		      &tnmSnmpStats.snmpInPkts },
    { "snmpOutPkts.0",		      &tnmSnmpStats.snmpOutPkts },
    { "snmpInBadVersions.0",	      &tnmSnmpStats.snmpInBadVersions },
    { "snmpInBadCommunityNames.0",    &tnmSnmpStats.snmpInBadCommunityNames },
    { "snmpInBadCommunityUses.0",     &tnmSnmpStats.snmpInBadCommunityUses },
    { "snmpInASNParseErrs.0",	      &tnmSnmpStats.snmpInASNParseErrs },
    { "snmpInTooBigs.0",	      &tnmSnmpStats.snmpInTooBigs },
    { "snmpInNoSuchNames.0",	      &tnmSnmpStats.snmpInNoSuchNames },
    { "snmpInBadValues.0",	      &tnmSnmpStats.snmpInBadValues },
    { "snmpInReadOnlys.0",	      &tnmSnmpStats.snmpInReadOnlys },
    { "snmpInGenErrs.0",	      &tnmSnmpStats.snmpInGenErrs },
    { "snmpInTotalReqVars.0",	      &tnmSnmpStats.snmpInTotalReqVars },
    { "snmpInTotalSetVars.0",	      &tnmSnmpStats.snmpInTotalSetVars },
    { "snmpInGetRequests.0",	      &tnmSnmpStats.snmpInGetRequests },
    { "snmpInGetNexts.0",	      &tnmSnmpStats.snmpInGetNexts },
    { "snmpInSetRequests.0",	      &tnmSnmpStats.snmpInSetRequests },
    { "snmpInGetResponses.0",	      &tnmSnmpStats.snmpInGetResponses },
    { "snmpInTraps.0",		      &tnmSnmpStats.snmpInTraps },
    { "snmpOutTooBigs.0",	      &tnmSnmpStats.snmpOutTooBigs },
    { "snmpOutNoSuchNames.0",	      &tnmSnmpStats.snmpOutNoSuchNames },
    { "snmpOutBadValues.0",	      &tnmSnmpStats.snmpOutBadValues },
    { "snmpOutGenErrs.0",	      &tnmSnmpStats.snmpOutGenErrs },
    { "snmpOutGetRequests.0",	      &tnmSnmpStats.snmpOutGetRequests },
    { "snmpOutGetNexts.0",	      &tnmSnmpStats.snmpOutGetNexts },
    { "snmpOutSetRequests.0",	      &tnmSnmpStats.snmpOutSetRequests },
    { "snmpOutGetResponses.0",	      &tnmSnmpStats.snmpOutGetResponses },
    { "snmpOutTraps.0",		      &tnmSnmpStats.snmpOutTraps },
    { "snmpStatsPackets.0",	      &tnmSnmpStats.snmpStatsPackets },
    { "snmpStats30Something.0",	      &tnmSnmpStats.snmpStats30Something },
    { "snmpStatsUnknownDstParties.0", &tnmSnmpStats.snmpStatsUnknownDstParties },
    { "snmpStatsDstPartyMismatches.0",&tnmSnmpStats.snmpStatsDstPartyMismatches },
    { "snmpStatsUnknownSrcParties.0", &tnmSnmpStats.snmpStatsUnknownSrcParties},
    { "snmpStatsBadAuths.0",	      &tnmSnmpStats.snmpStatsBadAuths },
    { "snmpStatsNotInLifetimes.0",    &tnmSnmpStats.snmpStatsNotInLifetimes },
    { "snmpStatsWrongDigestValues.0", &tnmSnmpStats.snmpStatsWrongDigestValues },
    { "snmpStatsUnknownContexts.0",   &tnmSnmpStats.snmpStatsUnknownContexts },
    { "snmpStatsBadOperations.0",     &tnmSnmpStats.snmpStatsBadOperations },
    { "snmpStatsSilentDrops.0",	      &tnmSnmpStats.snmpStatsSilentDrops },
    { "snmpV1BadCommunityNames.0",    &tnmSnmpStats.snmpInBadCommunityNames },
    { "snmpV1BadCommunityUses.0",     &tnmSnmpStats.snmpInBadCommunityUses },
#ifdef TNM_SNMPv2U
    { "usecStatsUnsupportedQoS.0",
      &tnmSnmpStats.usecStatsUnsupportedQoS },
    { "usecStatsNotInWindows.0",
      &tnmSnmpStats.usecStatsNotInWindows },
    { "usecStatsUnknownUserNames.0",
      &tnmSnmpStats.usecStatsUnknownUserNames },
    { "usecStatsWrongDigestValues.0",
      &tnmSnmpStats.usecStatsWrongDigestValues },
    { "usecStatsUnknownContexts.0",
      &tnmSnmpStats.usecStatsUnknownContexts },
    { "usecStatsUnknownBadParameters.0", 
      &tnmSnmpStats.usecStatsBadParameters },
    { "usecStatsUnauthorizedOperations.0",   
      &tnmSnmpStats.usecStatsUnauthorizedOperations },
#endif
    { 0, 0 }
};


/*
 *----------------------------------------------------------------------
 *
 * CacheInit --
 *
 *	This procedure initializes the cache of answered requests.
 *	This procedure is only called once when we become an agent.
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
CacheInit()
{
    int i;
    memset((char *) cache, 0, sizeof(cache));
    for (i = 0; i < CACHE_SIZE; i++) {
	Tcl_DStringInit(&cache[i].request.varbind);
	Tcl_DStringInit(&cache[i].response.varbind);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CacheGet --
 *
 *	This procedure gets a free cache element. The cache is a
 *	simple FIFO ring buffer, so we just return the next element.
 *
 * Results:
 *	A pointer to a free cache element.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmSnmpPdu*
CacheGet(session, pdu)
    TnmSnmp *session;
    TnmSnmpPdu *pdu;
{
    static int last = 0;
    last = (last + 1 ) % CACHE_SIZE;
    Tcl_DStringFree(&cache[last].request.varbind);
    Tcl_DStringFree(&cache[last].response.varbind);
    cache[last].session = session;
    cache[last].response.requestId = 0;
    cache[last].response.errorStatus = TNM_SNMP_NOERROR;
    cache[last].response.errorIndex = 0;
    cache[last].response.addr = pdu->addr;
    Tcl_DStringAppend(&cache[last].request.varbind, 
		      Tcl_DStringValue(&pdu->varbind), 
		      Tcl_DStringLength(&pdu->varbind));
    cache[last].timestamp = time((time_t *) NULL);
    return &(cache[last].response);
}

/*
 *----------------------------------------------------------------------
 *
 * CacheHit --
 *
 *	This procedure checks if the request identified by session and
 *	request id is in the cache so we can send the answer without
 *	further processing.
 *
 * Results:
 *      A pointer to the PDU or NULL if the lookup failed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmSnmpPdu*
CacheHit(session, pdu)
    TnmSnmp *session;
    TnmSnmpPdu *pdu;
{
    int i;
    time_t now = time((time_t *) NULL);

    /*
     * Never try to lookup request id 0 because there are some
     * management applications that always use the request id 0.
     */

    if (pdu->requestId == 0) {
	return NULL;
    }

    for (i = 0; i < CACHE_SIZE; i++) {
	if (cache[i].response.requestId != pdu->requestId
            || cache[i].session != session) {
	    continue;
	}
	if (!cache[i].timestamp || now - cache[i].timestamp > 5) {
	    continue;
	}
	if (cache[i].response.requestId == pdu->requestId
	    && cache[i].session == session
	    && Tcl_DStringLength(&pdu->varbind) 
	       == Tcl_DStringLength(&cache[i].request.varbind)
	    && strcmp(Tcl_DStringValue(&pdu->varbind), 
		      Tcl_DStringValue(&cache[i].request.varbind)) == 0
	    ) {
	    cache[i].response.addr = pdu->addr;
	    return &cache[i].response;
	}
    }
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * CacheClear --
 *
 *	This procedure clears the cache for a given session.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
CacheClear(session)
    TnmSnmp *session;
{
    int i;

    for (i = 0; i < CACHE_SIZE; i++) {
	if (cache[i].session == session) {
	    cache[i].timestamp = 0;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TraceSysUpTime --
 *
 *	This procedure is a trace callback which is called by the
 *	Tcl interpreter whenever the sysUpTime variable is read.
 *
 * Results:
 *      Always NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char*
TraceSysUpTime(clientData, interp, name1, name2, flags)
    ClientData clientData;
    Tcl_Interp *interp;
    char *name1;
    char *name2;
    int flags;
{
    char buf[20];
    sprintf(buf, "%u", TnmSnmpSysUpTime());
    Tcl_SetVar2(interp, name1, name2, buf, TCL_GLOBAL_ONLY);
    return NULL;
}

#ifdef TNM_SNMPv2U
/*
 *----------------------------------------------------------------------
 *
 * TraceAgentTime --
 *
 *	This procedure is a trace callback which is called by the
 *	Tcl interpreter whenever the usecAgentTime variable is read.
 *
 * Results:
 *      Always NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char*
TraceAgentTime(clientData, interp, name1, name2, flags)
    ClientData clientData;
    Tcl_Interp *interp;
    char *name1;
    char *name2;
    int flags;
{
    char buf[20];
    sprintf(buf, "%u", TnmSnmpSysUpTime() / 100);
    Tcl_SetVar2(interp, name1, name2, buf, TCL_GLOBAL_ONLY);
    return NULL;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * TraceUnsignedInt --
 *
 *	This procedure writes the unsigned value pointed to by
 *	clientData into the Tcl variable under trace. Used to
 *	implement snmp statistics.
 *
 * Results:
 *      Always NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char*
TraceUnsignedInt(clientData, interp, name1, name2, flags)
    ClientData clientData;
    Tcl_Interp *interp;
    char *name1;
    char *name2;
    int flags;
{
    char buf[20];
    sprintf(buf, "%u", *(unsigned *) clientData);
    Tcl_SetVar2(interp, name1, name2, buf, TCL_GLOBAL_ONLY);
    return NULL;    
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpAgentInit --
 *
 *	This procedure initializes the agent by registering some
 *	default MIB variables.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmSnmpAgentInit(interp, session)
    Tcl_Interp *interp;
    TnmSnmp *session;
{
    static int done = 0;
    char tclvar[80], buffer[255];
    const char *value;
    struct StatReg *p;

    if (TnmSnmpResponderOpen(session->interp, session) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Make sure we are only called once - at least until we support
     * multiple agent entities in one scotty process.
     */

    if (done) {
	return TCL_OK;
    }

    done = 1;
    CacheInit();

    /*
     * Here we build up our engineID value. This roughly conformes to
     * the "description" in RFC 2271, which is IMHO not a real cool
     * thing.
     */
    
    Tcl_SetObjLength(session->engineID, 12);
    {
	u_char *p = Tcl_GetStringFromObj(session->engineID, NULL);
	int id = 1575;
	*p++ = (id >> 24) & 0xff;
	*p++ = (id >> 16) & 0xff;
	*p++ = (id >> 8) & 0xff;
	*p++ = id & 0xff;
	*p++ = 0x04;
	memcpy(p, "smile:)", 7);
    }
    session->engineTime = time((time_t *) NULL);
    session->engineBoots = session->engineTime - 849394800;

#ifdef TNM_SNMPv2U
    /*
     * This is a hack. We are required to store these values
     * in NVRAM - I need to think about how to implement this.
     * For now, we assume that the agent rebooted every second
     * since the beginning of the USEC epoch (1/1/1996 0:0:0).
     */
    {
	u_char *p = session->agentID;
	int id = 1575;
	*p++ = (id >> 24) & 0xff;
	*p++ = (id >> 16) & 0xff;
	*p++ = (id >> 8) & 0xff;
	*p++ = id & 0xff;
	id = session->maddr.sin_addr.s_addr;
	*p++ = (id >> 24) & 0xff;
	*p++ = (id >> 16) & 0xff;
	*p++ = (id >> 8) & 0xff;
	*p++ = id & 0xff;
	memcpy(p, "tubs", 4);
    }
    session->agentTime = time((time_t *) NULL);
    session->agentBoots = session->agentTime - 820454400;
    TnmSnmpUsecSetAgentID(session);
#endif

    strcpy(buffer, "Tnm SNMP agent");
    value = Tcl_GetVar2(interp, "tnm", "version", TCL_GLOBAL_ONLY);
    if (value) {
	strcat(buffer, " version ");
        strcat(buffer, value);
    }
    value = Tcl_GetVar2(interp, "tnm", "arch", TCL_GLOBAL_ONLY);
    if (value) {
	strcat(buffer, " (");
        strcat(buffer, value);
	strcat(buffer, ")");
    }

    /*
     * Copy the variable in a writable memory location because Tcl_SetVar
     * tries to modify the name which can cause core dumps depending on
     * how clever the compiler is.
     */

    TnmSnmpCreateNode(interp, "sysDescr.0", 
		       "tnm_system(sysDescr)", buffer);
    TnmSnmpCreateNode(interp, "sysObjectID.0", 
		       "tnm_system(sysObjectID)", "1.3.6.1.4.1.1575.1.1");
    TnmSnmpCreateNode(interp, "sysUpTime.0", 
		       "tnm_system(sysUpTime)", "0");
    Tcl_TraceVar2(interp, "tnm_system", "sysUpTime", 
		  TCL_TRACE_READS | TCL_GLOBAL_ONLY, 
		  TraceSysUpTime, (ClientData) NULL);
    TnmSnmpCreateNode(interp, "sysContact.0", 
		       "tnm_system(sysContact)", "");
    TnmSnmpCreateNode(interp, "sysName.0", 
		       "tnm_system(sysName)", "");
    TnmSnmpCreateNode(interp, "sysLocation.0", 
		       "tnm_system(sysLocation)", "");
    TnmSnmpCreateNode(interp, "sysServices.0", 
		       "tnm_system(sysServices)", "72");

    for (p = statTable; p->name; p++) {
	strcpy(tclvar, "tnm_snmp(");
	strcat(tclvar, p->name);
	strcat(tclvar, ")");
	TnmSnmpCreateNode(interp, p->name, tclvar, "0");
	Tcl_TraceVar2(interp, "tnm_snmp", p->name, 
		      TCL_TRACE_READS | TCL_GLOBAL_ONLY,
		      TraceUnsignedInt, (ClientData) p->value);
    }

    /* XXX snmpEnableAuthenTraps.0 should be implemented */

#ifdef TNM_SNMPv2U
    TnmHexEnc((char *) session->agentID, USEC_MAX_AGENTID, buffer);
    TnmSnmpCreateNode(interp, "agentID.0", "tnm_usec(agentID)", buffer);
    sprintf(buffer, "%u", session->agentBoots);
    TnmSnmpCreateNode(interp, "agentBoots.0", "tnm_usec(agentBoots)", buffer);
    TnmSnmpCreateNode(interp, "agentTime.0", "tnm_usec(agentTime)", "0");
    Tcl_TraceVar2(interp, "tnm_usec", "agentTime",
		  TCL_TRACE_READS | TCL_GLOBAL_ONLY, 
		  TraceAgentTime, (ClientData) NULL);
    sprintf(buffer, "%d", session->maxSize);
    TnmSnmpCreateNode(interp, "agentSize.0", "tnm_usec(agentSize)", buffer);
#endif

    Tcl_ResetResult(interp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * FindInstance --
 *
 *	This procedure locates the instance given by the oid in the
 *	instance tree. Ignores all tree nodes without a valid syntax
 *	that are only used internally as non leaf nodes.
 *
 * Results:
 *      A pointer to the instance or NULL is not found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmSnmpNode*
FindInstance(session, oidPtr)
    TnmSnmp *session;
    TnmOid *oidPtr;
{
    TnmSnmpNode *inst = TnmSnmpFindNode(session, oidPtr);
    return (inst && inst->syntax) ? inst : NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * FindNextInstance --
 *
 *	This procedure locates the next instance given by the oid in
 *	the instance tree. Ignores all tree nodes without a valid 
 *	syntax that are only used internally as non leaf nodes.
 *
 * Results:
 *      A pointer to the instance or NULL is not found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmSnmpNode*
FindNextInstance(session, oidPtr)
    TnmSnmp *session;
    TnmOid *oidPtr;
{
    TnmSnmpNode *inst = TnmSnmpFindNextNode(session, oidPtr);
    return (inst && inst->syntax) ? inst : NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * GetRequest --
 *
 *	This procedure is called to process get, getnext and getbulk
 *	requests.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
GetRequest(interp, session, request, response)
    Tcl_Interp *interp;
    TnmSnmp *session;
    TnmSnmpPdu *request;
    TnmSnmpPdu *response;
{
    int i, code;
    TnmSnmpNode *inst;
    Tcl_Obj *vbList, **vbListElems;
    int vbListLen;

    vbList = Tcl_NewStringObj(Tcl_DStringValue(&request->varbind), -1);
    code = Tcl_ListObjGetElements((Tcl_Interp *) NULL, vbList,
				  &vbListLen, &vbListElems);
    if (code != TCL_OK) {
	Tcl_DecrRefCount(vbList);
	return TCL_ERROR;
    }

    for (i = 0; i < vbListLen; i++) {

	char *syntax;
	const char *value;
	Tcl_Obj *objPtr;
	TnmOid *oidPtr;

	(void) Tcl_ListObjIndex(interp, vbListElems[i], 0, &objPtr);
	oidPtr = TnmGetOidFromObj(interp, objPtr);
	if (! oidPtr) {
	    response->errorStatus = TNM_SNMP_GENERR;
	    tnmSnmpStats.snmpOutGenErrs++;
	    goto varBindError;
	}
	if (request->type == ASN1_SNMP_GETNEXT 
	    || request->type == ASN1_SNMP_GETBULK) {
	    inst = FindNextInstance(session, oidPtr);
	} else {
	    inst = FindInstance(session, oidPtr);
	}

	if (! inst) {

	    char *soid;

	    /*
	     * SNMPv1 handles this case by sending back an error PDU
	     * while SNMPv2 uses exceptions. We create an exception
	     * varbind to reflect the error types defined in RFC 1905.
	     */

	    if (session->version == TNM_SNMPv1) {
		response->errorStatus = TNM_SNMP_NOSUCHNAME;
		tnmSnmpStats.snmpOutNoSuchNames++;
		goto varBindError;
	    }

	    soid = TnmOidToString(oidPtr);
	    Tcl_DStringStartSublist(&response->varbind);
	    Tcl_DStringAppendElement(&response->varbind, soid);
	    if (request->type == ASN1_SNMP_GET) {
		TnmMibNode *nodePtr = TnmMibFindNode(soid, NULL, 0);
		if (!nodePtr || nodePtr->childPtr) {
		    Tcl_DStringAppendElement(&response->varbind,
					     "noSuchObject");
		} else {
		    Tcl_DStringAppendElement(&response->varbind, 
					     "noSuchInstance");
		}
	    } else {
		Tcl_DStringAppendElement(&response->varbind, "endOfMibView");
	    }
	    Tcl_DStringAppendElement(&response->varbind, "");
	    Tcl_DStringEndSublist(&response->varbind);
	    continue;
	}

	Tcl_DStringStartSublist(&response->varbind);
	Tcl_DStringAppendElement(&response->varbind, inst->label);
	syntax = TnmGetTableValue(tnmSnmpTypeTable, (unsigned) inst->syntax);
	Tcl_DStringAppendElement(&response->varbind, syntax ? syntax : "");

	(void) Tcl_ListObjIndex(interp, vbListElems[i], 2, &objPtr);
	code = TnmSnmpEvalNodeBinding(session, request, inst, 
				      TNM_SNMP_GET_EVENT, 
				      Tcl_GetStringFromObj(objPtr, NULL),
				      (char *) NULL);
	if (code == TCL_ERROR) {
	    goto varBindTclError;
	}
	value = Tcl_GetVar(interp, inst->tclVarName, 
			   TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG);
	if (!value) {
	    response->errorStatus = TNM_SNMP_GENERR;
	    tnmSnmpStats.snmpOutGenErrs++;
	    goto varBindError;
	}
	Tcl_DStringAppendElement(&response->varbind, value);
	Tcl_ResetResult(interp);

	tnmSnmpStats.snmpInTotalReqVars++;

	Tcl_DStringEndSublist(&response->varbind);
	
	continue;

      varBindTclError:
	response->errorStatus = TnmGetTableKey(tnmSnmpErrorTable, 
						Tcl_GetStringResult (interp));
	if (response->errorStatus < 0) {
	    response->errorStatus = TNM_SNMP_GENERR;
	}
	tnmSnmpStats.snmpOutGenErrs += 
	    (response->errorStatus == TNM_SNMP_GENERR);
       
      varBindError:
	response->errorIndex = i+1;
	break;
    }

    /*
     * We check here if the varbind length in string representation
     * exceeds our buffer used to build the packet. This is not always
     * correct, but we should be on the safe side in most cases.
     */

    if (Tcl_DStringLength(&response->varbind) >= TNM_SNMP_MAXSIZE) {
	response->errorStatus = TNM_SNMP_TOOBIG;
	response->errorIndex = 0;
    }

    Tcl_DecrRefCount(vbList);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SetRequest --
 *
 *	This procedure is called to process set requests which is
 *	much more difficult to do than get* requests.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
SetRequest(interp, session, request, response)
    Tcl_Interp *interp;
    TnmSnmp *session;
    TnmSnmpPdu *request;
    TnmSnmpPdu *response;
{
    int i, code;
    TnmOid oid;
    SNMP_VarBind *inVarBindPtr;
    int inVarBindSize;
    TnmSnmpNode *inst;
    int varsToRollback = 0;

    TnmOidInit(&oid);

    code = Tnm_SnmpSplitVBList(interp, Tcl_DStringValue(&request->varbind),
			       &inVarBindSize, &inVarBindPtr);
    if (code != TCL_OK) {
	return TCL_ERROR;
    }

    for (i = 0; i < inVarBindSize; i++) {

	const char *value;
	char *syntax;
	int setAlreadyDone = 0;
	varsToRollback = i;

	TnmOidFromString(&oid, inVarBindPtr[i].soid);
	inst = FindInstance(session, &oid);
	TnmOidFree(&oid);

	if (! inst) {
	    TnmMibNode *nodePtr;
	    nodePtr = TnmMibFindNode(inVarBindPtr[i].soid, NULL, 0);
	    if (nodePtr && nodePtr->access == TNM_MIB_READCREATE) {
		char *name = TnmMibGetName(inVarBindPtr[i].soid, 0);
		char *tmp = ckalloc(strlen(name) + 2);
		char *c = tmp;
		strcpy(tmp, name);
		for (c = tmp; *c && *c != '.'; c++);
		if (*c) *c = '(';
		while (*c) c++;
		*c++ = ')';
		*c = '\0';
		TnmSnmpCreateNode(interp, inVarBindPtr[i].soid, tmp, "");
		inVarBindPtr[i].flags |= NODE_CREATED;
		ckfree(tmp);
		TnmOidFromString(&oid, inVarBindPtr[i].soid);
		inst = FindInstance(session, &oid);
		if (! inst) {
		    response->errorStatus = TNM_SNMP_NOCREATION;
		    varsToRollback--;
		    goto varBindError;
		}
		TnmOidFree(&oid);
		code = TnmSnmpEvalNodeBinding(session, request, inst, 
				    TNM_SNMP_CREATE_EVENT, inVarBindPtr[i].value,
				    (char *) NULL);
		if (code == TCL_ERROR) {
		    goto varBindTclError;
		}
		if (code != TCL_BREAK) {
		    Tcl_SetVar(interp, inst->tclVarName, 
			       inVarBindPtr[i].value, TCL_GLOBAL_ONLY);
		}
		setAlreadyDone = 1;
	    } else {
		response->errorStatus = TNM_SNMP_NOCREATION;
		varsToRollback--;
		goto varBindError;
	    }
	}

	if (!setAlreadyDone) {

	    /*
	     * Check if the instance is writable.
	     */

	    if (inst->access == TNM_MIB_READONLY) {
		response->errorStatus = TNM_SNMP_NOTWRITABLE;
		varsToRollback--;
		goto varBindError;
	    }

	    /*
	     * Check if the received value is of approriate type.
	     */
	    
	    if (TnmGetTableKey(tnmSnmpTypeTable, 
			       inVarBindPtr[i].syntax) != inst->syntax) {
		response->errorStatus = TNM_SNMP_WRONGTYPE;
		varsToRollback--;
		goto varBindError;
	    }

	    value = Tcl_GetVar(interp, inst->tclVarName, 
			       TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG);
	    if (value == NULL) {
		inVarBindPtr[i].clientData = (ClientData) NULL;
	    } else {
		inVarBindPtr[i].clientData = (ClientData) ckstrdup(value);
	    }
	    code = TnmSnmpEvalNodeBinding(session, request, inst, 
					TNM_SNMP_SET_EVENT, inVarBindPtr[i].value,
					(char *) inVarBindPtr[i].clientData);
	    if (code == TCL_ERROR) {
		goto varBindTclError;
	    }
	    if (code != TCL_BREAK) {
	        if (Tcl_SetVar(interp, inst->tclVarName, 
			       inVarBindPtr[i].value, 
			       TCL_GLOBAL_ONLY) == NULL) {
		    goto varBindTclError;
		}
	    }
	    tnmSnmpStats.snmpInTotalSetVars++;
	}
	
	Tcl_DStringStartSublist(&response->varbind);
	Tcl_DStringAppendElement(&response->varbind, inst->label);
	syntax = TnmGetTableValue(tnmSnmpTypeTable, (unsigned) inst->syntax);
	Tcl_DStringAppendElement(&response->varbind, syntax ? syntax : "");
	value = Tcl_GetVar(interp, inst->tclVarName, 
			   TCL_GLOBAL_ONLY | TCL_LEAVE_ERR_MSG);
	if (!value) {
	    response->errorStatus = TNM_SNMP_GENERR;
	    goto varBindError;
	}
	Tcl_DStringAppendElement(&response->varbind, value);
	Tcl_ResetResult(interp);

	Tcl_DStringEndSublist(&response->varbind);
	
	continue;

      varBindTclError:
	response->errorStatus = TnmGetTableKey(tnmSnmpErrorTable, 
						Tcl_GetStringResult (interp));
	if (response->errorStatus < 0) {
	    response->errorStatus = TNM_SNMP_GENERR;
	}
	tnmSnmpStats.snmpOutGenErrs += 
	    (response->errorStatus == TNM_SNMP_GENERR);
       
      varBindError:
	response->errorIndex = i+1;
	break;
    }

    /*
     * We check here if the varbind length in string representation
     * exceeds our buffer used to build the packet. This is not always
     * correct, but we should be on the safe side in most cases.
     */
    
    if (Tcl_DStringLength(&response->varbind) >= TNM_SNMP_MAXSIZE) {
	response->errorStatus = TNM_SNMP_TOOBIG;
	response->errorIndex = 0;
    }

    /*
     * Another check for consistency errors before we start 
     * the commit/rollback phase. This additional check was 
     * suggested by Peter.Polkinghorne@gec-hrc.co.uk.
     */
    
    if (response->errorStatus == TNM_SNMP_NOERROR) {
	for (i = 0; i < inVarBindSize; i++) {
	    TnmOidFromString(&oid, inVarBindPtr[i].soid);
	    inst = FindInstance(session, &oid);
	    TnmOidFree(&oid);
	    if (inst) {
		code = TnmSnmpEvalNodeBinding(session, request, inst, 
				    TNM_SNMP_CHECK_EVENT, inVarBindPtr[i].value,
				    (char *) inVarBindPtr[i].clientData);
	    } else {
	        Tcl_ResetResult(interp);
	        code = TCL_ERROR;
	    }

	    if (code != TCL_OK) {
		response->errorStatus = TnmGetTableKey(tnmSnmpErrorTable, 
							Tcl_GetStringResult (interp));
		if (response->errorStatus < 0) {
		    response->errorStatus = TNM_SNMP_GENERR;
		}
		tnmSnmpStats.snmpOutGenErrs +=
		    (response->errorStatus == TNM_SNMP_GENERR);
		response->errorIndex = i+1;
		break;
	    }
	}
    }

    /*
     * We now start the commit/rollback phase. Note, we must be
     * careful to do rollbacks in the correct order and only
     * on those instances that were actually processed.
     */
	
    if (response->errorStatus == TNM_SNMP_NOERROR) {

        /*
	 * Evaluate commit bindings if we have no error yet.
	 * Ignore all errors now since we have already decided 
	 * that this PDU has been processed successfully.
	 */
      
        for (i = 0; i < inVarBindSize; i++) {
	    TnmOidFromString(&oid, inVarBindPtr[i].soid);
	    inst = FindInstance(session, &oid);
	    TnmOidFree(&oid);
	    if (inst) {
	        TnmSnmpEvalNodeBinding(session, request, inst,
			     TNM_SNMP_COMMIT_EVENT, inVarBindPtr[i].value,
			     (char *) inVarBindPtr[i].clientData);
	    }
	    if (inVarBindPtr[i].clientData) {
	        char *oldValue = (char *) inVarBindPtr[i].clientData;
	        ckfree(oldValue);
	        inVarBindPtr[i].clientData = NULL;
	    }
	}

    } else {

        /*
	 * Evaluate the rollback bindings for all the instances
	 * that have been processed so far. Ignore all errors now
	 * as they would overwrite the error that caused us to
	 * rollback the set request processing.
	 */

        for (i = varsToRollback; i >= 0; i--) {
	    TnmOidFromString(&oid, inVarBindPtr[i].soid);
	    inst = FindInstance(session, &oid);
	    TnmOidFree(&oid);
	    if (inst) {
	        TnmSnmpEvalNodeBinding(session, request, inst, 
			     TNM_SNMP_ROLLBACK_EVENT, inVarBindPtr[i].value,
			     (char *) inVarBindPtr[i].clientData);
		if (inVarBindPtr[i].flags & NODE_CREATED) {
		    Tcl_UnsetVar(interp, inst->tclVarName, TCL_GLOBAL_ONLY);
		} else if (inVarBindPtr[i].clientData) {
		    Tcl_SetVar(interp, inst->tclVarName,
			       (char *) inVarBindPtr[i].clientData,
			       TCL_GLOBAL_ONLY);
		}
		if (inVarBindPtr[i].clientData) {
		    ckfree((char *) inVarBindPtr[i].clientData);
		    inVarBindPtr[i].clientData = NULL;
		}
	    }
	}
    }

    Tnm_SnmpFreeVBList(inVarBindSize, inVarBindPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpAgentRequest --
 *
 *	This procedure is called when the agent receives a get, getnext
 *	getbulk or set request. It splits the varbind, looks up the 
 *	variables and assembles a response.
 *
 * Results:
 *      A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmSnmpAgentRequest(interp, session, pdu)
    Tcl_Interp *interp;
    TnmSnmp *session;
    TnmSnmpPdu *pdu;
{
    int rc;
    TnmSnmpPdu *reply;

    switch (pdu->type) {
      case ASN1_SNMP_GET:
	  tnmSnmpStats.snmpInGetRequests++;
	  break;
      case ASN1_SNMP_GETNEXT:
	  tnmSnmpStats.snmpInGetNexts++;
	  break;
      case ASN1_SNMP_GETBULK:
	  break;
      case ASN1_SNMP_SET:
	  tnmSnmpStats.snmpInSetRequests++;
	  break;
      case ASN1_SNMP_INFORM:
	  break;

    }

    if (pdu->type == ASN1_SNMP_SET) {
	CacheClear(session);
    }

    reply = CacheHit(session, pdu);
    if (reply != NULL) {
	rc = TnmSnmpEncode(interp, session, reply, NULL, NULL);
	return rc;
    }

    TnmSnmpEvalBinding(interp, session, pdu, TNM_SNMP_BEGIN_EVENT);

    reply = CacheGet(session, pdu);

    if (pdu->type == ASN1_SNMP_SET) {
	rc = SetRequest(interp, session, pdu, reply);
    } else {
	rc = GetRequest(interp, session, pdu, reply);
    }
    if (rc != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Throw the reply varbind away and re-use the one in the original
     * request if we had an error.
     */

    if (reply->errorStatus != TNM_SNMP_NOERROR) {
	Tcl_DStringFree(&reply->varbind);
	Tcl_DStringAppend(&reply->varbind,
			  Tcl_DStringValue(&pdu->varbind),
			  Tcl_DStringLength(&pdu->varbind));
    }
 
    reply->type = ASN1_SNMP_RESPONSE;
    reply->requestId = pdu->requestId;

    TnmSnmpEvalBinding(interp, session, reply, TNM_SNMP_END_EVENT);

    if (TnmSnmpEncode(interp, session, reply, NULL, NULL) != TCL_OK) {
	Tcl_AddErrorInfo(interp, "\n    (snmp send reply)");
	Tcl_BackgroundError(interp);
	Tcl_ResetResult(interp);
	reply->errorStatus = TNM_SNMP_GENERR;
	Tcl_DStringFree(&reply->varbind);
        Tcl_DStringAppend(&reply->varbind,
			  Tcl_DStringValue(&pdu->varbind),
			  Tcl_DStringLength(&pdu->varbind));
	return TnmSnmpEncode(interp, session, reply, NULL, NULL);
    } else {
	return TCL_OK;
    }
}
