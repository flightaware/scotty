# TnmIetf.tcl --
#
#	This file contains administrative information about IETF 
#	network management standards and MIBs. It needs to be updated
#	whenever a new IETF working group is created or when a new
#	RFC appears.
#
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1997-2000 Technical University Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: TnmIetf.tcl,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $

package require Tnm 3.1
package provide TnmIetf 3.1.0

namespace eval TnmMap {
    namespace export GetRfcUrl GetMibModuleUrl
    namespace export GetRfcList GetWgList GetTagList
}

namespace eval TnmIetf {
    variable rfc
    variable wg
}

# set TnmIetf::rfc(<number>) {
#     title	<RFC title>
#     status	<RFC status>
#     obsoletes	<list of RFC numbers>
#     modules   <list of SMI or MIB modules>
#     tags	<list of tags>
# }

# set TnmIetf::wg(<token>) {
#     name <WG name>
#     url  <URL to WG charter page>
#     rfc  <list of RFC numbers>
# }

# Tags: MIB, SNMPv1, SNMPv2, SNMPv2U, SNMPv3, SMIv1, SMIv2, AgentX, General,
# Tags: Hardware Transmission Network Transport Application Vendor

set TnmIetf::rfc(3055) {
    title "Management Information Base for the PINT Services Architecture"
    modules { PINT-MIB }
    status proposed
    tags { MIB }
}

set TnmIetf::rfc(3020) {
    title "Definitions of Managed Objects for Monitoring and Controlling the UNI/NNI Multilink Frame Relay Function"
    modules { FR-MFR-MIB }
    status proposed
    tags { MIB Transmission }
}

set TnmIetf::rfc(3019) {
    title "IP Version 6 Management Information Base for The Multicast Listener Discovery Protocol"
    modules { IPV6-MLD-MIB }
    status proposed
    tags { MIB Network }
}

set TnmIetf::rfc(3014) {
    title "Notification Log MIB"
    modules { NOTIFICATION-LOG-MIB }
    status proposed
    tags { MIB General }
}

set TnmIetf::rfc(2982) {
    title "Distributed Management Expression MIB"
    modules { DISMAN-EXPRESSION-MIB }
    status proposed
    tags { MIB General }
}

set TnmIetf::rfc(2981) {
    title "Event MIB"
    modules { DISMAN-EVENT-MIB }
    status proposed
    tags { MIB General }
}

set TnmIetf::rfc(2959) {
    title "Real-Time Transport Protocol Management Information Base"
    modules { RTP-MIB }
    status proposed
    tags { MIB Transport }
}

set TnmIetf::rfc(2955) {
    title "Definitions of Managed Objects for Monitoring and Controlling the Frame Relay/ATM PVC Service Interworking Function"
    status proposed
    modules { FR-ATM-PVC-SERVICE-IWF-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2954) {
    title "Definitions of Managed Objects for Frame Relay Service"
    status proposed
    obsoletes { 1596 1604 }
    modules { FRNETSERV-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2940) {
    title "Definitions of Managed Objects for Common Open Policy Service (COPS) Protocol Clients"
    modules { COPS-CLIENT-MIB }
    status proposed
    tags { MIB Application }
}

set TnmIetf::rfc(2934) {
    title "Protocol Independent Multicast MIB for IPv4"
    modules { PIM-MIB }
    status experimental
    tags { MIB Network }
}

set TnmIetf::rfc(2933) {
    title "Internet Group Management Protocol MIB"
    modules { IGMP-STD-MIB }
    status proposed
    tags { MIB Network }
}

set TnmIetf::rfc(2932) {
    title "IPv4 Multicast Routing MIB"
    modules { IPMROUTE-STD-MIB }
    status proposed
    tags { MIB Network }
}

set TnmIetf::rfc(2925) {
    title "Definitions of Managed Objects for Remote Ping, Traceroute, and Lookup Operations"
    modules { DISMAN-PING-MIB DISMAN-TRACEROUTE-MIB DISMAN-NSLOOKUP-MIB }
    status proposed
    tags { MIB General }
}

set TnmIetf::rfc(2922) {
    title "Physical Topology MIB"
    modules { PTOPO-MIB }
    status informational
    tags { MIB }
}

set TnmIetf::rfc(2896) {
    title "RMON Protocol Identifier Macros"
    status informational
}

set TnmIetf::rfc(2895) {
    title "RMON Protocol Identifier Reference"
    status proposed
    obsoletes { 2074 }
}

set TnmIetf::rfc(2864) {
    title "The Inverted Stack Table Extension to the Interfaces Group MIB"
    status proposed
    modules { IF-INVERTED-STACK-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2863) {
    title "The Interfaces Group MIB"
    status draft
    obsoletes { 2233 }
    modules { IF-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2856) {
    title "Textual Conventions for Additional High Capacity Data Types"
    status proposed
    modules { HCNUM-TC }
    tags { MIB }
}

set TnmIetf::rfc(2851) {
    title "Textual Conventions for Internet Network Addresses"
    status proposed
    modules { INET-ADDRESS-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2837) {
    title "Definitions of Managed Objects for the Fabric Element in Fibre Channel Standard"
    status proposed
    modules { FIBRE-CHANNEL-FE-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2819) {
    title "Remote Network Monitoring Management Information Base"
    status standard
    obsoletes { 1271 1757 }
    modules { RMON-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2790) {
    title "Host Resources MIB"
    status draft
    modules { HOST-RESOURCES-MIB HOST-RESOURCES-TYPES }
    tags { MIB Application }
}

set TnmIetf::rfc(2787) {
    title "Definitions of Managed Objects for the Virtual Router Redundancy Protocol"
    status proposed
    modules { VRRP-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2786) {
    title "Diffie-Helman USM Key Management Information Base and Textual Convention"
    status experimental
    modules { SNMP-USM-DH-OBJECTS-MIB }
    tags { MIB SNMPv3 }
}

set TnmIetf::rfc(2758) {
    title "Definitions of Managed Objects for Service Level Agreements Performance Monitoring"
    status experimental
    modules { SLAPM-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(2742) {
    title "Definitions of Managed Objects for Extensible SNMP Agents"
    status proposed
    modules { AGENTX-MIB }
    tags { MIB AgentX }
}

set TnmIetf::rfc(2737) {
    title "Entity MIB (Version 2)"
    status proposed
    modules { ENTITY-MIB }
    tags { MIB Hardware }
}

set TnmIetf::rfc(2707) {
    title "Job Monitoring MIB - V1.0"
    status informational
    modules { Job-Monitoring-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(2720) {
    title "Traffic Flow Measurement: Meter MIB"
    status proposed
    modules { FLOW-METER-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2677) {
    title "Definitions of Managed Objects for the NBMA Next Hop Resolution Protocol (NHRP)"
    status proposed
    modules { NHRP-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2674) {
    title "Definitions of Managed Objects for Bridges with Traffic Classes, Multicast Filtering and Virtual LAN Extensions"
    status proposed
    modules { P-BRIDGE-MIB Q-BRIDGE-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2670) {
    title "Radio Frequency (RF) Interface Management Information Base for MCNS/DOCSIS compliant RF interfaces"
    status proposed
    modules { DOCS-IF-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2669) {
    title "Cable Device Management Information Base for DOCSIS compliant Cable Modems and Cable Modem Termination Systems"
    status proposed
    modules { DOCS-CABLE-DEVICE-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2668) {
    title "Definitions of Managed Objects for IEEE 802.3 Medium Attachment Units (MAUs) using SMIv2"
    status proposed
    obsoletes { 2239 1515 }
    modules { MAU-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2667) {
    title "IP Tunnel MIB"
    status proposed
    modules { TUNNEL-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2666) {
    title "Definitions of Object Identifiers for Identifying Ethernet Chip Sets"
    status informational
    modules { ETHER-CHIPSET-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2665) {
    title "Definitions of Managed Objects for the Ethernet-like Interface Types"
    status proposed
    modules { EtherLike-MIB }
    obsoletes { 2358 }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2662) {
    title "Definitions of Managed Objects for the ADSL Lines"
    status proposed
    modules { ADSL-TC-MIB ADSL-LINE-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2621) {
    title "RADIUS Accounting Server MIB"
    status informational
    modules { RADIUS-ACC-SERVER-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(2620) {
    title "RADIUS Accounting Client MIB"
    status informational
    modules { RADIUS-ACC-CLIENT-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(2619) {
    title "RADIUS Authentication Server MIB"
    status proposed
    modules { RADIUS-AUTH-SERVER-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(2618) {
    title "RADIUS Authentication Client MIB"
    status proposed
    modules { RADIUS-AUTH-CLIENT-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(2613) {
    title "Remote Network Monitoring MIB Extensions for Switched Networks Version 1.0"
    status proposed
    modules { SMON-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2605) {
    title "Directory Server Monitoring MIB"
    status proposed
    modules { DIRECTORY-SERVER-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(2594) {
    title "Definitions of Managed Objects for WWW Services"
    status proposed
    modules { WWW-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(2592) {
    title "Definitions of Managed Objects for the Delegation of Management Scripts"
    status proposed
    modules { DISMAN-SCRIPT-MIB }
    tags { MIB General }
}

set TnmIetf::rfc(2591) {
    title "Definitions of Managed Objects for Scheduling Management Operations"
    status proposed
    modules { DISMAN-SCHEDULE-MIB }
    tags { MIB General }
}

set TnmIetf::rfc(2584) {
    title "Definitions of Managed Objects for APPN/HPR in IP Networks"
    status proposed
    modules { HPR-IP-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(2580) {
    title "Conformance Statements for SMIv2"
    status standard
    modules { SNMPv2-CONF }
    obsoletes { 1904 }
    tags { SMIv2 }
}

set TnmIetf::rfc(2579) {
    title "Textual Conventions for SMIv2"
    status standard
    obsoletes { 1903 }
    modules { SNMPv2-TC }
    tags { SMIv2 }
}

set TnmIetf::rfc(2578) {
    title "Structure of Management Information Version 2 (SMIv2)"
    status standard
    obsoletes { 1902 }
    modules { SNMPv2-SMI }
    tags { SMIv2 }
}

set TnmIetf::rfc(2576) {
    title "Coexistence between Version 1, Version 2, and Version 3 of the Internet-standard Network Management Framework"
    status proposed
    modules { SNMP-COMMUNITY-MIB }
    tags { SNMPv3 }
}

set TnmIetf::rfc(2575) {
    title "View-based Access Control Model (VACM) for the Simple Network Management Protocol (SNMP)"
    status draft
    obsoletes { 2265 2275 }
    modules { SNMP-VIEW-BASED-ACM-MIB }
    tags { SNMPv3 }
}

set TnmIetf::rfc(2574) {
    title "User-based Security Model (USM) for version 3 of the Simple Network Management Protocol (SNMPv3)"
    status draft
    obsoletes { 2264 2274 }
    modules { SNMP-USER-BASED-SM-MIB }
    tags { SNMPv3 }
}

set TnmIetf::rfc(2573) {
    title "SNMP Applications"
    status draft
    obsoletes { 2263 2273 }
    modules { SNMP-TARGET-MIB SNMP-NOTIFICATION-MIB SNMP-PROXY-MIB }
    tags { SNMPv3 }
}

set TnmIetf::rfc(2572) {
    title "Message Processing and Dispatching for the Simple Network Management Protocol (SNMP)"
    status draft
    obsoletes { 2262 2272 }
    modules { SNMP-MPD-MIB }
    tags { SNMPv3 }
}

set TnmIetf::rfc(2571) {
    title "An Architecture for Describing SNMP Management Frameworks"
    status draft
    obsoletes { 2261 2271 }
    modules { SNMP-FRAMEWORK-MIB }
    tags { SNMPv3 }
}

set TnmIetf::rfc(2564) {
    title "Application Management MIB"
    status proposed
    modules { APPLICATION-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(2562) {
    title "Definitions of Protocol and Managed Objects for TN3270E Response Time Collection Using SMIv2"
    status proposed
    modules { TN3270E-RT-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(2561) {
    title "Base Definitions of Managed Objects for TN3270E Using SMIv2"
    status proposed
    modules { TN3270E-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(2558) {
    title "Definitions of Managed Objects for the SONET/SDH Interface Type"
    status proposed
    obsoletes { 1595 }
    modules { SONET-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2515) {
    title "Definitions of Managed Objects for ATM Management"
    status proposed
    obsoletes { 1695 }
    modules { ATM-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2514) {
    title "Definitions of Textual Conventions and OBJECT-IDENTITIES for ATM Management"
    status proposed
    modules { ATM-TC-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2513) {
    title "Managed Objects for Controlling the Collection and Storage of Accounting Information for Connection-Oriented Networks"
    status proposed
    modules { ACCOUNTING-CONTROL-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2512) {
    title "Accounting Information for ATM Networks"
    status proposed
    modules { ATM-ACCOUNTING-INFORMATION-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2496) {
    title "Definitions of Managed Objects for the DS3/E3 Interface Type"
    status proposed
    obsoletes { 1407 }
    modules { DS3-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2495) {
    title "Definitions of Managed Objects for the DS1, E1, DS2 and E2 Interface Types"
    status proposed
    obsoletes { 1406 }
    modules { DS1-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2494) {
    title "Definitions of Managed Objects for the DS0 and DS0 Bundle Interface Type"
    status proposed
    modules { DS0-MIB DS0BUNDLE-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2493) {
    title "Textual Conventions for MIB Modules Using Performance History Based on 15 Minute Intervals"
    status proposed
    modules { PerfHist-TC-MIB }
    tags { MIB }
}

set TnmIetf::rfc(2466) {
    title "Management Information Base for IP Version 6: ICMPv6 Group"
    status proposed
    modules { IPV6-ICMP-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2465) {
    title "Management Information Base for IP Version 6: Textual Conventions and General Group"
    status proposed
    modules { IPV6-TC IPV6-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2457) {
    title "Definitions of Managed Objects for Extended Border Node"
    status proposed
    modules { EBN-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(2456) {
    title "Definitions of Managed Objects for APPN TRAPS"
    status proposed
    modules { APPN-TRAP-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(2455) {
    title "Definitions of Managed Objects for APPN"
    status proposed
    obsoletes { 2155 }
    modules { APPN-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(2454) {
    title "IP Version 6 Management Information Base for the User Datagram Protocol"
    status proposed
    modules { IPV6-UDP-MIB }
    tags { MIB Transport }
}

set TnmIetf::rfc(2452) {
    title "IP Version 6 Management Information Base for the Transmission Control Protocol"
    status proposed
    modules { IPV6-TCP-MIB }
    tags { MIB Transport }
}

set TnmIetf::rfc(2417) {
    title "Definitions of Managed Objects for Multicast over UNI 3.0/3.1 based ATM Networks"
    status proposed
    obsoletes { 2366 }
    modules { IPATM-IPMC-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2320) {
    title "Definitions of Managed Objects for Classical IP and ARP Over ATM Using SMIv2 (IPOA-MIB)"
    status proposed
    modules { IPOA-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2287) {
    title "Definitions of System-Level Managed Objects for Applications"
    status proposed
    modules { SYSAPPL-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(2266) {
    title "Definitions of Managed Objects for IEEE 802.12 Repeater Devices"
    status proposed
    modules { DOT12-RPTR-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2741) {
    title "Agent Extensibility (AgentX) Protocol Version 1"
    status proposed
    tags { AgentX }
}

set TnmIetf::rfc(2249) {
    title "Mail Monitoring MIB"
    status proposed
    obsoletes { 1566 }
    modules { MTA-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(2248) {
    title "Network Services Monitoring MIB"
    status proposed
    obsoletes { 1565 }
    modules { NETWORK-SERVICES-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(2238) {
    title "Definitions of Managed Objects for HPR using SMIv2"
    status proposed
    modules { HPR-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(2232) {
    title "Definitions of Managed Objects for DLUR using SMIv2"
    status proposed
    modules { APPN-DLUR-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(2214) {
    title "Integrated Services Management Information Base Guaranteed Service Extensions using SMIv2"
    status proposed
    modules { INTEGRATED-SERVICES-GUARANTEED-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2213) {
    title "Integrated Services Management Information Base using SMIv2"
    status proposed
    modules { INTEGRATED-SERVICES-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2206) {
    title "RSVP Management Information Base using SMIv2"
    status proposed
    modules { RSVP-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2128) {
    title "Dial Control Management Information Base using SMIv2"
    status proposed
    modules { DIAL-CONTROL-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2127) {
    title "ISDN Management Information Base using SMIv2"
    status proposed
    modules { ISDN-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2115) {
    title "Management Information Base for Frame Relay DTEs Using SMIv2"
    status draft
    modules { FRAME-RELAY-DTE-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2108) {
    title "Definitions of Managed Objects for IEEE 802.3 Repeater Devices using SMIv2"
    status proposed
    obsoletes { 1516 }
    modules { SNMP-REPEATER-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2096) {
    title "IP Forwarding Table MIB"
    status proposed
    obsoletes { 1354 }
    modules { IP-FORWARD-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2089) {
    title "Mapping SNMPv2 onto SNMPv1 within a bi-lingual SNMP agent"
    status informational
    tags { SNMPv2 }
}

set TnmIetf::rfc(2051) {
    title "Definitions of Managed Objects for APPC using SMIv2"
    status proposed
    modules { APPC-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(2039) {
    title "Applicablity of Standards Track MIBs to Management of World Wide Web Servers"
    status informational
    tags { Miscellany }
}

set TnmIetf::rfc(2024) {
    title "Definitions of Managed Objects for Data Link Switching using SMIv2"
    status proposed
    modules { DLSW-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2021) {
    title "Remote Network Monitoring Management Information Base Version 2 using SMIv2"
    status proposed
    modules { RMON2-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2020) {
    title "Definitions of Managed Objects for IEEE 802.12 Interfaces"
    status proposed
    modules { DOT12-IF-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(2013) {
    title "SNMPv2 Management Information Base for the User Datagram Protocol using SMIv2"
    status proposed
    modules { UDP-MIB }
    tags { MIB Transport }
}

set TnmIetf::rfc(2012) {
    title "SNMPv2 Management Information Base for the Transmission Control Protocol using SMIv2"
    status proposed
    modules { TCP-MIB }
    tags { MIB Transport }
}

set TnmIetf::rfc(2011) {
    title "SNMPv2 Management Information Base for the Internet Protocol using SMIv2"
    status proposed
    modules { IP-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(2006) {
    title "The Definitions of Managed Objects for IP Mobility Support using SMIv2"
    status proposed
    modules { MIP-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(1944) {
    title "Benchmarking Methodology for Network Interconnect Devices"
    status informational
    tags { Miscellany }
}

set TnmIetf::rfc(1910) {
    title "User-based Security Model for SNMPv2"
    status experimental
    modules { SNMPv2-USEC-MIB }
    tags { SNMPv2U }
}

set TnmIetf::rfc(1909) {
    title "An Administrative Infrastructure for SNMPv2"
    status experimental
    tags { SNMPv2U }
}

set TnmIetf::rfc(1908) {
    title "Coexistence between Version 1 and Version 2 of the Internet-standard Network Management Framework"
    status draft
    obsoletes { 1452 }
    tags { SNMPv2 }
}

set TnmIetf::rfc(1907) {
    title "Management Information Base for Version 2 of the Simple Network Management Protocol (SNMPv2)"
    status draft
    obsoletes { 1450 }
    modules { SNMPv2-MIB }
    tags { SNMPv2 }
}

set TnmIetf::rfc(1906) {
    title "Transport Mappings for Version 2 of the Simple Network Management Protocol (SNMPv2)"
    status draft
    obsoletes { 1449 }
    modules { SNMPv2-TM }
    tags { SNMPv2 }
}

set TnmIetf::rfc(1905) {
    title "Protocol Operations for Version 2 of the Simple Network Management Protocol (SNMPv2)"
    status draft
    obsoletes { 1448 }
    tags { SNMPv2 }
}

set TnmIetf::rfc(1901) {
    title "Introduction to Community-based SNMPv2"
    status experimental
    tags { SNMPv2 }
}

set TnmIetf::rfc(1857) {
    title "A Model for Common Operational Statistics"
    status informational
    obsoletes { 1404 }
}

set TnmIetf::rfc(1850) {
    title "OSPF Version 2 Management Information Base"
    status draft
    obsoletes { 1253 }
    modules { OSPF-MIB OSPF-TRAP-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(1792) {
    title "TCP/IPX Connection Mib Specification"
    status experimental
    modules { TCPIPX-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(1759) {
    title "Printer MIB"
    status proposed
    modules { Printer-MIB }
    tags { MIB Hardware }
}

set TnmIetf::rfc(1749) {
    title "IEEE 802.5 Station Source Routing MIB using SMIv2"
    status proposed
    modules { TOKENRING-STATION-SR-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1748) {
    title "IEEE 802.5 MIB using SMIv2"
    status draft
    obsoletes { 1743 1231 }
    modules { TOKENRING-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1747) {
    title "Definitions of Managed Objects for SNA Data Link Control (SDLC) using SMIv2"
    status proposed
    obsoletes { 1243 }
    modules { SNA-SDLC-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(1742) {
    title "AppleTalk Management Information Base II"
    status proposed
    obsoletes { 1243 }
    modules { APPLETALK-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(1724) {
    title "RIP Version 2 MIB Extension"
    status draft
    obsoletes { 1389 }
    modules { RIPv2-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(1697) {
    title "Relational Database Management System (RDBMS) Management Information Base (MIB) using SMIv2"
    status proposed
    modules { RDBMS-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(1696) {
    title "Modem Management Information Base (MIB) using SMIv2"
    status proposed
    modules { Modem-MIB }
    tags { MIB Hardware }
}

set TnmIetf::rfc(1694) {
    title "Definitions of Managed Objects for SMDS Interfaces using SMIv2"
    status draft
    obsoletes { 1304 }
    modules { SIP-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1666) {
    title "Definitions of Managed Objects for SNA NAUs using SMIv2"
    status proposed
    obsoletes { 1665 }
    modules { SNA-NAU-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(1660) {
    title "Definitions of Managed Objects for Parallel-printer-like Hardware Devices using SMIv2"
    status draft
    obsoletes { 1318 }
    modules { PARALLEL-MIB }
    tags { MIB Hardware }
}

set TnmIetf::rfc(1659) {
    title "Definitions of Managed Objects for RS-232-like Hardware Devices using SMIv2"
    status draft
    obsoletes { 1317 }
    modules { RS-232-MIB }
    tags { MIB Hardware }
}

set TnmIetf::rfc(1658) {
    title "Definitions of Managed Objects for Character Stream Devices using SMIv2"
    status draft
    obsoletes { 1316 }
    modules { CHARACTER-MIB }
    tags { MIB Hardware }
}

set TnmIetf::rfc(1657) {
    title "Definitions of Managed Objects for the Fourth Version of the Border Gateway Protocol (BGP-4) using SMIv2"
    status draft
    modules { BGP4-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(1643) {
    title "Definitions of Managed Objects for the Ethernet-like Interface Types"
    status standard
    obsoletes { 1623 1398 }
}

set TnmIetf::rfc(1628) {
    title "UPS Management Information Base"
    status proposed
    modules { UPS-MIB }
    tags { MIB Hardware }
}

set TnmIetf::rfc(1612) {
    title "DNS Resolver MIB Extensions"
    status proposed
    modules { DNS-RESOLVER-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(1611) {
    title "DNS Server MIB Extensions"
    status proposed
    modules { DNS-SERVER-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(1592) {
    title "Simple Network Management Protocol Distributed Protocol Interface Version 2.0"
    status experimental
    obsoletes { 1228 }
}

set TnmIetf::rfc(1573) {
    title "Evolution of the Interfaces Group of MIB-II"
    status proposed
    obsoletes { 1229 }
    modules { IANAifType-MIB }
    tags { MIB }
}

set TnmIetf::rfc(1567) {
    title "X.500 Directory Monitoring MIB"
    status proposed
    modules { DSA-MIB }
    tags { MIB Application }
}

set TnmIetf::rfc(1559) {
    title "DECnet Phase IV MIB Extensions"
    status draft
    obsoletes { 1289 }
    modules { DECNET-PHIV-MIB }
    tags { MIB Vendor }
}

set TnmIetf::rfc(1525) {
    title "Definitions of Managed Objects for Source Routing Bridges"
    status proposed
    obsoletes { 1286 }
    modules { SOURCE-ROUTING-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1513) {
    title "Token Ring Extensions to the Remote Network Monitoring MIB"
    status proposed
    obsoletes { 1271 }
    modules { TOKEN-RING-RMON-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(1512) {
    title "FDDI Management Information Base"
    status proposed
    obsoletes { 1285 }
    modules { FDDI-SMT73-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1493) {
    title "Definitions of Managed Objects for Bridges"
    status draft
    obsoletes { 1286 }
    modules { BRIDGE-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1474) {
    title "The Definitions of Managed Objects for the Bridge Network Control Protocol of the Point-to-Point Protocol"
    status proposed
    modules { PPP-BRIDGE-NCP-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1473) {
    title "The Definitions of Managed Objects for the IP Network Control Protocol of the Point-to-Point Protocol"
    status proposed
    modules { PPP-IP-NCP-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1472) {
    title "The Definitions of Managed Objects for the Security Protocols of the Point-to-Point Protocol"
    status proposed
    modules { PPP-SEC-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1471) {
    title "The Definitions of Managed Objects for the Link Control Protocol of the Point-to-Point Protocol"
    status proposed
    modules { PPP-LCP-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1470) {
    title "FYI on a Network Management Tool Catalog: Tools for Monitoring and Debugging TCP/IP Internets and Interconnected Devices"
    status information
    obsoletes { 1147 }
    tags { Miscellany }
}

set TnmIetf::rfc(1461) {
    title "SNMP MIB extension for Multiprotocol Interconnect over X.25"
    status proposed
    modules { MIOX25-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1420) {
    title "SNMP over IPX"
    status proposed
    obsoletes { 1298 }
    tags { SNMPv1 Vendor }
}

set TnmIetf::rfc(1419) {
    title "SNMP over AppleTalk"
    status proposed
    tags { SNMPv1 Vendor }
}

set TnmIetf::rfc(1418) {
    title "SNMP over OSI"
    status proposed
    obsoletes { 1161 1283 }
    tags { SNMPv1 }
}

set TnmIetf::rfc(1414) {
    title "Identification MIB"
    status proposed
    modules { RFC1414-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(1407) {
    title "Definitions of Managed Objects for the DS3/E3 Interface Type"
    status proposed
    obsoletes { 1233 }
    modules { RFC1407-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1406) {
    title "Definitions of Managed Objects for the DS1 and E1 Interface Types"
    status proposed
    obsoletes { 1232 } 
    modules { RFC1406-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1382) {
    title "SNMP MIB Extension for the X.25 Packet Layer"
    status proposed
    modules { RFC1382-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1381) {
    title "SNMP MIB Extension for X.25 LAPB"
    status proposed
    modules { RFC1381-MIB }
    tags { MIB Transmission }
}

set TnmIetf::rfc(1321) {
    title "The MD5 Message-Digest Algorithm"
    status informational
    tags { Miscellany }
}

set TnmIetf::rfc(1303) {
    title "A Convention for Describing SNMP-based Agents"
    status informational
}

set TnmIetf::rfc(1270) {
    title "SNMP Communications Services"
    status informational
    tags { Miscellany }
}

set TnmIetf::rfc(1269) {
    title "Definitions of Managed Objects for the Border Gateway Protocol (Version 3)"
    status proposed
    modules { RFC1269-MIB }
    tags { MIB Network }
}

set TnmIetf::rfc(1238) {
    title "CLNS MIB for use with Connectionless Network Protocol (ISO 8473) and End System to Intermediate System (ISO 9542)"
    status experimental
    obsoletes { 1162 }
    modules { CLNS-MIB }
}

set TnmIetf::rfc(1224) {
    title "Techniques for Managing Asynchronously Generated Alerts"
    status experimental
    tags { Miscellany }
}

set TnmIetf::rfc(1215) {
    title "A Convention for Defining Traps for use with the SNMP"
    status informational
    tags { SMIv1 }
}

set TnmIetf::rfc(1213) {
    title "Management Information Base for Network Management of TCP/IP-based internets: MIB-II"
    status standard
    obsoletes { 1158 }
    modules { RFC1213-MIB }
    tags { SNMPv1 MIB Network }
}

set TnmIetf::rfc(1212) {
    title "Concise MIB Definitions"
    status standard
    tags { SMIv1 }
}

set TnmIetf::rfc(1187) {
    title "Bulk Table Retrieval with the SNMP"
    status experimental
}

set TnmIetf::rfc(1157) {
    title "A Simple Network Management Protocol (SNMP)"
    status standard
    obsoletes { 1098 }
    tags { SNMPv1 }
}

set TnmIetf::rfc(1155) {
    title "Structure and Identification of Management Information for TCP/IP-based Internets"
    status standard
    obsoletes { 1065 }
    modules { RFC1155-SMI }
    tags { SMIv1 }
}

# Definitions of the various working groups. This needs to be updated
# if new working groups publish RFCs.

set TnmIetf::wg(pint) {
    name "PSTN and Internet Internetworking"
    url http://www.ietf.org/html.charters/pint-charter.html
    rfcs { 3055 }
}

set TnmIetf::wg(ipfc) {
    name "Audio/Video Transport"
    url http://www.ietf.org/html.charters/avt-charter.html
    rfcs { 2959 }
}

set TnmIetf::wg(rap) {
    name "Resource Allocation Protocol"
    url http://www.ietf.org/html.charters/rap-charter.html
    rfcs { 2940 }
}

set TnmIetf::wg(ipfc) {
    name "IP Over Fibre Channel"
    url http://www.ietf.org/html.charters/ipfc-charter.html
    rfcs { 2837 }
}

set TnmIetf::wg(rtfm) {
    name "Realtime Traffic Flow Measurement"
    url http://www.ietf.org/html.charters/rtfm-charter.html
    rfcs { 2720 }
}

set TnmIetf::wg(ion) {
    name "Internetworking Over NBMA"
    url http://www.ietf.org/html.charters/ion-charter.html
    rfcs { 2677 }
}

set TnmIetf::wg(ipcdn) {
    name "IP over Cable Data Network"
    url http://www.ietf.org/html.charters/ipcdn-charter.html
    rfcs { 2669 2670 }
}

set TnmIetf::wg(adslmib) {
    name "ADSL MIB"
    url http://www.ietf.org/html.charters/adslmib-charter.html
    rfcs { 2662 }
}

set TnmIetf::wg(radius) {
    name "Remote Authentication Dial-In User Service"
    url http://www.ietf.org/html.charters/radius-charter.html
    rfcs { 2618 2619 2620 2621 }
}

set TnmIetf::wg(rsvp) {
    name "Resource Reservation Setup Protocol"
    url http://www.ietf.org/html.charters/rsvp-charter.html
    rfcs { 2206 }
}

set TnmIetf::wg(intserv) {
    name "Integrated Services"
    url http://www.ietf.org/html.charters/intserv-charter.html
    rfcs { 2213 2214 }
}

set TnmIetf::wg(agentx) {
    name "SNMP Agent Extensibility"
    url http://www.ietf.org/html.charters/agentx-charter.html
    rfcs { 2741 2742 }
}

set TnmIetf::wg(appleip) {
    name "IP Over AppleTalk"
    url http://www.ietf.org/html.charters/appleip-charter.html
    rfcs { 1742 }
}

set TnmIetf::wg(applmib) {
    name "Application MIB"
    url http://www.ietf.org/html.charters/applmib-charter.html
    rfcs { 2287 2564 2594 }
}

set TnmIetf::wg(atommib) {
    name "AToM MIB"
    url http://www.ietf.org/html.charters/atommib-charter.html
    rfcs { 1595 2512 2513 2514 2515 }
}

set TnmIetf::wg(bridge) {
    name "Bridge MIB"
    url http://www.ietf.org/html.charters/bridge-charter.html
    rfcs { 2674 1525 1493 }
}

set TnmIetf::wg(charmib) {
    name "Character MIB"
    url ""
    rfcs { 1660 1659 1658 }
}

set TnmIetf::wg(disman) {
    name "Distributed Management"
    url http://www.ietf.org/html.charters/disman-charter.html
    rfcs { 2591 2592 2925 2981 2982 3014 }
}

set TnmIetf::wg(decnetiv) {
    name "DECnet Phase IV MIB"
    url ""
    rfcs { 1559 }
}

set TnmIetf::wg(dlswmib) {
    name "Data Link Switching MIB"
    url http://www.ietf.org/html.charters/dlswmib-charter.html
    rfcs { 2024 }
}

set TnmIetf::wg(dns) {
    name "Domain Name System"
    url ""
    rfcs { 1612 1611 }
}

set TnmIetf::wg(entmib) {
    name "Entity MIB"
    url http://www.ietf.org/html.charters/entmib-charter.html
    rfcs { 2037 2737 }
}

set TnmIetf::wg(frnetmib) {
    name "Frame Relay Service MIB"
    url http://www.ietf.org/html.charters/frnetmib-charter.html
    rfcs { 1604 2954 3020 }
}

set TnmIetf::wg(fddimib) {
    name "FDDI MIB"
    url ""
    rfcs { 1512 }
}

set TnmIetf::wg(hostmib) {
    name "Host Resources MIB"
    url ""
    rfcs { 1514 2790 }
}

set TnmIetf::wg(hubmib) {
    name "IEEE 802.3 Hub MIB"
    url http://www.ietf.org/html.charters/hubmib-charter.html
    rfcs { 2668 2666 2665 2358 2239 2108 }
}

set TnmIetf::wg(idmr) {
    name "Inter-Domain Multicast Routing"
    url http://www.ietf.org/html.charters/idmr-charter.html
    rfcs { 2932 2933 2934 }
}

set TnmIetf::wg(idr) {
    name "Inter-Domain Routing"
    url http://www.ietf.org/html.charters/idr-charter.html
    rfcs { 1657 1269 }
}

set TnmIetf::wg(ifmib) {
    name "Interfaces MIB"
    url http://www.ietf.org/html.charters/ifmib-charter.html
    rfcs { 1749 1748 1694 1643 1573 2233 2667 2863 2864 }
}

set TnmIetf::wg(iplpdn) {
    name "IP over Large Public Data Networks"
    url ""
    rfcs { 2115 }
}

set TnmIetf::wg(ipv6mib) {
    name "IPv6 MIB"
    url http://www.ietf.org/html.charters/ipv6mib-charter.html
}

set TnmIetf::wg(isdnmib) {
    name "ISDN MIB"
    url http://www.ietf.org/html.charters/isdnmib-charter.html
    rfcs { 2127 2128 }
}

set TnmIetf::wg(isis) {
    name "IS-IS for IP Internets"
    url http://www.ietf.org/html.charters/isis-charter.html
}

set TnmIetf::wg(madman) {
    name "Mail and Directory Management"
    url http://www.ietf.org/html.charters/madman-charter.html
    rfcs { 1567 2249 2248 2605 }
}

set TnmIetf::wg(modemmgt) {
    name "Modem Management"
    url ""
    rfcs { 1696 }
}

set TnmIetf::wg(opstat) {
    name "Operational Statistics"
    url http://www.ietf.org/html.charters/opstat-charter.html
    rfcs { 1857 }
}

set TnmIetf::wg(ospf) {
    name "Open Shortest Path First IGP"
    url http://www.ietf.org/html.charters/ospf-charter.html
    rfcs { 1850 1224 }
}

set TnmIetf::wg(pppext) {
    name "Point-to-Point Protocol Extensions"
    url http://www.ietf.org/html.charters/pppext-charter.html
    rfcs { 1474 1473 1472 1471 2667 }
}

set TnmIetf::wg(ptopomib) {
    name "Physical Topology MIB"
    url http://www.ietf.org/html.charters/ptopomib-charter.html
}

set TnmIetf::wg(rdbmsmib) {
    name "Relational Database Management Systems MIB"
    url ""
    rfcs { 1697 }
}

set TnmIetf::wg(rip) {
    name "Routing Information Protocol"
    url http://www.ietf.org/html.charters/rip-charter.html
    rfcs { 1724 }
}

set TnmIetf::wg(rmonmib) {
    name "Remote Network Monitoring"
    url http://www.ietf.org/html.charters/rmonmib-charter.html
    rfcs { 2896 2895 2819 2613 2074 2021 1513 }
}

set TnmIetf::wg(snadlc) {
    name "SNA DLC Services MIB"
    url http://www.ietf.org/html.charters/snadlc-charter.html
    rfcs { 1747 }
}

set TnmIetf::wg(snanau) {
    name "SNA NAU Services MIB"
    url http://www.ietf.org/html.charters/snanau-charter.html
    rfcs { 1666 2051 2232 2238 2455 2456 2457 2584 }
}

set TnmIetf::wg(snmpv2) {
    name "SNMP Version 2"
    url http://www.ietf.org/html.charters/snmpv2-charter.html
    rfcs { 1908 1907 1906 1905 1904 1903 1902 1901 }
}

set TnmIetf::wg(snmpv3) {
    name "SNMP Version 3"
    url http://www.ietf.org/html.charters/snmpv3-charter.html
    rfcs { 2570 2571 2572 2573 2574 2575 2576 }
}

set TnmIetf::wg(tn3270e) {
    name "Telnet TN3270 Enhancements"
    url http://www.ietf.org/html.charters/tn3270e-charter.html
    rfcs { 2561 2562 }
}

set TnmIetf::wg(trmon) {
    name "Token Ring Remote Monitoring"
    url ""
    rfcs { 1513 }
}

set TnmIetf::wg(trunkmib) {
    name "DS1/DS3 MIB"
    url http://www.ietf.org/html.charters/trunkmib-charter.html
    rfcs { 1407 1406 2493 2494 2495 2496 }
}

set TnmIetf::wg(upsmib) {
    name "Uninterruptible Power Supply"
    url http://www.ietf.org/html.charters/upsmib-charter.html
    rfcs { 1628 }
}

set TnmIetf::wg(vgmib) {
    name "100VG-AnyLAN MIB"
    url http://www.ietf.org/html.charters/vgmib-charter.html
    rfcs { 2266 }
}

set TnmIetf::wg(x25mib) {
    name "X.25 MIB"
    url ""
    rfcs { 1461 1382 1381 }
}

# This is the end of the configuration section. Nothing below should
# need modifications is the normal use of this package.

# TnmIetf::GetRfcUrl --
#
#	Return a URL which points to an RFC document. This procedure
#	relies on the availability of RFC from ds.internic.net.
#
# Arguments:
#	number	The RFC number.
# Results:
#	The URL for the RFC document.

proc TnmIetf::GetRfcUrl {number} {
    return "ftp://ftp.isi.edu/in-notes/rfc$number.txt"
}

# TnmIetf::GetMibModuleUrl --
#
#	Return a URL which points to a HTML version of a MIB. This
#	relies on the availability of MIB modules on the SimpleTimes
#	Web server.
#
# Arguments:
#	moduler	The name of the MIB module.
# Results:
#	The URL for the MIB module.

proc TnmIetf::GetMibModuleUrl {module} {
    return "http://wwwsnmp.cs.utwente.nl/Docs/ietf/rfc/$module.html" 
}

# TnmIetf::GetRfcList --
#
#	Return the list of RFC numbers that match a given tag.
#
# Arguments:
#	topic	The topic of the requested MIB modules.
# Results:
#	A list of RFC numbers matching a given tag.

proc TnmIetf::GetRfcList {{tag {}}} {
    set result {}
    if ![string length $tag] {
	return [lsort -integer -decreasing [array names TnmIetf::rfc]]
    }
    foreach number [lsort -integer -decreasing [array names TnmIetf::rfc]] {
	array set r $TnmIetf::rfc($number)
	if {[info exists r(tags)]} {
	    if {![string length $tag] || [lsearch $r(tags) $tag] >= 0} {
		lappend result $number
	    }
	}
	unset r
    }
    return $result
}

# TnmIetf::GetWgList --
#
#       Return the list of working groups.
#
# Arguments:
#       None.
# Results:
#       A sorted list of working group names.

proc TnmIetf::GetWgList {} {
    return [lsort [array names TnmIetf::wg]]
}

# TnmIetf::GetTagList --
#
#	Return the list of tags used to classify the RFCs.
#
# Arguments:
#	None.
# Results:
#	The alphabetically sorted list of tags.

proc TnmIetf::GetTagList {} {
    foreach number [array names TnmIetf::rfc] {
	array set r $TnmIetf::rfc($number)
	foreach t $r(tags) {
	    set tags($t) $t
	}
    }
    return [lsort [array names tags]]
}

# TnmIetf::GetModuleList --
#
#       Return the list of MIB or SMI module names.
#
# Arguments:
#       None.
# Results:
#       The alphabetically sorted list of module names.

proc TnmIetf::GetModuleList {} {
    foreach number [array names TnmIetf::rfc] {
	array set r $TnmIetf::rfc($number)
	foreach mod $r(modules) {
	    set modules($mod) $number
	}
    }
    return [lsort [array names modules]]
}

# TnmIetf::GetRfcFromModule --
#
#       Return the RFC number for a given MIB or SMI module name.
#
# Arguments:
#       The MIB module name.
# Results:
#       The RFC number or an empty string if the module name is unknown.

proc TnmIetf::GetRfcFromModule { module } {
    foreach number [array names TnmIetf::rfc] {
        array set r $TnmIetf::rfc($number)
	if {[info exists r(modules)] && [lsearch $r(modules) $module] >= 0} {
	    return $number
	}
    }
    return
}

# TnmIetf::GetStatusFromModule --
#
#       Return the RFC status for a given MIB or SMI module name.
#
# Arguments:
#       The MIB module name.
# Results:
#       The status or an empty string if the module name is unknown.

proc TnmIetf::GetStatusFromModule { module } {
    foreach number [array names TnmIetf::rfc] {
        array set r $TnmIetf::rfc($number)
	if {[info exists r(modules)] && [lsearch $r(modules) $module] >= 0} {
	    if {[info exists r(status)]} {
		return $r(status)
	    } else {
		return
	    }
	}
    }
    return
}

# TnmIetf::PrintModuleList --
#
#       Returns a human readable list of all SMI and MIB modules.
#
# Arguments:
#       None.
# Results:
#       The human readable text listing all SMI and MIB modules.

proc TnmIetf::PrintModuleList {} {
    set txt ""
    foreach number [TnmIetf::GetRfcList] {
	array set r $TnmIetf::rfc($number)
	if {[info exists r(modules)]} {
	    append txt [format "RFC%4d\t%-14s %s\n" \
		    $number "\[$r(status)\]" $r(modules)]
	    set s $r(title)
	    while {[string length $s]} {
		set i [string wordend $s 50]
		append txt "\t[string range $s 0 [expr $i-1]]\n"
		set s [string trim [string range $s $i end]]
	    }
	}
	unset r
    }
    return $txt
}

# TnmIetf::Check --
#
#	Some consistency checks for the data defined in this package.
#
# Arguments:
#	None.
# Results:
#	None.

proc TnmIetf::Check {} {
    foreach number [array names TnmIetf::rfc] {
	array set r $TnmIetf::rfc($number)
	if {![info exists r(tags)]} {
	    puts stderr "No tags for RFC $number."
	}
	unset r
    }
    return
}

