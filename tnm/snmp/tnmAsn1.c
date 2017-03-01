/*
 * tnmAsn1.c --
 *
 *	This is the implementation of the ASN1/BER encoding and 
 *	decoding functions. This file also includes the functions
 *	to handle ASN1 object identifier.
 *
 * Copyright (c) 1994-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2002 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#include "tnmSnmp.h"

#include <math.h>

/*
 * The following tables are used to convert SNMP types and SNMP
 * exceptions (or syntax tags in ASN.1 speak) to internally used
 * token.
 */

TnmTable tnmSnmpTypeTable[] =
{
    { ASN1_OTHER,		"OTHER" },
    { ASN1_INTEGER,		"Integer32" },
    { ASN1_INTEGER,		"INTEGER" },
    { ASN1_OCTET_STRING,	"OCTET STRING" },
    { ASN1_NULL,		"NULL" },
    { ASN1_OBJECT_IDENTIFIER,	"OBJECT IDENTIFIER" },
    { ASN1_SEQUENCE,		"SEQUENCE" },
    { ASN1_SEQUENCE_OF,		"SEQUENCE OF" },
    { ASN1_IPADDRESS,		"IpAddress" },
    { ASN1_IPADDRESS,		"NetworkAddress" },
    { ASN1_COUNTER32,		"Counter32" },
    { ASN1_COUNTER32,		"Counter" },
    { ASN1_GAUGE32,		"Unsigned32" },
    { ASN1_GAUGE32,		"Gauge32", },
    { ASN1_GAUGE32,		"Gauge" },
    { ASN1_TIMETICKS,		"TimeTicks" },
    { ASN1_OPAQUE,		"Opaque" },
    { ASN1_COUNTER64,		"Counter64" },
    { 0, NULL }
};

TnmTable tnmSnmpPDUTable[] = {
    { ASN1_SNMP_GET,		"get" },
    { ASN1_SNMP_GETNEXT,	"getnext" },
    { ASN1_SNMP_RESPONSE,	"response" },
    { ASN1_SNMP_SET,		"set" },
    { ASN1_SNMP_TRAP1,		"trap1" },
    { ASN1_SNMP_GETBULK,	"getbulk" },
    { ASN1_SNMP_INFORM,		"inform" },
    { ASN1_SNMP_TRAP2,		"trap2" },
    { ASN1_SNMP_REPORT,		"report" },
    { 0, NULL }
};

TnmTable tnmSnmpExceptionTable[] =
{
    { ASN1_NO_SUCH_OBJECT,	"noSuchObject" },
    { ASN1_NO_SUCH_INSTANCE,	"noSuchInstance" },
    { ASN1_END_OF_MIB_VIEW,	"endOfMibView" },
    { 0, NULL }
};

/*
 * Forward declarations for procedures defined later in this file:
 */

#if 0
static u_char*
EncodeLength		(u_char *packet, int *packetlen,
				     int length);
#endif


/*
 *----------------------------------------------------------------------
 *
 * TnmOidToStr --
 *
 *	This procedure converts an object identifier into string
 *	in dotted notation.
 *
 * Results:
 *	Returns the pointer to the string in static memory.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char*
TnmOidToStr(Tnm_Oid *oid, int oidLen)
{
    int i;
    static char buf[TNM_OID_MAX_SIZE * 8];
    char *cp;

    if (oid == NULL) return NULL;

    buf[0] = '\0';
    
    for (cp = buf, i = 0; i < oidLen; i++) {
        if (oid[i] < 10) {
	    *cp++ = '0' + oid[i];
	} else {
	    u_int t=10;
	    char c = '0'+ (oid[i] % 10);
	    u_int u = oid[i] / 10;
	    while (u / t) t *= 10;
	    while (t /= 10) *cp++ = '0'+ (u / t) % 10;
	    *cp++ = c;
	}
	*cp++ = '.';
    }
    if (cp > buf) {
	*--cp = '\0';
    }
    
    return buf;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmStrToOid --
 *
 *	This procedure converts a string with an object identifier
 *	in dotted representation into an object identifier vector.
 *
 * Results:
 *	Returns the pointer to the vector in static memory or a
 *	NULL pointer if the string contains illegal characters or
 *	exceeds the maximum length of an object identifier.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tnm_Oid*
TnmStrToOid(const char *str, int *len)
{
    static Tnm_Oid oid[TNM_OID_MAX_SIZE];

    if (str == NULL) return NULL;
    if (*str == '.') str++;

    memset((char *) oid, 0, sizeof(oid));

    if (! *str) {
	*len = 0;
	return oid;
    }

    for (*len = 0; *str; str++) {
	if (isdigit(*str)) {
	    oid[*len] = 10 * oid[*len] + *str - '0';
	} else if (*str == '.' && *len < (TNM_OID_MAX_SIZE - 1)) {
	    *len += 1;
	} else {
	    return NULL;
	}
    }
    *len += 1;

    /*
     * Check the ASN.1 restrictions on object identifier values.
     */

    if ((*len < 2) || (oid[0] > 2)) {
	return NULL;
    }

    return oid;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerCreate --
 *
 *	This procedure create a new BER stream.
 *
 * Results:
 *	The new BER stream or NULL
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerCreate(u_char *packet, int packetlen)
{
    TnmBer *ber;

    ber = (TnmBer *) ckalloc(sizeof(TnmBer));
    memset((char *) ber, 0, sizeof(TnmBer));

    if (packet && packetlen > 0) {
	ber->start = packet;
	ber->end = packet + packetlen;
	ber->current = packet;
    }
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDelete --
 *
 *	This procedure deletes an existing BER stream.
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
TnmBerDelete(TnmBer *ber)
{
    if (ber) {
	ckfree((char *) ber);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerGetError --
 *
 *	This procedure provides access to a human readable error
 *	message.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char*
TnmBerGetError(TnmBer *ber)
{
    return ber->error;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerSetError --
 *
 *	This procedure allows to store an arbitrary error message
 *	in the BER error variable.
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
TnmBerSetError(TnmBer *ber, char *msg)
{
    if (ber) {
	strncpy(ber->error, msg, sizeof(ber->error));
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerWrongValue --
 *
 *	This procedure is called to signal a BER encoding/decoding
 *	error due to an invalid value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The BER error message is updated.
 *
 *----------------------------------------------------------------------
 */

void
TnmBerWrongValue(TnmBer *ber, u_char tag)
{
    sprintf(ber->error, "invalid value for tag 0x%.2x at byte %d",
	    tag, ber->current - ber->start);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerWrongLength --
 *
 *	This procedure is called to signal a BER encoding/decoding
 *	error due to an invalid length.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The BER error message is updated.
 *
 *----------------------------------------------------------------------
 */

void
TnmBerWrongLength(TnmBer *ber, u_char tag, int length)
{
    sprintf(ber->error, "invalid length %d for tag 0x%.2x at byte %d", 
	    length, tag, ber->current - ber->start);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerWrongTag --
 *
 *	This procedure is called to signal a BER encoding/decoding
 *	error due to an unexpected or unknown tag value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The BER error message is updated.
 *
 *----------------------------------------------------------------------
 */

void
TnmBerWrongTag(TnmBer *ber, u_char tag, u_char expected)
{
    if (expected) {
	sprintf(ber->error, "invalid tag 0x%.2x at byte %d (expected 0x%.2x)",
		tag, ber->current - ber->start, expected);
    } else {
	sprintf(ber->error, "invalid tag 0x%.2x at byte %d", tag,
		ber->current - ber->start);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDecDone --
 *
 *	This procedure returns a boolean indicating whether we are done
 *	decoding the BER stream.
 *
 * Results:
 *	A boolean value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmBerDecDone(TnmBer *ber)
{
    return (!ber || ber->current >= ber->end);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerSize --
 *
 *	This procedure returns the size in bytes of the currently
 *      encoded/decoded BER stream.
 *
 * Results:
 *	Number of the encoded/decoded BER bytes so far.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TnmBerSize(TnmBer *ber)
{
    return (ber->current - ber->start);
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDecPeek --
 *
 *	This procedure returns the next byte of the BER stream
 *	without actually removing the byte from the BER stream.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerDecPeek(TnmBer *ber, u_char *byte)
{
    if (! ber) {
	return NULL;
    }

    if (ber->current >= ber->end) {
	TnmBerSetError(ber, "incomplete BER encoding");
	return NULL;
    }
    *byte = *(ber->current);
    return ber;
}

#if 0
static u_char*
EncodeLength(u_char *packet, int *packetlen, int length)
{
    if (length < 0 || length > 0xffff) {
	Tcl_Panic("illegal ASN.1 length");
    }

    if (length < 0x7f) {
	*packet++ = length;
	packetlen += 1;
    } else if (length < 0xff) {
	*packet++ = 0x81;
	*packet++ = length;
	packetlen += 2;
    } else if (length < 0xffff) {
	*packet++ = 0x82;
	*packet++ = ((length >> 8) & 0xff);
	*packet++ = (length & 0xff);
	packetlen += 3;
    }
    return packet;
}
#endif



/*
 *----------------------------------------------------------------------
 *
 * TnmBerEncByte --
 *
 *	This procedure encodes a single byte into a BER stream.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	The BER stream is getting longer by one byte.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerEncByte(TnmBer *ber, u_char byte)
{
    if (! ber) {
	return NULL;
    }

    if (ber->current >= ber->end) {
	TnmBerSetError(ber, "BER buffer size exceeded");
	return NULL;
    }
    *(ber->current)++ = byte;
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDecByte --
 *
 *	This procedure decodes a single byte from an BER stream.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	The BER stream is getting shorter by one byte.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerDecByte(TnmBer *ber, u_char *byte)
{
    if (! ber) {
	return NULL;
    }

    if (ber->current >= ber->end) {
	TnmBerSetError(ber, "BER buffer overflow");
	return NULL;
    }
    *byte = *(ber->current)++;
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerEncLength --
 *
 *	This procedure sets the length field of any BER encoded
 *	ASN.1 type. If length is > 0x7f the array is shifted to get
 *	enough space to hold the value.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	The BER encoded octets might be moved.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerEncLength(TnmBer *ber, u_char *position, int length)
{
    int i, d;

    if (! ber) {
	return NULL;
    }

    if (length < 0x80) {

	*position = length;

    } else if (length <= 0x80000000) {	/* msgMaxSize, RFC 2572 */

	for (d = 0; (length >> (d * 8)); d++) ;

	if (ber->current + d >= ber->end) {
	    TnmBerSetError(ber, "BER buffer overflow");
	    return NULL;
	}

    	for (i = ber->current - position - 1; i > 0; i--) {
	    position[i + d] = position[i];
        }
	ber->current += d;
	*position++ = 0x80 + d;

	for (; d > 0; d--) {
	    *position++ = d > 2 ? 0 : ( ( length >> (8 * (d-1)) ) & 0xff );
	}
	
    } else {

	TnmBerSetError(ber, "ASN.1 length too long");
	return NULL;
    }

    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDecLength --
 *
 *	This procedure decodes the length field of any ASN1 encoded 
 *	type. If length field is in indefinite length form or longer
 *	than the size of an unsigned int, an error is reported.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerDecLength(TnmBer *ber, int *length)
{
    u_char byte;

    if (! ber) {
        return NULL;
    }

    ber = TnmBerDecByte(ber, &byte);
    if (! ber) {
	return NULL;
    }
    
    /*
     * Check if length field is longer than one byte.
     */
    
    if (byte & 0x80) {

	int i, len;

	len = byte & 0x7f;

	/*
	 * This code does skip any leading 0's and thus it is able
	 * to correctly handle strange length encodings such as
	 * 87:00:00:00:00:00:00:93.
	 */

	*length = 0;
	for (i = 0; i < len; i++) {
	    ber = TnmBerDecByte(ber, &byte);
	    if (! ber) {
		return NULL;
	    }
	    if (len - i > sizeof(int) && byte != 0x00) {
		/* xxx TnmBerWrongLength(0, *packetlen, *packet); */
		return NULL;
	    }
	    *length = *length << 8 | (byte & 0xff);
	}
	if (*length < 0) {
	    return NULL;
	}

    } else {

	*length    = byte;
    }

    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerEncSequenceStart --
 *
 *	This procedure marks the start of a BER encoded SEQUENCE or
 *	SEQUENCE OF. A pointer to the start of the sequence is
 *	initialized which must be presented when calling the 
 *	procedure to close a SEQUENCE or SEQUENCE OF.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerEncSequenceStart(TnmBer *ber, u_char tag, u_char **token)
{
    ber = TnmBerEncByte(ber, tag);
    if (! ber) {
        return NULL;
    }
    
    *token = ber->current;
    ber = TnmBerEncByte(ber, 0);
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerEncSequenceEnd --
 *
 *	This procedure closes a previously opened SEQUENCE or
 *	SEQUENCE OF encoding. The token passed to the procedure
 *	must be obtained from a previous call to the procedure
 *	which starts a SEQUENCE or SEQUENCE OF.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	The BER encoded octets might be moved.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerEncSequenceEnd(TnmBer *ber, u_char *token)
{
    if (! ber) {
        return ber;
    }

    ber = TnmBerEncLength(ber, token, ber->current - (token + 1));
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDecSequenceStart --
 *
 *	This procedure decodes the start of a BER encoded SEQUENCE or
 *	SEQUENCE OF. A pointer to the start of the sequence is
 *	initialized which must be presented when calling the 
 *	procedure to close a SEQUENCE or SEQUENCE OF.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerDecSequenceStart(TnmBer *ber, u_char tag, u_char **token, int *length)
{
    u_char byte;

    ber = TnmBerDecByte(ber, &byte);
    if (! ber) {
	return NULL;
    }

    if (byte != tag) {
	TnmBerWrongTag(ber, byte, tag);
	return NULL;
    }

    ber = TnmBerDecLength(ber, length);
    if (! ber) {
	return NULL;
    }

    *token = ber->current;
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDecSequenceEnd --
 *
 *	This procedure closes a previously opened SEQUENCE or
 *	SEQUENCE OF decoding. The token and length passed to the
 *	procedure must be obtained from a previous call to the
 *	procedure which starts a SEQUENCE or SEQUENCE OF.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerDecSequenceEnd(TnmBer *ber, u_char *token, int length)
{
    int len;

    if (! ber) {
        return NULL;
    }

    len = ber->current - token;
    if (length != len) {
	sprintf(ber->error, "sequence %s at byte %d (%d bytes missing)",
		(length > len) ? "underflow" : "overflow",
		ber->current - ber->start,
		(length > len) ? length - len : len - length);
	return NULL;
    }
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerEncInt --
 *
 *	This procedure encodes an ASN.1 Integer value (means an int) 
 *	by using the primitive, definite length encoding method.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerEncInt(TnmBer *ber, u_char tag, int value)
{
    int asnlen  = 0;
    int intsize = sizeof(int);
    int mask;
    u_char *length;

    ber = TnmBerEncByte(ber, tag);
    if (! ber) {
	return NULL;
    }
    
    length = ber->current;
    ber = TnmBerEncByte(ber, 0);
    if (! ber) {
	return ber;
    }

    /* 
     * Set the leftmost 9 bits of mask to 1 and check if the 
     * leftmost bits of value are 0 or 1.
     */

    mask = 0x1ff << ( ( 8 * ( sizeof(int) - 1 ) ) - 1 );
    
    while ((((value & mask) == 0) 
	    || ((value & mask) == mask )) && intsize > 1) {
	intsize--;
	value <<= 8;
    }

    /*
     * Set the leftmost 8 bits of mask to 1 and build the 
     * two's complement of value.
     */

    mask = 0xff << ( 8 * ( sizeof(int) - 1 ) );
    while (ber && intsize--) {
	ber = TnmBerEncByte(ber, (( value & mask ) >> ( 8 * ( sizeof(int) - 1 ))));
	value <<= 8;
	asnlen += 1;
    }

    ber = TnmBerEncLength(ber, length, asnlen);
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDecInt --
 *
 *	This procedure decodes an ASN.1 integer value. We return an
 *	error if an int is not large enough to hold the ASN.1 value.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerDecInt(TnmBer *ber, u_char tag, int *value)
{
    int len = 0;
    int negative = 0;
    u_char byte;

    ber = TnmBerDecByte(ber, &byte);
    if (! ber) {
	return NULL;
    }

    if (byte != tag) {
	TnmBerWrongTag(ber, byte, tag);
	return NULL;
    }

    /*
     * Handle invalid integer size.
     */

    ber = TnmBerDecLength(ber, &len);
    if (! ber) {
	return NULL;
    }
    
    if (len == 0) {
	*value = 0;
	return ber;
    }
    
    /*
     * Check for an overflow for normal 32 bit integer values.
     */
    
    ber = TnmBerDecPeek(ber, &byte);
    if (! ber) {
	return NULL;
    }

    if ((byte != 0 && len > sizeof(int))
	|| (byte == 0 && len-1 > sizeof(int))) {
	TnmBerWrongLength(ber, tag, len);
	return NULL;
    }
    
    /*
     * Check if it is a negative value and decode data.
     */

    if ((tag == ASN1_INTEGER) && (byte & 0x80)) {
	*value = -1;
	negative = 1;
    } else {
	*value = 0;
	negative = 0;
    }

    while (len-- > 0) {
	ber = TnmBerDecByte(ber, &byte);
	if (! ber) {
	    return NULL;
	}
	*value = (*value << 8) | (byte & 0xff);
    }

    /*
     * Negative values are only allowed for ASN1_INTEGER tags.
     * Return an error if we get something negative for an
     * unsigned type, e.g. a Counter.
     */

    if (negative && (tag != ASN1_INTEGER)) {
	TnmBerWrongValue(ber, tag);
	return NULL;
    }

    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerEncUnsigned64 --
 *
 *	This procedure encodes an ASN.1 Unsigned64 value by using the
 *	primitive, definite length encoding method. Note, on 64 bit
 *	machines, TnmBerEncInt() should be used as it yields accurate
 *	results.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerEncUnsigned64(TnmBer *ber, double value)
{
    int i, len = 0;
    u_char *length;
    double d;

    ber = TnmBerEncByte(ber, ASN1_COUNTER64);
    if (! ber) {
	return NULL;
    }
    
    length = ber->current;
    ber = TnmBerEncByte(ber, 0);
    if (! ber) {
	return NULL;
    }
    
    /*
     * Calculate the number of bytes needed to encode the ASN.1
     * integer.
     */

    for (d = value; d >= 1; len++) {
	d /= 256.0;
    }

    /*
     * Now encode the bytes: We start at the end and move up
     * to the high byte.
     */

    if (len > 65535 || ber->current + len > ber->end) {
	TnmBerSetError(ber, "BER buffer size exceeded");
	return NULL;
    }

    for (i = len - 1; i >= 0; i--) {
	d = value / 256.0;
	ber->current[i] = (int) (value - floor(d) * 256);
	value = d;
    }
    ber->current += len;
    
    ber = TnmBerEncLength(ber, length, len);
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDecUnsigned64 --
 *
 *	This procedure decodes an ASN.1 Unsigned64 value. We return 
 *	the result as a double. Use TnmBerDecInt() on 64 bit machines
 *	to get an accurate result.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerDecUnsigned64(TnmBer *ber, TnmUnsigned64 *uPtr)
{
    int len = 0;
    u_char byte;

    ber = TnmBerDecByte(ber, &byte);
    if (! ber) {
	return NULL;
    }

    if (byte != ASN1_COUNTER64) {
	TnmBerWrongTag(ber, byte, ASN1_COUNTER64);
	return NULL;
    }

    ber = TnmBerDecLength(ber, &len);
    if (!  ber) {
	return NULL;
    }

    /*
     * Check for an overflow for normal 32 bit integer values.
     */
    
    if (len-1 > 8) {
	TnmBerWrongLength(ber, ASN1_COUNTER64, len);
	return NULL;
    }


    *uPtr = 0;
    while (len-- > 0) {
	ber = TnmBerDecByte(ber, &byte);
	*uPtr = *uPtr * 256 + byte;
    }

    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerEncOID --
 *
 *	This procedure encodes an OBJECT IDENTIFIER value.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerEncOID(TnmBer *ber, Tnm_Oid *oid, int oidLen)
{
    int len = 0;
#if (SIZEOF_LONG == 8)
    int mask, bits;
#else
    long mask, bits;
#endif
    Tnm_Oid *op = oid;
    u_char *length;
    
    /* 
     * Check for a valid length. An OBJECT IDENTIFIER must have at
     * least two components and the first two values must fall into
     * the range [0..2] (see also below).
     */

    if ((oidLen < 2) || (oid[0] > 2)) {
	TnmBerSetError(ber, "illegal OBJECT IDENTIFIER value");
        return NULL;
    }

    ber = TnmBerEncByte(ber, ASN1_OBJECT_IDENTIFIER);
    if (! ber) {
	return NULL;
    }
    
    length = ber->current;
    ber = TnmBerEncByte(ber, 0);
    if (! ber) {
	return NULL;
    }
    
    /*
     * Pack the first two components using the formula (X * 40) + Y
     * under the constraint that X is in the range [0..2]. The precise
     * rules can be found in X.690:1997 clause 8.19. This clause also
     * gives an example of the legal OID value 2.100.3 which is
     * encoded as 0x0603813403.
     */

    oid[1] += oid[0] * 40;
    oidLen--;
    op++;
    
    while (ber && oidLen-- > 0) {
	
	/* are seven bits enough for this component */
	
	if (*op <= 0x7f) {

	    ber = TnmBerEncByte(ber, *op++);
	    len += 1;
	    
	} else {
	    
	    /* we need two or more octets for encoding */
	    
	    /* check nr of bits for this component */
	    
	    int n = sizeof(*op) * 8;		/* max bit of op */
	    
	    mask = 1 << (n - 1);
	    for (bits = n; bits > 0; bits--, mask >>= 1) {
		if (*op & mask) break;
	    }
	    
	    /* round # of bits to multiple of 7: */
	    
	    bits = ((bits + 6) / 7) * 7;
	    
	    /* Handle the first sequence of 7 bits if we have a
	       large number. */
	    
	    if (bits > n ) {
		bits -= 7;
		ber = TnmBerEncByte(ber, ((( *op >> bits ) & 0x7f) | 0x80 ));
		len += 1;
	    }
	    
	    mask = (1 << bits) - 1;
	    
	    /* encode the mostleft 7 bits and shift right */
	    
	    for (; ber && bits > 7; mask >>= 7 ) {
		bits -= 7;
		ber = TnmBerEncByte(ber, ((( *op & mask ) >> bits ) | 0x80 ));
		len += 1;
	    }

	    ber = TnmBerEncByte(ber, ( *op++ & mask ));
	    len += 1;
	}
    }

    oid[1] -= oid[0] * 40;
    ber = TnmBerEncLength(ber, length, len);
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDecOID --
 *
 *	This procedure decodes and object identifier. The caller of
 *	this function is responsible to provide enough memory to hold
 *	the object identifier.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerDecOID(TnmBer *ber, Tnm_Oid *oid, int *oidLen)
{
    int len;
    u_char byte;

    ber = TnmBerDecByte(ber, &byte);
    if (! ber) {
	return NULL;
    }

    if (byte != ASN1_OBJECT_IDENTIFIER) {
	TnmBerWrongTag(ber, byte, ASN1_OBJECT_IDENTIFIER);
	return NULL;
    }
    
    ber = TnmBerDecLength(ber, &len);
    if (! ber) {
	return NULL;
    }

    if (len == 0) {
	TnmBerWrongValue(ber, ASN1_OBJECT_IDENTIFIER);
	return NULL;
    }

    *oidLen = 1;
    while (len > 0) {
	oid[*oidLen] = 0;
	ber = TnmBerDecByte(ber, &byte);
	if (! ber) {
	    return NULL;
	}
	len--;
	
	while (byte > 0x7f) {
	    oid[*oidLen] = ( oid[*oidLen] << 7 ) + ( byte & 0x7f );
	    ber = TnmBerDecByte(ber, &byte);
	    if (! ber) {
		return NULL;
	    }
	    len--;
	}
	oid[*oidLen] = ( oid[*oidLen] << 7 ) + ( byte );
	
	if (*oidLen == 1) {
	    oid[0] = oid[1] / 40;
	    if (oid[0] > 2) oid[0] = 2;
	    oid[1] -= (oid[0] * 40);
	}
	(*oidLen)++;
	if (*oidLen > TNM_OID_MAX_SIZE) {
	    TnmBerWrongValue(ber, ASN1_OBJECT_IDENTIFIER);
	    return NULL;
	}
    }

    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerEncOctetString --
 *
 *	This procedure encodes an OCTET STRING value. The first
 *	encoded byte is the tag of this octet string followed by the
 *	length of the octet string and the octets itself.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerEncOctetString(TnmBer *ber, u_char tag, char *octets, int octets_len)
{
    int	i;
    u_char *length;

    ber = TnmBerEncByte(ber, tag);
    if (! ber) {
	return NULL;
    }

    length = ber->current;
    ber = TnmBerEncByte(ber, 0);

    for (i = 0; ber && i < octets_len; i++) {
	ber = TnmBerEncByte(ber, octets[i]);
    }

    ber = TnmBerEncLength(ber, length, octets_len);
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDecOctetString --
 *
 *	This procedure decodes an OCTET STRING. It is up to the caller
 *	to copy the octets in private memory if the packet itself is
 *	cleared. The first byte in the packet must match the tag given
 *	by type followed by the length of the octet string and the 
 *	octets themself.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerDecOctetString(TnmBer *ber, u_char tag, char **octets, int *octets_len)
{
    int len;
    u_char byte;

    ber = TnmBerDecByte(ber, &byte);
    if (! ber) {
        return NULL;
    }

    if (byte != tag) {
	TnmBerWrongTag(ber, byte, tag);
	return NULL;
    }

    /*
     * Handle zero length octet string of the form 0x04 0x00.
     */

    ber = TnmBerDecLength(ber, &len);
    if (! ber) {
	return NULL;
    }

    if (len > 65535 || ber->current + len > ber->end) {
	TnmBerSetError(ber, "BER buffer size exceeded");
	return NULL;
    }

    if (octets && octets_len) {
	*octets = (char *) ber->current;
	*octets_len = len;
    }
    ber->current += len;

    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerEncNull --
 *
 *	This procedure encodes an ASN.1 NULL value.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerEncNull(TnmBer *ber, u_char tag)
{
    ber = TnmBerEncByte(ber, tag);
    ber = TnmBerEncByte(ber, 0);
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDecNull --
 *
 *	This procedure decodes an ASN.1 NULL value. Checks if the tag 
 *	matches the required tag given in tag.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerDecNull(TnmBer *ber, u_char tag)
{
    int len;
    u_char byte;

    ber = TnmBerDecByte(ber, &byte);
    if (! ber) {
        return NULL;
    }
    
    if (byte != tag) {
	TnmBerWrongTag(ber, byte, tag);
	return NULL;
    }

    ber = TnmBerDecLength(ber, &len);
    return ber;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmBerDecAny --
 *
 *	This procedure decodes an arbitrary value and returns it as an
 *	Opaque value. This is used as a last resort if we receive
 *	packets with unkown tags in it.
 *
 * Results:
 *	A pointer to the BER byte stream or NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TnmBer*
TnmBerDecAny(TnmBer *ber, char **octets, int *octets_len)
{
    int len;
    u_char *start;
    u_char byte;

    start = ber->current;

    ber = TnmBerDecByte(ber, &byte);
    if (! ber) {
	return NULL;
    }

    ber = TnmBerDecLength(ber, &len);
    if (! ber) {
	return NULL;
    }

    if (len > 65535 || ber->current + len > ber->end) {
	return NULL;
    }
    *octets = (char *)start;
    *octets_len = len + (ber->current - start);
    ber->current += len;
    
    return ber;
}


