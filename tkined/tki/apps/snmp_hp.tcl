#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# snmp_hp.tcl -
#
#	Some HP specific procedures to display some HP specific
#	SNMP tables.
#
# Copyright (c) 1994-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: snmp_hp.tcl,v 1.1.1.1 2006/12/07 12:16:59 karl Exp $

package require Tnm 3.0
package require TnmSnmp $tnm(version)

namespace import Tnm::*

ined size
LoadDefaults snmp
SnmpInit SNMP-HP

##
## Load the mib modules required by this script. This will prevent to load 
## all mib modules and reduce memory requirements and statup time.
##

mib load hp-unix.mib

##
## List all scalars of a group for the node objects in list.
##

proc ShowScalars {list group} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
        writeln [TnmSnmp::ShowScalars $s $group]
        $s destroy
    }
}

##
## Show a complete MIB table for the node objects in list.
##

proc ShowTable {list table} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	writeln [TnmSnmp::ShowTable $s $table]
	$s destroy
    }
}

##
## Display the system MIB of CISCO devices.
##

proc "Computer Information" {list} {
    ShowScalars $list hp.nm.system.general.computerSystem
}

##
## Get the ieee 802.3 specific variables of the HP SNMP daemon.
##

proc hpieee8023 {s idx} {
    set prefix hp.nm.interface.ieee8023Mac.ieee8023MacTable.ieee8023MacEntry
    set txt ""
    foreach var [mib children [mib oid $prefix]] {
	if {[catch {$s get $var.$idx} result]} continue
	set val [lindex [lindex $result 0] 2]
	append txt [format "  %-45s %s\n" "[mib name $var.$idx]:" $val]
    }
    write $txt
}

proc "IEEE 802.3 Statistics" {list} {
    set prefix hp.nm.interface.ieee8023Mac.ieee8023MacTable.ieee8023MacEntry
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	writeln "Checking for ieee 802.3 interfaces on $host \[$ip\]:"
	$s walk x "ifIndex ifDescr" {
	    set idx [lindex [lindex $x 0] 2]
	    set dsc [lindex [lindex $x 1] 2]
	    if {![catch {$s get $prefix.ieee8023MacIndex.$idx}]} {
		writeln "Interface Number $idx ($dsc)"
		hpieee8023 $s $idx
	    }
	}
	$s destroy
	writeln
    }
}

##
## Show the file system information of the HP MIB.
##

proc "File Systems" {list} {
    set prefix hp.nm.system.general.fileSystem.fileSystemTable.fileSystemEntry
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
        set txt "Filesystems on $host \[$ip\]:\n"
        append txt "Filesystem	              kbytes    used    avail Mounted on\n"
        $s walk x "$prefix.fileSystemName $prefix.fileSystemBsize \
		   $prefix.fileSystemBlock $prefix.fileSystemBfree \
		   $prefix.fileSystemBavail $prefix.fileSystemDir" {
            set Name   [lindex [lindex $x 0] 2]
            set Bsize  [lindex [lindex $x 1] 2]
	    set Block  [expr {[lindex [lindex $x 2] 2] * $Bsize / 1024}]
	    set Bfree  [expr {[lindex [lindex $x 3] 2] * $Bsize / 1024}]
            set Bavail [expr {[lindex [lindex $x 4] 2] * $Bsize / 1024}]
            set Dir    [lindex [lindex $x 5] 2]
            append txt [format "%-28s%8s%8s%8s %s\n" \
                        $Name $Block [expr $Block-$Bfree] $Bavail $Dir]
        }
        $s destroy
        writeln $txt
    }
}

##
## HP cluster information.
##

proc "Cluster Information" {list} {
    set prefix hp.nm.system.general.cluster.clusterTable.clusterEntry
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
        set txt "Cluster Information on $host \[$ip\]:\n"
        append txt "Index         Name             Address    Type  SwapServer\n"
        $s walk x "$prefix.clusterID $prefix.clusterCnodeName \
		   $prefix.clusterType $prefix.clusterSwapServingCnode \
		   $prefix.clusterCnodeAddress" {
            set ID      [lindex [lindex $x 0] 2]
	    set Name    [lindex [lindex $x 1] 2]
            set Type    [lindex [lindex $x 2] 2]
	    set Swap    [lindex [lindex $x 3] 2]
	    set Address [lindex [lindex $x 4] 2]
            append txt [format "%4s%16s%20s%5s%8s\n" \
                        $ID $Name $Address $Type $Swap]
        }
	$s destroy
        writeln $txt
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

proc "Delete SNMP-HP" {list} {
    global menus
    foreach id $menus { ined delete $id }
    exit
}

##
## Display some help about this tool.
##

proc "Help SNMP-HP" {list} {
    ined browse "Help about SNMP-HP" {
	"HP Information:" 
	"    Display some interesting system specific information for a" 
	"    HP device." 
	"" 
	"Set SNMP Parameter:" 
	"    This dialog allows you to set SNMP parameters like retries, " 
	"    timeouts, community name and port number. " 
    }
}

set menus [ ined create MENU "SNMP-HP" "Computer Information" \
	    "File Systems" "Cluster Information" \
	    "IEEE 802.3 Statistics" "" \
	    "Set SNMP Parameter" "" \
	    "Help SNMP-HP" "Delete SNMP-HP"]

vwait forever
