/* 
 * tkiUnixPort.h --
 *
 *	This header file handles porting issues that occur because
 *	of differences between systems.  It reads in UNIX-related
 *	header files and sets up UNIX-related macros for tkined's UNIX
 *	core.  It should be the only file that contains #ifdefs to
 *	handle different flavors of UNIX.  This file sets up the
 *	union of all UNIX-related things needed by any of the tkined
 *	core files.  This file depends on configuration #defines such
 *	as NO_DIRENT_H that are set up by the "configure" script.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TKIUNIXPORT
#define _TKIUNIXPORT

#include <config.h>

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_MALLOC_H
#include <malloc.h>
#endif

#include <pwd.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/time.h>

/*
 * Define some default places where we expect our own files.
 */

#ifndef TKINEDLIB
#define TKINEDLIB "/usr/local/lib/tkined"
#endif

#endif /* TKIUNIXPORT */
