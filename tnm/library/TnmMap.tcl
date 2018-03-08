# TnmMap.tcl --
#
#	This file contains utilities to manipulate Tnm maps and 
#	map items. It also includes a set of standard monitoring
#	procedures.
#
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1997      Gaertner Datensysteme.
# Copyright (c) 1997-1998 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: TnmMap.tcl,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $

package require Tnm 3.0
package require TnmInet 3.0
package provide TnmMap 3.0.4

namespace eval TnmMap {
    namespace export GetIpAddress GetIpName GetSnmpSession
}

# TnmMap::GetIpAddress --
#
#	Return an IP address of a Tnm map item.
#	See the user documentation for details on what it does.
#
# Arguments:
#	node	The map item for which we want to get an IP address.
# Results:
#	The IP address of the given node or an error if it is not
#	possible to lookup an IP address.

proc TnmMap::GetIpAddress {node} {
    set ip [lindex [$node cget -address] 0]
    if {! [catch {Tnm::netdb ip class $ip}]} {
	return $ip
    }
    if [string length $ip] {
	if [catch {TnmInet::GetIpAddress $ip} ip] {
	    error $ip
	}
	return $ip
    }
    set name [lindex [$node cget -name] 0]
    if [string length $name] {
	if [catch {TnmInet::GetIpAddress $name} ip] {
            error $ip
        }
	if {[string length [$node cget -address]] == 0} {
	    $node configure -address $ip
	}
        return $ip
    }
    error "failed to lookup IP address for \"$node\""
}

# TnmMap::GetIpName --
#
#	Return an IP name of a Tnm map item.
#	See the user documentation for details on what it does.
#
# Arguments:
#	node	The map item for which we want to get an IP name.
# Results:
#	The IP name of the given node or an error if it is not
#	possible to lookup an IP name.

proc TnmMap::GetIpName {node} {
    set name [lindex [$node cget -name] 0]
    if [string length $name] {
	if [catch {Tnm::netdb ip class $name}] {
	    return $name
	}
	if {! [catch {TnmInet::GetIpName $name} name]} {
	    return $name
	}
    }
    set ip [lindex [$node cget -address] 0]
    if [string length $ip] {
	if [catch {TnmInet::GetIpName $ip} name] {
            error $name
        }
	if {[string length [$node cget -name]] == 0} {
	    $node configure -name $name
	}
        return $name
    }
    error "failed to lookup IP name for \"$node\""
}

# TnmMap::GetSnmpSession --
#
#	Return an SNMP session for a Tnm map item.
#	See the user documentation for details on what it does.
#
# Arguments:
#	node	The map item for which we want to get an SNMP session.
# Results:
#	The SNMP session for the given node or an error if it is not
#	possible to lookup an SNMP session.

proc TnmMap::GetSnmpSession {node} {
    set map [$node map]
    set alias [$node attribute Tnm:Snmp:Alias]
    if [string length $alias] {
	if [catch {snmp generator -alias $alias} s] {
	    error "invalid value \"$alias\" in Tnm:Snmp:Alias for \"$node\""
	}
	return $s
    }
    set config [$node attribute Tnm:Snmp:Config]
    if [string length $config] {
	if [catch {eval Tnm::snmp generator $config} s] {
	    error "invalid value \"$config\" in Tnm:Snmp:Config for \"$node\""
	}
	return $s
    }

    if [catch {TnmMap::GetIpAddress $node} ip] {
	error $ip
    }
    if {[catch {Tnm::snmp generator -address $ip} s] == 0} {
	return $s
    }

    error "failed to create an SNMP session to \"$node\""
}


proc TnmMap::GetMessages {item} {
    set cnt 0
    set txt [format "%-20s %-12s %s" "Date & Time" Tag Text]
    foreach msg [$item info messages] {
	append txt [format "\n%-20s %-12s %s" \
		[clock format [$msg time] -format {%b %d %T %Y}] \
		[$msg tag] [$msg text]]
	incr cnt
    }
    if {$cnt} {
	return $txt
    }
    return
}

proc TnmMap::GetEvents {item} {
    set cnt 0
    set txt [format "%-20s %-12s %s" "Date & Time" Tag Arguments]
    foreach msg [$item info events] {
	append txt [format "\n%-20s %-12s %s" \
		[clock format [$msg time] -format {%b %d %T %Y}] \
		[$msg tag] [$msg args]]
	incr cnt
    }
    if {$cnt} {
	return $txt
    }
    return
}

proc TnmMapNotifyViaEmail {item {interval 1800}} {
    set map [$item map]
    set to [$item attribute :Contact]
    if ![string length $to] {
        set to [$map attribute :Contact]
    }
    if ![string length $to] {
	return
    }

    set now [clock seconds]
    set last [$item attribute notify:mail:last]
    if {[string length $last] && $now - $last < $interval} {
	return
    }
    $item attribute notify:mail:last $now

    set name [$item cget -name]
    if ![string length $name] {
	set name [$item cget -address]
    }
    set subject "\[Tnm\]: $name ([$item health] %)"
    set body "\nMessages: (last [$item cget -expire] seconds)\n\n"
    foreach msg [$item info messages] {
	set time [clock format [$msg time] -format {%T}]
	append body [format "%s (%2d):\t %s\n" \
		$time [$msg health] [$msg text]]
    }
    TnmInet::SendMail $to $body $subject
}

# TnmMap::TraceRoute --
#
#	Trace a route using the van Jacobsen algorithm.
#	See the user documentation for details on what it does.
#
# Arguments:
#	node	The map item for which we want to trace the route.
# Results:
#	None. However, several events are raised while tracing the route.

proc TnmMap::TraceRoute {node {maxlength 32} {retries 3}} {
    set dst [TnmMap::GetIpAddress $node]
    for {set i 0} {$i < $retries} {incr i} { 
	lappend icmparg $dst
    }
    set l ""
    for {set ttl 1} {[lsearch $l $dst] < 0 && $ttl < $maxlength} {incr ttl} {
        set trace [icmp -retries 0 trace $ttl $icmparg]
        set l ""
        set time ""
        foreach {ip rtt} $trace {
            if {[string length $rtt]} {
                if {[lsearch $l $ip] < 0} { lappend l $ip }
                append time [format " %5d ms" $rtt]
            } else {
                append time "   *** ms"
            }
        }
	set names ""
        foreach ip $l {
            if {[catch {netdb hosts name $ip} name]} {
                if {[catch {dns name $ip} name]} {
                    set name $ip
                }
            }
            if {[lsearch $names $name] < 0} { lappend names $name }
        }
	$node raise TnmMap:TraceRoute:Value \
		[format "%2d %-47s %s" $ttl [string range $names 0 46] $time]
    }
    $node raise TnmMap:TraceRoute:Done
    return
}


# Utilities to convert maps into a set of html files.

proc TnmMap::Html {map root} {
    global tnm
    if {! [file isdirectory $root]} {
        file mkdir $root
    }
    set tmp $tnm(tmp)/map[pid]
    set f [open $tmp w]
    puts $f "<HTML>\n<HEAD>"
    puts $f "<TITLE>[$map cget -name]</TITLE>"
    puts $f "<META HTTP-EQUIV=\"refresh\" CONTENT=\"120\">"
    puts $f "</HEAD>\n"
    puts $f "<BODY>"
    puts $f [TnmMap::HtmlMap $map]
    puts $f "</BODY>\n</HTML>"
    close $f
    file rename -force $tmp $root/index.html

    foreach item [$map find] {
	set f [open $tmp w]
	puts $f "<HTML>\n<HEAD>"
	puts $f "<TITLE>[$item cget -name]</TITLE>"
	puts $f "<META HTTP-EQUIV=\"refresh\" CONTENT=\"120\">"
	puts $f "</HEAD>\n"
	puts $f "<BODY>"
	puts $f [TnmMap::HtmlItem $item]
	puts $f "</BODY>\n</HTML>"
	close $f
	file rename -force $tmp $root/$item.html
    }
}

proc TnmMap::HtmlMap {map} {
    global tnm
    set Attributes [TnmMap::HtmlAttributes $map]
    foreach type [Tnm::map info types] {
	set table($type) [TnmMap::HtmlItemSummary $map $type]
    }

    append html "<H1><A HREF=\"$tnm(url)\">"
    append html "<IMG SRC=\"$tnm(url)/tkined.gif\" ALIGN=RIGHT ALT=\"\"></A>"
    append html "Network Map Status Page ([$map cget -name])</H1>\n"
    append html "<TABLE>\n"
    append html "<TR><TD><B>Now:</B>"
    append html "<TD><EM>[clock format [clock seconds]]</EM><BR>\n"
    append html "<TR><TD><B>Start:</B>"
    append html "<TD><EM>[clock format $tnm(start)]</EM>\n"
    append html "</TABLE>\n"
    append html "<HR><B>"
    append html "\[<A HREF=\"#Description\">Description</A> "
    foreach part {Attributes} {
	if {[info exists $part] && [string length [set $part]]} {
	    append html " | <A HREF=\"#$part\">$part</A>"
	}
    }
    foreach type [array names table] {
	if [string length $table($type)] {
	    set part [TnmMap::HtmlCapitalize $type]s
	    append html " | <A HREF=\"#$part\">$part</A>"
	}
    }
    append html "\]</B><HR><P>\n"

    append html "<H2><A NAME=\"Description\">Description:</A></H2>\n"
    append html "This network status page is generated by the\n"
    append html "<A HREF=\"$tnm(url)\">scotty network management\n"
    append html "package</A>.\n"
    append html "It is based on the upcoming 3.0.0 version, which\n"
    append html "includes commands to manage generic network maps.\n"
    append html "A network map allows to generate events. Generic\n"
    append html "monitoring jobs are provided which act as event sources.\n"
    append html "Tcl scripts can be bound to map events in order to filter\n"
    append html "and analyze events.\n<P>\n"
    append html "Network maps maintain health attributes for every map\n"
    append html "object. The health is defined as a value between 0 (bad)\n"
    append html "and 100 (good). Tcl scripts bound to events usually\n"
    append html "modify the health of affected devices, which makes it\n"
    append html "easy to aggregate information.\n<P>\n"
    append html "The package includes a library package which allows to\n"
    append html "convert a map into a set of HTML pages. These HTML pages\n"
    append html "can be updated at regular intervals so that you can\n"
    append html "check the status of your network from remote locations.\n"
    append html "Unhealthy map elements are show in a red color while\n"
    append html "healthy elements are shown in green color.\n"
    append html "A history of events is automatically maintained for each\n"
    append html "map object which allows to pinpoint problems easily.\n"
    foreach part {Attributes} {
	append html [set $part]
    }
    foreach type [array names table] {
	append html [set table($type)]
    }
    return $html
}

proc TnmMap::HtmlItem {item} {
    global tnm
    set Attributes [TnmMap::HtmlAttributes $item]
    set Links [TnmMap::HtmlLinks $item]
    set Messages [TnmMap::HtmlMessages $item]

    set name [$item cget -name]
    if {![string length $name]} {
	set name [$item cget -address]
	if {![string length $name]} {
	    set name $item
	}
    }

    append html "<H1><A HREF=\"$tnm(url)\">"
    append html "<IMG SRC=\"$tnm(url)/tkined.gif\" ALIGN=RIGHT ALT=\"\"></A>"
    append html "[TnmMap::HtmlCapitalize [$item type]] Status Page ($name)</H1>\n"
    append html "<TABLE>\n"
    append html "<TR><TD><B>Now:</B>"
    append html "<TD><EM>[clock format [clock seconds]]</EM><BR>\n"
    append html "<TR><TD><B>Start:</B>"
    append html "<TD><EM>[clock format $tnm(start)]</EM>\n"
    append html "</TABLE>\n"
    append html "<HR><B>"
    append html "\[<A HREF=\"#Description\">Description</A> "
    foreach part {Attributes Links Messages} {
	if {[info exists $part] && [string length [set $part]]} {
	    append html " | <A HREF=\"#$part\">$part</A>"
	}
    }
    append html "\]</B><HR><P>\n"

    append html [TnmMap::HtmlDescription $item]
    foreach part {Attributes Links Messages} {
	append html [set $part]
    }
    return $html
}

proc TnmMap::HtmlItemSummary {map type} {
    foreach item [$map find -type $type -sort health -order increasing] {
	set health [lindex [$item health] 0]
	append table "<TR BGCOLOR=\"[TnmMap::HtmlColor $health]\">\n"
	catch {
	    append table \
	    "<TD> <A HREF=\"$item.html\">[TnmMap::HtmlQuote [$item cget -name]]</A>\n"
	}
	catch {
	    append table \
	    "<TD> <A HREF=\"$item.html\">[TnmMap::HtmlQuote [$item cget -address]]</A>\n"
	}
	catch {
	    append table \
	    "<TD> [TnmMap::HtmlQuote [$item cget -tags]]\n"
	}
	catch {
	    append table "<TD ALIGN=RIGHT> [TnmMap::HtmlQuote $health]\n"
	}
    }
    if {![info exists table]} return

    set name [TnmMap::HtmlCapitalize $type]s
    append html "<H2><A NAME=\"$name\">$name:</A></H2>\n"
    append html "<TABLE BORDER>\n"
    append html "<TR><TH>Name<TH>Address<TH>Tags<TH>Health\n"
    append html $table
    append html "</TABLE>\n"
    return $html
}

proc TnmMap::HtmlDescription {item} {
    catch {
	set health [lindex [$item health] 0]
	set color [TnmMap::HtmlColor $health]
    }
    append html "<H2><A NAME=\"Description\">Description:</A></H2>\n"
    if [info exists color] {
	append html "<TABLE BORDER BGCOLOR=\"$color\">\n"
    } else {
	append html "<TABLE BORDER>\n"
    }
    catch {
	append html "<TR><TD><B>Id:</B>"
	append html "<TD>[TnmMap::HtmlQuote $item]\n"
    }
    catch {
	append html "<TR><TD><B>Name:</B>"
	append html "<TD>[TnmMap::HtmlQuote [$item cget -name]]\n"
    }
    catch {
	append html "<TR><TD><B>Address:</B>"
        append html "<TD>[TnmMap::HtmlQuote [$item cget -address]]\n"
    }
    catch {
	append html "<TR><TD><B>Tags:</B>"
        append html "<TD>[TnmMap::HtmlQuote [$item cget -tags]]\n"
    }
    catch {
	append html "<TR><TD><B>Source:</B><TD>[$item cget -src]\n"
    }
    catch {
	append html "<TR><TD><B>Destination:</B><TD>[$item cget -dst]\n"
    }
    catch {
	append html "<TR><TD NOWRAP><B>Health:</B><TD>[$item health]\n"
    }
    catch {
	append html "<TR><TD><B>Priority:</B><TD>[$item cget -priority]\n"
    }
    catch {
	append html "<TR><TD><B>Expire:</B><TD>[$item cget -expire] seconds\n"
    }
    catch {
	append html "<TR><TD><B>Created:</B>"
        append html "<TD>[clock format [$item cget -ctime]]\n"
    }
    catch {
	append html "<TR><TD><B>Modified:</B>"
        append html "<TD>[clock format [$item cget -mtime]]\n"
    }
    append html "</TABLE>\n"
    return $html
}

proc TnmMap::HtmlLinks {item} {
    if [catch {$item info links} links] return
    foreach link $links {
	set dst [$link cget -src]
	if {[string compare $dst $item] == 0} {
	    set dst [$link cget -dst]
	}
	append table "<TR>"
	append table "<TD><A HREF=\"$link.html\">$link</A>"
	append table \
		"<TD><A HREF=\"$dst.html\">[TnmMap::HtmlQuote [$dst cget -name]]</A>"
	append table \
		"<TD><A HREF=\"$dst.html\">[TnmMap::HtmlQuote [$dst cget -address]]</A>\n"
    }
    if {![info exists table]} return

    append html "<H2><A NAME=\"Links\">Links:</A></H2>\n"
    append html "<TABLE BORDER>\n"
    append html "<TR><TH>Id<TH>Destination Name<TH>Destination Address\n"
    append html $table
    append html "</TABLE>\n"
    return $html
}

proc TnmMap::HtmlAttributes {item} {
    foreach name [lsort [$item attribute]] {
	if {! [regexp {(^:)|(^Tnm:)} $name]} continue
	append table "<TR>"
	append table "<TD NOWRAP><B>[TnmMap::HtmlQuote [string trim $name :]]</B>"
	append table "<TD NOWRAP>[TnmMap::HtmlQuote [$item attribute $name]]\n"
    }
    if {![info exists table]} return

    append html "<H2><A NAME=\"Attributes\">Attributes:</A></H2>\n"
    append html "<TABLE BORDER>\n"
    append html "<TR><TH>Attribute<TH>Value\n"
    append html $table
    append html "</TABLE>\n"
    return $html
}

proc TnmMap::HtmlEvents {item} {
 
    foreach event [$item info events] {
	set secs [$event time]
	append table "<TR>"
	append table "<TD NOWRAP> [clock format $secs -format {%d %B %Y}]\n"
	append table "<TD NOWRAP> [clock format $secs -format {%T}]\n"
	append table "<TD NOWRAP> [TnmMap::HtmlQuote [$event tag]]\n"
	append table "<TD NOWRAP> [TnmMap::HtmlQuote [$event args]]\n"
    }
    if {![info exists table]} return

    append html "<H2><A NAME=\"Events\">Events:</A></H2>\n"
    append html "<TABLE BORDER>\n"
    append html "<TR><TH>Date<TH>Time<TH>Event<TH>Arguments\n"
    append html $table
    append html "</TABLE>\n"
    return $html
}

proc TnmMap::HtmlMessages {item} {
 
    foreach msg [$item info messages] {
	set secs [$msg time]
	set color [TnmMap::HtmlColor [expr [$msg health] + 80]]
	append table "<TR BGCOLOR=\"$color\">"
	append table "<TD NOWRAP> [clock format $secs -format {%d %B %Y}]\n"
	append table "<TD NOWRAP> [clock format $secs -format {%T}]\n"
	append table "<TD NOWRAP> [TnmMap::HtmlQuote [$msg tag]]\n"
	append table "<TD ALIGN=RIGHT> [$msg health]\n"
	append table "<TD NOWRAP> [TnmMap::HtmlQuote [$msg text]]\n"
    }
    if {![info exists table]} return

    append html "<H2><A NAME=\"Messages\">Messages:</A></H2>\n"
    append html "<TABLE BORDER>\n"
    append html "<TR><TH>Date<TH>Time<TH>Tag<TH>Score<TH>Message\n"
    append html $table
    append html "</TABLE>\n"
    return $html
}

proc TnmMap::HtmlColor {health} {
    format #%2X%2X00 [expr 250 - $health] [expr 50 + ($health * 2)]
}

proc TnmMap::HtmlQuote {string} {
    regsub -all {&} $string {&amp;} string
    regsub -all {<} $string {\&lt;} string
    regsub -all {>} $string {\&gt;} string
    return "&nbsp;$string&nbsp;"
}

proc TnmMap::HtmlCapitalize {word} {
    return "[string toupper [string index $word 0]][string range $word 1 end]"
}
