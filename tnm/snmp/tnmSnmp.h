/*
 * tnmSnmp.h --
 *
 *	Definitions for the SNMP protocol implementation.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmSnmp.h,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

#ifndef _TNMSNMP
#define _TNMSNMP

#include "tnmInt.h"
#include "tnmAsn1.h"
#include "tnmMib.h"

/*
 *----------------------------------------------------------------
 * Some general defines. Note, all SNMPv2 versions that we will
 * ever implement should have the same SNMPv2 bit set, so that 
 * we can use this bit if we allow SNMPv2 protocol operations.
 *
 * You can turn off support for TNM_SNMPv2U by undefining this
 * symbol. This will make the code a bit smaller, but you won't 
 * gain much speed. So you might want to let these defines as
 * they are.
 *----------------------------------------------------------------
 */

#define TNM_SNMPv1		0x11
#define TNM_SNMPv2		0x20
#define TNM_SNMPv2C		0x21
/* #define TNM_SNMPv2U		0x22 */
#define TNM_SNMPv3		0x23

#if defined(TNM_SNMPv2U) && defined(TNM_SNMPv3)
#define TNM_SNMP_USER(session) \
	(session->version == TNM_SNMPv2U || session->version == TNM_SNMPv3)
#else
#if defined(TNM_SNMPv2U)
#define TNM_SNMP_USER(session) (session->version == TNM_SNMPv2U)
#else
#define TNM_SNMP_USER(session) (session->version == TNM_SNMPv3)
#endif
#endif

EXTERN TnmTable tnmSnmpVersionTable[];

/*
 *----------------------------------------------------------------
 * Error status definitions as of RFC 1905 and RFC 1157. RFC 1157
 * only defines the errors NOERROR - GENERR. Some of the version
 * V1 error codes are only for V1/V2 proxy compatibility.
 *----------------------------------------------------------------
 */

#define TNM_SNMP_NOERROR		0	/*  v1 / v2 error code */
#define TNM_SNMP_TOOBIG			1	/*  v1 / v2 error code */
#define TNM_SNMP_NOSUCHNAME		2	/*  v1      error code */
#define TNM_SNMP_BADVALUE		3	/*  v1      error code */
#define TNM_SNMP_READONLY		4	/*  v1      error code */
#define TNM_SNMP_GENERR			5	/*  v1 / v2 error code */
#define TNM_SNMP_NOACCESS		6	/*       v2 error code */
#define TNM_SNMP_WRONGTYPE		7	/*       v2 error code */
#define TNM_SNMP_WRONGLENGTH		8	/*       v2 error code */
#define TNM_SNMP_WRONGENCODING		9	/*       v2 error code */
#define TNM_SNMP_WRONGVALUE		10	/*       v2 error code */
#define TNM_SNMP_NOCREATION		11	/*       v2 error code */
#define TNM_SNMP_INCONSISTENTVALUE	12	/*       v2 error code */
#define TNM_SNMP_RESOURCEUNAVAILABLE	13	/*       v2 error code */
#define TNM_SNMP_COMMITFAILED		14	/*       v2 error code */
#define TNM_SNMP_UNDOFAILED		15	/*       v2 error code */
#define TNM_SNMP_AUTHORIZATIONERROR	16	/*       v2 error code */
#define TNM_SNMP_NOTWRITABLE		17	/*       v2 error code */
#define TNM_SNMP_INCONSISTENTNAME	18	/*       v2 error code */
#define TNM_SNMP_ENDOFWALK	       254	/* internal error code */
#define TNM_SNMP_NORESPONSE	       255	/* internal error code */

EXTERN TnmTable tnmSnmpErrorTable[];

/*
 *----------------------------------------------------------------
 * The following constants define the default session 
 * configuration.
 *----------------------------------------------------------------
 */

#define	TNM_SNMP_PORT		161
#define	TNM_SNMP_TRAPPORT	162
#define TNM_SNMP_RETRIES	3
#define TNM_SNMP_TIMEOUT	5
#define TNM_SNMP_WINDOW		10
#define TNM_SNMP_DELAY		0

/*
 *----------------------------------------------------------------
 * The size of the internal buffer used to decode or assemble 
 * SNMP packets and the size of MD5 keys.
 *----------------------------------------------------------------
 */

#define TNM_SNMP_MAXSIZE	16384
#define TNM_MD5_SIZE		16

/*
 *----------------------------------------------------------------
 * A structure to keep performance statistics.
 *----------------------------------------------------------------
 */

#ifdef TNM_SNMP_BENCH
typedef struct TnmSnmpMark {
    Tcl_Time sendTime;
    Tcl_Time recvTime;
    int sendSize;
    int recvSize;
} TnmSnmpMark;

EXTERN TnmSnmpMark tnmSnmpBenchMark;
#endif

/*
 *----------------------------------------------------------------
 * Definitions for the user based security model (USEC). See 
 * RFC 1909 and RFC 1910 for more details.
 *----------------------------------------------------------------
 */

#ifdef TNM_SNMPv2U
#define USEC_QOS_NULL		(0x00)
#define USEC_QOS_AUTH		(0x01)
#define USEC_QOS_PRIV		(0x02)
#define USEC_QOS_REPORT		(0x04)

#define USEC_MODEL		1
#define USEC_MAX_USER		16
#define USEC_MAX_CONTEXT	40
#define USEC_MAX_AGENTID	12
#define USEC_MAX_MMS		65507
#define USEC_MIN_MMS		484
#endif

/*
 *----------------------------------------------------------------
 * Definitions for SNMP version 3 and related specifications.
 *----------------------------------------------------------------
 */

#ifdef TNM_SNMPv3
#define TNM_SNMP_USM_SEC_MODEL	3

#define TNM_SNMP_FLAG_AUTH	0x01
#define TNM_SNMP_FLAG_PRIV	0x02
#define TNM_SNMP_FLAG_REPORT	0x04

#define TNM_SNMP_AUTH_NONE	0x00
#define TNM_SNMP_AUTH_MD5	0x01
#define TNM_SNMP_AUTH_SHA	0x02
#define TNM_SNMP_AUTH_MASK	0x0f

#define TNM_SNMP_PRIV_NONE	0x00
#define TNM_SNMP_PRIV_DES	0x10
#define TNM_SNMP_PRIV_MASK	0xf0

extern TnmTable tnmSnmpSecurityLevelTable[];
#endif

/*
 *----------------------------------------------------------------
 * SNMP sockets can be shared between multiple manager or agent
 * sessions. The TnmSnmpSocket data type adds a reference count
 * to a real system socket so that we can close the system socket
 * if it is not used anymore.
 *----------------------------------------------------------------
 */

#define TNM_SNMP_UDP_DOMAIN	0x01
#define TNM_SNMP_TCP_DOMAIN	0x02

extern TnmTable tnmSnmpDomainTable[];

typedef struct TnmSnmpSocket {
    int sock;				/* socket descriptor */
    char domain;			/* transport domain */
    struct sockaddr *name;		/* local name (if any) */
    struct sockaddr *peername;		/* peer name (if any) */
    int flags;				/* special flags (if any) */
    int refCount;			/* reference count */
    struct TnmSnmpSocket *nextPtr;	/* pointer to next socket */
} TnmSnmpSocket;

EXTERN TnmSnmpSocket *tnmSnmpSocketList;

TnmSnmpSocket*
TnmSnmpOpen		_ANSI_ARGS_((Tcl_Interp *interp, 
				     struct sockaddr_in *addr));
void
TnmSnmpClose		_ANSI_ARGS_((TnmSnmpSocket *sockPtr));

/*
 *----------------------------------------------------------------
 * The types of SNMP applications as defined in RFC 2271.
 *----------------------------------------------------------------
 */

typedef enum {
    TNM_SNMP_GENERATOR	= 1,
    TNM_SNMP_RESPONDER	= 2,
    TNM_SNMP_NOTIFIER	= 3,
    TNM_SNMP_LISTENER	= 4
} TnmSnmpAppl;

extern TnmTable tnmSnmpApplTable[];

/*
 *----------------------------------------------------------------
 * The TnmSnmp structure contains all infomation needed to handle
 * SNMP requests and responses between an agent and a manager.
 *----------------------------------------------------------------
 */

typedef struct TnmSnmp {
    struct sockaddr_in maddr;     /* The manager destination address. */
    char version;                 /* The SNMP version used by this session. */
    TnmSnmpAppl type;		  /* The type of this session (see above). */
    char domain;		  /* The transport domain (see above). */
    Tcl_Obj *community;           /* The community string for this session. */
    TnmOid enterpriseOid;	  /* The enterprise OID for SNMPv1 traps. */
    Tcl_Obj *context;		  /* The context name used by this session. */
    Tcl_Obj *user;		  /* The user name used by this session. */
    Tcl_Obj *engineID;		  /* The engine ID used by this session. */
    int engineBoots;		  /* The number of boots for this engine. */
    int engineTime;		  /* Time since last boot of this engine. */
    int maxSize;		  /* The maximum message size. */
    Tcl_Obj *authPassWord;	  /* The password to compute the authKey. */
    Tcl_Obj *privPassWord;	  /* The password to compute the privKey. */
    Tcl_Obj *usmAuthKey;	  /* The USM authentication key. */
    Tcl_Obj *usmPrivKey;	  /* The USM privacy key. */
    char securityLevel;		  /* The security level. */
#ifdef TNM_SNMPv2U
    u_char qos;
    u_char agentID[USEC_MAX_AGENTID];
    u_int agentBoots;
    u_int agentTime;
    u_int latestReceivedAgentTime;
    char *password;
    u_char authKey[TNM_MD5_SIZE];
    u_char privKey[TNM_MD5_SIZE];
#endif
    int retries;                  /* Number of retries until we give up. */
    int timeout;                  /* Milliseconds before we timeout. */
    int window;                   /* Max. number of active async. requests. */
    int delay;                    /* Minimum delay between requests. */
    int active;                   /* Number of active async. requests. */
    int waiting;                  /* Number of waiting async. requests. */
    Tcl_Obj *tagList;		  /* The tags associated with this session. */
    struct TnmSnmpBinding *bindPtr; /* Commands bound to this session. */
    Tcl_Interp *interp;		  /* Tcl interpreter owning this session. */
    Tcl_Command token;		  /* The command token used by Tcl. */
    TnmConfig *config;		  /* Option description for this session. */
    TnmSnmpSocket *socket;	  /* */
    struct TnmSnmpNode *instPtr;  /* Root of the tree of MIB instances. */
    struct TnmSnmp *nextPtr;	  /* Pointer to next session. */
#ifdef TNM_SNMP_BENCH
    TnmSnmpMark stats;		  /* Statistics for the last SNMP operation. */
#endif
} TnmSnmp;

/*
 *----------------------------------------------------------------
 * This global variable points to a list of all sessions.
 *----------------------------------------------------------------
 */

EXTERN TnmSnmp *tnmSnmpList;

/*
 *----------------------------------------------------------------
 * The following function is used to normalize the Tcl 
 * represenation of SNMP varbind lists.
 *----------------------------------------------------------------
 */

#define TNM_SNMP_NORM_OID	0x01
#define TNM_SNMP_NORM_INT	0x02
#define TNM_SNMP_NORM_BITS	0x04

EXTERN Tcl_Obj*
TnmSnmpNorm	_ANSI_ARGS_((Tcl_Interp *interp, Tcl_Obj *objPtr, 
			     int flags));
/*
 *----------------------------------------------------------------
 * Structure to hold a varbind. Lists of varbinds are mapped to 
 * arrays of varbinds as this simplifies memory management.
 *----------------------------------------------------------------
 */

typedef struct SNMP_VarBind {
    char *soid;
    char *syntax;
    char *value;
    char *freePtr;
    ClientData clientData;
    int flags;
} SNMP_VarBind;

EXTERN void
Tnm_SnmpFreeVBList	_ANSI_ARGS_((int varBindSize, 
				     SNMP_VarBind *varBindPtr));
EXTERN int
Tnm_SnmpSplitVBList	_ANSI_ARGS_((Tcl_Interp *interp, char *list,
				     int *varBindSizePtr, 
				     SNMP_VarBind **varBindPtrPtr));
EXTERN char*
Tnm_SnmpMergeVBList	_ANSI_ARGS_((int varBindSize, 
				     SNMP_VarBind *varBindPtr));

/*
 *----------------------------------------------------------------
 * Structure to describe a SNMP PDU.
 *----------------------------------------------------------------
 */

typedef struct TnmSnmpPdu {
    struct sockaddr_in addr;	/* The address from/to it is sent.     */
    int type;			/* The type of this PDU.               */
    int requestId;		/* A unique request id for this PDU.   */
    int errorStatus;		/* The SNMP error status field.        */
    int errorIndex;		/* The SNMP error index field.         */
    char *trapOID;		/* Trap object identifier.             */
#ifdef TNM_SNMPv3
    int contextLength;
    char *context;
    int engineIDLength;
    char *engineID;
#endif
#if 1
    Tcl_Obj *vbList;		/* The list of varbinds as a Tcl_Obj.  */
#endif
    Tcl_DString varbind;	/* The list of varbinds as Tcl string. */
} TnmSnmpPdu;

/*
 *----------------------------------------------------------------
 * Structure to describe an asynchronous request.
 *----------------------------------------------------------------
 */

typedef void (TnmSnmpRequestProc)	_ANSI_ARGS_((TnmSnmp *session,
		TnmSnmpPdu *pdu, ClientData clientData));

typedef struct TnmSnmpRequest {
    int id;                          /* The unique request identifier. */
    int sends;                       /* Number of send operations. */
    u_char *packet;                  /* The encoded SNMP message. */
    int packetlen;		     /* The length of the encoded message. */
    Tcl_TimerToken timer;	     /* Token used by Tcl Timer Handler. */
    TnmSnmp *session;		     /* The SNMP session for this request. */
    TnmSnmpRequestProc *proc;        /* The callback functions. */
    ClientData clientData;           /* The argument of the callback. */
    struct TnmSnmpRequest *nextPtr;  /* Pointer to next pending request. */
#ifdef TNM_SNMP_BENCH
    TnmSnmpMark stats;              /* Statistics for this SNMP operation. */
#endif
    Tcl_Interp *interp;		     /* The interpreter needed internally. */
} TnmSnmpRequest;

EXTERN TnmSnmpRequest*
TnmSnmpCreateRequest	_ANSI_ARGS_((int id, u_char *packet, int packetlen,
				     TnmSnmpRequestProc *proc,
				     ClientData clientData,
				     Tcl_Interp *interp));
EXTERN TnmSnmpRequest*
TnmSnmpFindRequest	_ANSI_ARGS_((int id));

EXTERN int
TnmSnmpQueueRequest	_ANSI_ARGS_((TnmSnmp *session, 
				     TnmSnmpRequest *request));
EXTERN void
TnmSnmpDeleteRequest	_ANSI_ARGS_((TnmSnmpRequest *request));

EXTERN int
TnmSnmpGetRequestId	_ANSI_ARGS_((void));

/*
 *----------------------------------------------------------------
 * The event types currently supported for SNMP bindings.
 *----------------------------------------------------------------
 */

#define TNM_SNMP_NO_EVENT	0x0000
#define TNM_SNMP_GET_EVENT	0x0001
#define TNM_SNMP_SET_EVENT	0x0002
#define TNM_SNMP_CREATE_EVENT	0x0004
#define TNM_SNMP_TRAP_EVENT	0x0008
#define TNM_SNMP_INFORM_EVENT	0x0010
#define TNM_SNMP_CHECK_EVENT	0x0020
#define TNM_SNMP_COMMIT_EVENT	0x0040
#define TNM_SNMP_ROLLBACK_EVENT	0x0080
#define TNM_SNMP_BEGIN_EVENT	0x0100
#define TNM_SNMP_END_EVENT	0x0200
#define TNM_SNMP_SEND_EVENT	0x0400
#define TNM_SNMP_RECV_EVENT	0x0800

#define TNM_SNMP_GENERIC_BINDINGS \
(TNM_SNMP_TRAP_EVENT | TNM_SNMP_INFORM_EVENT | \
TNM_SNMP_BEGIN_EVENT | TNM_SNMP_END_EVENT |\
TNM_SNMP_RECV_EVENT | TNM_SNMP_SEND_EVENT)

#define TNM_SNMP_INSTANCE_BINDINGS \
(TNM_SNMP_GET_EVENT | TNM_SNMP_SET_EVENT | TNM_SNMP_CREATE_EVENT |\
TNM_SNMP_CHECK_EVENT | TNM_SNMP_COMMIT_EVENT | TNM_SNMP_ROLLBACK_EVENT)

EXTERN TnmTable tnmSnmpEventTable[];

/*
 *----------------------------------------------------------------
 * A structure to describe a binding. Multiple bindings are 
 * organized in a simple list as it is not too likely to have 
 * many bindings for an object.
 *----------------------------------------------------------------
 */

typedef struct TnmSnmpBinding {
    int event;				/* Event that triggers binding. */
    char *command;			/* Tcl command to evaluate.     */
    struct TnmSnmpBinding *nextPtr;	/* Next binding in our list.    */
} TnmSnmpBinding;


EXTERN int
TnmSnmpEvalBinding	_ANSI_ARGS_((Tcl_Interp *interp, TnmSnmp *session,
                                     TnmSnmpPdu *pdu, int event));

/*
 *----------------------------------------------------------------
 * Structure to describe a MIB node known by a session handle.
 * MIB nodes are either used to keep information about session 
 * bindings or to store data needed to process incoming SNMP 
 * requests in the agent role.
 *----------------------------------------------------------------
 */

typedef struct TnmSnmpNode {
    char *label;			/* The complete OID.	    */
    int offset;				/* Offset to instance id.   */
    int syntax;				/* Syntax string from MIB.  */
    int	access;				/* Access mode from MIB.    */
    char *tclVarName;			/* Tcl variable name.	    */
    TnmSnmpBinding *bindings;		/* List of bindings.        */ 
    u_int subid;			/* Sub identifier in Tree.  */
    struct TnmSnmpNode *childPtr;       /* List of child nodes.	    */
    struct TnmSnmpNode *nextPtr;	/* List of peer node.	    */
} TnmSnmpNode;

EXTERN int
TnmSnmpCreateNode	_ANSI_ARGS_((Tcl_Interp *interp, char *id,
				     char *varName, char *defval));
EXTERN TnmSnmpNode*
TnmSnmpFindNode		_ANSI_ARGS_((TnmSnmp *session, TnmOid *oidPtr));

EXTERN TnmSnmpNode*
TnmSnmpFindNextNode	_ANSI_ARGS_((TnmSnmp *session, TnmOid *oidPtr));

EXTERN int
TnmSnmpSetNodeBinding	_ANSI_ARGS_((TnmSnmp *session, TnmOid *oidPtr,
				     int event, char *command));
EXTERN char*
TnmSnmpGetNodeBinding	_ANSI_ARGS_((TnmSnmp *session, TnmOid *oidPtr,
				     int event));
EXTERN int
TnmSnmpEvalNodeBinding	_ANSI_ARGS_((TnmSnmp *session,
				     TnmSnmpPdu *pdu, TnmSnmpNode *inst,
				     int operation, char *value,
				     char *oldValue));

/*
 *----------------------------------------------------------------
 * Structure to collect SNMP related statistics. See RFC 1213
 * and RFC 1450 for more details when these counters must be
 * incremented.
 *----------------------------------------------------------------
 */

typedef struct TnmSnmpStats {
    /* RFC 1213 */
    u_int snmpInPkts;
    u_int snmpOutPkts;
    u_int snmpInBadVersions;
    u_int snmpInBadCommunityNames;
    u_int snmpInBadCommunityUses;
    u_int snmpInASNParseErrs;
    u_int snmpInTooBigs;
    u_int snmpInNoSuchNames;
    u_int snmpInBadValues;
    u_int snmpInReadOnlys;
    u_int snmpInGenErrs;
    u_int snmpInTotalReqVars;
    u_int snmpInTotalSetVars;
    u_int snmpInGetRequests;
    u_int snmpInGetNexts;
    u_int snmpInSetRequests;
    u_int snmpInGetResponses;
    u_int snmpInTraps;
    u_int snmpOutTooBigs;
    u_int snmpOutNoSuchNames;
    u_int snmpOutBadValues;
    u_int snmpOutGenErrs;
    u_int snmpOutGetRequests;
    u_int snmpOutGetNexts;
    u_int snmpOutSetRequests;
    u_int snmpOutGetResponses;
    u_int snmpOutTraps;
    u_int snmpEnableAuthenTraps;
    /* RFC 1450 XXX not yet implemented */
    u_int snmpStatsPackets;
    u_int snmpStats30Something;
    u_int snmpStatsEncodingErrors;
    u_int snmpStatsUnknownDstParties;
    u_int snmpStatsDstPartyMismatches;
    u_int snmpStatsUnknownSrcParties;
    u_int snmpStatsBadAuths;
    u_int snmpStatsNotInLifetimes;
    u_int snmpStatsWrongDigestValues;
    u_int snmpStatsUnknownContexts;
    u_int snmpStatsBadOperations;
    u_int snmpStatsSilentDrops;
    /* snmpV1BadCommunityNames is the same as snmpInBadCommunityNames */
    /* snmpV1BadCommunityUses  is the same as snmpInBadCommunityUses  */
#ifdef TNM_SNMPv2U
    u_int usecStatsUnsupportedQoS;
    u_int usecStatsNotInWindows;
    u_int usecStatsUnknownUserNames;
    u_int usecStatsWrongDigestValues;
    u_int usecStatsUnknownContexts;
    u_int usecStatsBadParameters;
    u_int usecStatsUnauthorizedOperations;
#endif
} TnmSnmpStats;

/*
 *----------------------------------------------------------------
 * Global variable used to collect the SNMP statistics.
 *----------------------------------------------------------------
 */

EXTERN TnmSnmpStats tnmSnmpStats;

/*
 *----------------------------------------------------------------
 * Exported SNMP procedures:
 *----------------------------------------------------------------
 */

EXTERN TnmSnmp*
TnmSnmpCreateSession	_ANSI_ARGS_((Tcl_Interp *interp, char type));

EXTERN void
TnmSnmpDeleteSession	_ANSI_ARGS_((TnmSnmp *session));

EXTERN int
TnmSnmpEncode		_ANSI_ARGS_((Tcl_Interp *interp, TnmSnmp *session,
				     TnmSnmpPdu *pdu, TnmSnmpRequestProc *proc,
				     ClientData clientData));
EXTERN int
TnmSnmpDecode		_ANSI_ARGS_((Tcl_Interp *interp, 
				     u_char *packet, int packetlen,
				     struct sockaddr_in *from,
				     TnmSnmp *session, int *reqid,
				     int *status, int *index));
EXTERN void
TnmSnmpTimeoutProc	_ANSI_ARGS_((ClientData clientData));

EXTERN int
TnmSnmpAgentInit	_ANSI_ARGS_((Tcl_Interp *interp, 
				     TnmSnmp *session));
EXTERN int
TnmSnmpAgentRequest	_ANSI_ARGS_((Tcl_Interp *interp, TnmSnmp *session,
				     TnmSnmpPdu *pdu));
EXTERN int
TnmSnmpEvalCallback	_ANSI_ARGS_((Tcl_Interp *interp, TnmSnmp *session,
				     TnmSnmpPdu *pdu,
				     char *cmd, char *instance, char *oid, 
				     char *value, char* oldValue));

/*
 *----------------------------------------------------------------
 * The following function is used to create a socket used for
 * all manager initiated communication. The Close function
 * is used to close this socket if all SNMP sessions have been
 * destroyed.
 *----------------------------------------------------------------
 */

EXTERN int
TnmSnmpManagerOpen	_ANSI_ARGS_((Tcl_Interp	*interp));

EXTERN void
TnmSnmpManagerClose	_ANSI_ARGS_((void));

/*
 *----------------------------------------------------------------
 * Create and close a socket used for notification listener
 * sessions. This socket is used to process notifications
 * received from other notification originators. The default
 * port for incoming notifications is 162, which requires
 * special privileges on the Unix operating system. We use the
 * nmtrapd forwarder on Unix to open this port indirectly.
 *----------------------------------------------------------------
 */

EXTERN int
TnmSnmpListenerOpen	_ANSI_ARGS_((Tcl_Interp *interp,
				     TnmSnmp *session));

EXTERN void
TnmSnmpListenerClose	_ANSI_ARGS_((TnmSnmp *session));

/*
 *----------------------------------------------------------------
 * Create and close a socket used for command responder sessions.
 * This socket is used to process commands received from other
 * command generators.
 *----------------------------------------------------------------
 */

EXTERN int
TnmSnmpResponderOpen	_ANSI_ARGS_((Tcl_Interp *interp, 
				     TnmSnmp *session));

EXTERN void
TnmSnmpResponderClose	_ANSI_ARGS_((TnmSnmp *session));

/*
 *----------------------------------------------------------------
 * Create and close a socket used for notification listener
 * sessions on Unix operating systems where we receive these
 * traps indirectly via the nmtrapd daemon.
 *----------------------------------------------------------------
 */

EXTERN int
TnmSnmpNmtrapdOpen	_ANSI_ARGS_((Tcl_Interp *interp));

EXTERN void
TnmSnmpNmtrapdClose	_ANSI_ARGS_((void));

/*
 *----------------------------------------------------------------
 * Functions used to send and receive SNMP messages. The 
 * TnmSnmpWait function is used to wait for an answer.
 *----------------------------------------------------------------
 */

#define TNM_SNMP_SYNC	0x01
#define TNM_SNMP_ASYNC	0x02

EXTERN int
TnmSnmpSend		_ANSI_ARGS_((Tcl_Interp *interp,
				     TnmSnmp *session,
				     u_char *packet, int packetlen,
				     struct sockaddr_in *to, int flags));
EXTERN int
TnmSnmpRecv		_ANSI_ARGS_((Tcl_Interp *interp, 
				     u_char *packet, int *packetlen,
				     struct sockaddr_in *from, int flags));
EXTERN int 
TnmSnmpWait		_ANSI_ARGS_((int ms, int flags));

EXTERN void
TnmSnmpDelay		_ANSI_ARGS_((TnmSnmp *session));

/*
 *----------------------------------------------------------------
 * Some more utility and conversion functions.
 *----------------------------------------------------------------
 */

EXTERN int
TnmSnmpSysUpTime	_ANSI_ARGS_((void));

EXTERN void
TnmSnmpMD5Digest	_ANSI_ARGS_((u_char *packet, int packetlen,
				     u_char *key, u_char *digest));

#ifdef TNM_SNMPv3
EXTERN void
TnmSnmpComputeKeys	_ANSI_ARGS_((TnmSnmp *session));

EXTERN void
TnmSnmpComputeDigest	_ANSI_ARGS_(());

EXTERN void
TnmSnmpAuthOutMsg	_ANSI_ARGS_((int algorithm, Tcl_Obj *authKey,
				     u_char *msg, int msgLen,
				     u_char *msgAuthenticationParameters));
#endif

#ifdef TNM_SNMPv2U
EXTERN void
TnmSnmpUsecSetAgentID	_ANSI_ARGS_((TnmSnmp *session));

EXTERN void
TnmSnmpUsecGetAgentID	_ANSI_ARGS_((TnmSnmp *session));

EXTERN void
TnmSnmpUsecAuth	_ANSI_ARGS_((TnmSnmp *session, 
				     u_char *packet, int packetlen));
#endif

EXTERN void
TnmSnmpDumpPacket	_ANSI_ARGS_((u_char *packet, int len,
				     struct sockaddr_in *from,
				     struct sockaddr_in *to));
EXTERN void
TnmSnmpDumpPDU		_ANSI_ARGS_((Tcl_Interp *interp, TnmSnmpPdu *pdu));

#endif /* _TNMSNMP */
