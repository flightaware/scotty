/*
 * tnmSnmpSend.c --
 *
 *	This file contains all functions to encode a SNMP packet
 *	and to send it over the network.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2002 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmSnmpSend.c,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

#include "tnmSnmp.h"
#include "tnmMib.h"

/* 
 * Flag that controls hexdump. See the watch command for its use.
 */

extern int	hexdump;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int
EncodeMessage		_ANSI_ARGS_((Tcl_Interp *interp,
				     TnmSnmp *sess, TnmSnmpPdu *pdu,
				     TnmBer *ber));
static TnmBer*
EncodeHeader		_ANSI_ARGS_((Tcl_Interp *interp, TnmSnmp *session,
				     TnmSnmpPdu *pdu, TnmBer *ber));
static TnmBer*
EncodeScopedPDU		_ANSI_ARGS_((Tcl_Interp *interp, TnmSnmp *session,
				     TnmSnmpPdu *pdu, TnmBer *ber));
static u_char*
EncodeUsmSecParams	_ANSI_ARGS_((TnmSnmp *session, TnmSnmpPdu *pdu,
				     int *lengthPtr));
#ifdef TNM_SNMPv2U
static int
EncodeUsecParameter	_ANSI_ARGS_((TnmSnmp *session, TnmSnmpPdu *pdu, 
				     u_char *parameter));
#endif

static TnmBer*
EncodePDU		_ANSI_ARGS_((Tcl_Interp *interp, 
				     TnmSnmp *sess, TnmSnmpPdu *pdu,
				     TnmBer *ber));

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpEncode --
 *
 *	This procedure converts the pdu into BER transfer syntax and 
 *	sends it from this entity (either a manager or an agent) on
 *	this system to a remote entity.
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
TnmSnmpEncode(interp, session, pdu, proc, clientData)
    Tcl_Interp *interp;
    TnmSnmp *session;
    TnmSnmpPdu *pdu;
    TnmSnmpRequestProc *proc;
    ClientData clientData;
{
    int	retry = 0, packetlen = 0, code = 0;
    u_char packet[TNM_SNMP_MAXSIZE];
    TnmBer *ber;

    memset((char *) packet, 0, sizeof(packet));

    /*
     * Some special care must be taken to conform to SNMPv1 sessions:
     * SNMPv2 getbulk requests must be turned into getnext requests
     * and SNMPv2 error codes must be mapped on SNMPv1 error codes
     * (e.g. genErr as nothing more appropriate is available).
     *
     * This is based on the mapping presented in Marhall Rose and
     * Keith McCloghrie: "How to Manage your Network using SNMP"
     * page 95.
     */

    if (session->version == TNM_SNMPv1) {
        if (pdu->type == ASN1_SNMP_GETBULK) {
	    pdu->type = ASN1_SNMP_GETNEXT;
	    pdu->errorStatus = TNM_SNMP_NOERROR;
	    pdu->errorIndex  = 0;
	}
	if (pdu->type == ASN1_SNMP_INFORM || pdu->type == ASN1_SNMP_TRAP2) {
	    pdu->type = ASN1_SNMP_TRAP1;
	}
	if (pdu->errorStatus > TNM_SNMP_GENERR) {
	    switch (pdu->errorStatus) {
	      case TNM_SNMP_NOACCESS:
	      case TNM_SNMP_NOCREATION:
	      case TNM_SNMP_AUTHORIZATIONERROR:
	      case TNM_SNMP_NOTWRITABLE:
	      case TNM_SNMP_INCONSISTENTNAME:
		pdu->errorStatus = TNM_SNMP_NOSUCHNAME; break;
	      case TNM_SNMP_WRONGTYPE:
	      case TNM_SNMP_WRONGLENGTH:
	      case TNM_SNMP_WRONGENCODING:
	      case TNM_SNMP_WRONGVALUE:
	      case TNM_SNMP_INCONSISTENTVALUE:
		pdu->errorStatus = TNM_SNMP_BADVALUE; break;	
	      case TNM_SNMP_RESOURCEUNAVAILABLE:
	      case TNM_SNMP_COMMITFAILED:
	      case TNM_SNMP_UNDOFAILED:
		pdu->errorStatus = TNM_SNMP_GENERR; break;
	      default:
		pdu->errorStatus = TNM_SNMP_GENERR; break;
	    }
	}
    }

    /*
     * Encode message into ASN1 BER transfer syntax. Authentication or
     * encryption is done within the following procedures if it is an
     * authentic or private message.
     */

    ber = TnmBerCreate(packet, sizeof(packet));
    code = EncodeMessage(interp, session, pdu, ber);
    if (code != TCL_OK) {
	TnmBerDelete(ber);
	return TCL_ERROR;
    }
    packetlen = TnmBerSize(ber);
    TnmBerDelete(ber);

    switch (pdu->type) {
      case ASN1_SNMP_GET:
	  tnmSnmpStats.snmpOutGetRequests++;
	  break;
      case ASN1_SNMP_GETNEXT:
	  tnmSnmpStats.snmpOutGetNexts++;
	  break;
      case ASN1_SNMP_SET:
	  tnmSnmpStats.snmpOutSetRequests++;
	  break;
      case ASN1_SNMP_RESPONSE:
	  tnmSnmpStats.snmpOutGetResponses++;
	  break;
      case ASN1_SNMP_TRAP1:
	  tnmSnmpStats.snmpOutTraps++; 
	  break;
    }
    
    /*
     * Show the contents of the PDU - mostly for debugging.
     */
    
    TnmSnmpEvalBinding(interp, session, pdu, TNM_SNMP_SEND_EVENT);

    TnmSnmpDumpPDU(interp, pdu);

    /*
     * A trap message or a response? - send it and we are done!
     */
    
    if (pdu->type == ASN1_SNMP_TRAP1 || pdu->type == ASN1_SNMP_TRAP2 
	|| pdu->type == ASN1_SNMP_RESPONSE || pdu->type == ASN1_SNMP_REPORT) {
#ifdef TNM_SNMPv2U
	if (session->version == TNM_SNMPv2U) {
	    TnmSnmpUsecAuth(session, packet, packetlen);
	}
#endif
	code = TnmSnmpSend(interp, session, packet, packetlen,
			   &pdu->addr, TNM_SNMP_ASYNC);
	if (code != TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_ResetResult(interp);
	return TCL_OK;
    }
  
    /*
     * Asychronous request: queue request and we are done.
     */

    if (proc) {
	TnmSnmpRequest *rPtr;
	rPtr = TnmSnmpCreateRequest(pdu->requestId, packet, packetlen,
				    proc, clientData, interp);
	TnmSnmpQueueRequest(session, rPtr);
	sprintf(interp->result, "%d", (int) pdu->requestId);
	return TCL_OK;
    }
    
    /*
     * Synchronous request: send packet and wait for response.
     */
    
    for (retry = 0; retry <= session->retries; retry++) {
	int id, status, index;
#ifdef TNM_SNMP_BENCH
	TnmSnmpMark stats;
	memset((char *) &stats, 0, sizeof(stats));
#endif

      repeat:
#ifdef TNM_SNMPv2U
	if (session->version == TNM_SNMPv2U) {
	    TnmSnmpUsecAuth(session, packet, packetlen);
	}
#endif
	TnmSnmpDelay(session);
	code = TnmSnmpSend(interp, session, packet, packetlen, 
			   &pdu->addr, TNM_SNMP_SYNC);
	if (code != TCL_OK) {
	    return TCL_ERROR;
	}

#ifdef TNM_SNMP_BENCH
	if (stats.sendSize == 0) {
	    stats.sendSize = tnmSnmpBenchMark.sendSize;
	    stats.sendTime = tnmSnmpBenchMark.sendTime;
	}
#endif

	while (TnmSnmpWait(session->timeout * 1000
			   / (session->retries + 1), TNM_SNMP_SYNC) > 0) {
	    u_char packet[TNM_SNMP_MAXSIZE];
	    int rc, packetlen = TNM_SNMP_MAXSIZE;
	    struct sockaddr_in from;

	    code = TnmSnmpRecv(interp, packet, &packetlen, 
			       &from, TNM_SNMP_SYNC);
	    if (code != TCL_OK) {
		return TCL_ERROR;
	    }
	    
	    rc = TnmSnmpDecode(interp, packet, packetlen, &from,
			       session, &id, &status, &index);
	    if (rc == TCL_BREAK) {
		if (retry++ <= session->retries + 1) {
		    goto repeat;
		}
	    }
	    if (rc == TCL_OK) {
		if (id == pdu->requestId) {
#ifdef TNM_SNMP_BENCH
		    stats.recvSize = tnmSnmpBenchMark.recvSize;
		    stats.recvTime = tnmSnmpBenchMark.recvTime;
		    session->stats = stats;
#endif
		    return TCL_OK;
		}
		rc = TCL_CONTINUE;
	    }
	    
	    if (rc == TCL_CONTINUE) {
		if (hexdump) {
		    fprintf(stderr, "%s\n", interp->result);
		}
		continue;
	    }
	    if (rc == TCL_ERROR) {
		pdu->errorStatus = status;
		pdu->errorIndex = index;
		return TCL_ERROR;
	    }
	}
    }
    
    Tcl_SetResult(interp, "noResponse 0 {}", TCL_STATIC);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * EncodeMessage --
 *
 *	This procedure takes a session and a PDU and serializes the
 *	ASN1 pdu as an octet string into the buffer pointed to by 
 *	packet using the "Basic Encoding Rules". See RFC 1157 and 
 *	RFC 1910 for the message header description. The main parts
 *	are the version number, the community string and the SNMP PDU.
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
EncodeMessage(interp, session, pdu, ber)
    Tcl_Interp *interp;
    TnmSnmp *session;
    TnmSnmpPdu *pdu;
    TnmBer *ber;
{
    u_char *seqToken;
    int version = 0, parameterLen = 0;
    u_char *parameter = NULL;
#ifdef TNM_SNMPv2U
#define PARAM_MAX_LENGTH 340
    u_char buffer[PARAM_MAX_LENGTH];

    if (session->qos & USEC_QOS_PRIV) {
	Tcl_SetResult(interp, "encryption not supported", TCL_STATIC);
	return TCL_ERROR;
    }
#endif

    ber = TnmBerEncSequenceStart(ber, ASN1_SEQUENCE, &seqToken);

    switch (session->version) {
      case TNM_SNMPv1:
	version = 0;
	parameter = Tcl_GetStringFromObj(session->community,
					 &parameterLen);
	break;
      case TNM_SNMPv2C:
	version = 1;
	parameter = Tcl_GetStringFromObj(session->community,
					 &parameterLen);
	break;
#ifdef TNM_SNMPv2U
      case TNM_SNMPv2U:
	version = 2;
	parameter = buffer;
	parameterLen = EncodeUsecParameter(session, pdu, parameter);
	break;
#endif
    case TNM_SNMPv3:
	version = 3;
	break;
    }
    ber = TnmBerEncInt(ber, ASN1_INTEGER, version);
    if (version < 3) {
	ber  = TnmBerEncOctetString(ber, ASN1_OCTET_STRING,
				    (char *) parameter, parameterLen);
	ber = EncodePDU(interp, session, pdu, ber);
	if (ber == NULL) {
	    if (*interp->result == '\0') {
		Tcl_SetResult(interp, TnmBerGetError(NULL), TCL_STATIC);
	    }
	    return TCL_ERROR;
	}
    }

    if (version == 3) {
	int secParamLength;
	char *secParam;
	ber = EncodeHeader(interp, session, pdu, ber);
	secParam = EncodeUsmSecParams(session, pdu, &secParamLength);
	if (! secParam) {
	    Tcl_SetResult(interp, TnmBerGetError(NULL), TCL_STATIC);
	    return TCL_ERROR;
	}
	ber = TnmBerEncOctetString(ber, ASN1_OCTET_STRING,
				   (char *) secParam, secParamLength);
	ber = EncodeScopedPDU(interp, session, pdu, ber);
	if (*interp->result == '\0') {
	    Tcl_SetResult(interp, TnmBerGetError(NULL), TCL_STATIC);
	}
    }

    ber = TnmBerEncSequenceEnd(ber, seqToken);
    return (ber ? TCL_OK : TCL_ERROR);
}

/*
 *----------------------------------------------------------------------
 *
 * EncodeHeader --
 *
 *	This procedure serializes the SNMPv3 message header which
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
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmBer*
EncodeHeader(interp, session, pdu, ber)
    Tcl_Interp *interp;
    TnmSnmp *session;
    TnmSnmpPdu *pdu;
    TnmBer *ber;
{
    u_char *seqToken;
    u_char flags = 0;

    /*
     * Set the reportable flag on a request-type PDU and an
     * acknowledged notification-type PDU. (RFC 2272, section 6.4)
     */

    switch (pdu->type) {
    case ASN1_SNMP_GET:
    case ASN1_SNMP_GETNEXT:
    case ASN1_SNMP_GETBULK:
    case ASN1_SNMP_SET:
    case ASN1_SNMP_INFORM:
	flags |= TNM_SNMP_FLAG_REPORT;
	break;
    default:
	flags &= ~TNM_SNMP_FLAG_REPORT;
    }

    if (session->securityLevel & TNM_SNMP_AUTH_MASK) {
	flags |= TNM_SNMP_FLAG_AUTH;
    }
    if (session->securityLevel & TNM_SNMP_PRIV_MASK) {
	flags |= TNM_SNMP_FLAG_PRIV;
    }

    ber = TnmBerEncSequenceStart(ber, ASN1_SEQUENCE, &seqToken);

    ber = TnmBerEncInt(ber, ASN1_INTEGER, pdu->requestId);
    ber = TnmBerEncInt(ber, ASN1_INTEGER, TNM_SNMP_MAXSIZE);
    ber = TnmBerEncOctetString(ber, ASN1_OCTET_STRING, &flags, 1);
    ber = TnmBerEncInt(ber, ASN1_INTEGER, TNM_SNMP_USM_SEC_MODEL);

    ber = TnmBerEncSequenceEnd(ber, seqToken);
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * EncodeUsmSecParams --
 *
 *	This procedure serializes the SNMPv3 USM security parameters.
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
 *	A pointer to the beginning to the encoded security parameters.
 *	The length is returned in the lengthPtr parameter.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static u_char*
EncodeUsmSecParams(session, pdu, lengthPtr)
    TnmSnmp *session;
    TnmSnmpPdu *pdu;
    int *lengthPtr;
{
    u_char *seqToken;
    char *user, *engineID;
    int userLength, engineIDLength;
    static u_char buffer[TNM_SNMP_MAXSIZE];
    TnmBer *ber;

    /*
     * Start building the UsmSecurityParameters field.
     */

    ber = TnmBerCreate(buffer, sizeof(buffer));
    ber = TnmBerEncSequenceStart(ber, ASN1_SEQUENCE, &seqToken);

    engineID = TnmGetOctetStringFromObj(NULL, session->engineID,
					&engineIDLength);
    ber = TnmBerEncOctetString(ber, ASN1_OCTET_STRING,
			       engineID, engineIDLength);

    if (pdu->type == ASN1_SNMP_RESPONSE
	|| session->securityLevel & TNM_SNMP_AUTH_MASK) {
	ber = TnmBerEncInt(ber, ASN1_INTEGER, session->engineBoots);
	ber = TnmBerEncInt(ber, ASN1_INTEGER, session->engineTime);
    } else {
	ber = TnmBerEncInt(ber, ASN1_INTEGER, 0);
	ber = TnmBerEncInt(ber, ASN1_INTEGER, 0);
    }
        
    user = Tcl_GetStringFromObj(session->user, &userLength);
    ber = TnmBerEncOctetString(ber, ASN1_OCTET_STRING,
			       user, userLength);
    
    if (session->securityLevel & TNM_SNMP_AUTH_MASK) {
	char zeros[12];
	memset(zeros, 0, 12);
	ber = TnmBerEncOctetString(ber, ASN1_OCTET_STRING, zeros, 12);
    } else {
	ber = TnmBerEncOctetString(ber, ASN1_OCTET_STRING, "", 0);
    }
    ber = TnmBerEncOctetString(ber, ASN1_OCTET_STRING, "", 0);
    ber = TnmBerEncSequenceEnd(ber, seqToken);

    if (! ber) {
	*lengthPtr = 0;
	/* TnmBerDelete(ber); xxx */
	return NULL;
    }

    *lengthPtr = (ber->current - ber->start);
    TnmBerDelete(ber);
    return buffer;
}

/*
 *----------------------------------------------------------------------
 *
 * EncodeScopedPDU --
 *
 *	This procedure serializes the SNMPv3 scoped PDU as defined
 *	in RFC 2272, section 6.
 *
 *	    ScopedPDU ::= SEQUENCE {
 *	        contextEngineID  OCTET STRING,
 *		contextName      OCTET STRING,
 *		data             ANY -- e.g., PDUs as defined in RFC1905
 *	    }
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
EncodeScopedPDU(interp, session, pdu, ber)
    Tcl_Interp *interp;
    TnmSnmp *session;
    TnmSnmpPdu *pdu;
    TnmBer *ber;
{
    u_char *seqToken;
    char *context, *engineID;
    int contextLength, engineIDLength;

    ber = TnmBerEncSequenceStart(ber, ASN1_SEQUENCE, &seqToken);

    engineID = TnmGetOctetStringFromObj(NULL, session->engineID,
					&engineIDLength);
    ber = TnmBerEncOctetString(ber, ASN1_OCTET_STRING,
			       engineID, engineIDLength);
    context = Tcl_GetStringFromObj(session->context, &contextLength);
    ber = TnmBerEncOctetString(ber, ASN1_OCTET_STRING,
			       context, contextLength);
    ber = EncodePDU(interp, session, pdu, ber);

    ber = TnmBerEncSequenceEnd(ber, seqToken);
    return ber;
}

#ifdef TNM_SNMPv2U
/*
 *----------------------------------------------------------------------
 *
 * EncodeUsecParameter --
 *
 *	This procedure builds the parameters string. Note, some fields
 *	are left blank as they are filled in later in TnmSnmpUsecAuth().
 *	This way we can patch in new agentTime or agentBoot values in 
 *	case our clock drifts away.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
EncodeUsecParameter(session, pdu, parameter)
    TnmSnmp *session;
    TnmSnmpPdu *pdu;
    u_char *parameter;
{
    u_char *p = parameter, *context, *user;
    u_int boots = 0, clock = 0;
    int contextLength, userLength;

    /* 
     * The first byte is the model indicator, which is 0 for the USEC 
     * model. The second byte is the quality of service (qos) field.
     */

    *p++ = USEC_MODEL;

    if ((pdu->type == ASN1_SNMP_GET) 
	|| (pdu->type == ASN1_SNMP_GETNEXT)
	|| (pdu->type == ASN1_SNMP_GETBULK) 
	|| (pdu->type == ASN1_SNMP_SET)) {
	*p++ = session->qos | USEC_QOS_REPORT;
    } else {
	*p++ = session->qos;
    }

    /*
     * The following bytes contain the agent identifier. This value
     * is only filled if we authenticate the message or if we send
     * a notification.
     */

    if ((session->qos & USEC_QOS_REPORT)
	|| (session->qos & USEC_QOS_PRIV)
	|| (pdu->type == ASN1_SNMP_TRAP2)
	|| (pdu->type == ASN1_SNMP_INFORM)) {
	memcpy(p, session->agentID, USEC_MAX_AGENTID);
    } else {
	memset(p, 0, USEC_MAX_AGENTID);
    }
    p += USEC_MAX_AGENTID;
    
    /* 
     * The next 4 bytes contain the number of agent boots followed by
     * the current agent time which is calculated by using the time
     * offset saved in the session structure. Note, these fields are
     * patched later and set to zero if the message is not authenticated.
     */

    if (session->qos & USEC_QOS_AUTH || session->qos & USEC_QOS_PRIV) {
	boots = session->agentBoots;
	clock = time((time_t *) NULL) - session->agentTime;
    }

    *p++ = (boots >> 24) & 0xff;
    *p++ = (boots >> 16) & 0xff;
    *p++ = (boots >> 8) & 0xff;
    *p++ = boots & 0xff;
    *p++ = (clock >> 24) & 0xff;
    *p++ = (clock >> 16) & 0xff;
    *p++ = (clock >> 8) & 0xff;
    *p++ = clock & 0xff;

#ifdef DEBUG_USEC
    fprintf(stderr, "encode: agentBoots = %u agentTime = %u\n",
	    session->agentBoots, session->agentTime);
#endif

    /*
     * The following two bytes contain the max message size we accept.
     */
    
    *p++ = (session->maxSize >> 8) & 0xff;
    *p++ = session->maxSize & 0xff;
    
    /*
     * The next variable length field contains the user name. The first
     * byte is the length of the user name.
     */

    user = Tcl_GetStringFromObj(session->user, &userLength);
    *p++ = userLength;
    memcpy(p, user, userLength);
    p += userLength;

    /*
     * The next variable length field is the authentication digest. Its
     * length is 0 for unauthenticated messages or 16 for the MD5 digest
     * algorithm. Note, this field is patched later.
     */
    
    if (session->qos & USEC_QOS_AUTH || session->qos & USEC_QOS_PRIV) {
	*p++ = TNM_MD5_SIZE;
	p += TNM_MD5_SIZE;
    } else {
	*p++ = 0;
    }

    /* 
     * Note, the context identifier is variable length but the length
     * is not given as it is implicitly contained in the length of the
     * octet string.  
     */

    context = Tcl_GetStringFromObj(session->context, &contextLength);
    memcpy(p, context, contextLength);
    p += contextLength;
    return (p - parameter);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpUsecAuth --
 *
 *	This procedure patches the USEC authentication information
 *	into a BER encoded SNMP USEC packet. We therefore decode the 
 *	packet until we have found the parameter string. This is still 
 *	a lot faster than doing BER encodings every time the packet 
 *	is sent. 
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
TnmSnmpUsecAuth(session, packet, packetlen)
    TnmSnmp *session;
    u_char *packet;
    int packetlen;
{
    u_char *parm, *p = packet;
    int dummy = packetlen;
    u_int boots = 0, clock = 0;
    u_char digest[TNM_MD5_SIZE];

    /*
     * Get the parameter section out of the message header.
     */
    
    if (*p ++ != ASN1_SEQUENCE) {
	return;
    }
    p = TnmBerDecLength(p, &dummy, (u_int *) &dummy);
    p = TnmBerDecInt(p, &dummy, ASN1_INTEGER, &dummy);
    p = TnmBerDecOctetString(p, &dummy, ASN1_OCTET_STRING, 
			     (char **) &parm, &dummy);
    if (! p) return;

    /*
     * Set the agentID, the agentBoots and the agentTime values.
     */

    p = parm + 2;
    memcpy(p, session->agentID, USEC_MAX_AGENTID);
    p += USEC_MAX_AGENTID;

    if (session->qos & USEC_QOS_AUTH || session->qos & USEC_QOS_PRIV) {
	boots = session->agentBoots;
	clock = time((time_t *) NULL) - session->agentTime;
	*p++ = (boots >> 24) & 0xff;
	*p++ = (boots >> 16) & 0xff;
	*p++ = (boots >> 8) & 0xff;
	*p++ = boots & 0xff;
	*p++ = (clock >> 24) & 0xff;
	*p++ = (clock >> 16) & 0xff;
	*p++ = (clock >> 8) & 0xff;
	*p++ = clock & 0xff;
	
#ifdef DEBUG_USEC
	fprintf(stderr, "auth: agentBoots = %u agentTime = %u\n",
		session->agentBoots, session->agentTime);
#endif

	/*
	 * Skip the max message size field.
	 */
	
	p += 2;
	
	/*
	 * Skip the user name field, check the digest len (should be 16),
	 * copy the key into the packet, compute the digest and copy the
	 * result back into the packet.
	 */
	
	p += *p;
	p++;
	if (*p++ != TNM_MD5_SIZE) return;
	memcpy(p, session->authKey, TNM_MD5_SIZE);
	TnmSnmpMD5Digest(packet, packetlen, session->authKey, digest);
	memcpy(p, digest, TNM_MD5_SIZE);
    }
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * EncodePDU --
 *
 *	This procedure takes a session and a PDU and serializes it 
 *	into an ASN.1 PDU hold in the buffer pointed to by packet
 *	using the "Basic Encoding Rules".
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TnmBer*
EncodePDU(interp, session, pdu, ber)
    Tcl_Interp *interp;
    TnmSnmp *session;
    TnmSnmpPdu *pdu;
    TnmBer *ber;
{    
    u_char *pduSeqToken, *vbSeqToken, *vblSeqToken;
    
    int i, vblc, vbc;
    char **vblv, **vbv;

    Tnm_Oid *oid;
    int oidlen;

    ber = TnmBerEncSequenceStart(ber, (u_char) pdu->type, &pduSeqToken);

    if (pdu->type == ASN1_SNMP_TRAP1) {

	int generic = 0, specific = 0;
	struct sockaddr_in addr;

	/*
	 * RFC 1907 defines the standard trap using snmpTraps which
	 * is registered at 1.3.6.1.6.3.1.1.5. A TRAP-TYPE macro (RFC 
	 * 1215) is converted into a SNMPv2 oid using the rules in
	 * RFC 1908 and these rules use the snmp node registered
	 * at 1.3.6.1.2.1.11. We have to accept both definitions
	 * for the standard traps to be backward compatible.
	 */

	oid = TnmStrToOid(pdu->trapOID, &oidlen);
	if (! oid || oidlen < 4) {
	    Tcl_SetResult(interp, "illegal notification object identifier",
			  TCL_STATIC);
	    return NULL;
	}
	if (strncmp(pdu->trapOID, "1.3.6.1.6.3.1.1.5", 17) == 0) {
	    generic = oid[oidlen-1] - 1;
	    specific = 0;
	    ber = TnmBerEncOID(ber,
			       TnmOidGetElements(&session->enterpriseOid),
			       TnmOidGetLength(&session->enterpriseOid));
	} else if (strncmp(pdu->trapOID, "1.3.6.1.2.1.11.0", 16) == 0) {
            generic = oid[oidlen-1];
            specific = 0;
	    ber = TnmBerEncOID(ber,
			       TnmOidGetElements(&session->enterpriseOid),
			       TnmOidGetLength(&session->enterpriseOid));
	} else {
	    generic = 6;
	    specific = oid[oidlen-1];
	    ber = TnmBerEncOID(ber, oid, oidlen-2);
	}
	if (ber == NULL) {
	    Tcl_SetResult(interp, 
			  "failed to encode enterprise object identifier", 
			  TCL_STATIC);
	    return NULL;
	}
 
	if (TnmSetIPAddress(interp, Tcl_GetHostName(), &addr) != TCL_OK) {
	    return NULL;
	}
	ber = TnmBerEncOctetString(ber, ASN1_IPADDRESS,
				   (char *) &addr.sin_addr, 4);

	ber = TnmBerEncInt(ber, ASN1_INTEGER, generic);
	ber = TnmBerEncInt(ber, ASN1_INTEGER, specific);
	ber = TnmBerEncInt(ber, ASN1_TIMETICKS, TnmSnmpSysUpTime());

    } else {
    
	ber = TnmBerEncInt(ber, ASN1_INTEGER, pdu->requestId);
	ber = TnmBerEncInt(ber, ASN1_INTEGER, pdu->errorStatus);
	switch (pdu->errorStatus) {
	  case TNM_SNMP_TOOBIG:
	      tnmSnmpStats.snmpOutTooBigs++;
	      break;
	  case TNM_SNMP_NOSUCHNAME:
	      tnmSnmpStats.snmpOutNoSuchNames++;
	      break;
	  case TNM_SNMP_BADVALUE:
	      tnmSnmpStats.snmpOutBadValues++;
	      break;
	  case TNM_SNMP_READONLY:
	      break; /* not used */
	  case TNM_SNMP_GENERR:
	      tnmSnmpStats.snmpOutGenErrs++;
	      break;
	}
	ber = TnmBerEncInt(ber, ASN1_INTEGER, pdu->errorIndex);
    }

    /*
     * encode VarBindList ( SEQUENCE OF VarBind )
     */
    
    ber = TnmBerEncSequenceStart(ber, ASN1_SEQUENCE, &vblSeqToken);
    
    /*
     * split the varbind list and loop over all elements
     */
    
    if (Tcl_SplitList(interp, Tcl_DStringValue(&pdu->varbind), &vblc, &vblv)
	!= TCL_OK) {
	return NULL;
    }

    if (pdu->type == ASN1_SNMP_TRAP2 || pdu->type == ASN1_SNMP_INFORM) {

	/* 
	 * Encode two default varbinds: sysUpTime.0 and snmpTrapOID.0
	 * as defined in section 4.2.7 of RFC 1905.
	 */

	ber = TnmBerEncSequenceStart(ber, ASN1_SEQUENCE, &vbSeqToken);
	oid = TnmStrToOid("1.3.6.1.2.1.1.3.0", &oidlen);
	ber = TnmBerEncOID(ber, oid, oidlen);
	ber = TnmBerEncInt(ber, ASN1_TIMETICKS, TnmSnmpSysUpTime());
	ber = TnmBerEncSequenceEnd(ber, vbSeqToken);

	ber = TnmBerEncSequenceStart(ber, ASN1_SEQUENCE, &vbSeqToken);
	oid = TnmStrToOid("1.3.6.1.6.3.1.1.4.1.0", &oidlen);
	ber = TnmBerEncOID(ber, oid, oidlen);
	oid = TnmStrToOid(pdu->trapOID, &oidlen);
	ber = TnmBerEncOID(ber, oid, oidlen);
	ber = TnmBerEncSequenceEnd(ber, vbSeqToken);
    }
    
    for (i = 0; i < vblc; i++) {
	
	char *value;
	int asn1_type = ASN1_OTHER;
	
	/*
	 * split a single varbind into its components
	 */
	
	if (Tcl_SplitList(interp, vblv[i], &vbc, &vbv) != TCL_OK) {
	    ckfree((char *) vblv);
	    return NULL;
	}

	if (vbc == 0) {
	    Tcl_SetResult(interp, "missing OBJECT IDENTIFIER", TCL_STATIC);
	    ckfree((char *) vblv);
            return NULL;
	}
	
	/*
	 * encode each VarBind ( SEQUENCE name, value )
	 */
	
	ber = TnmBerEncSequenceStart(ber, ASN1_SEQUENCE, &vbSeqToken);

	/*
	 * encode the object identifier, perhaps consulting the MIB
	 */

	oid = TnmStrToOid(vbv[0], &oidlen);
	if (! oid) {
	    char *tmp = TnmMibGetOid(vbv[0]);
	    if (tmp) {
		oid = TnmStrToOid(tmp, &oidlen);
	    }
	}
	if (! oid) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "invalid object identifier \"",
			     vbv[0], "\"", (char *) NULL);
	    ckfree((char *) vbv);
	    ckfree((char *) vblv);
	    return NULL;
	}

	ber = TnmBerEncOID(ber, oid, oidlen);
	if (ber == NULL) {
	    break;
	}

	/*
	 * guess the asn1 type field and the value
	 */

	switch (vbc) {
	  case 1:
	    value = "";
	    asn1_type = ASN1_NULL;
	    break;
	  case 2:
	    value = vbv[1];
	    asn1_type = TnmMibGetBaseSyntax(vbv[0]);
	    break;
	  default:
	    value = vbv[2];

	    /*
	     * Check if there is an exception in the asn1 type field.
	     * Convert this into an appropriate NULL type if we create
	     * a response PDU. Otherwise, ignore this stuff and use
	     * the type found in the MIB.
	     */

	    if (pdu->type == ASN1_SNMP_RESPONSE) {
		asn1_type = TnmGetTableKey(tnmSnmpExceptionTable, vbv[1]);
	        if (asn1_type < 0) {
		    asn1_type = TnmGetTableKey(tnmSnmpTypeTable, vbv[1]);
		    if (asn1_type < 0) {
			asn1_type = ASN1_OTHER;
		    }
		}
	    } else {
		asn1_type = TnmGetTableKey(tnmSnmpTypeTable, vbv[1]);
		if (asn1_type < 0) {
		    asn1_type = ASN1_OTHER;
		}
	    }

	    if (asn1_type == ASN1_OTHER) {
		TnmMibType *typePtr;
		typePtr = TnmMibFindType(vbv[1]);
		if (typePtr) {
		    asn1_type = typePtr->syntax;
		}
	    }
	    break;
	}

	if (asn1_type == ASN1_OTHER) {
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "unknown type \"", vbv[1], "\"",
			     (char *) NULL);
	    ckfree((char *) vbv);
	    ckfree((char *) vblv);
	    return NULL;
	}

	/*
         * Check whether we have to encode the value. Don't bother
	 * to encode the actual value for retrieval operations.
	 */

	if (TnmSnmpGet(pdu->type)) {
	    ber = TnmBerEncNull(ber, ASN1_NULL);
	} else {
	    switch (asn1_type) {
	    case ASN1_INTEGER:
	    case ASN1_COUNTER32:
	    case ASN1_GAUGE32:
	    case ASN1_TIMETICKS: {
		int int_val, rc;
		rc = Tcl_GetInt(interp, value, &int_val);
		if (rc != TCL_OK) {
		    char *tmp = TnmMibScan(vbv[0], 0, value);
		    if (tmp && *tmp) {
			Tcl_ResetResult(interp);
			rc = Tcl_GetInt(interp, tmp, &int_val);
		    }
		    if (rc != TCL_OK) return NULL;
		}
		ber = TnmBerEncInt(ber, (u_char) asn1_type, int_val);
		break;
	    }
	    case ASN1_COUNTER64: {
		int int_val, rc;
		if (session->version == TNM_SNMPv1) {
		    Tcl_SetResult(interp,
				  "Counter64 not allowed on an SNMPv1 session",
				  TCL_STATIC);
		    return NULL;
		}
		if (sizeof(int) >= 8) {
		    rc = Tcl_GetInt(interp, value, &int_val);
		    if (rc != TCL_OK) {
			return NULL;
		    }
		    ber = TnmBerEncInt(ber, ASN1_COUNTER64, int_val);
		} else {
		    double d;
		    rc = Tcl_GetDouble(interp, value, &d);
		    if (rc != TCL_OK) {
			return NULL;
		    }
		    if (d < 0) {
			Tcl_SetResult(interp, "negativ counter value",
				      TCL_STATIC);
			return NULL;
		    }
		    ber = TnmBerEncUnsigned64(ber, d);
		}
		break;
	    }
	    case ASN1_IPADDRESS: {
		int a, b, c, d, addr = inet_addr(value);
		int cnt = sscanf(value, "%d.%d.%d.%d", &a, &b, &c, &d);
		if ((addr == -1 && strcmp(value, "255.255.255.255") != 0)
		    || (cnt != 4)) {
		    Tcl_SetResult(interp, "invalid IP address", TCL_STATIC);
		    return NULL;
		}
		ber = TnmBerEncOctetString(ber, ASN1_IPADDRESS,
					   (char *) &addr, 4);
		break;
	    }
	    case ASN1_OCTET_STRING: {
		char *hex = value, *scan;
		size_t len;
		static char *bin = NULL;
		static size_t binLen = 0;
		/* quick test for empty strings ... */
		if (value[0] == 0) {
		    ber = TnmBerEncOctetString(ber, ASN1_OCTET_STRING, NULL, 0);
		    break;
		}
		scan = TnmMibScan(vbv[0], 0, value);
		if (scan) hex = scan;
		if (*hex) {
		    len = strlen(hex);
		    if (binLen < len + 1) {
			if (bin) ckfree(bin);
			binLen = len + 1;
			bin = ckalloc(binLen);
		    }
		    if (TnmHexDec(hex, bin, &len) < 0) {
			Tcl_SetResult(interp, "illegal OCTET STRING value",
				      TCL_STATIC);
			return NULL;
		    }
		} else {
		    len = 0;
		}
		ber = TnmBerEncOctetString(ber, ASN1_OCTET_STRING, bin, len);
		break;
	    }
	    case ASN1_OPAQUE: {
		char *hex = value;
		size_t len;
		static char *bin = NULL;
		static size_t binLen = 0;
		if (*hex) {
		    len = strlen(hex);
		    if (binLen < len + 1) {
			if (bin) ckfree(bin);
			binLen = len + 1;
			bin = ckalloc(binLen);
		    }
		    if (TnmHexDec(hex, bin, &len) < 0) {
			Tcl_SetResult(interp, "illegal Opaque value",
				      TCL_STATIC);
			return NULL;
		    }
		} else {
		    len = 0;
		}
		ber = TnmBerEncOctetString(ber, ASN1_OPAQUE, bin, len);
		break;
	    }
	    case ASN1_OBJECT_IDENTIFIER:
		oid = TnmStrToOid(value, &oidlen);
		if (! oid) {
		    char *tmp = TnmMibGetOid(value);
		    if (tmp) {
			oid = TnmStrToOid(tmp, &oidlen);
		    }
		}
		if (! oid) {
		    Tcl_AppendResult(interp, 
				     "illegal object identifier \"",
				     value, "\"", (char *) NULL);
		    return NULL;
		}
		ber = TnmBerEncOID(ber, oid, oidlen);
		break;
	    case ASN1_NO_SUCH_OBJECT:
	    case ASN1_NO_SUCH_INSTANCE:
	    case ASN1_END_OF_MIB_VIEW:
	    case ASN1_NULL:
		ber = TnmBerEncNull(ber, (u_char) asn1_type);
		break;
	    default:
		sprintf(interp->result, "unknown asn1 type 0x%.2x",
			asn1_type);
		return NULL;
	    }
	}
	
	ber = TnmBerEncSequenceEnd(ber, vbSeqToken);

	ckfree((char *) vbv);
    }

    ckfree((char *) vblv);

    ber = TnmBerEncSequenceEnd(ber, vblSeqToken);
    ber = TnmBerEncSequenceEnd(ber, pduSeqToken);
    return ber;
}
