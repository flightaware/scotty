#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# clock.tcl -
#
#	A simple clock for tkined. Mainly an example to demonstrate
#	the job scheduler.
#
# Copyright (c) 1993-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: clock.tcl,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $

package require Tnm 3.0

namespace import Tnm::*

ined size

set clock_job ""
set clock_id ""

proc time_display {id} {

    set time [clock seconds]
    set sec  [expr $time % 60]
    set min  [expr ($time % 3600) / 60]
    set hour [expr ($time % 86400) / 3600]

    ined -noupdate attribute $id time [format "%2d:%02d:%02d" $hour $min $sec]
    ined label $id time
}

proc "Create Clock" {list} {

    global clock_job clock_id

    if {$clock_job == ""} {
        set clock_id [ined -noupdate create NODE]
        ined -noupdate icon $clock_id Clock
        ined -noupdate move $clock_id 100 40
        set clock_job [job create \
		       -command "time_display $clock_id" -interval 1000]
    }
} 

proc "Delete Clock" {list} {
    
    global clock_job clock_id
    
    if {$clock_id != ""} {
        ined delete $clock_id
        set clock_id ""
    }
    if {$clock_job != ""} {
        catch {$clock_job destroy}
        set clock_job ""
    }
}

proc  "Delete Time" {list} {

    global menus
    global clock_job clock_id

    "Delete Clock" {}
    foreach id $menus { ined delete $id }
    exit
}

set menus [ined create MENU Time \
	  "Create Clock" "Delete Clock" "" \
	  "Create Job" "Suspend Job" "Resume Job" "Kill Job" "Modify Job" \
	  "Job Info" "" \
	  "Delete Time"]

##
## Below are some generic procs that bring the job scheduler to the
## tkined user interface. They demonstrate how to program periodic 
## control or monitoring jobs.
##

proc "Create Job" {list} {
    
    static jobname interval count

    if {![info exists jobname]}  { set jobname "" }
    if {![info exists interval]} { set interval 5000 }
    if {![info exists count]}    { set count 0 }

    set result [ined request "Create a new job to execute." \
		 [list [list "Job Command:" $jobname] \
                       [list "Intervaltime \[ms\]:" $interval] \
                       [list "Count:" $count] ] \
                 [list create cancel] ]

    if {[lindex $result 0] == "cancel"}  return

    set jobname  [lindex $result 1]
    set interval [lindex $result 2]
    set count    [lindex $result 3]

    if {$jobname != ""} {
	job create -command $jobname -interval $interval -iterations $count
    }
}

proc "Suspend Job" {list} {
    set job [select_job suspend]
    if {$job == ""} return
    $job configure -status suspended
}

proc "Resume Job" {list} {
    set job [select_job resume]
    if {$job == ""} return
    $job configure -status waiting
}

proc "Kill Job" {list} {
    set job [select_job kill]
    if {$job == ""} return
    $job destroy
}

proc "Modify Job" {list} {

    set job [select_job modify]
    if {$job == ""} return 

    set jobname [$job cget -command]
    set jobtime [$job cget -interval]

    set result [ined request "Enter new values for job $job ($jobname)." \
		  [list [list "Job Name:" $jobname] \
                     [list "Intervaltime \[ms\]:" $jobtime scale 100 10000] ] \
		  [list "set values" cancel] ]

    if {[lindex $result 0] == "cancel"} return

    set jobname [lindex $result 1]
    set jobtime [lindex $result 2]

    $job configure -interval $jobtime  
    $job configure -command  $jobname
}

proc "Job Info" {list} {

    set jobs [job find]

    if {$jobs == ""} {
	ined acknowledge "Sorry, no jobs available."
	return
    } 

    set result ""
    set len 0
    foreach j $jobs {

	set line \
	    [format "%s %6.1f %6.1f %3d %8s %s" \
	    $j [$j cget -interval] [$j cget -time] \
	    [$j cget -iterations] [$j cget -status] [$j cget -command] ]

	if {[string length $line] > $len} {
            set len [string length $line]
        }

        lappend result $line
    }

    set header " ID    INTV   REM CNT  STATUS      CMD"

    for {set i [string length $header]} {$i < $len} {incr i} {
	append header " "
    }

    ined browse $header $result
}

proc select_job { action } {
    
    set jobs [job find]

    if {$jobs == ""} {
	ined acknowledge "Sorry, no job to $action."
        return
    }

    set res ""
    foreach j $jobs {
	set status [$j cget -status]
	if {   ($action == "resume"  && $status == "suspended")
            || ($action == "suspend" && $status == "waiting")
            || ($action == "kill") || ($action == "modify")} {
		lappend res [format "%s %s" $j [$j cget -command]]
	    }
    }

    if {$res == ""} {
	ined acknowledge "Sorry, no job to $action."
	return ""
    } 

    set res [ined list "Choose a job to $action:" $res [list $action cancel]]
    if {[lindex $res 0] == "cancel"} {
	return ""
    } else {
	return [lindex [lindex $res 1] 0]
    }
}

vwait forever
