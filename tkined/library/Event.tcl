##
## Event.tcl
##
## This file contains all the code that implements the EVENT viewer.
##
## Copyright (c) 1993, 1994, 1995
##
## J. Schoenwaelder
## TU Braunschweig, Germany
## Institute for Operating Systems and Computer Networks
##
## Permission to use, copy, modify, and distribute this
## software and its documentation for any purpose and without
## fee is hereby granted, provided that this copyright
## notice appears in all copies.  The University of Braunschweig
## makes no representations about the suitability of this
## software for any purpose.  It is provided "as is" without
## express or implied warranty.
##

package provide TkinedEvent 1.5.0

##
## The following set of procedures handle EVENT objects.
##

proc EVENT__canvas { event } {
    EVENT__list $event
}

proc EVENT__delete { event } {
}

proc EVENT__bell { event } {
    bell
}

proc EVENT__list { event } {
    set top [EVENT__top $event]
    $top.list delete 0 end
    set eventlist ""
    foreach object [[$event editor] retrieve EVENT] {
	lappend eventlist $object
    }
    foreach object [lsort $eventlist] {
	$top.list insert end [format "%s : %s" $object [$object name]]
    }
}

proc EVENT__top { event } {

    # The offset used to position log windows automatically.
    static offset
    if {![info exists offset]} {
	set offset 80
    } else {
	incr offset 10
	if {$offset > 180} {set offset 80}
    }

    set top [winfo toplevel [$event canvas]].events

    if {![winfo exists $top]} {

	toplevel $top

	# set up the menu bar
	frame $top.menu -borderwidth 1 -relief raised
	pack $top.menu -side top -fill x

	# set up the file menu
	menubutton $top.menu.file -text "File" -menu $top.menu.file.m
	pack $top.menu.file -side left
	menu $top.menu.file.m
	$top.menu.file.m add command -label "Close View" \
		-accelerator "  ^K" \
		-command "destroy $top"
	bind $top <Control-k> "$top.menu.file.m invoke {Close View}"
	
	scrollbar $top.scrollbar -command "$top.list yview" -relief sunken
	listbox $top.list -yscrollcommand "$top.scrollbar set" 
	pack $top.scrollbar -side right -fill y
	pack $top.list -side left -padx 2 -pady 2 -fill both -expand yes
    }

    return $top
}

