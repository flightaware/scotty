##
## Diagram.tcl
##
## This file contains the procedures that are used to create and
## manipulate Diagram toplevel windows.
##
## Copyright (c) 1994, 1995
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

package provide TkinedDiagram 1.5.1

proc Diagram__create { editor } {
    set w [$editor toplevel].diagram
    toplevel $w -class tkined
    wm withdraw $w
    wm iconbitmap $w graph
    wm iconname $w "graph"
    wm geometry $w 400x220
    wm minsize $w 400 220
    frame $w.menu -relief raised -borderwidth 1
    pack $w.menu -fill x

    blt_graph $w.blt -width 1000 -height 800 -relief raised
    $w.blt xaxis configure -title ""
    $w.blt yaxis configure -title ""
    $w.blt xaxis configure -command "XLocalTime"
    bind $w.blt <Button-1> "bb %W %x %y %X %Y"
    bind $w.blt <Button-3> "aa %W %x %y 1"
    bind $w.blt <Shift-Button-3> "aa %W %x %y 0"
    pack $w.blt

    menubutton $w.menu.diag -text "Diagram" -menu $w.menu.diag.m
    pack $w.menu.diag -side left
    menu $w.menu.diag.m
    $w.menu.diag.m add command -label "Logarithmic Y Scale" \
	    -command "$w.blt yaxis configure -logscale on"
    $w.menu.diag.m add command -label "Linear Y Scale" \
	    -command "$w.blt yaxis configure -logscale off"
    $w.menu.diag.m add command -label "Limit Axis" \
	    -command "Diagram__SetLimits $editor"
    $w.menu.diag.m add command -label "PostScript" \
	    -command "Diagram__PostScript $editor"

    menubutton $w.menu.edit -text "Edit" -menu $w.menu.edit.m
    pack $w.menu.edit -side left
    menu $w.menu.edit.m
    $w.menu.edit.m add command -label "Delete" \
	-accelerator " ^D" \
	-command "$w.menu.edit configure -state disabled; update idletasks; \
                  Diagram__Delete $editor; \
                  $w.menu.edit configure -state normal"
    bind $w <Control-d> "$w.menu.edit.m invoke 8"

    menubutton $w.menu.icon -text "Icon" -menu $w.menu.icon.m
    pack $w.menu.icon -side left
    menu $w.menu.icon.m

    $w.menu.icon.m add cascade -label "Dashes" -menu $w.menu.icon.m.dashes
    menu $w.menu.icon.m.dashes
    set i 1
    while {[set val [$editor attribute dashes$i]]!=""} {
	set name [lrange $val 1 end]
	if {$name == ""} {
            $w.menu.icon.m.dashes add sep
        } else {
	    tkined_makemenu $w.menu.icon.m.dashes $name last name
	    $last add radio -label " $name" \
		-variable network$w -value $last$name \
		-command "$w.menu.icon configure -state disabled; \
                      update idletasks; \
                      Diagram__Icon $editor {$name}; \
                      $w.menu.icon configure -state normal"
	}
        incr i
    }
    $w.menu.icon.m.dashes add radio -label " Solid" \
	-variable network$w -value network \
	-command "$w.menu.icon configure -state disabled;
                  update idletasks; \
                  Diagram__Icon $editor solid; \
                  $w.menu.icon configure -state normal"
    $w.menu.icon.m.dashes invoke $i

    if {[winfo cells .] > 2} { 
	$w.menu.icon.m add cascade -label "Color" -menu $w.menu.icon.m.color
    }
    if {[winfo cells .] > 2} { 
	menu $w.menu.icon.m.color
	set i 1
	while {[set val [$editor attribute color$i]]!=""} {
	    set name [lrange $val 1 end]
	    if {$name == ""} {
		$w.menu.icon.m.color add sep
	    } else {
		tkined_makemenu $w.menu.icon.m.color $name last name
		$last add radio -label " $name" \
		    -variable color$w -value $name \
		    -command "$w.menu.icon configure -state disabled;
                          update idletasks; \
                          Diagram__Color $editor {$name}; \
                          $w.menu.icon configure -state normal"
		$editor attribute color-$name [lindex $val 0]
	    }
	    incr i
	}
	$w.menu.icon.m.color add radio -label " Black" \
	    -variable color$w -value Black \
	    -command "$w.menu.icon configure -state disabled;
                  update idletasks; \
                  Diagram__Color $editor Black; \
                  $w.menu.icon configure -state normal"
	$editor attribute color-Black black
	$w.menu.icon.m.color invoke $i
    }

    menubutton $w.menu.view -text "View" -menu $w.menu.view.m
    pack $w.menu.view -side left
    menu $w.menu.view.m
    $w.menu.view.m add command -label "Close View" \
	    -command "Diagram__Close $editor"

    # now its time to map the whole thing on the screen
    update
    wm deiconify $w
    update

    return $w
}

proc Diagram__Delete { editor } {
    set w [$editor graph]
    foreach graph [$w.blt element names] {
	if {[$graph selected]} {
	    $graph delete
	}
    }
}

proc Diagram__Close { editor } {
    set w [$editor graph]
    foreach graph [$w.blt element names] {
	$graph delete
    }
    destroy $w
}

proc Diagram__Color { editor color } {
    set w [$editor graph]
    foreach graph [$w.blt element names] {
	if {[$graph selected]} {
	    $graph color $color
	}
    }
}

proc Diagram__Icon { editor icon } {
    set w [$editor graph]
    foreach graph [$w.blt element names] {
	if {[$graph selected]} {
	    $graph icon $icon
	}
    }
}

proc Diagram__SetLimits { editor } {
    set w [$editor graph]
    set xmin [lindex [$w.blt xaxis configure -min] 4]
    set xmax [lindex [$w.blt xaxis configure -max] 4]
    set ymin [lindex [$w.blt yaxis configure -min] 4]
    set ymax [lindex [$w.blt yaxis configure -max] 4]
    set result [Dialog__request $w "Set the limits on the axis:" [list \
		   [list "Min X Axis:" $xmin ] [list "Max X Axis:" $xmax ] \
		   [list "Min Y Axis:" $ymin ] [list "Max Y Axis:" $ymax ] \
	         ] [list "set limits" cancel] ]
    if {[lindex $result 0] == "cancel"} return
    catch {
	$w.blt xaxis configure -min [lindex $result 1]
	$w.blt xaxis configure -max [lindex $result 2]
	$w.blt yaxis configure -min [lindex $result 3]
	$w.blt yaxis configure -max [lindex $result 4]
    }
}

proc Diagram__XLabel { w x } {
    set h [expr (int($x) / 3600) % 24]
    set m [expr (int($x) / 60) % 60]
    return [format "%02s:%02s" $h $m]
}

proc Diagram__PostScript { editor } {
    set w [$editor graph]
    set types {
	{{PostScript}   {.ps}  }
	{{All Files}    *       }
    }
    set fname [tk_getSaveFile -defaultextension .ps -filetypes $types \
	    -parent $w -title "Save PostScript to file:"]
    if {$fname == ""} return

    if {[file exists $fname]} {
	if {![file writable $fname]} {
	    Dialog__acknowledge $w "Can not write $fname"
	    return
	}
	set res [Dialog__confirm $w "Replace file $fname?" \
		 [list replace cancel]]
	if {$res == "cancel"} return
    }

    if {[catch {open $fname w} f]} {
	Dialog__acknowledge $w "Can not open $fname: $f"
	return
    }

    if {[catch {puts $f [$w.blt postscript]} err]} {
	Dialog__acknowledge $w "Failed to write $fname: $err"
    }

    catch {close $f}
    return
}

proc aa {w x y unsel} {
    set g [lindex [$w element closest $x $y] 0]
    if {$g == ""} {
	foreach id [$editor selection] {
            $id unselect
        }
	return
    }
    set editor [$g editor]
    if {$unsel} {
	foreach id [$editor selection] {
	    $id unselect
	}
    }
    if {[$g selected]} {
	$g unselect
    } else {
	$g select
    }
}

proc bb {w x y X Y} {
    set g [lindex [$w element closest $x $y] 0]
    if {$g == ""} return
    menu $w.popup
    Tool__MakePopup $w.popup $g
    $w.popup add separator
    $w.popup add command -label "Clear graph" \
	    -command "$g clear; destroy $w.popup"
    tk_popup $w.popup [incr X -20] [incr Y -50]
}

