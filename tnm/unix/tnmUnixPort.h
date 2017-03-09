/*
 * tnmUnixPort.h --
 *
 *	This header file handles porting issues that occur because
 *	of differences between systems.  It reads in UNIX-related
 *	header files and sets up UNIX-related macros for scotty's UNIX
 *	core.  It should be the only file that contains #ifdefs to
 *	handle different flavors of UNIX.  This file sets up the
 *	union of all UNIX-related things needed by any of the scotty
 *	core files.  This file depends on configuration #defines such
 *	as HAVE_STDLIB_H that are set up by the "configure" script.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-1998 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TNMUNIXPORT
#define _TNMUNIXPORT

#include <config.h>

/*
 * The default directory name where we will find the tnm library
 * files. This is normally overwritten in the Makefile.
 */

#ifndef TNMLIB
#define TNMLIB "/usr/local/lib/tnm"
#endif

/*
 *----------------------------------------------------------------
 * UNIX related includes.
 *----------------------------------------------------------------
 */

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

/* malloc.h is deprecated, stdlib.h declares the malloc function  */
#ifndef HAVE_STDLIB_H
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <time.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifdef __osf__
#include <machine/endian.h>
#endif

/*
 *----------------------------------------------------------------
 * FreeBSD and Ultrix defines INADDR_LOOPBACK only in rpc/types.h.
 * That's why we have to provide the fall-through definition below.
 *----------------------------------------------------------------
 */

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK		(u_long) 0x7F000001
#endif

/*
 *----------------------------------------------------------------
 * The following defines are needed to handle UDP sockets in a 
 * platform independent way.
 *----------------------------------------------------------------
 */

#define TNM_SOCKET_FD	TCL_UNIX_FD

/*
 *----------------------------------------------------------------
 * Definitions for UNIX-only features.
 *----------------------------------------------------------------
 */

EXTERN int
TnmSmxInit		(Tcl_Interp *interp);

#endif /* _TNMUNIXPORT */
