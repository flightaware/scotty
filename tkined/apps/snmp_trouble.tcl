#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# snmp_trouble.tcl -
#
#	Simple troubleshooting scripts based on SNMP.
#
# Copyright (c) 1993-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1997-1998 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: snmp_trouble.tcl,v 1.1.1.1 2006/12/07 12:16:59 karl Exp $

package require Tnm 3.0
package require TnmSnmp $tnm(version)
package require TnmEther $tnm(version)

namespace import Tnm::*

ined size
LoadDefaults snmp
SnmpInit SNMP-Trouble

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
## Try to figure out if the devices responds to snmp requests.
##

proc SnmpDeviceCallback {id ip host session error} {
    if {$error == "noError" } {
	write   [format "%-32s \[" $host ]
	write   $ip "IpFlash $ip"
	writeln "\] "
	ined -noupdate attribute $id "SNMP:Config" [$session configure]
    } else {
	writeln [format "%-32s \[%s\] %s" $host $ip $error]
    }
    $session destroy
}

proc "SNMP Devices" {list} {
    writeln "SNMP devices:"
    ForeachIpNode id ip host $list {
	[SnmpOpen $id $ip] getnext 1.0 \
		[list SnmpDeviceCallback $id $ip $host %S %E]
    }
    snmp wait
    writeln
}

##
## Show the system information of MIB II.
##

proc "System Information" {list} {
    ShowScalars $list system
}

##
## Show the system information of MIB II.
##

proc "System Capabilities" {list} {
    ShowTable $list sysORTable
}

##
## Get the list of interfaces and their status.
##

proc "IF Status" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	set txt "Interface status of $host \[$ip\]:\n"
	append txt "ifIndex ifDescr              ifAdminStatus ifOperStatus (ifType)\n"
	try {
	    $s walk x "ifIndex ifDescr ifAdminStatus ifOperStatus ifType" {
		set ifIndex [lindex [lindex $x 0] 2]
		set ifDescr [lindex [lindex [lindex [lindex $x 1] 2] 0] 0]
		set ifAdmin [lindex [lindex $x 2] 2]
		set ifOper  [lindex [lindex $x 3] 2]
		set ifType  [lindex [lindex $x 4] 2]
		append txt [format "%5s   %-18s %12s %12s    (%s)\n" \
			$ifIndex $ifDescr $ifAdmin $ifOper $ifType]
	    }
	} msg { 
	    append txt "$msg\n" 
	}
	writeln $txt
	$s destroy
    }
}

##
## Get the list of interfaces and their status.
##

proc "IF Parameter" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	set txt "Interface parameter of $host \[$ip\]:\n"
	append txt "ifIndex ifDescr                ifSpeed      ifMtu   ifPhysAddress\n"
	try {
	    $s walk x "ifIndex ifDescr ifSpeed ifMtu ifPhysAddress" {
		set ifIndex [lindex [lindex $x 0] 2]
		set ifDescr [lindex [lindex [lindex [lindex $x 1] 2] 0] 0]
		set ifSpeed [lindex [lindex $x 2] 2]
		set ifMtu   [lindex [lindex $x 3] 2]
		set ifPhys  [lindex [lindex $x 4] 2]
		append txt [format " %5s  %-18s %11s %10s   %s\n" \
			$ifIndex $ifDescr $ifSpeed $ifMtu $ifPhys]
	    }
	} msg { 
	    append txt "$msg\n" 
	}
	writeln $txt
	$s destroy
    }
}

##
## We should check which interfaces are up and running.
##

proc "IF Usage Statistics" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	set txt "Interface usage of $host \[$ip\]:\n"
        append txt "ifIndex ifDescr           ifInOctets  ifInUcastPkts  ifOutOctets ifOutUcastPkts\n"
	try {
	    $s walk x "ifIndex ifDescr ifInOctets ifInUcastPkts \
		    ifOutOctets ifOutUcastPkts" {
		set ifIndex   [lindex [lindex $x 0] 2]
		set ifDescr   [lindex [lindex [lindex [lindex $x 1] 2] 0] 0]
		set ifInOct   [lindex [lindex $x 2] 2]
		set ifInUOct  [lindex [lindex $x 3] 2]
		set ifOutOct  [lindex [lindex $x 4] 2]
		set ifOutUOct [lindex [lindex $x 5] 2]
		append txt [format " %5s  %-15s %12s %14s %12s %14s\n" \
			$ifIndex $ifDescr $ifInOct $ifInUOct \
			$ifOutOct $ifOutUOct]
	    }
	} msg {
            append txt "$msg\n"
        }
	writeln $txt
	$s destroy
    }
}


proc "IF Error Statistics" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	set txt "Interface errors of $host \[$ip\]:\n"
	append txt "ifIndex ifDescr           ifInErrors   ifInDiscards  ifOutErrors  ifOutDiscards\n"
	try {
	    $s walk x "ifIndex ifDescr ifInErrors ifInDiscards \
		    ifOutErrors ifOutDiscards" {
		set ifIndex   [lindex [lindex $x 0] 2]
		set ifDescr   [lindex [lindex [lindex [lindex $x 1] 2] 0] 0]
		set ifInErr   [lindex [lindex $x 2] 2]
		set ifInDisc  [lindex [lindex $x 3] 2]
		set ifOutErr  [lindex [lindex $x 4] 2]
		set ifOutDisc [lindex [lindex $x 5] 2]
		append txt [format " %5s  %-15s %12s %14s %12s %14s\n" \
			$ifIndex $ifDescr $ifInErr $ifInDisc \
			$ifOutErr $ifOutDisc]
	    }
        } msg {
            append txt "$msg\n"
        }
	writeln $txt
	$s destroy
    }
}

##
## Display relative error statistics.
##

proc "IF Quality" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	set txt "Interface quality of $host \[$ip\]:\n"
	append txt "(may be invalid if counter have wrapped)\n"
	append txt "ifIndex ifDescr          Error Rate   Discard Rate\n"
	try {
	    $s walk x "ifIndex ifDescr ifInUcastPkts ifInNUcastPkts \
		    ifOutUcastPkts ifOutNUcastPkts ifInErrors ifOutErrors \
		    ifInDiscards ifOutDiscards" {
		set ifIndex   [lindex [lindex $x 0] 2]
		set ifDescr   [lindex [lindex [lindex [lindex $x 1] 2] 0] 0]
		set in  [expr [lindex [lindex $x 2] 2] + [lindex [lindex $x 3] 2]]
		set out [expr [lindex [lindex $x 4] 2] + [lindex [lindex $x 5] 2]]
		set pkts [expr {$in + $out}]
		if {$in == 0} {
		    set err 0
		    set dis 0
		} else {
		    set err [expr {[lindex [lindex $x 6] 2] \
			    + [lindex [lindex $x 7] 2]}]
		    set err [expr {100 * double($err) / $pkts}]
		}
		if {$out == 0} {
		    set err 0
		    set dis 0
		} else {
		    set dis [expr {[lindex [lindex $x 8] 2] \
			    + [lindex [lindex $x 9] 2]}]
		    set dis [expr {100 * double($dis) / $pkts}]
		}
		append txt \
			[format "%5s   %-18s %5.2f %%        %5.2f %%\n" \
			$ifIndex $ifDescr $err $dis]
	    }
	} msg {
            append txt "$msg\n"
        }
	writeln $txt
	$s destroy
    }
}

##
## Show the IP Scalars.
##

proc "IP Statistics" {list} {
    ShowScalars $list ip
}

##
## Get the IP addresses belonging to this device.
##

proc "IP Addresses" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	set txt "IP addresses of $host \[$ip\]:\n"
	append txt "ifIndex ifDescr            ipAdEntAddr     ipAdEntNetMask ipAdEntBcastAddr\n"
	try {
	    $s walk x "ifIndex ifDescr" {
		set ifDescr([lindex [lindex $x 0] 2]) [lindex [lindex $x 1] 2]
	    }
	}
	try {
	    set list ""
	    $s walk x {ipAdEntAddr ipAdEntNetMask 
	               ipAdEntBcastAddr ipAdEntIfIndex} {
		set ipAdEntAddr      [lindex [lindex $x 0] 2]
		set ipAdEntNetMask   [lindex [lindex $x 1] 2]
		set ipAdEntBcastAddr [lindex [lindex $x 2] 2]
		set ipAdEntIfIndex   [lindex [lindex $x 3] 2]
		if [info exists ifDescr($ipAdEntIfIndex)] {
                    set name $ifDescr($ipAdEntIfIndex)
                } else {
		    set name ""
		}
		lappend list [format "%5s   %-18s %-15s %-15s %5s" \
			$ipAdEntIfIndex $name \
			$ipAdEntAddr $ipAdEntNetMask $ipAdEntBcastAddr]
	    }
	    append txt "[join [lsort $list] "\n"]\n"
	} msg { 
	    append txt "$msg\n" 
	}
	writeln $txt
	$s destroy
    }
}

##
## Show the routing tables of the agents.
##

proc "IP Routing Table" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	set txt "Routing Table of $host \[$ip\]:\n"
	append txt "ipRouteDest      ipRouteNextHop   ipRouteMask      IfIndex    Type     Proto\n"
	try {
	    $s walk x {ipRouteDest ipRouteNextHop ipRouteMask ipRouteIfIndex
	    ipRouteType ipRouteProto} {
		set ipRouteDest    [lindex [lindex $x 0] 2]
		set ipRouteNext    [lindex [lindex $x 1] 2]
		set ipRouteMask    [lindex [lindex $x 2] 2]
		set ipRouteIfIndex [lindex [lindex $x 3] 2]
		set ipRouteType    [lindex [lindex $x 4] 2]
		set ipRouteProto   [lindex [lindex $x 5] 2]
		append txt [format "%-16s %-16s %-16s %5d %10s %8s\n" \
			$ipRouteDest $ipRouteNext $ipRouteMask \
			$ipRouteIfIndex $ipRouteType $ipRouteProto]
	    }
	} msg { 
	    append txt "$msg\n" 
	}
	writeln $txt
	$s destroy
    }
}

##
## Show the routing tables of the agents.
##

proc "IP ARP Table" {list} {
    global vendor
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	set txt "ARP Table of $host \[$ip\]:\n"
	append txt "ifIndex PhysAddress        NetAddress         Type       Vendor\n"
	set done 0
	$s walk x {ipNetToMediaIfIndex ipNetToMediaPhysAddress 
	ipNetToMediaNetAddress ipNetToMediaType} {
	    set ipNetToMediaIfIndex	    [lindex [lindex $x 0] 2]
	    set ipNetToMediaPhysAddress [lindex [lindex $x 1] 2]
	    set ipNetToMediaNetAddress  [lindex [lindex $x 2] 2]
	    set ipNetToMediaType        [lindex [lindex $x 3] 2]
	    set done 1
#	    set l [join [lrange [split $ipNetToMediaPhysAddress :] 0 2] {}]
#	    set l [string toupper $l]
#	    if {[info exists vendor($l)]} {
#		set vend $vendor($l)
#	    } else {
#		set vend ""
#	    }
            set vend [TnmEther::GetVendor $ipNetToMediaPhysAddress]
	    append txt [format " %5s  %-18s %-18s %-10s %s\n" \
			$ipNetToMediaIfIndex $ipNetToMediaPhysAddress \
			$ipNetToMediaNetAddress $ipNetToMediaType $vend]
	}
	if {!$done} {
	    $s walk x {atIfIndex atPhysAddress atNetAddress} {
		set atIfIndex     [lindex [lindex $x 0] 2]
		set atPhysAddress [lindex [lindex $x 1] 2]
		set atNetAddress  [lindex [lindex $x 2] 2]
#		set l [join [lrange [split $atPhysAddress :] 0 2] {}]
#		set l [string toupper $l]
#		if {[info exists vendor($l)]} {
#		    set vend $vendor($l)
#		} else {
#		    set vend ""
#		}
                set vend [TnmEther::GetVendor $atPhysAddress]
		append txt [format " %5s  %-18s %-18s %s\n" \
			    $atIfIndex $atPhysAddress $atNetAddress $vend]
	    }
	}
	writeln $txt
	$s destroy
    }
}

##
## Show the TCP Scalars.
##

proc "TCP Statistics" {list} {
    ShowScalars $list tcp
}

##
## Show the tcp connection tables of the agents.
##

proc "TCP Connections" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	set percent -1
	if {![catch {$s get "tcp.tcpMaxConn.0 tcp.tcpCurrEstab.0"} vbl]} {
	    set tcpMaxConn   [lindex [lindex $vbl 0] 2]
	    set tcpCurrEstab [lindex [lindex $vbl 1] 2]
	    if {$tcpMaxConn > 0} {
		set percent [expr {$tcpCurrEstab/double($tcpMaxConn)*100}]
	    }
	}
	    
	if {$percent > 0} {
	    set txt "TCP Connections of $host \[$ip\] ($percent% established):\n"
	} else {
	    set txt "TCP Connections of $host \[$ip\]:\n"
	}
	append txt "State            LocalAddress       LocalPort    RemoteAddress      RemotePort\n"
	try {
	    $s walk x "tcpConnState tcpConnLocalAddress tcpConnLocalPort \
		    tcpConnRemAddress tcpConnRemPort" {
		set tcpConnState        [lindex [lindex $x 0] 2]
		set tcpConnLocalAddress [lindex [lindex $x 1] 2]
		set tcpConnLocalPort    [lindex [lindex $x 2] 2]
		set tcpConnRemAddress   [lindex [lindex $x 3] 2]
		set tcpConnRemPort      [lindex [lindex $x 4] 2]

		if {![catch {netdb services name $tcpConnLocalPort tcp} n]} {
		    set tcpConnLocalPort "$n"
		}
		if {![catch {netdb services name $tcpConnRemPort tcp} n]} {
		    set tcpConnRemPort "$n"
		}

		append txt [format "%-12s %16s %15s %16s %15s\n" \
			$tcpConnState \
			$tcpConnLocalAddress $tcpConnLocalPort \
			$tcpConnRemAddress $tcpConnRemPort]
	    }
	} msg {
	    append txt "$msg\n"
	}
	writeln $txt
	$s destroy
    }
}

##
## Show the UDP Scalars.
##

proc "UDP Statistics" {list} {
    ShowScalars $list udp
}

##
## Show the udp listener table of the agents.
##

proc "UDP Listener" {list} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	set txt "UDP Listener of $host \[$ip\]:\n"
	append txt "      LocalAddress       LocalPort\n"
	try {
	    $s walk x "udpLocalAddress udpLocalPort" {
		set udpLocalAddress	[lindex [lindex $x 0] 2]
		set udpLocalPort    [lindex [lindex $x 1] 2]
		if {![catch {netdb services name $udpLocalPort udp} n]} {
		    set udpLocalPort "$n"
		}
		append txt [format "  %16s %15s\n" \
			$udpLocalAddress $udpLocalPort]
	    }
	} msg {
	    append txt "$msg\n"
	}
	writeln $txt
	$s destroy
    }
}

##
## Show the SNMP Scalars.
##

proc "SNMP Statistics" {list} {
    ShowScalars $list snmp
}

##
## Show the ICMP Scalars.
##

proc "ICMP Statistics" {list} {
    ShowScalars $list icmp
}

##
## Dump a complete hierarchy. The user may choose the hierarchy
## before we start our action. We store the last selected hierarchy
## in a static variable called dump_mib_tree_path.
##

proc "Walk MIB Tree" {list} {

    static dump_mib_tree_path

    if {![info exists dump_mib_tree_path]} {
        set dump_mib_tree_path "mib-2"
    }

    set path [ined request "Walk MIB Tree:" \
	        [list [list "MIB path:" $dump_mib_tree_path] ] \
		[list walk cancel] ]

    if {[lindex $path 0]== "cancel"} return

    set dump_mib_tree_path [lindex $path 1]

    ForeachIpNode id ip host $list {
	write   "MIB Tree Walk for $host \[$ip\] "
	writeln "starting at $dump_mib_tree_path:"
	set s [SnmpOpen $id $ip]
	catch {TnmSnmp::Walk $s $dump_mib_tree_path} txt
        $s destroy
        writeln $txt
	writeln
    }
}

##
## Show an arbitrary group of MIB scalars.
##

proc "Show MIB Scalars" {list} {
    static scalars

    if {![info exists scalars]} {
	mib walk x 1.3 {
	    if {[mib macro $x] == "OBJECT-TYPE"} continue
	    foreach c [mib children $x] {
		if {[mib macro $c] == "OBJECT-TYPE" \
			&& [mib syntax $c] != "SEQUENCE OF"} {
		    lappend scalars([mib module $x]) [mib label $x]
		    break
		}
	    }
	}
    }

    set result [ined list "Select a MIB module:" \
            [lsort [array names scalars]] "select cancel"]
    if {[lindex $result 0] == "cancel"} return
    set module [lindex $result 1]

    set result [ined list "Select a group of MIB scalars from $module:" \
	    $scalars($module) "select cancel"]
    if {[lindex $result 0] == "cancel"} return
    set group [lindex $result 1]

    ForeachIpNode id ip host $list {
	set txt "$group @ "
	set s [SnmpOpen $id $ip]
        append txt [TnmSnmp::ShowScalars $s $module!$group]
        $s destroy
	writeln $txt
    }
}

##
## Show an arbitrary MIB table.
##

proc "Show MIB Table" {list} {
    static tables
    if {![info exists tables]} {
	mib walk x 1.3 {
	    if {[mib syntax $x] == "SEQUENCE OF"} {
		lappend tables([mib module $x]) [mib label $x]
	    }
	}
    }

    set result [ined list "Select a MIB Module:" \
	    [lsort [array names tables]] "select cancel"]
    if {[lindex $result 0] == "cancel"} return
    set module [lindex $result 1]

    set result [ined list "Select a table from $module:" \
	    [lsort $tables($module)] "select cancel"]
    if {[lindex $result 0] == "cancel"} return
    set table [lindex $result 1]

    ForeachIpNode id ip host $list {
	set txt "$table @ "
	set s [SnmpOpen $id $ip]
        append txt [TnmSnmp::ShowTable $s $module!$table]
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
## Display some help about this tool.
##

proc "Help SNMP-Trouble" {list} {
    ined browse "Help about SNMP-Trouble" {
	"SNMP Devices:" 
	"    Test which of the selected nodes respond to SNMP requests." 
	"" 
	"System Information:" 
	"    Display the information of the system group." 
	"" 
	"Interface -> IF Status:" 
	"    Display status information of the interfaces." 
	"" 
	"Interface -> IF Parameter:" 
	"    Display interface parameter like speed and MTU." 
	"" 
	"Interface -> IF Usage Statistics:" 
	"    Display interface statistics." 
	"" 
	"Interface -> IF Error Statistics:" 
	"    Display interface statistics." 
	"" 
	"Interface -> IF Quality:" 
	"    Show the error and discard rate per received packed for" 
	"    each interface. The output is only valid if your counters" 
	"    have not wrapped!" 
	"" 
	"" 
	"IP -> IP Statistics" 
	"    Displays some statistics and parameter of the IP layer." 
	"" 
	"IP -> IP Addresses:" 
	"    Show the IP addresses used by this device." 
	"" 
	"IP -> IP Routing Table:" 
	"    Display the routing table." 
	"" 
	"IP -> IP ARP Table:" 
	"    Display the ipNetToMedia Table of the selected hosts." 
	"    Ethernet addresses are converted to vendor names using" 
	"    the Ethernet-Code-List available from MIT." 
	"" 
	"TCP -> TCP Statistics" 
	"    Statistics and parameter of the TCP layer." 
	"" 
	"TCP -> TCP Connections:" 
	"    Display the status of existing TCP connections." 
	"" 
	"UDP -> UDP Statistics" 
	"    Statistics and parameter of the UDP layer." 
	"" 
	"UDP -> UDP Listener:" 
	"    Display the status of existing UDP listener." 
	"" 
	"ICMP Statistics:" 
	"    Statistics and parameter of the ICMP layer." 
	"" 
	"SNMP Statistics:" 
	"    Statistics and parameter of the SNMP layer." 
	"" 
	"Walk MIB Tree:" 
	"    Walk through the MIB tree and print the object values." 
	"" 
	"Show MIB Table:" 
	"    Show an arbitrary MIB table." 
	"" 
	"Show MIB Scalars:" 
	"    Show an arbitrary group of MIB scalars." 
	"" 
	"Set SNMP Parameter:" 
	"    This dialog allows you to set SNMP parameters like retries, " 
	"    timeouts, community name and port number. " 
    }
}

##
## Delete the menus created by this interpreter.
##

proc "Delete SNMP-Trouble" {list} {
    global menus
    foreach id $menus { ined delete $id }
    exit
}

set menus [ ined create MENU "SNMP-Trouble" \
    "SNMP Devices" "" \
    "System Information" "System Capabilities" "" \
    "Interfaces:IF Status" \
    "Interfaces:IF Parameter" \
    "Interfaces:IF Usage Statistics" \
    "Interfaces:IF Error Statistics" \
    "Interfaces:IF Quality" \
    "IP:IP Statistics" \
    "IP:IP Addresses" \
    "IP:IP Routing Table" \
    "IP:IP ARP Table" \
    "TCP:TCP Statistics" \
    "TCP:TCP Connections" \
    "UDP:UDP Statistics" \
    "UDP:UDP Listener" "" \
    "ICMP Statistics" \
    "SNMP Statistics" "" \
    "Walk MIB Tree" "Show MIB Table" "Show MIB Scalars" "" \
    "Set SNMP Parameter" "" \
    "Help SNMP-Trouble" "Delete SNMP-Trouble" ]

vwait forever
