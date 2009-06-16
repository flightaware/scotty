/*
 * tnmAsn1.h --
 *
 *	Definitions for the ASN.1/BER encoder/decoder for the SNMP
 *	protocol. This is only the subset of ASN.1/BER required by
 *	the SNMP protocol.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente. 
 * Copyright (c) 1997-2002 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TNMASN1
#define _TNMASN1

/*
 *----------------------------------------------------------------
 * Definition of ASN.1 data types used by SNMP.
 *----------------------------------------------------------------
 */

#define ASN1_UNIVERSAL		( 0x00 )
#define ASN1_APPLICATION	( 0x40 )
#define ASN1_CONTEXT		( 0x80 )
#define ASN1_PRIVATE		( 0xc0 )

#define ASN1_PRIMITIVE		( 0x00 )
#define ASN1_CONSTRUCTED	( 0x20 )

#define	ASN1_OTHER		( ASN1_UNIVERSAL | ASN1_PRIMITIVE   | 0x00 )
#define	ASN1_INTEGER		( ASN1_UNIVERSAL | ASN1_PRIMITIVE   | 0x02 )
#define	ASN1_OCTET_STRING	( ASN1_UNIVERSAL | ASN1_PRIMITIVE   | 0x04 )
#define	ASN1_NULL		( ASN1_UNIVERSAL | ASN1_PRIMITIVE   | 0x05 )
#define ASN1_OBJECT_IDENTIFIER	( ASN1_UNIVERSAL | ASN1_PRIMITIVE   | 0x06 )
#define ASN1_SEQUENCE		( ASN1_UNIVERSAL | ASN1_CONSTRUCTED | 0x10 )
#define	ASN1_SEQUENCE_OF	( ASN1_UNIVERSAL | ASN1_CONSTRUCTED | 0x11 )

#define ASN1_IPADDRESS		( ASN1_APPLICATION | ASN1_PRIMITIVE | 0x00 )
#define ASN1_COUNTER32		( ASN1_APPLICATION | ASN1_PRIMITIVE | 0x01 )
#define ASN1_GAUGE32		( ASN1_APPLICATION | ASN1_PRIMITIVE | 0x02 )
#define ASN1_TIMETICKS		( ASN1_APPLICATION | ASN1_PRIMITIVE | 0x03 )
#define ASN1_OPAQUE		( ASN1_APPLICATION | ASN1_PRIMITIVE | 0x04 )
#define ASN1_COUNTER64		( ASN1_APPLICATION | ASN1_PRIMITIVE | 0x06 )

EXTERN TnmTable tnmSnmpTypeTable[];

/*
 *----------------------------------------------------------------
 * Exception codes used in SNMPv2 varbind lists (RFC 1905).
 *----------------------------------------------------------------
 */

#define ASN1_NO_SUCH_OBJECT	( ASN1_CONTEXT | ASN1_PRIMITIVE | 0x00 )
#define ASN1_NO_SUCH_INSTANCE	( ASN1_CONTEXT | ASN1_PRIMITIVE | 0x01 )
#define ASN1_END_OF_MIB_VIEW	( ASN1_CONTEXT | ASN1_PRIMITIVE | 0x02 )

EXTERN TnmTable tnmSnmpExceptionTable[];

#define TnmSnmpException(x) \
	((x >= ASN1_NO_SUCH_OBJECT && x <= ASN1_END_OF_MIB_VIEW))

/*
 *----------------------------------------------------------------
 * SNMP PDU types as defined in RFC 1157 and in RFC 1905.
 *----------------------------------------------------------------
 */

#define ASN1_SNMP_GET		( ASN1_CONTEXT | ASN1_CONSTRUCTED | 0x00 )
#define ASN1_SNMP_GETNEXT	( ASN1_CONTEXT | ASN1_CONSTRUCTED | 0x01 )
#define ASN1_SNMP_RESPONSE	( ASN1_CONTEXT | ASN1_CONSTRUCTED | 0x02 )
#define	ASN1_SNMP_SET		( ASN1_CONTEXT | ASN1_CONSTRUCTED | 0x03 )
#define ASN1_SNMP_TRAP1		( ASN1_CONTEXT | ASN1_CONSTRUCTED | 0x04 )
#define ASN1_SNMP_GETBULK	( ASN1_CONTEXT | ASN1_CONSTRUCTED | 0x05 )
#define ASN1_SNMP_INFORM	( ASN1_CONTEXT | ASN1_CONSTRUCTED | 0x06 )
#define ASN1_SNMP_TRAP2		( ASN1_CONTEXT | ASN1_CONSTRUCTED | 0x07 )
#define ASN1_SNMP_REPORT	( ASN1_CONTEXT | ASN1_CONSTRUCTED | 0x08 )

#define ASN1_SNMP_GETRANGE	( ASN1_CONTEXT | ASN1_CONSTRUCTED | 0x0f )

EXTERN TnmTable tnmSnmpPDUTable[];

#ifndef ASN1_SNMP_GETRANGE
#define TnmSnmpGet(x) \
    ((x == ASN1_SNMP_GET || x == ASN1_SNMP_GETNEXT || x == ASN1_SNMP_GETBULK))
#else
#define TnmSnmpGet(x) \
    ((x == ASN1_SNMP_GET || x == ASN1_SNMP_GETNEXT \
      || x == ASN1_SNMP_GETBULK || x == ASN1_SNMP_GETRANGE))
#endif

#define TnmSnmpSet(x) \
    ((x == ASN1_SNMP_SET))

#define TnmSnmpTrap(x) \
    ((x == ASN1_SNMP_TRAP1 || x == ASN1_SNMP_TRAP2))

#define TnmSnmpNotification(x) \
    ((x == ASN1_SNMP_TRAP1 || x == ASN1_SNMP_TRAP2 || x == ASN1_SNMP_INFORM))

#define TnmSnmpReport(x) \
    ((x == ASN1_SNMP_REPORT))

#define TnmSnmpResponse(x) \
    ((x == ASN1_SNMP_RESPONSE))

/*
 *----------------------------------------------------------------
 * Functions that manipulate object identifier. THIS IS OLD CODE
 * AND SHOULD NOT BE USED ANYMORE.
 *----------------------------------------------------------------
 */

typedef u_int Tnm_Oid;

EXTERN char*
TnmOidToStr		_ANSI_ARGS_((Tnm_Oid *oid, int len));

EXTERN Tnm_Oid*
TnmStrToOid		_ANSI_ARGS_((char *str, int *len));

/*
 *----------------------------------------------------------------
 * Structure to hold a BER encode/decode buffer.
 *----------------------------------------------------------------
 */

typedef struct TnmBer {
    u_char *start;
    u_char *end;
    u_char *current;
    char error[256];
} TnmBer;

EXTERN TnmBer*
TnmBerCreate		_ANSI_ARGS_((u_char *packet, int packetlen));

EXTERN void
TnmBerDelete		_ANSI_ARGS_((TnmBer *ber));

/*
 *----------------------------------------------------------------
 * These functions get or set a static error string describing
 * the last error which incurred in the BER encoder/decoder.
 *----------------------------------------------------------------
 */

EXTERN char*
TnmBerGetError		_ANSI_ARGS_((TnmBer *ber));

EXTERN void
TnmBerSetError		_ANSI_ARGS_((TnmBer *ber, char *msg));

EXTERN void
TnmBerWrongValue	_ANSI_ARGS_((TnmBer *ber, u_char tag));

EXTERN void
TnmBerWrongLength	_ANSI_ARGS_((TnmBer *ber, u_char tag, int length));

EXTERN void
TnmBerWrongTag		_ANSI_ARGS_((TnmBer *ber, u_char tag, u_char expected));

/*
 *----------------------------------------------------------------
 * BER functions visible for other modules (encoding/decoding):
 *----------------------------------------------------------------
 */

EXTERN int
TnmBerDecDone		_ANSI_ARGS_((TnmBer *ber));

EXTERN int
TnmBerSize		_ANSI_ARGS_((TnmBer *ber));

EXTERN TnmBer*
TnmBerDecPeek		_ANSI_ARGS_((TnmBer *ber, u_char *byte));

EXTERN TnmBer*
TnmBerEncByte		_ANSI_ARGS_((TnmBer *ber, u_char byte));

EXTERN TnmBer*
TnmBerDecByte		_ANSI_ARGS_((TnmBer *ber, u_char *byte));

EXTERN TnmBer*
TnmBerEncLength		_ANSI_ARGS_((TnmBer *ber, u_char *position,
				     int length));
EXTERN TnmBer*
TnmBerDecLength		_ANSI_ARGS_((TnmBer *ber, int *length));

EXTERN TnmBer*
TnmBerEncSequenceStart  _ANSI_ARGS_((TnmBer *ber, u_char tag,
				     u_char **token));
EXTERN TnmBer*
TnmBerEncSequenceEnd    _ANSI_ARGS_((TnmBer *ber,
				     u_char *token));
EXTERN TnmBer*
TnmBerDecSequenceStart  _ANSI_ARGS_((TnmBer *ber, u_char tag,
				     u_char **token, int *length));
EXTERN TnmBer*
TnmBerDecSequenceEnd    _ANSI_ARGS_((TnmBer *ber,
                                     u_char *token, int length));
EXTERN TnmBer*
TnmBerEncInt		_ANSI_ARGS_((TnmBer *ber, u_char tag,
				     int value));
EXTERN TnmBer*
TnmBerDecInt		_ANSI_ARGS_((TnmBer *ber, u_char tag,
				     int *value));
EXTERN TnmBer*
TnmBerEncUnsigned64	_ANSI_ARGS_((TnmBer *ber,
				     double value));
EXTERN TnmBer*
TnmBerDecUnsigned64	_ANSI_ARGS_((TnmBer *ber,
				     TnmUnsigned64 *uPtr));
EXTERN TnmBer*
TnmBerEncOID		_ANSI_ARGS_((TnmBer *ber,
				     Tnm_Oid *oid, int oidlen));
EXTERN TnmBer*
TnmBerDecOID		_ANSI_ARGS_((TnmBer *ber,
				     Tnm_Oid *oid, int *oidlen));
EXTERN TnmBer*
TnmBerEncOctetString	_ANSI_ARGS_((TnmBer *ber, u_char tag,
				     char *octets, int len));
EXTERN TnmBer*
TnmBerDecOctetString	_ANSI_ARGS_((TnmBer *ber, u_char tag,
				     char **octets, int *len));
EXTERN TnmBer*
TnmBerEncNull		_ANSI_ARGS_((TnmBer *ber, u_char tag));

EXTERN TnmBer*
TnmBerDecNull		_ANSI_ARGS_((TnmBer *ber, u_char tag));

EXTERN TnmBer*
TnmBerDecAny		_ANSI_ARGS_((TnmBer *ber, char **octets, int *len));

#endif /* _TNMASN1 */
