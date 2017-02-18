# init.tcl --
#
# Tnm initialization file. At the end of this file, we source the file
# init.tcl in the site subdirectory of the Tnm library directory. This
# is the preferred way to do site specific initializations because this
# will work even after updating the Tnm installation.
#
# Copyright (c) 1994-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1997-1998 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: init.tcl,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $

# Extend the auto_path to include $tnm(library)/library.

lappend auto_path $tnm(library)/library

# Export procedures from the Tnm namespace. This is the set of public
# Tnm commands. Additional commands might be defined by packages
# contained in the Tnm namespace. They will provide their own export
# list definitions.

namespace eval Tnm {
    namespace export dns icmp ined job map mib netdb ntp smx
    namespace export snmp sunrpc syslog udp
}

# Alias definitions for SNMP sessions. You can add more local 
# definitions in the $tnm(library)/site/init.tcl file.

if {[info commands ::Tnm::snmp] == "::Tnm::snmp"} {
    ::Tnm::snmp alias localhost	 "-address 127.0.0.1"
    ::Tnm::snmp alias mcasttrap	 "-address 234.0.0.1 -port 162"
}

# The global variable tnm(mibs:core) is used to hold the minimum set
# of core SNMP definitions that should be always available. You
# normally do not need to modify this list. Expect some unexpected
# results if you remove this definition.

lappend tnm(mibs:core) RFC1155-SMI SNMPv2-SMI SNMPv2-TC
lappend tnm(mibs:core) SNMPv2-TM SNMPv2-MIB SNMPv2-USEC-MIB

lappend tnm(mibs:core) IANAifType-MIB IF-MIB IF-INVERTED-STACK-MIB
lappend tnm(mibs:core) IP-MIB IP-FORWARD-MIB TCP-MIB UDP-MIB
lappend tnm(mibs:core) RFC1213-MIB

lappend tnm(mibs:core) SNMP-FRAMEWORK-MIB SNMP-MPD-MIB
lappend tnm(mibs:core) SNMP-TARGET-MIB SNMP-NOTIFICATION-MIB SNMP-PROXY-MIB
lappend tnm(mibs:core) SNMP-USER-BASED-SM-MIB SNMP-VIEW-BASED-ACM-MIB

lappend tnm(mibs:core) INET-ADDRESS-MIB

# The global variable tnm(mibs) is used to hold a list of mibs that
# are auto-loaded by the tnm Tcl extension. You can extend or
# redefine this list in your $tnm(library)/site/init.tcl file,
# which is sourced at the end of this script.

lappend tnm(mibs) RMON-MIB TOKEN-RING-RMON-MIB RMON2-MIB SMON-MIB

lappend tnm(mibs) RFC1269-MIB CLNS-MIB RFC1381-MIB RFC1382-MIB
lappend tnm(mibs) RFC1414-MIB
lappend tnm(mibs) MIOX25-MIB
 
lappend tnm(mibs) PPP-LCP-MIB PPP-SEC-MIB PPP-IP-NCP-MIB PPP-BRIDGE-NCP-MIB

lappend tnm(mibs) BRIDGE-MIB SOURCE-ROUTING-MIB P-BRIDGE-MIB Q-BRIDGE-MIB

lappend tnm(mibs) FDDI-SMT73-MIB 

lappend tnm(mibs) HOST-RESOURCES-MIB HOST-RESOURCES-TYPES RDBMS-MIB
lappend tnm(mibs) DNS-SERVER-MIB DNS-RESOLVER-MIB

lappend tnm(mibs) DECNET-PHIV-MIB
lappend tnm(mibs) SNA-NAU-MIB SNA-SDLC-MIB DLSW-MIB APPC-MIB 
lappend tnm(mibs) APPN-MIB APPN-DLUR-MIB APPN-TRAP-MIB EBN-MIB
lappend tnm(mibs) APPLETALK-MIB
lappend tnm(mibs) TCPIPX-MIB

lappend tnm(mibs) EtherLike-MIB ETHER-CHIPSET-MIB DOT12-IF-MIB DOT12-RPTR-MIB
lappend tnm(mibs) FRNETSERV-MIB FR-ATM-PVC-SERVICE-IWF-MIB FR-MFR-MIB SIP-MIB

lappend tnm(mibs) CHARACTER-MIB RS-232-MIB PARALLEL-MIB 
lappend tnm(mibs) Modem-MIB Printer-MIB UPS-MIB

lappend tnm(mibs) TOKENRING-MIB TOKENRING-STATION-SR-MIB

lappend tnm(mibs) BGP4-MIB RIPv2-MIB OSPF-MIB OSPF-TRAP-MIB

lappend tnm(mibs) MIP-MIB ENTITY-MIB

lappend tnm(mibs) SNMP-REPEATER-MIB FRAME-RELAY-DTE-MIB
lappend tnm(mibs) ISDN-MIB DIAL-CONTROL-MIB

lappend tnm(mibs) INTEGRATED-SERVICES-MIB INTEGRATED-SERVICES-GUARANTEED-MIB
lappend tnm(mibs) RSVP-MIB 
lappend tnm(mibs) HPR-MIB HPR-IP-MIB MAU-MIB
lappend tnm(mibs) NETWORK-SERVICES-MIB DSA-MIB MTA-MIB DIRECTORY-SERVER-MIB

lappend tnm(mibs) SYSAPPL-MIB APPLICATION-MIB WWW-MIB

lappend tnm(mibs) IPATM-IPMC-MIB

lappend tnm(mibs) IPV6-TC IPV6-MIB IPV6-ICMP-MIB IPV6-TCP-MIB IPV6-UDP-MIB
lappend tnm(mibs) IPV6-MLD-MIB

lappend tnm(mibs) PerfHist-TC-MIB SONET-MIB

lappend tnm(mibs) DS0-MIB DS0BUNDLE-MIB DS1-MIB DS3-MIB

lappend tnm(mibs) ATM-TC-MIB ATM-MIB
lappend tnm(mibs) ATM-ACCOUNTING-INFORMATION-MIB ACCOUNTING-CONTROL-MIB

lappend tnm(mibs) IANATn3270eTC-MIB TN3270E-MIB TN3270E-RT-MIB

lappend tnm(mibs) IANA-LANGUAGE-MIB DISMAN-SCHEDULE-MIB DISMAN-SCRIPT-MIB
lappend tnm(mibs) DISMAN-PING-MIB DISMAN-TRACEROUTE-MIB DISMAN-NSLOOKUP-MIB
# the following are commented out since our parser is too stupid
# lappend tnm(mibs) DISMAN-EVENT-MIB DISMAN-EXPRESSION-MIB

lappend tnm(mibs) RADIUS-AUTH-SERVER-MIB RADIUS-AUTH-CLIENT-MIB
lappend tnm(mibs) RADIUS-ACC-SERVER-MIB RADIUS-ACC-CLIENT-MIB

lappend tnm(mibs) ADSL-TC-MIB ADSL-LINE-MIB

lappend tnm(mibs) DOCS-CABLE-DEVICE-MIB DOCS-IF-MIB

lappend tnm(mibs) IANA-ADDRESS-FAMILY-NUMBERS-MIB NHRP-MIB

lappend tnm(mibs) FLOW-METER-MIB

lappend tnm(mibs) AGENTX-MIB

lappend tnm(mibs) SLAPM-MIB

lappend tnm(mibs) SNMP-USM-DH-OBJECTS-MIB

lappend tnm(mibs) VRRP-MIB

lappend tnm(mibs) FIBRE-CHANNEL-FE-MIB

lappend tnm(mibs) HCNUM-TC

lappend tnm(mibs) PTOPO-MIB

lappend tnm(mibs) IPMROUTE-STD-MIB

lappend tnm(mibs) IGMP-STD-MIB

lappend tnm(mibs) PIM-MIB

lappend tnm(mibs) COPS-CLIENT-MIB

lappend tnm(mibs) RTP-MIB

lappend tnm(mibs) NOTIFICATION-LOG-MIB

lappend tnm(mibs) PINT-MIB

# Some local MIB fun for experimentation...

lappend tnm(mibs) TUBS-SMI TUBS-IBR-TNM-MIB 
lappend tnm(mibs) TUBS-IBR-NFS-MIB TUBS-IBR-PROC-MIB
lappend tnm(mibs) TUBS-IBR-LINUX-MIB

# Define a proc to handle background errors if none exists. First try
# to auto_load a definition so that we don't overwrite a definition
# provided by another package.

catch {auto_load bgerror}
if {[info commands bgerror] == ""} {
    proc bgerror {msg} {
	global errorInfo
	puts stderr $errorInfo
    }
}

# This nice procedure allows us to use static variables. It was
# posted on the net by Karl Lehenbauer. There was another one
# which does not pollute the name space, but it fails on proc
# names or variable names with spaces in it...

proc static {args} {
    set procName [lindex [info level [expr [info level]-1]] 0]
    foreach varName $args {
        uplevel 1 "upvar #0 {$procName:$varName} $varName"
    }
}

# Allow for site specific initializations. Be careful to check if this
# interpreter has a file command before using it in order to deal with
# safe Tcl interpreters.

if {[info commands file] == "file"} {
    if [file exists [file join $tnm(library) site init.tcl]] {
	source [file join $tnm(library) site init.tcl]
    }
}
