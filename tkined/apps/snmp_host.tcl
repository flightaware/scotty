#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# snmp_host.tcl -
#
#	HOST-RESOURCES-MIB (RFC 2790) specific functions for Tkined.
#
# Copyright (c) 1995-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1997-2000 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: snmp_host.tcl,v 1.1.1.1 2006/12/07 12:16:59 karl Exp $

package require Tnm 3.0
package require TnmSnmp $tnm(version)

namespace import Tnm::*

ined size
LoadDefaults snmp
SnmpInit Host-MIB

##
## Load the mib modules required by this script. This will prevent to load 
## all mib modules and reduce memory requirements and startup time.
##

mib load RFC1414-MIB
mib load HOST-RESOURCES-MIB

##
## Show the hrSystem information of MIB II.
##

proc "Host Information" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
        writeln [TnmSnmp::ShowScalars $s hrSystem]
        $s destroy
    }
}

proc "Storage Information" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	writeln "SNMP table hrStorageTable of $host \[$ip\]:"
	set txt "Index   Description            Unitsize      Total       Used   Allocated\n"
	try {
	    $s walk x "hrStorageIndex hrStorageDescr hrStorageAllocationUnits hrStorageSize hrStorageUsed" {
		set hridx   [lindex [lindex $x 0] 2]
		set hrdesc  [lindex [lindex $x 1] 2]
		set hrunits [lindex [lindex $x 2] 2]
		set hrtotal [lindex [lindex $x 3] 2]
		set hrused  [lindex [lindex $x 4] 2]
		set alloc   [expr $hrtotal ? $hrused * 100.0 / $hrtotal : 0]
		append txt  [format " %-5s  %-20s %10d %10d %10d     %5.2f %%\n" \
			$hridx $hrdesc $hrunits $hrtotal $hrused $alloc]
	    }
	} msg {
	    append txt "$msg\n"
	}
	writeln $txt
	$s destroy
    }
}

proc "Device Information" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	writeln "SNMP table hrDeviceTable of $host \[$ip\]:"
	set txt " Status  ProductID  Description\n"
	try {
	    $s walk x "hrDeviceStatus hrDeviceID hrDeviceDescr" {
		set hrstat [lindex [lindex $x 0] 2]
		set hrid   [lindex [lindex $x 1] 2]
		set hrdesc [lindex [lindex $x 2] 2]
		append txt [format "%-10s %-8s %s\n" \
			$hrstat $hrid $hrdesc]
	    }
	} msg {
	    append txt "$msg\n"
	}
	writeln $txt
	$s destroy
    }       
}

proc "Filesystem Information" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	writeln "SNMP table hrFSTable of $host \[$ip\]:"
	set txt "Index   Mountpoint           Remote                         StorageIndex\n"
	try {
	    $s walk x "hrFSIndex hrFSMountPoint hrFSRemoteMountPoint hrFSStorageIndex" {
		set fsidx   [lindex [lindex $x 0] 2]
		set fsremnt [lindex [lindex $x 1] 2]
		set fsmnt   [lindex [lindex $x 2] 2]
		set fssto   [lindex [lindex $x 3] 2]
		append txt [format " %-6s %-20s %-30s %-8s\n" \
			$fsidx $fsremnt $fsmnt $fssto]
	    }
	} msg {
	    append txt "$msg\n"
	}
	writeln $txt
	$s destroy
    }       
}


proc "Process Information" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	writeln "SNMP table hrSWRunTable of $host \[$ip\]:"
	set txt "ID         Status     Memory         CPU-Time    Command\n"
	try {
	    $s walk x "hrSWRunIndex hrSWRunStatus hrSWRunParameters hrSWRunPerfCPU hrSWRunPerfMem" {
		set pid  [lindex [lindex $x 0] 2]
		set stat [lindex [lindex $x 1] 2]
		set cmd  [lindex [lindex $x 2] 2]
		set cpu  [lindex [lindex $x 3] 2]
		set mem  [lindex [lindex $x 4] 2]
		## format cpu time to something readable:
		set cputime [format "%d:%02d.%02d" [expr $cpu / 6000] [expr ($cpu % 6000) / 100] [expr $cpu % 100]]
		append txt [format " %-8s %-10s %6sk %16s    %-20s\n" $pid $stat $mem $cputime $cmd]
	    }
	} msg {
	    append txt "$msg\n"
        }
        writeln $txt
	$s destroy
    }
}

proc "Ident Information" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
        writeln "SNMP table identTable of $host \[$ip\]:"
        set txt " Source                 Destination            User-Identity\n"
	try {
	    $s walk x "identStatus identUserid identMisc" {
		set status [lindex [lindex $x 0] 2]
                set userid [mib format sysDescr [lindex [lindex $x 1] 2]]
                set misc   [mib format sysDescr [lindex [lindex $x 2] 2]]
		set index [split [lindex [lindex $x 0] 0] .]
		set src	[join [lrange $index 11 15] .]
		set dst [join [lrange $index 16 end] .]
		if {$status != "noError"} {
		    set user ""
		} else {
		    set user "$misc ($userid)"
		}
		append txt [format " %-22s %-22s %s\n" $src $dst $user]
	    }
	} msg {
            append txt "$msg\n"
	}
	writeln $txt
        $s destroy
    }
}

##
## Set the parameters (community, timeout, retry) for snmp requests.
##

proc "Set SNMP Parameter" {list} {
    SnmpParameter $list
}

##
## Delete the menus created by this interpreter.
##

proc "Delete Host-MIB" {list} {
    global menus
    foreach id $menus { ined delete $id }
    exit
}

##
## Display some help about this tool.
##

proc "Help Host-MIB" {list} {
    ined browse "Help about Host-MIB" {
	"This script allows to read information provided by the Host" 
	"Resources MIB (RFC 1514) and the Ident MIB (RFC 1414)." 
	"" 
	"Host Information:" 
	"    Display general information provided by the host MIB." 
	"" 
	"Storage Information" 
	"    Display information about storage devices on the host." 
	"" 
	"Device Information" 
	"    Display information about devices connected to the host." 
	"" 
	"Filesystem Information" 
	"    Display information about the filesystems on this host." 
	"" 
	"Process Information" 
	"    Display information about running software on the host." 
	"" 
	"Ident Information" 
	"    Display information about the identity of TCP connections" 
	"    from/to this host." 
	"" 
	"Set SNMP Parameter:" 
	"    This dialog allows you to set SNMP parameters like retries, " 
	"    timeouts, community name and port number. " 
    }
}

set menus [ ined create MENU "Host-MIB" \
	"Host Information" \
	"Storage Information" \
	"Device Information" \
	"Filesystem Information" \
	"Process Information" "" \
	"Ident Information" "" \
	"Set SNMP Parameter" "" \
	"Help Host-MIB" "Delete Host-MIB"]

vwait forever

## end of snmp_host.tcl
