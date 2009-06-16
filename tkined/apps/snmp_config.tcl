#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# snmp_config.tcl -
#
#	Simple interface for SNMP remote configuration.
#
# Copyright (c) 1998      Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: snmp_config.tcl,v 1.1.1.1 2006/12/07 12:16:59 karl Exp $

package require Tnm 3.0
package require TnmSnmp $tnm(version)

namespace import Tnm::*

ined size
LoadDefaults snmp
SnmpInit SNMP

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

proc "Targets" {list} {
    ShowTable $list SNMP-TARGET-MIB!snmpTargetAddrTable
    ShowTable $list SNMP-TARGET-MIB!snmpTargetParamsTable
}

proc "Notifications" {list} {
    ShowTable $list SNMP-NOTIFICATION-MIB!snmpNotifyTable
    ShowTable $list SNMP-NOTIFICATION-MIB!snmpNotifyFilterProfileTable
    ShowTable $list SNMP-NOTIFICATION-MIB!snmpNotifyFilterTable
}

proc "View-based Access Control" {list} {
    ShowTable $list SNMP-VIEW-BASED-ACM-MIB!vacmContextTable
    ShowTable $list SNMP-VIEW-BASED-ACM-MIB!vacmSecurityToGroupTable
    ShowTable $list SNMP-VIEW-BASED-ACM-MIB!vacmAccessTable
    ShowTable $list SNMP-VIEW-BASED-ACM-MIB!vacmViewTreeFamilyTable
}

proc "User-based Security" {list} {
    ShowTable $list SNMP-USER-BASED-SM-MIB!usmUserTable
}

##
## Set the parameters (community, timeout, retry) for snmp requests.
##

proc "Set SNMP Parameter" {list} {
    SnmpParameter $list
}

##
## Display some help about this tool.
##

proc "Help SNMP-Config" {list} {
    ined browse "Help about SNMP" {
	"Targets:" 
	"    Show the configured targets." 
	"" 
	"Notifications:" 
	"    Show the notifications filtering and forwarding tables." 
	"" 
	"View-based Access Control:" 
	"    Show the view-based access control lists." 
	"" 
	"User-based Security:" 
	"    Show the user-based security model control table." 
	"" 
	"Set SNMP Parameter:" 
	"    This dialog allows you to set SNMP parameters like retries, " 
	"    timeouts, community name and port number. " 
    }
}

##
## Delete the menus created by this interpreter.
##

proc "Delete SNMP-Config" {list} {
    global menus
    foreach id $menus { ined delete $id }
    exit
}

set menus [ ined create MENU "SNMP-Config" \
    "Targets" "Notifications" "" \
    "View-based Access Control" "" \
    "User-based Security" "" \
    "Set SNMP Parameter" "" \
    "Help SNMP-Config" "Delete SNMP-Config" ]

vwait forever
