# TnmEther.tcl --
#
#	This file contains a simple database of ethernet codes used
#	by various vendors. The main purpose of this module is to
#	map ethernet addresses to vendor names.
#
#	This list is taken from the ethernet code list available 
#	from ftp://ftp.cavebear.com/pub/Ethernet-codes. It was last
#	updated in September 1996.
#
# Copyright (c) 1997-1998 Technical University Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: TnmEther.tcl,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $

package require Tnm 3.0
package provide TnmEther 3.0.4

namespace eval TnmEther {
    namespace export GetVendor GetEthers
}

# TnmEther::GetVendor --
#
#	Get the vendor for a given IEEE 802 ethernet address.
#
# Arguments:
#	The ethernet address must be in the format aa:bb:cc:dd:ee:ff.
# Results:
#	The name of the vendor or an empty string if there is
#	no known vendor for the given ethernet address.

proc TnmEther::GetVendor {ether} {
    set l [string toupper [join [lrange [split $ether :] 0 2] {}]]
    if {[info exists TnmEther::vendor($l)]} {
	return $TnmEther::vendor($l)
    }
    return
}

# TnmEther::GetEthers --
#
#	Get the ethernet address prefixes for all vendors that 
#	match a given pattern.
#
# Arguments:
#	The regular expression pattern used to match the vendor names.
# Results:
#	A list of ethernet address prefixes in the format aa:bb:cc.

proc TnmEther::GetEthers {pattern} {
    set result {}
    foreach ether [array names TnmEther::vendor] {
	if {[regexp -nocase -- $pattern $TnmEther::vendor($ether)]} {
	    lappend result $ether
	}
    }
    return $result
}

namespace eval TnmEther {
    variable vendor

set vendor(000002) {BBN (was internal usage only, no longer used)}
set vendor(00000C) {Cisco}
set vendor(00000E) {Fujitsu}
set vendor(00000F) {NeXT}
set vendor(000010) {Hughes LAN Systems (formerly Sytek)}
set vendor(000011) {Tektronix}
set vendor(000015) {Datapoint Corporation }
set vendor(000018) {Webster Computer Corporation Appletalk/Ethernet Gateway}
set vendor(00001A) {AMD (?)}
set vendor(00001B) {Novell (now Eagle Technology)}
set vendor(00001D) {Cabletron}
set vendor(00001F) {Cryptall Communications Corp.}
set vendor(000020) {DIAB (Data Intdustrier AB)}
set vendor(000021) {SC&C (PAM Soft&Hardware also reported)}
set vendor(000022) {Visual Technology}
set vendor(000023) {ABB Automation AB, Dept. Q}
set vendor(000029) {IMC}
set vendor(00002A) {TRW}
set vendor(000037) {Oxford Metrics Ltd}
set vendor(00003B) {Hyundai/Axil Sun Sparc Station 2 clone}
set vendor(00003C) {Auspex}
set vendor(00003D) {AT&T}
set vendor(00003F) {Syntrex Inc}
set vendor(000044) {Castelle}
set vendor(000046) {ISC-Bunker Ramo, An Olivetti Company}
set vendor(000049) {Apricot Ltd.}
set vendor(00004B) {APT A.P.T. Appletalk WAN router}
set vendor(00004C) {NEC Corporation}
set vendor(00004F) {Logicraft 386-Ware P.C. Emulator}
set vendor(000051) {Hob Electronic Gmbh & Co. KG}
set vendor(000052) {ODS}
set vendor(000055) {AT&T}
set vendor(000058) {Racore Computer Products Inc}
set vendor(00005A) {SK (Schneider & Koch in Europe and Syskonnect outside of Europe)}
set vendor(00005A) {Xerox 806 (unregistered)}
set vendor(00005B) {Eltec}
set vendor(00005D) {RCE}
set vendor(00005E) {U.S. Department of Defense (IANA)}
set vendor(00005F) {Sumitomo}
set vendor(000061) {Gateway Communications}
set vendor(000062) {Honeywell}
set vendor(000063) {Hewlett-Packard LanProbe}
set vendor(000064) {Yokogawa Digital Computer Corp}
set vendor(000065) {Network General}
set vendor(000069) {Concord Communications, Inc (although someone said Silicon Graphics)}
set vendor(00006B) {MIPS}
set vendor(00006D) {Case}
set vendor(00006E) {Artisoft, Inc.}
set vendor(000068) {Rosemount Controls}
set vendor(00006F) {Madge Networks Ltd}
set vendor(000073) {DuPont}
set vendor(000075) {Bell Northern Research (BNR)}
set vendor(000077) {Interphase [Used in other systems, e.g. MIPS, Motorola]}
set vendor(000078) {Labtam Australia}
set vendor(000079) {Networth Incorporated}
set vendor(00007A) {Ardent}
set vendor(00007B) {Research Machines}
set vendor(00007D) {Cray Research Superservers, Inc}
set vendor(00007F) {Linotronic}
set vendor(000080) {Cray Communications (formerly Dowty Network Services)}
set vendor(000081) {Synoptics}
set vendor(000083) {Optical Data Systems}
set vendor(000083) {Tadpole Technology}
set vendor(000084) {Aquila (?), ADI Systems Inc.(?)}
set vendor(000086) {Gateway Communications Inc. (also Megahertz Corporation?)}
set vendor(000089) {Cayman Systems Gatorbox}
set vendor(00008A) {Datahouse Information Systems}
set vendor(00008E) {Solbourne(?), Jupiter(?) (I've had confirming mail on Solbourne)}
set vendor(000092) {Unisys, Cogent (both reported)}
set vendor(000093) {Proteon}
set vendor(000094) {Asante MAC}
set vendor(000095) {Sony/Tektronix}
set vendor(000097) {Epoch}
set vendor(000098) {Cross Com}
set vendor(000099) {Memorex Telex Corporations}
set vendor(00009F) {Ameristar Technology}
set vendor(0000A0) {Sanyo Electronics}
set vendor(0000A2) {Wellfleet}
set vendor(0000A3) {Network Application Technology (NAT)}
set vendor(0000A4) {Acorn}
set vendor(0000A5) {Compatible Systems Corporation}
set vendor(0000A6) {Network General (internal assignment, not for products)}
set vendor(0000A7) {Network Computing Devices (NCD) X-terminals}
set vendor(0000A8) {Stratus Computer, Inc.}
set vendor(0000A9) {Network Systems}
set vendor(0000AA) {Xerox Xerox machines}
set vendor(0000AC) {Apollo}
set vendor(0000AE) {Dassault Automatismes et Telecommunications}
set vendor(0000AF) {ear Data Acquisition Interface Modules (AIM)}
set vendor(0000B0) {RND (RAD Network Devices)}
set vendor(0000B1) {Alpha Microsystems Inc.}
set vendor(0000B3) {CIMLinc}
set vendor(0000B4) {Edimax}
set vendor(0000B5) {Datability Terminal Servers}
set vendor(0000B6) {Micro-matic Research}
set vendor(0000B7) {Dove Fastnet}
set vendor(0000BB) {TRI-DATA Systems Inc. Netway products, 3274 emulators}
set vendor(0000BC) {Allen-Bradley}
set vendor(0000C0) {Western Digital now SMC (Std. Microsystems Corp.)}
set vendor(0000C1) {Olicom A/S}
set vendor(0000C5) {Farallon Computing Inc}
set vendor(0000C6) {HP Intelligent Networks Operation (formerly Eon Systems)}
set vendor(0000C8) {Altos}
set vendor(0000C9) {Emulex Terminal Servers, Print Servers}
set vendor(0000CC) {Densan Co., Ltd.}
set vendor(0000CD) {Industrial Research Limited}
set vendor(0000D0) {Develcon Electronics, Ltd.}
set vendor(0000D1) {Adaptec, Inc. "Nodem" product}
set vendor(0000D2) {SBE Inc}
set vendor(0000D3) {Wang Labs}
set vendor(0000D4) {PureData}
set vendor(0000D7) {Dartmouth College (NED Router)}
set vendor(0000D8) {old Novell NE1000's (before about 1987?) (also 3Com)}
set vendor(0000DD) {Gould}
set vendor(0000DE) {Unigraph}
set vendor(0000E2) {Acer Counterpoint}
set vendor(0000E3) {Integrated Micro Products Ltd}
set vendor(0000E6) {Aptor Produits De Comm Indust}
set vendor(0000E8) {Accton Technology Corporation}
set vendor(0000E9) {ISICAD, Inc.}
set vendor(0000ED) {April}
set vendor(0000EE) {Network Designers Limited	[also KNX Ltd, a former division]}
set vendor(0000EF) {Alantec}
set vendor(0000F0) {Samsung}
set vendor(0000F2) {Spider Communications (Montreal, not Spider Systems)}
set vendor(0000F3) {Gandalf Data Ltd. - Canada}
set vendor(0000F4) {Allied Telesis, Inc.}
set vendor(0000F6) {A.M.C. (Applied Microsystems Corp.)}
set vendor(0000F8) {DEC}
set vendor(0000FB) {Rechner zur Kommunikation}
set vendor(0000FD) {High Level Hardware (Orion, UK)}
set vendor(000102) {BBN (Bolt Beranek and Newman, Inc.)	internal usage (not registered)}
set vendor(000143) {IEEE 802}
set vendor(000163) {NDC  (National Datacomm Corporation)}
set vendor(000168) {W&G  (Wandel & Goltermann)}
set vendor(0001C8) {Thomas Conrad Corp.}
set vendor(0001FA) {q (PageMarq printers)}
set vendor(000204) {Novell NE3200}
set vendor(000205) {lton (Sparc Clones)}
set vendor(000288) {Global Village (PCcard in Mac portable)}
set vendor(000502) {Apple (PCI bus Macs)}
set vendor(0005A8) {rComputing Mac clones}
set vendor(000701) {Racal-Datacom}
set vendor(000852) {Technically Elite Concepts}
set vendor(000855) {Fermilab}
set vendor(001700) {Kabel}
set vendor(002008) {Cable & Computer Technology}
set vendor(00200C) {Adastra Systems Corp}
set vendor(002011) {Canopus Co Ltd}
set vendor(002017) {Orbotech}
set vendor(002025) {Control Technology Inc}
set vendor(002036) {BMC Software}
set vendor(002042) {Datametrics Corp}
set vendor(002045) {SolCom Systems Limited}
set vendor(002048) {Fore Systems Inc}
set vendor(00204B) {Autocomputer Co Ltd}
set vendor(00204C) {Mitron Computer Pte Ltd}
set vendor(002056) {Neoproducts}
set vendor(002061) {Dynatech Communications Inc}
set vendor(002063) {Wipro Infotech Ltd}
set vendor(002066) {General Magic Inc}
set vendor(002067) {Node Runner Inc}
set vendor(002078) {Runtop Inc}
set vendor(00208A) {Sonix Communications Ltd}
set vendor(00208B) {Focus Enhancements}
set vendor(00208C) {Galaxy Networks Inc}
set vendor(002094) {Cubix Corporation}
set vendor(0020A6) {Proxim Inc}
set vendor(0020AF) {3COM Corporation}
set vendor(0020B6) {Agile Networks Inc}
set vendor(0020B9) {Metricom, Inc.}
set vendor(0020C6) {NECTEC}
set vendor(0020D2) {RAD Data Communications Ltd}
set vendor(0020D3) {OST (Ouet Standard Telematique)}
set vendor(0020DC) {Densitron Taiwan Ltd}
set vendor(0020EE) {Gtech Corporation}
set vendor(0020F6) {Net Tek & Karlnet Inc}
set vendor(0020F8) {Carrera Computers Inc}
set vendor(004001) {Zero One Technology Co Ltd (ZyXEL?)}
set vendor(004005) {TRENDware International Inc.; Linksys; Simple Net; all three reported}
set vendor(004009) {Tachibana Tectron Co Ltd}
set vendor(00400B) {Crescendo (now owned by Cisco)}
set vendor(00400C) {General Micro Systems, Inc.}
set vendor(00400D) {LANNET Data Communications}
set vendor(004010) {Sonic Mac Ethernet interfaces}
set vendor(004013) {NTT Data Communication Systems Corp}
set vendor(004014) {Comsoft Gmbh}
set vendor(004015) {Ascom}
set vendor(00401F) {Colorgraph Ltd}
set vendor(004020) {Pilkington Communication}
set vendor(004023) {Logic Corporation}
set vendor(004025) {Molecular Dynamics}
set vendor(004026) {Melco Inc}
set vendor(004027) {SMC Massachusetts}
set vendor(004028) {Netcomm}
set vendor(00402A) {Canoga-Perkins}
set vendor(00402B) {TriGem}
set vendor(00402F) {Xlnt Designs Inc (XDI)}
set vendor(004030) {GK Computer }
set vendor(004032) {Digital Communications}
set vendor(004033) {Addtron Technology Co., Ltd.}
set vendor(004036) {eStar}
set vendor(004039) {Optec Daiichi Denko Co Ltd}
set vendor(00403C) {Forks, Inc.}
set vendor(004041) {Fujikura Ltd.}
set vendor(004043) {Nokia Data Communications}
set vendor(004048) {SMD Informatica S.A.}
set vendor(00404C) {Hypertec Pty Ltd.}
set vendor(00404D) {Telecomm Techniques}
set vendor(00404F) {Space & Naval Warfare Systems}
set vendor(004050) {Ironics, Incorporated}
set vendor(004052) {Star Technologies Inc}
set vendor(004054) {Thinking Machines Corporation}
set vendor(004057) {Lockheed-Sanders}
set vendor(004059) {Yoshida Kogyo K.K.}
set vendor(00405B) {Funasset Limited}
set vendor(00405D) {Star-Tek Inc}
set vendor(004066) {Hitachi Cable, Ltd.}
set vendor(004067) {Omnibyte Corporation}
set vendor(004068) {Extended Systems}
set vendor(004069) {Lemcom Systems Inc}
set vendor(00406A) {Kentek Information Systems Inc}
set vendor(00406E) {Corollary, Inc.}
set vendor(00406F) {Sync Research Inc}
set vendor(004074) {Cable and Wireless}
set vendor(004076) {AMP Incorporated}
set vendor(004078) {Wearnes Automation Pte Ltd}
set vendor(00407F) {Agema Infrared Systems AB}
set vendor(004082) {Laboratory Equipment Corp}
set vendor(004085) {SAAB Instruments AB}
set vendor(004086) {Michels & Kleberhoff Computer}
set vendor(004087) {Ubitrex Corporation}
set vendor(004088) {Mobuis NuBus (Mac) combination video/EtherTalk}
set vendor(00408A) {TPS Teleprocessing Sys. Gmbh}
set vendor(00408C) {Axis Communications AB}
set vendor(00408E) {CXR/Digilog}
set vendor(00408F) {WM-Data Minfo AB}
set vendor(004090) {Ansel Communications PC NE2000 compatible twisted-pair ethernet cards}
set vendor(004091) {Procomp Industria Eletronica}
set vendor(004092) {ASP Computer Products, Inc.}
set vendor(004094) {Shographics Inc}
set vendor(004095) {Eagle Technologies}
set vendor(004096) {Telesystems SLW Inc}
set vendor(00409A) {Network Express Inc}
set vendor(00409C) {Transware}
set vendor(00409D) {DigiBoard Ethernet-ISDN bridges}
set vendor(00409E) {Concurrent Technologies  Ltd.}
set vendor(00409F) {Lancast/Casat Technology Inc}
set vendor(0040A4) {Rose Electronics}
set vendor(0040A6) {Cray Research Inc.}
set vendor(0040AA) {Valmet Automation Inc}
set vendor(0040AD) {SMA Regelsysteme Gmbh}
set vendor(0040AE) {Delta Controls, Inc.}
set vendor(0040AF) {Digital Products, Inc. (DPI).}
set vendor(0040B4) {3COM K.K.}
set vendor(0040B5) {Video Technology Computers Ltd}
set vendor(0040B6) {Computerm Corporation}
set vendor(0040B9) {MACQ Electronique SA}
set vendor(0040BD) {Starlight Networks Inc}
set vendor(0040C1) {Bizerba-Werke Wilheim Kraut}
set vendor(0040C2) {Applied Computing Devices}
set vendor(0040C3) {Fischer and Porter Co.}
set vendor(0040C5) {Micom Communications Corp.}
set vendor(0040C6) {Fibernet Research, Inc.}
set vendor(0040C8) {Milan Technology Corp.}
set vendor(0040CC) {Silcom Manufacturing Technology Inc}
set vendor(0040CF) {Strawberry Tree Inc}
set vendor(0040D2) {Pagine Corporation}
set vendor(0040D4) {Gage Talker Corp.}
set vendor(0040D7) {Studio Gen Inc}
set vendor(0040D8) {Ocean Office Automation Ltd}
set vendor(0040DC) {Tritec Electronic Gmbh}
set vendor(0040DF) {Digalog Systems, Inc.}
set vendor(0040E1) {Marner International Inc}
set vendor(0040E2) {Mesa Ridge Technologies Inc}
set vendor(0040E3) {Quin Systems Ltd}
set vendor(0040E5) {Sybus Corporation}
set vendor(0040E7) {Arnos Instruments & Computer}
set vendor(0040E9) {Accord Systems, Inc.}
set vendor(0040EA) {PlainTree Systems Inc}
set vendor(0040ED) {Network Controls International Inc}
set vendor(0040F0) {Micro Systems Inc}
set vendor(0040F1) {Chuo Electronics Co., Ltd.}
set vendor(0040F4) {Cameo Communications, Inc.}
set vendor(0040F5) {OEM Engines}
set vendor(0040F6) {Katron Computers Inc}
set vendor(0040F9) {Combinet}
set vendor(0040FA) {Microboards Inc}
set vendor(0040FB) {Cascade Communications Corp.}
set vendor(0040FD) {LXE}
set vendor(0040FF) {Telebit Corporation	Personal NetBlazer}
set vendor(00603E) {Cisco 100Mbps interface}
set vendor(006070) {Cisco routers (2524 and 4500)}
set vendor(00608C) {3Com (1990 onwards)}
set vendor(0060B0) {Hewlett-Packard}
set vendor(008000) {Multitech Systems Inc}
set vendor(008004) {Antlow Computers, Ltd.}
set vendor(008005) {Cactus Computer Inc.}
set vendor(008006) {Compuadd Corporation}
set vendor(008007) {Dlog NC-Systeme}
set vendor(00800D) {Vosswinkel FU}
set vendor(00800F) {SMC (Standard Microsystem Corp.)}
set vendor(008010) {Commodore}
set vendor(008012) {IMS Corp. IMS failure analysis tester}
set vendor(008013) {Thomas Conrad Corp.}
set vendor(008015) {Seiko Systems Inc}
set vendor(008016) {Wandel & Goltermann}
set vendor(008017) {PFU}
set vendor(008019) {Dayna Communications "Etherprint" product}
set vendor(00801A) {Bell Atlantic}
set vendor(00801B) {Kodiak Technology}
set vendor(008021) {Newbridge Networks Corporation}
set vendor(008023) {Integrated Business Networks}
set vendor(008024) {Kalpana}
set vendor(008026) {Network Products Corporation}
set vendor(008029) {Microdyne Corporation}
set vendor(00802A) {Test Systems & Simulations Inc}
set vendor(00802C) {The Sage Group PLC}
set vendor(00802D) {Xylogics, Inc. Annex terminal servers}
set vendor(00802E) {Plexcom, Inc.}
set vendor(008033) {Formation (?)}
set vendor(008034) {SMT-Goupil}
set vendor(008035) {Technology Works}
set vendor(008037) {Ericsson Business Comm.}
set vendor(008038) {Data Research & Applications}
set vendor(00803B) {APT Communications, Inc.}
set vendor(00803D) {Surigiken Co Ltd}
set vendor(00803E) {Synernetics}
set vendor(00803F) {Hyundai Electronics}
set vendor(008042) {Force Computers}
set vendor(008043) {Networld Inc}
set vendor(008045) {Matsushita Electric Ind Co}
set vendor(008046) {University of Toronto}
set vendor(008048) {Commodore (PC Ethercards - clones of SMC)}
set vendor(008049) {Nissin Electric Co Ltd}
set vendor(00804C) {Contec Co., Ltd.}
set vendor(00804D) {Cyclone Microsystems, Inc.}
set vendor(008051) {ADC Fibermux}
set vendor(008052) {Network Professor}
set vendor(008057) {Adsoft Ltd}
set vendor(00805A) {Tulip Computers International BV}
set vendor(00805B) {Condor Systems, Inc.}
set vendor(00805C) {Agilis(?)}
set vendor(00805F) {aq Computer Corporation}
set vendor(008060) {Network Interface Corporation}
set vendor(008062) {Interface Co.}
set vendor(008063) {Richard Hirschmann Gmbh & Co }
set vendor(008067) {Square D Company}
set vendor(008069) {Computone Systems}
set vendor(00806A) {ERI (Empac Research Inc.)}
set vendor(00806B) {Schmid Telecommunication }
set vendor(00806C) {Cegelec Projects Ltd}
set vendor(00806D) {Century Systems Corp.}
set vendor(00806E) {Nippon Steel Corporation}
set vendor(00806F) {Onelan Ltd}
set vendor(008071) {SAI Technology}
set vendor(008072) {Microplex Systems Ltd}
set vendor(008074) {Fisher Controls}
set vendor(008079) {Microbus Designs Ltd}
set vendor(00807B) {Artel Communications Corp.}
set vendor(00807C) {FiberCom}
set vendor(008082) {PEP Modular Computers Gmbh}
set vendor(008086) {Computer Generation Inc.}
set vendor(008087) {Okidata}
set vendor(00808A) {Summit (?)}
set vendor(00808B) {Dacoll Limited}
set vendor(00808C) {Frontier Software Development}
set vendor(00808D) {Westcove Technology BV}
set vendor(00808E) {Radstone Technology}
set vendor(008090) {Microtek International Inc}
set vendor(008092) {Japan Computer Industry, Inc.}
set vendor(008093) {Xyron Corporation}
set vendor(008094) {Sattcontrol AB}
set vendor(008096) {HDS (Human Designed Systems) X terminals}
set vendor(008098) {TDK Corporation}
set vendor(00809A) {Novus Networks Ltd}
set vendor(00809B) {Justsystem Corporation}
set vendor(00809D) {Datacraft Manufactur'g Pty Ltd}
set vendor(00809F) {Alcatel Business Systems}
set vendor(0080A1) {Microtest}
set vendor(0080A3) {Lantronix}
set vendor(0080A6) {Republic Technology Inc}
set vendor(0080A7) {Measurex Corp}
set vendor(0080AD) {CNet Technology (Telebit also reported, CNet claims that's an error)}
set vendor(0080AE) {Hughes Network Systems}
set vendor(0080AF) {Allumer Co., Ltd.}
set vendor(0080B1) {Softcom A/S}
set vendor(0080B2) {NET  (Network Equipment Technologies)}
set vendor(0080B6) {Themis corporation}
set vendor(0080BA) {Specialix (Asia) Pte Ltd}
set vendor(0080C0) {Penril Datability Networks}
set vendor(0080C2) {IEEE 802.1 Committee}
set vendor(0080C7) {Xircom, Inc.}
set vendor(0080C8) {D-Link (also Solectek Pocket Adapters)}
set vendor(0080C9) {Alberta Microelectronic Centre}
set vendor(0080CE) {Broadcast Television Systems}
set vendor(0080D0) {Computer Products International}
set vendor(0080D3) {Shiva Appletalk-Ethernet interface}
set vendor(0080D4) {Chase Limited}
set vendor(0080D6) {Apple Mac Portable(?)}
set vendor(0080D7) {Fantum Electronics}
set vendor(0080D8) {Network Peripherals}
set vendor(0080DA) {Bruel & Kjaer}
set vendor(0080E0) {XTP Systems Inc}
set vendor(0080E3) {Coral (?)}
set vendor(0080E7) {Lynwood Scientific Dev Ltd}
set vendor(0080EA) {The Fiber Company}
set vendor(0080F0) {Kyushu Matsushita Electric Co}
set vendor(0080F1) {Opus}
set vendor(0080F3) {Sun Electronics Corp}
set vendor(0080F4) {Telemechanique Electrique}
set vendor(0080F5) {Quantel Ltd}
set vendor(0080F7) {Zenith Communications Products}
set vendor(0080FB) {BVM Limited}
set vendor(0080FE) {Azure Technologies Inc}
set vendor(00A0D2) {Allied Telesyn}
set vendor(00A024) {3com (PCMCIA 3C589C cards, maybe others)}
set vendor(00A040) {Apple (PCI Mac)}
set vendor(00AA00) {Intel}
set vendor(00B0D0) {Computer Products International}
set vendor(00C000) {Lanoptics Ltd}
set vendor(00C001) {Diatek Patient Managment}
set vendor(00C002) {Sercomm Corporation}
set vendor(00C003) {Globalnet Communications}
set vendor(00C004) {Japan Business Computer Co.Ltd}
set vendor(00C005) {Livingston Enterprises Inc	Portmaster (OEMed by Cayman)}
set vendor(00C006) {Nippon Avionics Co Ltd}
set vendor(00C007) {Pinnacle Data Systems Inc}
set vendor(00C008) {Seco SRL}
set vendor(00C009) {KT Technology (s) Pte Inc}
set vendor(00C00A) {Micro Craft}
set vendor(00C00B) {Norcontrol A.S.}
set vendor(00C00D) {Advanced Logic Research Inc}
set vendor(00C00E) {Psitech Inc}
set vendor(00C00F) {QNX Software Systems Ltd. (also Quantum Software Systems Ltd)}
set vendor(00C011) {Interactive Computing Devices}
set vendor(00C012) {Netspan Corp}
set vendor(00C013) {Netrix}
set vendor(00C014) {Telematics Calabasas}
set vendor(00C015) {New Media Corp}
set vendor(00C016) {Electronic Theatre Controls}
set vendor(00C017) {e}
set vendor(00C018) {Lanart Corp}
set vendor(00C01A) {Corometrics Medical Systems}
set vendor(00C01B) {Socket Communications}
set vendor(00C01C) {Interlink Communications Ltd.}
set vendor(00C01D) {Grand Junction Networks, Inc.}
set vendor(00C01F) {S.E.R.C.E.L.}
set vendor(00C020) {Arco Electronic, Control Ltd.}
set vendor(00C021) {Netexpress}
set vendor(00C023) {Tutankhamon Electronics}
set vendor(00C024) {Eden Sistemas De Computacao SA}
set vendor(00C025) {Dataproducts Corporation}
set vendor(00C027) {Cipher Systems, Inc.}
set vendor(00C028) {Jasco Corporation}
set vendor(00C029) {Kabel Rheydt AG}
set vendor(00C02A) {Ohkura Electric Co}
set vendor(00C02B) {Gerloff Gesellschaft Fur}
set vendor(00C02C) {Centrum Communications, Inc.}
set vendor(00C02D) {Fuji Photo Film Co., Ltd.}
set vendor(00C02E) {Netwiz}
set vendor(00C02F) {Okuma Corp}
set vendor(00C030) {Integrated Engineering B. V.}
set vendor(00C031) {Design Research Systems, Inc.}
set vendor(00C032) {I-Cubed Limited}
set vendor(00C033) {Telebit Corporation}
set vendor(00C034) {Dale Computer Corporation}
set vendor(00C035) {Quintar Company}
set vendor(00C036) {Raytech Electronic Corp}
set vendor(00C039) {Silicon Systems}
set vendor(00C03B) {Multiaccess Computing Corp}
set vendor(00C03C) {Tower Tech S.R.L.}
set vendor(00C03D) {Wiesemann & Theis Gmbh}
set vendor(00C03E) {Fa. Gebr. Heller Gmbh}
set vendor(00C03F) {Stores Automated Systems Inc}
set vendor(00C040) {ECCI}
set vendor(00C041) {Digital Transmission Systems}
set vendor(00C042) {Datalux Corp.}
set vendor(00C043) {Stratacom}
set vendor(00C044) {Emcom Corporation}
set vendor(00C045) {Isolation Systems Inc}
set vendor(00C046) {Kemitron Ltd}
set vendor(00C047) {Unimicro Systems Inc}
set vendor(00C048) {Bay Technical Associates}
set vendor(00C049) {obotics Total Control (tm) NETServer Card}
set vendor(00C04D) {Mitec Ltd}
set vendor(00C04E) {Comtrol Corporation}
set vendor(00C050) {Toyo Denki Seizo K.K.}
set vendor(00C051) {Advanced Integration Research}
set vendor(00C055) {Modular Computing Technologies}
set vendor(00C056) {Somelec}
set vendor(00C057) {Myco Electronics}
set vendor(00C058) {Dataexpert Corp}
set vendor(00C059) {Nippondenso Corp}
set vendor(00C05B) {Networks Northwest Inc}
set vendor(00C05C) {Elonex PLC}
set vendor(00C05D) {L&N Technologies}
set vendor(00C05E) {Vari-Lite Inc}
set vendor(00C060) {ID Scandinavia A/S}
set vendor(00C061) {Solectek Corporation}
set vendor(00C063) {Morning Star Technologies Inc}
set vendor(00C064) {General Datacomm Ind Inc}
set vendor(00C065) {Scope Communications Inc}
set vendor(00C066) {Docupoint, Inc.}
set vendor(00C067) {United Barcode Industries}
set vendor(00C068) {Philp Drake Electronics Ltd}
set vendor(00C069) {California Microwave Inc}
set vendor(00C06A) {Zahner-Elektrik Gmbh & Co KG}
set vendor(00C06B) {OSI Plus Corporation}
set vendor(00C06C) {SVEC Computer Corp}
set vendor(00C06D) {Boca Research, Inc.}
set vendor(00C06F) {Komatsu Ltd}
set vendor(00C070) {Sectra Secure-Transmission AB}
set vendor(00C071) {Areanex Communications, Inc.}
set vendor(00C072) {KNX Ltd}
set vendor(00C073) {Xedia Corporation}
set vendor(00C074) {Toyoda Automatic Loom Works Ltd}
set vendor(00C075) {Xante Corporation	}
set vendor(00C076) {I-Data International A-S}
set vendor(00C077) {Daewoo Telecom Ltd}
set vendor(00C078) {Computer Systems Engineering}
set vendor(00C079) {Fonsys Co Ltd}
set vendor(00C07A) {Priva BV}
set vendor(00C07B) {Ascend Communications ISDN bridges/routers}
set vendor(00C07D) {RISC Developments Ltd}
set vendor(00C07F) {Nupon Computing Corp}
set vendor(00C080) {Netstar Inc}
set vendor(00C081) {Metrodata Ltd}
set vendor(00C082) {Moore Products Co}
set vendor(00C084) {Data Link Corp Ltd}
set vendor(00C085) {Canon}
set vendor(00C086) {The Lynk Corporation}
set vendor(00C087) {UUNET Technologies Inc}
set vendor(00C089) {Telindus Distribution}
set vendor(00C08A) {Lauterbach Datentechnik Gmbh}
set vendor(00C08B) {RISQ Modular Systems Inc}
set vendor(00C08C) {Performance Technologies Inc}
set vendor(00C08D) {Tronix Product Development}
set vendor(00C08E) {Network Information Technology}
set vendor(00C08F) {Matsushita Electric Works, Ltd.}
set vendor(00C090) {Praim S.R.L.}
set vendor(00C091) {Jabil Circuit, Inc.}
set vendor(00C092) {Mennen Medical Inc}
set vendor(00C093) {Alta Research Corp.}
set vendor(00C095) {Zynx Network Appliance box}
set vendor(00C096) {Tamura Corporation}
set vendor(00C097) {Archipel SA}
set vendor(00C098) {Chuntex Electronic Co., Ltd.}
set vendor(00C09B) {Reliance Comm/Tec, R-Tec Systems Inc}
set vendor(00C09C) {TOA Electronic Ltd}
set vendor(00C09D) {Distributed Systems Int'l, Inc.}
set vendor(00C09F) {Quanta Computer Inc}
set vendor(00C0A0) {Advance Micro Research, Inc.}
set vendor(00C0A1) {Tokyo Denshi Sekei Co}
set vendor(00C0A2) {Intermedium A/S}
set vendor(00C0A3) {Dual Enterprises Corporation}
set vendor(00C0A4) {Unigraf OY}
set vendor(00C0A7) {SEEL Ltd}
set vendor(00C0A8) {GVC Corporation}
set vendor(00C0A9) {Barron McCann Ltd}
set vendor(00C0AA) {Silicon Valley Computer}
set vendor(00C0AB) {Jupiter Technology Inc}
set vendor(00C0AC) {Gambit Computer Communications}
set vendor(00C0AD) {Computer Communication Systems}
set vendor(00C0AE) {Towercom Co Inc DBA PC House}
set vendor(00C0B0) {GCC Technologies,Inc.}
set vendor(00C0B2) {Norand Corporation}
set vendor(00C0B3) {Comstat Datacomm Corporation}
set vendor(00C0B4) {Myson Technology Inc}
set vendor(00C0B5) {Corporate Network Systems Inc}
set vendor(00C0B6) {Meridian Data Inc}
set vendor(00C0B7) {American Power Conversion Corp}
set vendor(00C0B8) {Fraser's Hill Ltd.}
set vendor(00C0B9) {Funk Software Inc}
set vendor(00C0BA) {Netvantage}
set vendor(00C0BB) {Forval Creative Inc}
set vendor(00C0BD) {Inex Technologies, Inc.}
set vendor(00C0BE) {Alcatel - Sel}
set vendor(00C0BF) {Technology Concepts Ltd}
set vendor(00C0C0) {Shore Microsystems Inc}
set vendor(00C0C1) {Quad/Graphics Inc}
set vendor(00C0C2) {Infinite Networks Ltd.}
set vendor(00C0C3) {Acuson Computed Sonography}
set vendor(00C0C4) {Computer Operational}
set vendor(00C0C5) {SID Informatica}
set vendor(00C0C6) {Personal Media Corp}
set vendor(00C0C8) {Micro Byte Pty Ltd}
set vendor(00C0C9) {Bailey Controls Co}
set vendor(00C0CA) {Alfa, Inc.}
set vendor(00C0CB) {Control Technology Corporation}
set vendor(00C0CD) {Comelta S.A.}
set vendor(00C0D0) {Ratoc System Inc}
set vendor(00C0D1) {Comtree Technology Corporation (EFA also reported)}
set vendor(00C0D2) {Syntellect Inc}
set vendor(00C0D4) {Axon Networks Inc}
set vendor(00C0D5) {Quancom Electronic Gmbh}
set vendor(00C0D6) {J1 Systems, Inc.}
set vendor(00C0D9) {Quinte Network Confidentiality Equipment Inc}
set vendor(00C0DB) {IPC Corporation (Pte) Ltd}
set vendor(00C0DC) {EOS Technologies, Inc.}
set vendor(00C0DE) {ZComm Inc}
set vendor(00C0DF) {Kye Systems Corp}
set vendor(00C0E1) {Sonic Solutions}
set vendor(00C0E2) {Calcomp, Inc.}
set vendor(00C0E3) {Ositech Communications Inc}
set vendor(00C0E4) {Landis & Gyr Powers Inc}
set vendor(00C0E5) {GESPAC S.A.}
set vendor(00C0E6) {TXPORT}
set vendor(00C0E7) {Fiberdata AB}
set vendor(00C0E8) {Plexcom Inc}
set vendor(00C0E9) {Oak Solutions Ltd}
set vendor(00C0EA) {Array Technology Ltd.}
set vendor(00C0EC) {Dauphin Technology}
set vendor(00C0ED) {US Army Electronic Proving Ground}
set vendor(00C0EE) {Kyocera Corporation}
set vendor(00C0EF) {Abit Corporation}
set vendor(00C0F1) {Shinko Electric Co Ltd}
set vendor(00C0F2) {Transition Engineering Inc}
set vendor(00C0F3) {Network Communications Corp}
set vendor(00C0F4) {Interlink System Co., Ltd.}
set vendor(00C0F5) {Metacomp Inc}
set vendor(00C0F6) {Celan Technology Inc.}
set vendor(00C0F7) {Engage Communication, Inc.}
set vendor(00C0F8) {About Computing Inc.}
set vendor(00C0FA) {Canary Communications Inc}
set vendor(00C0FB) {Advanced Technology Labs}
set vendor(00C0FC) {ASDG Incorporated}
set vendor(00C0FD) {Prosum}
set vendor(00C0FF) {Box Hill Systems Corporation}
set vendor(00DD00) {Ungermann-Bass		IBM RT}
set vendor(00DD01) {Ungermann-Bass}
set vendor(00DD08) {Ungermann-Bass}
set vendor(020406) {BBN (Bolt Beranek and Newman, Inc.)	internal usage (not registered)}
set vendor(020701) {MICOM/Interlan DEC (UNIBUS or QBUS), Apollo, Cisco}
set vendor(020701) {Racal-Datacom}
set vendor(026060) {3Com}
set vendor(026086) {Satelcom MegaPac (UK)}
set vendor(02608C) {3Com IBM PC; Imagen; Valid; Cisco; Macintosh}
set vendor(02CF1F) {CMC Masscomp; Silicon Graphics; Prime EXL}
set vendor(02E6D3) {BTI (Bus-Tech, Inc.) IBM Mainframes}
set vendor(080001) {Computer Vision}
set vendor(080002) {3Com (formerly Bridge)}
set vendor(080003) {ACC (Advanced Computer Communications)}
set vendor(080005) {Symbolics Symbolics LISP machines}
set vendor(080006) {Siemens Nixdorf PC clone}
set vendor(080007) {Apple}
set vendor(080008) {BBN (Bolt Beranek and Newman, Inc.)}
set vendor(080009) {Hewlett-Packard}
set vendor(08000A) {Nestar Systems}
set vendor(08000B) {Unisys}
set vendor(08000D) {ICL (International Computers, Ltd.)}
set vendor(08000E) {NCR/AT&T}
set vendor(08000F) {SMC (Standard Microsystems Corp.)}
set vendor(080010) {AT&T [misrepresentation of 800010?]}
set vendor(080011) {Tektronix, Inc.}
set vendor(080014) {Excelan BBN Butterfly, Masscomp, Silicon Graphics}
set vendor(080017) {National Semiconductor Corp. (used to have Network System Corp., wrong NSC)}
set vendor(08001A) {Tiara? (used to have Data General)}
set vendor(08001B) {Data General}
set vendor(08001E) {Apollo}
set vendor(08001F) {Sharp}
set vendor(080020) {Sun}
set vendor(080022) {NBI (Nothing But Initials)}
set vendor(080023) {Matsushita Denso}
set vendor(080025) {CDC}
set vendor(080026) {Norsk Data (Nord)}
set vendor(080027) {PCS Computer Systems GmbH}
set vendor(080028) {TI Explorer}
set vendor(08002B) {DEC}
set vendor(08002E) {Metaphor}
set vendor(08002F) {Prime Computer Prime 50-Series LHC300}
set vendor(080030) {CERN}
set vendor(080032) {Tigan}
set vendor(080036) {Intergraph CAE stations}
set vendor(080037) {Fuji Xerox}
set vendor(080038) {Bull}
set vendor(080039) {Spider Systems}
set vendor(08003B) {Torus Systems}
set vendor(08003D) {cadnetix}
set vendor(08003E) {Motorola VME bus processor modules}
set vendor(080041) {DCA (Digital Comm. Assoc.)}
set vendor(080044) {DSI (DAVID Systems, Inc.)}
set vendor(080045) {???? (maybe Xylogics, but they claim not to know this number)}
set vendor(080046) {Sony}
set vendor(080047) {Sequent}
set vendor(080048) {Eurotherm Gauging Systems}
set vendor(080049) {Univation}
set vendor(08004C) {Encore}
set vendor(08004E) {BICC (3com also reported)}
set vendor(080051) {Experdata}
set vendor(080056) {Stanford University}
set vendor(080057) {Evans & Sutherland (?)}
set vendor(080058) {??? DECsystem-20}
set vendor(08005A) {IBM}
set vendor(080066) {AGFA printers, phototypesetters etc.}
set vendor(080067) {Comdesign}
set vendor(080068) {Ridge}
set vendor(080069) {Silicon Graphics}
set vendor(08006A) {ATTst (?)}
set vendor(08006E) {Excelan}
set vendor(080070) {Mitsubishi}
set vendor(080074) {Casio}
set vendor(080075) {DDE (Danish Data Elektronik A/S)}
set vendor(080077) {TSL (now Retix)}
set vendor(080079) {Silicon Graphics}
set vendor(08007C) {Vitalink TransLAN III}
set vendor(080080) {XIOS}
set vendor(080081) {Crosfield Electronics}
set vendor(080083) {Seiko Denshi }
set vendor(080086) {Imagen/QMS}
set vendor(080087) {Xyplex			terminal servers}
set vendor(080088) {McDATA Corporation}
set vendor(080089) {Kinetics AppleTalk-Ethernet interface}
set vendor(08008B) {Pyramid}
set vendor(08008D) {XyVision XyVision machines}
set vendor(08008E) {Tandem / Solbourne Computer ?}
set vendor(08008F) {Chipcom Corp.}
set vendor(080090) {Retix, Inc. Bridges}
set vendor(09006A) {AT&T}
set vendor(10005A) {IBM}
set vendor(100090) {Hewlett-Packard Advisor products}
set vendor(1000D4) {DEC}
set vendor(1000E0) {Apple A/UX (modified addresses for licensing)}
set vendor(3C0000) {3Com dual function (V.34 modem + Ethernet) card}
set vendor(400003) {Net Ware (?)}
set vendor(444649) {DFI (Diamond Flower Industries)}
set vendor(475443) {GTC (Not registered!) (This number is a multicast!)}
set vendor(484453) {HDS ???}
set vendor(484C00) {Network Solutions}
set vendor(4C424C) {rmation Modes software modified addresses (not registered?)}
set vendor(800010) {AT&T [misrepresented as 080010? One source claims this is correct]}
set vendor(80AD00) {CNET Technology Inc. (Probably an error, see instead 0080AD)}
set vendor(AA0000) {DEC obsolete}
set vendor(AA0001) {DEC obsolete}
set vendor(AA0002) {DEC obsolete}
set vendor(AA0003) {DEC Global physical address for some DEC machines}
set vendor(AA0004) {DEC Local logical address for systems running DECNET}
set vendor(C00000) {Western Digital (may be reversed 00 00 C0?)}
set vendor(EC1000) {Enance Source Co., Ltd. PC clones(?)}

}
