# TnmMonitor.tcl --
#
#	This file contains the implementation of some generic monitoring
#	procedures which usually run in the background and create events
#	on Tnm network maps. This is work in progress and the APIs might
#	change in future versions.
#
# Copyright (c) 1996	  Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1997      Gaertner Datensysteme.
# Copyright (c) 1997-1998 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: TnmMonitor.tcl,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $

package require Tnm 3.0
package require TnmMap 3.0
package provide TnmMonitor 3.0.2

#########################################################################

proc TnmGetStatusProc {job status vbl} {
    set event Tnm_MonitorStatus:Value
    set error Tnm_MonitorStatus:Error
    array set cx [$job attribute status]
    switch $status {
	noError {
	    set value [Tnm::snmp value $vbl 0]
	    if {$value != $cx(value)} {
		$cx(node) raise $event [list $cx(oid) $value]
	    }
	    set cx(value) $value
	    $job attribute status [array get cx]
	}
	noSuchName {
	    $cx(node) raise $error:NoSuchName $cx(session)
	}
	noResponse {
	    $cx(node) raise $error:NoResponse $cx(oid)
	}
    }
}

proc TnmGetStatusCmd {} {
    set job [Tnm::job current]
    array set cx [$job attribute status]
    if {[info commands $cx(node)] == ""} { $job destroy }
    catch {
	$cx(session) get $cx(oid) \
		[subst { TnmGetStatusProc "$job" "%E" "%V" } ]
    }
}

proc Tnm_MonitorStatus {node tree seconds} {
    set s [TnmMap::GetSnmpSession $node]
    set result ""
    set ms [expr $seconds * 1000]
    foreach var $tree {
	catch {
	    $s walk vbl $var {
		set cx(node)     $node
		set cx(session)  [eval Tnm::snmp generator [$s configure]]
		set cx(oid)      [Tnm::snmp oid $vbl 0]
		set cx(value)    [Tnm::snmp value $vbl 0]
		set j [Tnm::job create -command TnmGetStatusCmd -interval $ms]
		$j configure -exit "$cx(session) destroy"
		$j attribute status [array get cx]
		lappend result $j
	    }
	}
    }
    $s destroy
    return $result
}

#########################################################################

proc TnmGetValueProc {job status vbl} {
    set event Tnm_MonitorValue:Value
    set error Tnm_MonitorValue:Error
    array set cx [$job attribute status]
    switch $status {
	noError {
	    set value [Tnm::snmp value $vbl 0]
	    $cx(node) raise $event [list $cx(oid) $value]
	}
	noSuchName {
	    $cx(node) raise $error:NoSuchName $cx(oid)
	}
	noResponse {
	    $cx(node) raise $error:NoResponse $cx(oid)
	}
    }
}

proc TnmGetValueCmd {} {
    set job [Tnm::job current]
    array set cx [$job attribute status]
    if {[info commands $cx(node)] == ""} { $job destroy }
    catch {
	$cx(session) get $cx(oid) \
		[subst { TnmGetValueProc "$job" "%E" "%V" } ]
    }
}

proc Tnm_MonitorValue {node tree seconds} {
    set s [TnmMap::GetSnmpSession $node]
    set result ""
    set ms [expr $seconds * 1000]
    foreach var $tree {
	catch {
	    $s walk vbl $var {
		set cx(node)     $node
		set cx(session)  [eval Tnm::snmp generator [$s configure]]
		set cx(oid)	 [Tnm::snmp oid $vbl 0]
		set j [Tnm::job create -command TnmGetValueCmd -interval $ms]
		$j configure -exit "$cx(session) destroy"
		$j attribute status [array get cx]
		lappend result $j
	    }
	}
    }
    $s destroy
    return $result
}

#########################################################################

proc TnmGetUpTimeProc {job status vbl} {
    set event Tnm_MonitorSysUpTime:Value
    set error Tnm_MonitorSysUpTime:Error
    set oid [Tnm::mib oid SNMPv2-MIB!sysUpTime.0]
    switch $status {
	noError {
	    array set cx [$job attribute status]
	    set uptime [Tnm::snmp value $vbl 0]
	    $cx(node) raise $event [list $oid $uptime]
	    if {$uptime < $cx(sysUpTime)} {
		$cx(node) raise $error:Restart [list $oid $uptime]
	    }
	    set cx(sysUpTime) $uptime
	    $job attribute status [array get cx]
	}
	noResponse {
	    $cx(node) raise $error:NoResponse [list $cx(session) $oid]
	}
    }
}

proc TnmGetUpTimeCmd {} {
    set oid [Tnm::mib oid SNMPv2-MIB!sysUpTime.0]
    set job [Tnm::job current]
    array set cx [$job attribute status]
    if {[info commands $cx(node)] == ""} { $job destroy }
    catch {
	$cx(session) get $oid \
		[subst { TnmGetUpTimeProc "$job" "%E" "%V" } ]
    }
}

proc Tnm_MonitorSysUpTime {node seconds} {
    set s [TnmMap::GetSnmpSession $node]
    set ms [expr $seconds * 1000]
    set vbl [$s get [Tnm::mib oid SNMPv2-MIB!sysUpTime.0]]
    set cx(node)         $node
    set cx(session)      [eval Tnm::snmp generator [$s configure]]
    set cx(sysUpTime)    [Tnm::snmp value $vbl 0]
    set j [Tnm::job create -command TnmGetUpTimeCmd -interval $ms]
    $j configure -exit "$cx(session) destroy"
    $j attribute status [array get cx]
    $s destroy
    return $j
}

#########################################################################

proc TnmGetDot3StatsProc {job status vbl} {
    set event Tnm_MonitorDot3Stats:Value
    set error Tnm_MonitorDot3Stats:Error
    switch $status {
	noError {
	    array set cx [$job attribute status]
	    set uptime [Tnm::snmp value $vbl 0]

	    if [info exists cx(sysUpTime)] {
		if {$uptime < $cx(sysUpTime)} {
		    $cx(node) raise $error:Restart [list $oid.0 $uptime]
		}
		
		set msg ""
		foreach vb $vbl { 
		    lappend msg [Tnm::mib label [lindex $vb 0]]
		    lappend msg [lindex $vb 2]
		}
		$cx(node) raise $event $msg
	    }
	    
	    set cx(sysUpTime) $uptime
	    $job attribute status [array get cx]
	}
	noResponse {
	    $cx(node) raise $error:NoResponse
	}
    }
}

proc TnmGetDot3StatsCmd {} {
    set job [Tnm::job current]
    array set cx [$job attribute status]
    if {[info commands $cx(node)] == ""} { $job destroy }
    set vbl [list \
	sysUpTime.0 \
	ifOutUcastPkts.$cx(index) \
	ifOutNUcastPkts.$cx(index) \
	dot3StatsSingleCollisionFrames.$cx(index) \
	dot3StatsMultipleCollisionFrames.$cx(index) \
	dot3StatsExcessiveCollisions.$cx(index) \
	dot3StatsLateCollisions.$cx(index) \
	dot3StatsAlignmentErrors.$cx(index) \
	dot3StatsFCSErrors.$cx(index) \
	dot3StatsCarrierSenseErrors.$cx(index) \
	dot3StatsFrameTooLongs.$cx(index) \
    ]
    catch {
	$cx(session) get $vbl \
		[subst { TnmGetDot3StatsProc "$job" "%E" "%V" } ]
    }
}

proc Tnm_MonitorDot3Stats {node seconds} {
    set s [TnmMap::GetSnmpSession $node]
    set ms [expr $seconds * 1000]
    set cx(node) $node
    set jobs ""
    $s walk x dot3StatsIndex {
	set cx(index) [Tnm::snmp value $x 0]
	set cx(session) [eval Tnm::snmp generator [$s configure]]
	set j [Tnm::job create -command TnmGetDot3StatsCmd -interval $ms]
	$j configure -exit "$cx(session) destroy"
	$j attribute status [array get cx]
	lappend jobs $j
    }
    $s destroy
    return $jobs
}

#########################################################################

#
# Calculate the interface utilisation. This is done using the formula
#
# util = ( 8 * ( delta (ifInOctets, t1, t0) 
#              + delta (ifOutOctets, t1, t0) ) / (t1 - t0) ) / ifSpeed
#
# This formula returns incorrect results for full-duplex point to point
# links. In this case, the following formula should be used:
#
# util = ( 8 * max ( delta (ifInOctets, t1, t0) ,
#                    delta (ifOutOctets, t1, t0) ) / (t1 - t0) ) / ifSpeed
#
# See Simple Times, 1(5), November/December, 1992 for more details.
#

proc TnmGetIfLoadProc {job status vbl} {

    set event Tnm_MonitorIfLoad

    if {$status != "noError"} return

    array set cx [$job attribute status]

    set ifIndex	     $cx(ifIndex)
    set sysUpTime    [Tnm::snmp value $vbl 0]
    set ifOperStatus [Tnm::snmp value $vbl 1]
    set ifInOctets   [Tnm::snmp value $vbl 2]
    set ifOutOctets  [Tnm::snmp value $vbl 3]
    
    # be careful with Tcl's broken arithmetic
    
    if {[catch {expr $ifInOctets - $cx(ifInOctets)} deltaIn]} {
	set deltaIn  [expr double($ifInOctets) - $cx(ifInOctets)]
    }
    if {[catch {expr $ifOutOctets - $cx(ifOutOctets)} deltaOut]} {
	set deltaOut [expr double($ifOutOctets) - $cx(ifOutOctets)]
    }
    
    if {$cx(fullduplex)} {
	set delta [expr $deltaIn > $deltaOut ? $deltaIn : $deltaOut]
    } else {
	set delta [expr $deltaIn + $deltaOut]
    }
    
    if {$sysUpTime > $cx(sysUpTime) && $cx(ifSpeed) > 0} {
	set secs [expr ($sysUpTime - $cx(sysUpTime)) / 100.0]
	set val  [expr (8.0 * $delta / $secs) / $cx(ifSpeed) * 100]
    } else {
	set val 0
    }

    if {$ifOperStatus == "up"} {
	$cx(node) raise $event:Value [list \
		ifIndex $ifIndex ifDescr $cx(ifDescr) \
		ifOperStatus $ifOperStatus ifLoad $val ]
    }
    if {$ifOperStatus != $cx(ifOperStatus)} {
	$cx(node) raise $event:StatusChange [list \
		ifIndex $ifIndex ifDescr $cx(ifDescr) \
		ifOperStatus $ifOperStatus]
    }

    set cx(sysUpTime)    $sysUpTime
    set cx(ifInOctets)   $ifInOctets
    set cx(ifOutOctets)  $ifOutOctets
    set cx(ifOperStatus) $ifOperStatus
    $job attribute status [array get cx]
}

proc TnmGetIfLoad {} {
    set job [Tnm::job current]
    array set cx [$job attribute status]
    if {[info commands $cx(node)] == ""} { $job destroy }
    set i $cx(ifIndex)
    set vbl "sysUpTime.0 ifOperStatus.$i ifInOctets.$i ifOutOctets.$i"
    $cx(session) get $vbl [subst { TnmGetIfLoadProc "$job" "%E" "%V" } ]
}

#
# The following procedure walks the ifTable and starts an interface 
# load monitoring procedure for every interface. We retrieve some 
# initial status information from the agent to initialize the monitor
# jobs.
#

proc Tnm_MonitorIfLoad {node seconds {iterations {}}} {

    set s [TnmMap::GetSnmpSession $node]
    set ms [expr $seconds * 1000]
    set result ""

    # The list of full duplex interface types. Note, IANAifType 
    # (RFC 1573) uses slightly different encodings than RFC 1213. 
    # We use RFC 1213 style here.
    
    set fullDuplex {
	regular1822 hdh1822 ddn-x25 rfc877-x25 lapb sdlc ds1 e1 
	basicISDN primaryISDN propPointToPointSerial ppp slip ds3 sip 
	frame-relay
    }

    $s walk vbl ifIndex {
	set ifIndex [Tnm::snmp value $vbl 0]

	set vbl [$s get [list sysUpTime.0 \
                          ifInOctets.$ifIndex ifOutOctets.$ifIndex \
                          ifSpeed.$ifIndex ifDescr.$ifIndex \
			  ifType.$ifIndex ifOperStatus.$ifIndex]]

	set cx(node)         $node
	set cx(session)      [eval Tnm::snmp generator [$s configure]]
	set cx(ifIndex)      $ifIndex
	set cx(sysUpTime)    [Tnm::snmp value $vbl 0]
	set cx(ifInOctets)   [Tnm::snmp value $vbl 1]
	set cx(ifOutOctets)  [Tnm::snmp value $vbl 2]
	set cx(ifSpeed)      [Tnm::snmp value $vbl 3]
	set cx(ifDescr)      [Tnm::snmp value $vbl 4]
	set cx(ifType)       [Tnm::snmp value $vbl 5]
	set cx(ifOperStatus) [Tnm::snmp value $vbl 6]
	set cx(fullduplex)   [expr [lsearch $fullDuplex $cx(ifType)] >= 0]

	set j [Tnm::job create -command TnmGetIfLoad -interval $ms]
	if {$iterations != ""} {
	    $j configure -iterations $iterations
	}
	$j configure -exit "$cx(session) destroy"

	$j attribute status [array get cx]
	lappend result $j
    }

    return $result
}

#########################################################################

proc TnmPingCmd {} {
    set event Tnm_MonitorPing:Value
    set error Tnm_MonitorPing:Error
    set job [Tnm::job current]
    array set cx [$job attribute status]
    if {[info commands $cx(node)] == ""} { $job destroy }
    if [catch {Tnm::icmp echo $cx(ip)} msg] {
	$cx(node) raise $error:NoResponse $msg
    }
    set rtt [lindex $msg 1]
    if {$rtt >= 0} {
	$cx(node) raise $event $rtt
    } else {
	$cx(node) raise $error:noResponse
    }
}

proc Tnm_MonitorPing {node seconds} {
    set ms [expr $seconds * 1000]
    set cx(node) $node
    set cx(ip) [TnmMap::GetIpAddress $node]
    set j [Tnm::job create -command TnmPingCmd -interval $ms]
    $j attribute status [array get cx]
    return $j
}

#########################################################################

proc TnmRstatCmd {} {
    set event Tnm_MonitorRstat:Value
    set error Tnm_MonitorRstat:Error
    set job [Tnm::job current]
    array set cx [$job attribute status]
    if {[info commands $cx(node)] == ""} { $job destroy }
    if [catch {Tnm::sunrpc stat $cx(ip)} new] {
	$cx(node) raise $error $new
	return
    }
    set new [join $new]
    if [info exists cx(last)] {
	foreach {name type value} $cx(last) { set s1($name) $value }
	foreach {name type value} $new { set s2($name) $value }

	# On some system, the boottime changes about 1 or 2 seconds
	# occasionally. So we are a bit fuzzy here. Note, this means
	# that we can't detect counter discontinuities if machines
	# boot very fast, which however is unlikely.

	if {abs($s1(boottime) - $s2(boottime)) > 2} {
	    $cx(node) raise $error:Restart "$s1(boottime) $s2(boottime)"
	} else {
	    set r(time) [expr $s2(curtime) - $s1(curtime)]
	    foreach {n1 t1 v1} $cx(last) {n2 t2 v2} $new {
		switch $t1 {
		    Counter {
			set r($n1) [expr $r(time) > 0 ? \
				double($v2-$v1)/$r(time) : 0]
		    }
		    Gauge {
			set r($n1) [expr $v2/256.0]
		    }
		    default {
			set r($n1) $v2
		    }
		}
	    }
	    $cx(node) raise $event [list \
		interval $r(time) \
		load1 $r(avenrun_0) load5 $r(avenrun_1) load15 $r(avenrun_2) \
		user [expr $r(cp_user) + $r(cp_nice)] system $r(cp_system) \
		idle $r(cp_idle) disk0 $r(dk_xfer_0) \
		disk1 $r(dk_xfer_1) disk2  $r(dk_xfer_2) disk3 $r(dk_xfer_3) \
		pgpgin $r(v_pgpgin) pgpgout $r(v_pgpgout) \
		pswpin $r(v_pswpin) pswpout $r(v_pswpout) \
		intr $r(v_intr) swtch $r(v_swtch) \
		ipackets $r(if_ipackets) ierrors $r(if_ierrors) \
		opackets $r(if_opackets) oerrors $r(if_oerrors) \
	    ]
	}
    }
    set cx(last) $new
    $job attribute status [array get cx]
}

proc Tnm_MonitorRstat { node seconds } {
    set ms [expr $seconds * 1000]
    set cx(node) $node
    set cx(ip) [TnmMap::GetIpAddress $node]
    set j [Tnm::job create -command TnmRstatCmd -interval $ms]
    $j attribute status [array get cx]
    return $j
}

#########################################################################

proc TnmRecvTrap {map ip vbl} {
    set event Tnm_MonitorTraps
    set sysUpTime [Tnm::snmp value $vbl 0]
    set trapOid [Tnm::snmp value $vbl 1]
    set nodes [$map find -address $ip -type node]
    if [llength $nodes] {
	foreach node $nodes {
	    $node raise $event:$trapOid $vbl
	}
    } else {
	$map raise $event:$trapOid:$ip $vbl
    }
}

proc Tnm_MonitorTraps {map {community public}} {
    set s [Tnm::snmp listener -notify $community]
    $s bind trap [subst { TnmRecvTrap "$map" "%A" "%V" } ]
}

#########################################################################

proc TnmMapCheckCmd {} {
    set job [Tnm::job current]
    array set cx [$job attribute status]
    if {[info commands $cx(map)] == ""} { $job destroy }
    foreach node [$cx(map) find -type node] {
	set a [$node cget -address]
	set n [$node cget -name]
	if {! [string length $a] && [string length $n]} {
	    if [catch {netdb hosts address $n} a] {
		set a ""
	    } else {
		$node configure -address $a
	    }
	}
	if {[string length $a] && ! [string length $n]} {
            if [catch {netdb hosts name $a} n] {
                set n ""
            } else {
                $node configure -name $n
            }
        }
    }
}

proc Tnm_MonitorCheck {map seconds} {
    set ms [expr $seconds * 1000]
    set cx(map) $map
    set j [Tnm::job create -command TnmMapCheckCmd -interval $ms]
    $j attribute status [array get cx]
    return $j
}

#########################################################################

proc TnmCheckHealthCmd {} {
    set event Tnm_CheckHealth
    set job [Tnm::job current]
    array set cx [$job attribute status]
    if {[info commands $cx(map)] == ""} { $job destroy }
    foreach item [$cx(map) find] {
	set h [lindex [$item health] 0]
	if {$h == 0} {
	    $item raise $event:Critical $h
	} elseif {$h < 30} {
            $item raise $event:Alert $h
        } elseif {$h < 50} {
	    $item raise $event:Warning $h
	}
    }
}

proc Tnm_CheckHealth { map seconds } {
    set ms [expr $seconds * 1000]
    set cx(map) $map
    set j [Tnm::job create -command TnmCheckHealthCmd -interval $ms]
    $j attribute status [array get cx]
    return $j
}

#########################################################################

proc TnmTestTcpConnect {channel job} {
    array set cx [$job attribute status]
    puts stderr "** [clock seconds] connect $job: [$job attribute status]"
    puts stderr "** eof -> [eof $channel]"
    puts stderr "** flush -> [flush $channel]"
    puts stderr "** puts -> [puts -nonewline $channel {nase}]"
    catch {after cancel $cs(timer)}
    catch {close $channel}
}

proc TnmTestTcpTimeout {channel job} {
    puts stderr "** [clock seconds] timeout $job: [$job attribute status]"
    catch {close $channel}
}

proc TnmTestTcpCmd {} {
    set event Tnm_TestTcp:Value
    set error Tnm_TestTcp:Error
    set job [Tnm::job current]
    array set cx [$job attribute status]
    if {[info commands $cx(node)] == ""} { $job destroy }
    puts stderr "** [clock seconds] trying $job: [$job attribute status]"
    set channel [socket -async $cx(ip) $cx(port)]
    set cx(timer) [after 60000 TnmTestTcpTimeout $channel $job]
    fileevent $channel writable "TnmTestTcpConnect $channel $job"
}

proc Tnm_TestTcp {node port seconds} {
    set ms [expr $seconds * 1000]
    set cx(node) $node
    set cx(port) $port
    set cx(ip) [TnmMap::GetIpAddress $node]
    set j [Tnm::job create -command TnmTestTcpCmd -interval $ms]
    $j attribute status [array get cx]
    return $j
}


proc Tnm_MonitorHtml {map path seconds} {
    set ms [expr $seconds * 1000]
    set j [Tnm::job create -command [list TnmMap::Html $map $path] -interval $ms]
    
}

