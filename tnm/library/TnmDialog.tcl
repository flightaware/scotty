# TnmDialog.tcl --
#
#	This file contains some useful dialogs that I use frequently
#	to implement management scripts.
#
# Copyright (c) 1995-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
# Copyright (c) 1997-1998 Technical University of Braunschweig.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: TnmDialog.tcl,v 1.1.1.1 2006/12/07 12:16:57 karl Exp $

package require Tnm 3.0
package provide TnmDialog 3.0.0

namespace eval TnmDialog {
    namespace export Confirm
}

# TnmDialog::Center --
#
#	Compute the geometry of a dialog window so that it appears
#	centered over a toplevel windows. Unfortunately, we can not
#	get the window size before the window is mapped.
#
# Arguments:
#	w	The dialog window that should be centered.
#	top	The optional toplevel over which the dialog is centered.
# Results:
#	None.

proc TnmDialog::Center {w {top ""}} {
    global tcl_platform

    if {$top == ""} {
	set top [winfo parent $w]
    }
    set top [winfo toplevel $top]
    wm withdraw $w
    update idletasks
	
    if {$top == "."} {
	set x [expr [winfo screenwidth $w]/2 - [winfo reqwidth $w]/2 \
		- [winfo vrootx [winfo parent $w]]]
	set y [expr [winfo screenheight $w]/2 - [winfo reqheight $w]/2 \
		- [winfo vrooty [winfo parent $w]]]
    } else {
	set rx [expr {[winfo rootx $top]+[winfo vrootx $top]}]
	set ry [expr {[winfo rooty $top]+[winfo vrooty $top]}]
	
	set cx [expr $rx+[winfo width $top]/2]
	set cy [expr $ry+[winfo height $top]/2]
	
	set x  [expr $cx-[winfo reqwidth $w]/2]
	set y  [expr $cy-[winfo reqheight $w]/2]
	
	if {$x < 0} { set x 0 }
	if {$y < 0} { set y 0 }
    }
	
    wm geometry $w +$x+$y
    wm deiconify $w

    # work around a windows platform bug:
    if {[string compare $tcl_platform(platform) "windows"] == 0} {
        wm geometry $w +$x+$y
    }

    return
}

# TnmDialog::Wait --
#
#	Wait until the dialog window is destroyed. This proc makes 
#	sure that the grab and focus is re-installed properly.
#
# Arguments:
#	w	The toplevel of the dialog window.
# Results:
#	None.

proc TnmDialog::Wait w {
    set oldFocus [focus]
    set oldGrab [grab current $w]
    if {$oldGrab != ""} {
        set grabStatus [grab status $oldGrab]
    }
    grab $w
    tkwait visibility $w
    focus $w
    tkwait window $w
    catch {focus $oldFocus}
    if {$oldGrab != ""} { 
	if {$grabStatus == "global"} {
	    grab -global $oldGrab
	} else {
	    grab $oldGrab
	}
    }
    return
}

# TnmDialog::Toplevel --
#
#	Create a toplevel window that can be used to build dialogs.
#
# Arguments:
#	w	The toplevel window to create.
# Results:
#	None.

proc TnmDialog::Toplevel w {
    catch {destroy $w}
    toplevel $w
    wm title $w [winfo name $w]
    wm protocol $w WM_DELETE_WINDOW { }
    wm transient $w [winfo toplevel [winfo parent $w]]
    return
}

# TnmDialog::Text --
#
#	Every dialog window has some text at the top which describes
#	the purpose of the dialog. This procedure is used to put this
#	text into the dialog window. A bitmap can be defined which
#	is show on the left side of the dialog window.
#
# Arguments:
#	w	The toplevel of the dialog window.
#	bitmap	The bitmap to show on the left side of the text.
#	text	The text to display.
# Results:
#	None.

proc TnmDialog::Text {w bitmap text} {
    frame $w.top -relief raised -bd 1
    catch {
	label $w.top.b -bitmap $bitmap
	pack $w.top.b -side left -padx 3m -pady 3m
    }
    label $w.top.l -text $text
    pack $w.top.l -fill x -padx 2m -pady 2m
    pack $w.top -fill x
    return
}

# TnmDialog::Buttons --
#
#	Dialogs have a list of buttons in the bottom. This procedure
#	creates these buttons and makes the first one the default.
#
# Arguments:
#	w		The toplevel of the dialog window.
#	buttons		The list of button names.
# Results:
#	None.

proc TnmDialog::Buttons {w buttons} {
    frame $w.bot -relief raised -bd 1
    frame $w.bot.0 -relief sunken -border 1
    pack $w.bot.0 -side left -expand yes -padx 8 -pady 8
    set arg [lindex $buttons 0]
    button $w.bot.0.button -text $arg \
	    -command "[list set result $arg] ; destroy $w"
    pack $w.bot.0.button -expand yes -padx 2 -pady 2 -ipadx 2
    bind $w <Return> "$w.bot.0.button invoke"
    focus $w
    
    set i 1
    foreach arg [lrange $buttons 1 end] {
	button $w.bot.$i -text $arg \
		-command "[list set result $arg] ; destroy $w"
	pack $w.bot.$i -side left -expand yes -padx 10 -ipadx 2
	incr i
    }
    pack $w.bot -fill x
    return
}

# TnmDialog::Confirm --
#
#	Display a message and let the user confirm the message by 
#	klicking on one of several buttons.
#
# Arguments:
#	w		The toplevel of the dialog window.
#	bitmap		The bitmap to show on the left side of the dialog.
#	text		The text to display right beside the bitmap.
#	buttons		List of buttons to show in the bottom of the dialog.
# Results:
#	Returns the name of the button selected by the user.

proc TnmDialog::Confirm {w bitmap text buttons} {
    global result
    TnmDialog::Toplevel $w
    TnmDialog::Text $w $bitmap $text
    TnmDialog::Buttons $w $buttons
    TnmDialog::Center $w
    TnmDialog::Wait $w
    return $result
}

# TnmDialog::Browse
#
#	Display a lengthy message and let the user confirm the 
#	message by klicking on one of several buttons. This
#	dialog allows to display texts of arbitrary length.
#
# Arguments:
#	w		The toplevel of the dialog window.
#	title		The title to display above the text.
#	text		The text to display right beside the bitmap.
#	buttons		List of buttons to show in the bottom of the dialog.
# Results:
#	Returns the name of the button selected by the user.

proc TnmDialog::Browse {w title text buttons} {
    global result
    TnmDialog::Toplevel $w
    TnmDialog::Text $w "" $title
    frame $w.box -relief raised -bd 1
    scrollbar $w.box.scroll -command "$w.box.text yview" -relief sunken
    text $w.box.text -yscroll "$w.box.scroll set" -relief sunken
    $w.box.text insert 0.0 $text 
    $w.box.text configure -state disabled
    pack $w.box.scroll -side right -fill y
    pack $w.box.text -fill both -expand true
    pack $w.box -expand true -fill both
    TnmDialog::Buttons $w $buttons
    TnmDialog::Center $w
    TnmDialog::Wait $w
    return $result
}


# Tnm_DialogRequest --
#
# Request a simple line of input from the user.
#
# Arguments:
# w -		The dialog window.
# bitmap -	The bitmap to show on the left side of the dialog.
# text -	The text to display right beside the bitmap.
# value -	The default value to be edited by the user.
# buttons -	A list of buttons to show in the bottom of the dialog.

proc Tnm_DialogRequest {w bitmap text value buttons} {
    global result result$w
    TnmDialog::Toplevel $w
    TnmDialog::Text $w $bitmap $text
    entry $w.top.e -textvariable result$w
    set result$w $value
    bind $w.top.e <Return> "$w.bot.0.button invoke; break"
    pack $w.top.e -fill both -padx 3m -pady 3m
    TnmDialog::Buttons $w $buttons
    TnmDialog::Center $w
    TnmDialog::Wait $w
    return [list $result [set result$w]]
}


# Tnm_DialogSelect --
#
# Select an element out of a list of elements using a listbox.
#
# Arguments:
# w -		The dialog window.
# bitmap -	The bitmap to show on the left side of the dialog.
# text -	The text to display right beside the bitmap.
# list -	The list of elements presented to the user.
# buttons -	A list of buttons to show in the bottom of the dialog.

proc Tnm_DialogSelect {w bitmap text list buttons} {
    global result result$w
    TnmDialog::Toplevel $w
    TnmDialog::Text $w $bitmap $text
    frame $w.box -relief raised -bd 1
    scrollbar $w.box.scroll -command "$w.box.list yview" -relief sunken
    listbox $w.box.list -yscroll "$w.box.scroll set" -relief sunken
    foreach elem $list {
        $w.box.list insert end $elem
    }
    set result$w ""
    bind $w.box.list <ButtonRelease-1> \
	    "+set result$w \[%W get \[%W curselection\]\]"
    bind $w.box.list <Double-Button-1> "$w.bot.0.button invoke; break"
    pack $w.box.scroll -side right -fill y
    pack $w.box.list -side left -expand true -fill both
    pack $w.box -expand true -fill both
    TnmDialog::Buttons $w $buttons
    TnmDialog::Center $w
    TnmDialog::Wait $w
    return [list $result [set result$w]]
}


# Tnm_DialogConfigureSnmpSession --
#
# This dialog allows to configure SNMP session (SNMPv1, SNMPv2c, 
# SNMPv2u). This a special purpose dialog that may be used by 
# some applications.
#
# Arguments:
# w -		The dialog window.
# alias -	The snmp session alias to configure.

proc Tnm_DialogConfigureSnmpSession {w alias} {
    global result
    global value
    catch {unset value}
    TnmDialog::Toplevel $w
    TnmDialog::Text $w questhead "Configure SNMP session alias \"$alias\":"
    set s [snmp generator -alias $alias]
    frame $w.s
    frame $w.s.fa
    label $w.s.fa.lc -text Community:
    label $w.s.fa.lu -text User:
    label $w.s.fa.lw -text Password:
    label $w.s.fa.lx -text Context:
    label $w.s.fa.la -text Address:
    label $w.s.fa.lp -text Port:
    entry $w.s.fa.ec -textvariable value(ec)
    entry $w.s.fa.eu -textvariable value(eu)
    entry $w.s.fa.ew -show * -textvariable value(ew)
    entry $w.s.fa.ex -textvariable value(ex)
    entry $w.s.fa.ea -textvariable value(ea)
    entry $w.s.fa.ep -textvariable value(ep)

    switch [$s cget -version] {
	SNMPv1 -
	SNMPv2c {
	    set value(ec) [$s cget -read]
	    grid $w.s.fa.lc $w.s.fa.ec -ipady 1 -sticky e
	    $w.s.fa.lw configure -text ""
	    grid $w.s.fa.lw -ipady 1 -sticky e
	    $w.s.fa.lx configure -text ""
	    grid $w.s.fa.lx -ipady 1 -sticky e
	}
	SNMPv2u {
	    set value(eu) [$s cget -user]
	    grid $w.s.fa.lu $w.s.fa.eu -ipady 1 -sticky e
	    set value(ew) [$s cget -password]
	    grid $w.s.fa.lw $w.s.fa.ew -ipady 1 -sticky e
	    set value(ex) [$s cget -context]
	    grid $w.s.fa.lx $w.s.fa.ex -ipady 1 -sticky e
	}
	default {
	    destroy $w
	    TnmDialog::Confirm $w error \
		    "Unable to configure [$s cget -version] session!" dismiss
	    return
	    error "unable to configure historic SNMPv2CLASSIC"
	}
    }
    set value(ea) [$s cget -address]
    grid $w.s.fa.la $w.s.fa.ea -ipady 1 -sticky e
    set value(ep) [$s cget -port]
    grid $w.s.fa.lp $w.s.fa.ep -ipady 1 -sticky e

    frame $w.s.fg
    scale $w.s.fg.t -orient horizontal -from 1 -to 60  -label {Timeout [s]:} \
	    -command "$s configure -timeout"
    $w.s.fg.t set [$s cget -timeout]
    scale $w.s.fg.r -orient horizontal -from 1 -to 10  -label Retries: \
	    -command "$s configure -retries"
    $w.s.fg.r set [$s cget -retries]
    scale $w.s.fg.w -orient horizontal -from 0 -to 100 -label Window: \
	    -command "$s configure -window"
    $w.s.fg.w set [$s cget -window]
    scale $w.s.fg.d -orient horizontal -from 0 -to 100 -label {Delay [ms]:} \
	    -command "$s configure -delay"
    $w.s.fg.d set [$s cget -delay]

    grid $w.s.fg.t $w.s.fg.w
    grid $w.s.fg.r $w.s.fg.d

    pack $w.s.fa $w.s.fg -side left -padx 4 -pady 4 -fill x -expand true
    pack $w.s

    TnmDialog::Buttons $w "accept cancel"
    TnmDialog::Center $w
    TnmDialog::Wait $w

    $s configure -address $value(ea) -port $value(ep)
    switch [$s cget -version] {
	SNMPv1 -
	SNMPv2c {
	    $s configure -read $value(ec)
	}
	SNMPv2u {
	    $s configure -user $value(eu) \
			 -password $value(ew) \
			 -context $value(ex)
	}
    }

    if {$result == "accept"} {
        set result [$s configure]
    } else {
        set result ""
    }
    catch {$s destroy}
    catch {unset value}
    return $result 
}

