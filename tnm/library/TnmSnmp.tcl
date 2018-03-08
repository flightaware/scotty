# TnmSnmp.tcl --
#
#	This file implements useful utilities for programming SNMP
#	based applications in Tcl. This code is extracted from
#	applications written for the tkined(1) editor.
#
# Copyright (c) 1994-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1997-1998 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require Tnm 3.0
package require TnmDialog 3.0
package require TnmInet 3.0
package provide TnmSnmp 3.0.4

namespace eval TnmSnmp {
    namespace export Walk
}

# TnmSnmpGetIp --
#
#	Return the IP destination address of a SNMP session. We also
#	define the writeln proc to a reasonable default if it is not
#	defined yet.
#
# Arguments:
#	s	The SNMP session handle.
# Results:
#	The IP address for this SNMP session.

proc TnmSnmpGetIp {s} {
    if {[info proc writeln] == ""} {
	proc writeln args { puts [join $args] }
    }
    return [$s cget -address]
}

# TnmSnmpGetHeader --
#
#	Return the header which is printed above any SNMP output.
#
# Arguments:
#	s	The SNMP session handle.
# Results:
#	The hearder.

proc TnmSnmpGetHeader {s} {
    if {[info proc writeln] == ""} {
	proc writeln args { puts [join $args] }
    }
    set ip [$s cget -address]
    set port [$s cget -port]
    set txt ""
    if {! [catch {TnmInet::GetIpName $ip} host]} {
	set txt "[lindex $host 0] "
    }
    append txt "\[$ip:$port\] \[[clock format [clock seconds]]\]:\n"
    return $txt
}

# TnmSnmpPrintOid --
#
#	Return the OID of a variable in a human readable format.
#
# Arguments:
#	oid	The OID value that should be printed.
# Results:
#	The pretty printed representation of the OID.

proc TnmSnmpPrintOid {oid} {
    set status [catch {Tnm::mib unpack $oid} unpacked]
    if {$status} {
	return [Tnm::mib name $oid]
    }
    set result [Tnm::mib label $oid]
    foreach v $unpacked {
	append result "\[$v\]"
    }
    return $result
}

# TnmSnmp::Walk --
#
#	Walk through a MIB tree and format the results into a human
#	readable format.
#
# Arguments:
#	s	The SNMP session handle to use.
#	subtree	The object identifier used to identify the MIB subtree.
# Results:
#	The walk result contained in a multi-line text.

proc TnmSnmp::Walk {s subtree} {

    set txt [TnmSnmpGetHeader $s]

    set maxl 0
    if {[catch {
	set n 0
	$s walk vbl $subtree {
	    foreach vb $vbl {
		set name($n) [TnmSnmpPrintOid [lindex $vb 0]]
		set value($n) [lindex $vb 2]
		if {$maxl < [string length $name($n)]} {
		    set maxl [string length $name($n)]
		}
		incr n
	    }
	}
    } msg]} {
	error $msg
    }

    for {set i 0} {$i < $n} {incr i} {
	append txt [format "  %-*s : %s\n" $maxl $name($i) $value($i)]
    }

    return $txt
}


# TnmSnmp::Scalars --
#
#	Retrieve all scalar variables of a MIB group and store the
#	values in a Tcl array.
#
# Arguments:
#	s	The SNMP session to use.
#	path	The object identifier of the MIB group.
#	name	The name of an array where we store the values.
# Results:
#	The ordered list of scalars.

proc TnmSnmp::Scalars {s group name} {

    upvar $name value
    catch {unset value}

    # Make sure value is an array and the group is a valid object
    # identifier and not an instance or a table. This is a hack.

    set value(foo) bar
    unset value(foo)
    if {[Tnm::mib syntax $group] != ""} return

    set exception(noSuchObject)   1
    set exception(noSuchInstance) 1
    set exception(endOfMibView)   1

    # Build a list of all variables in this group that do not have any
    # sucessors (to avoid embedded tables).

    set varlist ""
    foreach var [Tnm::mib children [Tnm::mib oid $group]] {
	if {[Tnm::mib access $var] != "not-accessible"} {
	    lappend varlist "$var.0"
	}
    }

#    # Try to retrieve the varlist in one get request. If this fails,
#    # we use a get request to retrieve variables individually.
#    # Note, I would like to honor noSuchName or tooBig errors, but
#    # unfortunately some agents do not send this error code.
#
#    if {[catch {$s get $varlist} vbl]} {
#	set vbl ""
#	foreach var $varlist {
#	    if {[catch {$s get $var} v]} continue
#	    lappend vbl [lindex $v 0]
#	}
#    }

    # Try to retrieve the varlist in one get request. If this fails
    # due to a noSuchName error, we remove the varlist element and
    # try again. If it fails for some other reason (e.g. a tooBig
    # error), we use a sequence of get requests to retrieve variables
    # individually. If we get a noResponse error at this time, we
    # will break out of the loop.

    while {[llength $varlist]} {
	if {[catch {$s get $varlist} vbl]} {
	    if {[scan $vbl "noSuchName %d" index] == 1} {
		set varlist [lreplace $varlist $index $index]
		continue
	    }
	    set vbl ""
	    foreach var $varlist {
		if {[catch {$s get $var} v]} {
		    if {[string match noResponse* $v]} {
			break
		    }
		    continue
		}
		lappend vbl [lindex $v 0]
	    }
	    break
	} else {
	    break
	}
    }


#    # This commented version below does it asynchonously. Fast on 
#    # fast reliable networks but problematic if the net has problems.
#
#    if {[catch {$s get $varlist} vbl]} {
#	global _vbl_
#	foreach var $varlist {
#	    $s get $var {
#		if {"%E" == "noError"} {
#		    lappend _vbl_ [lindex "%V" 0]
#		}
#	    }
#	}
#	$s wait
#	set vbl $_vbl_
#	unset _vbl_
#    }

    set result ""
    foreach vb $vbl {
	set oid [lindex $vb 0]
	set syn [lindex $vb 1]
	set val [lindex $vb 2]
	if {[info exists exception($syn)]} continue
	set name [Tnm::mib name $oid]
	set value($name) $val
	lappend result $name
    }
    return $result
}


# TnmSnmp::ShowScalars --
#
#	Show all scalar variables of a MIB group.
#
# Arguments:
#	s	The SNMP session to use.
#	group	The object identifier of the MIB group.
# Results:
#	The scalars represented in human readable multi-line text.

proc TnmSnmp::ShowScalars {s group} {

    set txt [TnmSnmpGetHeader $s]

    if {[catch {TnmSnmp::Scalars $s $group values} msg]} {
	append txt "$msg\n"
	return $txt
    }

#    if {[catch {$s scalars $group values} msg]} {
#	append txt "$msg\n"
#	return $txt
#    }

    set maxl 0
    foreach name $msg {
	set printed($name) [TnmSnmpPrintOid $name]
	set length [string length $printed($name)]
	if {$maxl < $length} {
	    set maxl $length
	}
    }

    foreach name $msg {
	append txt [format "  %-*s : %s\n" \
		$maxl $printed($name) $values($name)]
    }
    return $txt
}


# TnmSnmp::Table --
#
#	Retrieve a table from an SNMP agent and store the values
#	in a Tcl array.
#
# Arguments:
#	s	The SNMP session to use.
#	table	The object identifier of an snmp table.
#	name	The name of an array where we store the values.
#
# Results:
#	The ordered list of instance identifiers.

proc TnmSnmp::Table {s table name} {

    upvar $name value
    catch {unset value}

    # Make sure value is an array and table is a valid object
    # identifier and not an instance or a table. This is a hack.

    set value(foo) bar
    unset value(foo)

    set table [Tnm::mib oid $table]
    if {[Tnm::mib syntax $table] == "SEQUENCE"} {
	set table [Tnm::mib parent $table]
    }
    if {[Tnm::mib syntax $table] != "SEQUENCE OF"} return

    set exception(noSuchObject)   1
    set exception(noSuchInstance) 1
    set exception(endOfMibView)   1

    # Get all the columns in the table that are accessible. Exclude
    # those variables that are part of the indexing structure of the
    # table.

    set cols ""
    set index [Tnm::mib index $table]
    foreach name $index {
	set idx([Tnm::mib oid $name]) $name
    }
    foreach var [Tnm::mib children [Tnm::mib children $table]] {
	if [info exists idx($var)] {
	    continue
	}
	if {[Tnm::mib access $var] != "not-accessible"} {
	    lappend cols $var
	}
    }
    if {[llength $cols] == 0} {
	set cols $var
    }

    # Check if we can walk the columns simultaneously. We start a walk
    # and if we get a result, we will be happy. Otherwise, we will
    # start a very silly walk to collect the table element by element.

    foreach attempt [list $cols $table] {
	catch {unset value}
	set status 1

	# Now walk through the table and fill the array named value,
	# indexed by row name : instance identifier, e.g.
	#	value(ifType:1) "le0"
	#	value(ifType:2) "le1"
	# The list called order maintains the order of the retrieved
	# rows.

	set order ""
	if [catch {
	    $s walk vbl $attempt {
		set oid [Tnm::snmp oid $vbl 0]
		set inst [lindex [Tnm::mib split $oid] 1]
		if {[lsearch $order $inst] < 0} {
		    lappend order $inst
		}
		
		foreach i $index v [Tnm::mib unpack $oid] {
		    set value([Tnm::mib label $i]:$inst) $v
		}
		
		foreach vb $vbl {
		    set oid  [lindex $vb 0]
		    set syn  [lindex $vb 1]
		    set val  [lindex $vb 2]
		    if {[info exists exception($syn)]} continue
		    set value([Tnm::mib label $oid]:$inst) $val
		}
	    }
	} msg] {
	    set status 0
	    if {[string match tooBig* $msg]} {
		continue
	    }
	}
	if {! [info exists value]} {
	    continue
	}
	break
    }

    if {! $status} {
	error $msg
    }

    return $order
}


# TnmSnmp::ShowTable --
#
#	Show a complete MIB table. First determine which rows are supported
#	by an agent and then walk through the table. The formating code is
#	ugly and should be replaced by a better version.
#
# Arguments:
#	s	The SNMP session to use.
#	table	The object identifier of an snmp table.
#
# Results:
#	The table represented in human readable multi-line text.

proc TnmSnmp::ShowTable {s table} {

    set txt [TnmSnmpGetHeader $s]

    if {[catch {TnmSnmp::Table $s $table values} msg]} {
	append txt "$msg\n"
	return $txt
    }

    set yorder $msg
    if {[llength $yorder] == 0} {
	return $txt
    }

    # Get the xorder

    set columnList [Tnm::mib index $table]
    Tnm::mib walk x $table {
	if {[lsearch $columnList $x] < 0} {
	    lappend columnList $x
	}
    }

    foreach x $columnList {
	set label [Tnm::mib label $x]
	if [llength [array names values $label*]] {
	    lappend xorder $label
	    switch [Tnm::mib syntax $x] {
		{OBJECT IDENTIFIER} -
		{OCTET STRING} {
		    set xjustify($label) -
		}
		default {
		    set xjustify($label) {}
		}
	    }
	}
    }
    
    # Calculate the max. length of all values for every row.

    foreach pfx $xorder {
	set xindex($pfx) [string length $pfx]
	foreach n [array names values $pfx:*] {
	    set len [string length $values($n)]
	    if {$len > $xindex($pfx)} {
		set xindex($pfx) $len
	    }
	}
    }

    # Try to display the table as a table. This is still no good solution... 

    while {[array names xindex] != ""} {
	set foo ""
	set total 0
	set fmt ""
	set line ""
	foreach pfx $xorder {
	    incr total $xindex($pfx)
	    incr total 2
	    set fmt "  %$xjustify($pfx)$xindex($pfx)s"
	    append line [format $fmt $pfx]
	    lappend foo $pfx
	}
	append txt "$line\n"
	foreach idx $yorder {
	    set line ""
	    foreach pfx $foo {
		set fmt "  %$xjustify($pfx)$xindex($pfx)s"
		# note, some tables may have holes
		if [catch {format $fmt $values($pfx:$idx)} xxx] {
		    set xxx [format $fmt "?"]
		}
		append line $xxx
	    }
	    append txt "$line\n"
	}
	foreach pfx $foo {
	    unset xindex($pfx) 
	}
    }

    return $txt
}


# Tnm_SnmpSetValue --
#
# Set a value of a MIB variable. A check is made if the MIB variable
# is an enumeration. If yes
#
# Arguments:
# s -		The SNMP session to use.
# oid -		The object identifier of the variable to set.
# w -		The root widget name or empty if running with tkined.

proc Tnm_SnmpSetValue {s oid {w ""}} {
    $s walk x $oid {
	set x [lindex $x 0]
	set value([Tnm::mib name [lindex $x 0]]) [lindex $x 2]
    }
    set name [array names value]
    if {[llength $name] > 1} {
	if {$w != ""} {
	    set res [Tnm_DialogSelect $w.l questhead "Select an instance:" \
		    [lsort [array names value]] "select cancel"]
	} else {
	    set res [ined list "Select an instance:" \
		    [lsort [array names value]] "select cancel"]
	}
	if {[lindex $res 0] != "select"} return
	set name [lindex $res 1]
    }
    if {$name == ""} return

    set enums ""
    catch {
	foreach {label number} [Tnm::mib enums [Tnm::mib type $name]] {
	    lappend enums $label
	}
    }

    if {$w != ""} {
	if {$enums != ""} {
	    set res [Tnm_DialogSelect $w.l questhead \
		    "Select a value (current = $value($name)):" \
		    $enums "ok cancel"]
	} else {
	    set res [Tnm_DialogRequest $w.r questhead \
		    "Enter new value for $name:" $value($name) "ok cancel"]
	}
    } else {
	if {$enums != ""} {
	    set res [ined list "Select a value (current = $value($name)):" \
		    $enums "ok cancel"]
	} else {
	    set res [ined request "Enter new value for $name:" \
		    [list [list $name $value($name) entry 20]] "ok cancel"]
	}
    }
    if {[lindex $res 0] != "ok"} return
    set new [lindex $res 1]
    if [catch {$s set [list [list $name $new]]} msg] {
	error $msg
    }
}


# Tnm_SnmpCreateInstance --
#
# Set a value of a MIB variable.
#
# Arguments:
# s -		The SNMP session to use.
# oid -		The object identifier of the variable to set.
# w -		The root widget name or empty if running with tkined.

proc Tnm_SnmpCreateInstance {s oid {w ""}} {
    if {$w != ""} {
	set res [Tnm_DialogRequest $w.l questhead \
		"Enter an instance identifier:" [Tnm::mib name $oid]. \
		"create cancel"]
    } else {
	set res [ined request "Enter an instance identifier:" \
		[list [list Instance [Tnm::mib name $oid] entry 20]] \
		"create cancel"]
    }
    if {[lindex $res 0] != "create"} return
    set name [lindex $res 1]
    if {$w != ""} {
	set res [Tnm_DialogRequest $w.r questhead \
		"Enter new value for $name:" "" "ok cancel"]
    } else {
	set res [ined request "Enter new value for $name:" \
		[list [list Value "" entry 20]] "ok cancel"]
    }
    if {[lindex $res 0] != "ok"} return
    set new [lindex $res 1]
    if [catch {$s set [list [list $name $new]]} msg] {
	error $msg
    }
}
