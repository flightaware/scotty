#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# snmp_browser.tcl -
#
#	A simple SNMP MIB browser for Tkined.
#
# Copyright (c) 1993-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: snmp_browser.tcl,v 1.1.1.1 2006/12/07 12:16:59 karl Exp $

package require Tnm 3.0
package require TnmSnmp $tnm(version)
package require TnmMib $tnm(version)

namespace import Tnm::*

ined size
LoadDefaults snmp
SnmpInit SNMP-Browser

##
## Callback for walk button.
##

proc mb_walk { browser prefix } {
    foreach id [ined -noupdate select] {
	ForeachIpNode id ip host [list [ined -noupdate retrieve $id] ] {
	    set s [SnmpOpen $id $ip]
	    catch {TnmSnmp::Walk $s $prefix} txt
	    $s destroy
	    writeln $txt
	    writeln
	}
    }
}

##
## Callback for the monitor button.
##

proc mb_monitor { browser prefix } {

    static monitor

    foreach id [ined -noupdate select] {
	lappend list [ined -noupdate retrieve $id]
    }

    if {![info exists list]} return

    if {![info exists monitor]} {
	set monitor [ined create INTERPRETER snmp_monitor.tcl]
    } else {
	if {[ined retrieve $monitor] == ""} {
	    set monitor [ined create INTERPRETER snmp_monitor.tcl]
	}
    }

    ined send $monitor MonitorVariable $list $prefix
}

##
## Callback for the set button.
##

proc mb_set { browser prefix } {

    ined size
    after 1000

    foreach id [ined -noupdate select] {
	ForeachIpNode id ip host [list [ined -noupdate retrieve $id] ] {
	    set s [SnmpOpen $id $ip]
	    set name [split $prefix .]
	    set len [llength $name]
	    incr len -1
	    set name [lindex $name $len]
	    set query ""
	    $s walk x $prefix {
		set name  [mib name [lindex [lindex $x 0] 0]]
		set value [lindex [lindex $x 0] 2]
		set pfx [lindex [split $name .] 0]
		set idx [join [lrange [split $name .] 1 end] .]
		lappend query [list $pfx.$idx $value]
		lappend oidlist "$pfx.$idx"
	    }
	    if {$query == ""} {
		$s destroy
		continue
	    }
	    set result [ined request "Set SNMP variable on $host \[$ip\]:" \
		    $query [list "set value" cancel]]
	    if {[lindex $result 0] == "cancel"} {
		$s destroy
                continue
	    }
	    set i 0
            foreach value [lrange $result 1 end] {
                lappend varbind [list [lindex $oidlist $i] $value]
                incr i
            }
            if {[catch {$s set $varbind} error]} {
                ined acknowledge "Set operation failed:" "" $error
            }
	    $s destroy
	}
    }
}

##
## Callback for show button (everything not in the first line).
##

proc mb_showit { browser prefix } {

    set subs [mib children $prefix]
    
    ined -noupdate clear $browser
    ined -noupdate hyperlink $browser "mb_walk $browser $prefix" --walk--
    ined -noupdate append $browser " "

    set last [mib parent $prefix]
    if {$last != "iso"} {
	ined -noupdate hyperlink $browser "mb_showit $browser $last" ---up---
	ined -noupdate append $browser " "
    }

    if {$last != ""} {
	set brothers [mib children $last]
	set idx [lsearch $brothers $prefix]
	incr idx -1
	set previous [lindex $brothers $idx]
	incr idx 2
	set next [lindex $brothers $idx]
	if {$previous != ""} {
	    ined -noupdate hyperlink $browser \
		    "mb_showit $browser $previous" -previous-
	    ined -noupdate append $browser " "
	}
	if {$next != ""} {
	    ined -noupdate hyperlink $browser \
		    "mb_showit $browser $next" --next--
	    ined -noupdate append $browser " "
	}
    }

    if {$subs == ""} {
	set oid [mib oid $prefix]
	set type [mib syntax $oid]
	if {[lsearch "Gauge Gauge32 Counter Counter32 INTEGER Integer32" $type] >= 0} {
	    ined -noupdate hyperlink $browser \
		    "mb_monitor $browser $prefix" -monitor-
	    ined -noupdate append $browser " "
	}
	set access [mib access $prefix]
	if {[string match *write* $access]} {
	    ined -noupdate hyperlink $browser \
		    "mb_set $browser $prefix" --set--
	}
    }

    ined -noupdate append $browser "\n\n"
    ined -noupdate append $browser "Path:        $prefix\n"

    if {$subs == ""} {
	ined -noupdate append $browser "\n[TnmMib::DescribeNode $prefix]"
	mb_walk $browser $prefix
    } else {
	set toggle 0
	ined -noupdate append $browser "\n"
	foreach elem $subs {
	    ined -noupdate hyperlink \
		$browser "mb_showit $browser $elem" [mib label $elem]
	    set len [string length [mib label $elem]]
	    incr len +2
	    if {$toggle} { 
		set toggle 0
		ined -noupdate append $browser "\n" 
	    } else {
		set toggle 1
		if {$len < 8} {
		    ined -noupdate append $browser "\t\t\t\t"
		} elseif {$len < 16} {
		    ined -noupdate append $browser "\t\t\t"
		} elseif {$len < 24} {
		    ined -noupdate append $browser "\t\t"
		} else {
		    ined -noupdate append $browser "\t"
		}
	    }
	}
    }
}

##
## This recursive proc creates menu entries by appending to the 
## global var mibcmds.
##

proc makemenu { top level } {
    global mibcmds
    set scalars ""
    set subs ""
    set writeable ""
    set level [mib oid $level]
    set name  [mib name $level]
    foreach suc [mib children $level] {
	if {[mib children $suc] == ""} {
	    set access [mib access $suc]
	    if {$access == "not-accessible"} 	continue
	    lappend scalars $suc
	    if {[string match *write* $access]} {
		lappend writeable $suc
	    }
	} else {
	    lappend subs $suc
	}
    }
    if {$scalars != ""} {
	set cmd "[mib label $name] ([mib module $name])"
	lappend mibcmds $top:[mib label $name]:$cmd
	proc $cmd { list } "ShowScalars \$list $name"
	if {$writeable != ""} {
	    set cmd "edit [mib label $name] ([mib module $name])"
	    lappend mibcmds $top:[mib label $name]:$cmd
	    proc $cmd { list } "EditScalars \$list $name"
	}
    }
    foreach suc $subs {
	if {[mib syntax $suc] == "SEQUENCE OF"} {
	    set writeable ""
	    set entries [mib children [mib children $suc]]
	    foreach entry $entries {
		set access [mib access $entry]
		if {[string match *write* $access]} {
		    lappend writeable $suc
		}
	    }
	    set cmd "[mib label $suc] ([mib module $name])"
	    lappend mibcmds $top:[mib label $name]:$cmd
	    proc $cmd { list } "ShowTable \$list $suc"
	    if {$writeable != ""} {
		set cmd "edit [mib label $suc] ([mib module $name])"
		lappend mibcmds $top:[mib label $name]:$cmd
		proc $cmd { list } "EditTable \$list $suc"
	    }
	} else {
	    makemenu $top:[mib label $name] $suc
	}
    }
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
## Edit all scalars of a group that are writeable.
##

proc EditScalars {list group} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	SnmpEditScalars $s $group
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
## Edit a complete table for the node objects in list.
##

proc EditTable {list table args} {
    ForeachIpNode id ip host $list {
	set s [SnmpOpen $id $ip]
	SnmpEditTable $s $table $args
	$s destroy
    }
}

##
## Start a MIB browser for this device.
##

proc "MIB Browser" { list } {

    global snmp_browser
    
    set browser [ined -noupdate create LOG]
    lappend snmp_browser $browser
    ined -noupdate name $browser "SNMP MIB Browser"
    mb_showit $browser internet
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
## Set the parameters (community, timeout, retry) for snmp requests.
##

proc "Set SNMP Parameter" {list} {
    SnmpParameter $list
}

##
## Display some help about this tool.
##

proc "Help SNMP-Browser" {list} {
    ined browse "Help about SNMP-Browser" {
	"MIB Browser:" 
	"    Open a new window with a MIB browser. You can step through" 
	"    the MIB similar to a file selector. When reaching a leaf," 
	"    the selected nodes are queried for the selected object and" 
	"    a description of the objects will be displayed." 
	"" 
	"<MIB MENUS>:" 
	"    These menus allow you to access MIB scalars and tables very" 
	"    once you know what you are looking for. All menus are created" 
	"    from the MIB database. New submenus should automatically appear" 
	"    if you add new MIB definitions to the MIB database." 
	"" 
	"Walk MIB Tree:" 
	"    Walk through the MIB tree and print the object values." 
	"" 
	"Set SNMP Parameter:" 
	"    This dialog allows you to set SNMP parameters like retries, " 
	"    timeouts, community name and port number. " 
    }
}

##
## Delete the menus created by this interpreter.
##

proc "Delete SNMP-Browser" {list} {
    global menus snmp_browser
    foreach id $snmp_browser { 
	catch {ined -noupdate delete $id}
    }
    foreach id $menus { ined delete $id }
    exit
}

##
## Create the MENU which will contain submenus to access the MIB
## information quickly (good if you know what you are searching for). 
##

set mibcmds [list "MIB Browser" ""]

catch {
    foreach suc [mib children [mib oid mib-2]] {
	makemenu mib-2 $suc
    }
}

catch {
    foreach suc [mib children [mib oid snmpModules]] {
	makemenu snmpModules $suc
    }
}

lappend mibcmds ""

catch {
    foreach suc [mib children [mib oid private.enterprises]] {
	foreach moresuc [mib children $suc] {
	    makemenu [mib label $suc] $moresuc
	}
    }
}

catch {
    foreach suc [mib children experimental] {
	foreach moresuc [mib children [mib oid $suc]] {
	    makemenu [mib label $suc] $moresuc
	}
    }
}

lappend mibcmds "" \
    "Walk MIB Tree" "" \
    "Set SNMP Parameter" "" \
    "Help SNMP-Browser" "Delete SNMP-Browser"

set menus [eval ined create MENU "SNMP-Browser" $mibcmds]

vwait forever
