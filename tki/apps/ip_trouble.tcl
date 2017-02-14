#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# ip_trouble.tcl -
#
#	Simple troubleshooting scripts based on general IP services.
#
# Copyright (c) 1993-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1998-2000 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: ip_trouble.tcl,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $

package require Tnm 3.0
package require TnmInet $tnm(version)

namespace import Tnm::*

ined size
LoadDefaults icmp
IpInit IP-Trouble

##
## Send a icmp echo request to all addresses on a given network.
##

proc netping {network} {
    if {[regexp {^[0-9]+\.[0-9]+\.[0-9]+$} $network]>0} {
	set hosts ""
	for {set a4 1} {$a4<255} {incr a4} {
	    lappend hosts $network.$a4
	}
	if {[catch {icmp -timeout 15 echo $hosts} res]} {
	    set res ""
	}
	return $res
    }
    if {[regexp {^[0-9]+\.[0-9]+$} $network]>0} {
	set result ""
	for {set a3 0} {$a3<256} {incr a3} {
	    lappend result [netping $network.$a3]
	}
	return "[join $result]"
    }
}

##
## IP Troubleshooter Tool for INED
##

proc Ping {list} {

    set nodes ""
    set networks ""
    foreach comp $list {
	set type [ined type $comp]
        if {$type=="NODE"}    { lappend nodes $comp }
	if {$type=="NETWORK"} { lappend networks $comp }
    }

    if {[llength $nodes]>0} {
	foreach comp $nodes {
	    set host [lindex [ined name $comp] 0]
	    set ip [GetIpAddress $comp]
	    if {$ip==""} {
		ined acknowledge "Can not lookup IP Address for $host."
		continue
	    }
	    if {[catch {icmp -size 44 -timeout 5 echo $ip} rtt]} {
		writeln "Could not send icmp packet: $rtt"
		writeln
		return
	    }
	    set rtt [lindex $rtt 1]
	    if {[string length $rtt]} {
		write   "Round trip time for $host \["
		write   $ip "IpFlash $ip"
		writeln "\]: $rtt ms"
	    } else {
		write   "$host \["
		write   $ip "IpFlash $ip"
		writeln "\] not reachable."
	    }
	}
	writeln
    }

    if {[llength $networks]>0} {
	foreach comp $networks {
	    set net [ined name $comp]
	    set ip [ined address $comp]
	    if {![regexp {^[0-9]+\.[0-9]+\.[0-9]+$} $ip]} {
		ined acknowledge "Can not lookup IP Address for $net."
		continue
	    }
	    writeln "Network ping for $net \[$ip\]:"
	    foreach {pr_ip pr_time} [netping $ip] {
		if {[string length $pr_time]} {
		    if [catch {nslook $pr_ip} pr_host] {
			set pr_host $pr_ip
		    }
		    write   "  Round trip time for [lindex $pr_host 0] \["
		    write   $pr_ip "IpFlash $pr_ip"
		    writeln "\]: $pr_time ms"
		}
	    }
	    writeln
	}
    }
}

##
## Ping a host with a series of packets with varying packet sizes.
##

proc "Multi Ping" {list} {
    static increment
    static maxsize
    
    if ![info exists increment] { set increment 128 }
    if ![info exists maxsize]   { set maxsize 2048 }
    
    set result [ined request "Send icmp Packets with varying sizes." \
	    [list [list "Increment \[bytes\]:" $increment scale 1 512] \
	    [list "Max. packet size \[bytes\]:" $maxsize scale 64 2048] \
	    ] [list start cancel] ]
    
    if {[lindex $result 0] == "cancel"} return
    
    set increment [lindex $result 1]
    set maxsize   [lindex $result 2]

    foreach comp $list {
        if {[ined type $comp]=="NODE"} {
	    set host [lindex [ined name $comp] 0]
            set ip [GetIpAddress $comp]
            if {$ip==""} {
                ined acknowledge "Can not lookup IP Address for $host."
                continue;
            }
	    writeln "Multi ping for $host \[$ip\]:"
	    for {set size 44} {$size <= $maxsize} {incr size $increment} {
		if {[catch {icmp -size $size echo $ip} time]} {
		    writeln "Could not send icmp packet: $time"
		    writeln
		    return
		}
		set time [lindex $time 1]
		if [string length $time] {
		    writeln [format "  Round trip time (%5s bytes) %5s ms" \
			     $size $time]
		} else {
		    writeln [format "  No answer for   (%5s bytes)" $size]
		}
	    }
	    writeln
	}
    }
}

##
## Trace the route to a host using the van Jakobsen method.
##

proc "Trace Route" {list} {
    global icmp_routelength icmp_retries

    ForeachIpNode id ip host $list {
	if {[catch {TnmInet::TraceRoute $ip $icmp_routelength $icmp_retries} txt]} {
	    writeln "Can not trace route to $host: $txt"
	    continue
	}
	writeln "IP route to $host \[$ip\]:\n$txt\n"
    }
}

##
## Get the netmask using a special icmp request.
##

proc "Netmask" {list} {
    ForeachIpNode id ip host $list {
	if {[catch {icmp mask $ip} result]} {
	    writeln "Could not send icmp packet: $result"
	    writeln
	    return
	}
	set result [lindex $result 1]
	writeln "Netmask for $host \[$ip\]: $result"
    }
    writeln
}

##
## Open an xterm with an ssh for every selected node.
##

proc Ssh {list} {
    global env tcl_platform

    set xterm xterm
    set ssh ssh
    set sshusername root
    set path ""

    if [info exists env(PATH)] {
	set path [split $env(PATH) :]
    }

    foreach d [concat $path /usr/bin/X11 /usr/openwin/bin] {
	if {[file exists $d/xterm]} {
	    set xterm $d/xterm
	}
	if {[file exists $d/ssh]} {
	    set ssh $d/ssh
	}
    }

    ForeachIpNode id ip host $list {
	set port [ined attribute $id IP-Trouble:SshPort]
	switch $tcl_platform(platform) {
	    unix {
		if {$port != ""} {
		    catch {exec $xterm -title Ssh\ remote\ session\ on\ $host -e $ssh -l $sshusername $ip -p $port &}
		} else {
		    catch {exec $xterm -title Ssh\ remote\ session\ on\ $host -e $ssh -l $sshusername $ip &}
		}
	    }
	    windows {
		if {$port != ""} {
		    catch {exec $ssh -l $sshusername $ip $port &}
		} else {
		    catch {exec $ssh -l $sshusername $ip &}
		}
	    }
	    default {
		ined acknowledge "Ssh not supported on this platform."
                return
	    }
	}
    }
}

##
## Open an xterm with a telnet for every selected node.
##

proc Telnet {list} {
    global env tcl_platform

    set xterm xterm
    set path ""
    if [info exists env(PATH)] {
	set path [split $env(PATH) :]
    }
    foreach d [concat $path /usr/bin/X11 /usr/openwin/bin] {
	if {[file exists $d/xterm]} {
	    set xterm $d/xterm
	}
    }

    ForeachIpNode id ip host $list {
	set port [ined attribute $id IP-Trouble:TelnetPort]
	switch $tcl_platform(platform) {
	    unix {
		if {$port != ""} {
		    catch {exec $xterm -title $host -e telnet $ip $port &}
		} else {
		    catch {exec $xterm -title $host -e telnet $ip &}
		}
	    }
	    windows {
		if {$port != ""} {
		    catch {exec telnet $ip $port &}
		} else {
		    catch {exec telnet $ip &}
		}
	    }
	    default {
		ined acknowledge "Telnet not supported on this platform."
                return
	    }
	}
    }
}

##
## Open a Vnc session for every selected node.
##

proc Vnc {list} {
    global env tcl_platform

    set vnc vncviewer
    set path ""
    if [info exists env(PATH)] {
	set path [split $env(PATH) :]
    }
    foreach d [concat $path /usr/bin/X11 /usr/openwin/bin] {
	if {[file exists $d/vncviewer]} {
	    set vnc $d/vncviewer
	}
    }

    ForeachIpNode id ip host $list {
	set port [ined attribute $id IP-Trouble:VncPort]
	switch $tcl_platform(platform) {
	    unix {
		if {$port != ""} {
		    catch {exec $vnc -name=Vnc\ Remote\ management\ on\ $host $ip $port &}
		} else {
		    catch {exec $vnc -name=Vnc\ Remote\ management\ on\ $host $ip &}
		}
	    }
	    windows {
		if {$port != ""} {
		    catch {exec $vnc $ip $port &}
		} else {
		    catch {exec $vnc $ip &}
		}
	    }
	    default {
		ined acknowledge "Vnc not supported on this platform."
                return
	    }
	}
    }
}

##
## Ask the inetd of the selected nodes for the daytime.
##

proc Daytime {list} {
    ForeachIpNode id ip host $list {
	if {[catch {TnmInet::DayTime $ip} txt]} {
	    writeln "Can not get daytime from $host: $txt"
	    continue
	}
	writeln "Daytime for $host \[$ip\]: $txt"
    }
    writeln
}

##
## Connect to the finger port and get what they are willing to tell us.
## This is also nice for cisco routers.
##

proc Finger {list} {
    ForeachIpNode id ip host $list {
	if {[catch {TnmInet::Finger $ip} txt]} {
	    writeln "Can not finger $host: $txt"
	    continue
	}
	writeln "Finger for $host \[$ip\]:\n$txt\n"
    }
}

##
## Get the host information from the name server. This should really
## go to the troubleshooter. And it should use the buildin hostname
## lookup command.
##

proc "DNS Info" {list} {
    ForeachIpNode id ip host $list {
	if {[catch {dns name $ip} name]} { set name $host }
        set name [lindex $name 0]
	if {[catch {dns address $name} a]}       { set a "" }
	if {[catch {dns name [lindex $a 0]} ptr]} { set ptr "" }
	if {[catch {dns hinfo $name} hinfo]}     { set hinfo "" }
	if {[catch {dns mx $name} mx]}           { set mx "" }

	set soa ""
	while {([llength [split $name .]]>0) && ($soa=="")} {
	    if {[catch {dns soa $name} soa]} { set soa "" }
	    set name [join [lrange [split $name .] 1 end] .]
	}
	
	writeln "Domain Name Server information for $host \[$ip\]:"
	if {$ptr != ""} { writeln "Name:    $ptr" }
	if {$a   != ""} { writeln "Address: $a" }
	if {[lindex $hinfo 0] != ""} { writeln "CPU:     [lindex $hinfo 0]" }
	if {[lindex $hinfo 1] != ""} { writeln "OS:      [lindex $hinfo 1]" }
	foreach m $mx {
	    writeln "MX:      $m"
	}
	if {$soa != ""} { writeln "SOA:     $soa" }
	
	writeln
    }
}

##
## Whois information directly from whois.internic.net. The static
## array whois is used to cache whois information to reduce server
## load. The magic regular expression is used to strip away unwanted
## comments send by the whois server.
##

proc WhoisInfo { server list } {

    static whois

    foreach comp $list {
	set type [ined type $comp]
	if {$type == "NETWORK" || $type == "NODE"} {

	    if {$type == "NODE"} {
		set ip [GetIpAddress $comp]
	    } else {
		set ip [lindex [ined address $comp] 0]
	    }
	    if {$ip == ""} continue

	    set bytes [split $ip .]

	    if {[lindex $bytes 0] == 127} {
		continue
	    } elseif {[lindex $bytes 0] < 127} {
		set ip "[lindex $bytes 0].0.0.0"
	    } elseif {[lindex $bytes 0] < 192} {
		set ip "[lindex $bytes 0].[lindex $bytes 1].0.0"
	    } elseif {[lindex $bytes 0] < 224} {
		set ip "[lindex $bytes 0].[lindex $bytes 1].[lindex $bytes 2].0"
	    }

	    if {! [info exists whois($ip:$server)]} {

		if {[catch {socket $server whois} s]} {
		    writeln "Can not connect to whois server $server."
		    continue
		}
		
		puts $s $ip
		flush $s
		set answer ""
		while {![eof $s]} {
		    gets $s line
		    if {![regexp {^The InterNIC|^\(Netw|^Please} $line]} {
			append answer "$line\n"
		    }
		}
		close $s
		set whois($ip:$server) $answer
	    }
            writeln "Whois answer from $server:\n"
	    writeln $whois($ip:$server)
	}
    }
}

proc "Whois arin.net" { list } {
    WhoisInfo whois.arin.net $list
}

proc "Whois ripe.net" { list } {
    WhoisInfo whois.ripe.net $list
}

proc "Whois apnic.net" { list } {
    WhoisInfo whois.apnic.net $list
}

proc "Whois nic.mil" { list } {
    WhoisInfo whois.nic.mil $list
}

proc "Whois nic.gov" { list } {
    WhoisInfo whois.nic.gov $list
}

##
## Show some information retrieved via the network time protocol.
##

proc "NTP Info" { list } {

    ForeachIpNode id ip host $list {
	if {[catch {ntp $ip result} err]} {
	    writeln "$host \[$ip\]: $err"
	} else {
	    writeln "NTP information for $host \[$ip\]:"
	    foreach name [lsort [array names result]] {
		writeln "  $name = $result($name)"
	    }
	}
	writeln
	catch {unset result}
    }
}

##
## Scan the services file and try to connect to all ports
## listed there. This will not really tell us, if we could you
## a service but it may be a hint.
##

proc "TCP Services" {list} {
    ForeachIpNode id ip host $list {
	if {[catch {TnmInet::TcpServices $ip} txt]} {
	    writeln "Can not test TCP services on $host: $txt"
	    continue
	}
	writeln "TCP services on $host \[$ip\]:\n$txt\n"
    }
}

##
## Ask the portmapper of registered RPC services for the selected nodes.
##

proc "RPC Services" {list} {
    ForeachIpNode id ip host $list {
	if {[catch {TnmInet::RpcServices $ip} txt]} {
	    writeln "Can not test RPC services on $host: $txt"
	    continue
	}
	writeln "RPC services on $host \[$ip\]:\n$txt\n"
    }
}

##
## Get the NFS exports list by querying the mountd of the selected nodes.
##

proc "NFS Exports" {list} {
    ForeachIpNode id ip host $list {
	if {[catch {sunrpc exports $ip} inf]} {
	    ined acknowledge "Can not connect to $host."
	    continue
	}
	if {$inf == ""} {
	    ined acknowledge "No filesystems exported by $host."
	    continue
	}
	catch {unset fstab}
	writeln "Exported NFS Filesystems for $host \[$ip\]:"
	foreach fs $inf {
	    writeln "  [lindex $fs 0] -> [join [lrange $fs 1 end]]"
	}
	writeln
    }
}

##
## Show who has mounted partitions of the selected nodes.
##

proc "NFS Mounts" {list} {
    ForeachIpNode id ip host $list {
	if {[catch {sunrpc mount $ip} inf]} {
	    ined acknowledge "Can not connect to $host."
	    continue
	}
	if {$inf == ""} {
	    ined acknowledge "No filesystems mounted from $host."
	    continue
	}
	catch {unset fstab}
	foreach fs $inf {
	    set fsname [lindex $fs 0]
	    set mname  [join [lrange $fs 1 end]]
	    if {![info exists fstab($fsname)]} {
		set fstab($fsname) $mname
	    } else {
		lappend fstab($fsname) $mname
	    }
	}
	writeln "Mounted NFS Filesystems from $host \[$ip\]:"
	foreach fsname [lsort -ascii [array names fstab]] {
	    writeln "  $fsname -> $fstab($fsname)"
	}
	writeln
    }
}

##
## Make a call to the rstat daemon and write the results compared to
## the last call to the log window associated with this job.
##

proc RstatProc {} {
    set j [job current]
    set log [$j attribute log]
    if {[ined retrieve $log] == ""} {
	$j destroy
    } else {
	set host [$j attribute host]
	set stat [$j attribute stat]
	if {[catch {sunrpc stat $host} new]} {
	    ined append $log "\nFatal error: $new"
            $j destroy
	    return
	}
	set time [expr [$j cget -interval] / 1000]

	foreach {n1 t1 v1} [join $stat] {n2 t2 v2} [join $new] {
	    switch $t1 {
		Counter {
		    set r($n2) [expr int(double($v2 - $v1) / $time)]
		}
		Gauge {
		    set r($n2) [expr $v2/256.0]
		}
		default {
		    set r($n2) $v2
		}
	    }
        }
	
	ined append $log [format "%2.2f %2.2f %2.2f  %3d %3d %3d %5d %5d %3d%3d%3d%3d %4d%4d%4d%4d %4d %4d\n" \
	    $r(avenrun_0) $r(avenrun_1) $r(avenrun_2) \
	    [expr $r(cp_user) + $r(cp_nice)] $r(cp_system) $r(cp_idle) \
	    $r(v_swtch) $r(v_intr) \
	    $r(dk_xfer_0) $r(dk_xfer_1) $r(dk_xfer_2) $r(dk_xfer_3) \
	    $r(v_pgpgin) $r(v_pgpgout) $r(v_pswpin) $r(v_pswpout) \
	    $r(if_ipackets) $r(if_opackets) \
	 ]
	
	$j attribute stat $new
    }
}

##
## Create a new output window for each selected host and start a
## job to monitor the remote host status using rstat RPC calls.
##

proc "Host Status" {list} {

    static interval
    if {![info exists interval]} { set interval 10 }

    set interval [ined request "Show remote host status (rstat)" \
		   [list [list "Interval \[s\]:" $interval scale 5 60] ] \
		   [list start cancel] ]

    if {[lindex $interval 0] == "cancel"} return

    set interval [lindex $interval 1]
    ForeachIpNode id ip host $list {
	if {[catch {sunrpc stat $host} result]==0} {
	    set log [ined create LOG]
	    ined name $log "$host host status"
	    ined append $log "    load            cpu         traps        disk            vm           net  \n"
	    ined append $log " l5   l10  l15   us  sy  id    in    cs  d0 d1 d2 d3   pi  po  si  so   in  out\n"
	    set j [job create \
		    -command RstatProc -interval [expr $interval * 1000]]
	    $j attribute host $host
	    $j attribute stat [sunrpc stat $host]
	    $j attribute log  $log
	} else {
	    ined acknowledge "rstat RPC failed for $host: $result"
	}
    }
}

##
## Make a call to the etherstat daemon and write the results to the
## log window associated with this job.
##

proc EtherProc {} {
    set j [job current]
    set host [$j attribute host]
    set log  [$j attribute log]
    if {[ined retrieve $log] == ""} {
	catch {sunrpc ether $host close}
	$j destroy
    } else {
	if {[catch {sunrpc ether $host} stat]} {
	    ined append $log "\nFatal error: $stat"
	    $j destroy
	    return
	}

	foreach {n t v} [join $stat] {
	    if {[string match *-* $n]} {
		set line [format "%9s =%5s " $n $v]
		for {set i 0} {$i <= $v/100} {incr i} {
		    append line *
		}
		append graph "$line\n"
	    } else {	
		set r($n) $v
	    }
        }
	
	set mbits         [expr {$r(bytes)*8/1000000.0}]
        set time          [expr {$r(time)/1000.0}]
        set mbits_per_s   [expr {$mbits/$time}]
        set packets_per_s [expr {$r(packets)/$time}]

	set icmp_p	  [expr $r(icmp) * 100.0 / $r(packets)]
	set udp_p	  [expr $r(udp) * 100.0 / $r(packets)]
	set tcp_p	  [expr $r(tcp) * 100.0 / $r(packets)]
	set arp_p	  [expr $r(arp) * 100.0 / $r(packets)]
	set other_p	  [expr ($r(other) + $r(nd)) * 100.0 / $r(packets)]

	append txt \
		[format "\nTraffic: %.3f MBit/s %.1f Packets/s   Time: %s   Interval: %.1f s\n" \
		$mbits_per_s $packets_per_s [clock format [clock seconds] -format %X] $time]
	append txt \
		[format "Distribution: icmp %.2f %% udp %.2f %% tcp %.2f %% arp %.2f %% other %.2f %%\n\n" \
		$icmp_p $udp_p $tcp_p $arp_p $other_p]
		
	ined append $log $txt
	ined append $log $graph
    }
}

##
## Make a call to the ether daemon and report the results for
## a time interval which is queried from the user.
##

proc "Ether Status" {list} {

    static interval
    if {![info exists interval]} { set interval 10 }

    set interval [ined request "Show ether results (etherd)" \
		   [list [list "Interval \[s\]:" $interval scale 5 60] ] \
		   [list start cancel] ]

    if {[lindex $interval 0] == "cancel"} return

    set interval [lindex $interval 1]
    ForeachIpNode id ip host $list {
	if {[catch {sunrpc ether $ip open} result] == 0} {
	    set log [ined create LOG]
	    ined name $log "$host ether status"
	    set j [job create \
		    -command EtherProc -interval [expr $interval * 1000]]
	    $j attribute host $host
	    $j attribute log $log
	} else {
	    ined acknowledge "etherstat RPC failed for $host"
	}
    }
}

##
## Set some constants that control the tool.
##

proc "Set Parameter" {list} {
    global icmp_retries icmp_timeout icmp_delay
    global icmp_routelength

    set result [ined request "IP Trouble Parameter" \
	[list [list "# of ICMP retries:" $icmp_retries scale 1 10] \
          [list "ICMP timeout \[s\]:" $icmp_timeout scale 1 42] \
          [list "Delay between ICMP packets \[ms\]:" $icmp_delay scale 1 100] \
          [list "Max. length of a route:" $icmp_routelength scale 1 42] ] \
	[list "set values" cancel] ]

    if {[lindex $result 0] == "cancel"} return

    set icmp_retries     [lindex $result 1]
    set icmp_timeout     [lindex $result 2]
    set icmp_delay       [lindex $result 3]
    set icmp_routelength [lindex $result 4]

    icmp -retries $icmp_retries
    icmp -timeout $icmp_timeout
    icmp -delay   $icmp_delay
}

##
## Display some help about this tool.
##

proc "Help IP-Trouble" {list} {
    ined browse "Help about IP-Trouble" {
	"Ping:" 
	"    Send an ICMP echo request to all selected objects. If you" 
	"    select a network with a valid address, then all address of" 
	"    address space will be queried (be careful!)." 
	"" 
	"Multi Ping:" 
	"    Send ICMP echo request with varying packet size." 
	"" 
	"Netmask:" 
	"    Send ICMP netmask request to all selected nodes." 
	"" 
	"Trace Route:" 
	"    Trace the route to the selected nodes." 
	"" 
	"Ssh:" 
	"    Open an ssh session to the selected nodes. You can" 
	"    define an alternate port number by setting the attribute" 
	"    IP-Trouble:SshPort." 
	"" 
	"Telnet:" 
	"    Open a telnet session to the selected nodes. You can" 
	"    define an alternate port number by setting the attribute" 
	"    IP-Trouble:TelnetPort." 
	"" 
	"Vnc:" 
	"    Open a vnc session to the selected nodes. You can" 
	"    define an alternate port number by setting the attribute" 
	"    IP-Trouble:VncPort." 
	"" 
	"Daytime:" 
	"    Get the daytime of all selected nodes." 
	"" 
	"Finger:" 
	"    Finger all selected nodes." 
	"" 
	"DNS Info:" 
	"    Show information stored in the Domain Name Service." 
	"" 
	"Whois Info:" 
	"    Query the whois server whois.internic.net for information" 
	"    about a network." 
	"" 
	"NTP Info:" 
	"    Display some status information about the time on the selected" 
	"    hosts. Uses the network time protocol (ntp)." 
	"" 
	"TCP Services:" 
	"    Find available TCP services on all selected nodes." 
	"" 
	"RPC Services:" 
	"    Find available RPC services on all selected nodes." 
	"" 
	"NFS Exports:" 
	"    List the NFS export list of all selected nodes." 
	"" 
	"NFS Mounts:" 
	"    Show who has mounted the selected nodes." 
	"" 
	"Host Status:" 
	"    Display the host status (obtained via the rstat RPC service):" 
	"" 
	"    load l5:  system load average over  5 minutes" 
	"    load l10: system load average over 10 minutes" 
	"    load l15: system load average over 15 minutes" 
	"" 
	"    cpu us: percentage user time" 
	"    cpu sy: percentage system time" 
	"    cpu id: percentage idle time" 
	"" 
	"    traps in: interrupts per second" 
	"    traps cs: context switches per second" 
	"" 
	"    disk d0: utilization of disks d0" 
	"    disk d1: utilization of disks d1" 
	"    disk d2: utilization of disks d2" 
	"    disk d3: utilization of disks d3" 
	"" 
	"    vm pi: number of pageins per second" 
	"    vm po: number of pageouts per second" 
	"    vm si: number of swapins per second" 
	"    vm so: number of swapouts per second" 
	"" 
	"    net in:  packets reveived per second" 
	"    net out: packets send per second" 
	"" 
	"Ether Status:" 
	"    Display the results of a calls to the etherstat RPC service." 
	"" 
	"Set Parameter:" 
	"    Display and change parameters of the tools." 
    }
}

##
## Delete the menus created by this interpreter.
##

proc "Delete IP-Trouble" {list} {
    global menus
    foreach id $menus { ined delete $id }
    exit
}

set menus [ ined create MENU "IP-Trouble" \
	    Ping "Multi Ping" "Netmask" "Trace Route" "" \
	    Ssh Telnet Vnc Daytime Finger "DNS Info" \
	    "Whois Info:Whois arin.net" "Whois Info:Whois ripe.net" \
	    "Whois Info:Whois apnic.net" \
	    "Whois Info:Whois nic.mil" "Whois Info:Whois nic.gov" \
	    "NTP Info" "" \
	    "TCP Services" "RPC Services" "" \
	    "NFS Exports" "NFS Mounts" \
	    "Host Status" "Ether Status" "" \
	    "Set Parameter" "" \
	    "Help IP-Trouble" "Delete IP-Trouble" ]

vwait forever
