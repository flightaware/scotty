/*
 * tnm.h --
 *
 *	Common definitions for the Tnm Tcl extension.
 *
 * Copyright (c) 1993-1996 Technical University of Braunschweig.
 * Copyright (c) 1996-1997 University of Twente.
 * Copyright (c) 1997-2001 Technical University of Braunschweig.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef _TNM
#define _TNM

/*
 *----------------------------------------------------------------
 * The following definitions set up the proper options for Windows
 * compilers.  We use this method because there is no autoconf
 * equivalent. This is mostly copied from the tcl.h header file.
 *----------------------------------------------------------------
 */

#if defined(_WIN32) && !defined(__WIN32__)
#   define __WIN32__
#endif

#ifdef __WIN32__
#   ifndef STRICT
#	define STRICT
#   endif
#   ifndef USE_PROTOTYPE
#	define USE_PROTOTYPE 1
#   endif
#   ifndef HAS_STDARG
#	define HAS_STDARG 1
#   endif
#   ifndef USE_PROTOTYPE
#	define USE_PROTOTYPE 1
#   endif
#   ifndef USE_TCLALLOC
#	define USE_TCLALLOC 1
#   endif
#endif /* __WIN32__ */

/*
 * Macro to use instead of "void" for arguments that must have
 * type "void *" in ANSI C;  maps them to type "char *" in
 * non-ANSI systems.
 */

#ifndef __WIN32__
#ifndef VOID
#   ifdef __STDC__
#       define VOID void
#   else
#       define VOID char
#   endif
#endif
#else /* __WIN32__ */
/*
 * The following code is copied from winnt.h
 */
#ifndef __MINGW32__
#ifndef VOID
#define VOID void
typedef char CHAR;
typedef short SHORT;
typedef long LONG;
#endif
#endif
#endif /* __WIN32__ */

/*
 *----------------------------------------------------------------
 * Here start the common definitions for the Tnm extension:
 *----------------------------------------------------------------
 */

#define TNM_VERSION "3.0.4"
#ifndef TKI_VERSION
#define TKI_VERSION "1.5.1"
#endif

#ifndef TNM_URL
#define TNM_URL	"https://github.com/jorge-leon/scotty"
#endif

#include <tcl.h>

/*
 * The support follows the convention that a macro called BUILD_xxxx, where
 * xxxx is the name of a library we are building, is set on the compile line
 * for sources that are to be placed in the library.
 */

#ifdef TCL_STORAGE_CLASS
# undef TCL_STORAGE_CLASS
#endif
#ifdef BUILD_tnm
# define TCL_STORAGE_CLASS DLLEXPORT
#else
# define TCL_STORAGE_CLASS DLLIMPORT
#endif

/*
 *----------------------------------------------------------------
 * Tcl command procedures provided by the Tnm extension:
 *----------------------------------------------------------------
 */

EXTERN int
Tnm_MapObjCmd	(ClientData clientData, Tcl_Interp *interp,
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_JobObjCmd	(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_MibObjCmd	(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_NetdbObjCmd	(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_SnmpObjCmd	(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_IcmpObjCmd	(ClientData clientData, Tcl_Interp *interp,
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_SyslogObjCmd (ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_UdpObjCmd	(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_DnsObjCmd	(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_NtpObjCmd	(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_SunrpcObjCmd (ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_InedObjCmd	(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_SmxObjCmd	(ClientData clientData, Tcl_Interp *interp,
			     int objc, Tcl_Obj *const objv[]);
EXTERN int
Tnm_SmiObjCmd	(ClientData clientData, Tcl_Interp *interp, 
			     int objc, Tcl_Obj *const objv[]);

#endif /* _TNM */
