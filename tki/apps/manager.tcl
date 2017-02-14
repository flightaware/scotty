#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# manager.tcl -
#
#	A script which is used to launch other applications.
#
# Copyright (c) 1993-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1997-2000 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: manager.tcl,v 1.1.1.1 2006/12/07 12:16:59 karl Exp $

package require Tnm 3.0

namespace import Tnm::*

ined size
LoadDefaults remote manager

##
## Set up some default parameters.
##

if {[info exists default(listen)]} {
    set dolist $default(listen)
} else {
    set dolist false
}

if {[info exists default(silent)]} {
    set doaccept $default(silent)
} else {
    set doaccept false
}

##
## Search for the interpreter. Return the absolute path or an
## empty string if not found.
##

proc FindScript { fname } {

    global auto_path

    if {[file exists $fname]} { 
	return [file dirname $fname]/[file tail $fname]
    }

    foreach dir $auto_path {
        if {[file exists $dir/$fname]} {
            return [file dirname $dir/$fname]/$fname
        }
    }

    return ""
}

##
## Start the interpreter given by base (searching the path).
## Be smart and check accept names without a tcl extension.
##

proc StartScript { base args } {

    set fullname [FindScript $base]
    if {$fullname == ""} {
	set fullname [FindScript $base.tcl]
    }

    if {$fullname == ""} {
	ined acknowledge "Unable to find $base nor $base.tcl."
	return
    }

    if {[catch {ined create INTERPRETER [concat $fullname $args]} msg]} {
	ined acknowledge "$msg"
    }
}

##
## Find a program searching along the environment variable PATH.
##

proc FindProgram { fname } {

    global env

    if {[info exists env(PATH)]} {
	set path [split $env(PATH) :]
    } else {
	set path "/bin /usr/bin /usr/local/bin"
    }

    if {[file exists $fname]} {
        return [file dirname $fname]/[file tail $fname]
    }

    foreach dir $path {
        if {[file exists $dir/$fname]} {
	    if {[file executable $dir/$fname]} {
		return [file dirname $dir/$fname]/$fname
	    }
        }
    }

    return ""
}

##
## Start the program given by fname (searching the path).
##

proc StartProgram { fname } {

    set fullname [FindProgram $fname]

    if {$fullname == ""} {
	ined acknowledge "Unable to find $fname."
    } else {
	exec "$fullname" "&"
    }
}

##
## Fire up various tcl scripts for different tasks. These procs
## are registered in the Tools menu below and called from the 
## tkined editor.
##

proc "IP Trouble" { list } {
    StartScript ip_trouble.tcl
}

proc "IP Monitor" { list } {
    StartScript ip_monitor.tcl
}

proc "IP Layout" { list } {
    StartScript ip_layout.tcl
}

proc "IP Discover" { list } {
    StartScript ip_discover.tcl
}

proc "IP World" { list } {
    StartScript ip_world.tcl
}

proc "SNMP Trouble" { list } {
    StartScript snmp_trouble.tcl
}

proc "SNMP Monitor" { list } {
    StartScript snmp_monitor.tcl
}

proc "SNMP Script-MIB" { list } {
    StartScript snmp_scriptmib.tcl
}

proc "SNMP Host-MIB" { list } {
    StartScript snmp_host.tcl
}

proc "SNMP Config" { list } {
    StartScript snmp_config.tcl
}

proc "SNMP Browser" { list } {
    StartScript snmp_browser.tcl
}

proc "SNMP Tree" { list } {
    StartScript mibtree -i
}

proc "SNMP CISCO" { list } {
    StartScript snmp_cisco.tcl
}

proc "SNMP HP" { list } {
    StartScript snmp_hp.tcl
}

proc "SNMP UCD" { list } {
    StartScript snmp_ucd.tcl
}

proc "SNMP ACCT" { list } {
    StartScript snmp_acct.tcl
}

proc "SNMP HTTP" { list } {
    StartScript snmp_http.tcl
}

proc "User Client" { list } {
    StartScript netguard_usr.tcl
}

proc "Admin Client" { list } {
    StartScript netguard_adm.tcl
}

proc "Tkgraphs Client" { list } {
    StartProgram tkgraphs
}

proc "Event Filter" { list } {
    StartScript event.tcl
}

proc "GAME" { list } {
    StartScript game.tcl
}

proc "Bones" { list } {
    StartScript bones.tcl
}

proc "SimuLan" { list } {
    StartScript /usr/local/lib/simuLan/simulan_tool.tcl
}

##
## Show a clipboard that can be used to enter ined commands for
## interactive debugging purposes.
##

proc "Show Clipboard" { list } {

    static log

    if {[info exists log]} {
	if {[ined -noupdate retrieve $log] != ""} {
	    set res [ined confirm "Replace previous clipboard?" \
		     [list replace cancel] ]
	    if {$res != "replace"} return
	}
	ined -noupdate delete $log
    }

    set log [ined -noupdate -notrace create LOG]
    ined -noupdate name $log "tkined clipboard"
}

##
## Let the user select a script file and start it as a tkined
## interpreter.
##

proc "Start Script" { list } {

    static dir
    
    if {![info exists dir]} { set dir "" }

    set file [ined openfile "Please select a script file:" $dir]
    if {$file == ""} return
    
    set dir [file dirname $file]
    StartScript $file
}

##
## Kill an application form the tkined editor.
##

proc "Kill Interpreter" { list } {

    set interps ""
    set tools ""
    foreach comp [ined retrieve] {
	switch [ined type $comp] {
	    INTERPRETER {
		lappend interps $comp
	    }
	    MENU {
		lappend tools $comp
	    }
	}
    }

    set list ""
    foreach interp $interps {
	set id [ined id $interp]
	set  tlist ""
	foreach tool $tools {
	    if {[ined interpreter [ined id $tool]] == $id} {
		lappend tlist [lindex $tool 2]
	    }
	}
	set name [file tail [ined name $id]]
	lappend list [format "%-14s(%s) %s" $id $name $tlist]
    }

    set result [ined list "Select an interpreter to kill:" $list \
		[list kill cancel] ]
    if {[lindex $result 0] == "cancel"} return

    ined delete [lindex [lindex $result 1] 0]
}

##
## Display some help about this tool.
##

proc "Help Tools" { list } {
    ined browse "Help about Tool Manager" {
	"The Tool Manager is responsible to dynamically load new tools" 
	"into tkined. It currently knows about the following tools. (Note" 
	"that not all of them may be available on your site.)" 
	"" 
	"IP Trouble:" 
	"    A set of commands to find out why something is broken." 
	"" 
	"IP Monitor:" 
	"    Some simple monitoring commands. They allow you to sit down" 
	"    and watch whats going on." 
	"" 
	"IP Layout:" 
	"    These commands help you to layout your network." 
	"" 
	"IP Discover:" 
	"    Discover the IP structure of you network. This saves a lot" 
        "    of time when starting INED without a useable network map." 
	"" 
	"SNMP Trouble:" 
	"    Commands to query and monitor your SNMP devices." 
	"" 
	"SNMP Monitor:" 
	"    Monitor SNMP variables. Needs more work to be serious." 
	"" 
	"SNMP Script-MIB:" 
	"    Commands to interact with IETF's remote scripting MIB." 
	"" 
	"SNMP Host-MIB:" 
	"    Commands to interact with IETF's host resources MIB." 
	"" 
	"SNMP Config:" 
	"    Commands to configure SNMP engines." 
	"" 
	"SNMP Browser:" 
	"    A MIB browser to manually inspect a MIB." 
	"" 
	"SNMP Private:" 
	"    SNMP commands specific to some private MIB extensions." 
	"" 
	"Event Filter:" 
	"    Display and filter events collected by the syslog daemon." 
	"" 
	"Start Script:" 
	"    Prompt the user for the file name of a script to execute." 
	"" 
	"Kill Interpreter:" 
	"    Kill an interpreter. This may be useful if a tool seems." 
	"    to hang in a loop. Note, it may be difficult to find the" 
	"    right interpreter if you have one tool running more than" 
	"    once." 
	"" 
	"Show Clipboard:" 
	"    Open a clipboard where you may save some notices." 
    }
}

##
## Delete the TOOL Manager and exit this interpreter.
##

proc "Delete Tools" { list } {
    global menus
    foreach id $menus {	ined delete $id }
    exit
}

##
## Set up the menu. This is ugly because we have features locally
## that should not appear on other sites.
##

set cmds [list "IP Trouble" "IP Monitor" "IP Layout" "IP Discover" "IP World" ""]
if {[lsearch [info commands] snmp] >= 0} {
    lappend cmds "SNMP Trouble"
    lappend cmds "SNMP Monitor"
    lappend cmds "SNMP Script-MIB"
    lappend cmds "SNMP Host-MIB"
    lappend cmds "SNMP Config"
    lappend cmds "SNMP Browser"
    if {[FindScript mibtree] != ""} {
        lappend cmds "SNMP Tree"
    }
    lappend cmds "SNMP Private:SNMP CISCO"
    lappend cmds "SNMP Private:SNMP HP"
    lappend cmds "SNMP Private:SNMP UCD"
    lappend cmds ""
}
lappend cmds "Event Filter"
if {[info commands msqlconnect] != ""} {
    lappend cmds "Bones"
}

# lappend cmds "Remote tkined:Connect"
# lappend cmds "Remote tkined:Listen"
# lappend cmds "Remote tkined:Info"

set done 0
if {[info exists default]} {
    foreach name [array names default] {
	set l [split $name .]
	if {[lindex $l 0] == "tool"} {
	    
	    if {$done == 0} { lappend cmds "" ; set done 1}
	    set toolname [lindex $l 1]
	    set toolcmd $default($name)
	    
	    # define a proc for the callback
	    
	    proc $toolname [list list] [list StartScript $toolcmd]
	    
	    lappend cmds $toolname
	}
    }
}


lappend cmds ""
lappend cmds "Start Script" 
lappend cmds "Kill Interpreter"
lappend cmds "Show Clipboard" 
lappend cmds ""
lappend cmds "Help Tools" 
lappend cmds "Delete Tools"

set menus [eval ined create MENU Tools $cmds]

vwait forever
