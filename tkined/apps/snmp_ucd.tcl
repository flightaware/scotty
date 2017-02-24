#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# snmp_ucd.tcl -
#
#        Simple SNMP monitoring scripts for UCD agents.
#
# Copyright (c) 1993-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: snmp_ucd.tcl,v 1.1.1.1 2006/12/07 12:16:59 karl Exp $

package require Tnm 3.0

namespace import Tnm::*

ined size
LoadDefaults snmp monitor
SnmpInit SNMP-UCD

if {[info exists default(interval)]} {
    set interval $default(interval)
} else {
    set interval 60
}
if {![info exists default(graph)]} {
    set default(graph) false
}

##
## Load the UCD MIBs.
##

mib load UCD-SNMP-MIB

##
## Create a chart. Honours the monitor.graph default definition.
##


proc CreateChart {id x y} {
    global default
    if {$default(graph) != "true"} {
        set id [CloneNode $id [ined create STRIPCHART] $x $y]
    } else {
        set id [CloneNode $id [ined create GRAPH]]
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

    foreach id $args {
        catch {ined -noupdate clear $id}
        catch {ined -noupdate clear $id}
    }
    catch {eval $cmd $args}

    if {[info exists old_interval]} {
        set interval $old_interval
    }
}


###############################################################################
##
## Calculate the percentage of memory allocated by reading the UCD
## enterprise tables.  Useful for monitoring memory/swap.
##
## util =   ( ( memTotalReal - memTotalFree + memCached ) / memTotalReal )
##        + ( memAvailSwap / memTotalSwap )
##
## Rational: memCached is supposed to be readily available for applications,
##           hence we count it as free.
##

proc ShowMemoryUtilProc {id status vbl} {
    global cx

    if {$status != "noError"} {
            ined -noupdate attribute $id MemoryLoad "Memory: $status"
            return
    }
        
    set now     [mib scan sysUpTime [lindex [lindex $vbl 0] 2]]
    set totalSwap [lindex [lindex $vbl 1] 2]
    set availSwap [lindex [lindex $vbl 2] 2]
    set totalReal [lindex [lindex $vbl 3] 2]
    set totalFree [lindex [lindex $vbl 4] 2]
    set Cached    [lindex [lindex $vbl 5] 2]
    
    if {$totalSwap > 0} {
        set total   [expr $totalSwap + $totalReal ]
        set free    [expr $totalFree + $Cached + $availSwap ]
        set used    [expr $total - $free ]
        set val     [expr 1.0 * $used / $total * 100.0 ]
    } else {
        set val 0
    }

    ined values $id $val
    if {$val > 90} {
        ined -noupdate color $id red
    } else {
        ined -noupdate color $id blue
    }
    ined -noupdate attribute $id MemoryLoad [format "Memory: %.2f %%" $val]

    set txt "[ined name $id] Memory used"
    MoJoCheckThreshold $id $txt $val "%%"
    
    set cx(sysUpTime,$id)                   $now
    set cx(memTotalSwap,$id)                $totalSwap
    set cx(memAvailSwap,$id)                $availSwap
    set cx(memTotalReal,$id)                $totalReal
    set cx(memTotalFree,$id)                $totalFree
    set cx(memCached,$id)                   $Cached
}

proc ShowMemoryUtil { ids } {
    global cx
    foreach id [MoJoCheckIds ShowMemoryUtil $ids] {
        set i $cx(idx,$id)
        set vbl "sysUpTime.0 memTotalSwap.0 memAvailSwap.0 memTotalReal.0 memTotalFree.0 memCached.0"
        $cx(snmp,$id) get $vbl "ShowMemoryUtilProc $id %E \"%V\""
    }
}

##
## Start a new memory util monitoring job for the device given by ip. 
##
proc start_memory_monitor { ip args } {

    global cx interval

    set ids ""
    set re_args ""

    set id [lindex $args 0]
        
    if {$id != ""} {
        set sh [SnmpOpen $id $ip]
        if {[catch {$sh get [list sysUpTime.0 \
                                  memTotalSwap.0 memAvailSwap.0 \
        	                      memTotalReal.0 memTotalFree.0 \
        	                      memCached.0]} value]} {
            writeln "start_memory_monitor: Can not read memory info"
            return 
        }
            
        set cx(sysUpTime,$id)      [lindex [lindex $value 0] 2]
        set cx(memTotalSwap,$id)   [lindex [lindex $value 1] 2]
        set cx(memAvailSwap,$id)   [lindex [lindex $value 2] 2]
        set cx(memTotalReal,$id)   [lindex [lindex $value 3] 2]
        set cx(memTotalFree,$id)   [lindex [lindex $value 4] 2]
        set cx(memCached,$id)      [lindex [lindex $value 5] 2]
        set cx(sysUpTime,$id)      [mib scan sysUpTime $cx(sysUpTime,$id)]
        set cx(snmp,$id) $sh
        set cx(idx,$id) $ip

        ined -noupdate attribute $id MemoryLoad "Memory: waiting"
        ined -noupdate label $id MemoryLoad

        lappend ids $id

    }
    append re_args " \$$id"

    if {$ids != ""} {
        set jid [job create -command [list ShowMemoryUtil $ids] \
        	 -interval [expr $interval * 1000]]
        save "restart start_memory_monitor $interval $jid $ip $re_args"
    }
}


proc "Memory Utilization" { list } {
    ForeachIpNode id ip host $list {
        set sh [SnmpOpen $id $ip]
        if {[catch {
            $sh walk x "memTotalSwap memAvailSwap memTotalReal memTotalFree memCached" {
            set totalSwap [lindex [lindex $x 0] 2]
            set availSwap [lindex [lindex $x 1] 2]
            set totalReal [lindex [lindex $x 2] 2]
            set totalFree [lindex [lindex $x 3] 2]
            set Cached    [lindex [lindex $x 4] 2]
            }
        }]} {
            $sh destroy
            continue
        }

        set args [CreateChart $id 30 30]
        eval start_memory_monitor $ip $args
        $sh destroy
    }
}


###############################################################################
##
## Calculate the percentage of memory allocated by reading the UCD
## enterprise tables.  Useful for monitoring memory/swap.
##
## Rational: Swap usage is often a better indicator of wether you need
##           more RAM or not.
##

proc ShowSwapUtilProc {id status vbl} {
    global cx

    if {$status != "noError"} {
            ined -noupdate attribute $id SwapLoad "Swap: $status"
            return
    }
        
    set now     [mib scan sysUpTime [lindex [lindex $vbl 0] 2]]
    set totalSwap [lindex [lindex $vbl 1] 2]
    set availSwap [lindex [lindex $vbl 2] 2]
    
    if {$totalSwap > 0} {
        set total   [expr $totalSwap ]
        set free    [expr $availSwap ]
        set used    [expr $total - $free ]
        set val     [expr 1.0 * $used / $total * 100.0 ]
    } else {
        set val 0
    }

    ined values $id $val
    if {$val > 90} {
        ined -noupdate color $id red
    } else {
        ined -noupdate color $id blue
    }
    ined -noupdate attribute $id SwapLoad [format "Swap: %.2f %%" $val]

    set txt "[ined name $id] Swap used"
    MoJoCheckThreshold $id $txt $val "%%"
    
    set cx(sysUpTime,$id)                   $now
    set cx(memTotalSwap,$id)                $totalSwap
    set cx(memAvailSwap,$id)                $availSwap
}


proc ShowSwapUtil { ids } {
    global cx
    foreach id [MoJoCheckIds ShowSwapUtil $ids] {
        set i $cx(idx,$id)
        set vbl "sysUpTime.0 memTotalSwap.0 memAvailSwap.0"
        $cx(snmp,$id) get $vbl "ShowSwapUtilProc $id %E \"%V\""
    }
}

##
## Start a new memory util monitoring job for the device given by ip. 
##
proc start_swap_monitor { ip args } {

    global cx interval

    set ids ""
    set re_args ""

    set id [lindex $args 0]
        
    if {$id != ""} {
        set sh [SnmpOpen $id $ip]
        if {[catch {$sh get [list sysUpTime.0 \
                                  memTotalSwap.0 memAvailSwap.0 \
        	                      ]} value]} {
            writeln "start_swap_monitor: Can not read memory info"
            return 
        }
            
        set cx(sysUpTime,$id)      [lindex [lindex $value 0] 2]
        set cx(memTotalSwap,$id)   [lindex [lindex $value 1] 2]
        set cx(memAvailSwap,$id)   [lindex [lindex $value 2] 2]
        set cx(sysUpTime,$id)      [mib scan sysUpTime $cx(sysUpTime,$id)]
        set cx(snmp,$id) $sh
        set cx(idx,$id) $ip

        ined -noupdate attribute $id SwapLoad "Swap: waiting"
        ined -noupdate label $id SwapLoad

        lappend ids $id

    }
    append re_args " \$$id"

    if {$ids != ""} {
        set jid [job create -command [list ShowSwapUtil $ids] \
        	 -interval [expr $interval * 1000]]
        save "restart start_swap_monitor $interval $jid $ip $re_args"
    }
}


proc "Swap Utilization" { list } {
    ForeachIpNode id ip host $list {
        set sh [SnmpOpen $id $ip]
        if {[catch {
            $sh walk x "memTotalSwap memAvailSwap" {
            set totalSwap [lindex [lindex $x 0] 2]
            set availSwap [lindex [lindex $x 1] 2]
            }
        }]} {
            $sh destroy
            continue
        }

        set args [CreateChart $id 30 30]
        eval start_swap_monitor $ip $args
        $sh destroy
    }
}


###############################################################################
##
##  Show load average.
##
## Rational: although also available by rstatd, this is now available
##           via SNMP for when rstatd isn't available.
##

proc ShowLoadUtilProc {id status vbl} {
    global cx

    if {$status != "noError"} {
            ined -noupdate attribute $id LoadLoad "Load: $status"
            return
    }
        
    set now         [mib scan sysUpTime [lindex [lindex $vbl 0] 2]]
    set loadAverage [lindex [lindex $vbl 1] 2]
    set val         $loadAverage
    
    if {$val > 3} {
        ined -noupdate color $id red
    } else {
        ined -noupdate color $id blue
    }
    set load [expr $val * 100 ]
    ined -noupdate values $id $load
    ined -noupdate attribute $id LoadLoad [format "Load: %.2f" $val]

    set txt "[ined name $id] Load used"
    MoJoCheckThreshold $id $txt $val
    
    set cx(sysUpTime,$id)   $now
    set cx(laLoad,$id)      $loadAverage
}

proc ShowLoadUtil { ids } {
    global cx
    foreach id [MoJoCheckIds ShowLoadUtil $ids] {
        set i $cx(idx,$id)
        set vbl "sysUpTime.0 laLoad.1"
        $cx(snmp,$id) get $vbl "ShowLoadUtilProc $id %E \"%V\""
    }
}

##
## Start a new load average util monitoring job for the device given by ip. 
##
proc start_load_monitor { ip args } {

    global cx interval

    set ids ""
    set re_args ""

    set id [lindex $args 0]
        
    if {$id != ""} {
        set sh [SnmpOpen $id $ip]
        if {[catch {$sh get [list sysUpTime.0 \
                                  laLoad.1 \
        	                      ]} value]} {
            writeln "start_load_monitor: Can not read load average info"
            return 
        }
            
        set cx(sysUpTime,$id) [lindex [lindex $value 0] 2]
        set cx(laLoad,$id)    [lindex [lindex $value 1] 2]
        set cx(sysUpTime,$id) [mib scan sysUpTime $cx(sysUpTime,$id)]
        set cx(snmp,$id)      $sh
        set cx(idx,$id)       $ip

        ined -noupdate attribute $id LoadLoad "Load: waiting"
        ined -noupdate label $id LoadLoad

        lappend ids $id

    }
    append re_args " \$$id"

    if {$ids != ""} {
        set jid [job create -command [list ShowLoadUtil $ids] \
        	 -interval [expr $interval * 1000]]
    }
}
    
proc "Load Average" { list } {
    ForeachIpNode id ip host $list {
        set sh [SnmpOpen $id $ip]
        if {[catch {
            $sh walk x "laLoad.1" {
            set loadAverage [lindex [lindex $x 0] 2]
            }
        }]} {
            $sh destroy
            continue
        }

        set args [CreateChart $id 30 30]
        eval start_load_monitor $ip $args
        $sh destroy
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
## Set the parameters (community, timeout, retry) for snmp requests.
##

proc "Set SNMP Parameter" {list} {
    SnmpParameter $list
}

##
## Set the default parameters for monitoring jobs.
##

proc "Set Monitor Parameter" {list} {
    MoJoParameter
}

##
## Display some help about this tool.
##

proc "Help SNMP-UCD" {list} {
    ined browse "Help about SNMP-UCD" {
        "The SNMP-UCD contains a set of utilities to monitor hosts" 
        "using SNMP requests.  This plugin is specifically for the"
        "ucdavies enterprise MIB.  This is mainly useful for servers"
        "running ucd-snmp or net-snmp -- mainly Linux and BSD machines."
        ""
        "You can define thresholds by setting the" 
        "attributes Monitor:RisingThreshold or Monitor:FallingThreshold." 
        "The attribute Monitor:ThresholdAction controls what action will" 
        "be taken if the rising threshold or the falling threshold is" 
        "exceeded. The action types are syslog, flash and write." 
        "" 
        "Memory Utilization:"
        "    This displays the percentage of virtual memory used."
        "    This includes RAM and swap."
        ""
        "Swap Utilization:"
        "    This displays the percentage of swap used.  This is often"
        "    a better indication of low-memory situations than the"
        "    Memory Utilization."
        ""
        "Load Average:"
        "    Obtain the load average by SNMP."
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
        "Set SNMP Parameter:" 
        "    This dialog allows you to set SNMP parameters like retries, " 
        "    timeouts, community name and port number. " 
        "" 
        "Set Monitor Parameter:" 
        "    This dialog allows you to set the sampling interval and " 
        "    other parameter related to monitoring jobs." 
    }
}

##
## Delete the menus created by this interpreter.
##

proc "Delete SNMP-UCD" {list} {

    global menus

    if {[job find] != ""} {
        set res [ined confirm "Kill running monitoring jobs?" \
        	 [list "kill & exit" cancel] ]
        if {$res == "cancel"} return
    }

    DeleteClones

    foreach id $menus { ined delete $id }
    exit
}

set menus [ ined create MENU "SNMP-UCD" \
           "Memory Utilization" "Swap Utilization" "Load Average" "" \
           "Monitor Job Info" "Modify Monitor Job" "" \
           "Set SNMP Parameter" "Set Monitor Parameter" "" \
           "Help SNMP-UCD" "Delete SNMP-UCD" ]

vwait forever


