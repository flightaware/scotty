/*
 * tnmPort.h --
 *
 *	This header file handles porting issues that occur because
 *	of differences between systems.  It reads in platform specific
 *	portability files.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TNMPORT
#define _TNMPORT

#if defined(__WIN32__) || defined(_WIN32)
#   include "../../win/tnmWinPort.h"
#else
#   include "../../unix/tnmUnixPort.h"
#endif

#endif /* _TNMPORT */
