# TnmMib.tcl --
#
#	This file defines a package of MIB utility procedures used
#	to build applications like MIB browsers.
#
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: TnmMib.tcl,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $

package require Tnm 3.1
package provide TnmMib 3.1.3

namespace eval TnmMib {
    namespace export DescribeType DescribeNode GetVendor
}

# TnmMib::DescribeNode --
#
#	This procedure returns a human readable string which
#	contains information about a MIB node.
#
# Arguments:
#	node	The name or object identifier of a MIB node.
# Results:
#       A string describing the MIB node. An error is generated
#	if the node parameter does not resolve to a MIB node.

proc TnmMib::DescribeNode {node} {
    append txt [format "%-8s%-22s%-12s%s\n" \
	    Module: [Tnm::mib module $node] \
	    Label: [Tnm::mib label $node]]
    append txt [format "%-8s%-22s%-12s%s\n" \
	    Macro: [Tnm::mib macro $node] \
	    Identifier: [Tnm::mib oid $node]]
    append txt [format "%-8s%-22s%-12s%s\n" \
	    Access: [Tnm::mib access $node] \
	    Type: [Tnm::mib type $node]]
    append txt [format "%-8s%-22s%-12s%s\n" \
	    Status: [Tnm::mib status $node] \
	    File: [file tail [Tnm::mib file $node]]]

    if {[Tnm::mib description $node descr]} {
	append txt "\n$descr\n"
    }

    return $txt
}

# TnmMib::DescribeType --
#
#	This procedure returns a human readable string which
#	contains information about a MIB type.
#
# Arguments:
#	type	The name of a MIB type.
# Results:
#       A string describing the MIB type. An error is generated
#	if the type parameter does not resolve to a MIB type.

proc TnmMib::DescribeType {type} {
    append txt [format "%-8s%-22s%-12s%s\n" \
	    Module: [Tnm::mib module $type] \
	    Label: [Tnm::mib label $type]]
    append txt [format "%-8s%-22s%-12s%s\n" \
            Macro: [Tnm::mib macro $type] \
            Base: [Tnm::mib syntax $type]]
    append txt [format "%-8s%-22s%-12s%s\n" \
            Status: [Tnm::mib status $type] \
            File: [file tail [Tnm::mib file $type]]]

    if {[Tnm::mib displayhint $type displayhint]} {
	append txt [format "%-8s%s\n" Format: $displayhint]
    }

    if {[Tnm::mib enums $type enums]} {
	set prefix "Enums: "
	if {[Tnm::mib syntax $type] == "OCTET STRING"} {
	    set prefix "Bits:  "
	}
	set line $prefix
	foreach {label number} $enums {
	    set enum [format " %s(%s)" $label $number]
	    if {[string length $line] + [string length $enum] < 56} {
		append line $enum
	    } else {
		append txt $line
		append txt "\n"
		set line "$prefix$enum"
	    }
	}
	if [string length $line] {
	    append txt $line
	    append txt "\n"
	}
    }

    if {[Tnm::mib description $type descr]} {
        append txt "\n$descr\n"
    }

    return $txt
}


# TnmMib::GetVendor --
#
#	This procedure returns the vendor name for a vendor
#	specific object identifier. The list of known vendor
#	names is currently quite short. Send additions to the
#	maintainer of the scotty distribution.
#
# Arguments:
#	oid	The vendor specific object identifier.
# Results:
#       A string with the name of the vendor or an empty result.

proc TnmMib::GetVendor {oid} {

    array set vendors {
	9	{CISCO}
	11	{HP}
	18	{Wellfleet}
	42	{Sun Microsystems}
	45	{SynOptics}
	72	{Retix}
	82	{Network Computing Devices (NCD)}
	311	{Microsoft}
	326	{Fore Systems, Inc.}
	480	{QMS, Inc.}
	1575	{TU Braunschweig, Germany}
    }

    foreach n [array names vendors] {
	if [Tnm::mib subtree 1.3.6.1.4.1.$n $oid] {
	    return $vendors($n)
	}
    }

    return
}
