#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# ip_world.tcl -
#
#	This file contains code to position nodes on image maps based 
#	on some guesses about the geographic position of the machines.
#
# Copyright (c) 1994-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: ip_world.tcl,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $

package require Tnm 3.0

namespace import Tnm::*

ined size
LoadDefaults layout
IpInit IP-Trouble

##
## Clear the current view if not empty. Makes sure that LOG, MENU
## and INTERPRETER objects will survive.
##

proc ClearView {} {
    foreach comp [ined -noupdate retrieve] {
	set type [ined type $comp]
	if {[lsearch [list LOG MENU INTERPRETER] $type] >= 0} continue
	lappend idlist [ined id $comp]
    }
    if {![info exists idlist]} return
    set result [ined confirm "The IP World script will clear this view." \
	             [list proceed cancel] ]
    if {$result == "cancel"} exit
    foreach id $idlist {
	catch {ined -noupdate delete $id}
    }
}

##
## Load the geographic locations stored in the locations file and store
## them in the global arrays loc_name, loc_domain, loc_ip.
##

proc LoadLocations {} {
    global loc_name loc_domain loc_ip
    global tnm auto_path

    foreach dir [concat $auto_path $tnm(library) ~/.tkined] {
	set fname [file join $dir locations]
	if {[file readable $fname]} break
    }
    if {$fname == ""} {
	ined acknowledge "Can not read geographic locations file, sorry."
	exit
    }

    set f [open $fname]
    while {![eof $f]} {
	gets $f line
	set line [string trim $line]
	if {($line == "") || ([string match "#" $line])} continue
	set key [string tolower [lindex $line 0]]
	set loc [lindex $line 1]
	set len [llength $loc]
	if {$len == 4} {
	    if {[scan $loc "%d %d%s %d %d%s" l1 l2 l b1 b2 b] != 6} {
		puts stderr "failed to convert $loc!"
		continue
	    }
	    set ll [expr $l1 + $l2/60.0]
	    set bb [expr $b1 + $b2/60.0]
	    if {$l == "n"} { set ll [expr -1 * $ll]}
	    if {$b == "w"} { set bb [expr -1 * $bb]}
	    set loc "$bb $ll"
	} elseif {$len == 6} {
	    if {[scan $loc "%d %d %d%s %d %d %d%s" l1 l2 l3 l b1 b2 b3 b] != 8} {
		puts stderr "failed to convert $loc!"
		continue
	    }
	    set ll [expr $l1 + $l2/60.0 + $l3/3600.0]
	    set bb [expr $b1 + $b2/60.0 + $b3/3600.0]
	    if {$l == "n"} { set ll [expr -1 * $ll]}
	    if {$b == "w"} { set bb [expr -1 * $bb]}
	    set loc "$bb $ll"
	}
	switch -glob $key {
	    .* {
		set loc_domain(*$key) $loc
	    }
	    [A-Z,a-z]* {
		set loc_name(*$key*) $loc
	    }
	    [0-9]* {
		set loc_ip($key) $loc
	    }
	}
    }
}

##
## Load the world background image into tkined and move it to a default
## position. Switch the page size to A4 landscape.
##

proc LoadImage { file {xoffset 0} {yoffset 0}} {
    global image
    if {[info exists image]} {
	catch {ined -noupdate delete $image}
	"Hide Cities"
	"Hide Domains"
    }
    set image [ined create IMAGE $file]
    if {$image == ""} {
	ined acknowledge "Could not read image $file!"
	unset image
	return
    }
    # move the image to a default position
    set size [ined -noupdate size $image]
    set width  [expr [lindex $size 2] - [lindex $size 0] ]
    set height [expr [lindex $size 3] - [lindex $size 1] ]
    set position [ined -noupdate move $image \
	    [expr $width/2 + $xoffset] [expr $height/2 + $yoffset] ]
    return $image
}

##
## Guess the location of a host.
##

proc GuessLocation {ip host} {
    global loc_name loc_domain loc_ip
    static cache

    if {[info exists cache($ip)]} {
	return $cache($ip)
    }
    
    if {[info exists loc_ip]} {
	foreach pattern [array names loc_ip] {
	    if {[string match $pattern $ip]} {
		set cache($ip) $loc_ip($pattern)
		return $cache($ip)
	    }
	}
    }

    if {[info exists loc_name]} {
	foreach pattern [array names loc_name] {
	    if {[string match $pattern [string tolower $host]]} {
		set cache($ip) $loc_name($pattern)
		return $cache($ip)
	    }
	}
    }

    if {[info exists loc_domain]} {
	foreach pattern [array names loc_domain] {
	    if {[string match $pattern [string tolower $host]]} {
		set cache($ip) $loc_domain($pattern)
		return $cache($ip)
	    }
	}
    }

    return ""
}

##
## Move an object given by id to the absolute position given in
## x and y.
##

proc MoveTo {id x y } {
    set xy [ined -noupdate move $id]
    ined -noupdate move $id \
	    [expr -1 * [lindex $xy 0] + $x] [expr -1 * [lindex $xy 1] + $y]
}

##
## Move NODE objects to a suitable location. This will be called whenever
## a traceroute finishes or if we change the background image.
##

proc PlaceNodes { args } {

    global image_x image_y image_mx image_my
    static x y

    if {$args == ""} {
	foreach comp [ined -noupdate retrieve] {
	    if {[ined type $comp] == "NODE"} {
		lappend args $comp
	    }
	}
    }

    ForeachIpNode id ip host $args {
	set loc [GuessLocation $ip $host]
	if {$loc != ""} {
	    set x [expr $image_mx * [lindex $loc 0] + $image_x]
	    set y [expr $image_my * [lindex $loc 1] + $image_y]
	} 
	if {[info exists x] && [info exists y]} {
	    MoveTo $id $x $y
	}
    }
}

##
## Trace a route to the given host. Returns the route as a tcl list.
##

proc TraceRoute { host {maxlen 32} } {
    set ip $host

    if {[regexp {^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$} $host] > 0} {
	set ip $host
    } else {
	if {[catch {nslook $host} ip]} {
            ined confirm "Unknown host $host!" "dismiss"
            return
        }
    }

    set ttl 1
    set trace ""
    set trace_ip ""
    set result ""
    set last ""
    set last_ip "nope"
    while {($trace_ip != $ip) && ($last_ip != $trace_ip)} {
	set last_ip $trace_ip
	for {set i 0} {$i < 3} {incr i} {
	    if {[catch {icmp trace $ttl $ip} trace]} {
		writeln "Could not send icmp packet: $trace"
		writeln
		return
	    }
	    set trace_ip [lindex $trace 0]
	    if {[catch {nslook $trace_ip} trace_name]} {
		set trace_name ""
	    }
	    set trace_time [lindex $trace 1]
	    if {$trace_time >= 0} {
		if {[catch {lindex [nslook $trace_ip] 0} host]} {
		    set host $trace_ip
		}
		set id [ined -noupdate create NODE]
		ined -noupdate name $id $host
		ined -noupdate address $id $trace_ip
		ined -noupdate icon $id Connector
		PlaceNodes [ined retrieve $id]
		if {$last != ""} {
		    ined -noupdate create LINK $last $id
		}
		set last $id
		break
	    }
	}

	if {[incr ttl] > $maxlen} break
    }
    return $result
}

##
## Create a routing trace and map it on the world map.
##

proc "Trace Route" { list } {

    global image image_x image_y image_mx image_my
    static host

    if {![info exists host]} { set host "" }

    # ask the user for some destination hosts

    set result [ined request "Destination hosts:" \
	             [list [list Host: $host ] ] [list trace cancel] ]
    if {[lindex $result 0] == "cancel"} return

    foreach h [set host [lindex $result 1]] {
	TraceRoute $h
    }
}

##
## Clear the map by removing all the nodes, stripcharts, ...
##

proc "Clear Map" {list} {
    foreach comp [ined retrieve] {
	set id [ined id $comp]
	switch [ined type $id] {
	    STRIPCHART -
	    BARCHART -
	    NODE {
		ined delete $id
	    }
	}
    }
}

##
## Show the city names on the map.
##

proc "Show Cities" { args } {

    global loc_name image image_x image_y image_mx image_my cities

    if {![info exists loc_name]} return

    "Hide Cities"

    foreach name [array names loc_name] {
	set x [expr $image_mx * [lindex $loc_name($name) 0] + $image_x]
	set y [expr $image_my * [lindex $loc_name($name) 1] + $image_y]
	if {($x < 0) || ($y < 0)} continue
	set size [ined size]
	if {($x > [lindex $size 2]) || ($y > [lindex $size 3])} continue
	set txt [string trim $name *]
	set id [ined -noupdate create TEXT [string trim $txt]]
	MoveTo $id $x $y
	lappend cities $id
    }
}

##
## Remove all city names from the map.
##

proc "Hide Cities" { args } {
    global cities
    if {[info exists cities]} {
	foreach id $cities {
	    catch {ined -noupdate delete $id}
	}
    }
}

##
## Show the domain names on the map.
##

proc "Show Domains" { args } {

    global loc_domain image image_x image_y image_mx image_my domains

    if {![info exists loc_domain]} return

    "Hide Domains"

    foreach name [array names loc_domain] {
	set x [expr $image_mx * [lindex $loc_domain($name) 0] + $image_x]
	set y [expr $image_my * [lindex $loc_domain($name) 1] + $image_y]
	if {($x < 0) || ($y < 0)} continue
        set size [ined size]
        if {($x > [lindex $size 2]) || ($y > [lindex $size 3])} continue
	set txt [string trim $name *]
	set id [ined -noupdate create TEXT $txt]
	MoveTo $id $x $y
	lappend domains $id
    }
}

##
## Remove all domain names from the map.
##

proc "Hide Domains" { args } {
    global domains
    if {[info exists domains]} {
	foreach id $domains {
	    catch {ined -noupdate delete $id}
	}
    }
}

##
## Since we have no zoom in/out support, we just take different bitmaps
## and adjust the point 0 0 and the magnification factor accordingly. 
## The offset is simply stored in the global variables image_x and image_y
## and the magnification is stored in image_mx and image_my. NOTE: This
## must be kept in sync with the bitmap file!
##

proc "World" { args } {
    global image_x image_y image_mx image_my
    LoadImage world-map.xbm 20 0
    set image_x 476
    set image_y 376
    set image_mx 2.53333
    set image_my 4.17778
    ined page A4 landscape
    PlaceNodes
}

proc "USA" { args } {
    global image_x image_y image_mx image_my
    LoadImage usa-map.xbm
    set image_x 2250
    set image_y 1434
    set image_mx 18
    set image_my 29
    ined page A4 landscape
    PlaceNodes
}

proc "Germany" { args } {
    global image_x image_y image_mx image_my
    LoadImage germany-map.xbm 100 80
    set image_x -300
    set image_y 5570
    set image_mx 65
    set image_my 100
    ined page A4 portrait
    PlaceNodes
}

proc "Netherlands" { args } {
    global image_x image_y image_mx image_my
    LoadImage netherlands-map.xbm 0 0
    set image_x -280
    set image_y 6700
    set image_mx 105
    set image_my 122
    ined page A4 portrait
    PlaceNodes
}

##
## Some help about this script.
##

proc "Help IP-World" { list } {
    ined browse "Help about IP-World" {
	"Trace Route:" 
	"    Trace the route to a number of hosts and display the" 
	"    result on a world map. This script was inspired by the" 
	"    geotraceman program." 
	"" 
	"World:" 
	"USA:" 
	"Netherlands:" 
	"Germany:" 
	"    Set the focus. More maps are possible by creating a bitmap" 
	"    file and setting the zoom and offset factors." 
	"" 
	"Show Cities:" 
	"    Show the cities defined in the locations file on the map." 
	"" 
	"Hide Cities:" 
	"    Remove all cities from the map." 
	"" 
	"Show Domains:" 
	"    Show the domain names defined in the locations file." 
	"" 
	"Hide Domains:" 
	"    Remove all domain names from the map." 
    }
}

##
## Delete the menus created by this interpreter.
##

proc "Delete IP-World" { list } {
    global menus
    foreach id $menus {	ined delete $id }
    exit
}

ClearView
LoadLocations
World
# USA
# "Show Cities"

set menus [ ined create MENU "IP-World" \
	    "Trace Route" \
	    "Clear Map" "" \
	    "World" "USA" "Netherlands" "Germany" "" \
	    "Show Cities" "Hide Cities" "" \
	    "Show Domains" "Hide Domains" "" \
	    "Help IP-World" "Delete IP-World" ]

vwait forever
