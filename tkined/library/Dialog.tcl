##
## Dialog.tcl
##
## This file contains the dialogs used by the tkined editor. The
## Help dialogs are located in Help.tcl as we do not change them
## so frequently.
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

package provide TkinedDialog 1.6.0

##
## Compute the geometry of a dialog window w based on the geometry
## of the toplevel window top. Unfortunately we can not get the
## size before a window is mapped.
##

proc tkined_position {w top} {
    global tcl_platform

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
}

##
## Make a dialog toplevel window with the given dialog name.
##

proc tkined_dialog_toplevel {w} {
    catch {destroy $w}
    toplevel $w
    wm title $w [winfo name $w]
    wm protocol $w WM_DELETE_WINDOW { }
    wm transient $w [winfo toplevel [winfo parent $w]]
}

##
## Make a title for a dialog. Create a message containing the lines
## given in the lines argument.
##

proc tkined_dialog_title {w lines} {
    set text ""
    foreach line $lines { append text "$line\n" }
    message $w -aspect 25000 -text $text
    pack $w -side top
}

##
## Most dialogs have a list of buttons in the bottom. This proc
## creates these buttons and makes the first one the default.
##

proc tkined_dialog_buttons {w blist} {
    set arg [lindex $blist 0]
    frame $w.0 -relief sunken -border 1
    pack $w.0 -side left -expand yes -padx 8 -pady 8
    button $w.0.button -text [lindex $arg 0] \
	-command "[lindex $arg 1]; destroy $w"
    pack $w.0.button -expand yes -padx 2 -pady 2 -ipadx 2
    bind $w <Return> "$w.0.button invoke"
    focus $w
    
    set i 1
    foreach arg [lrange $blist 1 end] {
	button $w.$i -text [lindex $arg 0] \
	    -command "[lindex $arg 1]; destroy $w"
	pack $w.$i -side left -expand yes -padx 10 -ipadx 2
	incr i
    }
}

##
## Make sure that we don't conflict with other grabs. We simply try
## to re-install the focus and grab as it was before we post the 
## dialog.
##

proc tkined_grab {w} {
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
}

##
## An acknowledge dialog takes its arguments, puts them into a
## message and displays them until the user hits the dismiss 
## button.
##

proc Dialog__acknowledge {c args} {
    set w "$c.acknowledge"

    tkined_dialog_toplevel $w
    tkined_dialog_title $w.title $args
    tkined_dialog_buttons $w "dismiss"

    tkined_position $w $c
    tkined_grab $w
}

##
## A confirm dialog takes its arguments, puts them into a
## message and displays them until the user hits the yes, no
## or cancel button.
##

proc Dialog__confirm {c args} {
    global tkined_confirm_result

    set tkined_confirm_result ""
    set w "$c.confirm"

    tkined_dialog_toplevel $w
    set idx [llength $args]; incr idx -1
    set buttons [lindex $args $idx]; incr idx -1
    tkined_dialog_title $w.title [lrange $args 0 $idx]

    if {$buttons == ""} { set buttons dismiss }    
    foreach button $buttons {
	lappend foobar [list $button [list set tkined_confirm_result $button]]
    }
    tkined_dialog_buttons $w $foobar

    tkined_position $w $c
    tkined_grab $w
    return $tkined_confirm_result
}

##
## Browse a long list of text. This dialog opens a text widget
## with a scrollbar. The dialog is finished when the dismiss
## button is invoked.
##

proc Dialog__browse {c args} {
    set w "$c.browse"

    tkined_dialog_toplevel $w
    set idx [llength $args]; incr idx -1
    set lastarg [lindex $args $idx]; incr idx -1
    tkined_dialog_title $w.title [lrange $args 0 $idx]
    regsub "\n" $lastarg " " lastarg

    frame $w.b
    set width 20
    set height 1
    set msg ""
    foreach line $lastarg {
	if {$line == "\n"} continue
	set line [string trimright $line]
	append msg "$line\n"
	set len [string length $line]
	if {$len>$width} { set width $len }
	incr height
    }
    if {$height>24} { set height 24 }
    incr width
    
    set scroll [expr {$height > 10}]

    if {$scroll} {
	text $w.b.txt -width $width -height $height \
	    -yscrollcommand "$w.b.scrollbar set" -setgrid true \
	    -borderwidth 2 -relief flat -wrap none
	scrollbar $w.b.scrollbar -command "$w.b.txt yview" -relief sunken
    } else {
	text $w.b.txt -width $width -height $height -setgrid true \
	    -borderwidth 2 -relief flat -wrap none
    }
    $w.b.txt insert 0.0 $msg
    $w.b.txt configure -state disabled

    if {$scroll} {
	pack $w.b.scrollbar -side right -fill y
    }
    pack $w.b.txt -side left -fill both -expand yes
    pack $w.b  -side top -padx 10 -pady 10 -fill both -expand yes
    tkined_dialog_buttons $w "dismiss"

    tkined_position $w $c
    tkined_grab $w
}

##
## This dialog displays a list and lets the users select items from
## the list. It returns the list of selected items.
##

proc Dialog__list {c args} {
    global tkined_list_result
    set tkined_list_result ""
    set w "$c.list"

    tkined_dialog_toplevel $w
    set idx [llength $args]; incr idx -1
    set buttons [lindex $args $idx]; incr idx -1
    set lastarg [lindex $args $idx]; incr idx -1
    tkined_dialog_title $w.title [lrange $args 0 $idx]

    # compute the width and height of the listbox
    set width 24
    set height 8
    foreach elem $lastarg {
	set l [string length $elem]
	incr l
	if {($l > $width) && ($l <= 80)} {
	    set width $l
	}
    }
    set h [llength $lastarg]
    if {($h > $height)} {
	set height [expr {($h <= 24) ? $h : 24}]
    }

    frame $w.box
    scrollbar $w.box.scroll -command "$w.box.list yview" -relief sunken
    listbox $w.box.list -yscroll "$w.box.scroll set" -relief sunken \
	-width $width -height $height
    foreach elem $lastarg {
	$w.box.list insert end $elem
    }
    pack $w.box.scroll -side right -fill y
    pack $w.box.list -side left -expand true -fill both
    pack $w.box -side top -padx 10 -pady 10 -expand true -fill both

    if {$buttons == ""} { set buttons dismiss }
    foreach button $buttons {
	lappend foobar [list $button [list tkined_list_result $w $button]]
    }
    tkined_dialog_buttons $w $foobar

    bind $w.box.list <Double-Button-1> \
        "%W selection set \[%W nearest %y\];
         tkined_list_result $w {}; destroy $w; break"

    tkined_position $w $c
    tkined_grab $w
    return $tkined_list_result
}

proc tkined_list_result { w button } {
    global tkined_list_result
    set tkined_list_result [list $button]
    set done 0
    foreach i [$w.box.list curselection] {
	lappend tkined_list_result [$w.box.list get $i]
	set done 1
    }
    if {! $done} {
	lappend tkined_list_result ""
    }
}

##
## A request dialog displays a message and asks the user to enter
## something in a request box. The last argument is a default for
## the entry widget.
##

proc Dialog__request {c args} {
    global tkined_request_result

    set w "$c.request"
    set tkined_request_result ""

    tkined_dialog_toplevel $w
    set idx [llength $args]; incr idx -1
    set buttons [lindex $args $idx]; incr idx -1
    set lastarg [lindex $args $idx]; incr idx -1
    tkined_dialog_title $w.title [lrange $args 0 $idx]

    set idx 1
    set tlist ""
    frame $w.ia
    frame $w.ia.msg
    frame $w.ia.entry
    foreach elem $lastarg {
	label $w.ia.msg.$idx -text [lindex $elem 0]
	pack $w.ia.msg.$idx -side top -ipady 3
	switch [lindex $elem 2] {
	    scale {
		frame $w.ia.entry.$idx
		label $w.ia.entry.$idx.l -width 4 -font fixed
		scale $w.ia.entry.$idx.s -orient h -label "" -showvalue false \
		    -command "$w.ia.entry.$idx.l configure -text"
		set from [expr round([lindex $elem 3])]
		set to   [expr round([lindex $elem 4])]
		if {[lindex $elem 3] != ""} {
		    catch {$w.ia.entry.$idx.s configure -from $from}
		}
		if {[lindex $elem 4] != ""} {
		    catch {$w.ia.entry.$idx.s configure -to $to}
		}
		set val [expr round([lindex $elem 1])]
		catch {$w.ia.entry.$idx.s set $val} err
		pack $w.ia.entry.$idx.l -side left
		pack $w.ia.entry.$idx.s -side right -fill x -expand true
		pack $w.ia.entry.$idx -side top -fill both -expand true 
	    }
	    logscale {
		frame $w.ia.entry.$idx
		label $w.ia.entry.$idx.l -width 4 -font fixed
		scale $w.ia.entry.$idx.s -orient h -label "" -showvalue false \
		    -command "$w.ia.entry.$idx.l configure -text"
		set from [expr round(exp([lindex $elem 3]))]
		set to [expr round(exp([lindex $elem 4]))]
		if {[lindex $elem 3] != ""} {
		    catch {$w.ia.entry.$idx.s configure -from $from}
		}
		if {[lindex $elem 4] != ""} {
		    catch {$w.ia.entry.$idx.s configure -to $to}
		}
		set val [expr round(exp([lindex $elem 1]))]
		catch {$w.ia.entry.$idx.s set $val}
		pack $w.ia.entry.$idx.l -side left
		pack $w.ia.entry.$idx.s -side right -fill x -expand true
		pack $w.ia.entry.$idx -side top -fill both \
		    -expand true -padx 1 -pady 1
	    }
	    radio {
		frame $w.ia.entry.$idx
		set i 0
		foreach word [lrange $elem 3 end] {
		    radiobutton $w.ia.entry.$idx.$i -text $word -relief flat \
			-variable tkined_request_value_$idx -value $word
		    pack $w.ia.entry.$idx.$i -side left -fill x -expand true
		    if {$word == [lindex $elem 1]} {
			catch { $w.ia.entry.$idx.$i invoke }
		    }
		    incr i
		}
		pack $w.ia.entry.$idx -side top -fill both \
		    -expand true -padx 1 -pady 1
	    }
	    check {
		frame $w.ia.entry.$idx
		global tkined_request_value
                set i 0
                foreach word [lrange $elem 3 end] {
		    set tkined_request_value($idx,$i) ""
		    checkbutton $w.ia.entry.$idx.$i -text $word -relief flat \
			-variable tkined_request_value($idx,$i) \
			-offvalue "" -onvalue $word
		    pack $w.ia.entry.$idx.$i -side left -fill x -expand true
		    if {[lsearch [lindex $elem 1] $word] >= 0} {
                        catch { $w.ia.entry.$idx.$i invoke }
                    }
                    incr i
                }
		pack $w.ia.entry.$idx -side top -fill both \
		    -expand true -padx 1 -pady 1
	    }
	    option {
		eval tk_optionMenu $w.ia.entry.$idx \
		    tkined_request_value_$idx [join [lrange $elem 3 end]]
		$w.ia.entry.$idx configure \
		    -highlightthickness 0 -pady 0
		[winfo child $w.ia.entry.$idx] configure -tearoff false 
		pack $w.ia.entry.$idx -side top -fill both \
		    -expand true -padx 1 -pady 2
	    }
	    entry {
		entry $w.ia.entry.$idx -width [lindex $elem 3] -relief sunken
                $w.ia.entry.$idx insert 0 [lindex $elem 1]
                pack $w.ia.entry.$idx -side top -fill both \
                    -expand true -padx 1 -pady 1
	    }
	    default {
		entry $w.ia.entry.$idx -width 40 -relief sunken
		$w.ia.entry.$idx insert 0 [lindex $elem 1]
		pack $w.ia.entry.$idx -side top -fill both \
		    -expand true -padx 1 -pady 1
	    }
	}

	lappend tlist [lindex $elem 2]
	incr idx
    }
    pack $w.ia.msg -side left
    pack $w.ia.entry -side right -padx 10
    pack $w.ia -side top

    if {$buttons == ""} { set buttons dismiss }
    foreach button $buttons {
	lappend foobar [list $button \
			     [list tkined_request_result $w $tlist $button]]
    }

    tkined_dialog_buttons $w $foobar
    tkined_position $w $c
    tkined_grab $w
    return $tkined_request_result
}

proc tkined_request_result { w tlist button } {
    global tkined_request_result
    set idx 1
    set tkined_request_result [list $button]
    foreach t $tlist {
	switch $t {
	    scale {
		set val [$w.ia.entry.$idx.s get]
		lappend tkined_request_result $val
	    }
	    logscale {
		set val [$w.ia.entry.$idx.s get]
		lappend tkined_request_result [expr log($val)]
	    }
	    radio {
		global tkined_request_value_$idx
		lappend tkined_request_result [set tkined_request_value_$idx]
	    }
	    check {
		global tkined_request_value
		set aa ""
		foreach name [array names tkined_request_value] {
		    if {[string match "$idx,*" $name]} {
			if {$tkined_request_value($name) != ""} {
			    lappend aa $tkined_request_value($name)
			}
		    }
		}
		lappend tkined_request_result $aa
	    }
	    option {
		global tkined_request_value_$idx
		lappend tkined_request_result [set tkined_request_value_$idx]
	    }
	    default {
		lappend tkined_request_result [$w.ia.entry.$idx get]
	    }
	}
	incr idx
    }
}

##
## Select a file to read.
##

proc Dialog__openfile {c {title {Select a file to read:}} {fname {}}} {
    set w [winfo toplevel $c]
    set types {
	{{All Files}    *       }
	{{Tcl Files}	{.tcl}  }
	{{Text Files}	{.txt}  }
    }
    tk_getOpenFile -defaultextension * -filetypes $types \
            -parent $w -title $title -initialdir [file dirname $fname] \
	    -initialfile [file tail $fname]
}

##
## Select a file to write.
##

proc Dialog__savefile {c {title {Select a file to write:}} {fname {}}} {
    set w [winfo toplevel $c]
    set types {
	{{All Files}    *       }
	{{Tcl Files}	{.tcl}  }
	{{Text Files}	{.txt}  }
    }
    tk_getSaveFile -defaultextension * -filetypes $types \
            -parent $w -title $title -initialdir [file dirname $fname] \
	    -initialfile [file tail $fname]
}
