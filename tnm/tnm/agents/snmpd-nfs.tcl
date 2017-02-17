# snmpd-nfs.tcl --
#
#	This file implements the nfsstat objects.
#
# Copyright (c) 1994-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

Tnm::mib load tubs-nfs.mib

##
## Create the scalars in the nfsstat group. A get request to
## the variables nfsstatScalls or nfsstatCcalls will refresh
## all values by calling nfsstat and parsing the output.
##

proc SNMP_NfsGetStats {what} {
    switch $what {
	client { set arg -cn }
	server { set arg -sn }
	default {
	    error "unknown option $what given to SNMP_NfsGetStats"
	}
    }
    set nfsstat /usr/etc/nfsstat
    if {[catch {open "|$nfstat $arg"} f]} {
	return
    }
    gets $f line; gets $f line; gets $f line; gets $f line
    scan $line "%d %d" nfs(calls) nfs(bad)
    gets $f line; gets $f line
    scan $line "%d %*s %d %*s %d %*s %d %*s %d %*s %d %*s %d %*s" \
	    nfs(null) nfs(getattr) nfs(setattr) nfs(root) \
	    nfs(lookup) nfs(readlink) nfs(read)
    gets $f line; gets $f line
    scan $line "%d %*s %d %*s %d %*s %d %*s %d %*s %d %*s %d %*s" \
	    nfs(wrcache) nfs(write) nfs(create) \
	    nfs(remove) nfs(rename) nfs(link) nfs(symlink)
    gets $f line; gets $f line
    scan $line "%d %*s %d %*s %d %*s %d %*s" \
	    nfs(mkdir) nfs(rmdir) nfs(readdir) nfs(fsstat)
    close $f
    return [array get nfs]
}

##
## Create all the variables contained in the nfs client and 
## server groups.
##

proc SNMP_NfsInit {s} {
    set interp [$s cget -agent]
    $interp alias SNMP_NfsGetStats SNMP_NfsGetStats
    foreach n [Tnm::mib children nfsServer] {
	$s instance nfsServer.$n.0 nfsServerStats($n) 0
    }
    foreach n [Tnm::mib children nfsClient] {
	$s instance nfsClient.$n.0 nfsClientStats($n) 0
    }
    $s bind nfsServerCalls get {
	array set nfsServerStats [SNMP_NfsGetStats server]
    }
    $s bind nfsClientCalls get {
	array set nfsClientStats [SNMP_NfsGetStats client]
    }
}
