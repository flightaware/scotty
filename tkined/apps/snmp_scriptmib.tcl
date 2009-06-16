#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh8.3 "$0" "$@"
#
# snmp_scriptmib.tcl -
#
#	DISMAN-SCRIPT-MIB (RFC 2592) specific functions for Tkined.
#
# Copyright (c) 1998-2000 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: snmp_scriptmib.tcl,v 1.1.1.1 2006/12/07 12:16:59 karl Exp $

package require Tnm 3.0
package require TnmSnmp $tnm(version)
package require TnmScriptMib $tnm(version)

namespace import Tnm::*

ined size
LoadDefaults snmp
SnmpInit Script-MIB

##
## Load the mib modules required by this script. This will prevent to load 
## all mib modules and reduce memory requirements and startup time.
##

mib load IANA-LANGUAGE-MIB
mib load DISMAN-SCRIPT-MIB

set owner $tnm(user)
set sowner $tnm(user)
set source "http://www.ibr.cs.tu-bs.de/projects/jasmin/scripts/"
set lang 1
set lifetime 3600000
set expiretime 3600000
set maxrunning 5
set maxcompleted 5

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



proc "List Languages" {list} {
    ShowTable $list smLangTable
}



proc "List Extensions" {list} {
    ShowTable $list smExtsnTable
}



proc "List Scripts" {list} {
    ShowTable $list smScriptTable
}



proc "Install Script" {list} {
    global tnm
    global source
    global lang
    global owner
    set name ""
    ForeachIpNode id ip host $list {
	set res [ined request "Install a new script on $host:" \
		[list [list "Language:" $lang entry 10] \
	              [list "Owner:" $owner entry 20] \
	              [list "Name:" $name entry 20] \
	              [list "Description:" $name entry 50] \
		      [list "Source:" $source entry 50] ] \
	        [list install cancel] ]

	if {[lindex $res 0] == "cancel"} return

	set lang   [lindex $res 1]
	set owner  [lindex $res 2]
	set name   [lindex $res 3]
	set descr  [lindex $res 4]
	set source [lindex $res 5]

	set s [SnmpOpen $id $ip]

	set code [catch {TnmScriptMib::InstallScript $s $owner $name $lang $source $descr} msg]
	if {$code} {
	    writeln "Failed to install new script $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}

	set code [catch {TnmScriptMib::EnableScript $s $owner $name} msg]
	if {$code} {
	    writeln "Failed to enable script $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}
	
	$s destroy
    }
}



proc "Delete Script" {list} {
    ForeachIpNode id ip host $list {
        set s [SnmpOpen $id $ip]
	$s walk vbl {smScriptOperStatus} {
	    lappend scriptList [mib unpack [snmp oid $vbl 0]]
	}
	$s destroy
    }

    if {! [info exists scriptList]} {
	ined acknowledge "Could not find a single script."
	return
    }
    set result [ined list "Select a script to delete:" \
	    $scriptList "select cancel"]
    if {[lindex $result 0] == "cancel"} {
	return
    }
    set owner [lindex [lindex $result 1] 0]
    set name [lindex [lindex $result 1] 1]

    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]

	set code [catch {TnmScriptMib::DisableScript $s $owner $name} msg]
	if {$code} {
	    writeln "Failed to disable script $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}

	set oid [mib pack smScriptRowStatus $owner $name]
	set code [catch {$s set [list [list $oid 6]]} msg]
	if {$code} {
	    writeln "Failed to destroy script $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}
	$s destroy
    }
}



proc "List Launch Buttons" {list} {
    ShowTable $list smLaunchTable
}



proc "Install Launch Button" {list} {
    global tnm
    global lifetime
    global expiretime
    global maxrunning
    global maxcompleted
    global owner
    global sowner
    set name ""
    set sname ""
    set argument ""
    ForeachIpNode id ip host $list {
	set res [ined request "Install a new launch button on $host:" \
		[list [list "Owner:" $owner entry 20] \
	              [list "Name:" $name entry 20] \
	              [list "Script Owner:" $sowner entry 20] \
	              [list "Script Name:" $sname entry 20] \
	              [list "Arguments:" $argument entry 40] \
	              [list "Life Time (ms):" $lifetime entry 10] \
	              [list "Expire Time (ms):" $expiretime entry 10] \
	              [list "Max Running:" $maxrunning entry 10] \
		      [list "Max Completed:" $maxcompleted entry 10] ] \
	        [list install cancel] ]

	if {[lindex $res 0] == "cancel"} return

	set owner        [lindex $res 1]
	set name         [lindex $res 2]
	set sowner       [lindex $res 3]
	set sname        [lindex $res 4]
	set argument     [mib scan DisplayString [lindex $res 5]]
	set lifetime     [lindex $res 6]
	set expiretime   [lindex $res 7]
	set maxrunning   [lindex $res 8]
	set maxcompleted [lindex $res 9]

	set s [SnmpOpen $id $ip]

	set code [catch {TnmScriptMib::InstallLaunch $s $owner $name $sowner $sname $argument $lifetime $expiretime $maxrunning $maxcompleted} msg]
	if {$code} {
	    writeln "Failed to install launch button $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}

	set code [catch {TnmScriptMib::EnableLaunch $s $owner $name} msg]
	if {$code} {
	    writeln "Failed to enable launch button $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}

	$s destroy
    }
}



proc "Press Launch Button" {list} {
    ForeachIpNode id ip host $list {
        set s [SnmpOpen $id $ip]
	$s walk vbl {smLaunchOperStatus} {
	    set smLaunchOperStatus [snmp value $vbl 0]
	    if {$smLaunchOperStatus == "enabled"} {
		lappend launchList [mib unpack [snmp oid $vbl 0]]
	    }
	}
	$s destroy
    }

    if {! [info exists launchList]} {
	ined acknowledge "Could not find a single launch button."
	return
    }
    set result [ined list "Select a launch button:" \
	    $launchList "select cancel"]
    if {[lindex $result 0] == "cancel"} {
	return
    }
    set owner [lindex [lindex $result 1] 0]
    set name [lindex [lindex $result 1] 1]

    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	set oid [mib pack smLaunchStart $owner $name]
	set code [catch {$s set [list [list $oid 0]]} msg]
	if {$code} {
	    writeln "Failed to start script $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}
	$s destroy
    }
}



proc "Delete Launch Button" {list} {
    ForeachIpNode id ip host $list {
        set s [SnmpOpen $id $ip]
	$s walk vbl {smLaunchOperStatus} {
	    lappend launchList [mib unpack [snmp oid $vbl 0]]
	}
	$s destroy
    }

    if {! [info exists launchList]} {
	ined acknowledge "Could not find a single launch button."
	return
    }
    set result [ined list "Select a launch button to delete:" \
	    $launchList "select cancel"]
    if {[lindex $result 0] == "cancel"} {
	return
    }
    set owner [lindex [lindex $result 1] 0]
    set name [lindex [lindex $result 1] 1]

    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]

	set code [catch {TnmScriptMib::DisableLaunch $s $owner $name} msg]
	if {$code} {
	    writeln "Failed to disable launch button $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}

	set oid [mib pack smLaunchRowStatus $owner $name]
	set code [catch {$s set [list [list $oid 6]]} msg]
	if {$code} {
	    writeln "Failed to destroy launch button $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}
	$s destroy
    }
}



proc "List Running Scripts" {list} {
    ShowTable $list smRunTable
}



proc "Suspend Running Script" {list} {
    ForeachIpNode id ip host $list {
        set s [SnmpOpen $id $ip]
	$s walk vbl {smRunState} {
	    set smRunState [snmp value $vbl 0]
	    if {$smRunState == "executing"} {
		lappend runList [mib unpack [snmp oid $vbl 0]]
	    }
	}
	$s destroy
    }

    if {! [info exists runList]} {
	ined acknowledge "Could not find a single executing script."
	return
    }
    set result [ined list "Select a running script to suspend:" \
	    $runList "select cancel"]
    if {[lindex $result 0] == "cancel"} {
	return
    }
    set owner [lindex [lindex $result 1] 0]
    set name [lindex [lindex $result 1] 1]
    set index [lindex [lindex $result 1] 2]

    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]

	set code [catch {TnmScriptMib::SuspendRun $s $owner $name $index} msg]
	if {$code} {
	    writeln "Failed to suspend running script $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}

	$s destroy
    }
}



proc "Resume Running Script" {list} {
    ForeachIpNode id ip host $list {
        set s [SnmpOpen $id $ip]
	$s walk vbl {smRunState} {
	    set smRunState [snmp value $vbl 0]
	    if {$smRunState == "suspended"} {
		lappend runList [mib unpack [snmp oid $vbl 0]]
	    }
	}
	$s destroy
    }

    if {! [info exists runList]} {
	ined acknowledge "Could not find a single suspended script."
	return
    }
    set result [ined list "Select a suspended script to resume:" \
	    $runList "select cancel"]
    if {[lindex $result 0] == "cancel"} {
	return
    }
    set owner [lindex [lindex $result 1] 0]
    set name [lindex [lindex $result 1] 1]
    set index [lindex [lindex $result 1] 2]

    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]

	set code [catch {TnmScriptMib::ResumeRun $s $owner $name $index} msg]
	if {$code} {
	    writeln "Failed to resume script $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}
	
	$s destroy
    }
}



proc "Abort Running Script" {list} {
    ForeachIpNode id ip host $list {
        set s [SnmpOpen $id $ip]
	$s walk vbl {smRunState} {
	    set smRunState [snmp value $vbl 0]
	    if {$smRunState != "terminated"} {
		lappend runList [mib unpack [snmp oid $vbl 0]]
	    }
	}
	$s destroy
    }

    if {! [info exists runList]} {
	ined acknowledge "Could not find a single running script."
	return
    }
    set result [ined list "Select a running script to abort:" \
	    $runList "select cancel"]
    if {[lindex $result 0] == "cancel"} {
	return
    }
    set owner [lindex [lindex $result 1] 0]
    set name [lindex [lindex $result 1] 1]
    set index [lindex [lindex $result 1] 2]

    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]

	set code [catch {TnmScriptMib::AbortRun $s $owner $name $index} msg]
	if {$code} {
	    writeln "Failed to abort script $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}
	
	$s destroy
    }
}



proc "Delete Running Script" {list} {
    ForeachIpNode id ip host $list {
        set s [SnmpOpen $id $ip]
	$s walk vbl {smRunState} {
	    set smRunState [snmp value $vbl 0]
	    if {$smRunState == "terminated"} {
		lappend runList [mib unpack [snmp oid $vbl 0]]
	    }
	}
	$s destroy
    }

    if {! [info exists runList]} {
	ined acknowledge "Could not find a single terminated script."
	return
    }
    set result [ined list "Select a terminated script to delete:" \
	    $runList "select cancel"]
    if {[lindex $result 0] == "cancel"} {
	return
    }
    set owner [lindex [lindex $result 1] 0]
    set name [lindex [lindex $result 1] 1]
    set index [lindex [lindex $result 1] 2]

    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]

	set code [catch {TnmScriptMib::DeleteRun $s $owner $name $index} msg]
	if {$code} {
	    writeln "Failed to delete running script $name owned by $owner on $host:"
	    writeln "  -> $msg"
	}
	
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
## Display some help about this tool.
##

proc "Help Script-MIB" {list} {
    ined browse "Help about Script-MIB" {
	"List Languages:" 
	"    List the languages the agent is aware of." 
	"" 
	"List Extensions:" 
	"    List the language extensions the agent is aware of." 
	"" 
	"List Scripts:" 
	"    List the scripts installed at the agent." 
	"" 
	"Install Script:" 
	"    Install a new script at the agent." 
	"" 
	"Delete Script:" 
	"    Delete a script from the agent." 
	"" 
	"List Launch Buttons:" 
	"    List the script launch buttons installed at the agent." 
	"" 
	"Install Launch Button:" 
	"    Install a new script launch button at the agent." 
	"" 
	"Delete Launch Button:" 
	"    Delete a script launch button from the agent." 
	"" 
	"Press Launch Button:" 
	"    Launch a script from an existing launch button." 
	"" 
	"List Running Scripts:" 
	"    List the scripts currently running at the agent," 
	"    including suspended and terminated scripts." 
	"" 
	"Suspend Running Script:" 
	"    Suspend a currently running script." 
	"" 
	"Resume Running Script:" 
	"    Suspend a currently suspended script." 
	"" 
	"Abort Running Script:" 
	"    Abort a currently running or suspended script." 
	"" 
	"Delete Running Script:" 
	"    Remove a terminated script from the agent." 
	"" 
	"Set SNMP Parameter:" 
	"    This dialog allows you to set SNMP parameters like retries, " 
	"    timeouts, community name and port number. " 
    }
}



##
## Delete the menus created by this interpreter.
##

proc "Delete Script-MIB" {list} {
    global menus
    foreach id $menus { ined delete $id }
    exit
}



set menus [ ined create MENU "Script-MIB" \
    "List Languages" \
    "List Extensions" \
    "" \
    "List Scripts" \
    "Install Script" \
    "Delete Script" \
    "" \
    "List Launch Buttons" \
    "Install Launch Button" \
    "Delete Launch Button" \
    "Press Launch Button" \
    "" \
    "List Running Scripts" \
    "Suspend Running Script" \
    "Resume Running Script" \
    "Abort Running Script" \
    "Delete Running Script" \
    "" \
    "Set SNMP Parameter" \
    "" \
    "Help Script-MIB" \
    "Delete Script-MIB" ]

vwait forever
