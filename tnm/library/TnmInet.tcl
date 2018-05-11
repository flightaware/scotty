# inet.tcl --
#
#	This file contains some useful procedures that allow to use 
#	various standard TCP/IP network services.
#
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: TnmInet.tcl,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $

package require Tnm 3.1
package provide TnmInet 3.1.0

namespace eval TnmInet {
    namespace export GetIpAddress GetIpName DayTime Finger TraceRoute
    namespace export TcpServices RpcServices
    namespace export SendMail
    # namespace export WhoIs NfsMounts NfsExports
    # namespace export Telnet Ping
}

# TnmInet::GetIpAddress --
#
#	Get the IP address for a given IP name. This procedure
#	first tries to lookup the address in the local network
#	database. If not found, it looks for the address in the
#	Domain Name System (DNS).
#
# Arguments:
#       something       An IP address or an IP name.
# Results:
#       The IP address or an error if all lookup attempts failed.

proc TnmInet::GetIpAddress {something} {
    if {![catch {Tnm::netdb hosts name $something}]} {
	return $something
    }
    if [catch {Tnm::netdb hosts address $something} ip] {
	if [catch {Tnm::dns address $something} ip] {
	    error "failed to lookup IP address for \"$something\""
	}
    }
    return $ip
}

# TnmInet::GetIpName --
#
#	Get the IP name for a given IP address. This procedure
#	first tries to lookup the name in the local network
#	database. If not found, it looks for the name in the
#	Domain Name System (DNS).
#
# Arguments:
#       something	An IP address or an IP name.
# Results:
#       The IP name or an error if all lookup attempts failed.

proc TnmInet::GetIpName {something} {
    if {![catch {Tnm::netdb hosts address $something}]} {
	return $something
    }
    if [catch {Tnm::netdb hosts name $something} name] {
	if [catch {Tnm::dns name $something} name] {
	    error "failed to lookup IP name for \"$something\""
	}
    }
    return $name
}

# TnmInet::DayTime --
#
#	Retrieve the time of the day from a remote host.
#
# Arguments:
#	host	The target host that should be queried.
# Results:
#	The time of the day as reported by the remote host.

proc TnmInet::DayTime {{host localhost}} {
    set s [socket $host daytime]
    fconfigure $s -translation crlf
    set code [catch {gets $s txt} msg]
    close $s
    if $code { error $msg }
    string trim $txt \n
}

# TnmInet::Finger --
#
#	Get information about a host or a user on a host by using the
#	finger user information protocol (RFC 1288).
#
# Arguments:
#	host	The target host that should be finger'ed.
#	user	The name of the user from whom you want to get information.
# Results:
#	The finger information as reported by the remote host.

proc TnmInet::Finger {{host {localhost}} {user {}}} {
    set s [socket $host finger]
    fconfigure $s -translation crlf
    set txt ""
    set code [catch {
	puts $s $user; flush $s
	while {! [eof $s]} { append txt "[gets $s]\n" }
    } msg]
    close $s
    if $code { error $msg }
    string trim $txt \n
}

# TnmInet::TraceRoute --
#
#	Trace a route to a remote host. This is based on the classic 
#	van Jacobsen traceroute algorithm.
#
# Arguments:
#	host		The target host that should be traced.
#	maxlength	The maximum length of a route (default 32).
#	retries		The number of retries for each hop (default 3).
# Results:
#	The routing trace in human readable format (much like traceroute).

proc TnmInet::TraceRoute {{host localhost} {maxlength 32} {retries 3}} {
    if {![catch {Tnm::netdb ip class $host} dst]} {
	set dst $host
    } elseif {[catch {Tnm::netdb hosts address $host} dst]} {
	if {[catch {Tnm::dns address $host} dst]} {
	    error "unknown host name \"$host\""
	}
    }
    for {set i 0} {$i < $retries} {incr i} { 
	lappend icmparg $dst
    }
    set l ""
    for {set ttl 1} {[lsearch $l $dst] < 0 && $ttl < $maxlength} {incr ttl} {
        set trace [Tnm::icmp -retries 0 trace $ttl $icmparg]
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
            if {[catch {Tnm::netdb hosts name $ip} name]} {
                if {[catch {Tnm::dns name $ip} name]} {
                    set name $ip
                }
            }
            if {[lsearch $names $name] < 0} { lappend names $name }
        }
        append txt [format "%2d %-47s %s\n" \
		$ttl [string range $names 0 46] $time]
    }
    string trim $txt \n
}

# TnmInet::TcpServices --
#
#	Test the TCP services listed in the netdb services database
#	by connecting to the ports listed there. This does not really
#	tell us, if we could actually use a service, but it is 
#	nevertheless a useful hint.
#
# Arguments:
#	host	The target host that should be checked.
# Results:
#	A human readable list of services the allow to establish a
#	TCP connection.

proc TnmInet::TcpServices {{host localhost}} {
    set txt ""
    set services "X11 6000"
    foreach {name port protocol} [join [Tnm::netdb services]] {
	if {[string compare $protocol tcp] == 0} {
	    lappend services $name $port
	}
    }
    foreach {name port} $services {
	if {[catch {socket $host $port} s]} {
	    continue
	}
	close $s
	append txt [format "  %-12s %6s/tcp\n" $name $port]
    }
    string trim $txt \n
}

# TnmInet::RpcServices --
#
#	Test the RPC services registered by the portmapper of a
#	remote host by calling their procedure number 0.
#
# Arguments:
#	host	The target host that should be checked.
# Results:
#	A human readable list of RPC services registered and the
#	result of each test. The time used to call procedure 0 is
#	returned if the test was successful.

proc TnmInet::RpcServices {{host localhost}} {
    set txt ""
    set server [Tnm::sunrpc info $host]
    foreach probe [lsort -ascii $server] {
	set probe [eval format "{%10s %2d %s %5d %-16s}" $probe]
	if {[catch {Tnm::sunrpc probe $host \
		[lindex $probe 0] [lindex $probe 1] [lindex $probe 2]} res]} {
	    append txt "$probe\n"
	} else {
	    append txt [format "%s %6d ms %s\n" $probe \
		    [lindex $res 0] [lindex $res 1] ]
	}
    }
    string trim $txt \n
}

# TnmInet::SendMail --
#
#	This procedure sends a mail message to a list of recipients. 
#	It uses the SMTP protocol (RFC 821) to deliver the mail message
#	to the next SMTP MTA. The default MTA is $tnm(user)@$tnm(domain).
#	It can be overwritten by setting the global tnm(email) variable.
#
# Arguments:
#	recipients	The list of recipients.
#	message		The mail message.
#	subject		The optional subject of the message.
# Result:
#	None.

proc TnmInet::SendMail {recipients message {subject {}}} {
    global tnm

    if ![info exists tnm(email)] {
	set tnm(email) "$tnm(user)@$tnm(domain)"
    }
    if [catch {socket $tnm(host) smtp} s] {
	if [catch {Tnm::dns mx $tnm(domain)} mxhosts] {
	    error "no smtp gateway found"
	}
	foreach {h p} [join $mxhosts] {
	    lappend l [list $p $h]
	}
	foreach {priority host} [join [lsort $l]] {
	    if [catch {socket $host smtp} s] {
		continue
	    }
	    break
	}
    }
    if [catch {fconfigure $s -translation crlf}] {
	error "no smtp gateway found"
    }
    fconfigure $s -blocking true -buffering line

    TnmInet::Expect $s 220
    puts $s "HELO $tnm(host).$tnm(domain)"
    TnmInet::Expect $s 250
    puts $s "MAIL FROM:<$tnm(email)>"
    TnmInet::Expect $s 250
    foreach r $recipients {
	puts $s "RCPT TO:<$r>"
	TnmInet::Expect $s 250
    }
    puts $s "DATA"
    TnmInet::Expect $s 354
    puts $s "To: [join $recipients ,]"
    if {[string length $subject]} {
	puts $s "Subject: $subject"
    }
    puts $s $message
    puts $s "."
    TnmInet::Expect $s 250
    close $s
    return
}

# TnmInet::Expect --
#
#	This is a utility procedure to read from a stream until a 
#	RFC 821 response has been recceived.
#
# Arguments:
#	channel	The Tcl channel to read from.
#	code	The expected response code.
# Results:
#	None. Errors are generated if something goes wrong.

proc TnmInet::Expect {channel code} {
    while 1 {
	set n [gets $channel line]
	if {$n < 0} { error "error while reading from $channel" }
	if [string match {[0-9][0-9][0-9] *} $line] break
    }
    if ![string match "$code*" $line] {
	close $s
	error $line
    }
    return
}
