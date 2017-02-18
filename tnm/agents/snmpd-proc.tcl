# snmpd-proc.tcl --
#
#	This file implements a table of actually running processes.
#
# Copyright (c) 1994-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

Tnm::mib load TUBS-IBR-PROC-MIB

##
## The proc group contains a table of actually running processes.
## This is much the same as the running software group of the Host
## Resources MIB, but it was here much earlier and it has a mechanism
## to explicitly reload the proc table.
##

proc SNMP_ProcInit {s} {
    set interp [$s cget -agent]
    $interp alias DateAndTime DateAndTime
    if {! [catch SNMP_ProcPSAX]} {
	$interp alias SNMP_GetPS SNMP_ProcPSAX
    } elseif {! [catch SNMP_ProcPSE result]} {
	$interp alias SNMP_GetPS SNMP_ProcPSE
    } else {
	$interp alias SNMP_GetPS join ""
    }

    $s instance procReload.0 procReload
    $s bind procReload.0 set {
	foreach id [array names proc] {
	    unset proc($id) 
	}
	catch {unset cmd}
	array set cmd [SNMP_GetPS]
	foreach pid [array names cmd] {
	    %S instance procID.$pid	proc(id,$pid)	$pid
	    %S instance procCmd.$pid	proc(cmd,$pid)	$cmd($pid)
	}
	set procReload [DateAndTime]
    }
}

##
## Two implementations to read the process list for BSD and SYSV machines. 
## SNMP_ProcInit above installs the right one simply by checking which
## of these procs does not fail.
##

proc SNMP_ProcPSAX {} {
    set ps [open "| ps -axc"]
    gets $ps line
    while {![eof $ps]} {
        set pid [lindex $line 0]
        if {[scan $pid "%d" dummy] == 1} {
            regsub {^[^:]*:[0-9][0-9]} $line "" cmd
	    set foo($pid) [string trim $cmd]
        }
        gets $ps line
    }
    close $ps
    return [array get foo]
}

proc SNMP_ProcPSE {} {
    set ps [open "| ps -e"]
    gets $ps line
    while {![eof $ps]} {
        set pid [lindex $line 0]
        if {[scan $pid "%d" dummy] == 1} {
            regsub {^[^:]*:[0-9][0-9]} $line "" cmd
	    set foo($pid) [string trim $cmd]
        }
        gets $ps line
    }
    close $ps
    return [array get foo]
}
