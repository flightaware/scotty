# snmpd-tnm.tcl --
#
#	This file implements the variables contained in the tnm
#	group of the tubs.ibr MIB subtree.
#
# Copyright (c) 1994-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

Tnm::mib load tubs-tnm.mib

##
## Return the actual date and time in the textual convention format.
##

proc DateAndTime {} {
    clock format [clock seconds] -format "%Y-%m-%d-%T"
}

##
## Create the scalars in the scotty group.
##

proc SNMP_TnmInit {s} {
    global tnm
    $s instance tnmDate.0 tnmDate [DateAndTime]
    $s instance tnmTrapDst.0 tnmTrapDst localhost
    $s instance tnmTrapMsg.0 tnmTrapMsg ""
    $s instance tnmVersion.0 tnmVersion [set tnm(version)]
    $s instance tnmTclVersion.0 tnmTclVersion [info tclversion]
    $s instance tnmTclCmdCount.0 tnmTclCmdCount
    $s bind tnmTclCmdCount.0 get { 
	set tnmTclCmdCount [info cmdcount] 
    }
}

##
## Create the http group variables.
##

## http://www.cs.tu-bs.de/~schoenw/hello.tcl

proc SNMP_HttpSource {interp url} {
    global tnm
    set file $tnm(tmp)/http.[pid]
    catch { exec rm -f $file }
    if {[catch { http get $url $file } msg]} {
	return $msg
    }
    if {[catch {read [open $file r]} content]} {
        return $content
    }
    catch { close $f }
    if {[catch { $interp eval $content } msg]} {
	return $msg
    }
    catch { exec rm -f $file }
    return ""
}

proc SNMP_HttpInit {s} {

    set interp [$s cget -agent]
    $interp alias SNMP_HttpSource SNMP_HttpSource $interp
    $interp alias SNMP_HttpProxy http proxy

    $s instance tnmHttpProxy.0 tnmHttpProxy
    $s instance tnmHttpSource.0 tnmHttpSource
    $s instance tnmHttpError.0 tnmHttpError
    
    $s bind tnmHttpProxy.0 set {
	catch {SNMP_HttpProxy "%v"}
	set tnmHttpProxy [SNMP_HttpProxy]
    }

    $s bind tnmHttpSource.0 set {
	set tnmHttpSource "%v"
	set msg [SNMP_HttpSource $tnmHttpSource]
	if {$msg != ""} {
	    set tnmHttpError $msg
	    error inconsistentValue
	}
    }

    $s bind tnmHttpSource.0 rollback {
	set tnmHttpSource ""
    }
    $s bind tnmHttpSource.0 commit {
	set tnmHttpError ""
    }
}

##
## The tnmEval group contains a table of variables that can store
## tcl code for evaluation. This is only useful if you use security
## mechanisms in the protocol exchange.
##

proc SMMP_EvalInit {s} {

    $s instance tnmEvalSlot.0 tnmEvalSlot 0
    $s bind tnmEvalSlot.0 get { 
	incr tnmEvalSlot
    }
    
    $s bind tnmEvalValue get {
	global tnmEvalStatus tnmEvalString
	set idx "%i"
	if {$tnmEvalStatus($idx) == "active"} {
	    if [catch {eval $tnmEvalString($idx)} result] {
		error "tnmEvaluation of $tnmEvalString($idx) failed"
	    }
	    set tnmEvalValue($idx) $result
	}
    }
    
    $s bind tnmEvalStatus create {
	set idx "%i"
	switch "%v" {
	    createAndGo {
		%S instance tnmEvalIndex.$idx  tnmEvalIndex($idx) $idx
		%S instance tnmEvalString.$idx tnmEvalString($idx)
		%S instance tnmEvalValue.$idx  tnmEvalValue($idx)
		%S instance tnmEvalStatus.$idx tnmEvalStatus($idx) active
	    }
	    createAndWait {
		%S instance tnmEvalIndex.$idx  tnmEvalIndex($idx) $idx
		%S instance tnmEvalString.$idx tnmEvalString($idx)
		%S instance tnmEvalValue.$idx  tnmEvalValue($idx)
		%S instance tnmEvalStatus.$idx tnmEvalStatus($idx) notReady
	    }
	    destroy {
	    }
	    default {
		error inconsistentValue
	    }
	}
    }

    $s bind tnmEvalStatus set {
	set idx "%i"
	switch "%v" {
	    active {
	    }
	    notInService {
	    }
	    notReady {
	    }
	    createAndGo {
		error inconsistentValue
	    }
	    createAndWait {
		error inconsistentValue
	    }
	    destroy {
		after idle "unset tnmEvalIndex($idx)"
		after idle "unset tnmEvalString($idx)"
		after idle "unset tnmEvalValue($idx)"
		after idle "unset tnmEvalStatus($idx)"
	    }
	}
    }
}

