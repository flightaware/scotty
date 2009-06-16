#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# game.tcl -
#
#	A simple game to be played inside the Tkined editor. This
#	was actually the first application ever written for Tkined.
#
# Copyright (c) 1993-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: game.tcl,v 1.1.1.1 2006/12/07 12:16:58 karl Exp $

package require Tnm 3.0

namespace import Tnm::*

ined size
set score_file $tnm(tmp)/.ined_scores

##
## This is a very simple random generator x_n = 125 * x_{n-1} mod 2796203
##

set last [expr {[clock seconds] % 2796203}]
set last [expr {$last > 0 ? $last : -1*$last}]

proc random {} {
    global last
    set last [expr {($last * 125) % 2796203} ]
    return [expr {$last/2796203.0}]
}

##
## Save the time in the score file.
##

proc save_score {time} {
    global score_file tnm
    set f [open $score_file a]
    puts $f "$time $tnm(user) [clock format [clock seconds]]"
    close $f
    ined acknowledge "You needed $time seconds."
}

##
## A simple game running in the ined editor.
##

proc Start { list } {
    ined page DINA4 landscape
    set status [ined size]
    set xmax [lindex $status 2]
    set ymax [lindex $status 3]
    set nodes ""
    foreach foo "1 2 3 4 5 6" {
        set id [ined -noupdate create NODE]
        set x [expr {$xmax/2+([random]-0.5)*$xmax/4}]
        set y [expr {$ymax/2+([random]-0.5)*$ymax/4}]
	ined -noupdate icon $id machine
	ined -noupdate move $id $x $y
	ined name $id ""
        lappend nodes $id
    }
    set n [llength $nodes]
    ined acknowledge "Try to connect a node to all other new nodes."
    set start [clock seconds]
    while {1} {
        foreach id $nodes {
            set foo [ined retrieve $id]
            set linkcount [expr {[llength [lindex $foo 5] ]+1} ]
            if {$linkcount == $n} {
		set elapsed [expr {[clock seconds]-$start}]
		save_score $elapsed
		Highscores dummy
		foreach node $nodes { ined delete $node }
                return
            }
            set x [expr {([random]-0.5)*20*$linkcount}]
            set y [expr {([random]-0.5)*20*$linkcount}]
            set xy [ined move $id $x $y]
	    set x [lindex $xy 0]
	    set y [lindex $xy 1]
	    if {($x <= 0) || ($y <= 0) || ($x >= $xmax) || ($y >= $ymax)} {
		ined acknowledge "YOU LOOSE. A node has left the play ground."
		foreach node $nodes { ined delete $node }
                return
	    }
        }
    }
}

##
## Show the current high score list.
##

proc compare { a b } {
    return [expr {[lindex $a 0] > [lindex $b 0]}]
}

proc Highscores { list } {
    global score_file
    set scores ""
    if {[catch {open $score_file r} f]} {
	ined acknowledge "Unable to open highscore file!"
    } else {
	while {![eof $f]} {
	    set entry [gets $f]
	    lappend scores $entry
	}
	close $f
	set txt ""
	foreach line [lsort -command compare $scores] {
	    if {$line == ""} continue
	    set time [lindex $line 0]
	    set name [lindex $line 1]
	    set date [lrange $line 2 end]
	    lappend txt [format "%3s %-10s %s" $time $name $date]
	}
	ined browse "The highscore list:" $txt
    }
}

##
## Display some help about this tool.
##

proc "Help Game" {list} {
    ined browse "Help about this game:" {
	"This is a very simple game. You have to hunt some crazy" 
	"nodes by linking them together. The game finishes if either" 
	"one node is linked with all other nodes (forming a star" 
	"topology) or if one node hits the page boundary." 
	"" 
	"This game is for Martin and was in fact the first application" 
	"using tkined as its user interface. Perhaps I will add" 
	"extensions for other topologies and different levels..." 
    }
}

##
## Delete the menus created by this interpreter.
##

proc "Delete GAME" {list} {
    global menus
    foreach id $menus { ined delete $id }
    exit
}

set menus [ ined create MENU GAME Start Highscores "" \
	   "Help Game" "Delete GAME" ]

vwait forever
