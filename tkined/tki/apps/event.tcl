#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# event.tcl -
#
#	Interface to read and process events.
#
# Copyright (c) 1994-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: event.tcl,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $

package require Tnm 3.0

namespace import Tnm::*

ined size
LoadDefaults event
IpInit Event

# set debug true

##
## These are the global parameters. The port number is used to
## connect to our syslogd with tcp forwarding support.
##

if {[info exists default(port)]} {
    set port $default(port)
} else {
    set port 8567
}
if {[info exists default(server)]} {
    set server $default(server)
} else {
    set server bayes.ibr.cs.tu-bs.de
}
if {[info exists default(file)]} {
    set file $default(file)
} else {
    set file /var/adm/messages
}
if {[info exists default(use)]} {
    set use $default(use)
} else {
    set use server
}
if {$use != "file" && $use != "server"} {
    set use server
}
if {[info exists default(start)]} {
    set start $default(start)
} else {
    set start user
}
if {$start != "auto" && $use != "user"} {
    set start user
}


##
## The sl_level table is used to map priorities to readable strings.
##

set sl_level(0) "emergency"
set sl_level(1) "alert"
set sl_level(2) "critical"
set sl_level(3) "error"
set sl_level(4) "warning"
set sl_level(5) "notice"
set sl_level(6) "info"
set sl_level(7) "debug"

##
## The sl_facility table maps facility ids to readable names.
## Codes from 9 through 15 are reserved for system use.
##

set sl_facility(0)  "kernel"
set sl_facility(1)  "user"
set sl_facility(2)  "mail"
set sl_facility(3)  "daemon"
set sl_facility(4)  "auth"
set sl_facility(5)  "syslog"
set sl_facility(6)  "lpr"
set sl_facility(7)  "news"
set sl_facility(8)  "uucp"
set sl_facility(15) "cron"
set sl_facility(16) "local0"
set sl_facility(17) "local1"
set sl_facility(18) "local2"
set sl_facility(19) "local3"
set sl_facility(20) "local4"
set sl_facility(21) "local5"
set sl_facility(22) "local6"
set sl_facility(23) "local7"

##
## Convert the default definitions into event filters.
##

proc setup_default_filter {} {

    global filter default

    if {[info exists default]} {
	set names [array names default]
	for {set idx 1} {1} {incr idx} {
	    set found 0
	    foreach name $names {
		if {[string match filter$idx* $name]} {
		    set found 1
		    break
		}
	    }
	    if {! $found} break
	    set attid [lindex [split $name .] 0]
	    set id [create_filter $attid]
	    foreach name $names {
		if {[string match filter$idx* $name]} {
		    set attname [lindex [split $name .] 1]
		set attval $default($name)
		set filter($id,$attname) $attval
		}
	    }
	}
    }
}

##
## Flash the icon of the object given by the ip address.
## Keep track of all ip addresses to flash in an array.
##

proc flash {op {ip ""}} {

    static jobid
    static ipaddrs

    switch $op {

	list {
	    if {[info exists ipaddrs]} {
		return [lsort [array names ipaddrs]]
	    } else {
		return {}
	    }
	}

	add {
	    if {$ip == ""} return
	    if {![info exists ipaddrs($ip)]} {
		set ipaddrs($ip) 1
	    } else {
		incr ipaddrs($ip) 1
	    }
	    if {![info exists jobid]} {
		set jobid [job create -command "flash run" -interval 10000]
	    }
	    return
	}

	remove {
	    if {![info exists ipaddrs]} return
	    if {![info exists ipaddrs($ip)]} return
	    incr ipaddrs($ip) -1
	    if {$ipaddrs($ip) <= 0} {
		unset ipaddrs($ip)
	    }
	    if {[llength [array names ipaddrs]] == 0} {
		$jobid destroy
		unset jobid
		unset ipaddrs
		writeln "All highlights deleted."
		writeln
	    }
	    return
	}

	reset {
	    if {![info exists ipaddrs]} return
	    if {[info exists ipaddrs($ip)]} {
		unset ipaddrs($ip)
	    }
	    if {[llength [array names ipaddrs]] == 0} {
		$jobid destroy
		unset jobid
		unset ipaddrs
		writeln "All highlights deleted."
		writeln
	    }
	    return
	}

	run {
	    foreach ip [array names ipaddrs] {
		IpFlash $ip [expr {$ipaddrs($ip) < 10 ? $ipaddrs($ip) : 10}]
	    }
	}
    }
}

##
## Create a new filter and return a handle for it.
##

proc create_filter { name } {

    global filter
    static lastid

    if {![info exists lastid]} {
	set lastid 0
    } else {
	incr lastid
    }

    set id "filter$lastid"

    set filter($id,name) $name
    set filter($id,host) ""
    set filter($id,level) ""
    set filter($id,facility) ""
    set filter($id,process) ""
    set filter($id,message) ""
    set filter($id,action) ""
    set filter($id,status) active
    set filter($id,highlight) false
    set filter($id,report) local
    set filter($id,window) ""
    set filter($id,match) "includes"

    lappend filter(ids) $id

    return $id
}

##
## Modify an existing filter.
##

proc edit_filter { id } {

    global filter

    set result [ined request "Event Filter Settings:" \
	[list [list "Filter Name:" $filter($id,name)] \
	      [list "Host Filter:" $filter($id,host)] \
              [list "Level Filter:" $filter($id,level)] \
              [list "Facility Filter:" $filter($id,facility)] \
              [list "Process Filter:" $filter($id,process)] \
              [list "Message Filter:" $filter($id,message)] \
              [list "Action:" $filter($id,action)] \
              [list "Status:" $filter($id,status) radio active suspend] \
              [list "Flash Nodes:" $filter($id,highlight) radio true false] \
              [list "Report Window:" $filter($id,report) radio local global] \
	      [list "Match:" $filter($id,match) radio includes excludes] ] \
	[list "set filter" cancel] ]

    if {[lindex $result 0] == "cancel"} {
	return ""
    }

    set filter($id,name)      [lindex $result  1]
    set filter($id,host)      [lindex $result  2]
    set filter($id,level)     [lindex $result  3]
    set filter($id,facility)  [lindex $result  4]
    set filter($id,process)   [lindex $result  5]
    set filter($id,message)   [lindex $result  6]
    set filter($id,action)    [lindex $result  7]
    set filter($id,status)    [lindex $result  8]
    set filter($id,highlight) [lindex $result  9]
    set filter($id,report)    [lindex $result 10]
    set filter($id,match)     [lindex $result 11]

    return $id
}

##
## Select a filter from all existing filters. Returns the selected
## id or an empty string.
##

proc select_filter {} {

    global filter

    if {![info exists filter(ids)]} {
	ined acknowledge "There is no filter defined yet."
	return
    }

    foreach id $filter(ids) {
	lappend filternames "$id $filter($id,name)"
    }
    set result [ined list "Select a filter:" $filternames \
		[list select cancel]]
    if {[lindex $result 0] == "cancel"} return

    return [lindex [lindex $result 1] 0]
}

##
## Delete an existing filter.
##

proc delete_filter { id } {

    global filter

    if {![info exists filter(ids)]} return

    while {[set i [lsearch $filter(ids) $id]] >= 0} {
	set filter(ids) [lreplace $filter(ids) $i $i]
    }
    if {[llength $filter(ids)] == 0} {
	unset filter(ids)
    }

    catch {
	unset filter($id,name)
	unset filter($id,host)
	unset filter($id,level)
	unset filter($id,facility)
	unset filter($id,process)
	unset filter($id,message)
	unset filter($id,action)
	unset filter($id,status)
	unset filter($id,highlight)
	unset filter($id,report)
	unset filter($id,match)
    }
}

##
## List all defined filters.
##

proc list_filter { id } {

    global filter

    if {![info exists filter($id,name)]} return

    lappend txt [format "%-16s%s" Filter: $id]
    if {$filter($id,name) != ""} {
	lappend txt [format "%-16s%s" Name: $filter($id,name)]
    }
    if {$filter($id,host) != ""} {
	lappend txt [format "%-16s%s" Host: $filter($id,host)]
    }
    if {$filter($id,level) != ""} {
	lappend txt [format "%-16s%s" Level: $filter($id,level)]
    }
    if {$filter($id,facility) != ""} {
	lappend txt [format "%-16s%s" Facility: $filter($id,facility)]
    }
    if {$filter($id,process) != ""} {
	lappend txt [format "%-16s%s" Process: $filter($id,process)]
    }
    if {$filter($id,message) != ""} {
	lappend txt [format "%-16s%s" Message: $filter($id,message)]
    }
    if {$filter($id,action) != ""} {
	lappend txt [format "%-16s%s" Action: $filter($id,action)]
    }
    if {$filter($id,status) != ""} {
	lappend txt [format "%-16s%s" Status: $filter($id,status)]
    }
    if {$filter($id,highlight) != ""} {
	lappend txt [format "%-16s%s" "Flash Nodes:" $filter($id,highlight)]
    }
    if {$filter($id,report) != ""} {
	lappend txt [format "%-16s%s" "Report Window:" $filter($id,report)]
    }
    if {$filter($id,match) != ""} {
	lappend txt [format "%-16s%s" "Match:" $filter($id,match)]
    }

    eval ined acknowledge $txt
}

##
## Setup the connect menu once we are connected to a server or a file.
##

proc ev_connected {} {

    global menus

    if {[info exists menus]} {
        foreach id $menus { ined delete $id }
    }

    set menus [ ined create MENU "Event" \
               "Disconnect" "" \
	       "Create Filter" "List Filter" "Edit Filter" "Delete Filter" "" \
	       "Reset" "Reset All" "" "Show Defaults" "" \
               "Help Event" "Delete Event" ]
}

##
## Connect to a syslog daemon that supports event forwarding.
##

proc ev_server { server port } {

    global syslog menus

    if {[catch {socket $server $port} syslog]} {
	ined acknowledge \
	    "Can not connect to $server using port $port" "" $syslog
	return
    }
    fileevent $syslog readable ev_receive
    ined restart [list "ev_server $server $port"]

    ev_connected
}

##
## Connect to a dump syslog server by reading its output file.
##

proc ev_file { file } {

    global syslog

    if {![file exists $file]} {
	ined acknowledge "Can not read $file."
	return
    }
    if {[catch {open "$file" r} syslog]} {
	ined acknowledge "Can not open $file:" "" $syslog
	return
    }
    seek $syslog 0 end
    job create -command ev_tail -interval 1000
    ined restart [list "ev_file $file"]

    ev_connected
}

##
## Close the connection to out server or our file.
##

proc ev_close {} {

    global syslog menus

    if {[info exists syslog]} {
	catch {fileevent $syslog readable ""}
	catch {close $syslog}
	unset syslog
    }

    ined restart ""

    if {[info exists menus]} {
	foreach id $menus { ined delete $id }
    }

    set menus [ ined create MENU "Event" \
	       "Connect" "" \
	       "Create Filter" "List Filter" "Edit Filter" "Delete Filter" "" \
               "Reset" "Reset All" "" "Show Defaults" "" \
	       "Help Event" "Delete Event" ]
}

##
## This proc is called periodically to check if something new
## has been added to the syslog file.
##

proc ev_tail {} {

    global syslog

    if {[catch {
	while {[gets $syslog line] > 0} {
	    ev_convert $line
	}
    }]} {
	[job current] destroy
    }
}

##
## Callback when a message from the syslog daemon arrives.
##

proc ev_receive {} {

    global syslog

    if {[eof $syslog] || [catch {gets $syslog} line]} {
	Disconnect {}
	return
    }
    ev_convert $line
}

##
## Process the event contained in line.
##

proc ev_convert { line } {

    global sl_level sl_facility

    # scan the administrative fields

    if {[string match "-- MARK --" $line]} return

    set n [scan $line "%s %d.%d %s %d %d:%d:%d %s" \
	   host facility level month day hour min sec proc]
    if {$n != 9} {

	set n [scan $line "%s %d %d:%d:%d %s %s" \
	       month day hour min sec host proc]
	if {$n != 7} {
	    debug "** error parsing event message: $line"
	    return
	}
	set facility ""
	set level ""
    }

    # extract the message

    set i [string first $proc $line]
    if {$i < 0} {
	debug "** $line"
	debug "** can not extract message after $proc"
    }
    incr i [string length $proc]
    set message [string range $line $i end]

    # extract the pid of the proc

    set proc [string trim $proc "\]:"]
    set pid  [lindex [split $proc "\["] 1]
    set proc [lindex [split $proc "\["] 0]

    # convert the date to a gmt clock value

    set date [format "%s %2d %2d:%2d:%2d" $month $day $hour $min $sec]
    set clock [clock scan $date]

    # map the facility and level numbers to a readable string

    if {[info exists sl_level($level)]} {
	set level $sl_level($level)
    }

    if {[info exists sl_facility($facility)]} {
	set facility $sl_facility($facility)
    }

    ev_filter $host $facility $level $clock $proc $pid $message
}

##
## Filter an incoming event against all active filters.
##

proc ev_filter { host facility level clock process pid message } {

    global filter

    if {![info exists filter(ids)]} return

    foreach id $filter(ids) {

	if {$filter($id,status) != "active"} continue

	if {$filter($id,match) == "includes"} {
	    set doit [expr {[regexp -nocase $filter($id,host) $host]
		&& [regexp -nocase $filter($id,level) $level]
		&& [regexp -nocase $filter($id,facility) $facility]
		&& [regexp -nocase $filter($id,process) $process]
		&& [regexp -nocase $filter($id,message) $message]} ]
	    if {! $doit} continue
	} else {
	    set doit [expr {($filter($id,host) != "" 
			 && [regexp -nocase $filter($id,host) $host])
		     || ($filter($id,level) != "" 
			 && [regexp -nocase $filter($id,level) $level])
		     || ($filter($id,facility) != ""
			 && [regexp -nocase $filter($id,facility) $facility])
		     || ($filter($id,process) != "" 
			 && [regexp -nocase $filter($id,process) $process])
		     || ($filter($id,message) != ""
			 && [regexp -nocase $filter($id,message) $message])} ]
	    if {$doit} continue
	}

	if {[catch {dns address $host} ip]} {
	    if {[catch {nslook $host} ip]} {
		set ip ""
	    }
	}
	set ip [lindex $ip 0]
	set date [clock format $clock]

	# create a description line for the event

	set descr "$date"
	if {$facility != "" || $level != ""} {
            append descr " ($facility.$level)"
        }
	if {$filter($id,name) != ""} { append descr " ($filter($id,name))" }
	append descr " $host"
	if {$ip != ""} { append descr " \[$ip\]" }
	append descr " $process"
	if {$pid != ""} { append descr " \[$pid\]" }

	# write the event message

	if {$filter($id,report) == "global"} {
	    writeln $descr\n[string trim $message]"
	} else {
	    if {$filter($id,window) != ""} {
		if {[ined retrieve $filter($id,window)] == ""} {
		    set filter($id,window) ""
		}
	    }
	    if {$filter($id,window) == ""} {
		set filter($id,window) [ined create LOG]
		ined -noupdate name $filter($id,window) "$filter($id,name)"
	    }
	    ined append $filter($id,window) \
		    "$descr\n[string trim $message]\n"
	}

	if {$filter($id,action) != ""} {
	    catch {eval exec $filter($id,action)} result
	    if {$filter($id,report) == "global"} {
		writeln $result
	    } else {
		ined append $filter($id,window) $result
	    }
	}

	if {$filter($id,highlight) == "true" && $ip != ""} {
	    flash add $ip
	}

	if {$filter($id,report) == "global"} {
	    writeln 
	} else {
	    ined append $filter($id,window) "\n"
	}
    }
}

##
## Request actual events from the syslogd server.
##

proc "Connect" {list} {

    global server port use file
    global menus

    set result [ined request "Event Parameter" \
		 [list [list Server: $server] \
		       [list Port: $port] \
		       [list Use: $use radio server file ] \
		       [list File: $file] \
		 ] \
		 [list connect cancel] \
	       ]

    if {[lindex $result 0] == "cancel"} return

    set server  [lindex $result 1]
    set port    [lindex $result 2]
    set use     [lindex $result 3]
    set file    [lindex $result 4]

    if {$use == "server"} {
	ev_server $server $port
    } else {
	ev_file $file
    }
}

##
## Stop event listing.
##

proc "Disconnect" {list} {
    ev_close
}

##
## Create a new filter.
##

proc "Create Filter" {list} {

    set id [create_filter "Temporary Filter"]
    if {[edit_filter $id] != $id} {
	delete_filter $id
    }
}

##
## List all existing filters.
##

proc "List Filter" {list} {

    set id [select_filter]
    if {$id == ""} return

    list_filter $id
}

##
## Edit an existing filter.
##

proc "Edit Filter" {list} {

    set id [select_filter]
    if {$id == ""} return

    edit_filter $id
}

##
## Delete an existing filter
##

proc "Delete Filter" {list} {
    
    set id [select_filter]
    if {$id == ""} return

    delete_filter $id
}

##
## Reset flashing icons.
##

proc "Reset All" {list} {
    set ips ""
    foreach ip [flash list] {
	flash reset $ip
    }
}

proc "Reset" {list} {
    if {$list == ""} {
	set ips ""
	foreach ip [flash list] {
	    if {![catch {nslook $ip} name]} {
		lappend ips "$ip $name"
	    }
	}
	if {$ips != ""} {
	    set result [ined list "Select an IP address to reset:" $ips \
			[list reset cancel] ]
	    if {[lindex $result 0] == "cancel"} return
	    flash reset [lindex [lindex $result 1] 0]
	    return
	}
    } else {
	foreach comp $list {
	    if {[ined type $comp] != "NODE"} continue
	    set ip [GetIpAddress $comp]
	    flash reset $ip
	}
    }
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

proc "Help Event" {list} {
    ined browse "Help about the event logger:" {
	"Connect:" 
	"    Connect to a syslog daemon that supports event forwarding." 
	"" 
	"Disconnect:" 
	"    Disconnect from the currently selected syslog daemon." 
	"" 
	"Create Filter:" 
	"    Create a new event filter." 
	"" 
	"List Filter:" 
	"    List all defined event filter." 
	"" 
	"Edit Filter:" 
	"    Display and change an existing event filter. All event fields" 
	"    are matched against the corresponding event field using regular" 
	"    expressions." 
	"" 
	"Delete Filter:" 
	"    Remove a filter expression from the system." 
	"" 
	"Reset" 
	"    Reset the selected icons to normal state (remove all flashing" 
	"    going on). A menu of all flashing ip adresses is displayed" 
	"    when no icon is selected." 
	"" 
	"Reset All" 
	"    Reset all flashing icons to normal state." 
	"" 
	"Show Defaults:" 
	"    Show the defaults that may be defined in the tkined.defaults" 
	"    files. This script makes use of definitions in the form" 
	"" 
	"        event.server:        <string>" 
	"        event.port:          <number>" 
	"        event.file:          <string>" 
	"        event.use:           server | file" 
	"" 
	"        event.filter<i>.name:      <string>" 
	"        event.filter<i>.host:      <string>" 
	"        event.filter<i>.level:     <string>" 
	"        event.filter<i>.facility:  <string>" 
	"        event.filter<i>.process:   <string>" 
	"        event.filter<i>.message:   <string>" 
	"        event.filter<i>.action:    <string>" 
	"        event.filter<i>.status:    active | suspend" 
	"        event.filter<i>.highlight: true | false" 
	"        event.filter<i>.report:    local | global" 
	"" 
	"    where <i> is an index from 1 to the number of defined filters." 
    }
}

##
## Delete the tools created by this interpreter.
##

proc "Delete Event" {list} {
    global menus
    foreach id $menus { ined delete $id }
    exit
}

setup_default_filter

Disconnect {}

if {! [info exists default(start)]} {
    set default(start) ""
}

if {$default(start) == "auto"} {
    if {$use == "server"} {
	ev_server $server $port
    } else {
	ev_file $file
    }
}

vwait forever
