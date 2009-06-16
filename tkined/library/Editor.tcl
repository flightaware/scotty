##
## Editor.tcl
##
## This file contains the definition of a Tkined editor. The
## procs in this file are mostly callbacks that are called from
## an editor object, which can be found in editor.c.
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

package provide TkinedEditor 1.5.0

event add <<TkiClear>>	<Alt-c> <Alt-C> <Meta-c> <Meta-C>
event add <<TkiOpen>>	<Alt-o> <Alt-O> <Meta-o> <Meta-O>
event add <<TkiMerge>>	<Alt-m> <Alt-M> <Meta-m> <Meta-M>
event add <<TkiSaveAs>>	<Alt-a> <Alt-A> <Meta-a> <Meta-A>
event add <<TkiPrint>>	<Alt-p> <Alt-P> <Meta-p> <Meta-P>
event add <<TkiTnmMap>>	<Alt-t> <Alt-T> <Meta-t> <Meta-T>
event add <<TkiImport>>	<Alt-i> <Alt-I> <Meta-i> <Meta-I>
event add <<TkiHistory>> <Alt-h> <Alt-H> <Meta-h> <Meta-H>
event add <<TkiNew>>	<Alt-n> <Alt-N> <Meta-n> <Meta-N>
event add <<TkiClose>>	<Alt-w> <Alt-W> <Meta-w> <Meta-W>

##
## Set up a new editor. Create all the menus and the canvas with
## the scollbars.
##

proc Editor__create { editor } { 
    $editor toplevel ".$editor"
}

proc Editor__delete { editor } {
    destroy [$editor toplevel]
}

proc Editor__graph { editor } {
    set w [$editor toplevel]
    set top $w.diagram
    if {! [winfo exists $top]} { 
	Diagram__create $editor
    }
    return $top
}

proc Editor__toplevel { editor } {

    global tkined tcl_platform

    set w [$editor toplevel]

    catch {destroy $w}
    toplevel $w -class tkined -menu $w.menu

    wm withdraw $w
    wm iconbitmap $w icon
    wm iconname $w "tkined"
    wm minsize $w 500 300

    set width [$editor attribute width]
    if {$width == ""} { set width 600 }
    set height [$editor attribute height]
    if {$height == ""} { set height 400 }

    # set up the canvas for the graphic
    canvas $w.canvas -borderwidth 1 -highlightthickness 0 \
	-width $width -height $height -relief groove
    
    # set up the tool box
    frame $w.tools
    
    button $w.tools.select -bitmap tkiSelect \
	-command "Tool__Select $editor"
    button $w.tools.resize -bitmap tkiResize \
	-command "Tool__Resize $editor"
    button $w.tools.text -bitmap tkiText \
	-command "Tool__Text $editor"
    button $w.tools.node -bitmap tkiNode \
	-command "Tool__Node $editor"
    button $w.tools.network -bitmap tkiNetwork \
	-command "Tool__Network $editor"
    button $w.tools.link -bitmap tkiLink \
	-command "Tool__Link $editor"
##    button $w.tools.group -bitmap tkiGroup  \
##	-command "Tool__Group $editor"
    button $w.tools.reference -bitmap tkiRefer  \
	-command "Tool__Reference $editor"
    Tool__Select $editor

    pack $w.tools.select    -side left
    pack $w.tools.resize    -side left
    pack $w.tools.text      -side left
    pack $w.tools.node      -side left
    pack $w.tools.network   -side left
    pack $w.tools.link      -side left
##    pack $w.tools.group     -side left
    pack $w.tools.reference -side left

    if {[$editor attribute zoom] != ""} {
	button $w.tools.up   -bitmap zoomin    \
		-command "ScaleTree $w.canvas 0.8"
	button $w.tools.down -bitmap zoomout  \
		-command "ScaleTree $w.canvas 1.25"
	pack $w.tools.up $w.tools.down -side bottom -side left -fill y
	
	proc ScaleTree {c factor} {
	    global zoom
	    if {![info exists zoom($c)]} {
		set zoom($c) 1.0
	    }
	    set n [expr $zoom($c) * $factor]
	    if {$n > 1 || $n < 0.2} return
	    set zoom($c) $n
	    $c scale all 0 0 $factor $factor
	}
    }

    menubutton $w.tools.color \
	    -textvariable color$w -indicatoron 1 -menu $w.tools.color.menu \
	    -relief raised -bd 2 -highlightthickness 2 -anchor c \
	    -direction flush

    # set up the color menu
    menu $w.tools.color.menu -tearoff 0
    $w.tools.color.menu add radio -label Black \
	    -variable color$w -value Black \
	    -command "Command__Color $editor Black"
    set i 1
    while {[set val [$editor attribute color$i]]!=""} {
	set name [lrange $val 1 end]
	if {$name == ""} {
	    $w.tools.color.menu add sep
	} else {
	    tkined_makemenu $w.tools.color.menu $name last name
	    $last add radio -label $name \
		    -variable color$w -value $name \
		    -command "Command__Color $editor {$name}"
	    $editor attribute color-$name [lindex $val 0]
	}
	incr i
    }
    $w.tools.color.menu invoke Black

    menubutton $w.tools.font \
	    -textvariable font$w -indicatoron 1 -menu $w.tools.font.menu \
            -relief raised -bd 2 -highlightthickness 2 -anchor c \
            -direction flush

    # set up the font menu
    menu $w.tools.font.menu -tearoff 0
    $w.tools.font.menu add radio -label Fixed \
	-variable font$w -value Fixed \
	-command "Command__Font $editor fixed"
    set i 1
    while {[set val [$editor attribute font$i]]!=""} {
	set name $val
	if {$name == ""} {
	    $w.tools.font.menu add sep
	} else {
	    tkined_makemenu $w.tools.font.menu $name last name
	    $last add radio -label $name \
		-variable font$w -value "$name" \
		-command "Command__Font $editor {$name}"
	    $editor attribute font-$name [lindex $val 0]
	}
	incr i
    }
    $w.tools.font.menu invoke Fixed

    pack $w.tools.color $w.tools.font -side right -fill y

    scrollbar $w.hscroll -command "$w.canvas xview" -orient horiz
    scrollbar $w.vscroll -command "$w.canvas yview"
    button $w.yow -bitmap corner -relief flat \
	    -command "Editor__UselessButton $w.canvas"

    grid rowconfig    $w 1 -weight 1 -minsize 0
    grid columnconfig $w 0 -weight 1 -minsize 0

    grid $w.tools -in $w -row 0 -column 0 -columnspan 2 -sticky news
    grid $w.canvas  -in $w -row 1 -column 0 -sticky news
    grid $w.vscroll -in $w -row 1 -column 1 \
            -rowspan 1 -columnspan 1 -sticky news
    grid $w.hscroll -in $w -row 2 -column 0 \
            -rowspan 1 -columnspan 1 -sticky news
    grid $w.yow -in $w -row 2 -column 1 -sticky news

    $w.canvas config \
	-xscrollcommand "$w.hscroll set" -yscrollcommand "$w.vscroll set"

    # set up the menu bar
    menu $w.menu -tearoff 0

    # set up the file menu
    $w.menu add cascade -label "File" -menu $w.menu.file
    menu $w.menu.file
    $w.menu.file add command -label "Clear" \
	-accelerator "  Alt+C" \
	-command "Command__Clear $editor"
    bind $w <<TkiClear>> "$w.menu.file invoke Clear"
    $w.menu.file add command -label "Open..." \
	-accelerator "  Alt+0" \
	-command "Command__Open $editor"
    bind $w <<TkiOpen>> "$w.menu.file invoke Open..."
    $w.menu.file add command -label "Merge..." \
	-accelerator "  Alt+M" \
	-command "Command__Merge $editor"
    bind $w <<TkiMerge>> "$w.menu.file invoke Merge..."
    $w.menu.file add command -label "Save As..." \
	-accelerator "  Alt+A" \
	-command "Command__SaveAs $editor"
    bind $w <<TkiSaveAs>> "$w.menu.file invoke {Save As...}"
    $w.menu.file add sep
    $w.menu.file add command -label "PostScript..." \
	    -accelerator "  Alt+P" \
	    -command "Command__PostScript $editor"
    bind $w <<TkiPrint>>  "$w.menu.file invoke PostScript..."
    $w.menu.file add command -label "Tnm Map..." \
	    -accelerator "  Alt+T" \
	    -command "Command__TnmMap $editor"
    bind $w <<TkiTnmMap>>  "$w.menu.file invoke {Tnm Map...}"
    $w.menu.file add command -label "Import..." \
	-accelerator "  Alt+I" \
	-command "Command__Import $editor"
    bind $w <<TkiImport>> "$w.menu.file invoke Import..."
    $w.menu.file add command -label "History..." \
	-accelerator "  Alt+H" \
	-command "Command__History $editor"
    bind $w <<TkiHistory>> "$w.menu.file invoke History..."
    $w.menu.file add sep
    $w.menu.file add command -label "New View" \
	-accelerator "  Alt+N" \
	-command "EDITOR"
    bind $w <<TkiNew>> "$w.menu.file invoke {New View}"
    $w.menu.file add command -label "Close View" \
	-accelerator "  Alt+W" \
	-command "Command__Close $editor"
    bind $w <<TkiClose>> "$w.menu.file invoke {Close View}"

    # set up the edit menu
    $w.menu add cascade -label "Edit" -menu $w.menu.edit
    menu $w.menu.edit
    $w.menu.edit add cascade -label "Attribute" -menu $w.menu.edit.attr

    menu $w.menu.edit.attr -tearoff false
    $w.menu.edit.attr add command -label Create... \
	-command "Command__Attribute $editor create"
    $w.menu.edit.attr add command -label Delete... \
	-command "Command__Attribute $editor delete"
    $w.menu.edit.attr add command -label Edit... \
	-command "Command__Attribute $editor edit"
    
    $w.menu.edit add command -label "Set Label..." \
	-accelerator "  S" \
	-command "Command__Label $editor"
    bind $w <S> "$w.menu.edit invoke {Set Label...}"
    $w.menu.edit add command -label "Scale Factor..." \
	-accelerator "  F" \
	-command "Command__Scale $editor"
    bind $w <F> "$w.menu.edit invoke {Scale Factor...}"
    $w.menu.edit add sep
    $w.menu.edit add command -label "Grid Spacing..." \
        -accelerator "  s" \
        -command "Command__GridSpacing $editor"
    bind $w <s> "$w.menu.edit invoke {Grid Spacing...}"
    $w.menu.edit add command -label "Align to Grid" \
        -accelerator "  t" \
        -command "Command__Grid $editor"
    bind $w <t> "$w.menu.edit invoke {Align to Grid}"
    $w.menu.edit add sep
    $w.menu.edit add command -label "Undo" \
	-accelerator "  U" \
	-command "Command__Undo $editor"
    bind $w <U> "$w.menu.edit invoke Undo"
    $w.menu.edit add command -label "Redo" \
	-accelerator "  R" \
	-command "Command__Redo $editor"
    bind $w <R> "$w.menu.edit invoke Redo"
    $w.menu.edit add sep
    $w.menu.edit add command -label "Delete" \
	-accelerator " ^D" \
	-command "Command__Delete $editor"
    bind $w <Control-d> "$w.menu.edit invoke Delete"
    $w.menu.edit add command -label "Cut" \
	-accelerator "  x" \
	-command "$editor cut"
    bind $w <x> "$w.menu.edit invoke Cut"
    $w.menu.edit add command -label "Copy" \
	-accelerator "  w" \
	-command "$editor copy"
    bind $w <w> "$w.menu.edit invoke Copy"
    $w.menu.edit add command -label "Paste" \
	-accelerator "  v" \
	-command "$editor paste"
    bind $w <v> "$w.menu.edit invoke Paste"

    # set up the select menu
    $w.menu add cascade -label "Select" -menu $w.menu.select
    menu $w.menu.select
    $w.menu.select add command -label "Select All" \
	-accelerator "  a" \
	-command "Command__SelectAll $editor"
    bind $w <a> "$w.menu.select invoke {Select All}"
    $w.menu.select add sep
    $w.menu.select add command -label "Select Neighbours" \
	-accelerator "  n" \
	-command "Command__SelectNeighbours $editor"
    bind $w <n> "$w.menu.select invoke {Select Neighbours}"
    $w.menu.select add command -label "Select Member" \
	-accelerator "  m" \
	-command "Command__SelectMember $editor"
    bind $w <m> "$w.menu.select invoke {Select Member}"
    $w.menu.select add sep
    $w.menu.select add command -label "Select by Type..." \
	-accelerator "  T" \
	-command "Command__SelectType $editor"
    bind $w <T> "$w.menu.select invoke {Select by Type...}"
    $w.menu.select add command -label "Select by Name..." \
	-accelerator "  N" \
	-command "Command__SelectName $editor"
    bind $w <N> "$w.menu.select invoke {Select by Name...}"
    $w.menu.select add command -label "Select by Address..." \
	-accelerator "  A" \
	-command "Command__SelectAddress $editor"
    bind $w <A> "$w.menu.select invoke {Select by Address...}"
    $w.menu.select add command -label "Select by Label..." \
        -accelerator "  L" \
        -command "Command__SelectLabel $editor"
    bind $w <L> "$w.menu.select invoke {Select by Label...}"
    $w.menu.select add command -label "Select by Attribute..." \
        -accelerator "  V" \
        -command "Command__SelectAttribute $editor"
    bind $w <V> "$w.menu.select invoke {Select by Attribute...}"

    # set up the structure menu
    $w.menu add cascade -label "Structure" -menu $w.menu.structure
    menu $w.menu.structure
    $w.menu.structure add command -label "Bring to Front" \
	-accelerator "  f" \
        -command "Command__Front $editor"
    bind $w <f> "$w.menu.structure invoke {Bring to Front}"
    $w.menu.structure add command -label "Send to Back" \
        -accelerator "  b" \
        -command "Command__Back $editor"
    bind $w <b> "$w.menu.structure invoke {Send to Back}"
    $w.menu.structure add sep
    $w.menu.structure add command -label "Group" \
        -accelerator "  g" \
        -command "Command__Group $editor"
    bind $w <g> "$w.menu.structure invoke Group"
    $w.menu.structure add command -label "Ungroup" \
        -accelerator "  u" \
        -command "Command__Ungroup $editor"
    bind $w <u> "$w.menu.structure invoke Ungroup"
    $w.menu.structure add command -label "Collapse" \
        -accelerator "  c" \
        -command "Command__Collapse $editor"
    bind $w <c> "$w.menu.structure invoke Collapse"
    $w.menu.structure add command -label "Expand" \
        -accelerator "  e" \
        -command "Command__Expand $editor"
    bind $w <e> "$w.menu.structure invoke Expand"
    $w.menu.structure add sep
    $w.menu.structure add command -label "Join Group" \
        -accelerator "  j" \
        -command "Command__Join $editor"
    bind $w <j> "$w.menu.structure invoke {Join Group}"
    $w.menu.structure add command -label "Leave Group" \
        -accelerator "  l" \
        -command "Command__RemoveGroup $editor"
    bind $w <l> "$w.menu.structure invoke {Leave Group}"

    # set up the icon menu
    $w.menu add cascade -label "Icon" -menu $w.menu.icon
    menu $w.menu.icon
    $w.menu.icon add cascade -label "Node" -menu $w.menu.icon.node
    $w.menu.icon add cascade -label "Network" -menu $w.menu.icon.network
    $w.menu.icon add cascade -label "Group" -menu $w.menu.icon.group
    $w.menu.icon add cascade -label "Reference" -menu $w.menu.icon.reference

    # set up the node menu
    menu $w.menu.icon.node
    set i 1
    while {[set val [$editor attribute node$i]]!=""} {
	set name [lrange $val 1 end]
	if {$name == ""} {
            $w.menu.icon.node add sep
        } else {
	    tkined_makemenu $w.menu.icon.node $name last name
	    $last add radio -label " $name" \
		-variable node$w -value $last$name \
		-command "Command__Icon $editor NODE {$name}"
	}
	incr i
    }
    $w.menu.icon.node add radio -label " Machine" \
	-variable node$w -value node \
	-command "Command__Icon $editor NODE node"
    $w.menu.icon.node invoke $i

    # set up the network menu
    menu $w.menu.icon.network
    set i 1
    while {[set val [$editor attribute network$i]]!=""} {
	set name [lrange $val 1 end]
	if {$name == ""} {
            $w.menu.icon.network add sep
        } else {
	    tkined_makemenu $w.menu.icon.network $name last name
	    $last add radio -label " $name" \
		-variable network$w -value $last$name \
		-command "Command__Icon $editor NETWORK {$name}"
	}
        incr i
    }
    $w.menu.icon.network add radio -label " Network" \
	-variable network$w -value network \
	-command "Command__Icon $editor NETWORK network"
    $w.menu.icon.network invoke $i

    # set up the group menu
    menu $w.menu.icon.group
    set i 1
    while {[set val [$editor attribute group$i]]!=""} {
	set name [lrange $val 1 end]
	if {$name == ""} {
            $w.menu.icon.group add sep
        } else {
	    tkined_makemenu $w.menu.icon.group $name last name
	    $last add radio -label " $name" \
		-variable group$w -value $last$name \
		-command "Command__Icon $editor GROUP {$name}"
	}
        incr i
    }
    $w.menu.icon.group add radio -label " Group" \
	-variable group$w -value group \
	-command "Command__Icon $editor GROUP group"
    $w.menu.icon.group invoke $i

    # set up the reference menu
    menu $w.menu.icon.reference
    set i 1
    while {[set val [$editor attribute reference$i]]!=""} {
	set name [lrange $val 1 end]
	if {$name == ""} {
            $w.menu.icon.reference add sep
        } else {
	    tkined_makemenu $w.menu.icon.reference $name last name
	    $last add radio -label " $name" \
		-variable reference$w -value $last$name \
		-command "Command__Icon $editor REFERENCE {$name}"
	}
        incr i
    }
    $w.menu.icon.reference add radio -label " Reference" \
	-variable reference$w -value reference \
	-command "Command__Icon $editor REFERENCE reference"
    $w.menu.icon.reference invoke $i

    # set up the option menu
    $w.menu add cascade -label "Options" -menu $w.menu.opts
    menu $w.menu.opts
    $w.menu.opts add cascade -label "Media" -menu $w.menu.opts.media
    $w.menu.opts add cascade -label "Orientation" -menu $w.menu.opts.orient
    $w.menu.opts add separator
    $w.menu.opts add checkbutton -label "Show Toolbar" \
	-accelerator "  H" \
        -command "Command__ToggleToolBox $editor" \
	-offvalue 0 -onvalue 1 -variable showToolbox$w
    bind $w <H> "$w.menu.opts invoke {Show Toolbar}"
    $w.menu.opts add checkbutton -label "Lock Editor" \
	-accelerator "  C" \
        -command "Command__LockEditor $editor" \
	-offvalue 0 -onvalue 1 -variable lockEditor$w
    bind $w <C> "$w.menu.opts invoke {Lock Editor}"
    $w.menu.opts invoke "Show Toolbar"
    $w.menu.opts add checkbutton -label "Flash Icon" \
	-command "Command__FlashIcon $editor" \
	-offvalue 0 -onvalue 1 -variable flashIcon$w
    if {[string compare $tcl_platform(platform) "unix"] == 0} {
	$w.menu.opts add sep
	$w.menu.opts add checkbutton -label "Strict Motif" \
		-offvalue 0 -onvalue 1 -variable tk_strictMotif
	if {[info commands memory] != ""} {
	    $w.menu.opts add sep
	    $w.menu.opts add checkbutton -label "Memory Validation" \
		    -command {memory validate $memval} \
		    -offvalue off -onvalue on -variable memval
	    set memval off
	    $w.menu.opts add checkbutton -label "Memory Trace" \
		    -command {memory trace $memtrace} \
		    -offvalue off -onvalue on -variable memtrace
	    set memtrace off
	    $w.menu.opts add command -label "Memory Info" \
		    -command "memory info"
	}
    }

    menu $w.menu.opts.media -tearoff false
    global tkined_pageSize$w
    $w.menu.opts.media add radio -label "Letter" \
	-variable tkined_pageSize$w -value Letter \
	-command "$editor pagesize Letter"
    $w.menu.opts.media add radio -label "Legal" \
	-variable tkined_pageSize$w -value Legal \
	-command "$editor pagesize Legal"
    $w.menu.opts.media add radio -label "ISO A4" \
	-variable tkined_pageSize$w -value A4 \
	-command "$editor pagesize A4"
    $w.menu.opts.media add radio -label "ISO A3" \
	-variable tkined_pageSize$w -value A3 \
	-command "$editor pagesize A3"
    $w.menu.opts.media add radio -label "ISO A2" \
	-variable tkined_pageSize$w -value A2 \
	-command "$editor pagesize A2"
    $w.menu.opts.media add radio -label "ISO A1" \
	-variable tkined_pageSize$w -value A1 \
	-command "$editor pagesize A1"
    $w.menu.opts.media add radio -label "ISO A0" \
        -variable tkined_pageSize$w -value A0 \
        -command "$editor pagesize A0"
    $w.menu.opts.media invoke "ISO A4"

    menu $w.menu.opts.orient -tearoff false
    global tkined_orientation$w
    $w.menu.opts.orient add radio -label "Portrait" \
	-variable tkined_orientation$w -value portrait \
	-command "$editor orientation portrait"
    $w.menu.opts.orient add radio -label "Landscape" \
	-variable tkined_orientation$w -value landscape \
	-command "$editor orientation landscape"
    $w.menu.opts.orient invoke Landscape

    # set up the help menu

    if {[string compare $tcl_platform(platform) "unix"] == 0} {
	$w.menu add cascade -label "Help" -menu $w.menu.help
    } else {
	$w.menu add cascade -label "?" -menu $w.menu.help
    }
    menu $w.menu.help
    $w.menu.help add command -label "General" \
	-command "Help__general $editor"
    $w.menu.help add command -label "Status" \
	-command "Help__status $editor"
    $w.menu.help add command -label "Key Bindings" \
	-command "Help__key_bindings $editor"

    focus $w

    # bind the popup menu (alter tool) on the right mouse button

    bind $w.canvas <3> "Tool__AlterMark $editor \
	    \[%W canvasx %x\] \[%W canvasy %y\] %X %Y"
    Editor__unlock $editor

    if {! [Editor__BooleanAttribute $editor optionShowToolbox]} {
	$w.menu.opts invoke {Show Toolbar}
    }
    if {[Editor__BooleanAttribute $editor optionLockEditor]} {
	$w.menu.opts invoke {Lock Editor}
    }
    if {[Editor__BooleanAttribute $editor optionFlashIcon]} {
	$w.menu.opts invoke {Flash Icon}
    }
    if {[Editor__BooleanAttribute $editor optionStrictMotif]} {
	$w.menu.opts invoke {Strict Motif}
    }

    # Key binding to turn on debugging mode
    bind $w <D> {set tkined(debug) [expr ! $tkined(debug)]}

    # A very special binding to kill an editor if the toplevel 
    # receives a destroy event.

    bind $w <Destroy> "yoyo %W $editor"

    proc yoyo {w editor} {
	if {$w == [$editor toplevel]} { 
	    after idle [list catch "$editor delete"]
	}
    }

    # now its time to map the whole thing on the screen
    update
    global geometry
    if {[info exists geometry]} {
        wm geometry $w $geometry
    }
    wm deiconify $w
    update

    # fire up all interpreters for this editor
    set i 1
    while {[set interp [$editor attribute interpreter$i]]!=""} {
        incr i
	if {[catch {INTERPRETER create $interp} interpreter]} {
	    if {$interp != "manager.tcl"} {
		Dialog__acknowledge $w.canvas \
		    "Sorry, $interp not found !!" "" \
		    "Check your tkined.defaults and your TKINED_PATH."
	    }
	} else {
	    $interpreter editor $editor
	    $interpreter canvas $w.canvas
	}
    }
}

##
## Lock the editor. This proc removed all menus and bindings that
## could change the current map.
##

proc Editor__lock { editor } {

    set w [$editor toplevel]
    $w.menu entryconfigure Edit -state disabled
    $w.menu.structure entryconfigure Group -state disabled
    $w.menu.structure entryconfigure Ungroup -state disabled
    $w.menu.structure entryconfigure {Join Group} -state disabled
    $w.menu.structure entryconfigure {Leave Group} -state disabled
    $w.menu entryconfigure Icon -state disabled
    
    # remove the move bindings on the middle mouse button
    bind $w.canvas <2> ""
    bind $w.canvas <B2-Motion> ""
    bind $w.canvas <ButtonRelease-2> ""
}

##
## Unlock the editor. This proc adds all menus and bindings that
## allow to edit the current map.
##

proc Editor__unlock { editor } {

    set w [$editor toplevel]
    $w.menu entryconfigure Edit -state normal
    $w.menu.structure entryconfigure Group -state normal
    $w.menu.structure entryconfigure Ungroup -state normal
    $w.menu.structure entryconfigure {Join Group} -state normal
    $w.menu.structure entryconfigure {Leave Group} -state normal
    $w.menu entryconfigure Icon -state normal
    
    Tool__Move $editor
}


##
## Change the title of the toplevel window to show the new filename.
##

proc Editor__filename { editor } { 
    wm title [$editor toplevel] "$editor: [$editor filename]"
}

##
## Set the canvas to the new width and size. Set the global
## variables tkined_orientation$w and tkined_pageSize$w to
## update the menu.
##

proc Editor__pagesize { editor width height } {

    set c [$editor toplevel].canvas
    $c configure -scrollregion "0 0 $width $height"

    set w [$editor toplevel]
    global tkined_orientation$w tkined_pageSize$w
    set tkined_orientation$w [$editor orientation]
    set tkined_pageSize$w    [$editor pagesize]
}

##
## Create a PostScript dump of the canvas. We need to do some magic here
## to get the background right. I think tk is a bit broken here...
##

proc Editor__postscript { editor } {

    global tkined_ps_map
    set w [$editor toplevel]
    set tkined_ps_map(fixed) [list Courier 10]

    set orientation [$editor orientation]
    set width       [$editor width]
    set height      [$editor height]
    set pagewidth   [$editor pagewidth]
    set pageheight  [$editor pageheight]

    foreach item [$w.canvas find all] {
	switch [$w.canvas type $item] {
	    bitmap -
	    stripchart -
	    barchart {
		$w.canvas itemconfigure $item -background White
	    }
	}
	$w.canvas itemconfigure clip$item -fill White -outline White
    }

    # make all selection marks invisible

    foreach id [$editor selection] {
	foreach item [$id items] {
	    $w.canvas itemconfigure mark$item -outline "" -fill ""
	}
    }

    update

    set border 10
    if {$orientation == "landscape"} {
	set anchor "nw"
    } else {
	set anchor "sw"
    }

    if {[catch {$w.canvas postscript -colormode color \
            -fontmap tkined_ps_map \
	    -x 0 -y 0 -height $height -width $width \
	    -pagewidth  "[expr $pagewidth - 2 * $border]m" \
	    -pageheight "[expr $pageheight - 2 * $border]m" \
	    -pagex "[set border]m" \
            -pagey "[set border]m" \
	    -pageanchor $anchor \
            -rotate [expr {$orientation == "landscape"}]} result]} {
	Dialog__acknowledge $w.canvas "Generating PostScript failed:" $result
    }

    set color [$w.canvas cget -background]
    foreach item [$w.canvas find all] {
	switch [$w.canvas type $item] {
	    bitmap -
	    stripchart -
	    barchart {
		$w.canvas itemconfigure $item -background $color
	    }
	}
	$w.canvas itemconfigure clip$item -fill $color -outline $color
    }

    # restore the selection marks

    foreach id [$editor selection] {
	foreach item [$id items] {
	    $w.canvas itemconfigure mark$item -outline black -fill black
	}
    }

    return $result
}

##
## Create a Tnm map dump of the Tkined map.
##

proc Editor__map { editor } {

    package require Tnm
    set map [Tnm::map create]

    foreach id [$editor retrieve] {
	switch [$id type] {
	    NODE {
		set node [$map create node]
		$node configure -name [$id name]
		$node configure -address [$id address]
		$node configure -color [$id color]
		$node configure -font [$id font]
		$node configure -icon [$id icon]
		foreach a [$id attribute] {
		    $node attribute $a [$id attribute $a]
		}
	    }
	}
    }

    set result [$map dump]
    $map destroy
    return $result
}

##
## Print a given file on a printer by running it through a print 
## command (like lpr) or save it to a file.
##

proc Editor__print {editor w srcFile} {
    global env

    set lpr [$editor attribute printcmd]
    if {$lpr == ""} {
	foreach dir [split $env(PATH) ":"] {
	    if {[file executable $dir/lpr]} {
		set lpr $dir/lpr
		break
	    }
	}
    }

    set res [Dialog__request $w "Saved to temporary file $srcFile." \
	     [list [list "Print Command:" "$lpr" entry 30] ] \
	     [list print "save to file" cancel]]

    if {[lindex $res 0] == "cancel"} return

    if {[lindex $res 0] == "print"} {
	set cmd [lindex $res 1]
	$editor attribute printcmd $cmd
	if {[catch {eval exec $cmd $srcFile} err]} {
	    Dialog__acknowledge $w "$lpr $srcFile failed:" "" $err
	}
    } else {
	set types {
	    {{PostScript}   {.ps}  }
	    {{All Files}    *       }
	}
	set dstFile [tk_getSaveFile -defaultextension .ps -filetypes $types \
		-parent $w -title "Save PostScript to file:"]
	if {$dstFile == ""} return
	
	if {[file exists $dstFile]} {
	    if {![file writable $dstFile]} {
		Dialog__acknowledge $w "Can not write $dstFile"
		return
	    }
	    set res [Dialog__confirm $w "Replace file $dstFile?" \
		     [list replace cancel]]
	    if {$res == "cancel"} return
	}
	
	if {[catch {file copy $srcFile $dstFile} err]} {
	    Dialog__acknowledge $w "Failed to write to $dstFile:" "" $err
	}
    }
}

##
## I do not know what to do with this button.
##

proc Editor__UselessButton { c } {
    if {[catch {exec /bin/sh -c "( fortune || yow )" 2> /dev/null} txt]} {
	set txt "You do not have fortune or yow on your system? Unbelievable!"
    }
    Dialog__acknowledge $c $txt
}

##
## Check whether a boolean editor attribute exists and return the
## boolean value as 1 or 0.
##

proc Editor__BooleanAttribute { editor name {default 0} } {
    set val [$editor attribute $name]
    if {$val == ""} {
	return $default
    }
    return [expr [lsearch "yes true on 1" $val] >= 0]
}

##
## Make a menu entry. The ultimate test for this proc is to create a
## menu entry like: ined create MENU asdf "aa:bb:cc bb:cc:dd cc:bb:cc"
##

proc tkined_makemenu {w path rlast rname} {

    static widgetpath
    static count

    upvar $rlast last
    upvar $rname name 

    if {![info exists count]} { set count 0 }
    incr count

    set path [split $path :]
    set len  [llength $path]
    set name [lindex $path [incr len -1]]
    set last $w

    for {set i 0} {$i < $len} {incr i} {
	set elem [join [lrange $path 0 $i] :]
	if {![info exists widgetpath($w$elem)]} {
	    set widgetpath($w$elem) $last.$count
	    menu $widgetpath($w$elem)
	    $last add cascade -label " [lindex $path $i]" \
		-menu $widgetpath($w$elem)
	}

	set last $widgetpath($w$elem)
    }
}
