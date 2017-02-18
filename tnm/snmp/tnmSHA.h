#ifndef SHA_H
#define SHA_H

/* NIST Secure Hash Algorithm */
/* heavily modified by Uwe Hollerbach <uh@alumni.caltech edu> */
/* from Peter C. Gutmann's implementation as found in */
/* Applied Cryptography by Bruce Schneier */

/* This code is in the public domain */

/* Useful defines & typedefs */

#ifndef __MINGW32__
typedef unsigned char BYTE;	/* 8-bit quantity */
typedef unsigned long LONG;	/* 32-or-more-bit quantity */
#endif

#define SHA_BLOCKSIZE		64
#define SHA_DIGESTSIZE		20

typedef struct {
    LONG digest[5];		/* message digest */
    LONG count_lo, count_hi;	/* 64-bit bit count */
    BYTE data[SHA_BLOCKSIZE];	/* SHA data buffer */
    int local;			/* unprocessed amount in data */
} SHA_CTX;

void TnmSHAInit(SHA_CTX *);
void TnmSHAUpdate(SHA_CTX *, BYTE *, int);
void TnmSHAFinal(unsigned char [20], SHA_CTX *);

#ifdef SHA_FOR_C

#include <stdlib.h>
#include <stdio.h>

void sha_stream(unsigned char [20], SHA_INFO *, FILE *);
void sha_print(unsigned char [20]);
char *sha_version(void);
#endif /* SHA_FOR_C */

#endif /* SHA_H */
