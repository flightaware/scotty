#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# ip_monitor.tcl -
#
#	Simple IP monitoring tools mostly based on Sun RPC calls
#	and ICMP test messages.
#
# Copyright (c) 1993-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: ip_monitor.tcl,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $

package require Tnm 3.0

namespace import Tnm::*

ined size
LoadDefaults icmp monitor
IpInit IP-Monitor

##
## Set up some default parameters.
##

if {[info exists default(interval)]} {
    set interval $default(interval)
} else {
    set interval 60
}
if {![info exists default(graph)]} {
    set default(graph) false
}

##
## Create a chart. Honours the monitor.graph default definition.
##

proc CreateChart {id x y} {
    global default
    if {[ined type $id] != "STRIPCHART"} {
	if {$default(graph) != "true"} {
	    set id [CloneNode $id [ined create STRIPCHART] $x $y]
	} else {
	    set id [CloneNode $id [ined create GRAPH]]
	}
    }
    return $id
}

##
## Tell tkined to restart a command when a saved map gets loaded.
##

proc save { cmd } {
    set cmdList [ined restart]
    if {[lsearch $cmdList $cmd] >= 0} return
    lappend cmdList $cmd
    ined restart $cmdList
}

##
## Remove a command from the list of commands saved in the tkined map.
##

proc forget { cmd } {
    set cmdList ""
    foreach c [ined restart] {
	if {$c != $cmd} {
	    lappend cmdList $c
	}
    }
    ined restart $cmdList
}

##
## Restart a monitor job by calling cmd. Build the list of objects
## from tkined using ined retrieve commands.
##

proc restart { cmd args } {

    global interval

    if {! [catch {expr [lindex $args 0] + 0}]} {
	set itv  [lindex $args 0]
	set jid  [lindex $args 1]
	set args [lrange $args 2 end]
	set old_interval $interval
	set interval [expr int($itv)]
    }

    set ids $args
    foreach id $ids {
	catch {ined -noupdate clear $id}
	catch {ined -noupdate clear $id}
	lappend list [ined retrieve $id]
    }
    catch { $cmd $list } err

    if {[info exists old_interval]} {
	set interval $old_interval
    }
}

##
## Extract all nodes of the given object list with a valid IP
## address that are reachable. Return a list of id/ip pairs.
##

proc extract { list } {

    set id_ip_list ""

    foreach comp $list {
	set type [ined type $comp]
        if {[lsearch "NODE STRIPCHART BARCHART GRAPH" $type] >= 0} {
            set id [ined id $comp]
            set host [lindex [ined name $comp] 0]
            set ip [GetIpAddress $comp]
            if {$ip == ""} {
                ined acknowledge "Can not lookup IP Address for $host."
                continue
            }
	    lappend id_ip_list [list $id $ip]
        }
    }

    return $id_ip_list
}

##
## Compute the difference between two rstat calls.
##

proc ComputeStatDelta {l1 l2} {
    set l1 [join $l1]
    set l2 [join $l2]
    foreach {name type value} $l1 {
	set s1($name) $value
    }
    foreach {name type value} $l2 {
	set s2($name) $value
    }
    if {$s1(boottime) != $s2(boottime)} {
	return ""
    }
    set r(time) [expr $s2(curtime) - $s1(curtime)]

    foreach {n1 t1 v1} $l1 {n2 t2 v2} $l2 {
	switch $t1 {
	    Counter {
		set r($n1) [expr $r(time) > 0 ? double($v2-$v1)/$r(time) : 0]
	    }
	    Gauge {
		set r($n1) [expr $v2/256.0]
	    }
	    default {
		set r($n1) $v2
	    }
	}
    }
    return [array get r]
}

proc rstat_diff {l1 l2 period} {
    set len [llength $l1]
    set res ""
    for {set i "0"} {$i < $len} {incr i} {
        set el1 [lindex $l1 $i]
        set el2 [lindex $l2 $i]
        set tmp [lindex $el1 2]
        if {[lindex $el1 1] == "Counter"} {
            set tmp [expr {[lindex $el1 2]-[lindex $el2 2]}]
	    if {$period <= 0} {
		set tmp 0
	    } else {
		set tmp [expr {"$tmp.0" / $period}]
	    }
	}
        if {[lindex $el1 1] == "Gauge"} {
            set tmp [expr {[lindex $el1 2]/256.0}]
        }
        lappend res [format "%-12s %-12s %16.3f" \
		     [lindex $el1 0] [lindex $el1 1] $tmp]
    }
    return $res
}

##
## Send an ICMP request periodically to test if the selected nodes
## are reachable. This routine makes use of the multi threaded ping 
## mechanism.
##

proc ReachabilityProc {ids} {

    global reachability_ip reachability_color reachability_label

    set ids [MoJoCheckIds ReachabilityProc $ids]
    if {$ids == ""} return

    set id_list ""
    set ip_list ""

    foreach id $ids {
	set ip $reachability_ip($id)
	lappend id_list $id
	lappend ip_list $ip
    }
	
    if {$ip_list != ""} {
	foreach id $id_list {ip rtt} [icmp echo $ip_list] {
	    if {![string length $rtt]} {
		if {$reachability_color($id) == ""} {
		    set reachability_color($id) [ined -noupdate color $id]
		    set reachability_label($id) [ined -noupdate label $id]
		    ined -noupdate color $id red
		}
		ined flash $id [expr {[[job current] cget -interval] / 1000}]
		ined -noupdate attribute $id \
		    "round trip time" "[ined name $id]\nunreachable"
		ined -noupdate label $id "round trip time"
		MoJoAction $id "[ined name $id] \[$ip\] unreachable"
	    } else {
		if {$reachability_color($id) != ""} {
		    ined -noupdate color $id $reachability_color($id)
		    set reachability_color($id) ""
		}
		if {$reachability_label($id) != ""} {
		    ined -noupdate label $id $reachability_label($id)
		    set reachability_label($id) ""
		}
		ined -noupdate attribute $id \
		    "round trip time" "rtt $rtt ms"
		set txt "[ined name $id] \[$ip\] round trip time"
		MoJoCheckThreshold $id $txt $rtt "ms"
	    }
	}
    }
}

##
## Send an ICMP request periodically to test if the selected nodes
## are reachable. This s the actual proc that is invoked from the 
## user interface.
##

proc "Check Reachability" {list} {

    global interval
    global reachability_ip reachability_color reachability_label
    global reachability_ids

    if {![info exists reachability_ids]} { set reachability_ids "" }

    foreach id_ip [extract $list] {
	set id [lindex $id_ip 0]
	set ip [lindex $id_ip 1]

	set reachability_ip($id)   $ip
	set reachability_color($id) ""
	set reachability_label($id) ""
	lappend ids $id
	if {[lsearch $reachability_ids $id] < 0} {
	    lappend reachability_ids $id
	}
    }

    if {[info exists ids]} {
	set jid [job create -command [list ReachabilityProc $ids] \
		 -interval [expr {$interval * 1000}]]
	foreach id $ids { append rsids "\$$id " }
	save "restart {Check Reachability} $interval $jid $rsids"
    }
}

##
## Display the round trip time in a strip chart.
##

proc RoundTripTimeProc {ids} {

    global pingtime_ip

    set ids [MoJoCheckIds RoundTripTimeProc $ids]
    if {$ids == ""} return

    set id_list ""
    set ip_list ""

    foreach id $ids {
	set ip $pingtime_ip($id)
	lappend id_list $id
	lappend ip_list $ip
    }
	
    if {$ip_list != ""} {
	foreach id $id_list {ip rtt} [icmp echo $ip_list] {
	    if {![string length $rtt]} {
		ined values $id 0
		ined -noupdate attribute $id \
		    "round trip time" "[ined name $id]\nunreachable"
		MoJoAction $id "[ined name $id] \[$ip\] unreachable"
	    } else {
		ined values $id [expr {$rtt/10.0}]
		ined -noupdate attribute $id \
		    "round trip time" "rtt $rtt ms"
		set txt "[ined name $id] \[$ip\] round trip time"
		MoJoCheckThreshold $id $txt $rtt "ms"
	    }
	}
    }
}

##
## Display the round trip time in a strip chart. This 
## is the actual proc that is invoked from the user interface.
##

proc "Round Trip Time" {list} {

    global interval
    global pingtime_ip

    foreach id_ip [extract $list] {
	set id [lindex $id_ip 0]
	set ip [lindex $id_ip 1]

	set id [CreateChart $id 15 15]
	ined -noupdate attribute $id "round trip time" "rtt"
	ined -noupdate label $id "round trip time"

	set pingtime_ip($id) $ip
	lappend ids $id
    }

    if {[info exists ids]} {
	set jid [job create -command [list RoundTripTimeProc $ids] \
		 -interval [expr {$interval * 1000}]]
	foreach id $ids { append rsids "\$$id " }
	save "restart {Round Trip Time} $interval $jid $rsids"
    }
}


##
## Display the ntp offset of a peer:
##

proc NtpOffsetProc {ids} {

    global ntp_offset_ip

    set ids [MoJoCheckIds NtpOffsetProc $ids]
    if {$ids == ""} return

    foreach id $ids {
	set ip $ntp_offset_ip($id)
	
	if {[catch {ntp $ip ntp} err]} {
	    ined -noupdate attribute $id "ntp offset" "$err"
	    ined values $id 0
	} else {
	    if {[info exists ntp(peer.offset)]} {
		set offset $ntp(peer.offset)
		ined -noupdate attribute $id "ntp offset" "$offset ms"
		set val [expr abs($offset)]
		ined values $id $val
		set txt "[ined name $id] \[$ip\] ntp offset"
		MoJoCheckThreshold $id $txt $val "ms"
	    }
	    if {[info exists ntp(peer.srcadr)]} {
		set srcadr [nslook $ntp(peer.srcadr)]
		ined -noupdate attribute $id "ntp peer" $srcadr
	    } else {
		ined -noupdate attribute $id "ntp offset" "no ntp peer"
	    }
	}
    }
}

##
## Display the round trip time in a strip chart. This 
## is the actual proc that is invoked from the user interface.
##

proc "NTP Offset" {list} {

    global interval
    global ntp_offset_ip

    foreach id_ip [extract $list] {
	set id [lindex $id_ip 0]
	set ip [lindex $id_ip 1]

	set id [CreateChart $id 15 15]
	ined -noupdate attribute $id "ntp offset" "offset"
	ined -noupdate label $id "ntp offset"

	set ntp_offset_ip($id) $ip
	lappend ids $id
    }

    if {[info exists ids]} {
	set jid [job create -command [list NtpOffsetProc $ids] \
                 -interval [expr {$interval * 1000}]]
	foreach id $ids { append rsids "\$$id " }
	save "restart {NTP Offset} $interval $jid $rsids"
    }
}


##
## Show the load given by the rstat RPC in a stripchart.
##

proc SystemLoadProc {ids} {

    global sysload_ip
    global sysload_stat

    set ids [MoJoCheckIds SystemLoadProc $ids]
    if {$ids == ""} return

    foreach id $ids {
	set ip $sysload_ip($id)

	if {[catch {sunrpc stat $ip} res]} {
	    ined -noupdate values $id 0
	    ined -noupdate attribute $id "system load" $res
	    continue
	}
	
	array set r [ComputeStatDelta $sysload_stat($id) $res]
	set sysload_stat($id) $res
	if {[array size r] == 0} continue

	set load [format "%.2f" $r(avenrun_0)]

	ined -noupdate values $id $load
	ined -noupdate attribute $id "system load" "load $load"
	set txt "[ined name $id] \[$ip\] system load"
	MoJoCheckThreshold $id $txt $load
    }
}

##
## Show the load given by the rstat RPC in a stripchart. This 
## is the actual proc that is invoked from the user interface.
##

proc "System Load" {list} {

    global interval
    global sysload_ip
    global sysload_stat

    foreach id_ip [extract $list] {
	set id [lindex $id_ip 0]
	set ip [lindex $id_ip 1]

	if {[catch {sunrpc stat $ip} res]} {
	    ined acknowledge "[ined name $id] \[$ip\]: $res"
	    continue
	}

        set id [CreateChart $id 35 25]
	catch {ined -noupdate scale $id 1}
	ined -noupdate attribute $id "system load" "load"
	ined -noupdate label $id "system load"

	set sysload_ip($id)   $ip
	set sysload_stat($id) $res

	lappend ids $id
    }

    if {[info exists ids]} {
	set jid [job create -command [list SystemLoadProc $ids] \
                 -interval [expr {$interval * 1000}]]
	foreach id $ids { append rsids "\$$id " }
	save "restart {System Load} $interval $jid $rsids"
    }
}

##
## Show the cpu time split given by the rstat RPC in a barchart.
##

proc CpuSplitProc {ids} {

    global cpusplit_ip
    global cpusplit_stat

    set ids [MoJoCheckIds CpuSplitProc $ids]
    if {$ids == ""} return

    lappend names cp_user cp_nice cp_system cp_idle

    foreach id $ids {
	set ip $cpusplit_ip($id)

	if {[catch {sunrpc stat $ip} res]} {
	    ined -noupdate values $id 0
	    ined -noupdate attribute $id "cpu activity" $res
	    continue
	}
	
	array set r [ComputeStatDelta $cpusplit_stat($id) $res]
	set cpusplit_stat($id) $res
	if {[array size r] == 0} continue

	set load ""
	foreach n $names {
	    lappend load $r($n)
	}

	ined -noupdate values $id $load
	ined -noupdate attribute $id "cpu activity" \
	    [eval format {"user %.1f nice %.1f\nsystem %.1f idle %.1f"} $load]
	set txt "[ined name $id] \[$ip\] cpu split"
	MoJoCheckThreshold $id $txt $load
    }
}

##
## Show the cpu time split given by the rstat RPC in a barchart. This
## is the actual proc that is invoked from the user interface.
##

proc "CPU Activity" {list} {

    global interval
    global cpusplit_ip
    global cpusplit_stat

    foreach id_ip [extract $list] {
	set id [lindex $id_ip 0]
	set ip [lindex $id_ip 1]

	if {[catch {sunrpc stat $ip} res]} {
	    ined acknowledge "[ined name $id] \[$ip\]: $res"
	    continue
	}

	if {[ined type $id] != "BARCHART"} {
	    set id [CloneNode $id [ined create BARCHART] 35 -35]
	}
	ined -noupdate attribute $id "cpu activity" "user nice system idle"
	ined -noupdate label $id "cpu activity"

	set cpusplit_ip($id)   $ip
	set cpusplit_stat($id) $res

	lappend ids $id
    }

    if {[info exists ids]} {
	set jid [job create -command [list CpuSplitProc $ids] \
                 -interval [expr {$interval * 1000}]]
	foreach id $ids { append rsids "\$$id " }
	save "restart {CPU Activity} $interval $jid $rsids"
    }
}

##
## Show the disk activity given by the rstat RPC in a barchart.
##

proc DiskLoadProc {ids} {

    global diskload_ip
    global diskload_stat

    set ids [MoJoCheckIds DiskLoadProc $ids]
    if {$ids == ""} return

    lappend names dk_xfer_0 dk_xfer_1 dk_xfer_2 dk_xfer_3
    lappend names v_pgpgin v_pgpgout v_pswpin v_pswpout

    foreach id $ids {
	set ip $diskload_ip($id)

	if {[catch {sunrpc stat $ip} res]} {
	    ined -noupdate values $id 0
	    continue
	}
    
	array set r [ComputeStatDelta $diskload_stat($id) $res]
	set diskload_stat($id) $res
	if {[array size r] == 0} continue

	set load ""
	foreach n $names {
	    lappend load [expr $r($n) * 2]
	}

	ined values $id $load
	set txt "[ined name $id] \[$ip\] disk load"
	MoJoCheckThreshold $id $txt $load
    }
}

##
## Show the disk activity given by the rstat RPC in a barchart. This
## is the actual proc that is invoked from the user interface.
##

proc "Disk Activity" {list} {

    global interval
    global diskload_ip
    global diskload_stat

    foreach id_ip [extract $list] {
	set id [lindex $id_ip 0]
	set ip [lindex $id_ip 1]

	if {[catch {sunrpc stat $ip} res]} {
	    ined acknowledge "[ined name $id] \[$ip\]: $res"
	    continue
	}

        if {[ined type $id] != "BARCHART"} {
	    set id [CloneNode $id [ined create BARCHART] -35 40]
	}
	ined -noupdate attribute $id "disk activity" "d0 d1 d2 d3 pi po si so"
	ined -noupdate label $id "disk activity"

	set diskload_ip($id)   $ip
	set diskload_stat($id) $res

	lappend ids $id
    }

    if {[info exists ids]} {
	set jid [job create -command [list DiskLoadProc $ids] \
                 -interval [expr {$interval * 1000}]]
	foreach id $ids { append rsids "\$$id " }
	save "restart {Disk Activity} $interval $jid $rsids"
    }
}

##
## Show the disk activity given by the rstat RPC in a barchart.
##

proc IfLoadProc {ids} {

    global ifload_ip
    global ifload_stat

    set ids [MoJoCheckIds IfLoadProc $ids]
    if {$ids == ""} return

    lappend names if_ipackets if_ierrors if_opackets if_oerrors

    foreach id $ids {
	set ip $ifload_ip($id)

	if {[catch {sunrpc stat $ip} res]} {
	    ined -noupdate values $id 0
	    continue
	}
	
	array set r [ComputeStatDelta $ifload_stat($id) $res]
	set ifload_stat($id) $res
	if {[array size r] == 0} continue

	set load ""
	foreach n $names {
	    lappend load $r($n)
	}

	ined values $id $load
	ined -noupdate attribute $id "interface activity" \
	    [eval format {"in %.1f error %.1f\nout %.1f error %.1f"} $load]
	set txt "[ined name $id] \[$ip\] interface load"
	MoJoCheckThreshold $id $txt $load
    }
}

##
## Show the interface activity given by the rstat RPC in a barchart. This
## is the actual proc that is invoked from the user interface.
##

proc "Interface Activity" {list} {

    global interval
    global ifload_ip
    global ifload_stat

    foreach id_ip [extract $list] {
	set id [lindex $id_ip 0]
	set ip [lindex $id_ip 1]

	if {[catch {sunrpc stat $ip} res]} {
	    ined acknowledge "[ined name $id] \[$ip\]: $res"
	    continue
	}

        if {[ined type $id] != "BARCHART"} {
	    set id [CloneNode $id [ined create BARCHART] -35 -40]
	}
	ined -noupdate attribute $id "interface activity" "in ierr out oerr"
	ined -noupdate label $id "interface activity"

	set ifload_ip($id)   $ip
	set ifload_stat($id) $res

	lappend ids $id
    }

    if {[info exists ids]} {
	set jid [job create -command [list IfLoadProc $ids] \
                 -interval [expr {$interval * 1000}]]
	foreach id $ids { append rsids "\$$id " }
	save "restart {Interface Activity} $interval $jid $rsids"
    }
}

##
## Show the ethernet load as reported by an etherstatd.
##

proc EtherLoadProc {ids} {

    global etherload_ip

    set ids [MoJoCheckIds EtherLoadProc $ids]
    if {$ids == ""} return

    foreach id $ids {
	set ip $etherload_ip($id)

	if {[catch {sunrpc ether $ip stat} load]} {
	    ined -noupdate attribute $id "ethernet load" $load
	    ined -noupdate values $id 0
	    continue
	}

	set time_ms [lindex [lindex $load 0] 2]
	set bytes   [lindex [lindex $load 1] 2]
	set packets [lindex [lindex $load 2] 2]
	set mbits   [expr {$bytes*8/1000000.0}]
	set time_s  [expr {$time_ms/1000.0}]
	set mbits_per_s   [expr {$mbits/$time_s}]
	set packets_per_s [expr {$packets/$time_s}]
	set bytes_per_s   [expr {$bytes*1.0/$packets}]
	ined -noupdate attribute $id "ethernet load" \
	    [format "%3.2f MBit/s\\n%4.1f Packet/s\\n %4.1f Bytes/Packet" \
	     $mbits_per_s $packets_per_s $bytes_per_s]
	set load [list [expr {$mbits_per_s*12.5}] \
		       [expr {$packets_per_s/20.0}] \
		       [expr {$bytes_per_s/10.0}] ]
	ined values $id $load
	set txt "[ined name $id] \[$ip\] ethernet load"
	MoJoCheckThreshold $id $txt $load
    }
}

##
## Show the ethernet load as reported by an etherstatd. This
## is the actual proc that is invoked from the user interface.
##

proc "Ethernet Load" {list} {

    global interval
    global etherload_ip

    foreach id_ip [extract $list] {
	set id [lindex $id_ip 0]
	set ip [lindex $id_ip 1]

	if {[catch {sunrpc ether $ip open} res]} {
	    if {[ined type $id] != "BARCHART"} {
		ined -noupdate attribute $id ""
            }
	    ined acknowledge "[ined name $id] \[$ip\]: $res"
            continue
	} else {

	    if {[ined type $id] != "BARCHART"} {
		set id [CloneNode $id [ined create BARCHART] -40 -25]
	    }
	    ined -noupdate attribute $id "ethernet load" "ethernet load"
	    ined -noupdate label $id "ethernet load"
	    set etherload_ip($id)   $ip
	    lappend ids $id
	}
    }

    if {[info exists ids]} {
	set jid [job create -command [list EtherLoadProc $ids] \
                 -interval [expr {$interval * 1000}]]
	foreach id $ids { append rsids "\$$id " }
	save "restart {Ethernet Load} $interval $jid $rsids"
    }
}

##
## Show the user connected to a host using the finger protocol.
##

proc ActiveUserProc {ids} {

    global users_ip

    set ids [MoJoCheckIds ActiveUserProc $ids]
    if {$ids == ""} return

    foreach id $ids {
        set ip $users_ip($id)
	if {[catch {socket $ip finger} f]} {
	    continue
	}
	if {[catch {puts $f ""}]} {
	    catch {close $f}
	    continue
	}
	if {[catch {flush $f}]} {
	    catch {close $f}
	    continue
	}
	catch {unset users}
	while {! [eof $f]} {
	    gets $f user
	    if {$user == ""} continue
	    if {[string match "Login*Name*" $user]} continue
	    if {[string match "No*one*" $user]} continue
	    if {[string match "*TTY*" $user]} continue
	    set user [lindex $user 0]
	    if {[info exists users($user)]} {
		incr users($user)
	    } else {
		set users($user) 1
	    }
	}
	close $f

	set txt ""
	foreach user [array names users] {
	    lappend txt "$user/$users($user)"
	}
	if {$txt == ""} {
	    set txt "no users"
	}

	ined -noupdate attribute $id "active users" $txt
    }
}

proc "Active Users" { list } {

    global interval
    global users_ip

    foreach id_ip [extract $list] {
	set id [lindex $id_ip 0]
	set ip [lindex $id_ip 1]

	if {[catch {socket $ip finger} f]} continue
	close $f

	ined -noupdate attribute $id "active users" "no users"
	ined label $id "active users"

	set users_ip($id) $ip

	lappend ids $id
    }

    if {[info exists ids]} {
        set jid [job create -command [list ActiveUserProc $ids] \
                 -interval [expr {$interval * 1000}]]
        foreach id $ids { append rsids "\$$id " }
        save "restart {Active Users} $interval $jid $rsids"
    }
}

##
## Display the jobs currently running.
##

proc "Monitor Job Info" { list } {
    MoJoInfo
}

##
## Modify the state or the interval of a running job.
##

proc "Modify Monitor Job" { list } {
    MoJoModify
}

##
## This simple dialog allows us to modify the monitor parameters.
##

proc "Set Parameter" {list} {

    global icmp_retries icmp_timeout icmp_delay
    global interval
    global default

    set result [ined request "Monitor Parameter" \
        [list [list "# of ICMP retries:" $icmp_retries scale 1 10] \
          [list "ICMP timeout \[s\]:" $icmp_timeout scale 1 42] \
          [list "Delay between ICMP packets \[ms\]:" $icmp_delay scale 1 100] \
	  [list "Interval \[s\]:" $interval entry 8] \
	  [list "Use Graph Diagram:" $default(graph) radio true false ] ] \
	[list "set values" cancel] ]

    if {[lindex $result 0] == "cancel"} return

    set icmp_retries   [lindex $result 1]
    set icmp_timeout   [lindex $result 2]
    set icmp_delay     [lindex $result 3]
    set interval       [lindex $result 4]
    set default(graph) [lindex $result 5]

    if {$interval<1} { set interval 1 }

    icmp -retries $icmp_retries
    icmp -timeout $icmp_timeout
    icmp -delay   $icmp_delay
}

##
## Show the defaults as loaded from the tkined.defaults files.
##

proc "Show Defaults" {list} {
    ShowDefaults
}

##
## Display some help about this tool.
##

proc "Help IP-Monitor" {list} {

    ined browse "Help about IP-Monitor" {
	"The IP-Monitor contains a set of utilities to monitor hosts" 
	"using simple ICMP messages or rstat/etherd RPCs. You can define" 
	"thresholds by setting the attributes Monitor:RisingThreshold" 
	"or Monitor:FallingThreshold. The attribute Monitor:ThresholdAction" 
	"controls what action will be taken if the rising threshold or" 
	"the falling threshold is exceeded. The action types are syslog," 
	"flash and write." 
	"" 
	"Check Reachability:" 
	"    This command will periodically send an echo ICMP request to the" 
	"    selected hosts and takes action when a host gets unreachable." 
	"" 
	"Round Trip Time:" 
	"    Send an ICMP echo request to the selected hosts and display the" 
	"    round trip time in a stripchart." 
	"" 
	"NTP Offset:" 
	"    Send an NTP mode 6 request to the selected hosts and display the" 
	"    referenz peer offset in a stripchart." 
	"" 
	"System Load:" 
	"    Query the rstat daemon of the selected hosts and display the" 
	"    system load in stripchart." 
	"" 
	"CPU Split:" 
	"    Query the rstat daemon of the selected hosts and display the" 
	"    the cpu split (user nice system idle) in a bar chart." 
	"" 
	"Disk Activity:" 
	"    Query the rstat daemon of the selected hosts and display the" 
        "    the disk activity in a bar chart. The first four bars show the" 
	"    activity of disk 0 to 3. The fifth and sixth bar display the " 
	"    paging activity and the last two bars show the swapping load." 
	"" 
	"Ethernet Load:" 
	"    Query the etherstat daemon of the selected hosts and display" 
	"    the ethernet load, the number of packets and the average packet" 
	"    size over the selected time interval." 
	"" 
	"Active Users:" 
	"    Display the number of active users as reported by the finger" 
	"    daemon in the 'active users' attribute. Note, there might be" 
	"    active users that are not displayed because finger output" 
	"    is quite untrustworthy." 
	"" 
	"Monitor Job Info:" 
	"    This command display information about all monitoring jobs" 
	"    started by this monitor script." 
	"" 
	"Modify Monitor Job:" 
	"    Select one of the monitoring jobs and modify it. You can change" 
	"    the sampling interval and switch the state from active (which" 
	"    means running) to suspended." 
	"" 
	"Set Parameter:" 
	"    This dialog allows you to set the sampling interval and " 
	"    some other parameters that control the monitoring commands. " 
	"" 
	"Show Defaults:" 
	"    Show the defaults that may be defined in the tkined.defaults" 
	"    files. This script makes use of definitions in the form:" 
	"" 
	"        monitor.interval:      <number>" 
	"" 
    }
}

##
## Delete the menus created by this interpreter.
##

proc "Delete IP-Monitor" {list} {

    global menus
    global reachability_ids

    if {[job find] != ""} {
	set res [ined confirm "Kill running monitoring jobs?" \
		 [list "kill & exit" cancel] ]
	if {$res == "cancel"} return
    }

    if {[info exists reachability_ids]} {
	foreach id $reachability_ids {
	    ined -noupdate attribute $id "round trip time" ""
	}
    }

    DeleteClones

    foreach id $menus { ined delete $id }    
    exit
}

set menus [ ined create MENU "IP-Monitor" \
	    "Check Reachability" "Round Trip Time" "" \
	    "NTP Offset" "" \
	    "System Load" "CPU Activity" \
	    "Disk Activity" "Interface Activity" "" \
	    "Ethernet Load" "" \
	    "Active Users" "" \
            "Monitor Job Info" "Modify Monitor Job" "" \
	    "Set Parameter" "Show Defaults" "" \
	    "Help IP-Monitor" "Delete IP-Monitor" ]

vwait forever
