/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Handcrafted version -- autoheader is too magic.  */

/* The size of a variable of type int in bytes. */
#define SIZEOF_INT 4

/* The size of a variable of type unsigned int in bytes. */
#define SIZEOF_UNSIGNED_INT 4

/* The size of a variable of type long in bytes. */
#define SIZEOF_LONG 8

/* The size of a variable of type unsigned long in bytes. */
#define SIZEOF_UNSIGNED_LONG 8

/* The size of a variable of type long long in bytes. */
#define SIZEOF_LONG_LONG 8

/* The size of a variable of type unsigned long long in bytes. */
#define SIZEOF_UNSIGNED_LONG_LONG 8

/* Define if your processor stores words with the most significant
   byte first (like Motorola and SPARC, unlike Intel and VAX).  */
/* #undef WORDS_BIGENDIAN */

/* Define if you have IP multicast support.  */
#define HAVE_MULTICAST 1

/* Define if you have the getmntent function.  */
/* #undef HAVE_GETMNTENT */

/* Define if you have the gethostent function.  */
#define HAVE_GETHOSTENT 1

/* Define if you have the getnetent function.  */
#define HAVE_GETNETENT 1

/* Define if you have the getprotoent function.  */
#define HAVE_GETPROTOENT 1

/* Define if you have the getservent function.  */
#define HAVE_GETSERVENT 1

/* Define if you have the getrpcent function.  */
#define HAVE_GETRPCENT 1

/* Define if you have the strerror function.  */
#define HAVE_STRERROR 1

/* Define if we have struct rpcent.  */
#define HAVE_RPCENT 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <malloc.h> header file.  */
/* #undef HAVE_MALLOC_H */

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <zlib.h> header file.  */
#define HAVE_ZLIB_H 1

/* Define if you have the <smi.h> header file.  */
/* #undef HAVE_SMI_H */

/* Define if you want to debug the USEC code. */
/* #undef DEBUG_USEC */

/* Define if you want to benchmark SNMP agents. */
/* #undef SNMP_BENCH */

/* Define if you do not have a definition for int16_t */
/* #undef int16_t */

/* Define if you do not have a definition for uint16_t */
/* #undef uint16_t */

/* Define if you do not have a definition for int32_t */
/* #undef int32_t */

/* Define if you do not have a definition for uint32_t */
/* #undef uint32_t */

/* Define if you do not have a definition for ipaddr_t */
#define ipaddr_t uint32_t

/* Define if you do have inet_pton */
#define HAVE_INET_PTON 1

/* Define if you do have inet_ntop */
#define HAVE_INET_NTOP 1

/* Define if you do have getaddrinfo */
#define HAVE_GETADDRINFO 1

/* Define if you do have getnameinfo */
#define HAVE_GETNAMEINFO 1

/* Define if you do have socklen_t type */
#define HAVE_SOCKLEN_T 1

/* Define if you do have sin_len in struct sockaddr */
#define HAVE_SA_LEN 1
