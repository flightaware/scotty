#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# ip_layout.tcl -
#
#	This file contains code to query name servers and to automatically
#	layout an IP network. This tool also provides utilities to set the 
#	name, address, icon of a network node. The layout algorithm used in
#	this script is fast but it usually create only sub-optimal graphs.
#	Fast, heuristic algorithms for "nice" graph-layout are actually an
#	interesting field of research.
#
# Copyright (c) 1993-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1997-1998 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: ip_layout.tcl,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $

package require Tnm 3.0

namespace import Tnm::*

ined size
LoadDefaults layout
IpInit IP-Trouble
SnmpInit IP-Trouble

mib load RFC1213-MIB

set gridx 65
set gridy 50
set columns 8
set report false

set default(icon.SUN.*4/370)   	"Sparc Server"
set default(icon.SUN.*20)      	"Sparc Station"
set default(icon.SUN.*10)      	"Sparc Station"
set default(icon.SUN.*5)      	"Sparc Station"
set default(icon.SUN.*SS2)     	"Sparc Station"
set default(icon.SUN.*4/75)    	"Sparc Station"
set default(icon.SUN.*IPX)     	"Sparc IPC / IPX"
set default(icon.SUN.*IPC)     	"Sparc IPC / IPX"
set default(icon.SUN.*4/65)    	"Sparc IPC / IPX"
set default(icon.SUN.*4/60)    	"Sparc IPC / IPX"
set default(icon.SUN.*4/45)    	"Sparc IPC / IPX"
set default(icon.SUN.*4/40)    	"Sparc IPC / IPX"
set default(icon.SUN.*ELC)     	"Sparc SLC / ELC"
set default(icon.SUN.*SLC)     	"Sparc SLC / ELC"
set default(icon.SUN.*4/25)    	"Sparc SLC / ELC"
set default(icon.SUN.*4/20)    	"Sparc SLC / ELC"
set default(icon.SUN.*3/50)    	"Sun 3"
set default(icon.SUN.*3/60)    	"Sun 3"
set default(icon.SUN.*3/80)    	"Sun 3"
set default(icon.PYRAMID.*90)  	"Host / Server"
set default(icon.X.*Term)      	"X-Terminal"
set default(icon.X.*Terminal)  	"X-Terminal"
set default(icon.PC.*ompatibel) "PC"
set default(icon.PC)            "PC"
set default(icon.PC.*AT)        "PC"
set default(icon.PC.*\[34\]86)  "PC"
set default(icon.i\[34\]86)     "UNIX-PC"
set default(icon..*Linux.*)     "UNIX-PC"
set default(icon..*laserjet.*)  "Printer"
set default(icon..*JET.*)	"Printer"
set default(icon.hp.*)          "HP WS"
set default(icon..*HP-UX.*)	"HP WS"
set default(icon.ibm.*ws)       "RS 6000"
set default(icon.rs6000)        "RS 6000"
set default(icon.cisco)		"Router"
set default(icon.DS3100)	"DEC System"
set default(icon.DS5000)	"DEC Server"
set default(icon.mac)		"Macintosh"
set default(icon.modem)		"Modem"
set default(icon.SynOp)		"Router"
set default(icon.DEC.*OSF)	"DEC System"

##
## Set the name of all node objects.
##

proc "Set Name" { list } {

    global report

    foreach comp $list {
	if {[lsearch "NODE STRIPCHART BARCHART" [ined type $comp]] >= 0} {
	    set id [ined id $comp]
	    set host [ined name $comp]
	    set ip [lindex [ined -noupdate address $id] 0]
	    if {$ip == ""} {
		if {$report == "true"} {
		    writeln "No IP Address set for $host."
		}
		continue;
	    }
	    if {[catch {dns name $ip} host]} {
		if {[catch {nslook $ip} host]} {
		    if {$report == "true"} {
			writeln "Can not lookup name for \[$ip\]."
		    }
		    continue
		}
	    }
	    ined -noupdate name $id [string tolower [lindex $host 0]]
	    ined label $id name
	}
    }
}

##
## Set the address of all node objects.
##

proc "Set Address" { list } {
    
    global report
    
    foreach comp $list {
	if {[lsearch "NODE STRIPCHART BARCHART" [ined type $comp]] >= 0} {
	    set id [ined id $comp]
	    set host [ined name $comp]
	    if {$host == ""} {
		if {$report == "true"} {
		    writeln "No name set for $host."
		}
		continue
	    }
	    if {[regexp {^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$} $host]} {
		ined -noupdate address $id $host
		ined label $id address
		continue
	    }
	    if {[catch {nslook $host} ip] == 0} {
		ined -noupdate address $id [lindex $ip 0]
		ined label $id address
	    } else {
		if {$report == "true"} {
		    writeln "Can not lookup address for \[$host\]."
		}
	    }
	}
    }
}

##
## Try to set the icon of all node objects. This procedure needs
## more support from scotty to query the name servers directly.
## Support is added to query by description from the SNMP agent.
##

proc "Set Icon" { list } {

    global default report
    
    foreach comp $list {
	if {[ined type $comp] != "NODE"} continue
	set id [ined id $comp]
	set host [lindex [ined name $comp] 0]
	set ip [GetIpAddress $comp]
	if {[catch {dns name $ip} name]} { set name "" }
	if {$name == ""} { set name $host }
	if {[catch {dns hinfo $name} hinfo]} {
	    set s [SnmpOpen $comp $ip]
	    if {[catch {$s get sysDescr.0} hinfo]} {
		$s destroy
		continue
	    }
	    set hinfo [list [lindex [lindex $hinfo 0] 2]]
	    $s destroy
	}
	set cpu [lindex $hinfo 0]
	set icon ""
	foreach name [array names default] {
	    set regex [join [lrange [split $name .] 1 end] .]
	    if {[regexp -nocase [string tolower $regex] $cpu]} {
		set icon $default($name)
		break
	    }
	}
	if {$icon != ""} {
	    ined icon $id $icon
	} else {
	    if {$report == "true"} {
		writeln "No icon mapping for host $host ($cpu)."
	    }
	}
    }
}

##
## For all nodes try to reduce the domain name to the unique prefix.
##

proc "Unique Name" { list } {
    set idlist ""
    foreach comp $list {
	set type [ined type $comp]
	if {$type == "NODE" || $type == "STRIPCHART" || $type == "BARCHART"} {
	    lappend idlist [ined id $comp]
	}
    }
    if {[llength $idlist]==0} return
    foreach nodeid $idlist {
	set name($nodeid) ""
	foreach p [split [lindex [ined name $nodeid] 0] "."] {
	    set name($nodeid) "$p $name($nodeid)"
	}
    }
    set unique 1
    while {$unique} {
	if {[llength $name($nodeid)]<=1} break
	set pfx [lindex $name($nodeid) 0]
	foreach id [array names name] {
	    if {$pfx != [lindex $name($id) 0]} {
		set unique 0
		break
	    }
	}
	if {$unique} {
	    foreach id [array names name] {
		set name($id) [lrange $name($id) 1 end]
	    }
	} 
    }
    foreach id [array names name] {
	set new ""
	foreach p $name($id) { set new "$p.$new" }
	set new [string trimright $new "."]
	ined name $id $new
    }
}

##
## Move the object given by id to a new absolute position.
##

proc position { id abs_x abs_y } {
    set xy [ined -noupdate move $id]
    ined -noupdate move $id \
	[expr {$abs_x-[lindex $xy 0]}] [expr {$abs_y-[lindex $xy 1]}]
}

##
## move all objects to a grid centered around net_x and net_y
## with a grid spacing set to grid_x and grid_y. Put columns
## objects in one row.
##

proc layout_grid { objects net_x net_y grid_x grid_y columns } {
    set y1 $net_y
    set y2 $net_y
    set i 0
    set x $net_x
    foreach node $objects {
	if {! ($i%(2*$columns))} {
	    set net_x [expr {$net_x+2}]
	    set y1 [expr {$y1+$grid_y}]
	    set y2 [expr {$y2-$grid_y}]
	    set x1 [expr {$net_x+($grid_x/2)}]
	    set x2 [expr {$net_x-($grid_x/2)}]
	    set x $x1
	    set y $y1
	}
	position $node $x $y
	if {$y == $y1} {
	    set y $y2
	} else {
	    set y $y1
	    if {$x == $x1} {
		set x $x2
	    } else {
		set x1 [expr {$x1+$grid_x}]
		set x2 [expr {$x2-$grid_x}]
		set x $x1
	    }
	}
	incr i
    }
}

##
## Move all nodes connected to network in the area of the
## network. We simply put them in a grid around the network
## position.
##

proc "Layout Network" { list } {
    global gridx gridy columns
    foreach comp $list {
	if {[ined type $comp] == "NETWORK"} {
	    set id [ined id $comp]
	    set size [ined -noupdate size $id]
	    set net_x [expr {([lindex $size 0]+[lindex $size 2])/2}]
	    set net_y [expr {([lindex $size 1]+[lindex $size 3])/2}]
	    set nodes [neighbours $id]
	    set single ""
	    set gates  ""
	    foreach node $nodes {
		set links [ined links [ined retrieve $node]]
		if {[llength $links] == 1} {
		    lappend single $node
		} else {
		    lappend gates $node
		}
	    }
	    set nodes [join [list $single $gates]]
	    layout_grid $nodes $net_x $net_y $gridx $gridy $columns
	}
    }
}

##
## Make groups. Get all nodes connected to a network and throw
## them into a group named like the network. Exclude nodes with
## more than one link. This command returns a list of ids containing
## the new group ids and the node ids with more than one link.
##

proc "Group Network" { list } {

    set result ""
    foreach comp $list {
	set id [ined id $comp]
	set type [ined type $comp]
	if {($type == "NODE") && ([llength [ined links $comp]] > 0)} {
	    continue
	}
	if {$type != "NETWORK"} {
	    lappend result $id
	    continue
	}
	set member $id
	foreach n [neighbours $id] {
	    set links [ined links [ined retrieve $n]]
	    if {[llength $links] == 1} {
		lappend member $n
		lappend member [lindex $links 0]
	    } else {
		lappend result $n
	    }
	}
	set group [eval ined -noupdate create GROUP $member]
	ined -noupdate name $group [ined name $comp]
	ined -noupdate icon $group Bus
	ined -noupdate label $group name
	ined -noupdate color $group [ined color $id]
	ined -noupdate font  $group [ined font  $id]
	lappend result $group
    }
    
    return $result
}

##
## Get all neighbours of a NETWORK or a NODE object that are
## connected using a direct link. Return the list of ids.
##

proc neighbours { id } {
    set object [ined retrieve $id]
    set type [ined type $object]
    if {($type != "NETWORK") && ($type != "NODE")} { 
	error "illegal object type"
    }
    set links [ined links $object]
    set result ""
    foreach link $links {
	set comp [ined retrieve $link]
	set src [ined src $comp]
	set dst [ined dst $comp]
	if {$src == $id} {
	    append result " $dst"
	} else {
	    append result " $src"
	}
    }
    return $result
}

##
## Call all of the above tools in turn. The group network command
## returns a list of ids that should be viewed by the final layout
## algorithm. We have to expand this id list to conform to the tkined
## calling convention.
##

proc "All of Above" { list } {

    "Set Address"    $list
    "Set Name"       $list
    "Set Icon"       $list
    "Unique Name"    $list
    "Layout Network" $list

    set groups ["Group Network"  $list]
    set list ""
    foreach id $groups {
	lappend list [ined retrieve $id]
    }

    "Layout Nodes & Groups" $list
}

##
## This simple dialog allows us to modify the layout
## parameters on the fly.
##

proc "Set Parameter" { list } {

    global gridx gridy columns report

    global radius demo

    set result [ined request "Layout Parameter" \
        [list [list "Horizontal grid space:" $gridx scale 20 160] \
	      [list "Vertical grid space:"   $gridy scale 20 160] \
	      [list "Nodes per row:"         $columns scale 10 40] \
	      [list "Radius:"                $radius  scale 60 120] \
	      [list "Animation:"             $demo radio true false] \
	      [list "Write report:"          $report radio true false] ] \
	[list "set values" cancel] ]

    if {[lindex $result 0] == "cancel"} return
    
    set gridx   [lindex $result 1]
    set gridy   [lindex $result 2]
    set columns [lindex $result 3]
    set radius  [lindex $result 4]
    set demo    [lindex $result 5]
    set report  [lindex $result 6]
}

##
## Show the defaults as loaded from the tkined.defaults files.
##

proc "Show Defaults" {list} {
    ShowDefaults
}

##
## Display some help about this tool.
##

proc "Help IP-Layout" { list } {

    ined browse "Help about IP-Layout" {
	"Set Name:" 
	"    Query the Domain Name Service and set the name of all " 
	"    selected nodes." 
	"" 
	"Set Address:" 
	"    Query the Domain Name Service and set the address of all " 
	"    selected nodes." 
	"" 
	"Set Icon:" 
	"    Query the Domain Name Service and set the icon of all " 
	"    selected nodes based on the HINFO record." 
	"" 
	"Unique Name:" 
	"    Shorten the names of all selected objects by removing " 
	"    common domain name endings."  
	"" 
	"Layout Network:" 
	"    Position all nodes connected to a network around it." 
	"" 
	"Group Networks:" 
	"    Throw all objects connected to network into a group." 
	"" 
	"Layout Nodes & Groups:" 
	"    Run a routing algorithm to position node and group objects" 
	"    on the page. The algorithm never takes back a decision which" 
	"    makes it fast but it will generate sub-optimial solutions." 
	"" 
	"Set Parameter:" 
	"    Set the parameter that control the layout algorithm." 
	"    The grid parameters control the space allocated while" 
	"    moving objects around a network. The nodes per row variable" 
	"    sets the max. number of nodes in a single row." 
	"" 
	"    The radius parameter sets the size occupied by a node or" 
	"    group object during the routing algorithm. You can see an" 
	"    animation of the routing algorithm by setting animation to" 
	"    true." 
	"" 
	"Show Defaults:" 
	"    Show the defaults that may be defined in the tkined.defaults" 
	"    files. This script makes use of definitions in the form" 
	"" 
	"        layout.icon.<regex>:      <icon-name>" 
	"" 
	"    where <ip> is an IP address in dot notation." 
    } 
}

##
## Delete the menus created by this interpreter.
##

proc "Delete IP-Layout" { list } {
    
    global menus
    foreach id $menus {	ined delete $id }
    exit
}

set menus [ ined create MENU "IP-Layout" \
	    "Set Name" "Set Address" "Set Icon" "" \
	    "Unique Name" "" \
	    "Layout Network" "Group Network" "Layout Nodes & Groups" "" \
	    "All of Above" "" \
	    "Set Parameter" "Show Defaults" "" \
	    "Help IP-Layout" "Delete IP-Layout" ]

## --------------------------------------------------------------------------

##
## Here starts the layout algorithm that was written by Sascha Bengsch.
## The comments are in german. I will change them once I have time to
## clean up this code.
##

set demo false
set radius 70

proc "Layout Nodes & Groups" { liste } {

    global objektarray
    global objekte
    set ergebnis 1

    ## Initialisierung aller globalen Parameter
    ##
    parameterIni

    ## 'objekte' ist eine Liste, die die ID's aller Bildschirmelemente enthaelt
    ##
    set objekte [erstelleObjektliste]

    if {([llength $liste] > 0) && ([llength $objekte] > 0)} {

	## Alle selektierten Elemente markieren und ihre Nachbarn nach Anzahl 
	## der Links sortieren
	##
	foreach element $liste {
	    
	    set typ [ined type $element]
	    
	    ## Von den selektierten Elementen, werden nur Knoten und Gruppen 
	    ## beruecksichtigt
	    ##
	    if {($typ== "NODE") || ($typ == "GROUP")} {
		set id [ined id $element]
		set objektarray($id,gesetzt) 0
		if {$objektarray($id,anznachbarn) > 0} {
		    set ende [expr {$objektarray($id,anznachbarn)-1}]
		    quicksort $id 0 $ende
		    lappend elemente $element
		} else {
		    lappend single $element
		} 
	    }
	}
	
	## Bildschirmbereiche aller nichtselektierten Elemente markieren
	##
	foreach id $objekte {
	    if {$objektarray($id,gesetzt) != 0} {
		positionMarkieren $id
		bestAusdehnung $id
	    }
	}

	## Starten des Algorithmus und plazieren aller selektierten Elemente 
	## (momentan werden nur Knoten und Gruppen beruecksichtigt)
	##
	if {[info exists elemente]} {
	    set ergebnis [starteAlgorithmus $elemente]
	}

	## Alle einzelnene Icons setzen
	##
	if {[info exists single] && ($ergebnis != "fehler")} {
	    singleIcons $single
	}
    }
}

############################################################################
##
##	Funktionen, die zum Erstellen der Listen fuer den Algorithmus
##	benoetigt werden
##
############################################################################


##
## parameterIni -	Initialisiert alle globalen Parameter
##

proc parameterIni {} {

    global bildschirm
    global ausdehnung
    global objektarray
    global objekte

    if {[info exists bildschirm]} {
	unset bildschirm
    }

    if {[info exists objektarray]} {
	unset objektarray
    }

    if {[info exists ausdehnung]} {
	unset ausdehnung
    }
    
    set objekte ""
}

##
## starteAlgorithmus  -	startet den Algorithmus und fuert ihn solange aus, 
##			bis alle selektierten Elemnte plaziert sind
##

proc starteAlgorithmus { elemente } {

    global objektarray

    while {[llength $elemente] > 0} {
	
	## Element mit den meisten Links suchen
	##
	set ee [sucheEE $elemente]
	
	## Plazieren der Elemente auf dem Bildschirm
	##
	set ergebnis [setzeEE $ee]

	if {$ergebnis == "fehler"} {
	    return $ergebnis
	}

	## Erstellen einer Objektarray aller nichtgesetzten Elemente
	##
	set elemente [sucheElemente $elemente]
    }

    return 1
}

##
## sucheElemente   -	erstellt eine Liste aller Elemente, die noch nicht 
##			gesetz sind 
##

proc sucheElemente { elemente } {

    global objektarray
    set netzwerk ""
    set single ""

    ## Alle selektierten Elemente durchlaufen
    ## 
    foreach element $elemente {
	
	set id [ined id $element]
	
	## Element noch nicht gesetzt
	##
	if {$objektarray($id,gesetzt) == 0} {
	    
	    if {$objektarray($id,anzlinks) > 0} {

		## Liste segmentierter Netzwerk
		##
		lappend netzwerk $element
	    }
	}
    }

    ## Liste aller noch nicht gesetzen Netzwerke
    ##
    return $netzwerk
}

##
## erstelleObjektliste - erstellt eine Liste aller Bildschirmelemente, eine 
## 			 Tabelle, die zu jedem Element einer Gruppe, die Grup-
##			 penID angibt und die Nachbarliste der einzelnen 
##			 Elemente
##

proc erstelleObjektliste {} {

    global objektarray
    set objekte ""
    set links ""
    global radius 
    set obj ""

    ## 'elemente' mit allen Bildschirmobjekten belegen
    ##
    set elemente [ined -noupdate retrieve]

    ## Alle Bildschirmelemente bearbeiten
    ##
    foreach element $elemente {

	set id [ined id $element]

	switch [ined type $element] {
	    
	    LINK { 
		lappend links $element 
	    } 
	    GROUP { 
		## radius in Abhaengigkeit von den Ausmassen des Groessten 
		## Elements bestimmen
##		bestRadius $id 
		lappend objekte $id
		set objektarray($id,anzlinks) 0
		set objektarray($id,anznachbarn) 0
		set objektarray($id,gesetzt) 2
		set objektarray($id,halbe) [bestHalbe $id]
		erstelleTabelle $id $id tabelle
	    }
	    NODE {
		## radius in Abhaengigkeit von den Ausmassen des Groessten 
		## Elements bestimmen
##		bestRadius $id 
		lappend objekte $id
		set objektarray($id,anzlinks) 0
		set objektarray($id,anznachbarn) 0
		set objektarray($id,gesetzt) 2
		set objektarray($id,halbe) [bestHalbe $id]
	    }
	    NETWORK {
		lappend objekte $id
		set objektarray($id,anzlinks) 0
		set objektarray($id,anznachbarn) 0
		set objektarray($id,halbe) [bestHalbe $id]
		set objektarray($id,gesetzt) 2
	    }
	    TEXT {
		lappend objekte $id
		set objektarray($id,anzlinks) 0
		set objektarray($id,anznachbarn) 0
		set objektarray($id,halbe) [bestHalbe $id]
		set objektarray($id,gesetzt) 2
	    }
	    REFERENCE {
		lappend objekte $id
		set objektarray($id,anzlinks) 0
		set objektarray($id,anznachbarn) 0
		set objektarray($id,halbe) [bestHalbe $id]
		set objektarray($id,gesetzt) 2
	    }
	    STRIPCHART {
		lappend objekte $id
		set objektarray($id,anzlinks) 0
		set objektarray($id,anznachbarn) 0
		set objektarray($id,halbe) [bestHalbe $id]
		set objektarray($id,gesetzt) 2
	    }
	    BARCHART {
		lappend objekte $id
		set objektarray($id,anzlinks) 0
		set objektarray($id,anznachbarn) 0
		set objektarray($id,halbe) [bestHalbe $id]
		set objektarray($id,gesetzt) 2
	    }
	}
    }
    
    if {[llength $links] > 0} {

	## Nachbarn eines jeden Elements bestimmen
	##
	bestimmeNachbarn $links tabelle 
    }

    foreach element $objekte {
	if {![info exists tabelle($element)]} {
	    lappend obj $element
	}
    }
    return $obj
}

## 
## bestRadius    -	bestimmt den Radius zum Setzen der Elemente in 
##			Abhaengigkeit vom Ausmass des Groessten Elements
##

proc bestRadius { id } {

    global radius

    set a [ined -noupdate size $id]
    set x [expr {[lindex $a 2] - [lindex $a 0]}]
    set y [expr {[lindex $a 3] - [lindex $a 1]}]

    set r [expr {($x+$y)*0.7}]

    if {$radius < $r } { set radius $r}
} 

## 
## bestimmeNachbarn  -	durchlaeuft alle Links und sucht zu jedem Link den 
##			optischen Nachbar (bei Gruppenelementen wird die 
##			GruppenID eingetragen und nicht die des eigentlichen
##			Linkpartners)
##

proc bestimmeNachbarn { links t } {
    
    ## Enthaelt zu jedem Element einer Gruppe, die GruppenID
    ##
    upvar $t tabelle
    global objektarray

    ## Alle Links durchlaufen
    ##
    foreach link $links {
	
	## Falls Linkpartner ein Gruppenelement, dann ida bzw. idb mit der 
	## GruppenID belegen
	##
	if {[catch {set ida $tabelle([ined src $link])}] != 0 } {
	    set ida [ined src $link]
	}
	if {[catch {set idb $tabelle([ined dst $link])}] != 0 } {
	    set idb [ined dst $link]
	}

	## Kein Link innerhalb einer Gruppe
	##
	if {$ida != $idb} {
	    
	    
	    ## Nachbarliste der Elemente erweitern
	    ##
	    nachbarListe $ida $idb
	    nachbarListe $idb $ida
	}
    }
}

##
## nachbarListe    - 	aktualisiert die Nachbarliste eines Elements, erhoeht
##			die Linkanzahl und ggf. die Anzahl der Nachbarn
##

proc nachbarListe { a b } {

    global objektarray

    ## Nachbarliste des Elements durchlaufen und nach dem neuen Nachbarn suchen
    ##
    for {set i 0} {$i < $objektarray($a,anznachbarn)} {incr i} {

	if { $objektarray($a,$i) == $b } { break }
    }
    
    ## Da Nachbar noch nicht in der Nachbarliste des Elements steht, 
    ## Nachbarn eintragen
    ##
    if { $i == $objektarray($a,anznachbarn) } {
	set objektarray($a,$i) $b
	incr objektarray($a,anznachbarn)
    }

    ## Linkanzahl des Elements erhoehen
    ##
    incr objektarray($a,anzlinks)
}


##
## erstelleTabelle   -	erstellt eine Tabelle, die zu jeder ID eines Gruppen-
##			elements die GruppenID zuordnet
## 

proc erstelleTabelle { idp id t } {

    ## Gibt zu jedem Elementt einer Gruppe, die GruppenID aus
    ##
    upvar $t tabelle

    ## ginfo mit Gruppeninformationen belegen
    ##
    set ginfo [ined -noupdate retrieve $id]

    ## Alle Mitglieder der Gruppe durchlaufen
    ##

    foreach e [ined member $ginfo] {

	## einfo mit Informationen ueber das Gruppenelement belegen
	##
	set einfo [ined -noupdate retrieve $e]

	if {[ined type $einfo] == "GROUP"} {

	    ## rekursiver Aufruf, falls ein Mitglied einer Gruppe wiederrum
	    ## eine Gruppe ist
	    ##
	    erstelleTabelle $idp [ined id $einfo] tabelle

	} else {

	    ## Unter der ID des Elements, den Gruppennamen speichern
	    ##
	    if {[ined type $einfo] != "LINK"} {
		set tabelle([ined id $einfo]) $idp
	    }
	}
    }    
}


## 
## sucheEE  -	sucht das Element mit den meistens Links (falls 2 Elemnte die
##		gleiche Anzahl haben, entscheidet die Linkanzahl der Nachbarn)
##

proc sucheEE { elemente } {

    global objektarray
    set anzahl 0
    global ee 
    set ee ""

    ## Element mit den meisten Links ermitteln
    ##
    foreach element $elemente {

	set id [ined id $element]
	set name [ined name $element]
	set a $objektarray($id,anzlinks)
	
	if {$anzahl == $a} {
	    lappend ee $id
	} else {
	    if {$anzahl < $a} {
		unset ee
		lappend ee $id
		set anzahl $a
	    }
	}
    }

    ## Mehrere Elemente ermittelt, eines auswaehlen
    ##
    if {[llength $ee] > 1} {
	set ee [waehleEE $ee]
    }
    return $ee
}

##
## waehleEE  -	Waehlt bei mehreren Startelementen eins aus (abhaengig von der
##		Anzahl der Links der Nachbarn)
##

proc waehleEE { ee } {
    
    global objektarray
    set anzahl 0

    ## 'ee' enthaelt alle Objekte, die Startobjekt werden koennen
    ## 
    foreach id $ee {
	
	set a 0

	## Linkanzahl der Nachbarn des Elements bestimmen
	##
	for {set i 0} {$i < $objektarray($id,anznachbarn)} {incr i} {
	    
	    ## ID des Nachbarn 
	    ##
	    set idn $objektarray($id,$i)

	    ## Anzahl Links der Nachbarn addieren
	    ##
	    incr a [expr {$objektarray($idn,anzlinks)}]
	}

	## Element mit den meisten "Nachbarlinks" speichern
	##
	if {$a > $anzahl} {
	    set ergebnis $id
	    set anzahl $a
	}
    }
    return $ergebnis
}

##
## Quicksort - sortiert die Nachbarn eines Objektes nach Anzlinks ihrer Links
##

proc quicksort { e u v} {

    global objektarray

    while {$u < $v} {

	set i $u
	set j $v
	set ab "false"
	set w $objektarray($e,$v)

	while {[expr {$i}] < [expr {$j}]} {
	    
	    if {$ab == "true"} {
		
		set id $objektarray($e,$j)
		if {$objektarray($id,anzlinks) <= $objektarray($w,anzlinks)} {
		    set objektarray($e,$i) $id
		    set ab "false"
		}

	    } else {

		set id $objektarray($e,$i)
		if {$objektarray($w,anzlinks) < $objektarray($id,anzlinks)} {
		    set objektarray($e,$j) $id
		    set ab "true"
		}
	    }

	    if {$ab == "true"} {
		set j [expr {$j-1}]
	    } else {
		incr i
	    }
       	}

	set objektarray($e,$j) $w
	set t $j

	if { [expr {$v-$t}] < [expr {$t-$u}] } {

	    if { [expr {$t+1}] < [expr {$v}]} {
		quicksort $e [expr {$t+1}] $v
	    }
	    set v [expr {$t-1}]

	} else {

	    if {[expr {$u}] <= [expr {$t-1}]} {
		quicksort $e $u [expr {$t-1}]
	    }
	    set u [expr {$t+1}]
	}
    }
}

#############################################################################
##
##	Funktionen, die zum Layout benoetigt werden
##
##
#############################################################################

##
##  erstesElementSetzen - Plaziert das erste Element (Element mit den meisten
##  Links) und seine Nachbarn auf dem Bildschirm
##

proc setzeEE { id } {

    global objektarray
    set anzahl 0
    set xm 0
    set ym 0

    ## Mittelwert eines jeden elements berechnen, dass mit dem Mittelwert-
    ## prinzip gesetzt werden soll
    ##
    for {set i 0} {$i<$objektarray($id,anznachbarn)} {incr i} {
	
	set idn $objektarray($id,$i)
	
	if {$objektarray($idn,gesetzt) == 2} {
	    
	    set k [mittelPkt $idn]
	    set xm [expr {[lindex $k 0]+$xm}]
	    set ym [expr {[lindex $k 1]+$ym}]
	    incr anzahl
	    
	}
    }
    
    if {$anzahl > 0} {

	## Mittelwert bestimmen
	##
	set xm [expr {$xm/$anzahl}]
	set ym [expr {$ym/$anzahl}]
    
	set pos [plaziereNachbarnMP $id $xm $ym]

    } else {
	
	## Mittelpunkt des Bildschirms berechnen
	##
	set s [ined -noupdate size]
	lappend p [expr {[lindex $s 2]/2}]
	lappend p [expr {[lindex $s 3]/2}]
	
	## Falls Mittelpunkt besetzt ist, auf 4 andere Punkte (statisch) 
	## versuchen auszuweichen
	##
	if {[positionFrei $id [lindex $p 0] [lindex $p 1]] == 0} {
	    
	    set p [suchePlatzEE $id [lindex $p 0] [lindex $p 1]]
	}
	
	## Element auf neue Position verschieben
	##
	verschiebeElement $id $p
	
	## Neue Position als gesetzt markieren
	##	
	positionMarkieren $id
    }

    ## Objekt als Startelement markieren
    ##
    set objektarray($id,gesetzt) 3

    ## Aktualisieren der Groesse des Graphens
    ##
    bestAusdehnung $id

    ## Nachbarn setzen
    ##
    set ergebnis [setzeNachbarn $id]

    return $ergebnis
}

##
## suchePlatzEE	- Sucht fuer das erste Element einen freien Platz
##

proc suchePlatzEE { id x y} {

    set anz 1
    set r 0
    set g 0
    set ergebnis ""

    set p [ined -noupdate size]
    set px [expr {[lindex $p 2]/3}]
    set py [expr {[lindex $p 3]/3}]
	    
    ## Gibt den Radius an, innerhalb dessen kein Element auftrat. Bei r < 0 ist
    ## ein Optimum erreicht und es braucht nicht mehr weitergesucht werden
    ##

    while {$r >= 0 && $anz < 5} {

	switch -- $anz {
	    1 { 
		set bx [expr {$x-$px}]
		set by [expr {$y-$py}]
	    }
	    2 {
		set bx [expr {$x+$px}]
		set by [expr {$y+$py}]
	    }
	    3 {
		set bx [expr {$x+$px}]
		set by [expr {$y-$py}]
	    }
	    4 {
		set bx [expr {$x-$px}]
		set by [expr {$y+$py}]
	    } 
	}

	incr anz
	set r [freieFlaeche $id $bx $by]
	
	## Optimum erreicht
	##
	if { $r < 0 } {
	    set ergebnis ""
	    lappend ergebnis $bx
	    lappend ergebnis $by
	    return $ergebnis

	}
	## Vergleich, ob neue Position besser ist (aber kein Optimum)
	##
	if { $r > $g} {
	    set ergebnis ""
	    lappend ergebnis $bx
	    lappend ergebnis $by
	    set g $r
	}
    }
    
    ## Wenn alle 4 Punkte belegt waren, wird schneckenfoermig 
    ## vom besten der letzten 4 Pkte. aus ein freier Platz gesucht
    ##
    if {[llength $ergebnis] == 0} {
	set ergebnis [suchePositionMP $id $x $y]
    }
 
    return $ergebnis
}

## 
## freieFlaeche    -	gibt den radius zureuck, innerhalb dessen Kreis sich 
##			kein Element befindet
##

proc freieFlaeche { id x y } {

    set max 150
    set pi_w 1.0471
    set rad 0
    set r 50
    set ergebnis -1
    global demo

    ## Wird nur dann gestartet, wenn Ausgangsposition frei ist
    ##
    if {[positionFrei $id $x $y] == 0} {

	set ergebnis 0

    } else {
	
	while {$rad < $max } {

	    set rad [expr {$rad+$r}]

	    ## Das vom radius abgesteckte Gebiet durch wahl von 6 Positionen
	    ## auf "Freiheit" testen
	    ##
	    for {set i 0} {$i < 6} {incr i} {
		
		## Punkte berechen
		##
		set cosinus [expr {cos ([expr {$pi_w*$i}])}]
		set sinus [expr {sin ([expr {$pi_w*$i}])}]
		set k [expr {$cosinus*$rad+$x}]
		lappend k  [expr {$sinus*$rad+$y}]

		if {$demo == "true"} {
		    verschiebeElement $id $k
		}
		
		## Ueberpruefen, ob der Platz belegt ist, wenn ja suche beenden
		##
		if {[positionFrei $id [lindex $k 0] [lindex $k 1]] == 0} {
		    if {$demo != "true"} {
			verschiebeElement $id $k
		    }
		    return $rad
		}
	    }
	}
    }

    return $ergebnis
}

##
## singleIcons     -	 schiebt alle einzelnen Icons in eine Ecke
##

proc singleIcons { icons } {

    global objektarray
    set z 0
    set ende [expr {[llength $icons]}]

    ## Bildschirmgroesse
    ##
    set b [ined -noupdate size]

    ## Bildschirm von links oben aus abwandern
    ##
    for {set i 20} {$i<[lindex $b 2]} {incr i 80} {
	for {set j 20} {$j<[lindex $b 3]} {incr j 15} {
	    
	    ## ID des einzelnen Icons
	    ##
	    set id [ined id [lindex $icons $z]]
	    
	    ## Freier Platz gefunden ?
	    ##
	    if {[positionFrei $id $i $j] == 1} {
		
		## Icon auf neue Position verschieben
		##
		set k $i
		lappend k $j
		verschiebeElement $id $k
		
		## Position als besetzt markieren
		##
		positionMarkieren $id
		
		## Element als gesetzt markieren
		##
		set objektarray($id,gesetzt) 2
		incr z
		
		## Alle Elemente gesetzt, dann Schluss
		##
		if {$z >= $ende} {
		    return
		}
	    }
	}
    }
}

##
## bestHalbe - berechnet die halbe Hoehe und Breite eines Icons
##

proc bestHalbe { id } {

    set m [ined -noupdate size $id]

    return "[expr {([lindex $m 2] - [lindex $m 0]) / 2}] \
	    [expr {([lindex $m 3] - [lindex $m 1]) / 2}] "
}

##
## mittelPkt - berechnet den Mittelpunkt eines Icons
##

proc mittelPkt { id } {

    set m [ined -noupdate size $id]

    return "[expr {(([lindex $m 2] - [lindex $m 0]) / 2) + [lindex $m 0]}] \
	    [expr {(([lindex $m 3] - [lindex $m 1]) / 2) + [lindex $m 1]}]"
}

##
## setzeNachbarn - 	setzt rekursiv alle von einem Element 'id' aus zu er-
##			reichenden Nachbarn. Diese Nachbarn werden nach zwei
##			Methoden gesetzt: 
##			1: Falls sie noch keine gesetzten Nachbarn besitzen, 
##			   werden sie nach einem Winkelprinzip gesetzt
##			2: Sie besitzen gesetzte Nachbarn, dann wird der Mit-
##			   telpunkt aus den Positionen aller gesetzten Nachbarn
##			   berechnet sie werden auf diesem Mittelpunktplaziert.
##			Sollten die berechneten Positionen bereits belegt sein,
##			werden Ausweichplaetze gesucht.
##

proc setzeNachbarn { id } {

    global objektarray 
    set wl ""
    set ml ""
    set poswerte ""

    ## Nachbarn in zwei Listen aufteilen, 'wl' fuer die Winkelplazierung und 
    ## 'ml' fuer die Mittelpunktplazierung
    ##
    set ll [teileNachbarnAuf $id]
    set wl [lindex $ll 0]
    set ml [lindex $ll 1]

    ## Plazieren nach Winkelprinzip
    ##
    if {[llength $wl] > 0} {

	## Winkel fuer die Nachbarn des Startlements werden gesondert berechnet
	## 
	if {$objektarray($id,gesetzt) == 3} {
	    set poswerte [startWinkelListeWP [llength $wl]]
	} else {
	    set poswerte [winkelListeWP [llength $wl] $objektarray($id,winkel)]
	}
	
	## Positionieren der Nachbarn
	##
	set ergebnis [winkelPlazierung $id $wl $poswerte]

	if {$ergebnis == "fehler"} {
	    return $ergebnis
	}
    }
    
    ## Plazieren nach Mittelwertprinzip
    ##
    if {[llength $ml] > 0} {

	set ergebnis [mittelwertPlazierung $ml $id]

	if {$ergebnis == "fehler"} {
	    return $ergebnis
	}
    }
    
    ## Markieren, dass die Nachbarn dieses Elements bearbeitet wurden
    ##
    set objektarray($id,gesetzt) 2
    
    ## Rekursiver Aufruf, um die Nachbarn der Nachbarn von 'id' zu setzen
    ##
    for {set i 0} {$i < $objektarray($id,anznachbarn)} {incr i} {
	
	## Nur Elemente, deren Nachbarn noch nicht von ihnen aus 
	## gesetzt wurden, werden aufgerufen
	##
	set idn $objektarray($id,$i)
	if {$objektarray($idn,gesetzt) != 2} {
	    set ergebnis [setzeNachbarn $idn]
	    if {$ergebnis == "fehler"} {
		return $ergebnis
	    }
	}
    }

    return 1
}

##
## teileNachbarnAuf - 	teilt die Nachbarn eines Elements in zwei Listen auf,
##			sortiert nach Winkelverteilung und nach Mittelpunktver-
##			teilung (Winkelverteilung genau dann, wenn ausser dem
##			Element 'id' kein weiterer Nachbar gesetzt ist)
##

proc teileNachbarnAuf { id } {

    ## Liste aller Bildschirmobjekte
    ##
    global objektarray

    set wl ""
    set ml ""
    set ergebnis ""
    
    ## Alle Nachbarn des Elements 'ID' durchlaufen
    ##
    for {set i 0} {$i < $objektarray($id,anznachbarn)} {incr i} {
	
	## ID des Nachbarn
	##
	set idn $objektarray($id,$i)
	
	## Nur Nachbarn, die noch nicht durch den Algorithmus plaziert wurden,
	## werden plaziert
	##
	if {$objektarray($idn,gesetzt) == 0} {

	    set anz 0

	    ## Anzahl der gesetzten Nachbarn des Nachbarn ermitteln
	    ##
	    for {set j 0} {$j<$objektarray($idn,anznachbarn)} {incr j} {
		
		## Test, ob Nachbar des Nachbarn gesetzt ist
		##
		set idnn $objektarray($idn,$j)
		if {$objektarray($idnn,gesetzt) != 0} {

		    incr anz
		    ## Mehr als ein Nachbar ist gesetzt, das zu setzende Ele-
		    ## ment 'idn', wird nach dem Mittelpunktprinzip gesetzt
		    ##
		    if {$anz > 1} {
			lappend ml $idn
			break
		    }
		}
	    }
	    
	    ## Nachbar hat keinen gesetzten Nachbarn (ausser 'id') und wird 
	    ## deshalb nach dem Winkelprinzip gesetzt
	    ##
	    if {$anz == 1} {
		lappend wl $idn
	    }

	    ## Objekt als 'vorerst'-gesetzt setzen
	    ##
	    set objektarray($idn,gesetzt) 1
	}
    }

    lappend ergebnis $wl
    lappend ergebnis $ml
    return $ergebnis
}
    
##
## startWinkelListeWP - uebergibt eine Liste von Paaren, die fuer jeden 
##			Nachbarn des Startelements den Winkel und den Radius
##			enthaelt
##

proc startWinkelListeWP { anzahl } {

    set first 1
    set runde 0
    global radius
    set rad $radius
    set r [expr {$radius*0.8}]

    while {$anzahl > 0} {

	set z [expr {($anzahl < 6) ? $anzahl : 6}]
	set anzahl [expr {$anzahl-$z}]

	## Unterscheidung, zw. den ersten 6 Nachbarn und den folgenden
	## 
	set w [expr {($first == 1) 
	    ? [startWinkelWP1 $z] : [startWinkelWP2 $runde $z]}]

	## Winkelliste vergroessern
	##
	for {set i 0} {$i<[llength $w]} {incr i} {
	    lappend p [lindex $w $i]
	    lappend p $rad
	    lappend a $p
	    unset p
	}
	set rad [expr {$rad+$r}]
	incr runde
	set first 0
    }
    
    return $a
}

##
## startWinkelWP1  -	berechnet fuer die ersten 6 Nachbarn des Startelements
##			(Element mit den meisten Links) die Winkel und gibt 
##			sie als Liste zurueck
##

proc startWinkelWP1 { anz } {

    set a ""
    set 2pi 6.2831

    switch -- $anz {

	1 { lappend a [expr {$2pi/4}]}
	4 { for {set i 0} {$i<$anz} {incr i 1} {
	    lappend a [expr {$i*$2pi/$anz + $2pi/8}]}}
	default { for {set i 0} {$i<$anz} {incr i} {
	    lappend a [expr {$i*$2pi/$anz}]}}
    }
    return $a
}
    
##
## startWinkelWP2  -	berechnet (ausser fuer die ersten 6 Nachbarn des Start-
##			elements) die Winkel fuer die Nachbarn des Startele-
##			ments. Fuer jeden neuen 'Ring' (6 Nachbarn bilden einen
##			Ring) wird der Winkel um 30 Grad verschoben.
##

proc startWinkelWP2 {ring anz} {

    set pi_3 1.0472
    set pi_6 0.5236
    set pi   3.1415
    set p 3
    set r 0
    set a ""

    lappend a [expr {$pi+$pi_6*$ring}]

    for {set i 1} {$i<$anz} {incr i} {
	
	if {$r == 0} {
	    set p [expr {$p-3}]
	    set r 1
	} else {
	    incr p 4
	    set r 0
	}
	lappend a [expr {$p*$pi_3+$pi_6*$ring}]
    }
    return $a
}


##
## winkelListeWP   -	uebergibt eine Liste von Paaren, die fuer jeden Nach-
##			barn den Winkel und den Radius enthaelt (Ausgenommen
##			sind die Nachbarn des Startelements).
##

proc winkelListeWP {anzahl richtung} {

    global radius
    set rad $radius
    set r [expr {$radius*0.8}]

    while {$anzahl > 0} {

	## Winkel werden immer in 3-Gruppen bestimmt
	##
	set z [expr {($anzahl < 3) ? $anzahl : 3}]
	set anzahl [expr {$anzahl-$z}]

	## w mit einer Liste von Winkeln (fuer max. 3 Elemente) belegen
	##
	set w [winkelWP $richtung $z]

	## Alle Winkel in 'a' speichern
	##
	for {set i 0} {$i<[llength $w]} {incr i} {
	    lappend p [lindex $w $i]
	    lappend p $rad
	    lappend a $p
	    unset p
	}

	## radius fuer den naechsten Ring vergroessern
	##
	set rad [expr {$rad+$r}]
    }
    return $a
}


##
## winkelWP -	berechnet die einzelnen Winkel fuer jedes durch Winkelvertei-
##		zu positionierende Element.
##

proc winkelWP {richtung anz} {

    set a ""
    set pi_6 0.5236
    set pi_5 0.6283

    switch -- $anz {

	1 { 
	    lappend a $richtung }
	2 { 
	    lappend a [expr {$richtung-$pi_6}]
	    lappend a [expr {$richtung+$pi_6}] }
	3 { 
	    lappend a [expr {$richtung-$pi_5}]
	    lappend a $richtung
	    lappend a [expr {$richtung+$pi_5}] }
    }
    
    return $a
}

##
## winkelPlazierung  -	plaziert die Nachbarn 'nachbarn' eines Elements 'id' 
##			mit Hilfe der uebergebenen Werte (Winkel & Radius) in
##			'poswerte'
##

proc winkelPlazierung { id nachbarn poswerte } {

    ## Liste aller auf dem Bildschirm befindlichen Objekte
    ##
    global objektarray
    set i 0

    ## Nachbarn von 'id' setzen
    ##
    foreach idn $nachbarn {
	
	## Position des Nachbarn berechnen und ggf. freie Position auf dem
	## Bildschirm suchen und Nachbarn auf neue Position verschieben
	##
	set pos [plaziereNachbarnWP $idn $id [lindex $poswerte $i]]

	if {[lindex $pos 0] != "fehler"} {
	    
	    ## gefundene Position in der Bildschirmmatrix als besetzt markieren
	    ##
	    positionMarkieren $idn
	    
	    ## Nachbar als gesetzt markieren
	    ##
	    set objektarray($idn,gesetzt) 4
	    
	    ## Winkel des Nachbarn speichern
	    ##
	    set objektarray($idn,winkel) [lindex $pos 2]
	    
	    ## Uberpruefen, ob die Ausdehnung des Graphen sich veraendert hat 
	    ## und ggf. 'ausdehnung' veraendern
	    ##
	    bestAusdehnung $idn
	    
	    incr i

	} else {

	    return [lindex $pos 0]
	}
    }

    return 1
}


## 
## plaziereNachbarWP -	plaziert den Nachbar, indem sie ihm entsprechend dem 
##			uebergebenen Winkel einen Position berechnet. Sollte
##			die Position besetz sein, wird eine neue Position in 
##			der Umgebung gesucht. Ist die Position ausserhalb des
##			Bildschirms, wird das gesamte Bildschirmlayout ueber-
##			arbeitet und dem Nachbarn eine freie Position 
##			zugewiesen
##

proc plaziereNachbarnWP { id idp poswert } {

    ## Berechnet die Bildschirmkoordinaten
    ##
    set k [berechneKoordinaten $idp $poswert]

    ## Ueberpruefen, ob neue Position frei/besetzt ist oder sich 
    ## ausserhalb des Bildschirms befindet
    ##
    set erg [positionFrei $id [lindex $k 0] [lindex $k 1]]
    
    switch -- $erg {

	0 { 
	    set ergebnis [suchePositionWP $id $idp $poswert]
	}
	1 {
	    ## Verschiebung des Nachbarn auf neue Position 
	    ##
	    verschiebeElement $id $k
	    set ergebnis [lindex $k 0]
	    lappend ergebnis [lindex $k 1]
	    lappend ergebnis [lindex $poswert 0]
	}
	default { 
	    set parameter $erg
	    lappend parameter $id
	    lappend parameter $idp
	    lappend parameter $poswert
	    set ergebnis [suchePositionWPB $parameter]
	}
    }
    
    return $ergebnis
}

##
## suchePositionWP - 	sucht fuer ein gegebens Element, das durch 
##			Winkelplazierung plaziert werden soll,  eine freie
##			Position
##

proc suchePositionWP {id idp poswert} {

    global radius
    global demo
    set rad [lindex $poswert 1]
    set r [expr {$radius*0.8}]
    set v -1
    
    for {set ende 0} {$ende < 25} {incr ende} {
	
	set winkel [lindex $poswert 0]

	for {set i 0} {$i<=1.2} {set i [expr {$i+0.2}]} {

	    for {set j 0} {$j<2} {incr j} {
		
		set i [expr {$i*(-1)}]

		## Neue Bildschirmkoordinaten berechnen
		##
		set p [expr {$winkel+$i}]
		lappend p $rad
		set k [berechneKoordinaten $idp $p]

	    
		## Ueberpruefen, ob neue Position frei/besetzt ist oder sich 
		## ausserhalb des Bildschirms befindet
		##
		set erg [positionFrei $id [lindex $k 0] [lindex $k 1]]

		if {$demo == "true"} {
		    verschiebeElement $id $k
		}
		
		if {$erg != 0} {
		    
		    if {$erg == 1} {
			set ergebnis [lindex $k 0]
			lappend ergebnis [lindex $k 1]
			lappend ergebnis [lindex $p 0]
			if {$demo != "true"} {
			    verschiebeElement $id $k
			}
			return $ergebnis
		    } else {
			set winkel [winkelSpiegelung $erg $winkel]
		    }
		}
	    }
	}
	
	set rad [expr {$rad+$r}]
    }
    
    
    if {$ende >= 25} {
	ined ackledge "Keine Layoutgestaltung moeglich !"
	return "fehler"
    }

    return 1
}

## 
## suchePositionWPB -	Sucht fuer ein Element, dass ausserhalb des Bildschirms
##			positioniert ist, einen neuen Platz. Zuerst wird 
##			versucht, den Schwerpunkt des Bildschirms zu verlagern,
##			d.h alle Elemente werden in die entgegengesetzte 
##			Richtung geschoben.

proc suchePositionWPB { p } {

    ## Ueberprueft, ob Schwerpunkt des Graphens verschoben werden kann
    ##
    set verschiebung [sucheSchwerpunkt [lindex $p 0]]

    ## Graph konnte nicht verschoben werden
    ##
    if {$verschiebung == 0} {

	## Winkel spiegeln
	##
	set winkel [winkelSpiegelung [lindex $p 0] [lindex [lindex $p 3] 0]]
	set poswert $winkel
	lappend poswert [lindex [lindex $p 3] 1]
	
    } else {
	
	## Alten Winkel beibehalten
	##
	set poswert [lindex $p 3]
    }
	
    ## Neue Position suchen und neue Koordinaten & Winkel zurueckgeben
    ##
    set id [lindex $p 1]
    set idp [lindex $p 2]
    set ergebnis [suchePositionWP $id $idp $poswert]

    return $ergebnis
}

##
## verschiebeElement -	verschiebt ein Icon auf dem Bildschirm
##

proc verschiebeElement { id k } { 

    ## Mittelpunkt des Icons berechnen
    ##
    set m [mittelPkt $id]

    ## Neue Position - Alte Position ergibt die zur alten Position relativen 
    ## Verschiebungswerte
    ##
    set x [expr {[lindex $k 0] - [lindex $m 0]}]
    set y [expr {[lindex $k 1] - [lindex $m 1]}]

    ined move $id $x $y

}

##
## berechneKoordinaten    -	berechnet die neue Position des Nachbarn 
##

proc berechneKoordinaten { id poswert } {

    ## Mittelpunkt des Elements 'id'
    ##
    set m [mittelPkt $id]

    ## Neue Position berechnen (Mittelpunkt des Nachbarn beruecksichtigen)
    ##
    set cosinus [expr {cos ([lindex $poswert 0])}]
    set sinus [expr {sin ([lindex $poswert 0])}]
    set x [expr {$cosinus*[lindex $poswert 1]}]
    set y [expr {$sinus*[lindex $poswert 1]}]

    ## Neue Koordinaten 
    ##
    set ergebnis  [expr {[lindex $m 0] + $x}]
    lappend ergebnis [expr {[lindex $m 1] + $y}]
    
    return $ergebnis
}
    

##
## winkelSpiegelung -	spiegelt einen uebergebene Winkel
##

proc winkelSpiegelung { fehler winkel } {

    set pi 3.1415
    set pi_2 1.5710
    set 2pi 6.2830
    set 3pi 9.4247
    set 3pi_2 4.7124

    switch -- $fehler {
	-1 {
	    set spiegel [expr {($winkel > $pi_2)
		? [expr {$pi-$winkel}] : [expr {$2pi-$winkel}]}]
	}
	-2 { 
	    set spiegel [expr {($winkel > $3pi_2) 
		? [expr {$2pi-$winkel}] : [expr {$2pi-$winkel}]}]
	}
	-3 {
	    set winkel [expr {($winkel > $2pi) 
		? [expr {$winkel-$2pi}] : $winkel}]
	    set spiegel [expr {($winkel > $pi_2) 
		? [expr {$3pi-$winkel}] : [expr {$pi-$winkel}]}]
	}
	default {
	    set spiegel [expr {$2pi-$winkel}]
	}
    }
    return $spiegel
}



##
## mittelPlazierung  - 	plaziert die uebergebene Elemente nach dem Mittelwert
## 			der Positionen aller ihrer bereits gesetzten Nachbarn
##			und berechnet den Winkel zw. sich und dem 'ElternIcon'
##

proc mittelwertPlazierung { l idp } {

    global objektarray

    ## Liste Elementeweise durchwandern
    ##
    foreach id $l {

	set anzahl 0
	set xm 0
	set ym 0

	## Mittelwert eines jeden elements berechnen, dass mit dem Mittelwert-
	## prinzip gesetzt werden soll
	##
	for {set i 0} {$i<$objektarray($id,anznachbarn)} {incr i} {

	    set idn $objektarray($id,$i)

	    if {$objektarray($idn,gesetzt) != 0} {

		set k [mittelPkt $idn]
		set xm [expr {[lindex $k 0]+$xm}]
		set ym [expr {[lindex $k 1]+$ym}]
		incr anzahl

	    }
	}
	
	## Mittelwert bestimmen
	##
	set xm [expr {$xm/$anzahl}]
	set ym [expr {$ym/$anzahl}]

	set pos [plaziereNachbarnMP $id $xm $ym]
	
	if {[lindex $pos 0] != "fehler" } {

	    ## Position in der Bildschirmmatrix als besetzt markieren
	    ##
	    positionMarkieren $id
	    
	    ## Element als gesetzt markieren
	    ##
	    set $objektarray($id,gesetzt) 4
	    
	    ## Uberpruefen, ob die Ausdehnung des Graphen sich veraendert hat 
	    ## und ggf. 'ausdehnung' veraendern
	    ##
	    bestAusdehnung $id
	    
	    ## Winkel zum 'Eltern-Icon bestimmen
	    ##
	    set x [lindex $pos 0]
	    set y [lindex $pos 1]
	    set objektarray($id,winkel) [bestWinkelMP $idp $x $y]

	} else {
	    
	    ## Layoutgestaltung kann nicht zu Ende gefuehrt werden
	    ##
	    return "fehler"
	}
    }

    return "ok"
}

##
## plaziereNachbarnMP
##

proc plaziereNachbarnMP { id x y } {

    ## Ueberpruefen, ob neue Position frei/besetzt ist oder sich 
    ## ausserhalb des Bildschirms befindet
    ##
    set erg [positionFrei $id $x $y]
    
    switch -- $erg {
	
	0 { 
	    set pos [suchePositionMP $id $x $y]
	}
	1 {
	    set pos $x
	    lappend pos $y
	    verschiebeElement $id $pos
	}
	default { 
	    verschiebeSchwerpunkt $erg
	    set pos [suchePositionMP $id $x $y]
	}
    }

    return $pos
}

##
## bestWinkelMP - 	Bestimmt nachtraeglich den Winkel zw. einem Nachbarn 
##			und seinem Elternicon (nur bei Mittelplazierung noetig)
##

proc bestWinkelMP { idp x y } { 

    set pos [mittelPkt $idp]
    set x [expr {[lindex $pos 0]-$x}]
    set y [expr {[lindex $pos 1]-$y}]
    set xq [expr {$x*$x}]
    set yq [expr {$y*$y}]

    set hyp [expr {sqrt([expr {$xq+$yq}])}]
    set winkel [expr {asin ([expr {$y/$hyp}])}]

    return $winkel
}

##
## suchePositionMP    -	sucht schneckenfoermig einen freien Platz auf dem 
##			Bildschirm
##

proc suchePositionMP {id x y} {

    set pi_w 1.0471
    set result ""
    global radius
    global demo
    set rad $radius
    set r $radius

    ## Bis freier Platz gefunedn ist
    ##
    for {set ende 0} {$ende < 25} {incr ende} {
	
	## In pi/6 - Schritten um den Startpunkt gehen
	##
	for {set i 0} {$i < 6} {incr i} {
	    
	    set cosinus [expr {cos ([expr {$pi_w*$i}])}]
	    set sinus [expr {sin ([expr {$pi_w*$i}])}]

	    ## Neuen Punkt bestimmen
	    ##
	    set xw [expr {$cosinus*$rad+$x}]
	    set yw [expr {$sinus*$rad+$y}]

	    set k $xw
	    lappend k $yw

	    if {$demo == "true"} {
		verschiebeElement $id $k
	    }

	    ## Wenn gefundener Platz frei ist, dann ende
	    ##
	    if {[positionFrei $id $xw $yw]} {

		if {$demo != "true"} {
		    verschiebeElement $id $k
		}
		return $k
	    }
	}
	
	## Radius erhoehen und suche in pi/6 im vergroesserten Kreis fortsetzen
	##
	set rad [expr {$rad+$r}]
    }

    if {$ende >= 25} {
	ined ackledge "Keine Layoutgestaltung moeglich !"
	return "fehler"
    }
}

##
## sucheSchwerpunkt   - 	versucht den Schwerpunkt des Graphen zu ver-
##				schieben
##

proc sucheSchwerpunkt { fehler } {

    global ausdehnung
    global radius
    global ee
    set b [ined -noupdate size]
    set pee [ined -noupdate size $ee]
    set ergebnis 1

    switch -- $fehler {
	-1 { 	
	    set w [expr {[lindex $b 2]-[lindex $ausdehnung 2]}]
	    set g [expr {[lindex $b 2]/2+[lindex $b 2]/7-[lindex $pee 2]}]
	    set s [expr {($w > $g) ? $g : $w}]
	    if {$s < $radius} { set ergebnis 0 }
	    lappend s 0 
        }
	-2 {
	    set s 0
	    set w [expr {[lindex $b 3]-[lindex $ausdehnung 3]}] 
	    set g [expr {[lindex $b 3]/2+[lindex $b 3]/7-[lindex $pee 3]}]
	    lappend s [expr {($w > $g) ? $g : $w}]
	    if {[lindex $s 1] < $radius} { set ergebnis 0 }
	}
	-3 {
	    set w [lindex $ausdehnung 0]
	    set g [expr {[lindex $pee 0]-([lindex $b 2]/2-[lindex $b 2]/7)}]
	    set s [expr {($w > $g) ? [expr {$g*(-1)}] : [expr {$w*(-1)}]}]
	    if {$s > ((-1)*$radius)} { set ergebnis 0 }
	    lappend s 0
	}
	-4 {
	    set s 0
	    set w [lindex $ausdehnung 1]
	    set g [expr {[lindex $pee 1]-([lindex $b 3]/2-[lindex $b 3]/7)}]
	    lappend s [expr {($w > $g) ? [expr {(-1)*$g}] : [expr {$w*(-1)}]}]
	    if {[lindex $s 1] > ((-1)*$radius)} { set ergebnis 0 }
	}
    }

    if {$ergebnis == 1} {
	verschiebeSchwerpunkt $s
    }

    return $ergebnis
}

##    
## verschiebeSchwerpunkt -	verschiebt alle Bildschirmelemnte um die
##				Strecke 's'
##    

proc verschiebeSchwerpunkt { s } {

    global bildschirm
    unset bildschirm
    global ausdehnung
    unset ausdehnung
    global objekte
    global objektarray

    set dx [lindex $s 0]
    set dy [lindex $s 1]

    foreach id $objekte {

	ined -noupdate move $id $dx $dy
	
	if {($objektarray($id,gesetzt) == 2) 
	    || ($objektarray($id,gesetzt) == 4)} {
		
		## Es werden zur Aktualisierung der Bildschirmmatrix &
		## der max. Ausdehnung nur bereits gesetzte Objekte 
		## beruecksichtigt
		## 
		positionMarkieren $id 
		bestAusdehnung $id
	    }
    }

    return 1
}
		
##
## positionMarkieren - 	Markiert die Position des Elements in der Bildschirm-
##			matrix als besetzt
##

proc positionMarkieren { id } {

    ## Bildschirmmatrix
    ##
    global bildschirm
    global objektarray

    ## Halbe Hoehe und Breite des Icons bestimmen
    ##
    set a $objektarray($id,halbe)
    set m [mittelPkt $id]
    
    ## Anfangs und Endfelder in der Bildschirmmatrix bestimmen und Position
    ## des Icons als besetzt markieren
    ##
    set xa [expr {round([expr {([lindex $m 0] - [lindex $a 0])/10}])-1}]
    set ya [expr {round([expr {([lindex $m 1] - [lindex $a 1])/10}])-1}]
    set xe [expr {round([expr {([lindex $m 0] + [lindex $a 0])/10}])+1}]
    set ye [expr {round([expr {([lindex $m 1] + [lindex $a 1])/10}])+1}]

    for {set i $xa} {$i<=$xe} {incr i} {
	for {set j $ya} {$j<=$ye} {incr j} {
	    set bildschirm($i,$j) $id
	}
    }
}

## 
## bestAusdehnung -	ueberprueft, ob sich die Ausdehnung des Graphen ver-
##			groessert hat und traegt die neuen Werte in 
##			'ausdehnung' ein
##

proc bestAusdehnung { id } {

    global ausdehnung
    set a ""

    set pos [ined -noupdate size $id]

    if {![info exists ausdehnung]}  {
	set ausdehnung $pos
    } else {
	lappend a [expr {([lindex $pos 0] < [lindex $ausdehnung 0]) 
	    ? [lindex $pos 0] : [lindex $ausdehnung 0] }]
	lappend a [expr {([lindex $pos 1] < [lindex $ausdehnung 1]) 
	    ? [lindex $pos 1] : [lindex $ausdehnung 1] }]
	lappend a [expr {([lindex $pos 2] > [lindex $ausdehnung 2]) 
	    ? [lindex $pos 2] : [lindex $ausdehnung 2] }]
	lappend a [expr {([lindex $pos 3] > [lindex $ausdehnung 3]) 
	    ? [lindex $pos 3] : [lindex $ausdehnung 3] }]
	set ausdehnung $a
    }

}

##
## positionFrei    -  	ueberprueft, ob Element an der Position (x,y) gesetzt 
##			werden kann, oder nicht. Rueckgabewerte : 
##			Position ist frei 		    :  1
##			Position besetzt		    :  0
##			Position ausserhalb des Bildschirms : -1
##

proc positionFrei {id x y} {

    ## Bildschirmmatrix
    ##
    global bildschirm
    global objektarray

    ## Koordinaten des Icons und des Bildschirms bestimmen
    ##
    set a $objektarray($id,halbe)
    set b [ined -noupdate size]

    ## Icon befindet sich nicht vollstaendig im Bildschirm
    ##
    if {($x-[lindex $a 0]) < [lindex $b 0]} { 
	return -1 
    } else {
	if {($y-[lindex $a 1]) < [lindex $b 1]} {
	    return -2
	} else {
	    if {($x+[lindex $a 0]) > [lindex $b 2]} {
		return -3
	    } else {
		if {($y+[lindex $a 1]) > [lindex $b 3]} {
		    return -4
		} else {
		    
		    ## Anfangs und Endfelder in der Bildschirmmatrix berechnen
		    ## und in der Matrix ueberpruefen, ob sie belegt sind
		    ##
		    set xa [expr {round([expr {($x - [lindex $a 0])/10}])-1}]
		    set ya [expr {round([expr {($y - [lindex $a 1])/10}])-1}]
		    set xe [expr {round([expr {($x + [lindex $a 0])/10}])+1}]
		    set ye [expr {round([expr {($y + [lindex $a 1])/10}])+1}]
		    
		    for {set i $xa} {$i<=$xe} {incr i} {
			for {set j $ya} {$j<=$ye} {incr j} {
			    if {[info exists bildschirm($i,$j)]} {
				
				## Platz bereits belegt
				##
				return 0
			    }
			}
		    }
		    
		    ## Platz ist frei
		    ##
		    return 1
		}
	    }
	}
    }
}

vwait forever
