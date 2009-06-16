/*
 * tnmSnmpRecv.c --
 *
 *	This file contains all functions that decode a received SNMP
 *	packet and do the appropriate actions. 
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2002 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmSnmpRecv.c,v 1.2 2006/12/07 12:20:58 karl Exp $
 */

#include "tnmSnmp.h"
#include "tnmMib.h"

/* 
 * Flag that controls hexdump. See the watch command for its use.
 */

extern int hexdump;

/*
 * A structure to keep the important parts of the message header 
 * while processing incoming SNMP messages.
 */

typedef struct Message {
    int version;
    int comLen;
    u_char *com;
    u_char *authDigest;
    int authDigestLen;
    char *msgFlags;
    int msgID;
#ifdef TNM_SNMPv2U
    u_char qos;
    u_char agentID[USEC_MAX_AGENTID];
    u_int agentBoots;
    u_int agentTime;
    int userNameLen;
    char userName[USEC_MAX_USER];
    int cntxtLen;
    char cntxt[USEC_MAX_CONTEXT];
#endif
    char *user;
    int userLength;
    int maxSize;
    char *engineID;
    int engineIDLength;
    int engineBoots;
    int engineTime;
} Message;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int
Authentic		_ANSI_ARGS_((TnmSnmp *session,
				     Message *msg, TnmSnmpPdu *pdu,
				     u_char *packet, int packetlen,
				     u_int **snmpStatPtr));

static int
DecodeMessage		_ANSI_ARGS_((Tcl_Interp	*interp, 
				     Message *msg, TnmSnmpPdu *pdu,
				     TnmBer *ber));
static TnmBer*
DecodeHeader		_ANSI_ARGS_((Message *msg, TnmSnmpPdu *pdu,
				     TnmBer *ber));
static TnmBer*
DecodeScopedPDU		_ANSI_ARGS_((TnmBer *ber, TnmSnmpPdu *pdu));

static TnmBer*
DecodeUsmSecParams	_ANSI_ARGS_((Message *msg, TnmSnmpPdu *pdu,
				     TnmBer *ber));

#ifdef TNM_SNMPv2U
static int
DecodeUsecParameter	_ANSI_ARGS_((Message *msg));

static void
SendUsecReport		_ANSI_ARGS_((Tcl_Interp *interp, 
				     TnmSnmp *session, 
				     struct sockaddr_in *to, 
				     int reqid, u_int *statPtr));
#endif

static TnmBer*
DecodePDU		_ANSI_ARGS_((TnmBer *ber, TnmSnmpPdu *pdu));


/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpDecode --
 *
 *	This procedure decodes a complete SNMP packet and does 
 *	all required actions (mostly executing callbacks or doing 
 *	gets/sets in the agent module).
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
TnmSnmpDecode(interp, packet, packetlen, from, session, reqid, status, index)
    Tcl_Interp *interp;
    u_char *packet;
    int	packetlen;
    struct sockaddr_in *from;
    TnmSnmp *session;
    int *reqid;
    int *status;
    int *index;
{
    TnmSnmpPdu _pdu, *pdu = &_pdu;
    Message _msg, *msg = &_msg;
    TnmSnmpRequest *request = NULL;
    int code, delivered = 0;
    TnmBer *ber;

    if (reqid) {
	*reqid = 0;
    }
    memset((char *) msg, 0, sizeof(Message));
    Tcl_DStringInit(&pdu->varbind);
    pdu->addr = *from;

    tnmSnmpStats.snmpInPkts++;
    ber = TnmBerCreate(packet, packetlen);
    code = DecodeMessage(interp, msg, pdu, ber);
    TnmBerDelete(ber);
    if (code == TCL_ERROR) {
	Tcl_DStringFree(&pdu->varbind);
	return TCL_ERROR;
    }

    /*
     * Show the contents of the PDU - mostly for debugging.
     */

    TnmSnmpDumpPDU(interp, pdu);

    if (msg->version == TNM_SNMPv3 && pdu->type == ASN1_SNMP_REPORT) {
	TnmSnmp *s = session;
	request = TnmSnmpFindRequest(pdu->requestId);
	if (request) {
	    s = request->session;
	}
	if (! s) {
	    Tcl_DStringFree(&pdu->varbind);
	    return TCL_CONTINUE;
	}

	/* 1.3.6.1.6.3.15.1.1.1		usmStatsUnsupportedSecLevels */
	/* 1.3.6.1.6.3.15.1.1.2		usmStatsNotInTimeWindows */
	/* 1.3.6.1.6.3.15.1.1.3		usmStatsUnknownUserNames */
	/* 1.3.6.1.6.3.15.1.1.4		usmStatsUnknownEngineIDs */
	/* 1.3.6.1.6.3.15.1.1.5		usmStatsWrongDigests */
	/* 1.3.6.1.6.3.15.1.1.6		usmStatsDecryptionErrors */

	TnmSnmpEvalBinding(interp, s, pdu, TNM_SNMP_RECV_EVENT);

	TnmSetOctetStringObj(s->engineID, msg->engineID, msg->engineIDLength);
	s->engineBoots = msg->engineBoots;
	s->engineTime = msg->engineTime;
	
	Tcl_DStringFree(&pdu->varbind);
	return TCL_BREAK;
    }

#ifdef TNM_SNMPv2U
    /*
     * Do some sanity checks here to detect unfair players
     * that have the report flag set for PDUs that are not
     * allowed to have the report flag turned on. Clear the
     * flag if necessary.
     */

    if (msg->version == TNM_SNMPv2U
	&& msg->qos & USEC_QOS_REPORT
	&& pdu->type != ASN1_SNMP_GET 
	&& pdu->type != ASN1_SNMP_GETNEXT
	&& pdu->type != ASN1_SNMP_GETBULK
	&& pdu->type != ASN1_SNMP_SET) {
	    msg->qos &= ~ USEC_QOS_REPORT;
    }

    /*
     * First check for REPORT PDUs. They are used internally to handle
     * SNMPv2 time synchronization (aka maintenance functions).
     */

    if (msg->version == TNM_SNMPv2U && pdu->type == ASN1_SNMP_REPORT) {
	TnmSnmp *s = session;
	time_t clock = time((time_t *) NULL);
	request = TnmSnmpFindRequest(pdu->requestId);
	if (request) {
	    s = request->session;
	}
	if (! s) {
	    Tcl_DStringFree(&pdu->varbind);
	    return TCL_CONTINUE;
	}

	TnmSnmpEvalBinding(interp, s, pdu, TNM_SNMP_RECV_EVENT);

	/*
	 * Accept new values for agentID, agentBoots and agentTime.
	 * Update the internal cache of agentIDs which makes also
	 * sure that localized authentication keys are re-computed.
	 */

	if (s->qos & USEC_QOS_AUTH) {

	    int update = 0;

	    /*
	     * XXX check the report type (must be usecStatsNotInWindows)
	     * and make sure that the digest is valid.
	     */

	    if (s->agentBoots < msg->agentBoots) {
		s->agentBoots = msg->agentBoots;
		update++;
	    }
	    if (s->agentTime != clock - msg->agentTime) {
		s->agentTime = clock - msg->agentTime;
		update++;
	    }

	    if (memcmp(s->agentID, msg->agentID, USEC_MAX_AGENTID) != 0) {
		memcpy(s->agentID, msg->agentID, USEC_MAX_AGENTID);
		update++;
	    }
	    
#ifdef DEBUG_USEC
	    fprintf(stderr, "check: agentBoots = %u agentTime = %u\n",
		    s->agentBoots, s->agentTime);
#endif

	    if (update) {
		TnmSnmpUsecSetAgentID(s);
	    }

	    /*
	     * Resend the original request (RFC 1910 section 3.2 step 12).
	     */
	    
	    if (request) {
		TnmSnmpUsecAuth(s, request->packet, request->packetlen);
		TnmSnmpDelay(s);
		TnmSnmpSend(interp, s, request->packet, request->packetlen,
			    &s->maddr, TNM_SNMP_ASYNC);
	    }
	}
	Tcl_DStringFree(&pdu->varbind);
	return TCL_BREAK;
    }
#endif

    /*
     * Next, handle RESPONSES as we should be able to find a session 
     * for the RESPONSE PDU. 
     */


    if (pdu->type == ASN1_SNMP_RESPONSE) {

	tnmSnmpStats.snmpInGetResponses++;
    
	/* 
	 * Lookup the request for this response and evaluate the callback
	 * or return the result if we can not find an async request and
	 * we already have the session pointer.
	 */
	
	request = TnmSnmpFindRequest(pdu->requestId);

	if (! request) {
	    if (! session) {
		Tcl_DStringFree(&pdu->varbind);
		return TCL_CONTINUE;
	    }
	    
	    if (reqid) {
		*reqid = pdu->requestId;
	    }

	    if (! Authentic(session, msg, pdu, packet, packetlen, NULL)) {
		Tcl_SetResult(interp, "authentication failure", TCL_STATIC);
		Tcl_DStringFree(&pdu->varbind);
		return TCL_CONTINUE;
	    }

	    TnmSnmpEvalBinding(interp, session, pdu, TNM_SNMP_RECV_EVENT);
	    
	    if (pdu->errorStatus) {
		char buf[20], *name;
		name = TnmGetTableValue(tnmSnmpErrorTable,
					(unsigned) pdu->errorStatus);
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, name ? name : "unknown", 
				 (char *) NULL);
		sprintf(buf, " %d ", pdu->errorIndex - 1);
		Tcl_AppendResult(interp, buf, 
				  Tcl_DStringValue(&pdu->varbind),
				  (char *) NULL);
		Tcl_DStringFree(&pdu->varbind);
		if (status) *status = pdu->errorStatus;
		if (index) *index = pdu->errorIndex;
		return TCL_ERROR;
	    }
	    Tcl_ResetResult(interp);
	    Tcl_DStringResult(interp, &pdu->varbind);
	    return TCL_OK;

	} else {

	    session = request->session;

	    if (! Authentic(session, msg, pdu, packet, packetlen, NULL)) {
		Tcl_SetResult(interp, "authentication failure", TCL_STATIC);
		Tcl_DStringFree(&pdu->varbind);
		return TCL_CONTINUE;
	    }

#ifdef TNM_SNMP_BENCH
	    request->stats.recvSize = tnmSnmpBenchMark.recvSize;
	    request->stats.recvTime = tnmSnmpBenchMark.recvTime;
	    session->stats = request->stats;
#endif

	    TnmSnmpEvalBinding(interp, session, pdu, TNM_SNMP_RECV_EVENT);

	    /* 
	     * Evaluate the callback procedure after we have deleted
	     * the request structure. This strange order makes sure
	     * that calls to Tcl_DoOneEvent() during the callback do
	     * not process this request again.
	     */

	    Tcl_Preserve((ClientData) request);
	    Tcl_Preserve((ClientData) session);
	    TnmSnmpDeleteRequest(request);
	    if (request->proc) {
		(request->proc) (session, pdu, request->clientData);
	    }
	    Tcl_Release((ClientData) session);
	    Tcl_Release((ClientData) request);

	    /*
	     * Free response message structure.
	     */
	    
	    Tcl_DStringFree(&pdu->varbind);
	    return TCL_OK;
	}
    }


    for (session = tnmSnmpList; session; session = session->nextPtr) {

	TnmSnmpBinding *bindPtr = session->bindPtr;

	if (session->version != msg->version) continue;

	switch (pdu->type) {
	  case ASN1_SNMP_TRAP1: 
	    while (bindPtr && bindPtr->event != TNM_SNMP_TRAP_EVENT) {
		bindPtr = bindPtr->nextPtr;
	    }
	    if (session->version == TNM_SNMPv1 && bindPtr && bindPtr->command
		&& (session->type == TNM_SNMP_LISTENER)
		&& Authentic(session, msg, pdu, packet, packetlen, NULL)) {
		delivered++;
		TnmSnmpEvalBinding(interp, session, pdu, TNM_SNMP_RECV_EVENT);
		TnmSnmpEvalCallback(interp, session, pdu, bindPtr->command,
				     NULL, NULL, NULL, NULL);
		tnmSnmpStats.snmpInTraps++;
	    }
	    break;
	  case ASN1_SNMP_TRAP2:
	    while (bindPtr && bindPtr->event != TNM_SNMP_TRAP_EVENT) {
		bindPtr = bindPtr->nextPtr;
	    }
	    if ((session->version & TNM_SNMPv2) && bindPtr && bindPtr->command
		&& (session->type == TNM_SNMP_LISTENER)
		&& Authentic(session, msg, pdu, packet, packetlen, NULL)) {
		delivered++;
		TnmSnmpEvalBinding(interp, session, pdu, TNM_SNMP_RECV_EVENT);
		TnmSnmpEvalCallback(interp, session, pdu, bindPtr->command,
				     NULL, NULL, NULL, NULL);
		tnmSnmpStats.snmpInTraps++;
	    }
	    break;
	  case ASN1_SNMP_INFORM:
	    while (bindPtr && bindPtr->event != TNM_SNMP_INFORM_EVENT) {
                bindPtr = bindPtr->nextPtr;
            }
	    if ((session->version & TNM_SNMPv2) && bindPtr && bindPtr->command
                && Authentic(session, msg, pdu, packet, packetlen, NULL)) {
                delivered++;
		TnmSnmpEvalBinding(interp, session, pdu, TNM_SNMP_RECV_EVENT);
                TnmSnmpEvalCallback(interp, session, pdu, bindPtr->command, 
				    NULL, NULL, NULL, NULL);
		pdu->type = ASN1_SNMP_RESPONSE;
		if (TnmSnmpEncode(interp, session, pdu, NULL, NULL)
		    != TCL_OK) {
		    Tcl_DStringFree(&pdu->varbind);
		    return TCL_ERROR;
		}
            }
	    break;
	  case ASN1_SNMP_GETBULK:
	    if (session->version == TNM_SNMPv1) break;
	  case ASN1_SNMP_GET:
	  case ASN1_SNMP_GETNEXT:
	  case ASN1_SNMP_SET: 
	    {
		u_int *statPtr;
		if (session->type != TNM_SNMP_RESPONDER) break;
		if (Authentic(session, msg, pdu, packet, packetlen, &statPtr)) {
		    TnmSnmpEvalBinding(interp, session, pdu, TNM_SNMP_RECV_EVENT);
		    if (TnmSnmpAgentRequest(interp, session, pdu) != TCL_OK) {
			Tcl_DStringFree(&pdu->varbind);
			return TCL_ERROR;
		    }
		    delivered++;
		} else {
#ifdef TNM_SNMPv2U
		    if (session->version == TNM_SNMPv2U 
			&& msg->qos & USEC_QOS_REPORT) {
			SendUsecReport(interp, session, from, 
				       pdu->requestId, statPtr);
		    }
#endif
		}
	    }
	    break;
	}
    }

    if (! delivered && msg->version == TNM_SNMPv1) {
	tnmSnmpStats.snmpInBadCommunityNames++;
    }

    Tcl_DStringFree(&pdu->varbind);
    return TCL_CONTINUE;
}

/*
 *----------------------------------------------------------------------
 *
 * Authentic --
 *
 *	This procedure verifies the authentication information 
 *	contained in the message header against the information 
 *	associated with the given session handle.
 *
 * Results:
 *	1 on success and 0 if the authentication process failed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
Authentic(session, msg, pdu, packet, packetlen, snmpStatPtr)
    TnmSnmp *session;
    Message *msg;
    TnmSnmpPdu *pdu;
    u_char *packet;
    int packetlen;
    u_int **snmpStatPtr;
{
    int authentic = 0;
    int communityLength;
    char *community;
#ifdef TNM_SNMPv2U
    char *context, *user;
    int contextLength, userLength;
    u_char recvDigest[16], md5Digest[16];
#endif
    
    if (msg->version != session->version) {
	return authentic;
    }

    switch (msg->version) {
	
    case TNM_SNMPv1:
    case TNM_SNMPv2C:

	/*
	 * Event reports are always considered authentic in SNMPv1 or
	 * SNMPv2c because the community string might indicate the
	 * context. Note, we should pass the community string to the
	 * receiving application. I do not yet know how to do this
	 * in a clean an elegant way.
	 */
	
	if (pdu->type == ASN1_SNMP_TRAP1
	    || pdu->type == ASN1_SNMP_TRAP2
	    || pdu->type == ASN1_SNMP_INFORM) {
	    authentic = 1;
	    break;
	}
	
	community = Tcl_GetStringFromObj(session->community,
					 &communityLength); 
	authentic = (communityLength == msg->comLen) &&
		     (memcmp(community, msg->com, (size_t) msg->comLen) == 0);
	break;

#ifdef TNM_SNMPv2U
    case TNM_SNMPv2U:
	
	authentic = 1;
	if (snmpStatPtr) {
	    *snmpStatPtr = NULL;
	}

	/*
	 * Check the user name (RFC 1910 section 3.2 step 7).
	 */

	user = Tcl_GetStringFromObj(session->user, &userLength);
	if (msg->userNameLen != userLength ||
	    memcmp(msg->userName, user, userLength) != 0) {
	    tnmSnmpStats.usecStatsUnknownUserNames++;
	    if (snmpStatPtr) {
		*snmpStatPtr = &tnmSnmpStats.usecStatsUnknownUserNames;
	    }
	    authentic = 0;
	    break;
	}

	/*
	 * Check the qos (RFC 1910 section 3.2 step 8).
	 */

	if (msg->qos & USEC_QOS_PRIV 
	    || (msg->qos & ~USEC_QOS_REPORT) != session->qos) {
	    tnmSnmpStats.usecStatsUnsupportedQoS++;
	    if (snmpStatPtr) {
		*snmpStatPtr = &tnmSnmpStats.usecStatsUnsupportedQoS;
	    }
	    authentic = 0;
	    break;
	}

	/*
	 * Check if the context matches (RFC 1910 section 3.2 step 6)
	 */

	context = Tcl_GetStringFromObj(session->context, &contextLength);
	if (msg->cntxtLen != contextLength
            || memcmp(msg->cntxt, context, contextLength) != 0) {
	    tnmSnmpStats.usecStatsUnknownContexts++;
	    if (snmpStatPtr) {
		*snmpStatPtr = &tnmSnmpStats.usecStatsUnknownContexts;
	    }
            authentic = 0;
            break;
        }
	
	if (msg->qos & USEC_QOS_AUTH) {

	    /*
	     * Check if the agentID matches (RFC 1910 section 3.2 step 6)
	     */
	    
	    if (memcmp(msg->agentID, session->agentID,USEC_MAX_AGENTID) != 0) {
		tnmSnmpStats.usecStatsUnknownContexts++;
		if (snmpStatPtr) {
		    *snmpStatPtr = &tnmSnmpStats.usecStatsUnknownContexts;
		}
		authentic = 0;
		break;
	    }
	    
	    if (session->type == TNM_SNMP_RESPONDER) {
		int clock = time((time_t *) NULL) - session->agentTime;
		if (msg->agentBoots != session->agentBoots
		    || (int) msg->agentTime < clock - 150
		    || (int) msg->agentTime > clock + 150
		    || session->agentBoots == 0xffffffff) {
		    tnmSnmpStats.usecStatsNotInWindows++;
		    if (snmpStatPtr) {
			*snmpStatPtr = &tnmSnmpStats.usecStatsNotInWindows;
		    }
		    authentic = 0;
		    break;
		}
	    }

	    if (msg->authDigestLen != TNM_MD5_SIZE) {
		tnmSnmpStats.usecStatsWrongDigestValues++;
		if (snmpStatPtr) {
		    *snmpStatPtr = &tnmSnmpStats.usecStatsWrongDigestValues;
		}
		authentic = 0;
		break;
	    }

	    memcpy(recvDigest, msg->authDigest, TNM_MD5_SIZE);
	    memcpy(msg->authDigest, session->authKey, TNM_MD5_SIZE);
	    TnmSnmpMD5Digest(packet, packetlen, session->authKey, md5Digest);
	    if (memcmp(recvDigest, md5Digest, TNM_MD5_SIZE) != 0) {
#ifdef DEBUG_USEC
	        int i;

	        fprintf(stdout, "received digest:");
		for (i = 0; i < 16; i++) 
		    fprintf(stdout, " %02x", recvDigest[i]);
		fprintf(stdout,"\n");
#endif
		tnmSnmpStats.usecStatsWrongDigestValues++;
		if (snmpStatPtr) {
		    *snmpStatPtr = &tnmSnmpStats.usecStatsWrongDigestValues;
		}
		authentic = 0;
	    }
	    memcpy(msg->authDigest, recvDigest, TNM_MD5_SIZE);
	    break;
	}
	break;
#endif
    case TNM_SNMPv3:
	if (! (*msg->msgFlags & TNM_SNMP_FLAG_AUTH) 
	    && ! (*msg->msgFlags & TNM_SNMP_FLAG_PRIV)) {
	    char *user;
	    int userLength;
	    user = Tcl_GetStringFromObj(session->user, &userLength);

	    authentic = (userLength == msg->userLength)
		&& (memcmp(user, msg->user, (size_t) userLength) == 0);
	    break;
	}
	break;
    }

    return authentic;
}

/*
 *----------------------------------------------------------------------
 *
 * DecodeMessage --
 *
 *	This procedure takes a serialized packet and decodes the 
 *	SNMPv1 (RFC 1157) and SNMPv2 (RFC 1901) message header.
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
DecodeMessage(interp, msg, pdu, ber)
    Tcl_Interp *interp;
    Message *msg;
    TnmSnmpPdu *pdu;
    TnmBer *ber;
{
    int version, msgSeqLength;
    u_char *msgSeqToken, *msgSeqStart;
    
    /*
     * Decode the SNMP message header and check whether the
     * message size matches the size of the packet received
     * from the transport.
     */

    msgSeqStart = ber->current;
    if (! TnmBerDecSequenceStart(ber, ASN1_SEQUENCE,
				 &msgSeqToken, &msgSeqLength)) {
        goto asn1Error;
    }

    /*
     * Decode the version field of the message (must be 0 for SNMPv1,
     * 1 for SNMPv2C, 2 for SNMPv2U, and 3 for SNMPv3).
     */
    
    if (! TnmBerDecInt(ber, ASN1_INTEGER, &version)) {
	goto asn1Error;
    }

    switch (version) {
    case 0:
	msg->version = TNM_SNMPv1;
	break;
    case 1:
	msg->version = TNM_SNMPv2C;
	break;
#ifdef TNM_SNMPv2U
    case 2:
	msg->version = TNM_SNMPv2U;
	break;
#endif
    case 3:
	msg->version = TNM_SNMPv3;
	break;
    default:
	TnmBerSetError(ber, "unknown version in SNMP message");
	tnmSnmpStats.snmpInBadVersions++;
	goto asn1Error;
    }
    
    if (version < 3) {
	if (! TnmBerDecOctetString(ber, ASN1_OCTET_STRING,
				   (char **) &msg->com, &msg->comLen)) {
	    goto asn1Error;
	}

#ifdef TNM_SNMPv2U
	if (version == 2) {
	    if (DecodeUsecParameter(msg) != TCL_OK) {
		TnmBerSetError("encoding error in USEC parameters");
		goto asn1Error;
	    }
	}
#endif

	if (! DecodePDU(ber, pdu)) {
	    goto asn1Error;
	}

#ifdef TNM_SNMPv3
	/*
	 * We probably have to make some mappings here according to
	 * the coexistance specification currently being defined. (XXXX)
	 */
	pdu->context = msg->com;
	pdu->contextLength = msg->comLen;
#endif
    }

    if (version == 3) {
	u_char *usmParam;
	TnmBer *usmBer;
	int usmParamLength;

	if (! DecodeHeader(msg, pdu, ber)) {
	    goto asn1Error;
	}
	if (! TnmBerDecOctetString(ber, ASN1_OCTET_STRING, 
				   (char **) &usmParam, &usmParamLength)) {
	    goto asn1Error;
	}
	usmBer = TnmBerCreate(usmParam, usmParamLength);
	if (! DecodeUsmSecParams(msg, pdu, usmBer)) {
	    TnmBerDelete(usmBer);
	    goto asn1Error;
	}
	TnmBerDelete(usmBer);
	if (! DecodeScopedPDU(ber, pdu)) {
	    goto asn1Error;
	}
    }

    if (! TnmBerDecSequenceEnd(ber, msgSeqToken, msgSeqLength)) {
	goto asn1Error;
    }

    if (! TnmBerDecDone(ber)) {
	goto lengthError;
    }
    
    return TCL_OK;

  asn1Error:
    Tcl_SetResult(interp, ber ? TnmBerGetError(ber) : "parse error",
		  TCL_VOLATILE);
    tnmSnmpStats.snmpInASNParseErrs++;
    return TCL_ERROR;

 lengthError:
    Tcl_SetResult(interp, "message length does not match packet size", 
		  TCL_STATIC);
    tnmSnmpStats.snmpInASNParseErrs++;
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DecodeHeader --
 *
 *	This procedure decodes the SNMPv3 message header which
 *	contains the administrative data.
 *
 *	    HeaderData ::= SEQUENCE {
 *		msgID            INTEGER (0..2147483647),
 *		msgMaxSize       INTEGER (484..2147483647),
 *		msgFlags         OCTET STRING (SIZE(1)),
 *		msgSecurityModel INTEGER (0..2147483647)
 *	    }
 *
 * Results:
 *	A pointer to the packet or NULL if there was an error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmBer*
DecodeHeader(msg, pdu, ber)
    Message *msg;
    TnmSnmpPdu *pdu;
    TnmBer *ber;
{
    u_char *seqToken;
    int seqLength, flagsLen, secmodel;

    if (! TnmBerDecSequenceStart(ber, ASN1_SEQUENCE, &seqToken, &seqLength)) {
	return NULL;
    }
    if (! TnmBerDecInt(ber, ASN1_INTEGER, &msg->msgID)) {
	return NULL;
    }
    if (! TnmBerDecInt(ber, ASN1_INTEGER, &msg->maxSize)) {
	return NULL;
    }
    if (msg->maxSize < 484) {
	return NULL;
    }
    if (! TnmBerDecOctetString(ber, ASN1_OCTET_STRING,
			       &msg->msgFlags, &flagsLen)) {
	return NULL;
    }
    if (flagsLen != 1) {
	return NULL;
    }
    if (! TnmBerDecInt(ber, ASN1_INTEGER, &secmodel)) {
	return NULL;
    }

    switch (secmodel) {
    case TNM_SNMP_USM_SEC_MODEL:
	if (! TnmBerDecSequenceEnd(ber, seqToken, seqLength)) {
	    return NULL;
	}
	break;
    default:
	return NULL;
    }

    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * DecodeUsmSecParams --
 *
 *	This procedure decodes the SNMPv3 USM security parameters.
 *	The format of the SEQUENCE is defined in RFC 2274, section 2.4:
 *
 *	    UsmSecurityParameters ::= SEQUENCE {
 *	        msgAuthoritativeEngineID     OCTET STRING,
 *	        msgAuthoritativeEngineBoots  INTEGER (0..2147483647),
 *	        msgAuthoritativeEngineTime   INTEGER (0..2147483647),
 *	        msgUserName                  OCTET STRING (SIZE(1..32)),
 *	        msgAuthenticationParameters  OCTET STRING,
 *	        msgPrivacyParameters         OCTET STRING
 *	  }
 *
 * Results:
 *	A pointer to the packet or NULL if there was an error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmBer*
DecodeUsmSecParams(msg, pdu, ber)
    Message *msg;
    TnmSnmpPdu *pdu;
    TnmBer *ber;
{
    u_char *seqToken;
    int seqLength;

    if (! TnmBerDecSequenceStart(ber, ASN1_SEQUENCE, &seqToken, &seqLength)) {
	return NULL;
    }
    
    if (! TnmBerDecOctetString(ber, ASN1_OCTET_STRING,
			       &msg->engineID, &msg->engineIDLength)) {
	return NULL;
    }
    if (! TnmBerDecInt(ber, ASN1_INTEGER, &msg->engineBoots)) {
	return NULL;
    }
    if (! TnmBerDecInt(ber, ASN1_INTEGER, &msg->engineTime)) {
	return NULL;
    }
    if (! TnmBerDecOctetString(ber, ASN1_OCTET_STRING,
			       &msg->user, &msg->userLength)) {
	return NULL;
    }
    if (! TnmBerDecOctetString(ber, ASN1_OCTET_STRING, NULL, NULL)) {
	return NULL;
    }
    if (! TnmBerDecOctetString(ber, ASN1_OCTET_STRING, NULL, NULL)) {
	return NULL;
    }

    return TnmBerDecSequenceEnd(ber, seqToken, seqLength);
}

/*
 *----------------------------------------------------------------------
 *
 * DecodeScopedPDU --
 *
 *	This procedure decodes the SNMPv3 scoped PDU as defined
 *	in RFC 2272, section 6.
 *
 *	    ScopedPDU ::= SEQUENCE {
 *	        contextEngineID  OCTET STRING,
 *		contextName      OCTET STRING,
 *		data             ANY -- e.g., PDUs as defined in RFC1905
 *	    }
 *
 * Results:
 *	A pointer to the packet or NULL if there was an error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmBer*
DecodeScopedPDU(ber, pdu)
    TnmBer *ber;
    TnmSnmpPdu *pdu;
{
    u_char *seqToken;
    int seqLength;

    if (! TnmBerDecSequenceStart(ber, ASN1_SEQUENCE, &seqToken, &seqLength)) {
	return NULL;
    }

    if (! TnmBerDecOctetString(ber, ASN1_OCTET_STRING,
			       &pdu->engineID, &pdu->engineIDLength)) {
	return NULL;
    }
    if (! TnmBerDecOctetString(ber, ASN1_OCTET_STRING,
			       &pdu->context, &pdu->contextLength)) {
	return NULL;
    }
    if (! DecodePDU(ber, pdu)) {
	return NULL;
    }
    
    return TnmBerDecSequenceEnd(ber, seqToken, seqLength);
}

#ifdef TNM_SNMPv2U

/*
 *----------------------------------------------------------------------
 *
 * DecodeUsecParameter --
 *
 *	This procedure decodes the USEC parameter field and updates
 *	the message structure pointed to by msg. See RFC 1910 for
 *	details about the parameter field.
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
DecodeUsecParameter(msg)
    Message *msg;
{
    u_char *p = msg->com;

    /*
     * Check the model number and get the qos value.
     */

    if (*p++ != USEC_MODEL) {
	tnmSnmpStats.usecStatsBadParameters++;
	return TCL_ERROR;
    }
    msg->qos = *p++;

    /*
     * Copy the agentID, the agentBoots and the agentTime into 
     * the message structure.
     */

    memcpy(msg->agentID, p, USEC_MAX_AGENTID);
    p += USEC_MAX_AGENTID;
	
    msg->agentBoots = *p++;
    msg->agentBoots = (msg->agentBoots << 8) + *p++;
    msg->agentBoots = (msg->agentBoots << 8) + *p++;
    msg->agentBoots = (msg->agentBoots << 8) + *p++;

    msg->agentTime = *p++;
    msg->agentTime = (msg->agentTime << 8) + *p++;
    msg->agentTime = (msg->agentTime << 8) + *p++;
    msg->agentTime = (msg->agentTime << 8) + *p++;

    msg->maxSize = *p++;
    msg->maxSize = (msg->maxSize << 8) + *p++;
    if (msg->maxSize < USEC_MIN_MMS || msg->maxSize > USEC_MAX_MMS) {
	tnmSnmpStats.usecStatsBadParameters++;
        return TCL_ERROR;
    }

#ifdef DEBUG_USEC
    fprintf(stderr, 
	    "decode: agentBoots = %u agentTime = %u maxSize = %u\n", 
	    msg->agentBoots, msg->agentTime, msg->maxSize);
#endif

    /*
     * Get the user name, the authentication digest, the max message 
     * size and finally the context identifier.
     */
	
    msg->userNameLen = *p++;
    if (msg->userNameLen > USEC_MAX_USER) {
	tnmSnmpStats.usecStatsBadParameters++;
	return TCL_ERROR;
    }
    memcpy(msg->userName, p, msg->userNameLen);
    p += msg->userNameLen;
    
    msg->authDigestLen = *p++;
    msg->authDigest = (u_char *) p;
    p += msg->authDigestLen;
    
    msg->cntxtLen = msg->comLen - (p - msg->com);
    if (msg->cntxtLen < 0 || msg->cntxtLen > USEC_MAX_CONTEXT) {
	tnmSnmpStats.usecStatsBadParameters++;
	return TCL_ERROR;
    }
    memcpy(msg->cntxt, p, msg->comLen - (p - msg->com));

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SendUsecReport --
 *
 *	This procedure sends a report PDU to let the receiver
 *	synchronize itself. Make sure that we never send a report PDU
 *	if we are not acting in an agent role. (We could get into nasty
 *	loops otherwise.) We create a copy of the session and adjust
 *	it to become the well-known usec user.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A report PDU is generated and send to the SNMP peer.
 *
 *----------------------------------------------------------------------
 */

static void
SendUsecReport(interp, session, to, reqid, statPtr)
    Tcl_Interp *interp;
    TnmSnmp *session;
    struct sockaddr_in *to;
    int reqid;
    u_int *statPtr;
{
    TnmSnmpPdu _pdu, *pdu = &_pdu;
    char varbind[80];
    int qos;

    /*
     * Make sure we are in an agent role and we actually have
     * something to report.
     */

    if (session->type != TNM_SNMP_RESPONDER || ! statPtr) {
	return;
    }

    /*
     * Create a report PDU and send it to the SNMP peer.
     */

    pdu->addr = *to;
    pdu->type = ASN1_SNMP_REPORT;
    pdu->requestId = reqid;
    pdu->errorStatus = TNM_SNMP_NOERROR;
    pdu->errorIndex = 0;    
    pdu->trapOID = NULL;
    Tcl_DStringInit(&pdu->varbind);
    
    if (statPtr > &tnmSnmpStats.usecStatsUnsupportedQoS) {
	sprintf(varbind, "{1.3.6.1.6.3.6.1.2.%d %u}", 
	    statPtr - &tnmSnmpStats.usecStatsUnsupportedQoS + 1,  *statPtr);
    } else if (statPtr > &tnmSnmpStats.snmpStatsPackets) {
	sprintf(varbind, "{1.3.6.1.6.3.1.1.1.%d %u}", 
	    statPtr -  &tnmSnmpStats.snmpStatsPackets + 1, *statPtr);
    } else {
	*varbind = '\0';
    }

    /*
     * Report PDUs are only authenticated if we are sending an 
     * usecStatsNotInWindows report (see RFC 1910).
     */

    qos = session->qos;
    if (statPtr != &tnmSnmpStats.usecStatsNotInWindows) {
	session->qos = USEC_QOS_NULL;
    }

    Tcl_DStringAppend(&pdu->varbind, varbind, -1);
    TnmSnmpEncode(interp, session, pdu, NULL, NULL);
    Tcl_DStringFree(&pdu->varbind);

    session->qos = qos;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * DecodePDU --
 *
 *	This procedure takes a serialized packet and decodes the PDU. 
 *	The result is written to the pdu structure and varbind list 
 *	is converted to a TCL list contained in pdu->varbind.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmBer*
DecodePDU(ber, pdu)
    TnmBer *ber;
    TnmSnmpPdu *pdu;
{
    int oidlen = 0;
    
    Tnm_Oid oid[TNM_OID_MAX_SIZE];
    int int_val;
    char buf[20];
    char *exception, *freeme;
    static char *vboid;
    static int vboidLen = 0;
    char *snmpTrapEnterprise = NULL;
    u_char byte;

    u_char tag;

    u_char *pduSeqToken, *vbSeqToken, *vblSeqToken;
    int pduSeqLength, vbSeqLength, vblSeqLength;

    if (ber == NULL) {
	return NULL;
    }

    Tcl_DStringInit(&pdu->varbind);

    /*
     * Decode the PDU sequence and check whether the PDU type is
     * acceptable for us.
     */

    TnmBerDecPeek(ber, (u_char *) &byte);
    if (! TnmBerDecSequenceStart(ber, byte, &pduSeqToken, &pduSeqLength)) {
	tnmSnmpStats.snmpInASNParseErrs++;
	goto asn1Error;
    }
    pdu->type = byte;

    if (TnmGetTableValue(tnmSnmpPDUTable, (unsigned) pdu->type) == NULL) {
	TnmBerSetError(ber, "unknown PDU tag in SNMP message");
	goto asn1Error;
    }

    /*
     * Decode good old SNMPv1 trap PDUs (RFC 1157). They have a
     * different format and require therefore some extra code.
     */

    if (pdu->type == ASN1_SNMP_TRAP1) {

	int generic, specific;
	char *toid = NULL;

	pdu->requestId = 0;
	pdu->errorStatus = 0;
	pdu->errorIndex = 0;

	if (! TnmBerDecOID(ber, oid, &oidlen)) goto asn1Error;

	/*
	 * Save the enterprise object identifier so we can add it
	 * at the end of the varbind list. See the definition of
	 * snmpTrapEnterprise for details.
	 */

	{
	    char *tmp;
	    snmpTrapEnterprise = TnmOidToStr(oid, oidlen);
	    tmp = TnmMibGetName(snmpTrapEnterprise, 0);
	    if (tmp) {
		snmpTrapEnterprise = ckstrdup(tmp);
	    } else {
		snmpTrapEnterprise = ckstrdup(snmpTrapEnterprise);
	    }
	}

	if (! TnmBerDecOctetString(ber, ASN1_IPADDRESS, 
				   (char **) &freeme, &int_val)) {
	    goto asn1Error;
	}
	if (! TnmBerDecInt(ber, ASN1_INTEGER, &generic)) {
	    goto asn1Error;
	}
	if (! TnmBerDecInt(ber, ASN1_INTEGER, &specific)) {
	    goto asn1Error;
	}

	/*
	 * Check whether the IP address has a valid length. Use the IP
	 * address in the SNMPv1_TRAP packet as a from address since 
	 * this allows people to fake traps. This feature was requested 
	 * by jsong@nlc.com (James Song).
	 */

	if (int_val != 4) goto asn1Error;

	/* KARL - NO - this screwed up US Cable by basically quoting
	 * an IP address through their NAT translation
	 */
	// memcpy(&pdu->addr.sin_addr, freeme, 4);

	/* 
	 * Ignore errors here to accept bogus trap messages.
	 */

	if (! TnmBerDecInt(ber, ASN1_TIMETICKS, &int_val)) {
	    goto asn1Error;
	}
	{   sprintf(buf, "%u", int_val);
	    Tcl_DStringStartSublist(&pdu->varbind);
	    Tcl_DStringAppendElement(&pdu->varbind, "1.3.6.1.2.1.1.3.0");
	    Tcl_DStringAppendElement(&pdu->varbind, "TimeTicks");
	    Tcl_DStringAppendElement(&pdu->varbind, buf);
	    Tcl_DStringEndSublist(&pdu->varbind);
	}

	switch (generic) {
	  case 0:				/* coldStart*/
	    toid = "1.3.6.1.6.3.1.1.5.1";
	    break;
	  case 1:				/* warmStart */
	    toid = "1.3.6.1.6.3.1.1.5.2";
	    break;
	  case 2:				/* linkDown */
	    toid = "1.3.6.1.6.3.1.1.5.3";
	    break;
	  case 3:				/* linkUp */
	    toid = "1.3.6.1.6.3.1.1.5.4";
	    break;
	  case 4:				/* authenticationFailure */
	    toid = "1.3.6.1.6.3.1.1.5.5";
	    break;
	  case 5:				/* egpNeighborLoss */
	    toid = "1.3.6.1.6.3.1.1.5.6";
	    break;
	  default:
	    oid[oidlen++] = 0;
	    oid[oidlen++] = specific;		/* enterpriseSpecific */
	    toid = ckstrdup(TnmOidToStr(oid, oidlen));
	    break;
	}

	Tcl_DStringStartSublist(&pdu->varbind);
	Tcl_DStringAppendElement(&pdu->varbind, "1.3.6.1.6.3.1.1.4.1.0");
	Tcl_DStringAppendElement(&pdu->varbind, "OBJECT IDENTIFIER");
	{
#if 1
	    Tcl_Obj *tmp = TnmMibFormat("1.3.6.1.6.3.1.1.4.1.0", 0, toid);
	    if (tmp) {
		Tcl_DStringAppendElement(&pdu->varbind, Tcl_GetString(tmp));
		Tcl_DecrRefCount(tmp);
	    } else {
		Tcl_DStringAppendElement(&pdu->varbind, toid);
	    }
#else
	    char *tmp = TnmMibGetName(toid, 0);
	    if (tmp) {
		Tcl_DStringAppendElement(&pdu->varbind, tmp);
	    } else {
		Tcl_DStringAppendElement(&pdu->varbind, toid);
	    }
#endif
	}

	if (((generic < 0) || (generic > 5)) && toid) {
	    ckfree(toid);
	    toid = NULL;
	}
	Tcl_DStringEndSublist(&pdu->varbind);

	if (ber == NULL) {
	    goto trapError;
	}

    } else {

	/*
	 * Decode the request-id, the error-status, and the error-index
	 * fields. Update our SNMP statistics if we got an error-status
	 * which is counted in the SNMP MIB.
	 */
	
	if (! TnmBerDecInt(ber, ASN1_INTEGER, &pdu->requestId)) {
	    goto asn1Error;
	}
	if (! TnmBerDecInt(ber, ASN1_INTEGER, &pdu->errorStatus)) {
	    goto asn1Error;
	}
	if (! TnmBerDecInt(ber, ASN1_INTEGER, &pdu->errorIndex)) {
	    goto asn1Error;
	}

	/*
	 * Check the error status and throw away packets that carry
	 * unknown error status values.
	 */

	if (pdu->errorStatus < TNM_SNMP_NOERROR
	    || pdu->errorStatus > TNM_SNMP_INCONSISTENTNAME) {
	    TnmBerSetError(ber, "unknown error status in SNMP PDU");
	    goto asn1Error;
	}
	
	switch (pdu->errorStatus) {
	  case TNM_SNMP_TOOBIG:
	      tnmSnmpStats.snmpInTooBigs++;
	      break;
	  case TNM_SNMP_NOSUCHNAME:
	      tnmSnmpStats.snmpInNoSuchNames++;
	      break;
	  case TNM_SNMP_BADVALUE:
	      tnmSnmpStats.snmpInBadValues++;
	      break;
	  case TNM_SNMP_READONLY:
	      tnmSnmpStats.snmpInReadOnlys++;
	      break;
	  case TNM_SNMP_GENERR:
	      tnmSnmpStats.snmpInGenErrs++;
	      break;
	}
    }
    
    /*
     * Decode the VarBindList sequence (RFC 1157, RFC 1905). There is
     * a special case for SNMPv1 traps. I am not sure how it got here
     * and why it is needed...
     */

    if (! TnmBerDecSequenceStart(ber, ASN1_SEQUENCE,
				 &vblSeqToken, &vblSeqLength)) {
	if (pdu->type == ASN1_SNMP_TRAP1) {
	    goto trapError;
	}
	goto asn1Error;
    }

    /*
     * vbLen contains the total length of the encoded varbind list. We
     * loop over the varbind list by decrementing vbLen until vbLen
     * gets zero. (We have an encoding error is vbLen ever falls below
     * zero.
     */

    while (!TnmBerDecDone(ber)) {

	/* 
	 * Decode a VarBind sequence (RFC 1157, RFC 1905) and
	 * start a new element in our varbind-list.
	 */

	if (! TnmBerDecSequenceStart(ber, ASN1_SEQUENCE,
				     &vbSeqToken, &vbSeqLength)) {
	    goto asn1Error;
	}
	
	Tcl_DStringStartSublist(&pdu->varbind);
	
	/*
	 * Decode the OBJECT-IDENTIFIER of the varbind.
	 */
	
	if (! TnmBerDecOID(ber, oid, &oidlen)) {
	    goto asn1Error;
	}

	{
	    char *soid = TnmOidToStr(oid, oidlen);
	    int len = strlen(soid);
	    if (vboidLen < len + 1)  {
		if (vboid) ckfree(vboid);
		vboidLen = len + 1;
		vboid = ckstrdup(soid);
	    } else {
		strcpy(vboid, soid);
	    }
	    Tcl_DStringAppendElement(&pdu->varbind, vboid);
	}

	/*
	 * Handle exceptions that are coded in the SNMP varbind. We
	 * try to create a type conforming null value if possible.
	 */

	if (! TnmBerDecPeek(ber, &tag)) {
	    goto asn1Error;
	}

	exception = TnmGetTableValue(tnmSnmpExceptionTable, tag);
	if (exception) {
	    Tcl_DStringAppendElement(&pdu->varbind, exception);
	    Tcl_DStringAppendElement(&pdu->varbind, 
				     TnmMibGetBaseSyntax(vboid)
				     == ASN1_OCTET_STRING ? "" : "0");
	    TnmBerDecNull(ber, tag);
	    goto nextVarBind;
	}

	/*
	 * Decode the ASN.1 type.
	 */

	{
	    char *syntax = TnmGetTableValue(tnmSnmpTypeTable, tag);
	    Tcl_DStringAppendElement(&pdu->varbind, syntax
				     ? syntax : "Opaque");
	}

	/*
	 * Decode the value of the object.
	 */

	switch (tag) {
	case ASN1_COUNTER32:
	case ASN1_GAUGE32:
	case ASN1_TIMETICKS:
	    if (! TnmBerDecInt(ber, tag, &int_val)) {
		goto asn1Error;
	    }
	    sprintf(buf, "%u", int_val);
            Tcl_DStringAppendElement(&pdu->varbind, buf);
            break;
	case ASN1_INTEGER:
	    if (! TnmBerDecInt(ber, tag, &int_val)) {
		goto asn1Error;
	    }
	    {   Tcl_Obj *tmp;
		sprintf(buf, "%d", int_val);
		tmp = TnmMibFormat(vboid, 0, buf);
		if (tmp) {
		    Tcl_DStringAppendElement(&pdu->varbind,
					     Tcl_GetString(tmp));
		    Tcl_DecrRefCount(tmp);
		} else {
		    Tcl_DStringAppendElement(&pdu->varbind, buf);
		}
	    }
            break;
	case ASN1_COUNTER64:
	    {
		TnmUnsigned64 u;
		Tcl_Obj *objPtr;
		if (! TnmBerDecUnsigned64(ber, &u)) {
		    goto asn1Error;
		}
		objPtr = TnmNewUnsigned64Obj(u);
		Tcl_DStringAppendElement(&pdu->varbind, 
					 Tcl_GetStringFromObj(objPtr, NULL));
		Tcl_DecrRefCount(objPtr);
	    }
	    break;
	case ASN1_NULL:
	    if (! TnmBerDecNull(ber, ASN1_NULL)) {
		goto asn1Error;
	    }
	    Tcl_DStringAppendElement(&pdu->varbind, "");
            break;
	case ASN1_OBJECT_IDENTIFIER:
	    if (! TnmBerDecOID(ber, oid, &oidlen)) {
		goto asn1Error;
	    }
#if 1
	    {   char *soid = TnmOidToStr(oid, oidlen);
		Tcl_Obj *tmp = TnmMibFormat(vboid, 0, soid);
		if (tmp) {
		    Tcl_DStringAppendElement(&pdu->varbind,
					     Tcl_GetString(tmp));
		    Tcl_DecrRefCount(tmp);
		} else {
		    Tcl_DStringAppendElement(&pdu->varbind, soid);
		}
	    }
#else
	    Tcl_DStringAppendElement(&pdu->varbind, TnmOidToStr(oid, oidlen));
#endif
            break;
	case ASN1_IPADDRESS:
	    if (! TnmBerDecOctetString(ber, ASN1_IPADDRESS, 
				       (char **) &freeme, &int_val)) {
		 goto asn1Error;
	    }
	    if (int_val != 4) goto asn1Error;
            Tcl_DStringAppend(&pdu->varbind, " ", 1);
	    {
		struct sockaddr_in addr;
		memcpy(&addr.sin_addr, freeme, 4);
		Tcl_DStringAppendElement(&pdu->varbind, 
					 inet_ntoa(addr.sin_addr));
	    }
            break;
	case ASN1_OPAQUE:
	case ASN1_OCTET_STRING:
            if (! TnmBerDecOctetString(ber, tag, 
				       (char **) &freeme, &int_val)) {
		goto asn1Error;
	    }
	    {
		static char *hex = NULL;
		static size_t hexLen = 0;
		if (hexLen < int_val * 5 + 1) {
		    if (hex) ckfree(hex);
		    hexLen = int_val * 5 + 1;
		    hex = ckalloc(hexLen);
		}
		TnmHexEnc(freeme, int_val, hex);
		if (tag == ASN1_OCTET_STRING) {
		    Tcl_Obj *tmp;
		    tmp = TnmMibFormat(vboid, 0, hex);
		    if (tmp) {
			Tcl_DStringAppendElement(&pdu->varbind,
						 Tcl_GetString(tmp));
			Tcl_DecrRefCount(tmp);
		    } else {
			Tcl_DStringAppendElement(&pdu->varbind, hex);
		    }
		} else {
		    Tcl_DStringAppendElement(&pdu->varbind, hex);
		}
	    }
            break;
	default:
	    if (! TnmBerDecAny(ber, (char **) &freeme, &int_val)) {
		    goto asn1Error;
	    }
	    {
		static char *hex = NULL;
		static size_t hexLen = 0;
		if (hexLen < int_val * 5 + 1) {
		    if (hex) ckfree(hex);
		    hexLen = int_val * 5 + 1;
		    hex = ckalloc(hexLen);
		}
		TnmHexEnc(freeme, int_val, hex);
		Tcl_DStringAppendElement(&pdu->varbind, hex);
	    }
	    break;
	}
	
      nextVarBind:

	Tcl_DStringEndSublist(&pdu->varbind);
	if (! TnmBerDecSequenceEnd(ber, vbSeqToken, vbSeqLength)) {
	    goto asn1Error;
	}
    }

    /*
     * Add the enterprise object identifier to the varbind list.
     * See the definition of snmpTrapEnterprise of details.
     */

    if (pdu->type == ASN1_SNMP_TRAP1 && snmpTrapEnterprise) {
	Tcl_DStringStartSublist(&pdu->varbind);
	Tcl_DStringAppendElement(&pdu->varbind, "1.3.6.1.6.3.1.1.4.3.0");
        Tcl_DStringAppendElement(&pdu->varbind, "OBJECT IDENTIFIER");
	Tcl_DStringAppendElement(&pdu->varbind, snmpTrapEnterprise);
	Tcl_DStringEndSublist(&pdu->varbind);
	ckfree(snmpTrapEnterprise);
    }

    if (! TnmBerDecSequenceEnd(ber, vblSeqToken, vblSeqLength)) {
	goto asn1Error;
    }
    if (! TnmBerDecSequenceEnd(ber, pduSeqToken, pduSeqLength)) {
	goto asn1Error;
    }

    return ber;
    
  asn1Error:
    tnmSnmpStats.snmpInASNParseErrs++;
    return NULL;

  trapError:
    return ber;
}

/*
 * Local Variables:
 * compile-command: "make -k -C ../../unix"
 * End:
 */
