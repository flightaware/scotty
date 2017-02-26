##
## This is the initialization file for the tkined network editor.
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

package provide TkinedMisc 1.5.1

##
## This is ined implemented on top of tk. This file contains only
## some basic stuff. Most stuff is organized in modules kept in the
## following files:
##
##  Editor.tcl	The editor abstraction. Generates a toplevel
##	        window with the menubar, the toolbox and the
##		canvas. The editor just defines the interior.
##
##  Tool.tcl	The tools that you can select from the tool box
##              and apply on the canvas. These tools provide
##              some graphical animation and finally call some
##              of the low level tcl commands.
##
##  Command.tcl	The commands that implement all the operation
##              that may be triggered by the menus and tools.
##              Most commands just break complex operations on
##              a selection down on low level operations on a
##              single object.
##
##  Objects.tcl This is the implementation of the callback
##              routines. They really implement how tkined
##		objects drawn themself.
##
##  Dialog.tcl	Some useful dialogs used by tkined. This
##		file also contains the (ugly) fileselector.
##
##  Help.tcl	Some help dialogs.
##
##  Misc.tcl    This file also contains some utility proc's.
##
##  init.tcl    The initialization and customization file.
##
## The object handling code is written in as ordinary C code.
## All operations are performed on the C objects. Callbacks to
## tk procedures are used to actually modify the picture shown
## by tkined.
##

##
## This precision is need by the blt graph widget to correctly
## convert times since 1970.
##

set tcl_precision 12

##
## This nice procedure allows us to use static variables. It was
## posted on the net by Karl Lehenbauer.
##

proc static {args} {
    set procName [lindex [info level [expr [info level]-1]] 0]
    foreach varName $args {
        uplevel 1 "upvar #0 {$procName:$varName} $varName"
    }
}

##
## Handle background errors here.
##

proc bgerror { message } {
    global errorInfo tkined
    if {$tkined(debug)} {
	puts stderr $errorInfo
    }
    bell
    # destroy .
}

##
## Redefine the builtin grab command to handle multiple 
## grab requests.
##

# rename grab tkgrab
# proc grab { args } { 
#    while {[catch {eval tkgrab $args} err]} {
# 	after 1000
#     }
# }


## OLD UGLY STUFF TO BE REMOVED IN FUTURE RELEASES!!

##
## This proc is used to get a file given by an URL via ftp.
##

proc tkined_ftp {server remotefile localfile} {
    
    global env
    
    if {[catch {exec hostname} hostname]} {
	set hostname unknown
    }

    set name tkined
    if {[info exists env(USER)]} {
	set name $env(USER)
    } elseif {[info exists env(LOGNAME)]} {
	set name $env(LOGNAME)
    }
    
    catch {file delete -force $localfile}
	
    if {[catch {
	set f [open "| ftp -n" "r+"]
	puts $f "open $server"
	puts $f "user anonymous $name@$hostname"
	puts $f "get $remotefile $localfile"
	puts $f "close"
	flush $f
	close $f} err 
       ]} {
	   error "ftp failed: $err"
       }

    if {![file exists $localfile]} {
	error "ftp failed"
    }
}

