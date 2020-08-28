# Tcl package index file, version 1.0
# This file is hand crafted so that the packages are loaded immediately.
#
# $Id: pkgIndex.tcl,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $

foreach pkg {
    TnmDialog TnmTerm TnmInet TnmMap TnmIetf TnmEther TnmMonitor
    TnmSnmp TnmMib TnmScriptMib TnmSmxProfiles
} {
    package ifneeded $pkg 3.1.3 [list source [file join $dir $pkg.tcl]]
}

