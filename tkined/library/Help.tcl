##
## Help.tcl
##
## This file contains the help commands of the tkined editor.
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

package provide TkinedHelp 1.6.0

proc Help__general { editor } {
    set w [$editor toplevel]
    Dialog__browse $w.canvas \
	"Welcome to Tkined -- a Tk based interactive network editor." {
	"This is the February 2017 version, based on the GitHub repository"
	"maintained by FlightAware.  For more information please read the"
	"documentation in the source tree at:"
	""
	"  https://github.com/jorge-leon/scotty"
	""
	"The following are historic notes by the original author, the web"
	"references are likely all dead links."
        ""
	"This version has been written based on the original ined version"
	"that was build on top of the Unidraw/Interviews framework. Many"
	"thanks to the developers of the original version, Thomas Birke"
	"and Hinnerk Ruemenapf."
	""
	"Also many thanks to Guntram Hueske (hueske@ibr.cs.tu-bs.de) who"
	"wrote the stripchart and barchart code in C to make it faster."
	"And of course, many thanks to Erik Schoenfelder for his support"
	"and ideas that improved tkined (and scotty) a lot. Frank Dippel"
	"(dippel@ibr.cs.tu-bs.de) has taken over the job of running into"
	"all the obscure bugs while he uses tkined as a front end to a"
	"network simulator."
	""
	"And I would also like to thanks all those people that have spend"
	"some of their time to play with tkined. I really appreciate all"
	"the bug reports and suggestions I have received."
	""
	"There is a mailing list you might join to report bugs or suggestions"
	"or to get the latest news about tkined. To join, send a message to:"
	""
	"          tkined-request@ibr.cs.tu-bs.de"
	""
	"The archive of the mailing list and much more is available on the"
	"World Wide Web:"
	""
	"          http://wwwsnmp.cs.utwente.nl/~schoenw/scotty/"
	""
	"Juergen Schoenwaelder (schoenw@cs.utwente.nl)"
    }
}

proc Help__status { editor } {
    global tkined
    global auto_path

    set w [$editor toplevel]
    set text ""
    set total 0

    foreach id [$editor retrieve] {
	set type [$id type]
	if {![info exists count($type)]} { set count($type) 0 }
	incr count($type)
	incr total
    }

    lappend text "file: [$editor filename]"
    lappend text "directory: [$editor dirname]"
    lappend text ""
    lappend text "total # of open views: [llength [winfo children .]]"
    lappend text "total # of objects: $total"
    lappend text ""
    foreach t [lsort [array names count]] {
	lappend text "# of $t objects: $count($t)"
    }
    if {$tkined(debug)} {
	lappend text ""
	lappend text "debugging mode is switched on."
	lappend text "auto_path: $auto_path"
    }
    Dialog__browse $w.canvas \
	"Status information for tkined version $tkined(version):" $text
}

proc Help__key_bindings { editor } {
    set w [$editor toplevel]
    Dialog__browse $w.canvas \
	"Inside of the canvas, the following key bindings apply:" { 
	"middle mouse button : move selected objects"  
	"shift middle mouse button : move the whole canvas"  
	"right mouse button : pop up an objects specific menu"  
	""  
	"The following tool specific bindings exist:"  
	""  
	"Select"  
	""  
	"left mouse button : select nearest object (deletes current selection)"  
	"shift left mouse button : add nearest object to the current selection"  
	""  
	"Resize"  
	""  
	"left mouse button on chart corner: resize stripcharts and barcharts"  
	"left mouse button on network:  alter network length"  
	""  
	"Node"  
	""  
	"left mouse button : put a new node object on the canvas"  
	""  
	"Network"  
	""  
	"left mouse button : define startpoint and endpoint of a network object"  
	"shift left mouse button : mark intermediate points of a network object"  
	""  
	"Link"  
	""  
	"left mouse button : define startpoint and endpoint of a link object"  
	"shift left mouse button : mark intermediate points of a link object"  
	""  
	"Group"  
	""  
	"left mouse button : put a new group object on the canvas"
    }
}
