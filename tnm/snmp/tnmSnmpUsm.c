/*
 * tnmSnmpUsm.c --
 *
 *	This file contains the implementation of the user based
 *	security model (USM) for SNMP version 3 as defined in
 *	RFC 2274.
 *
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * @(#) $Id: tnmSnmpUsm.c,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $
 */

#include "tnmSnmp.h"
#include "tnmMib.h"
#include "tnmMD5.h"
#include "tnmSHA.h"

/*
 * The table of known SNMP security levels.
 */

TnmTable tnmSnmpSecurityLevelTable[] =
{
    { TNM_SNMP_AUTH_NONE | TNM_SNMP_PRIV_NONE,	"noAuth/noPriv" },
    { TNM_SNMP_AUTH_MD5  | TNM_SNMP_PRIV_NONE,	"md5/noPriv" },
    { TNM_SNMP_AUTH_MD5  | TNM_SNMP_PRIV_DES,	"md5/des" },
    { TNM_SNMP_AUTH_SHA  | TNM_SNMP_PRIV_NONE,	"sha/noPriv" },
    { TNM_SNMP_AUTH_SHA  | TNM_SNMP_PRIV_DES,	"sha/des" },
    { 0, NULL }
};

/*
 * The following structures and procedures are used to keep a list of
 * keys that were computed with the SNMPv3 password to key algorithm.
 * This cache is needed so that identical sessions don't suffer from
 * repeated slow computations of authentication keys.
 */

typedef struct KeyCache {
    Tcl_Obj *password;
    Tcl_Obj *engineID;
    Tcl_Obj *key;
    int algorithm;
    struct KeyCache *nextPtr;
} KeyCache;

/*
 * Forward declarations for procedures defined later in this file:
 */

static void
MD5PassWord2Key	_ANSI_ARGS_((u_char *pwBytes, int pwLength,
			     u_char *engineBytes, int engineLength,
			     u_char *key));
static void
SHAPassWord2Key	_ANSI_ARGS_((u_char *pwBytes, int pwLength,
			     u_char *engineBytes, int engineLength,
			     u_char *key));
static void
ComputeKey	_ANSI_ARGS_((Tcl_Obj **objPtrPtr, Tcl_Obj *password,
			     Tcl_Obj *engineID, int algorithm));
static void
MD5AuthOutMsg	_ANSI_ARGS_((char *authKey, u_char *msg, int msgLen,
			     u_char *msgAuthenticationParameters));


/*
 *----------------------------------------------------------------------
 *
 * MD5PassWord2Key --
 *
 *	This procedure converts a password into a key by using
 *	the `Password to Key Algorithm' as defined in RFC 2274.
 *	The code is a slightly modified version of the source
 *	code in appendix A.2.1 of RFC 2274.
 *
 * Results:
 *	The key is written to the argument key.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
MD5PassWord2Key(pwBytes, pwLength, engineBytes, engineLength, key)
    u_char *pwBytes;
    int pwLength;
    u_char *engineBytes;
    int engineLength;
    u_char *key;
{
    MD5_CTX MD;
    u_char *cp, buffer[64];
    int i, index = 0, count = 0;

    TnmMD5Init(&MD);
    while (count < 1048576) {
	cp = buffer;
	for(i = 0; i < 64; i++) {
	    *cp++ = pwBytes[index++ % pwLength];
	}
	TnmMD5Update(&MD, buffer, 64);
	count += 64;
    }
    TnmMD5Final(key, &MD);

    memcpy(buffer, key, 16);
    memcpy(buffer + 16, engineBytes, (size_t) engineLength);
    memcpy(buffer + 16 + engineLength, key, 16);
    
    TnmMD5Init(&MD);
    TnmMD5Update(&MD, buffer, 16 + engineLength + 16);
    TnmMD5Final(key, &MD);
}

/*
 *----------------------------------------------------------------------
 *
 * SHAPassWord2Key --
 *
 *	This procedure converts a password into a key by using
 *	the `Password to Key Algorithm' as defined in RFC 2274.
 *	The code is a slightly modified version of the source
 *	code in appendix A.2.2 of RFC 2274.
 *
 * Results:
 *	The key is written to the argument key.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
SHAPassWord2Key(pwBytes, pwLength, engineBytes, engineLength, key)
    u_char *pwBytes;
    int pwLength;
    u_char *engineBytes;
    int engineLength;
    u_char *key;
{
    SHA_CTX SH;
    u_char *cp, buffer[72];
    int i, index = 0, count = 0;

    TnmSHAInit(&SH);
    while (count < 1048576) {
	cp = buffer;
	for(i = 0; i < 64; i++) {
	    *cp++ = pwBytes[index++ % pwLength];
	}
	TnmSHAUpdate(&SH, buffer, 64);
	count += 64;
    }
    TnmSHAFinal(key, &SH);
    
    memcpy(buffer, key, 20);
    memcpy(buffer + 20, engineBytes, (size_t) engineLength);
    memcpy(buffer + 20 + engineLength, key, 20);
    
    TnmSHAInit(&SH);
    TnmSHAUpdate(&SH, buffer, 20 + engineLength + 20);
    TnmSHAFinal(key, &SH);
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeKey --
 *
 *	This procedure computes keys by applying the password to
 *	key transformation. A cache of previously computed keys
 *	is maintained in order to save some computations.
 *
 * Results:
 *	A pointer to the key or NULL if not found.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
ComputeKey(objPtrPtr, password, engineID, algorithm)
    Tcl_Obj **objPtrPtr;
    Tcl_Obj *password;
    Tcl_Obj *engineID;
    int algorithm;
{
    char *pwBytes, *engineBytes, *bytes;
    int pwLength, engineLength, length;
    KeyCache *elemPtr;
    static KeyCache *keyList = NULL;
    char buffer[256];	/* must be large enough to hold keys */

    if (*objPtrPtr) {
	Tcl_DecrRefCount(*objPtrPtr);
	*objPtrPtr = NULL;
    }

    pwBytes = Tcl_GetStringFromObj(password, &pwLength);
    engineBytes = TnmGetOctetStringFromObj(NULL, engineID, &engineLength);

    if (! pwBytes || ! engineBytes || engineLength == 0 || pwLength == 0) {
	return;
    }
    
    /*
     * Check whether the key is already in our cache. We have
     * to check that the password and the engineID  matches as
     * well as the algorithm.
     */

    for (elemPtr = keyList; elemPtr; elemPtr = elemPtr->nextPtr) {
	if (elemPtr->algorithm != algorithm) continue;

	bytes = Tcl_GetStringFromObj(elemPtr->password, &length);
	if (length != pwLength) continue;
	if (memcmp(pwBytes, bytes, (size_t) length) != 0) continue;
	
	bytes = Tcl_GetStringFromObj(elemPtr->engineID, &length);
	if (length != engineLength) continue;
	if (memcmp(engineBytes, bytes, (size_t) length) != 0) continue;

	*objPtrPtr = elemPtr->key;
	Tcl_IncrRefCount(*objPtrPtr);
    }

    /*
     * Compute a new key as described in the appendix of RFC 2274.
     */

    switch (algorithm) {
    case TNM_SNMP_AUTH_MD5:
	MD5PassWord2Key(pwBytes, pwLength, engineBytes, engineLength, buffer);
	*objPtrPtr = TnmNewOctetStringObj(buffer, 16);
	Tcl_IncrRefCount(*objPtrPtr);
	break;
    case TNM_SNMP_AUTH_SHA:
	SHAPassWord2Key(pwBytes, pwLength, engineBytes, engineLength, buffer);
	*objPtrPtr = TnmNewOctetStringObj(buffer, 20);
	Tcl_IncrRefCount(*objPtrPtr);
	break;
    default:
	Tcl_Panic("unknown algorithm for password to key conversion");
    }

    /*
     * Finally, create a new cache entry to save the result for the
     * future.
     */

    elemPtr = (KeyCache *) ckalloc(sizeof(KeyCache));
    elemPtr->algorithm = algorithm;
    elemPtr->password = password;
    Tcl_IncrRefCount(elemPtr->password);
    elemPtr->engineID = engineID;
    Tcl_IncrRefCount(elemPtr->engineID);
    elemPtr->key = *objPtrPtr;
    Tcl_IncrRefCount(elemPtr->key);
    elemPtr->nextPtr = keyList;
    keyList = elemPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpComputeKeys --
 *
 *	This procedure computes new authentication and privacy key
 *	for a given SNMPv3 session.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The authentication and privacy keys for this session are updated.
 *
 *----------------------------------------------------------------------
 */

void
TnmSnmpComputeKeys(session)
    TnmSnmp *session;
{
    int authProto, privProto;

    authProto = (session->securityLevel & TNM_SNMP_AUTH_MASK);
    privProto = (session->securityLevel & TNM_SNMP_PRIV_MASK);

    if (authProto != TNM_SNMP_AUTH_NONE) {
	ComputeKey(&session->usmAuthKey, session->authPassWord,
		   session->engineID, authProto);
	if (privProto != TNM_SNMP_PRIV_NONE) {
	    ComputeKey(&session->usmPrivKey, session->privPassWord,
		       session->engineID, authProto);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TnmSnmpLocalizeKey --
 *
 *	This procedure computes a localized key from a given key and
 *	engineID, either using MD5 or SHA.
 *
 * Results:
 *	The localized key is returned in localAuthKey.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TnmSnmpLocalizeKey(algorithm, authKey, engineID, localAuthKey)
    int algorithm;
    Tcl_Obj *authKey;
    Tcl_Obj *engineID;
    Tcl_Obj *localAuthKey;
{
    char *engineBytes, *authKeyBytes;
    int engineLength, authKeyLength, localAuthKeyLength = 20;
    char localAuthKeyBytes[20]; /* must be big enough for MD5 and SHA */

    authKeyBytes = Tcl_GetStringFromObj(authKey, &authKeyLength);
    engineBytes = Tcl_GetStringFromObj(engineID, &engineLength);

    /*
     * Localize a key as described in section 2.6 of RFC 2274.
     */

    switch (algorithm) {
    case TNM_SNMP_AUTH_MD5: {
	MD5_CTX MD;
	TnmMD5Init(&MD);
	TnmMD5Update(&MD, authKeyBytes, authKeyLength);
	TnmMD5Update(&MD, engineBytes, engineLength);
	TnmMD5Update(&MD, authKeyBytes, authKeyLength);
	Tcl_SetObjLength(localAuthKey, 16);
	TnmMD5Final(localAuthKeyBytes, &MD);
	break;
    }
    case TNM_SNMP_AUTH_SHA: {
	SHA_CTX SH;
	TnmSHAInit(&SH);
	TnmSHAUpdate(&SH, authKeyBytes, authKeyLength);
	TnmSHAUpdate(&SH, engineBytes, engineLength);
	TnmSHAUpdate(&SH, authKeyBytes, authKeyLength);
	TnmSHAFinal(localAuthKeyBytes, &SH);
	break;
    }
    default:
	Tcl_Panic("unknown algorithm for key localization");
    }

    Tcl_SetStringObj(localAuthKey, localAuthKeyBytes, localAuthKeyLength);
}

static void
MD5AuthOutMsg(authKey, msg, msgLen, msgAuthenticationParameters)
    char *authKey;
    u_char *msg;
    int msgLen;
    u_char *msgAuthenticationParameters;
{
    MD5_CTX MD;
    char extendedAuthKey[64];
    char digest[16];
    int i;

    memset(msgAuthenticationParameters, 0, 12);
    memcpy(extendedAuthKey, authKey, 16);

    for (i = 1; i < 64; i++) {
	extendedAuthKey[i] = extendedAuthKey[i] ^ 0x36;
    }
    
    TnmMD5Init(&MD);
    TnmMD5Update(&MD, extendedAuthKey, 64);
    TnmMD5Update(&MD, msg, msgLen);
    TnmMD5Final(digest, &MD);

    for (i = 0; i < 64; i++) {
	extendedAuthKey[i] = extendedAuthKey[i] ^ 0x5c;
    }

    TnmMD5Init(&MD);
    TnmMD5Update(&MD, extendedAuthKey, 64);
    TnmMD5Update(&MD, digest, 16);
    TnmMD5Final(digest, &MD);

    memcpy(msgAuthenticationParameters, digest, 12);
}

void
TnmSnmpAuthOutMsg(algorithm, authKey, msg, msgLen, msgAuthenticationParameters)
    int algorithm;
    Tcl_Obj *authKey;
    u_char *msg;
    int msgLen;
    u_char *msgAuthenticationParameters;
{
    char *keyBytes;
    int keyLen;
    
    keyBytes = Tcl_GetStringFromObj(authKey, &keyLen);

    switch (algorithm) {
    case TNM_SNMP_AUTH_MD5:
	if (keyLen != 16) {
	    Tcl_Panic("illegal length of the MD5 authentication key");
	}
	MD5AuthOutMsg(keyBytes, msg, msgLen, msgAuthenticationParameters);
	break;
    default:
        Tcl_Panic("unknown authentication algorithm");
    }
}
