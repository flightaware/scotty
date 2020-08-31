non-generated files containing version strings that need updating:

./tkined/configure.ac:AC_INIT([Tkined], [1.6.0])
./tkined/configure.ac:TKI_VERSION=1.6.0
./tkined/generic/tkined.h:#define TKI_VERSION "1.6.0"
./tkined/library/Command.tcl:package provide TkinedCommand 1.6.0
./tkined/library/Diagram.tcl:package provide TkinedDiagram 1.6.0
./tkined/library/Dialog.tcl:package provide TkinedDialog 1.6.0
./tkined/library/Editor.tcl:package provide TkinedEditor 1.6.0
./tkined/library/Event.tcl:package provide TkinedEvent 1.6.0
./tkined/library/Help.tcl:package provide TkinedHelp 1.6.0
./tkined/library/Misc.tcl:package provide TkinedMisc 1.6.0
./tkined/library/Objects.tcl:package provide TkinedObjects 1.6.0
./tkined/library/pkgIndex.tcl:package ifneeded TkinedCommand 1.6.0 [list source [file join $dir Command.tcl]]
./tkined/library/pkgIndex.tcl:package ifneeded TkinedDiagram 1.6.0 [list source [file join $dir Diagram.tcl]]
./tkined/library/pkgIndex.tcl:package ifneeded TkinedDialog 1.6.0 [list source [file join $dir Dialog.tcl]]
./tkined/library/pkgIndex.tcl:package ifneeded TkinedEditor 1.6.0 [list source [file join $dir Editor.tcl]]
./tkined/library/pkgIndex.tcl:package ifneeded TkinedEvent 1.6.0 [list source [file join $dir Event.tcl]]
./tkined/library/pkgIndex.tcl:package ifneeded TkinedHelp 1.6.0 [list source [file join $dir Help.tcl]]
./tkined/library/pkgIndex.tcl:package ifneeded TkinedMisc 1.6.0 [list source [file join $dir Misc.tcl]]
./tkined/library/pkgIndex.tcl:package ifneeded TkinedObjects 1.6.0 [list source [file join $dir Objects.tcl]]
./tkined/library/pkgIndex.tcl:package ifneeded TkinedTool 1.6.0 [list source [file join $dir Tool.tcl]]
./tkined/library/Tool.tcl:package provide TkinedTool 1.6.0
./tkined/unix/configure.in:TKI_VERSION=1.6.0
./tkined/unix/configure.in:TNM_VERSION=3.1.3
./tkined/win/makefile:TKI_VERSION = 1.6.0
./tkined/win/makefile:TNM_VERSION = 3.1.3
./tnm/configure.ac:AC_INIT([Tnm], [3.1.3])
./tnm/configure.ac:TKI_VERSION=1.6.0
./tnm/configure.ac:TNM_VERSION=3.1.3
./tnm/configure:TKI_VERSION=1.6.0
./tnm/generic/tnm.h:#define TKI_VERSION "1.6.0"
./tnm/generic/tnm.h:#define TNM_VERSION "3.1.3"
./tnm/library/pkgIndex.tcl:    package ifneeded $pkg 3.1.3 [list source [file join $dir $pkg.tcl]]
./tnm/library/TnmDialog.tcl:package provide TnmDialog 3.1.3
./tnm/library/TnmEther.tcl:package provide TnmEther 3.1.3
./tnm/library/TnmIetf.tcl:package provide TnmIetf 3.1.3
./tnm/library/TnmInet.tcl:package provide TnmInet 3.1.3
./tnm/library/TnmMap.tcl:package provide TnmMap 3.1.3
./tnm/library/TnmMib.tcl:package provide TnmMib 3.1.3
./tnm/library/TnmMonitor.tcl:package provide TnmMonitor 3.1.3
./tnm/library/TnmScriptMib.tcl:package provide TnmScriptMib 3.1.3
./tnm/library/TnmSmxProfiles.tcl:package provide TnmSmxProfiles 3.1.3
./tnm/library/TnmSnmp.tcl:package provide TnmSnmp 3.1.3
./tnm/library/TnmTerm.tcl:package provide TnmTerm 3.1.3
./tnm/Makefile:TKI_VERSION  =	1.6.0
./tnm/unix/configure.in:TKI_VERSION=1.6.0
./tnm/unix/configure.in:TNM_VERSION=3.1.3
./tnm/win/makefile:TKI_VERSION = 1.6.0
./tnm/win/makefile:TNM_VERSION = 3.1.3
./tnm/win/tnmWinPort.h:#define TKINEDLIB "c:/tcl/lib/tkined1.6.0"
./tnm/win/tnmWinPort.h:#define TNMLIB "c:/tcl/lib/tnm3.1.3"
