/*
 * tkiWinPort.h --
 *
 *	This header file handles porting issues that occur because of
 *	differences between Windows and Unix. It should be the only
 *	file that contains #ifdefs to handle different flavors of OS.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TKIWINPORT
#define _TKIWINPORT

/*
 * The default directory name where we will find the tkined library
 * files. This is normally overwritten in the Makefile.
 */

#ifndef TKINEDLIB
#define TKINEDLIB "c:/tcl/lib/tkined1.5.0"
#endif

/*
 *----------------------------------------------------------------
 * Windows related defines and includes.
 *----------------------------------------------------------------
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>

#include <io.h>
#include <direct.h>

/*
 *----------------------------------------------------------------
 * Windows does not define the access modes - we add them here.
 *----------------------------------------------------------------
 */

#ifndef F_OK
#define F_OK 00
#endif
#ifndef X_OK
#define X_OK 01
#endif
#ifndef W_OK
#define W_OK 02
#endif
#ifndef R_OK
#define R_OK 04
#endif

/*
 *----------------------------------------------------------------
 * Define the function strcasecmp() because Windows system uses
 * a different name for it.
 *----------------------------------------------------------------
 */

#define strcasecmp strcmpi

#endif /* _TKIWINPORT */
