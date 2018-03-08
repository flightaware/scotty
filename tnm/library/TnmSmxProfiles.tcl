# TnmSmxProfiles.tcl --
#
#	This file implements some core SMX runtime security profiles.
#
# Copyright (c) 2000-2001 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require Tnm 3.0

package provide TnmSmxProfiles 3.0.4

namespace eval TnmSmxProfiles {
    namespace export safe tnm snmp snmp-134.169
}

# TnmSmxProfiles::safe --
#
#	The safe runtime security profile. This is trivial 
#	the default slave interpreter is already a safe one.
#
# Arguments:
#	slave	The slave Tcl interpreter.
#
# Results:
#	None.

proc TnmSmxProfiles::safe {slave} {
    # nothing to be done here...
    return
}

# TnmSmxProfiles::tnm --
#
#	The tnm runtime security profile supports all commands
#	provided by the Tnm extension. This is not a very safe
#	profile since it allows scripts arbitrary use of the
#	network. Furthermore, scripts can issue syslog messages
#	and disturb the system in other unpleasant ways.
#
# Arguments:
#	slave	The slave Tcl interpreter.
#
# Results:
#	None.

proc TnmSmxProfiles::tnm {slave} {
    # Expose the snmp and mib commands and rename them into the
    # Tnm namespace. (Note that the hidden commands exist in the
    # global namespace due to a Tcl limitation.
    foreach qualcmd [info commands ::Tnm::*] {
	set cmd [namespace tail $qualcmd]
	if {[lsearch [$slave hidden] $cmd] >= 0} {
	    $slave expose $cmd
	    $slave eval rename $cmd $qualcmd
	}
    }
    return
}

# TnmSmxProfiles::snmp --
#
#	The snmp runtime security profile. This is a safe
#	profile which allows to use the Tnm SNMP engine.
#
# Arguments:
#	slave	The slave Tcl interpreter.
#
# Results:
#	None.

proc TnmSmxProfiles::snmp {slave} {
    # Expose the snmp and mib commands and rename them into the
    # Tnm namespace. (Note that the hidden commands exist in the
    # global namespace due to a Tcl limitation.
    foreach qualcmd {::Tnm::snmp ::Tnm::mib} {
	set cmd [namespace tail $qualcmd]
	$slave expose $cmd
	$slave eval rename $cmd $qualcmd
    }
    return
}

# TnmSmxProfiles::snmp-134.169 --
#
#	The snmp runtime security profile which allows SNMP
#	access to the IPv4 network 134.169.*.*.
#
# Arguments:
#	slave	The slave Tcl interpreter.
#
# Results:
#	None.

proc TnmSmxProfiles::snmp-134.169 {slave} {
    # Expose the snmp and mib commands and rename them into the
    # Tnm namespace. (Note that the hidden commands exist in the
    # global namespace due to a Tcl limitation.
    foreach qualcmd {::Tnm::mib} {
	set cmd [namespace tail $qualcmd]
	$slave expose $cmd
	$slave eval rename $cmd $qualcmd
    }
    $slave alias Tnm::snmp \
	    TnmSmxProfiles::SnmpCheckAddress $slave {^134.169.[0-9]+.[0-9]+}
    return
}

proc TnmSmxProfiles::SnmpCheckAddress {slave pattern args} {
    set option [lindex $args 0]
    switch $option {
	responder -
	listener {
	    return -code error "access to $option sessions denied"
	}
	notifier -
	generator {
	    set s [$slave invokehidden snmp $option]
	    $slave hide $s
	    foreach {option value} [lrange $args 1 end] {
		if {$option == "-address" && ![regexp $pattern $value]} {
		    $slave invokehidden $s destroy
		    return -code error "access outside $pattern denied"
		}
		if {[catch {$slave invokehidden $s configure $option $value} msg]} {
		    return -code error $msg
		}
	    }
	    $slave alias $s \
		    TnmSmxProfiles::SnmpSessionCheckAddress $slave $pattern $s
	    return $s
	}
	default {
	    # xxx how do we expand $args into multiple arguments?
	    # xxx how do we best handle any runtime errors?
	    return [$slave invokehidden snmp $args]
	}
    }
}

proc TnmSmxProfiles::SnmpSessionCheckAddress {slave pattern s args} {
    set option [lindex $args 0]
    switch $option {
	configure {
	    foreach {option value} [lrange $args 1 end] {
		if {$option == "-address" && ! [regexp $pattern $value]} {
		    return -code error "access outside $pattern denied"
		}
		if {[catch {$slave invokehidden $s configure $option $value} msg]} {
		    return -code error $msg
		}
	    }
	    return [$slave invokehidden $s configure]
	}
	destroy {
	    $slave invokehidden $s destroy
	    $slave alias $s {}
	}
	default {
	    # xxx how do we expand $args into multiple arguments?
	    # xxx how do we best handle any runtime errors?
	    return [$slave invokehidden $s $args]
	}
    }
}

# Make the list of SMX profiles defined in this package known
# to the SMX protocol implementation.

Tnm::smx profiles [list safe snmp tnm snmp-134.169]
