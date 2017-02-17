/*
 * tnmInt.h --
 *
 *	Declarations of things used internally by the Tnm extension.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2001 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TNMINT
#define _TNMINT

#include <stdio.h>

#ifndef _TNM
#include "tnm.h"
#endif

#ifndef _TNMPORT
#include "tnmPort.h"
#endif

#ifndef HAVE_SOCKLEN_T
typedef size_t socklen_t;
#endif

/*
 *----------------------------------------------------------------
 * The following define should only be used when compiling a
 * demo/alpha/beta version to make sure that such a binary is
 * not used forever.
 *----------------------------------------------------------------
 */

#define TNM_EXPIRE_TIME	883612800
#undef TNM_EXPIRE_TIME

/*
 *----------------------------------------------------------------
 * The ckstrdup macro is useful for two reasons: It emulates
 * strdup which is missing on some systems and it allows to track
 * memory allocation problems using Tcl's memory debugger.
 *----------------------------------------------------------------
 */

#define ckstrdup(s)	strcpy(ckalloc(strlen(s)+1), s)

/*
 *----------------------------------------------------------------
 * The following functions are not officially exported by Tcl. 
 * They are used anyway because they make this code simpler and 
 * platform independent.
 *----------------------------------------------------------------
 */

EXTERN int
TnmMkDir		(Tcl_Interp *interp, Tcl_Obj *pathname);

/*
 *----------------------------------------------------------------
 * These functions provide the platform independent initialization
 * points. They are called from platform specific dynamic link
 * entry points.
 *----------------------------------------------------------------
 */

EXTERN int
TnmInit			(Tcl_Interp *interp, int safe);

EXTERN void
TnmInitPath		(Tcl_Interp *interp);

EXTERN void
TnmInitDns		(Tcl_Interp *interp);

/*
 *----------------------------------------------------------------
 * The following structure describes simple tables that map 
 * integer keys to string values and vice versa. This is usually 
 * used to map error codes or similar tokens to strings.
 *----------------------------------------------------------------
 */

typedef struct TnmTable {
    unsigned key;
    char *value;
} TnmTable;

EXTERN char*
TnmGetTableValue	(TnmTable *table, unsigned key);

EXTERN int
TnmGetTableKey		(TnmTable *table, const char *value);

EXTERN char*
TnmGetTableValues	(TnmTable *table);

EXTERN void
TnmListFromTable	(TnmTable *table, Tcl_Obj *listPtr,
				     char *pattern);
EXTERN int
TnmGetTableKeyFromObj	(Tcl_Interp *interp, TnmTable *table,
				     Tcl_Obj *objPtr, char *what);
EXTERN void
TnmListFromList		(Tcl_Obj *objPtr, Tcl_Obj *listPtr,
				     char *pattern);
	
/*
 *----------------------------------------------------------------
 * A simple mechanism to handle object configuration options.
 * This is not as flexible as the Tk option handling mechanism
 * but it fits our needs.
 *----------------------------------------------------------------
 */

typedef Tcl_Obj* (TnmGetConfigProc) (Tcl_Interp *interp,
        ClientData clientData, int option);
typedef int (TnmSetConfigProc) (Tcl_Interp *interp,
        ClientData clientData, int option, Tcl_Obj *objPtr);

typedef struct TnmConfig {
    TnmTable *optionTable;
    TnmSetConfigProc *setOption;
    TnmGetConfigProc *getOption;
} TnmConfig;

EXTERN int
TnmSetConfig		(Tcl_Interp *interp, TnmConfig *config,
				     ClientData obj, int objc, 
				     Tcl_Obj *CONST objv[]);
EXTERN int
TnmGetConfig		(Tcl_Interp *interp, TnmConfig *config,
				     ClientData obj, int objc, 
				     Tcl_Obj *CONST objv[]);
/*
 *----------------------------------------------------------------
 * The following typedefs and functions are used to implement
 * the Tnm Tcl_Obj types.
 *----------------------------------------------------------------
 */

#if (SIZEOF_UNSIGNED_LONG_LONG == 8)
typedef unsigned long long TnmUnsigned64;
#elif (SIZEOF_UNSIGNED_LONG == 8)
typedef unsigned long TnmUnsigned64;
#elif defined (WIN32)
/* for win32 we use integer32 instead of integer64 (should be fixed): */
typedef unsigned long TnmUnsigned64;
#else
#error "need to port the Tnm unsigned 64 data type to this platform"
#endif

#if (SIZEOF_UNSIGNED_LONG == 4)
typedef unsigned long TnmUnsigned32;
#elif (SIZEOF_UNSIGNED_INT == 4)
typedef unsigned int TnmUnsigned32;
#else
#error "need to port the Tnm unsigned 32 data type to this platform"
#endif

#if (SIZEOF_LONG == 4)
typedef unsigned long TnmInteger32;
#elif (SIZEOF_INT == 4)
typedef unsigned int TnmInteger32;
#else
#error "need to port the Tnm integer 32 data type to this platform"
#endif

EXTERN Tcl_ObjType tnmUnsigned64Type;

EXTERN Tcl_Obj*
TnmNewUnsigned64Obj	(TnmUnsigned64 u);

EXTERN void
TnmSetUnsigned64Obj	(Tcl_Obj *objPtr, TnmUnsigned64 u);

EXTERN int
TnmGetUnsigned64FromObj	(Tcl_Interp *interp, Tcl_Obj *objPtr,
				     TnmUnsigned64 *uPtr);

EXTERN Tcl_ObjType tnmUnsigned32Type;

EXTERN Tcl_Obj*
TnmNewUnsigned32Obj	(TnmUnsigned32 u);

EXTERN void
TnmSetUnsigned32Obj	(Tcl_Obj *objPtr, TnmUnsigned32 u);

EXTERN int
TnmGetUnsigned32FromObj	(Tcl_Interp *interp, Tcl_Obj *objPtr,
				     TnmUnsigned32 *uPtr);

EXTERN Tcl_ObjType tnmInteger32Type;

EXTERN Tcl_Obj*
TnmNewInteger32Obj	(TnmInteger32 i);

EXTERN void
TnmSetInteger32Obj	(Tcl_Obj *objPtr, TnmInteger32 i);

EXTERN int
TnmGetInteger32FromObj	(Tcl_Interp *interp, Tcl_Obj *objPtr,
				     TnmInteger32 *iPtr);

EXTERN Tcl_ObjType tnmOctetStringType;

EXTERN Tcl_Obj*
TnmNewOctetStringObj	(char *bytes, int length);

EXTERN void
TnmSetOctetStringObj	(Tcl_Obj *objPtr, char *bytes,
				     int length);
EXTERN char*
TnmGetOctetStringFromObj (Tcl_Interp *interp, Tcl_Obj *objPtr,
				     int *lengthPtr);

EXTERN Tcl_ObjType tnmIpAddressType;

EXTERN Tcl_Obj*
TnmNewIpAddressObj	(struct in_addr *ipaddr);

EXTERN void
TnmSetIpAddressObj	(Tcl_Obj *objPtr, struct in_addr *ipaddr);

EXTERN struct in_addr*
TnmGetIpAddressFromObj	(Tcl_Interp *interp, Tcl_Obj *objPtr);

/*
 *----------------------------------------------------------------
 * The following structure describes simple vector to hold 
 * ClientData arguments. This is usually used to keep references
 * to other objects.
 *----------------------------------------------------------------
 */

#define TNM_VECTOR_STATIC_SIZE 8
typedef struct TnmVector {
    ClientData *elements;
    int size;
    int spaceAvl;
    ClientData staticSpace[TNM_VECTOR_STATIC_SIZE + 1];
} TnmVector;

EXTERN void
TnmVectorInit		(TnmVector *vPtr);

EXTERN void
TnmVectorFree		(TnmVector *vPtr);

EXTERN void
TnmVectorAdd		(TnmVector *vPtr, ClientData clientData);

EXTERN void
TnmVectorDelete		(TnmVector *vPtr, ClientData clientData);

#define TnmVectorGet(vPtr,index)	((vPtr)->elements[index])
#define TnmVectorSet(vPtr,index,value)	((vPtr)->elements[index] = value)
#define TnmVectorSize(vPtr)		((vPtr)->size)
#define TnmVectorElements(vPtr)		((vPtr)->elements)

/*
 *----------------------------------------------------------------
 * Functions to convert common string arguments into an
 * internal format with additional error checks.
 *----------------------------------------------------------------
 */

EXTERN int 
TnmGetUnsigned		(Tcl_Interp *interp, char *string,
				     int *intPtr);
EXTERN int 
TnmGetUnsignedFromObj	(Tcl_Interp *interp, Tcl_Obj *objPtr,
				     int *intPtr);
EXTERN int 
TnmGetPositive		(Tcl_Interp *interp, char *string,
				     int *intPtr);
EXTERN int 
TnmGetPositiveFromObj	(Tcl_Interp *interp, Tcl_Obj *objPtr,
				     int *intPtr);
EXTERN int
TnmGetIntRangeFromObj	(Tcl_Interp *interp, Tcl_Obj *objPtr,
				     int min, int max, int *intPtr);
EXTERN int
TnmSetIPAddress		(Tcl_Interp *interp, char *name,
				     struct sockaddr_in *addr);
EXTERN char*
TnmGetIPName		(Tcl_Interp *interp,
				     struct sockaddr_in *addr);
EXTERN int
TnmSetIPPort		(Tcl_Interp *interp, char *protocol,
				     char *port, struct sockaddr_in *addr);
EXTERN char*
TnmGetIPPort		(Tcl_Interp *interp, char *protocol,
				     struct sockaddr_in *addr);
EXTERN int
TnmValidateIpHostName	(Tcl_Interp *interp, const char *name);

EXTERN int
TnmValidateIpAddress	(Tcl_Interp *interp, const char *address);

/*
 *----------------------------------------------------------------
 * The following defines are taken from the UNIX syslog facility.
 * Not all of them may be supported on all platforms. These
 * definitions follow RFC 3164 which is now the official
 * definition of the BSD syslog protocol.
 *----------------------------------------------------------------
 */

#define	TNM_LOG_EMERG	0	/* system is unusable */
#define	TNM_LOG_ALERT	1	/* action must be taken immediately */
#define	TNM_LOG_CRIT	2	/* critical conditions */
#define	TNM_LOG_ERR	3	/* error conditions */
#define	TNM_LOG_WARNING	4	/* warning conditions */
#define	TNM_LOG_NOTICE	5	/* normal but significant condition */
#define	TNM_LOG_INFO	6	/* informational */
#define	TNM_LOG_DEBUG	7	/* debug-level messages */

#define TNM_LOG_KERN	0	/* kernel messages */
#define TNM_LOG_USER	1	/* random user process messages */
#define TNM_LOG_MAIL	2	/* mail subsystem messages */
#define TNM_LOG_DAEMON	3	/* system daemon messages */
#define TNM_LOG_AUTH	4	/* security/authorization messages */
#define TNM_LOG_SYSLOG	5	/* syslog internal messages */
#define TNM_LOG_LPR	6	/* printing subsystem messages */
#define TNM_LOG_NEWS	7	/* network news subsystem messages */
#define TNM_LOG_UUCP	8	/* UUCP subsystem messages */
#define TNM_LOG_CRON	9	/* clock (cron/at) subsystem messages */
#define TNM_LOG_AUTHPRIV 10	/* security/authorization messages */
#define TNM_LOG_FTP	11	/* FTP daemon messages */
#define TNM_LOG_NTP	12	/* NTP daemon messages */
#define TNM_LOG_AUDITPRIV 13	/* auditing subsystem messages */
#define TNM_LOG_ALERTPRIV 14	/* alert subsystem messages */
#define TNM_LOG_CRONPRIV 15	/* clock (cron/at) daemon messages */

#define TNM_LOG_LOCAL0	16	/* local use 0 */
#define TNM_LOG_LOCAL1	17	/* local use 1 */
#define TNM_LOG_LOCAL2	18	/* local use 2 */
#define TNM_LOG_LOCAL3	19	/* local use 3 */
#define TNM_LOG_LOCAL4	20	/* local use 4 */
#define TNM_LOG_LOCAL5	21	/* local use 5 */
#define TNM_LOG_LOCAL6	22	/* local use 6 */
#define TNM_LOG_LOCAL7	23	/* local use 7 */

EXTERN int 
TnmWriteLogMessage	(char *ident, int level, int facility,
				     char *message);
EXTERN void
TnmWriteMessage		(CONST84 char *msg);

/*
 *----------------------------------------------------------------
 * The following defines are used to distinguish the ICMP packet
 * types supported by the icmp command.
 *----------------------------------------------------------------
 */

typedef struct TnmIcmpTarget {
    unsigned int tid;		/* The unique identifier for this target. */
    struct in_addr dst;		/* The address of the ICMP target. */
    struct in_addr res;		/* The address contained in the response. */
    union {
	unsigned rtt;		/* The round trip time in ms. */
	int tdiff;		/* The time stamp difference. */
	int mask;		/* The address mask. */
    } u;
    u_char status;		/* The status of this entry (see below). */
    u_char flags;		/* Some flags (see below). */
} TnmIcmpTarget;

#define TNM_ICMP_TYPE_ECHO		0x01
#define TNM_ICMP_TYPE_MASK		0x02
#define TNM_ICMP_TYPE_TIMESTAMP		0x03
#define TNM_ICMP_TYPE_TRACE		0x04

#define TNM_ICMP_STATUS_NOERROR		0x00
#define TNM_ICMP_STATUS_TIMEOUT		0x01
#define TNM_ICMP_STATUS_GENERROR	0x02

#define TNM_ICMP_FLAG_LASTHOP		0x01

typedef struct TnmIcmpRequest {
    int type;			/* The ICMP request type (see above). */
    int ttl;			/* The time-to-live value for this request. */
    int timeout;		/* The timeout value (ms) for this request. */
    int retries;		/* The retry value for this request. */
    int delay;			/* The delay value (ms) for this request. */
    int size;			/* The size of the ICMP packet. */
    int window;			/* The window size for this request. */
    int flags;			/* The flags for this particular request. */
    int numTargets;		/* The number of targets for this request. */
    TnmIcmpTarget *targets;	/* The vector of targets. */
    struct TnmIcmpRequest *nextPtr;	/* Next queued request. */
} TnmIcmpRequest;

EXTERN int
TnmIcmp			(Tcl_Interp *interp, 
				     TnmIcmpRequest *icmpPtr);

/*
 *----------------------------------------------------------------
 * The following functions are used internally to implement
 * attributes for objects.
 *----------------------------------------------------------------
 */

EXTERN int
TnmAttrSet	(Tcl_HashTable *tablePtr, Tcl_Interp *interp,
			     char *name, char *value);
EXTERN void
TnmAttrList	(Tcl_HashTable *tablePtr, Tcl_Interp *interp);

EXTERN void
TnmAttrClear	(Tcl_HashTable *tablePtr);

EXTERN void
TnmAttrDump	(Tcl_HashTable *tablePtr, char *name, 
			     Tcl_DString *dsPtr);

/*
 *----------------------------------------------------------------
 * The following functions are used internally to encode/decode
 * internal data into/from various formats.
 *----------------------------------------------------------------
 */

EXTERN void
TnmHexEnc	(char *s, int n, char *d);

EXTERN int
TnmHexDec	(char *s, char *d, int *n);

/*
 *----------------------------------------------------------------
 * Functions to simplify the handling of object-based Tcl
 * commands.
 *----------------------------------------------------------------
 */

EXTERN char*
TnmGetHandle	(Tcl_Interp *interp, char *prefix,
			     unsigned *id);
EXTERN int
TnmMatchTags	(Tcl_Interp *interp, Tcl_Obj *tagListObj, 
			     Tcl_Obj *patternListObj);
/*
 *----------------------------------------------------------------
 * The following functions are used internally to access sockets
 * in a platform independent way. These functions return the value
 * TNM_SOCKET_ERROR as an error result and set the errno variable
 * in a way that allows to use Tcl_PosixError().
 *----------------------------------------------------------------
 */

#define TNM_SOCKET_ERROR -1

EXTERN int
TnmSocket		(int domain, int type, int protocol);

EXTERN int
TnmSocketBind		(int s, struct sockaddr *name,
				     socklen_t namelen);
EXTERN int
TnmSocketSendTo		(int s, char *buf, size_t len, int flags,
				     struct sockaddr *to, socklen_t tolen);
EXTERN int
TnmSocketRecvFrom	(int s, char *buf, size_t len, int flags,
				     struct sockaddr *from, socklen_t *fromlen);
EXTERN int
TnmSocketClose		(int s);

typedef void (TnmSocketProc) (ClientData clientData, int mask);

EXTERN void
TnmCreateSocketHandler	(int sock, int mask,
				     TnmSocketProc *proc,
				     ClientData clientData);
EXTERN void
TnmDeleteSocketHandler	(int sock);

/*
 *----------------------------------------------------------------
 * The following functions are used to implement the SMX support.
 *----------------------------------------------------------------
 */

EXTERN int
TnmSmxInit		(Tcl_Interp *interp);

#endif /* _TNMINT */
