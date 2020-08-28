#!/bin/bash -norc
dnl	This file is an input file used by the GNU "autoconf" program to
dnl	generate the file "configure", which is run during Tcl installation
dnl	to configure the system for the local environment.

#-----------------------------------------------------------------------
# configure.ac for the Tnm Tcl Extension.
# The only places you should  need to modify this file are marked by
# the string __CHANGE__
#-----------------------------------------------------------------------

#-----------------------------------------------------------------------
# __CHANGE__
# Set your package name and version numbers here.
#
# This initializes the environment with PACKAGE_NAME and PACKAGE_VERSION
# set as provided.  These will also be added as -D defs in your Makefile
# so you can encode the package version directly into the source files.
# This will also define a special symbol for Windows (BUILD_<PACKAGE_NAME>
# so that we create the export library with the dll.
#-----------------------------------------------------------------------

AC_INIT([Tnm], [3.1.3])

#-----------------------------------------------------------------------
# Scotty specific definitions and checks
#-----------------------------------------------------------------------

TNM_VERSION=3.1.3
TKI_VERSION=1.6.0

# disabled config.h generation, since this inhibits stub compilation
AC_CONFIG_HEADER([config.h])

#--------------------------------------------------------------------
#       Check for various system specific things.
#--------------------------------------------------------------------

AC_TYPE_SIZE_T
AC_CHECK_SIZEOF([int])
AC_CHECK_SIZEOF([unsigned int])
AC_CHECK_SIZEOF([long])
AC_CHECK_SIZEOF([unsigned long])
AC_CHECK_SIZEOF([unsigned long long])


#----------------------------------------------------------------------------
#	Check if rpcgen is available. If not, use the files in 
#	$srcdir/../compat/.
#----------------------------------------------------------------------------

AC_CHECK_PROG(RPCGEN,rpcgen,rpcgen,echo)
if test "$RPCGEN" = "echo" ; then
    AC_WARN(couldn\'t find rpcgen. Will use precompiled files)
    cp $srcdir/../compat/ether* .
    cp $srcdir/../compat/mount* .
    cp $srcdir/../compat/pcnfsd* .
    cp $srcdir/../compat/rstat* .
fi

#----------------------------------------------------------------------------
#	Check for the existence of various libraries.
#       sin, ieee, inet, net/errno.h, socket, nsl 
#----------------------------------------------------------------------------

TEA_TCL_LINK_LIBS

#----------------------------------------------------------------------------
#	Check for the existence of various libraries.
#       zlib, rpc, resolv
#----------------------------------------------------------------------------


AX_CHECK_ZLIB([LIBS="$LIBS -lz"],no)

# See Alex Chernyakhovsky's article:
# http://blog.achernya.com/2013/03/os-x-has-silly-libsystem.html
# We have taken his solution from here:
# https://github.com/achernya/hesiod/blob/master/configure.ac
AC_CHECK_LIB([resolv], [res_mkquery], [], [
    AC_MSG_CHECKING([if res_mkquery is provided by libresolv with mangled symbols])
    save_LIBS="$LIBS"
    LIBS="-lresolv $LIBS"
    AC_TRY_LINK([#include <resolv.h>],
                [res_mkquery(0,NULL,0,0,NULL,0,NULL,NULL,0);], [
                    AC_DEFINE(HAVE_LIBRESOLV, [1], [Define if your libresolv provides res_mkquery.])
		    AC_MSG_RESULT(yes)
		    NM_LIBS="$NM_LIBS -lresolv"
		], [
		    LIBS="$save_LIBS"
		    AC_MSG_RESULT(no)])])

#----------------------------------------------------------------------------
#	Check for new socklen_t type which is not yet universally available.
#	Check for sin_len field in struct sockaddr (BSD-ish).
#----------------------------------------------------------------------------

AC_CHECK_HEADERS([sys/socket.h])

AC_CHECK_TYPE(socklen_t,AC_DEFINE([HAVE_SOCKLEN_T], 1, [socklen_t available]),, [
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
])

AC_CACHE_CHECK([for 'sa_len' in 'struct sockaddr'], ac_cv_struct_sa_len, 
[AC_TRY_COMPILE([#include <sys/types.h> 
#include <sys/socket.h>], [struct sockaddr sa; sa.sa_len = 0; ], 
        ac_cv_struct_sa_len=yes, ac_cv_struct_sa_len=no)]) 
if test "$ac_cv_struct_sa_len" = yes; then 
  AC_DEFINE([HAVE_SA_LEN], 1, [sa_len present]) 
fi 

#----------------------------------------------------------------------------
#	Checks for various IPv6 related functions.
#----------------------------------------------------------------------------

AC_CHECK_FUNCS(inet_pton inet_ntop getaddrinfo getnameinfo)

#----------------------------------------------------------------------------
#	Checks for various include files missing on some machines.
#----------------------------------------------------------------------------

AC_CHECK_HEADERS(stdlib.h unistd.h malloc.h sys/select.h zlib.h)

#----------------------------------------------------------------------------
#       Check for various Unix library functions that can be used.
#----------------------------------------------------------------------------

AC_CHECK_FUNC(strerror, AC_DEFINE([HAVE_STRERROR], 1, [strerror function available]))
# TODO gethostent is deprecated by POSIX, instead use getaddrinfo et.al.
AC_CHECK_FUNC(gethostent, AC_DEFINE([HAVE_GETHOSTENT], 1, [gethostent function available]))
AC_CHECK_FUNC(getnetent, AC_DEFINE([HAVE_GETNETENT], 1, [getnetent function available]))
AC_CHECK_FUNC(getprotoent, AC_DEFINE([HAVE_GETPROTOENT], 1, [getprotoent function available] ))
AC_CHECK_FUNC(getservent, AC_DEFINE([HAVE_GETSERVENT], 1, [getservent function available]))
AC_CHECK_FUNC(getrpcent, AC_DEFINE([HAVE_GETRPCENT], 1, [getrpcent function available]))

#----------------------------------------------------------------------------
#       Check if we want/need to use the libtirpc alternative rpc
#       implementation.
#----------------------------------------------------------------------------

AC_ARG_WITH([tirpc], AS_HELP_STRING([--with-tirpc], [Build with the tirpc rpc implementation]))

AS_IF([test "x$with_tirpc" = "xyes"], [
	  if pkg-config libtirpc; then
	      TIRPC_CFLAGS=`pkg-config --cflags libtirpc`
	      TIRPC_LIBS=`pkg-config --libs libtirpc`
	      CPPFLAGS="$TIRPC_CFLAGS $CPPFLAGS"
	  else
	      AC_MSG_ERROR(could not determine how to configure tirpc)
	  fi
      ])

#----------------------------------------------------------------------------
#       Some older SUN RPC implementations are really ugly.
#----------------------------------------------------------------------------

AC_MSG_CHECKING(for struct rpcent)
AC_EGREP_CPP([^struct * rpcent],[
#include <netdb.h>
#include <rpc/rpc.h>
], AC_MSG_RESULT(yes); AC_DEFINE([HAVE_RPCENT], 1, [rpcent structure defined in system libraries]), AC_MSG_RESULT(no))


#----------------------------------------------------------------------------
#       Checking for multicast options.
#----------------------------------------------------------------------------

AC_ARG_ENABLE(multicast, 
	[  --enable-multicast      enable support for multicasts],
	[mcast_ok=$enableval], [mcast_ok=no])
if test "$mcast_ok" = "yes"; then
    AC_MSG_CHECKING(for multicast support)
    AC_TRY_COMPILE([#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>], [int i=IP_MULTICAST_TTL, j=IP_ADD_MEMBERSHIP;],
    AC_MSG_RESULT(yes); AC_DEFINE([HAVE_MULTICAST], 1, [multicast enabled]), AC_MSG_RESULT(no))
fi

#----------------------------------------------------------------------------
#       Checking for the libsmi library.
#----------------------------------------------------------------------------

AC_CHECK_HEADERS(smi.h)
AC_CHECK_LIB(smi, smiInit, [LIBS="$LIBS -lsmi"])
AC_MSG_CHECKING([whether to enable libsmi])
        AC_TRY_RUN([ /* libsmi available check */
#include <smi.h>
main()
{
  int current, revision, age, n;
  const int required = 2;
  if (smiInit("")) 
    exit(1);
  if (strcmp(SMI_LIBRARY_VERSION, smi_library_version))
    exit(2);
  n = sscanf(smi_library_version, "%d:%d:%d", &current, &revision, &age);
  if (n != 3)
    exit(3);
  if (required < current - age || required > current)
    exit(4);
  exit(0);
}
],
[ AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_SMI_H)
  AC_DEFINE([LIBSMI], 1, [libsmi available])
  libsmi=yes],
[ AC_MSG_RESULT(no)
  libsmi=no],
[ AC_MSG_RESULT(not when cross-compiling)
  libsmi=no]
)

#----------------------------------------------------------------------------
#       Some more libraries for OSes that want your attention.
#----------------------------------------------------------------------------

AC_FUNC_GETMNTENT

#----------------------------------------------------------------------------
#       Required programs
#----------------------------------------------------------------------------

#--------------------------------------------------------------------
# Call TEA_INIT as the first TEA_ macro to set up initial vars.
# This will define a ${TEA_PLATFORM} variable == "unix" or "windows"
# as well as PKG_LIB_FILE and PKG_STUB_LIB_FILE.
#--------------------------------------------------------------------

TEA_INIT([3.10])

AC_CONFIG_AUX_DIR(tclconfig)

#--------------------------------------------------------------------
# Load the tclConfig.sh file
#--------------------------------------------------------------------

TEA_PATH_TCLCONFIG
TEA_LOAD_TCLCONFIG

#--------------------------------------------------------------------
# Load the tkConfig.sh file if necessary (Tk extension)
#--------------------------------------------------------------------

#TEA_PATH_TKCONFIG
#TEA_LOAD_TKCONFIG

#-----------------------------------------------------------------------
# Handle the --prefix=... option by defaulting to what Tcl gave.
# Must be called after TEA_LOAD_TCLCONFIG and before TEA_SETUP_COMPILER.
#-----------------------------------------------------------------------

TEA_PREFIX

#-----------------------------------------------------------------------
# Standard compiler checks.
# This sets up CC by using the CC env var, or looks for gcc otherwise.
# This also calls AC_PROG_CC and a few others to create the basic setup
# necessary to compile executables.
#-----------------------------------------------------------------------

TEA_SETUP_COMPILER

#-----------------------------------------------------------------------
# __CHANGE__
# Specify the C source files to compile in TEA_ADD_SOURCES,
# public headers that need to be installed in TEA_ADD_HEADERS,
# stub library C source files to compile in TEA_ADD_STUB_SOURCES,
# and runtime Tcl library files in TEA_ADD_TCL_SOURCES.
# This defines PKG(_STUB)_SOURCES, PKG(_STUB)_OBJECTS, PKG_HEADERS
# and PKG_TCL_SOURCES.
#-----------------------------------------------------------------------

# generic tnm
TEA_ADD_SOURCES([
		   generic/tnmInit.c 
		   generic/tnmUtil.c 
		   generic/tnmJob.c 
		   generic/tnmIned.c 
		   generic/tnmSyslog.c 
		   generic/tnmIcmp.c 
		   generic/tnmDns.c 
		   generic/tnmUdp.c 
		   generic/tnmSunRpc.c 
		   generic/tnmNtp.c 
		   generic/tnmNetdb.c 
		   generic/tnmMap.c 
		   generic/tnmMapUtil.c 
		   generic/tnmMapEvent.c 
		   generic/tnmMapNode.c 
		   generic/tnmMapNet.c 
		   generic/tnmMapLink.c 
		   generic/tnmMapPort.c 
		   generic/tnmMapGroup.c 
		   generic/tnmObj.c 
		   generic/tnmSmx.c
	       ])
# snmp
TEA_ADD_SOURCES([
		   snmp/tnmAsn1.c 
		   snmp/tnmOidObj.c 
		   snmp/tnmMD5.c 
		   snmp/tnmSHA.c 
		   snmp/tnmSnmpNet.c 
		   snmp/tnmSnmpUtil.c 
		   snmp/tnmSnmpUsm.c 
		   snmp/tnmSnmpInst.c 
		   snmp/tnmSnmpTcl.c 
		   snmp/tnmSnmpSend.c 
		   snmp/tnmSnmpRecv.c 
		   snmp/tnmSnmpAgent.c 
		   snmp/tnmMibUtil.c 
		   snmp/tnmMibParser.c 
		   snmp/tnmMibTree.c 
		   snmp/tnmMibFrozen.c 
		   snmp/tnmMibTcl.c 
		   snmp/tnmSmiTcl.c 
	       ])
TEA_ADD_HEADERS([])
TEA_ADD_INCLUDES([${TIRPC_CFLAGS} -I. -Igeneric -Isnmp])
TEA_ADD_LIBS([
		mount_clnt.o 
		mount_xdr.o 
		ether_clnt.o 
		ether_xdr.o 
		rstat_clnt.o 
		rstat_xdr.o 
		pcnfsd_clnt.o 
		pcnfsd_xdr.o
	    ])
TEA_ADD_CFLAGS([])
TEA_ADD_STUB_SOURCES([])
# This would be nice to use, but the current setup hardcodes related
# Tcl files to reside under $(pkglibdir)/library.
TEA_ADD_TCL_SOURCES([])

#--------------------------------------------------------------------
# __CHANGE__
#
# You can add more files to clean if your extension creates any extra
# files by extending CLEANFILES.
# Add pkgIndex.tcl if it is generated in the Makefile instead of ./configure
# and change Makefile.in to move it from CONFIG_CLEAN_FILES to BINARIES var.
#
# A few miscellaneous platform-specific items:
# TEA_ADD_* any platform specific compiler/build info here.
#--------------------------------------------------------------------

CLEANFILES="$CLEANFILES nmicmpd nmtrapd"
CLEANFILES="$CLEANFILES ether_* ether.h"
CLEANFILES="$CLEANFILES mount_* mount.h"
CLEANFILES="$CLEANFILES pcnfsd_* pcnfsd.h"
CLEANFILES="$CLEANFILES rstat_* rstat.h"

if test "${TEA_PLATFORM}" = "windows" ; then
    # Ensure no empty if clauses
    :
    #TEA_ADD_SOURCES([win/winFile.c])
    #TEA_ADD_INCLUDES([-I\"$(${CYGPATH} ${srcdir}/win)\"])
else
    # Ensure no empty else clauses
    :
    TEA_ADD_SOURCES([
		       unix/tnmUnixInit.c
		       unix/tnmUnixLog.c
		       unix/tnmUnixSocket.c
		       unix/tnmUnixIcmp.c
		       unix/tnmUnixSnmp.c
		   ])
    TEA_ADD_LIBS([${TIRPC_LIBS}])
fi

#--------------------------------------------------------------------
# __CHANGE__
# Choose which headers you need.  Extension authors should try very
# hard to only rely on the Tcl public header files.  Internal headers
# contain private data structures and are subject to change without
# notice.
# This MUST be called after TEA_LOAD_TCLCONFIG / TEA_LOAD_TKCONFIG
#--------------------------------------------------------------------

TEA_PUBLIC_TCL_HEADERS
#TEA_PRIVATE_TCL_HEADERS

#TEA_PUBLIC_TK_HEADERS
#TEA_PRIVATE_TK_HEADERS
#TEA_PATH_X

#--------------------------------------------------------------------
# Check whether --enable-threads or --disable-threads was given.
# This auto-enables if Tcl was compiled threaded.
#--------------------------------------------------------------------

TEA_ENABLE_THREADS

#--------------------------------------------------------------------
# The statement below defines a collection of symbols related to
# building as a shared library instead of a static library.
#--------------------------------------------------------------------

TEA_ENABLE_SHARED

#--------------------------------------------------------------------
# This macro figures out what flags to use with the compiler/linker
# when building shared/static debug/optimized objects.  This information
# can be taken from the tclConfig.sh file, but this figures it all out.
#--------------------------------------------------------------------

TEA_CONFIG_CFLAGS

#--------------------------------------------------------------------
# Set the default compiler switches based on the --enable-symbols option.
#--------------------------------------------------------------------

TEA_ENABLE_SYMBOLS

#--------------------------------------------------------------------
# This macro generates a line to use when building a library.  It
# depends on values set by the TEA_ENABLE_SHARED, TEA_ENABLE_SYMBOLS,
# and TEA_LOAD_TCLCONFIG macros above.
#--------------------------------------------------------------------

TEA_MAKE_LIB

#--------------------------------------------------------------------
# Determine the name of the tclsh and/or wish executables in the
# Tcl and Tk build directories or the location they were installed
# into. These paths are used to support running test cases only,
# the Makefile should not be making use of these paths to generate
# a pkgIndex.tcl file or anything else at extension build time.
#--------------------------------------------------------------------

TEA_PROG_TCLSH
#TEA_PROG_WISH

#----------------------------------------------------------------------------
#       The list of symbols to substitute.
#----------------------------------------------------------------------------

AC_SUBST(TNM_VERSION)
AC_SUBST(TKI_VERSION)



AC_SUBST(RPCGEN)
AC_SUBST(NM_LIBS)

AC_SUBST(THREADS_LIBS)
AC_SUBST(LD_SEARCH_FLAGS)

#--------------------------------------------------------------------
# Finally, substitute all of the various values into the Makefile.
# You may alternatively have a special pkgIndex.tcl.in or other files
# which require substituting th AC variables in.  Include these here.
#--------------------------------------------------------------------

# For now we have a Tcl only scotty.  Remove from where when it
# gets built from scotty.c
AC_OUTPUT([Makefile pkgIndex.tcl])
