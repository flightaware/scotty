# Makefile.vc
#
# This makefile builds tnm.dll as a dynamically loadable Tcl extension.
# This makefile is suitable for use with Microsoft Visual C++ 2.x and 4.0
# and is derived from the example provided by Sun Microsystems.
#

TNM_VERSION = 3.1.3
TNM_DLL_VERSION = 313

TKI_VERSION = 1.6.0
TKI_DLL_VERSION = 160

TCL_DLL_VERSION = 86
TK_DLL_VERSION = 86

TCL_SH_VERSION = 86

#
# Project directories -- these may need to be customized for your site
#
# ROOT --	top of the source tree.
# TMPDIR --	location for .obj files.
# TOOLS32 --	location of VC++ compiler installation.
# TCL --	location where Tcl is installed.
# TNM --	location where Tnm will be installed.
# TKI --	location where Tkined will be installed.
#

ROOT	= .\..
TMPDIR	= .
TOOLS32	= C:\Progra~1\Micros~4\VC98
TCLSRC = D:\develop\src\tcl8.6.8
TCL	= D:\develop\tcl8.6.8
TCLP	= D:/develop/tcl8.6.8
TNM	= $(TCL)\lib\tnm$(TNM_VERSION)
TNMP	= $(TCLP)/lib/tnm$(TNM_VERSION)
TKI	= $(TCL)\lib\tkined$(TKI_VERSION)
TKIP	= $(TCLP)/lib/tkined$(TKI_VERSION)

# comment the following line to compile with symbols
#NODEBUG=1

#
# Visual C++ tools
#

PATH=$(TOOLS32)\bin;$(PATH)

cc32	= $(TOOLS32)\bin\cl -I$(TOOLS32)\include
CP      = copy
RM      = del

INCLUDES = -I $(ROOT)\tnm\generic -I$(ROOT)\tnm\snmp -I$(ROOT)\compat -I$(ROOT)\compat\res -I$(ROOT)\tkined\generic -I$(TOOLS32)\include -I$(TCL)\include -I$(ROOT)\win -I$(TCLSRC)\generic -I$(TCLSRC)\win
DEFINES = -nologo $(DEBUGDEFINES) -Di386 -DBUILD_tnm -DTKINEDLIB=\"$(TKIP)\" -DTNMLIB=\"$(TNMP)\" -DUSE_TCL_STUBS=1 -DUSE_TK_STUBS=1
#DEFINES = -nologo $(DEBUGDEFINES) -Di386 -DBUILD_tnm -DTKINEDLIB=\"$(TKIP)\" -DTNMLIB=\"$(TNMP)\"

!include <ntwin32.mak>

#
# BoundsChecker tools -- uncomment only to run BoundsChecker!
#

#BCPRONT	= c:\BCProNT
#PATH=$(BCPRONT);$(PATH)
#cc32	= $(BCPRONT)\BCOMPILE
#link	= $(BCPRONT)\BCLINK

# 
# Global makefile settings
#

TNMOBJS = \
	$(TMPDIR)\tnmDns.obj \
	$(TMPDIR)\tnmIcmp.obj \
	$(TMPDIR)\tnmIned.obj \
	$(TMPDIR)\tnmInit.obj \
	$(TMPDIR)\tnmJob.obj \
	$(TMPDIR)\tnmMap.obj \
	$(TMPDIR)\tnmMapUtil.obj \
	$(TMPDIR)\tnmMapEvent.obj \
	$(TMPDIR)\tnmMapNode.obj \
	$(TMPDIR)\tnmMapNet.obj \
	$(TMPDIR)\tnmMapLink.obj \
	$(TMPDIR)\tnmMapPort.obj \
	$(TMPDIR)\tnmMapGroup.obj \
	$(TMPDIR)\tnmNetdb.obj \
	$(TMPDIR)\tnmNtp.obj \
	$(TMPDIR)\tnmSunrpc.obj \
	$(TMPDIR)\tnmSyslog.obj \
	$(TMPDIR)\tnmUdp.obj \
	$(TMPDIR)\tnmUtil.obj \
	$(TMPDIR)\tnmAsn1.obj \
	$(TMPDIR)\tnmMD5.obj \
	$(TMPDIR)\tnmOidObj.obj \
	$(TMPDIR)\tnmObj.obj \
	$(TMPDIR)\tnmSHA.obj \
	$(TMPDIR)\tnmSmx.obj \
	$(TMPDIR)\tnmSnmpAgent.obj \
	$(TMPDIR)\tnmSnmpInst.obj \
	$(TMPDIR)\tnmSnmpNet.obj \
	$(TMPDIR)\tnmSnmpRecv.obj \
	$(TMPDIR)\tnmSnmpSend.obj \
	$(TMPDIR)\tnmSnmpTcl.obj \
	$(TMPDIR)\tnmSnmpUsm.obj \
	$(TMPDIR)\tnmSnmpUtil.obj \
	$(TMPDIR)\tnmMibFrozen.obj \
	$(TMPDIR)\tnmMibParser.obj \
	$(TMPDIR)\tnmMibUtil.obj \
	$(TMPDIR)\tnmMibTcl.obj \
	$(TMPDIR)\tnmMibTree.obj \
	$(TMPDIR)\tnmWinIcmp.obj \
	$(TMPDIR)\tnmWinInit.obj \
	$(TMPDIR)\tnmWinLog.obj \
	$(TMPDIR)\tnmWinSocket.obj \
	$(TMPDIR)\ether_xdr.obj \
	$(TMPDIR)\ether_clnt.obj \
	$(TMPDIR)\mount_xdr.obj \
	$(TMPDIR)\mount_clnt.obj \
	$(TMPDIR)\rstat_xdr.obj \
	$(TMPDIR)\rstat_clnt.obj \
	$(TMPDIR)\pcnfsd_clnt.obj \
	$(TMPDIR)\pcnfsd_xdr.obj

TKIOBJS = \
	$(TMPDIR)\tkiEditor.obj \
	$(TMPDIR)\tkiObjects.obj \
	$(TMPDIR)\tkiMethods.obj \
	$(TMPDIR)\tkiMisc.obj \
	$(TMPDIR)\tkiFlash.obj \
	$(TMPDIR)\tkiStripchart.obj \
	$(TMPDIR)\tkiBarchart.obj \
	$(TMPDIR)\tkiUtil.obj \
	$(TMPDIR)\tkiInit.obj \
	$(TMPDIR)\tkiWinInit.obj

# Targets

all: tnm.dll tkined.dll

tnm.dll: $(TNMOBJS) oncrpc.lib resolv.lib
	$(link) $(linkdebug) $(dlllflags) \
		$(TCLP)/lib/tclstub$(TCL_DLL_VERSION).lib \
		oncrpc.lib resolv.lib wsock32.lib \
		$(guilibsdll) -out:tnm.dll $(TNMOBJS)

tkined.dll: $(TKIOBJS)
	$(link) $(linkdebug) $(dlllflags) \
		$(TCLP)/lib/tclstub$(TCL_DLL_VERSION).lib \
		$(TCLP)/lib/tkstub$(TK_DLL_VERSION).lib $(guilibsdll) \
		-out:tkined.dll $(TKIOBJS)

# Implicit Targets

.c.obj:
	$(cc32) $(cdebug) $(cflags) $(cvarsdll) $(INCLUDES) \
		$(DEFINES) -Fo$(TMPDIR)\ $<
{$(ROOT)\tnm\generic}.c{$(TMPDIR)}.obj:
	$(cc32) $(cdebug) $(cflags) $(cvarsdll) $(INCLUDES) \
		$(DEFINES) -Fo$(TMPDIR)\ $<
{$(ROOT)\tnm\snmp}.c{$(TMPDIR)}.obj:
	$(cc32) $(cdebug) $(cflags) $(cvarsdll) $(INCLUDES) \
		$(DEFINES) -Fo$(TMPDIR)\ $<
{$(ROOT)\compat}.c{$(TMPDIR)}.obj:
	$(cc32) $(cdebug) $(cflags) $(cvarsdll) $(INCLUDES) \
		$(DEFINES) -Fo$(TMPDIR)\ $<
{$(ROOT)\tkined\generic}.c{$(TMPDIR)}.obj:
	$(cc32) $(cdebug) $(cflags) $(cvarsdll) $(INCLUDES) \
		$(DEFINES) -Fo$(TMPDIR)\ $<

#
# Lots of installation targets.
#

install: tnm-install tki-install

tnm-install: tnm-install-bin tnm-install-library tnm-install-mibs tnm-install-agents tnm-install-examples

tnm-install-bin: tnm.dll
	-mkdir $(TNM)
	copy tnm.dll $(TNM)\tnm$(TNM_DLL_VERSION).dll
	echo package ifneeded Tnm $(TNM_VERSION) [list load [file join $$dir tnm$(TNM_DLL_VERSION).dll]] > $(TNM)\pkgIndex.tcl

tnm-install-library:
	-mkdir $(TNM)\library
	copy $(ROOT)\tnm\library\* $(TNM)\library

tnm-install-mibs:
	-mkdir $(TNM)\mibs
	copy $(ROOT)\tnm\mibs\*-SMI $(TNM)\mibs
        copy $(ROOT)\tnm\mibs\*-TC  $(TNM)\mibs
        copy $(ROOT)\tnm\mibs\*-TM  $(TNM)\mibs
        copy $(ROOT)\tnm\mibs\*-MIB $(TNM)\mibs
        copy $(ROOT)\tnm\mibs\*-CONF $(TNM)\mibs
        copy $(ROOT)\tnm\mibs\*-TYPES $(TNM)\mibs
!if exist($(ROOT)\tnm\mibs\*-COMPAT)
	copy $(ROOT)\tnm\mibs\*-COMPAT $(TNM)\mibs
!endif

tnm-install-agents:
	-mkdir $(TNM)\agents
	copy $(ROOT)\tnm\agents\* $(TNM)\agents

tnm-install-examples:
	-mkdir $(TNM)\examples
	copy $(ROOT)\tnm\examples\* $(TNM)\examples

tki-install: tki-install-bin tki-install-library tki-install-bitmaps tki-install-apps

tki-install-bin: tkined.dll
	-mkdir $(TKI)
	copy tkined.dll $(TKI)\tkined$(TKI_DLL_VERSION).dll
	copy tkined $(TCL)\bin\tkined$(TKI_DLL_VERSION).tcl
	copy $(ROOT)\tkined\tkined.defaults $(TKI)
	echo package ifneeded Tkined $(TKI_VERSION) [load [file join $$dir tkined$(TKI_DLL_VERSION).dll]] > $(TKI)\pkgIndex.tcl

tki-install-library:
	-mkdir $(TKI)\library
	copy $(ROOT)\tkined\library\* $(TKI)\library

tki-install-bitmaps:
	-mkdir $(TKI)\bitmaps
	copy $(ROOT)\tkined\bitmaps\* $(TKI)\bitmaps

tki-install-apps:
	-mkdir $(TKI)\apps
	copy $(ROOT)\tkined\apps $(TKI)\apps

check: tnm-check

tnm-check:
	cd $(ROOT)\tnm\tests
	$(TCL)\bin\tclsh$(TCL_SH_VERSION) all

# Note: we delete explicit .lib files rather than 
# *.lib so we don't delete onrpc.lib and resolv.lib
clean:
	-$(RM) $(TMPDIR)\*.obj
	-$(RM) $(TMPDIR)\tnm.dll
	-$(RM) $(TMPDIR)\tkined.dll
	-$(RM) $(TMPDIR)\tkined.lib $(TMPDIR)\tnm.lib
	-$(RM) $(TMPDIR)\*.exp
	-$(RM) $(TMPDIR)\bcc32.cfg

