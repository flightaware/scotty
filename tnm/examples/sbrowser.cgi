#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh8.0 "$0" "$@"
#
# sbrowser.cgi -
#
# A CGI script for the CERN httpd to browse SNMP MIBS through WWW.
#
# Copyright (c) 1994-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1997-1998 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

package require Tnm 3.0

set sbrowser sbrowser.cgi

set icon(dir) "<IMG ALT=\"\" SRC=\"/icons/dir.gif\">"
set icon(txt) "<IMG ALT=\"\" SRC=\"/icons/doc.gif\">"

##
## Exit if the load on the server is too high.
##

if {[catch {Tnm::sunrpc stat localhost} s]} {
    puts stderr "loadcheck: $s"
    exit 1
}
foreach {n t v} [join $s] { set stat($n) $v }
if {$stat(avenrun_0)/256.0 > 8} {
    puts stderr "loadcheck: load $stat(avenrun_0)/256.0 too high - exiting"
    exit 1
}

##
## Start a new html page. Writes a suitable header section.
##

proc StartPage { {title {}} } {

    puts "Content-type: text/html"
    puts ""

    puts "<HTML><HEAD><title>SNMP MIB Browser"
    if {$title != ""} {
	puts " ($title)"
    }
    puts "</title></HEAD><BODY BGCOLOR=#FFFFFF>"

    # make sure to write the header before we get error messages
    flush stdout
}

##
## End a page and the script.
##

proc EndPage {} {
    puts "</BODY></HTML>"
    exit
}

##
## List a table.
##

proc ListTable { oid host } {
    global sbrowser
    set oid [Tnm::mib oid $oid]
    if {[Tnm::mib syntax $oid] != "SEQUENCE OF"} return
    puts "<H3>[Tnm::mib label $oid] ([Tnm::mib module $oid], [file tail [Tnm::mib file $oid]])</H3>"
    puts "<BLOCKQUOTE>[Tnm::mib description $oid]</BLOCKQUOTE>"
    puts "<TABLE BORDER>"
    set entry [Tnm::mib children $oid]
    foreach o [Tnm::mib children $entry] {
	puts "<TR><TD><A HREF=\"$sbrowser?HOST=$host&OID=$o\">[Tnm::mib label $o]</A>"
	puts "<TD>[Tnm::mib oid $o]<TD>[Tnm::mib syntax $o]<TD>[Tnm::mib access $o]"
    }
    puts "</TABLE>"
}

##
## List scalars contained in a group.
##

proc ListScalars { oid host } {
    global sbrowser
    set oid [Tnm::mib oid $oid]
    foreach o [Tnm::mib children $oid] {
	if {[Tnm::mib syntax $o] != "SEQUENCE OF"} {
	    lappend scalars $o
	}
    }
    if ![info exists scalars] return
    puts "<H3>[Tnm::mib label $oid] ([Tnm::mib module $oid], [file tail [Tnm::mib file $oid]])</H3>"
    puts "<BLOCKQUOTE>[Tnm::mib description $oid]</BLOCKQUOTE>"
    puts "<TABLE BORDER>"
    foreach o $scalars {
	puts "<TR><TD><A HREF=\"$sbrowser?HOST=$host&OID=$o\">[Tnm::mib label $o]</A>"
	puts "<TD>[Tnm::mib oid $o]<TD>[Tnm::mib syntax $o]<TD>[Tnm::mib access $o]"
    }
    puts "</TABLE>"
}

##
## Some utility procs.
##

proc ListChildren { oid host {level 1} } {
    global sbrowser icon
    incr level -1
    set children [Tnm::mib children $oid]
    if {[llength $children] == 0} {
	return
    }
    puts "<DL>"
    foreach s [Tnm::mib children $oid] {
	puts "<DT>"
	if {[Tnm::mib children $s] == ""} {
	    puts "$icon(txt)"
	} else {
	    puts "$icon(dir)"
	}
	puts "<A HREF=\"$sbrowser?HOST=$host&OID=$s\">$s</A>"
	puts "([Tnm::mib oid $s])"
	puts "<DD>[Tnm::mib description $s]"
	if {$level > 0} {
	    ListChildren $s $host $level
	}
	puts "<P>"
    }
    puts "</DL>"
}

##
## List the elements of the current path.
##

proc ListPath { oid host {action BROWSE} } {
    global sbrowser
    foreach aa [split $oid .] {
	if {[info exists foo]} { puts -nonewline "." }
	lappend foo $aa
	set s [join $foo .]
	puts "<A HREF=\"$sbrowser?HOST=$host&OID=$s&ACTION=$action\">$aa</A>"
    }
}

##
## Walk a MIB subtree and print the result as HTML
##

proc WalkTree { oid host } {
    foreach host [split $host +] {
	puts "<H3>$host:</H3>"
	if {[catch {
	    set host [split $host :]
	    set s [Tnm::snmp generator -address [lindex $host 0]]
	    if {[lindex $host 1] != ""} {
		$s configure -port [lindex $host 1]
	    }
	    if {[lindex $host 2] != ""} {
		$s configure -community [lindex $host 2]
	    }
	    puts "<TABLE>"
	    $s walk x $oid {
		set o [lindex [lindex $x 0] 0]
		set v [lindex [lindex $x 0] 2]
		puts "<TR><TD>[Tnm::mib module $o]<TD>[Tnm::mib label $o].[lindex [Tnm::mib split $o] 1]<TD>$v"
		puts "<BR>"
	    }
	    puts "</TABLE>"
	    $s destroy
	} err]} {
	    puts "\[$host\] : $err"
	}
	puts "<P>"
    }
}

##
## Issue a form dialog to set the hosts to query.
##

proc GetHost { oid host } {
    global sbrowser
    StartPage
    puts "<FORM ACTION=\"$sbrowser\" METHOD=PUT>"    
    puts "<H2>Set SNMP agent addresses:</H2>" 
    puts "<INPUT TYPE=\"text\"   NAME=HOST VALUE=\"[split $host +]\" SIZE=60>"
    puts "<P>Agent addresses are either host names or internet addresses in"
    puts "decimal dot notation like 134.169.2.1 . Multiple addresses "
    puts "should be separated using white spaces. SNMP queries are send"
    puts "to the well known SNMP port 161 using community public. Every"
    puts "request will use a timeout of 5 seconds and 3 retries.<P>"
    puts "You can specify alternate port numbers or community strings other"
    puts "than public by using the syntax "
    puts "&lt;address&gt;:&lt;port&gt;:&lt;community&gt;<P>"
    puts "<INPUT TYPE=\"hidden\" NAME=OID  VALUE=$oid>"
    if {$oid == ""} {
	puts "<INPUT TYPE=\"hidden\" NAME=ACTION  VALUE=WELCOME>"
    }
    puts "<INPUT TYPE=\"submit\" VALUE=\"set and return\">"
    puts "</FORM>"
    EndPage
}

##
## And here we start. The main program at the end of this file calls
## the following procs whenever a request is received without any
## further arguments. This is the toplevel dialog page.
##

proc Welcome { oid host } {
    global sbrowser tnm
    StartPage

    puts "<H1>WWW SNMP MIB Browser</H1>"

    puts "Welcome to the WWW SNMP MIB Brower. This browser is a simple"
    puts "CGI script written in the Tool Command Language (Tcl)."
    puts "It uses the <A HREF=\"$tnm(url)\">Tnm Tcl extension</A>"
    puts "for network management applications. The CGI script is"
    puts "contained in the distribution of the"
    puts "<A HREF=\"$tnm(url)\">Tnm Tcl extension</A>.<P>"
    puts "Report bugs or any other comments to "
    puts "<TT>schoenw@ibr.cs.tu-bs.de</TT>"
    
    puts "<HR>"

    puts "<P><A HREF=\"$sbrowser?ACTION=GETHOST&OID=$oid&HOST=$host\">"
    puts "<B>Hosts:</B></A> <TT>[join [split $host +]]</TT><P><HR>"

    puts "<H2>Official Internet MIBs:</H2>"
    ListChildren mib-2 $host

    puts "<H2>SNMP Modules:</H2>"
    ListChildren snmpModules $host

    puts "<H2>Enterprise MIBs:</H2>"
    ListChildren enterprises $host

    EndPage
}

##
## Browse the mib level given by oid.
##

proc Browse { oid host } {
    global sbrowser
    StartPage [Tnm::mib name $oid]

    puts "<B>Goto:</B> "
    puts "<A HREF=\"$sbrowser?HOST=$host&OID=$oid&ACTION=WELCOME\">HOME</A> "

    ListPath $oid $host BROWSE

    puts "<P><B>Walk:</B> "

    ListPath $oid $host WALK

    puts "<P><A HREF=\"$sbrowser?ACTION=GETHOST&OID=$oid&HOST=$host\">"
    puts "<B>Hosts:</B></A> <TT>[join [split $host +]]</TT><P><HR>"

    set sucs [Tnm::mib children $oid]

    if {$sucs != ""} {
	ListChildren $oid $host 4
    } else {
	puts "<P>"
	puts "<DL>"
	puts "<DT><B>Name:</B><DD>[Tnm::mib name $oid]"
	puts "<DT><B>Identifier:</B><DD>[Tnm::mib oid $oid]"
	puts "<DT><B>Macro:</B><DD>[Tnm::mib macro $oid]"
	puts "<DT><B>Access:</B><DD>[Tnm::mib access $oid]"
	set type [Tnm::mib type $oid]
	if [string length $type] {
	    if {[Tnm::mib macro $type] == ""} {
		puts "<DT><B>Syntax:</B><DD>[Tnm::mib syntax $oid]"
	    } else {
		puts "<DT><B>Syntax:</B><DD>[Tnm::mib syntax $type]"
		if {[Tnm::mib displayhint $type] != ""} {
		    puts "<DT><B>Textual Convention:</B><DD>$type"
		    puts "<DT><B>Format:</B><DD>[Tnm::mib displayhint $type]"
		} elseif {[Tnm::mib enums $type] != ""} {
		    puts "<DT><B>Enumeration:</B><DD>[Tnm::mib enums $type]"
		} else {
		    puts "<DT><B>Textual Convention:</B><DD>$type"
		}
	    }
	}
	if {[Tnm::mib description $oid description]} {
	    puts "<DT><B>Description:</B><DD>$description"
	}
	puts "<DT><B>File:</B><DD>[file tail [Tnm::mib file $oid]]</DL><HR>"
	WalkTree $oid $host
    }

    EndPage
}

##
## Walk a MIB tree and write the result to the HTML page.
##

proc Walk { oid host } {
    global sbrowser
    StartPage [Tnm::mib name $oid]

    puts "<B>Goto:</B> "
    puts "<A HREF=\"$sbrowser?HOST=$host&OID=$oid&ACTION=WELCOME\">HOME</A> "

    ListPath $oid $host BROWSE

    puts "<P><B>Walk:</B> "

    ListPath $oid $host WALK

    puts "<P><A HREF=\"$sbrowser?ACTION=GETHOST&OID=$oid&HOST=$host\">"
    puts "<B>Hosts:</B></A> <TT>[join [split $host +]]</TT><P><HR>"

    WalkTree $oid $host

    EndPage
}

##
## The main program starts here. It checks the environment variable
## QUERY_STRING to decide what should be done. After parsing and checking 
## the parameters, we call the appropriate proc to do the job.
##

if {![info exists env(QUERY_STRING)] || ($env(QUERY_STRING) == "")} {

    Welcome "" ""

} else {

    regsub -all "%21" $env(QUERY_STRING) "!" env(QUERY_STRING)

    set query [split $env(QUERY_STRING) &]
    set action ""
    set oid ""
    set host ""
    set port 161
    foreach av $query {
	set a [lindex [split $av =] 0]
	set v [lindex [split $av =] 1]
	switch $a {
	    ACTION { set action $v }
	    HOST   { regsub -all "%20" [string trim $v +] "+" host
		     regsub -all "%3A" $host ":" host
		     split $host :
		     if {[llength $host] > 1} { 
			set port [lindex $host 1] 
			set host [lindex $host 0]
		     }
		   }
	    OID    { set oid $v }
	    *      { lappend args [list $a $v] }
	}
    }

    switch $action {
	GETHOST {
	    GetHost $oid $host
	}
	WELCOME {
	    Welcome $oid $host
	}
	WALK {
	    Walk $oid $host
	}
	TABLE {
	    StartPage
	    ListTable $oid $host
	    EndPage
	}
	SCALARS {
	    StartPage
	    ListScalars $oid $host
	    EndPage
  	}
	default {
	    Browse $oid $host
	}
    }
}

exit
