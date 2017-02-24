#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# ip_discover.tcl -
#
#	A script to discover the structure of IP networks. This script 
#	implements an algorithm that tries to figure out the topology 
#	without using SNMP. A paper which is descibes this script has
#	been presented at the USENIX LISA '93 conference.
#
# Copyright (c) 1993-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1997-1998 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: ip_discover.tcl,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $

package require Tnm 3.0

namespace import Tnm::*

ined size
LoadDefaults icmp
IpInit IP-Discover

##
## These are the global parameters that control the discovering
## process. They are manipulated by the "Discover Parameter" proc.
##

set gridx 65
set gridy 50
set columns 16
set report true
set debug false


##
## During our icmp fire, we build up the following tables.
##
##    ids[address]     <the id of a given ip address>
##    name[id]         <the name of the object given by id>
##    address[id]      <the ip address of a given id>
##    trace[id]        <the trace to a node>
##    mask[id]         <the network mask>
##    fip[id]          <the ip address as returned from the foreign node>
##    networks[id]     <exists if id belongs to a network, stores the mask>
##    nodes[id]        <exists if id belongs to a node>
##    links[id]        <the two ids of objects connected by this link>
##    gateways[id]     <exists if id is (part of) a gateway>
##

##
## Reset the global tables. This is now called before we start anything
## else to recover from abnormal terminated runs. Unset the variables
## in a loop just in case some do not yet exist.
##

proc reset {} {
    set globalvars {
	nodes networks links ids trace mask fip name address gateways
    }

    foreach g $globalvars {
	global $g
	catch {unset $g}
    }
}

##
## The following procedures are called whenever a new object has
## been discovered. They are here to maintain consistency in our
## global tables.
##

proc create_node {ip} {
    global ids nodes address name
    set id [newid]
    if {[catch {dns name $ip} ns_name]} {
	if {[catch {nslook $ip} ns_name]} {
	    set ns_name $ip
	}
    }
    set ns_name [lindex $ns_name 0]
    set ids($ip) $id
    set nodes($id) ""
    set address($id) $ip
    set name($id) $ns_name
}

proc create_network {ip mask } {
    global ids networks address name
    set id [newid]
    set ids($ip) $id
    set networks($id) $mask
    set address($id) $ip
    set name($id) $ip
}

proc create_link {id_a id_b} {
    global links
    if {[info exists links]} {
	foreach name [array names links] {
	    if {$links($name)=="$id_a $id_b"} return
	}
    }
    set id [newid]
    set links($id) "$id_a $id_b"
}

##
## This two procedures are used to get the next free x and y coordinates
## on the grid. This results in a very poor layout mechanism ...
##

proc nextx {} {
    global x y gridx gridy columns
    if ![info exists x] { set x 0 }
    if ![info exists y] { set y 0 }
    incr x $gridx
    if {$x>($gridx*$columns)} { set x $gridx }
    return $x
}

proc nexty {} {
    global x y gridx gridy columns
    if ![info exists x] { set x 0 }
    if ![info exists y] { set y 0 }
    if {$x==$gridx} { incr y $gridy }
    return [expr {$x%(2*$gridx) > 0 ? $y-12 : $y+12}]
}

##
## This procedure generates unused id's.
##

proc newid {} {
    static lastid
    if ![info exists lastid] { set lastid 0 }
    incr lastid
    return $lastid
}



##
## =========================================================================
## ========== P H A S E   1   (Discovering nodes on the networks) ==========
## =========================================================================
##

##
## Send an icmp request to all hosts on a class C like network. Return
## a list of all ip addresses that have responded to our request.
##

proc netping { network } {
    set result ""
    if {[regexp {^[0-9]+\.[0-9]+\.[0-9]+$} $network] > 0} {
	set hosts ""
	for {set a4 1} {$a4<255} {incr a4} {
	    append hosts " $network.$a4"
	}
	set result [icmp echo $hosts]
    }
    set res ""
    foreach {pr_ip pr_time} $result {
	if {$pr_time>=0} {
	    lappend res $pr_ip
	}
    }
    return $res
}

##
## Try to get all ip nodes of a given network. We currently handle
## 255 nodes (all addresses of a class C like network) in parallel.
## Bigger networks (class B or even class A) are split in class C
## like networks.
##

proc discover_nodes {network} {
    if {[regexp {^[0-9]+\.[0-9]+\.[0-9]+$} $network] > 0} {
	set count 0
	set start [clock seconds]
	foreach ip [netping $network] {
	    create_node $ip
	    incr count
	}
	writeln "$count nodes found on network $network in [expr {[clock seconds]-$start}] seconds."
	flush stdout
    }
    if {[regexp {^[0-9]+\.[0-9]+$} $network] > 0} {
	for {set a3 0} {$a3<256} {incr a3} {
	    discover_nodes $network.$a3
	}
    }
    if {[regexp {^[0-9]+$} $network] > 0} {
	for {set a2 0} {$a2<256} {incr a2} {
	    discover_nodes $network.$a2
	}
    }
}



##
## ===========================================================================
## ========== P H A S E   2   (Trace the routes to all known nodes) ==========
## ===========================================================================
##

##
## Trace the route to ip addresses using the van Jacobsen method.
## We get a list of ip addresses and try to process them in parallel.
## Therefore we set up an array which contains the routing information
## indexed by ip addresses. The todo list keeps track of all not
## complete routes.
##

proc trace_route {iplist} {
    global icmp_routelength

    foreach ip $iplist { set routetable($ip) "" }
    set todo $iplist
    set ttl 1

    while {$todo != ""} {
	if {$ttl > $icmp_routelength} break
	set i 0
	set new_todo ""
	foreach {trace_ip trace_time} [icmp trace $ttl $todo] {
	    set ip [lindex $todo $i]
	    lappend routetable($ip) $trace_ip
	    if {$trace_ip != $ip} {
		lappend new_todo $ip
	    }
	    incr i
	}
	set todo $new_todo
	incr ttl
    }

    set result ""
    foreach ip $iplist { lappend result "$ip {$routetable($ip)}" }
    return $result
}

##
## Get a trace of the routes to very node. Store them in the global
## array trace. If we find nodes that were not discovered using the
## icmp fire, put them in the global table.
##

proc discover_traces {} {
    global ids nodes trace address
    set count 0
    set start [clock seconds]

    set addrs ""
    foreach id [array names nodes] {
	set ip $address($id)
	lappend addrs $ip
	if {[llength $addrs] > 255} {
	    foreach route [trace_route $addrs] {
		set id $ids([lindex $route 0])
		set trace($id) [lindex $route 1]
		incr count
	    }
	    set addrs ""
	}
    }
    foreach route [trace_route $addrs] {
	set id $ids([lindex $route 0])
	set trace($id) [lindex $route 1]
	incr count
    }

    # add new discovered nodes to the global tables

    foreach id [array names trace] {
	foreach hop $trace($id) {
	    if {$hop == ""} continue
	    if {![info exists ids($hop)]} {create_node $hop }
	}
    }
    writeln "$count routes traced in [expr {[clock seconds]-$start}] seconds."
    flush stdout
}



##
## ===========================================================================
## ========== P H A S E   3   (Get the netmasks of all known nodes) ==========
## ===========================================================================
##

##
## Get the ip network masks of all nodes and put them in the
## global mask table. We try to handle 255 nodes in parallel.
##

proc discover_masks {} {
    global nodes ids mask address
    set count 0
    set start [clock seconds]

    set addrs ""
    foreach id [array names nodes] {
	lappend addrs $address($id)
	if {[llength $addrs] > 255} {
	    foreach {ip ipmask} [icmp mask $addrs] {
		set id $ids($ip)
		set mask($id) $ipmask
		incr count
	    }
	    set addrs ""
	}
    }

    foreach {ip ipmask} [icmp mask $addrs] {
	set id $ids($ip)
	set mask($id) $ipmask
	incr count
    }
    writeln "$count netmasks queried in [expr {[clock seconds]-$start}] seconds."
    flush stdout
}



##
## ===========================================================================
## ========== P H A S E   3.5 (Guess snmp parameters) ========================
## ===========================================================================
##

proc discover_snmp_callback {id s e vbl} {
    global snmp sysObjectID sysDescr
    if {$e == "noError"} {
	set snmp($id) [$s configure]
	set sysObjectID($id) [snmp value $vbl 0]
	set sysDescr($id) [snmp value $vbl 1]
    }
    $s destroy
}

proc discover_snmp {} {
    global nodes address snmp
    global icmp_retries icmp_timeout
    set start [clock seconds]
    mib load RFC1213-MIB
    foreach id [array names nodes] {
        set ip $address($id)
	if {[catch {snmp generator -address $ip \
		-retries $icmp_retries -timeout $icmp_timeout} s]} continue
	if {[catch {
	    $s get {sysObjectID.0 sysDescr.0} \
		    [subst {discover_snmp_callback $id "%S" "%E" "%V"}]
	} msg]} {
	    writeln "Oops: SNMP get request on $ip failed: $msg"
	}
        update
    }
    snmp wait
    set count [llength [array names snmp]]
    writeln "$count snmp agents queried in [expr {[clock seconds]-$start}] seconds."
    flush stdout
}



##
## ===========================================================================
## ========== P H A S E   4   (Get the ip address as returned from the nodes =
## ===========================================================================
##

##
## Get the ip address as returned from all nodes and put them in 
## the global fip table. This information is used to discover
## multihomed machines that return ip packets with a different
## ip address.
##

proc discover_fips {} {
    global nodes ids fip address
    set count 0
    set start [clock seconds]

    set addrs ""
    set idlist ""
    foreach id [array names nodes] {
	lappend addrs $address($id)
	lappend idlist $id
	if {[llength $addrs] > 255} {
	    foreach id $idlist {ip ipfip} [icmp ttl 32 $addrs] {
		set fip($id) $ip
		incr count
	    }
	    set addrs ""
	    set idlist ""
	}
    }
    foreach id $idlist {ip ipfip} [icmp ttl 32 $addrs] {
	set fip($id) $ip
	incr count
    }
    writeln "$count ip addresses queried in [expr {[clock seconds]-$start}] seconds."
    flush stdout
}



##
## ===========================================================================
## ========== P H A S E   5   (Identify networks and subnetworks) ============
## ===========================================================================
##

##
## Return the letter for the network class of a given IP address.
##

proc ip_class {network} {
    set byte_one [lindex [split $network .] 0]
    if {$byte_one <  127} { return A }
    if {$byte_one == 127} { return loopback }
    if {$byte_one <  192} { return B }
    if {$byte_one <  224} { return C }
    return D
}

##
## Compute the network address based on an ip address
## and a netmask.
##

proc ip_network {ip mask} {
    set bytes [split $ip .]
    set i 0
    set net ""
    foreach m [split $mask .] {
	if {[catch {expr {[lindex $bytes $i] & $m}} b]} {
	    set b 0
	}
	if {$b} { append net " $b"}
	incr i
    }
    return [join $net .]
}

##
## Test if an ip address belongs to a given network address space.
## We compute the unsigned long int that represente the host address.
## After applying the netmask, we test this against the unsigned long
## containing the network address.
##

proc ip_ismember {ip net mask} {
    set ip   [split $ip   .]
    set net  [split $net  .]
    set mask [split $mask .]

    append ip   " 0 0 0 0"
    append net  " 0 0 0 0"
    append mask " 0 0 0 0"

    set ip_bin 0
    set net_bin 0
    set mask_bin 0
    foreach i "0 1 2 3" {
	set ip_bin   [expr {$ip_bin   << 8 | [lindex $ip   $i]}]
	set net_bin  [expr {$net_bin  << 8 | [lindex $net  $i]}]
	set mask_bin [expr {$mask_bin << 8 | [lindex $mask $i]}]
    }

    return [expr {($ip_bin & $mask_bin) == ($net_bin & $mask_bin)}]
}

##
## Compare two netmasks. Return -1 if mask1 is less than mask2,
## 0 if both masks are equal and 1 if mask2 is greater than mask1.
##

proc netmask_compare {maska maskb} {
    if {"$maska"=="$maskb"} { return 0 }
    set bytesa [split $maska .]
    set bytesb [split $maskb .]
    if {[lindex $bytesa 0] > [lindex $bytesb 0]} { return 1 }
    if {[lindex $bytesa 1] > [lindex $bytesb 1]} { return 1 }
    if {[lindex $bytesa 2] > [lindex $bytesb 2]} { return 1 }
    if {[lindex $bytesa 3] > [lindex $bytesb 3]} { 
	return 1 
    } else {
	return -1
    }
}

##
## Discover networks. For all nodes get their official network
## (class A,B,C or D). If the netmask does not correspond to the
## official network address, try to guess the subnet.
##

proc discover_networks {} {
    global nodes networks ids trace mask address report
    global tnm
    set count 0
    set start [clock seconds]

    foreach id [array names nodes] {
	set ip $address($id)
	set netmask $mask($id)

	# Get the official network for this node.

	set class [ip_class $ip]
	if {$class == "loopback"} continue;
	set bytes [split $ip .]
	switch $class {
	    A { set bytes [lrange [split $ip .] 0 0]; set mmm "255.0.0.0" }
	    B { set bytes [lrange [split $ip .] 0 1]; set mmm "255.255.0.0" }
	    C { set bytes [lrange [split $ip .] 0 2]; set mmm "255.255.255.0" }
	    D { set bytes "" }
	}
	set net [join $bytes .]
	if {![info exists table($net)]} { set table($net) $mmm }

	# Sanity check for incorrect netmasks. Netmasks wider than
	# the official network type don't make any sense. Ignore
	# problems that are due to the loopback mask of the localhost.
	
	if {$netmask == ""} continue;
	if { [ip_network $net $netmask] != $net } {
	    if {![catch {nslook $tnm(host)} localhost]} {
		if {[string trim $localhost] == $ip} continue
	    }
	    set txt "Please check the netmask $netmask for $ip on class $class network $net."
	    ined acknowledge $txt
	    if {$report == "true"} {
		writeln $txt
	    }
	    continue
	}

	# Try to get the subnet of the node.

	set subnet [ip_network $ip $netmask]
	if {![info exists table($subnet)]} { set table($subnet) $netmask }
    }

    if {[info exist table]} {

	validate_networks table

        foreach net [array names table] {
	    create_network $net [lindex $table($net) 0]
	    incr count
        }
    }

    writeln "$count networks discovered in [expr {[clock seconds]-$start}] seconds."
    flush stdout
}

##
## Check if the discovered (sub) networks are valid. It may be possible
## that misconfigured hosts confused us to see subnetworks that do not
## exist. If the majority of the hosts belonging to a potential subnet
## have a wider netmask, we kick the subnet away.
##

proc validate_networks { net_table } {
    global nodes address mask ids report
    upvar $net_table table

    # Make a majority decision if the subnet is a vaild one or
    # if we have found misconfigured nodes.
    
    # Sort the network addresses into ascending order.
    
    set networks_sorted ""
    foreach net [array names table] {
	set i 0
	foreach n $networks_sorted {
	    if {[netmask_compare $net $n] > 0} break
	    incr i
	}
	set networks_sorted [linsert $networks_sorted $i $net]
    }
    
    foreach id [array names nodes] {
	set ip $address($id)
	foreach net $networks_sorted {
	    if {[ip_ismember $ip $net [lindex $table($net) 0]]} {
		lappend table($net) $id
		break
	    }
	}
    }

    foreach net [array names table] {
	set count [llength $table($net)]
	set pros 0
	set cons 0
	set unk 0
	foreach id [lrange $table($net) 1 end] {
	    set netmask $mask($id)
	    if {$netmask == ""} {
		incr unk
		continue
	    }
	    if {[ip_network $net $netmask] == $net} {
		incr pros
	    } else {
		incr cons
	    }
	}
	if {$cons > $pros} {
	    set res [ined confirm \
  "The majority ($cons : $pros) of nodes on network $net have wide netmasks." \
  "$unk nodes did not vote. Discard this network?" [list no yes]]
	    if {$res == "yes"} { 
		unset table($net) 
		if {$report == "true"} {
		    writeln "Network $net discarded ($pros, $cons, $unk)." 
		}
	    } else {
		if {$report == "true"} {
		    writeln "Network $net not discarded ($pros, $cons, $unk)."
		}
	    }
	}
    }

}



##
## ===========================================================================
## ========== P H A S E   6   (Identify nodes with more than one interface) ==
## ===========================================================================
##

##
## Try to identify the gateways. The first thing is to interpret
## the routing traces. We set up a table indexed by the gateway
## address. The second step is to examine the foreign ip addresses.
## Some gateways return udp requests to their remote ip address with
## a packet containing their another ip address. The third step is to
## compare the ptr records as returned by the domain name service.
## Gateways often have the same name on all its interfaces.
##
## We store the ids of gateways in the global gateway table.
##

proc discover_gateways {} {
    global ids trace mask nodes address name gateways fip
    global icmp_routelength
    set count 0
    set start [clock seconds]

    # Examine the traces and set up the gateway table.

    foreach id [array names trace] {
	set ip $address($id)
	set len [llength $trace($id)]
	if {$len > 1} {
	    foreach hop [lrange $trace($id) 0 [incr len -2]] {
		if {[info exists table($hop)]} continue
		if {[regexp {^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$} $hop]} {
		    set gate_id $ids($hop)
		    set gateways($gate_id) $hop
		}
	    }
	}
    }

    # Check if the foreign addresses give us any hints.

    foreach id [array names fip] {
	set ip $address($id)
	if {($ip == "0.0.0.0") || ($fip($id) == "0.0.0.0")} continue
	if {$ip != $fip($id)} {
	    if {[info exists gateways($id)]} {
		if {[lsearch $gateways($id) $ip] < 0} {
		    lappend gateways($id) $ip
		}
		if {[lsearch $gateways($id) $fip($id)] < 0} {
                    lappend gateways($id) $fip($id)
                }
	    } else {
		set gateways($id) "$ip $fip($id)"
	    }
	    set aa $fip($id)
	    if {[info exists ids($aa)]} {
		set id $ids($aa)
		if {[info exists gateways($id)]} {
		    if {[lsearch $gateways($id) $ip] < 0} {
			lappend gateways($id) $ip
		    }
		    if {[lsearch $gateways($id) $fip($id)] < 0} {
			lappend gateways($id) $fip($id)
		    }
		} else {
		    set gateways($id) "$fip($id) $ip"
		}
	    }
	}
    }

    # Search for nodes with the same name. Set up a table indexed
    # by name that contains the list of node ids sharing the name

    foreach id [array names nodes] {
	set n $name($id)
	if {[info exists used_names($n)]} {
	    lappend used_names($n) $id
	} else {
	    set used_names($n) $id
	}
    }
    foreach n [array names used_names] {
	if {[llength $used_names($n)] > 1} {
	    foreach id $used_names($n) {
		set ip $address($id)
		foreach aa $used_names($n) {
		    if {$aa == $id} continue
		    if {[info exists gateways($aa)]} {
			lappend gateways($aa) $ip
		    } else {
			set gateways($aa) "$address($aa) $ip"
		    }
		}
	    }
	}
    }

    if {[info exists gateways]} {
	set count [llength [array names gateways]]
    }

    writeln "$count gateways discovered in [expr {[clock seconds]-$start}] seconds."
    flush stdout
}



##
## ===========================================================================
## ========== P H A S E   7   (Decide which node is linked to which network) =
## ===========================================================================
##

##
## Sort the addresses of the networks into a list and create link
## objects based on the ip address of a node and the known subnets.
## In the second step, we use the trace information to connect
## our gateways to both networks.
##

proc discover_links {} {
    global ids trace nodes networks links gateways address
    set count 0
    set start [clock seconds]

    # Sort the network addresses into ascending order.

    set netids_sorted ""
    foreach id [array names networks] {
	set m $address($id)
	set i 0
	foreach netid $netids_sorted {
	    if {([netmask_compare $m $address($netid)]>0)} break
	    incr i
	}
	set netids_sorted [linsert $netids_sorted $i $id]
    }

    # Link every node to its network based on the network address.

    foreach id [array names nodes] {
	set ip_node $address($id)
	set id_net ""
	foreach netid $netids_sorted {
	    if {[ip_ismember $ip_node $address($netid) $networks($netid)]} {
		set id_net $netid
		break
	    }
	}
	if {$id_net != ""} {
	    create_link $id_net $id
	    incr count
	}
    }

    # Create links based on the routes traces. Skip over traces
    # that end on a gateway since gateways often return another
    # ip address when being traced. Put all connections into
    # a table indexed by `ip:next_ip' pairs.

    foreach id [array names trace] {

	set idx [expr {[llength $trace($id)] - 1}]
	set aa [lindex $trace($id) $idx]
	while {($aa == "") && ($idx > 1)} { 
	    incr idx -1 
	    set aa [lindex $trace($id) $idx]
	}
	if {[lindex $trace($id) $idx] == ""} continue
	set last_id $ids([lindex $trace($id) $idx])
	if {[info exist gateways($last_id)]} { incr idx -1 }

	set lasthop ""
	foreach hop [lrange $trace($id) 0 $idx] {
	    if {($lasthop != "") && ($hop != "")} {
		set table($hop:$lasthop) $trace($id)
	    }
	    set lasthop $hop
	}
    }

    
    # Now scan through the table and connect the ip addresses
    # to the right discovered network.

    if {[info exists table]} {
	foreach name [array names table] {
	    set hop [lindex [split $name :] 0]
	    set lasthop [lindex [split $name :] 1]
	    set id_net ""
	    foreach netid $netids_sorted {
		if {[ip_ismember $hop $address($netid) $networks($netid)]} {
		    set id_net $netid
		    break
		}
	    }
	    if {$id_net != ""} {
		create_link $id_net $ids($lasthop)
		incr count
	    }
	}
    }
    
    writeln "$count links discovered in [expr {[clock seconds]-$start}] seconds."
    flush stdout
}



##
## ===========================================================================
## ========== P H A S E   8   (Merge gateways to single nodes) ===============
## ===========================================================================
##

##
## For each discovered gateway with more than one known interface,
## merge the nodes for the interfaces to a single one. 
##

proc merge_gateways {} {
    global ids nodes networks links gateways address name
    set count 0
    set start [clock seconds]

    if {![info exists gateways]} return

    debug "** entering merge_gateways"
    foreach id [array names gateways] {
	if {[llength $gateways($id)] < 2} continue
	debug "** gateways ($id) :\t $gateways($id)"
    }

    # We set up a table indexed by the connected ids.
    # This is used to avoid duplicates.

    foreach lid [array names links] {
	set aa $links($lid)
	set already($aa) ""
    }

    # Loop through the gateways and decide which one is selected
    # to survive. Nodes with a valid domain name are prefered.

    foreach id [array names gateways] {

	if {[llength $gateways($id)] < 2} continue

	set myip [lindex $gateways($id) 0]
	if {[info exists done($myip)]} {
	    debug "** $myip skipped - $myip is already done"
	    continue
	}

	# Check if one of the foreign addresses of this gateway
	# contains more ip address. Skip this entry if we find
	# such an entry that also contains myip.

	set skip 0
	foreach ip [lrange $gateways($id) 1 end] {
debug "--   checking $ip for length"
	    if {($ip == "0.0.0.0") || (![info exists ids($ip)])} continue
	    set aid $ids($ip)
	    if {![info exists gateways($aid)]} continue
	    if {[llength $gateways($aid)] > [llength $gateways($id)]} {
		set skip 1
		# mark this ip number so that it will not skipped
		# because of any other reasons.
		set force($ip) $ip  
		break
	    }
	}

	if {$skip} {
	    debug "** $myip skipped - using $ip instead (more interfaces)"
	    continue
	}

	# Search throught the ip addresses of the gateway to find an
	# address with a real name if the name of the current gateway 
	# is just the ip address.

	if {(![info exists force($myip)]) && ($name($id) == $myip)} {
	    set skip 0
	    foreach ip [lrange $gateways($id) 1 end] {
debug "--   checking $ip for name"
		if {($ip == "0.0.0.0") || (![info exists ids($ip)])} continue
		set aid $ids($ip)
		if {$ip != $name($aid)} {
		    set skip 1
		    break
		}
	    }
	    if {$skip} {
		debug "** $myip skipped - using $ip instead (better name)"
		continue
	    }
	}
	
	set done($myip) 1
	incr count
	debug "** $myip merged with [lrange $gateways($id) 1 end]"
	foreach ip [lrange $gateways($id) 1 end] {
	    
	    if {$ip == "0.0.0.0"} continue
	    
	    # Merge node given by ip with the given by id. Ignore IP 
	    # addresses for which there is no id. We get such beasts
	    # when checking the returned udp address.
	    
	    if {![info exists ids($ip)]} continue
	    
	    set rid $ids($ip)
	    catch {unset nodes($rid)}
	    catch {unset address($rid)}
	    catch {unset name($rid)}
	    
	    # Adjust the links pointing to rid.
	    
	    foreach lid [array names links] {
		if {[lsearch $links($lid) $rid] < 0} continue
		set ida [lindex $links($lid) 0]
		set idb [lindex $links($lid) 1]
		if {$ida == $rid} { set ida $id }
		if {$idb == $rid} { set idb $id }
		
		# Remove the old link and create a new one
		# if it does not already exist.
		
		unset links($lid)
		set aa "$ida $idb"
		if {![info exists already($aa)]} {
		    set links($lid) $aa
		}
	    }
	    set done($ip) 1
	}
    }

    writeln "$count nodes merged in [expr {[clock seconds]-$start}] seconds."
    flush stdout
}


##
## ===========================================================================
## ========== P H A S E   9   (Tell tkined what we have found so far) ========
## ===========================================================================
##

##
## Talk with tkined to let him draw the network map.
##

proc talk_to_ined {} {
    global nodes networks links address snmp sysDescr sysObjectID name
    set count 0
    set start [clock seconds]

    # query ined for all known objects so that we can reuse
    # existing nodes (incremental discovering)

    set grouplist ""
    foreach comp [ined retrieve] {
	lappend grouplist [ined id $comp]
    }
    while {[llength $grouplist] > 0} {
	set newlist ""
	foreach id $grouplist {
	    set comp [ined retrieve $id]
	    set type [ined type $comp]
	    set id [ined id $comp]
	    switch $type {
		NODE {
		    set addr [GetIpAddress $comp]
		    set id_by_addr($addr) $id
		}
		NETWORK {
		    set id_by_addr([ined address $comp]) $id
		}
		LINK {
		    set ida [ined src $comp]
		    set idb [ined dst $comp]
		    set aa "$ida $idb"
		    set id_by_link($aa) $id
		}
		GROUP {
		    lappend newlist [ined member $comp]
		}
	    }
	}
	set grouplist [join $newlist]
    }

    # tell tkined about the node objects

    foreach id [array names nodes] {
	set ip $address($id)
	if {[catch {set route $trace($id)}]} { set route "" }

	if {[info exists id_by_addr($ip)]} {
	    set ined_id($id) $id_by_addr($ip)
	} else {
	    set ined_id($id) [ined -noupdate create NODE]
	    ined -noupdate address $ined_id($id) $ip
	    ined -noupdate name $ined_id($id) "[lindex $name($id) 0]"
	    ined -noupdate label $ined_id($id) name
	    ined -noupdate move $ined_id($id) [nextx] [nexty]
	    if {[info exists snmp($id)]} {
		ined -noupdate attribute $ined_id($id) \
			"SNMP:Config" $snmp($id)
	    }
	    if {[info exists sysObjectID($id)]} {
		ined -noupdate attribute $ined_id($id) \
			"sysObjectID.0" $sysObjectID($id)
	    }
	    if {[info exists sysDescr($id)]} {
		ined -noupdate attribute $ined_id($id) \
			"sysDescr.0" $sysDescr($id)
	    }
	    set id_by_addr($ip) $ined_id($id)
	    incr count
	}
    }

    # tell tkined about the network objects

    foreach id [array names networks] {
	set ip $address($id)
	set bytes [split $ip .]
	set class [ip_class $ip]
	set type "(class $class)"
	switch $class {
	    A { if {[llength $bytes] != 1} {set type "(subnet)"} }
	    B { if {[llength $bytes] != 2} {set type "(subnet)"} }
	    C { if {[llength $bytes] != 3} {set type "(subnet)"}} 
	}

	if {[info exists id_by_addr($ip)]} {
	    set ined_id($id) $id_by_addr($ip) 
	} else {
	    set ined_id($id) [ined -noupdate create NETWORK 0 0 300 0]
	    ined -noupdate name $ined_id($id) "$ip $type"
	    ined -noupdate address $ined_id($id) $ip
	    ined -noupdate label $ined_id($id) name
	    ined -noupdate move $ined_id($id) [nextx] [nexty]
	    set id_by_addr($ip) $ined_id($id)
	    incr count
	}
    }

    # tell tkined about link objects

    foreach id [array names links] {
	set link $links($id)
	set src [lindex $link 0]
	set dst [lindex $link 1]

	set aa "$ined_id($dst) $ined_id($src)"
	set bb "$ined_id($src) $ined_id($dst)"
	if {[info exists id_by_link($aa)]} {
	    set ined_id($id) $id_by_link($aa)
	} elseif {[info exists id_by_link($bb)]} {
	    set ined_id($id) $id_by_link($bb)
	} else {
	    set ined_id($id) [ined -noupdate create LINK \
			      $ined_id($src) $ined_id($dst)]
	    set id_by_link($aa) $ined_id($id)
	    incr count
	}
    }

    writeln "$count tkined objects created in [expr {[clock seconds]-$start}] seconds."
    flush stdout
}

##
## ===========================================================================
## ========== E N D   O F   D I S C O V E R   S U B R O U T I N E S ==========
## ===========================================================================
##

##
## This is the main procedure. We first try to figure out how
## many hosts are on our network. The next step is to identify
## networks (including subnetworks) and the last step is to
## send all information for the ined editor. Clear all informations
## to be prepared for the next run.
##

proc "Discover IP Network" {list} {

    global nodes networks links ids trace mask fip name address gateways
    global tnm
    static nets

    reset

    if {![info exists nets]} { set nets "" }

    set given_nets ""
    foreach comp $list {
	if {[ined type $comp] == "NETWORK"} {
	    set ip [lindex [ined address $comp] 0]
	    lappend given_nets $ip
	}
    }
    if {$given_nets != ""} {
	set nets [list $given_nets]
    }

    set result [ined request "Welcome to the Network Discovering Tool." "" \
		  "Enter the IP Numbers of the networks of interest:" \
		  [list [list "" $nets]] [list discover cancel] ]

    if {[lindex $result 0] == "cancel"} return

    # remove trailing .0's

    set tmp ""
    foreach net [lindex $result 1] {
	if {[ip_class $net] == "loopback"} continue
	set dots [llength [split $net .]]
	if {$dots == 4} {
	    regsub {(\.0+)+$} $net "" net
	}
	lappend tmp $net
    }
    set nets $tmp

    set hostname $tnm(host)
    set ip [nslook $hostname]
    writeln [clock format [clock seconds]]
    writeln "Discover $nets from $hostname \[$ip\]"
    set start [clock seconds]

    foreach network $nets { discover_nodes $network }
    if {[info exists nodes]} {
	discover_traces
	discover_masks
	discover_snmp
	discover_fips
	discover_networks
	discover_gateways
	discover_links
	merge_gateways
	talk_to_ined
	reset
    }

    writeln "Discover finished in [expr {[clock seconds]-$start}] seconds"
    writeln
}

##
## Discover a route to a host. This is a special case of the general
## discovering mechanism where we do not try to figure out which destination
## IP addresses are in use but get the interested one from the user.
##

proc "Discover Route" {list} {

    global nodes networks links ids trace mask fip name address gateways
    global tnm
    static ips

    reset

    if {![info exists ips]} { set ips "" }

    set given_ips ""
    foreach comp $list {
	if {[ined type $comp] == "NODE"} {
            lappend given_ips [GetIpAddress $comp]
	}
    }
    if {$given_ips != ""} {
	set ips [list $given_ips]
    }

    set ips [ined request "Welcome to the Route Discovering Tool." "" \
	       "Enter the IP Numbers of the destination hosts:" \
               [list "{} $ips"] [list discover cancel] ]

    if {[lindex $ips 0] == "cancel"} return
    
    # remove duplicate ip addresses, expand domain names and 
    # create target nodes

    set new_ips ""
    foreach ip [lindex $ips 1] {

	if {[lsearch $new_ips $ip] >= 0} continue
	lappend new_ips $ip

	if {![regexp {^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$} $ip]} {
	    set host $ip
	    if {[catch {nslook $host} ip]} {
		ined acknowledge "Can not lookup IP address for $host."
		continue
	    }
	    set ip [lindex $ip 0]
	}

	create_node $ip
    }
    set ips $new_ips
    if {$ips == ""} return

    set hostname $tnm(host)
    set ip [nslook $hostname]
    writeln [clock format [clock seconds]]
    writeln "discover route to $ips from $hostname \[$ip\]"
    set start [clock seconds]

    if {[info exists nodes]} {
	discover_traces
	discover_masks
	discover_snmp
	discover_fips
	discover_networks
	discover_gateways
	discover_links
	merge_gateways
	talk_to_ined
	reset
    }

    writeln "discover finished in [expr {[clock seconds]-$start}] seconds"
    writeln
}

##
## Set some constants that will control the discovery process.
##

proc "Set Parameter" {list} {
    global icmp_retries icmp_timeout icmp_delay gridx gridy columns
    global icmp_routelength
    global email_trace
    global report
    global debug

    set result [ined request "IP-Discover Parameter" \
	[list [list "# of ICMP retries:" $icmp_retries scale 1 10] \
          [list "ICMP timeout \[s\]:" $icmp_timeout scale 1 42] \
          [list "Delay between ICMP packets \[ms\]:" $icmp_delay scale 1 100] \
          [list "Max. length of a route:" $icmp_routelength scale 1 42] \
          [list "Horizontal grid space:" $gridx scale 20 160] \
          [list "Vertical grid space:" $gridy scale 20 160] \
          [list "Nodes per row:" $columns scale 10 40] \
          [list "Email Discover Routes:" $email_trace radio true false] \
          [list "Write Report:" $report radio true false] \
          [list "Debug Mode:" $debug radio true false] ] \
        [list "set values" cancel] ]

    if {[lindex $result 0] == "cancel"} return

    set icmp_retries     [lindex $result  1]
    set icmp_timeout     [lindex $result  2]
    set icmp_delay       [lindex $result  3]
    set icmp_routelength [lindex $result  4]
    set gridx            [lindex $result  5]
    set gridy            [lindex $result  6]
    set columns          [lindex $result  7]
    set email_trace      [lindex $result  8]
    set report           [lindex $result  9]
    set debug            [lindex $result 10]

    icmp -retries $icmp_retries
    icmp -timeout $icmp_timeout
    icmp -delay $icmp_delay
}

##
## Find RPC Server by querying the portmapper.
##

proc "Show RPC Server" {list} {

    global tnm

    set service_list ""
    foreach rpc [netdb sunrpcs] {
	lappend service_list [lindex $rpc 0]
    }

    set service [ined list "Select the RPC service of interest:" \
		 [lsort $service_list] [list select cancel]]
    if {[lindex $service 0] == "cancel"} return

    set service [lindex [lindex $service 1] 0]
    set hostname $tnm(host)
    set ip [nslook $hostname]
    writeln "Show RPC server $service from $hostname \[$ip\]:"
    set start [clock seconds]

    foreach comp $list {
        if {[ined type $comp]=="NODE"} {
            set id [ined id $comp]
	    set host [ined name $comp]
	    set ip [GetIpAddress $comp]
	    set color($id) [ined color $id]
	    set label($id) [ined label $id]
	    set rtt [lindex [icmp echo $ip] 0]
	    if {[lindex $rtt 1]<=0} continue
	    if {![catch {eval sunrpc info $ip} portmapinfo]} {
		foreach serv $portmapinfo {
		    if {[lsearch $serv $service]>=0} {
			write   [format "%-32s \[" $host ]
			write   $ip "IpFlash $ip"
			writeln "\]"
			break
		    }
		}
	    }
	}
    }
    writeln
}

##
## Find TCP Server by connecting to a well known TCP port.
##

proc "Show TCP Server" {list} {

    global tnm

    set service [IpService tcp]

    if {$service == ""} return

    set hostname $tnm(host)
    set ip [nslook $hostname]
    set name [lindex $service 0]
    set port [lindex $service 1]
    writeln "Show TCP server $name ($port) from $hostname \[$ip\]:"
    set start [clock seconds]

    foreach comp $list {
        if {[ined type $comp] == "NODE"} {
            set id [ined id $comp]
	    set host [ined name $comp]
	    set ip [GetIpAddress $comp]
	    set color($id) [ined color $id]
	    set label($id) [ined label $id]

	    if {[lindex [lindex [icmp echo $ip] 0] 1] <= 0} {
		ined acknowledge "Skipping $host (currently unreachable)"
		continue
	    }

	    if {![catch {socket $ip $port} f]} {
		close $f
		write   [format "%-32s \[" $host ]
		write   $ip "IpFlash $ip"
		writeln "\]"
	    }
	}
    }
    writeln
}

##
## Here are the beginnings of the email tools.
##

set last_email ""
set max_level 12
set email_trace false

##
## Create a new icon for this host (if we did not have it yet)
##

proc ined_create {host user} {
    global nodes networks links ids trace mask fip name address gateways
    global hosts email_trace
    global tnm

    if {![info exists hosts($host)]} {
	set hosts($host) $host
    }

    if {$email_trace == "true"} {
	set hostname $tnm(host)
	set ip [nslook $hostname]
	set ips [lindex [nslook $host] 0]
	create_node $ips
	writeln [clock format [clock seconds]]
	writeln "discover route to $ips from $hostname \[$ip\]"
	set start [clock seconds]
	
	if {[info exists nodes]} {
	    discover_traces
	    discover_masks
	    discover_snmp
	    discover_fips
	    discover_networks
	    discover_gateways
	    discover_links
	    merge_gateways
	    talk_to_ined
	    reset
	}
	
	writeln "discover finished in [expr {[clock seconds]-$start}] seconds"
	writeln
    } else {
	set id [ined -noupdate create NODE]
	ined -noupdate name $id $host
	ined -noupdate attribute $id email "$user@$host"
	ined -noupdate label $id email
	ined -noupdate move $id [nextx] [nexty]
    }

}

##
## Print n spaces to stdout.
##

proc space {n} {
    for {set i 0} {$i < $n} {incr i} { puts -nonewline "  " }
}

##
## Read an answer from an SMTP server and return the whole stuff
## in from of a tcl list (with all newline characters removed).
##

proc read_answer {f} {
    lappend answer [gets $f]
    while {[string index $line 3] == "-"} {
	lappend answer [gets $f]
	flush stdout
    }
    return $answer
}

##
## Contact the smtp daemon at a given host and check if it
## can verify the given user.
##

proc test_user_host {user host level} {

    global tnm

    if {$level > 20} {
        space $level; writeln "-> a loop? aborting after 20 hops"
	return 0
    }

    ined_create $host $user

    if {[catch {socket $host smtp} f]} {
	space $level; writeln "-> unable to connect to $host"
	return 0
    }
    if {[catch {read_answer $f}]} {
	return 0
    }

    if {![catch {dns name [nslook $tnm(host)]} hostname]} {
	puts $f "helo $hostname"
	if {[catch {read_answer $f}]} {
	    return 0
	}
    }

    if {[catch {puts $f "vrfy $user"}]} {
	space $level; writeln "-> unable to write to $host"
	return 0
    }

    if {[catch {read_answer $f} answer]} {
	return 0
    }
    foreach addr $answer {

	set code [string range $addr 0 2]
	if {$code != 250} {
	    space $level; writeln "$user@$host"
	    space $level; writeln "-> $addr"
	    set ok 0
	    continue
	}

	# split addr into an email address and an optional name

	regexp -nocase {<.*>} $addr email
	if {![info exist email]} { set email [string range $addr 4 end] }
	set email [string trim $email "<>"]
	regexp -nocase {\(.*\)} $email name
	regsub -nocase {\(.*\)} $email "" email
	set email [string trim $email]

	debug "** got email address $email"

	if {![info exists name]} {
	    set name [string range $addr 4 end]
	    regsub -nocase {<.*>} $name "" name
	}
	set name [string trimleft  $name " ("]
	set name [string trimright $name " )"]

	# Check for host names that contain dots.
	if {[string match {*@*.*} $email]} {
	    set ok [test_email $email [expr {$level+1}] $name]

	# Hostnames without dots get expanded magically.
	} elseif {[string match {*@*} $email]} {
	    set us [lindex [split $email @] 0]
            set nl [split [lindex [split $email @] 1] .]
            set ol [split $host .]

	    set len [llength $ol]
	    set ok 0
	    for {set i 0}  {($i < $len) && ($ok == 0)} { incr i} {
		set hn "$nl [lrange $ol $i end]"
		set hn [join $hn "."]

		# Do we have a A or a MX record for this?

		if {[catch {dns address $hn} res]} {
		    if {[catch {dns mx $hn} res]} continue
		}
		if {$res == ""} continue

		set email "$us@$hn"
		debug "** expanding with domain name extensions to $email"
		set ok [test_email $email [expr {$level+1}] $name]
	    }

	# OK, no @. We believe that we have found it.
	} else {
	    space [expr {$level+1}]
	    if {$name != ""} {
		writeln "$email ($name)"
	    } else {
		writeln "$email"
	    }
	    space [expr {$level+1}]; writeln "-> ok"
	    set ok 1
	}

	unset name
    }

    close $f

    return $ok
}

##
## Test a given email. First, split it in the user and host part.
## See if we have a local address or if we can resolve the hostname
## to an IP address. If this fails, check if we can get an MX record
## for the address.
##

proc test_email {email {level 0} {name ""}} {

    global last_email max_level

    # Loop detection.
    if {($last_email == $email) || ($level > $max_level)} {
	space $level
	if {$name != ""} {
	    writeln "$email ($name)"
	} else {
	    writeln "$email"
	}
	space $level; writeln "-> ok (possible loop detected)"
	return 0
    }
    set last_email $email

    # Say where we are.
    space $level
    if {$name != ""} {
	writeln "$email ($name)"
    } else {
	writeln "$email"
    }

    set email [split  $email @]
    set user  [lindex $email 0]
    set host  [lindex $email 1]

    ## Use localhost if we operate local.
    if {$host == ""} { 
	debug "** testing localhost"
	return [test_user_host $user localhost $level]
    }

    ## expand the host if we get an MX record for it
    if {![catch {dns mx $host} res]} {
	set ok 0
	foreach mx $res {
	    if {[lindex $mx 0] == $host} break
	    debug "** expanding based on MX record to [lindex $mx 0]"
	    set ok [test_email "$user@[lindex $mx 0]" [expr {$level+1}]]
	    if {$ok} break
	}
	return $ok
    }

    ## check if we have a real host name. convert it back into
    ## a fully qualified hostname if possible.
    if {![catch {dns address $host} res]} {
	if {$res != ""} {
	    set ok 0
	    foreach aa $res {
		if {![catch {dns name $aa} aa]} { set host $aa }
		debug "** testing fully qualified host address $aa"
		set ok [test_user_host $user $host $level]
		if {$ok} break
	    }
	    return $ok
        }
    }

    space $level; writeln "-> probably a bad email address"
    return 0
}

##
## Here is the main routine.
##

proc "Resolve Email" { list } {

    static email

    reset

    if {![info exists email]} { set email "" }

    set result [ined request "Please enter an email address to discover:" \
		  [list [list Email: $email]] [list discover cancel] ]

    if {[lindex $result 0] == "cancel"} return

    set email [lindex $result 1]

    foreach addr $email {
	test_email $addr
    }
}


##
## Display some help about this tool.
##

proc "Help IP-Discover" {list} {
    ined browse "Help about IP-Discover" {
	"Discover IP Network:"  
	"    Try to discover the structure of IP networks. You should"  
	"    use 3 Byte addresses like a.b.c or perhaps two byte addresses" 
	"    like a.b. Class A like addresses may also work but take a very" 
	"    long time to complete..."  
	""  
	"Discover Route:"  
	"    Trace the route to the selected hosts and add all new "  
	"    discovered gateways and their links to the current map."  
	""  
	"Discover Parameter:"  
	"    Set some parameters that control the discovering process."  
	""  
	"Show RPC Server:"  
	"    Ask all selected hosts if a given RPC service is registered by"  
	"    the portmapper. This can be used to locate NFS server etc."  
	""  
	"Show TCP Server:"  
	"    Test all selected hosts if a given TCP service is available by"  
	"    connecting to the service port. This can be used to locate"  
	"    X11 server etc."  
	""  
	"Resolve Email:" 
	"    Take an email address and resolve it to a set of hosts that" 
	"    host a recipient. Make a traceroute to every discovered node" 
	"    if Email Discover Routes is set to true." 
	"" 
    }
}

##
## Delete the discovering tool.
##

proc "Delete IP-Discover" {list} {
    global menus
    foreach id $menus { ined delete $id }
    exit
}

##
## Register the tool and fall into the main loop.
##

set menus [ ined create MENU "IP-Discover" \
	    "Discover IP Network" "Discover Route" "" \
	    "Show RPC Server" "Show TCP Server" "" \
	    "Resolve Email" "" \
	    "Set Parameter" "" \
	    "Help IP-Discover" "Delete IP-Discover" ]

vwait forever
