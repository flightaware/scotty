# TnmScriptMib.tcl --
#
#	This file implements a set of procedures to interact with
#	a remote scripting engine via the IETF Script MIB.
#
# Copyright (c) 1998-2000 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: TnmScriptMib.tcl,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $

package require Tnm 3.0
package require TnmSnmp 3.0
package provide TnmScriptMib 3.0.2

namespace eval TnmScriptMib {
    namespace export GetLanguages GetExtensions

    namespace export InstallScript DeleteScript
    namespace export SetScriptStorage SetScriptDescription SetScriptSource
    namespace export SetScriptLanguage EnableScript DisableScript

    namespace export InstallLaunch DeleteLaunch EnableLaunch DisableLaunch

    namespace export SuspendRun ResumeRun AbortRun DeleteRun
}

proc TnmScriptMib::GetLanguages {s} {

    set result {}
    lappend vbl [list smLangLanguage]
    $s walk vbl $vbl {
	set lang [Tnm::mib unpack [Tnm::snmp oid $vbl 0]]
	set langID [Tnm::snmp value $vbl 0]
	lappend result [list $lang $langID]
    }

    return $result
}

proc TnmScriptMib::GetExtensions {s language} {

    set result {}
    lappend vbl [list smExtsnExtension.$language]
    $s walk vbl $vbl {
	set extsn [lindex [Tnm::mib unpack [Tnm::snmp oid $vbl 0]] 1]
	set extsnID [Tnm::snmp value $vbl 0]
	lappend result [list $extsn $extsnID]
    }

    return $result
}

proc TnmScriptMib::InstallScript {s owner name language source descr} {

    lappend vbl [list [Tnm::mib pack smScriptRowStatus $owner $name] createAndGo]
    lappend vbl [list [Tnm::mib pack smScriptLanguage $owner $name] $language]
    lappend vbl [list [Tnm::mib pack smScriptSource $owner $name] $source]
    lappend vbl [list [Tnm::mib pack smScriptDescr $owner $name] $descr]

    if {[catch {$s set $vbl} vbl]} {
	error "unable to create row: $vbl"
    }

    return
}

proc TnmScriptMib::DeleteScript {s owner name} {
    
    TnmScriptMib::UnloadScript $s $owner $name

    lappend vbl [list [Tnm::mib pack smScriptRowStatus $owner $name] destroy]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to destroy row: $vbl"
    }

    return
}

proc TnmScriptMib::EnableScript {s owner name {timeout 60}} {

    lappend vbl [list [Tnm::mib pack smScriptAdminStatus $owner $name] enabled]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to enable script: $vbl"
    }

    set transient {disabled retrieving compiling}
    set status "disabled"
    for {set n $timeout} {$n >= 0} {incr n -1} {
        if {[catch {$s get [Tnm::mib pack smScriptOperStatus $owner $name]} vbl]} {
            error "unable to obtain script status: $vbl"
        }
        set status [Tnm::snmp value $vbl 0]
	if {[lsearch $transient $status] < 0} break
        after 1000
    }
    if {$status != "enabled"} {
        error "unable to enable script: $vbl"
    }

    return
}

proc TnmScriptMib::DisableScript {s owner name {timeout 60}} {

    lappend vbl [list [Tnm::mib pack smScriptAdminStatus $owner $name] disabled]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to enable script: $vbl"
    }

    set status "enabled"
    for {set n $timeout} {$n >= 0} {incr n -1} {
        if {[catch {$s get [Tnm::mib pack smScriptOperStatus $owner $name]} vbl]} {
            error "unable to obtain script status: $vbl"
        }
        set status [Tnm::snmp value $vbl 0]
	if {$status == "disabled"} break
        after 1000
    }
    if {$status != "disabled"} {
        error "unable to disable script: $vbl"
    }

    return
}

proc TnmScriptMib::SetScriptStorage {s owner name storage} {

    lappend vbl [list [Tnm::mib pack smScriptStorageType $owner $name] $storage]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to set storage: $vbl"
    }

    return
}

proc TnmScriptMib::SetScriptDescription {s owner name description} {

    lappend vbl [list [Tnm::mib pack smScriptDescr $owner $name] $description]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to set description: $vbl"
    }

    return
}

proc TnmScriptMib::SetScriptSource {s owner name source} {
    TnmScriptMib::UnloadScript $s $owner $name

    lappend vbl [list [Tnm::mib pack smScriptSource $owner $name] $source]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to set script source: $vbl"
    }

    return
}

proc TnmScriptMib::SetScriptLanguage {s owner name language} {
    TnmScriptMib::UnloadScript $s $owner $name

    lappend vbl [list [Tnm::mib pack smScriptLanguage $owner $name] $language]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to set script source: $vbl"
    }

    return
}

proc TnmScriptMib::InstallLaunch {s owner name sowner sname argument lifetime expiretime maxrunning maxcompleted} {

    lappend vbl [list [Tnm::mib pack smLaunchRowStatus $owner $name] createAndGo]
    lappend vbl [list [Tnm::mib pack smLaunchScriptOwner $owner $name] $sowner]
    lappend vbl [list [Tnm::mib pack smLaunchScriptName $owner $name] $sname]
    lappend vbl [list [Tnm::mib pack smLaunchArgument $owner $name] $argument]
    lappend vbl [list [Tnm::mib pack smLaunchLifeTime $owner $name] $lifetime]
    lappend vbl [list [Tnm::mib pack smLaunchExpireTime $owner $name] $expiretime]
    lappend vbl [list [Tnm::mib pack smLaunchMaxRunning $owner $name] $maxrunning]
    lappend vbl [list [Tnm::mib pack smLaunchMaxCompleted $owner $name] $maxcompleted]

    if {[catch {$s set $vbl} vbl]} {
	error "unable to create row: $vbl"
    }

    return
}

proc TnmScriptMib::DeleteLaunch {s owner name} {
    
    lappend vbl [list [Tnm::mib pack smLaunchRowStatus $owner $name] destroy]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to destroy row: $vbl"
    }

    return
}

proc TnmScriptMib::EnableLaunch {s owner name {timeout 60}} {

    lappend vbl [list [Tnm::mib pack smLaunchAdminStatus $owner $name] enabled]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to enable launch: $vbl"
    }

    set transient {disabled}
    set status "disabled"
    for {set n $timeout} {$n >= 0} {incr n -1} {
        if {[catch {$s get [Tnm::mib pack smLaunchOperStatus $owner $name]} vbl]} {
            error "unable to obtain launch status: $vbl"
        }
        set status [Tnm::snmp value $vbl 0]
	if {[lsearch $transient $status] < 0} break
        after 1000
    }
    if {$status != "enabled"} {
        error "unable to enable launch: $vbl"
    }

    return
}

proc TnmScriptMib::DisableLaunch {s owner name {timeout 60}} {

    lappend vbl [list [Tnm::mib pack smLaunchAdminStatus $owner $name] disabled]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to enable launch: $vbl"
    }

    set status "enabled"
    for {set n $timeout} {$n >= 0} {incr n -1} {
        if {[catch {$s get [Tnm::mib pack smLaunchOperStatus $owner $name]} vbl]} {
            error "unable to obtain launch status: $vbl"
        }
        set status [Tnm::snmp value $vbl 0]
	if {$status == "disabled"} break
        after 1000
    }
    if {$status != "disabled"} {
        error "unable to disable launch: $vbl"
    }

    return
}

proc TnmScriptMib::SuspendRun {s owner name index {timeout 60}} {

    lappend vbl [list [Tnm::mib pack smRunControl $owner $name $index] suspend]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to suspend running script: $vbl"
    }

    set transient {suspending executing}
    set status "suspending"
    for {set n $timeout} {$n >= 0} {incr n -1} {
        if {[catch {$s get [Tnm::mib pack smRunState $owner $name $index]} vbl]} {
            error "unable to obtain running script status: $vbl"
        }
        set status [Tnm::snmp value $vbl 0]
	if {[lsearch $transient $status] < 0} break
        after 1000
    }
    if {$status != "suspended"} {
        error "unable to suspend running script: $vbl"
    }

    return
}

proc TnmScriptMib::ResumeRun {s owner name index {timeout 60}} {

    lappend vbl [list [Tnm::mib pack smRunControl $owner $name $index] resume]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to resume running script: $vbl"
    }

    set transient {resuming suspended}
    set status "resuming"
    for {set n $timeout} {$n >= 0} {incr n -1} {
        if {[catch {$s get [Tnm::mib pack smRunState $owner $name $index]} vbl]} {
            error "unable to obtain running script status: $vbl"
        }
        set status [Tnm::snmp value $vbl 0]
	if {[lsearch $transient $status] < 0} break
        after 1000
    }
    if {$status != "executing"} {
        error "unable to resume running script: $vbl"
    }

    return
}

proc TnmScriptMib::AbortRun {s owner name index {timeout 60}} {

    lappend vbl [list [Tnm::mib pack smRunControl $owner $name $index] abort]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to abort running script: $vbl"
    }

    set transient {aborting executing suspended suspending resuming initializing}
    set status "aborting"
    for {set n $timeout} {$n >= 0} {incr n -1} {
        if {[catch {$s get [Tnm::mib pack smRunState $owner $name $index]} vbl]} {
            error "unable to obtain running script status: $vbl"
        }
        set status [Tnm::snmp value $vbl 0]
	if {[lsearch $transient $status] < 0} break
        after 1000
    }
    if {$status != "terminated"} {
        error "unable to abort running script: $vbl"
    }

    return
}

proc TnmScriptMib::DeleteRun {s owner name index} {
    
    lappend vbl [list [Tnm::mib pack smRunExpireTime $owner $name $index] 0]
    if {[catch {$s set $vbl} vbl]} {
        error "unable to expire row: $vbl"
    }

    return
}

