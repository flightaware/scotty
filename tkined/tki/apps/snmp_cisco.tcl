#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# snmp_cisco.tcl -
#
#	Some CISCO specific SNMP functions for the Tkined editor.
#
# Copyright (c) 1993-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: snmp_cisco.tcl,v 1.1.1.1 2006/12/07 12:16:59 karl Exp $

package require Tnm 3.0
package require TnmSnmp $tnm(version)

namespace import Tnm::*

ined size
LoadDefaults snmp
SnmpInit SNMP-CISCO

if {[info exists default(tftphost)]} {
    set snmp_tftphost $default(tftphost)
} else {
    set snmp_tftphost "tftphost"
}

if {[info exists default(upload)]} {
    set snmp_upload $default(upload)
} else {
    set snmp_upload "network-confg"
}

##
## Load the mib modules required by this script. This will prevent to load 
## all mib modules and reduce memory requirements and statup time.
##

mib load cisco.mib

##
## Sort a list in numerical order. The first argument is the list
## to be sorted and the second argument gives the index of the 
## element that will be used as the key.
##

proc numcmp {field a b} {
    return [expr {[lindex $a $field] < [lindex $b $field]}]
}

proc numsort {list field} {
    lsort -command "numcmp $field" $list 
}

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

proc "Cisco Information" {list} {
    ShowScalars $list cisco.local.lsystem
}


###############################################
##
## Down load config from cisco to TFTP Host
##

# Method, load the address of the tftp host into 'writeNet'
# wants a write only string - probably either name or IP No.
# The TFTP Server name should be a variable. - A changable thing.
# Lets pretend its called $snmp_tftphost
# The General file name to copy up is $snmp_upload

proc GetIp {host} {
    if {[regexp {^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$} $host] > 0} {
	return $host
    }
    if {! [catch {nslook $host} ip]} {
	return [lindex $ip 0]
    }
    return $host
}

proc "TFTP Download to Host" {list} {
    global snmp_tftphost
    set ip [GetIp $snmp_tftphost]
    ExecuteTFTP $list cisco.local.lsystem.writeNet.$ip
}

##
## Copy over a config from TFTP to cisco... Write TFTP name to 'hostConfigSet'
## Copy over a General (netconfig) config from TFTP to cisco... 
## Write TFTP name to 'netConfigSet'
##

proc "TFTP Upload to Cisco" {list} {
    global snmp_tftphost snmp_upload

    set result [ined request "Upload configuration files to Cisco:" \
	    [list [list "Host Configuration:" yes radio yes no] \
	          [list "Network Configuration:" yes radio yes no] ] \
	    [list accept cancel] ]

    if {[lindex $result 0] != "accept"} return

    set ip [GetIp $snmp_tftphost]

    if {[lindex $result 1] == "yes"} {
	ExecuteTFTP $list cisco.local.lsystem.hostConfigSet.$ip
    }
    if {[lindex $result 2] == "yes"} {
	ExecuteTFTP $list \
		cisco.local.lsystem.netConfigSet.$ip $snmp_upload
    }
}

##
## Save config to NVRAM - useful after Uploading a config!
##

proc "Write / Clear NVRAM" {list} {
    set result [ined confirm "Write configuration to NVRAM?" \
	    [list write clear cancel]]
    switch $result {
	write { set value 1 }
	clear { set value 0 }
	default { return }
    }
    ExecuteTFTP $list cisco.local.lsystem.writeMem.0 $value
}

##
## Execute a command for the node objects in list.
##

proc ExecuteTFTP {list command {arg ""}} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]

	# If arg is empty - we need to get the name of the cisco 
	# by asking the Cisco itself. This is a good idea as it also 
	# checks the SNMP connection. If no reply - skip this cisco.
	
	set value $arg
	if {$arg == ""} {
	    set var cisco.local.lsystem.hostName.0
	    if {[catch { $s get $var } vbl]} {
		writeln "$host \[$ip\] reading $var failed: [lindex $vbl 0]\n"
		$s destroy
		continue
	    }
	    set value [lindex [lindex $vbl 0] 2]-confg
	}
	
	# We have the host/file name - fire a question!

	writeln "$host \[$ip\] SNMP set [mib name $command] = $value"
	$s configure -retries 1
	if {[catch { $s set [list [list $command $value] ] } msg]} {
	    writeln "$host \[$ip\] transaction failed: [lindex $msg 0]"
	} else {
	    writeln "$host \[$ip\] transaction complete"
	}
	writeln
	$s destroy
    }
}

proc "TFTP Parameter" {list} {
    global snmp_tftphost snmp_upload
    
    set result [ined request "Set TFTP Parameter" \
		[list [list "TFTP Host:" $snmp_tftphost entry 30] \
                      [list "TFTP File:" $snmp_upload entry 30] ] \
		[list accept cancel] ]

    if {[lindex $result 0] == "cancel"} return

    set snmp_tftphost  [lindex $result 1]
    set snmp_upload    [lindex $result 2]
}


#############################################################

##
## This callback is invoked to show host names for IP addresses or
## to create NODE objects for IP addresses shown in an accounting table.
##

proc ShowName { dst pkts byts } {

    static x y

    if {![info exists x]} { set x 50 }
    if {![info exists y]} { set y 50 }

    set name ""
    if {[catch {dns name $dst} name]} {
	if {[catch {nslook $dst} name]} {
	    set result [ined confirm "Can not look up name for $dst." "" \
			"Create a NODE for it?" [list create cancel] ]
	    if {$result != "create"} return
	}
    }

    if {$name != ""} {
	set result [ined confirm "$name \[$dst\]" "" "Create a NODE for it?" \
		   [list create cancel] ]
    }

    if {$result == "create"} {
	set id [ined create NODE]
	ined -noupdate name $id $name
	ined -noupdate address $id $dst
	ined -noupdate attribute $id "ip accounting" "$pkts pkts $byts byte"
	ined -noupdate label $id name
	ined -noupdate move $id [incr x 20] $y
	if {[incr y 20] > 500} { set y 50 }
    }
}

##
## Show the result of an accounting table of the cisco MIB.
##

proc ShowAccountTable { table } {
    writeln
    writeln [format "%16s  %16s  %12s %12s" \
	    Source Destination Packets Bytes]
    writeln "-------------------------------------------------------------"
    foreach x [numsort $table 3] {
	set src  [lindex $x 0]
	set dst  [lindex $x 1]
	set pkts [lindex $x 2]
	set byts [lindex $x 3]
	write [format "%16s " $src]
	write [format "%16s" $dst] "ShowName $dst $pkts $byts"
	writeln [format " %12s %12s" $pkts $byts]
    }
    writeln
}

##
## Retrieve an accounting table of the cisco MIB.
##

proc GetAccountingTable { list {ck 0} {src ""} } {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	if {$ck} {
	    set actAge cisco.local.lip.ckactAge.0
	    if {$src == ""} {
		set varlist "ckactSrc ckactDst ckactPkts ckactByts"
	    } else {
		set varlist "ckactSrc.$src ckactDst.$src ckactPkts.$src ckactByts.$src"
	    }
	} else {
	    set actAge cisco.local.lip.actAge.0
	    if {$src == ""} {
		set varlist "actSrc actDst actPkts actByts"
	    } else {
		set varlist "actSrc.$src actDst.$src actPkts.$src actByts.$src"
	    }
	}
	if {[catch {$s get $actAge} age]} {
	    ined acknowledge "Can not get accounting information of $host."
	    continue
	}
	set age [lindex [lindex $age 0] 2]
	if {$ck} {
	    writeln "Checkpoint accounting statistics of $host ([join $age]):"
	} else {
	    writeln "Accounting statistics of $host ([join $age]):"
	}
	set table ""
	$s walk row $varlist {
	    set Src  [lindex [lindex $row 0] 2]
	    set Dst  [lindex [lindex $row 1] 2]
	    set Pkts [lindex [lindex $row 2] 2]
	    set Byts [lindex [lindex $row 3] 2]
	    lappend table "$Src $Dst $Pkts $Byts"
	}
	ShowAccountTable $table
	$s destroy
    }    
}

proc "Accounting Table" {list} {
    GetAccountingTable $list
}

proc "Checkpoint Accounting Table" {list} {
    GetAccountingTable $list 1
}

##
## Only retrieve those table elements that start with a given IP
## address prefix.
##

proc "Bytes send from Host" {list} {

    static src
    if {![info exists src]} { set src "" }

    set result [ined request "Set filter to source IP address:" \
	       [list [list IP: $src] \
	             [list Table: "actual" radio actual checkpointed] ] \
	       [list start cancel] ]
    if {[lindex $result 0] == "cancel"} return
    set src [lindex $result 1]
    set ck  [expr {[lindex $result 2] == "checkpointed"}]

    # should convert hostnames to ip dot notation

    if {![regexp {^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$} $src]} {
	if {[catch {nslook $src} src]} {
	    ined acknowledge "Can not convert to IP address."
	    return
	}
    }
    set src [lindex $src 0]

    GetAccountingTable $list $ck $src
}

##
## Display active connections on terminal server lines.
##

proc "Active Sessions" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]

	set txt "Active terminal sessions of $host \[$ip\]:\n"
	append txt "Type Direction Active Idle Address         Name\n"
	$s walk x {
	    tslineSesType tslineSesDir tslineSesAddr tslineSesName
	    tslineSesCur tslineSesIdle
	} {
	    set SesType [lindex [lindex $x 0] 2]
	    set SesDir  [lindex [lindex $x 1] 2]
	    set SesAddr [lindex [lindex $x 2] 2]
	    set SesName [lindex [lindex $x 3] 2]
	    set SesCur  [lindex [lindex $x 4] 2]
	    set SesIdle [lindex [lindex $x 5] 2]
	    append txt [format "%3s   %4s     %4s %5s  %-15s %s\n" \
			$SesType $SesDir $SesCur $SesIdle $SesAddr $SesName]
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

proc "Delete SNMP-CISCO" {list} {
    global menus
    foreach id $menus { ined delete $id }
    exit
}

##
## Display some help about this tool.
##

proc "Help SNMP-CISCO" {list} {
    ined browse "Help about SNMP-CISCO" {
	"Cisco Information:" 
	"    Display some interesting system specific information for a" 
	"    CISCO device." 
	"" 
	"Active Sessions:" 
	"    List the currently active terminal server sessions." 
	"" 
	"Set SNMP Parameter:" 
	"    This dialog allows you to set SNMP parameters like retries, " 
	"    timeouts, community name and port number. " 
    }
}

set menus [ ined create MENU "SNMP-CISCO" "Cisco Information" "" \
	    "Accounting Table" "Checkpoint Accounting Table" \
	    "Bytes send from Host" "" \
	    "TFTP Download to Host" \
	    "TFTP Upload to Cisco" \
	    "TFTP Parameter" "" \
	    "Write / Clear NVRAM" "" \
	    "Active Sessions" "" \
	    "Set SNMP Parameter" "" \
	    "Help SNMP-CISCO" "Delete SNMP-CISCO"]

vwait forever
