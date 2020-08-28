##
## Objects.tcl
##
## This file contains the callbacks that are called whenever and object
## changes its internal state. All calls to procs defined in this file
## should be made by one of the methods defined in methods.c. This file
## implements the View according to the Model-View-Controller paradigm.
##
## Every object can stores its main canvas item in its items attribute.
## All additional items used to create a label etc. are accessed using 
## tags. We use the following tag names (assuming $item has the canvas 
## as stored in the item attribute of an object):
##
##    mark$item   - a tag for all items marking $item as selected
##    label$item  - a tag for the text item containing a label
##    clip$item   - a tag for a box used to clip
##
## More speedups are possible by removing some indirection, but this
## will make the code less readable. So if we need more speed, we 
## should create a real icon canvas object that makes all this tk 
## code superfluous
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

package provide TkinedObjects 1.6.0

##
## These procs are of general interest and are called by the
## type specific callbacks to save some code.
##

proc __delete { id } {
    set c [$id canvas]
    catch {destroy $c.scale}
    if {$c == ""} {
	set c .[$id editor].canvas
    }
    foreach item [$id items] {
	$c delete $item mark$item label$item clip$item
    }
    $id items ""
}

proc __size { id type } {
    return [[$id canvas] bbox [$id items]]
}

proc __move { id dx dy } {
    set c [$id canvas]
    set item [$id items]
    $c move $item $dx $dy
    $c move mark$item $dx $dy
    $c move label$item $dx $dy
    $c move clip$item $dx $dy
}

proc __select { id type } {
    set c [$id canvas]
    set item [$id items]
    $c addtag selected withtag $item
    if {[$c type $item] == "line" && $type != "GROUP"} {
	tkined_mark_points $c $item
    } else {
	tkined_mark_box $c $item
    }
}

proc __unselect { id } {
    set c [$id canvas]
    set item [$id items]
    $c dtag $item selected
    $c delete mark$item
}

proc __color { id type color } {
    set c [$id canvas]
    set item [$id items]
    switch [$c type $item] {
	bitmap { $c itemconfigure $item -foreground $color }
	line   { $c itemconfigure $item -fill $color }
	text   { $c itemconfigure $item -fill $color }
	stripchart { $c itemconfigure $item -outline $color }
	barchart   { $c itemconfigure $item -outline $color }
    }
}

proc __font { id type font} {
    set c [$id canvas]
    set item [$id items]
    if {[$c type $item] == "text"} {
	$c itemconfigure $item -font $font
    }
    catch {$c itemconfigure label$item -font $font}
}

proc __raise { id } {
    [$id canvas] raise "id $id"
}

proc __lower { id } {
    [$id canvas] lower "id $id"
}

proc __icon { id bitmap } {
    set c [$id canvas]
    set item [$id items]
    if {[$c type $item] == "bitmap"} {
	$c itemconfigure $item -bitmap $bitmap
    }
}

proc __clearlabel { id } {
    set item [$id items]
    [$id canvas] delete label$item clip$item
}

proc __label { id type text } {

    set c [$id canvas]
    set item [$id items]

    if {$item == ""} return

    # search for an existing label

    set label [$c find withtag label$item]
    set clip  [$c find withtag clip$item]

    if {$label != ""} {
	if {$text == ""} {
	    $c delete $label $clip
	} else {
	    $c itemconfigure $label -text $text
	    if {$clip != ""} {
		eval $c coords $clip [$c bbox $label]
	    }
	}
	return
    }

    # create a new label with and a box behind it

    set color [$c cget -background]

    if {$type == "NETWORK"} {
	set xy [$id labelxy]
	set x [lindex $xy 0]
	set y [lindex $xy 1]
    } else {
	set bb [$c bbox $item]
	set x1 [lindex $bb 0]
	set x2 [lindex $bb 2]
	set y [expr {[lindex $bb 3]+1}]
	set x [expr {$x1+(($x2-$x1)/2)}]
    }

    set label [$c create text $x $y -anchor n -text $text -font fixed \
	       -justify center -tags [list label$item "id $id"] ]
    set tags [list clip$item "id $id"]
    set clip [eval $c create rectangle [$c bbox $label] \
	      -tags {$tags} -fill $color -outline $color -width 0 ]
    $c lower $clip $label
    $id font [$id font]
}

proc __postscript { id } {

    global tkined_ps_map
    set tkined_ps_map(fixed) [list Courier 10]

    set c [$id canvas]
    set item [$id items]

    set bb [$c bbox $items $label$item $clip$item]

    set width  [expr {[lindex $bb 2] -[lindex $bb 0]}]
    set height [expr {[lindex $bb 3] -[lindex $bb 1]}]
    set x [lindex $bb 0]
    set y [lindex $bb 1]

    $c postscript -fontmap tkined_ps_map \
	    -height $height -width $width -x $x -y $y
}

##
## The following set of procedures handle node objects.
##

proc NODE__canvas { node } {
    $node items [[$node canvas] create bitmap \
		  [lindex [$node move] 0] [lindex [$node move] 1] \
		      -background [[$node canvas] cget -background] \
		      -bitmap node -tags [list NODE "id $node"] ]
}

proc NODE__delete { node } {
    __delete $node
}

proc NODE__size { node } {
    __size $node NODE
}

proc NODE__move { node dx dy } {
    __move $node $dx $dy
}

proc NODE__select { node } {
    __select $node NODE
}

proc NODE__unselect { node } {
    __unselect $node
}

proc NODE__color { node color } {
    __color $node NODE $color
}

proc NODE__font { node font } {
    __font $node NODE $font
}

proc NODE__raise { node } {
    __raise $node
}

proc NODE__lower { node } {
    __lower $node
}

proc NODE__icon { node bitmap } {
    __icon $node $bitmap
}

proc NODE__clearlabel { node } {
    __clearlabel $node
}

proc NODE__label { node text } {
    __label $node NODE $text
}


##
## The following set of procedures handle group objects.
##

proc GROUP__canvas { group } {
    $group collapse
}

proc GROUP__delete { group } {
    __delete $group
}

proc GROUP__size { group } {
    __size $group GROUP
}

proc GROUP__move { group dx dy } {
    __move $group $dx $dy
}

proc GROUP__select { group } {
    __select $group GROUP
}

proc GROUP__unselect { group } {
    __unselect $group
}

proc GROUP__color { group color } {
    __color $group GROUP $color
}

proc GROUP__font { group font } {
    __font $group GROUP $font
}

proc GROUP__raise { group } {
    __raise $group
}

proc GROUP__lower { group } {
    __lower $group
}

proc GROUP__icon { group bitmap } {
    __icon $group $bitmap
}

proc GROUP__clearlabel { group } {
    __clearlabel $group
}

proc GROUP__label { group text} {
    __label $group GROUP $text
}

proc GROUP__collapse { group } {

    __delete $group
    foreach id [$group member] {
	__delete $id
    }

    $group items [[$group canvas] create bitmap \
		    [lindex [$group move] 0] [lindex [$group move] 1] \
		      -background [[$group canvas] cget -background] \
		      -bitmap group -tags [list GROUP "id $group"] ]
}

proc GROUP__expand { group } {

    set c [$group canvas]
    __delete $group

    set bb [$group move]
    set x1 [expr {[lindex $bb 0]-30}]
    set y1 [expr {[lindex $bb 1]-30}]
    set x2 [expr {[lindex $bb 0]+30}]
    set y2 [expr {[lindex $bb 1]+30}]
    
    set tags [list GROUP "id $group"]
    $group items [eval $c create line $x1 $y1 $x1 $y2 $x2 $y2 $x2 $y1 $x1 $y1 \
		    -width 2 -fill Black -stipple gray50 -joinstyle miter \
			-tags {$tags}]

    if {[$group member] != ""} {
	GROUP__resize $group
    }
}

proc GROUP__resize { group } {

    set c [$group canvas]

    set memberitems ""
    foreach id [$group member] {
	set item [$id items]
        lappend memberitems $item label$item
    }
    if {$memberitems == ""} return

    set bb [eval $c bbox [join $memberitems]]
    set x1 [expr {[lindex $bb 0]-3}]
    set y1 [expr {[lindex $bb 1]-3}]
    set x2 [expr {[lindex $bb 2]+3}]
    set y2 [expr {[lindex $bb 3]+3}]

    $c coords "id $group" $x1 $y1 $x1 $y2 $x2 $y2 $x2 $y1 $x1 $y1
}


##
## The following set of procedures handle network objects.
##

proc NETWORK__canvas { network } {
    set c [$network canvas]
    set points [join [$network points]]
    set tags [list NETWORK "id $network"]
    set item [eval $c create line $points -width 3 -fill black -tags {$tags} ]
    eval $c move $item [$network move]
    $network items $item
}

proc NETWORK__delete { network } {
    __delete $network
}

proc NETWORK__size { network } {
    __size $network NETWORK
}

proc NETWORK__move { network dx dy } {
    __move $network $dx $dy
}

proc NETWORK__select { network } {
    __select $network NETWORK
}

proc NETWORK__unselect { network } {
    __unselect $network
}

proc NETWORK__color { network color } {
    __color $network NETWORK $color
}

proc NETWORK__icon { network width } {
    [$network canvas] itemconfigure [$network items] -width $width
}

proc NETWORK__clearlabel { network } {
    __clearlabel $network
}

proc NETWORK__label { network text } {
    __label $network NETWORK $text
}

proc NETWORK__font { network font } {
    __font $network NETWORK $font
}

proc NETWORK__raise { network } {
    __raise $network
}

proc NETWORK__lower { network } {
    __lower $network
}


##
## The following set of procedures handle link objects.
##

proc LINK__canvas {link} {
    set c [$link canvas]
    set points [join [$link points]]
    set len [llength $points]
    if {$len%2 != 0} { 
	incr len -2
	set points [join [lrange $points 1 len]]
	inr len
    }
    set xya [[$link src] move]
    set xyb [[$link dst] move]
    set tags [list LINK "id $link"]
    $link items [eval $c create line $xya $points $xyb \
		 -fill black -tags {$tags}]
}

proc LINK__delete { link } {
    __delete $link
}

proc LINK__size { link } {
    __size $link LINK
}

proc LINK__move { link dx dy } {
    __move $link $dx $dy
}

proc LINK__select { link } {
    __select $link LINK
}

proc LINK__unselect { link } {
    __unselect $link
}

proc LINK__color { link color } {
    __color $link LINK $color
}

proc LINK__raise { link } {
    [$link canvas] raise [$link items]
}

proc LINK__lower { link } {
    [$link canvas] lower [$link items]
}


##
## The following set of procedures handle text objects.
##

proc TEXT__canvas { text } {
    set c [$text canvas]
    set w [winfo parent $c]
    set x [lindex [$text move] 0]
    set y [lindex [$text move] 1]
    set txt [$text text]
    regsub -all "\\\\n" $txt "\n" txt
    $text items [$c create text $x $y -anchor nw -text $txt -font fixed \
		 -tags [list TEXT "id $text"]  ]
}

proc TEXT__delete { text } {
    __delete $text
}

proc TEXT__size { text } {
    __size $text TEXT
}

proc TEXT__move { text dx dy } {
    __move $text $dx $dy
}

proc TEXT__select { text } {
    __select $text TEXT
}

proc TEXT__unselect { text } {
    __unselect $text
}

proc TEXT__color { text color } {
    __color $text TEXT $color
}

proc TEXT__font { text font } {
    __font $text TEXT $font
}

proc TEXT__text { text } {
    set c [$text canvas]
    regsub -all "\\\\n" [$text text] "\n" txt
    set item [$text items]
    $c itemconfigure $item -text $txt
}

proc TEXT__raise { text } {
    [$text canvas] raise [$text items]
}

proc TEXT__lower { text } {
    [$text canvas] lower [$text items]
}


##
## The following set of procedures handle image objects.
##

proc IMAGE__canvas { image } {

    global tkined

    set c [$image canvas]
    set fname [$image name]

    # expand URL ftp syntax

    if {[string match "ftp://*" $fname] || [string match "file://*" $fname]} {
        set idx [string first "://" $fname]
        incr idx 3
        set fname [string range $fname $idx end]
        set idx [string first "/" $fname]
        set server [string range $fname 0 [expr {$idx-1}]]
        set file [string range $fname $idx end]
        set fname "$tkined(tmp)/$server:[file tail $file]"

        if {[catch {tkined_ftp $server $file $fname} err]} {
            Dialog__acknowledge $w.canvas "Can not retrieve file $fname." $err
            return ""
        }
    }

    set x [lindex [$image move] 0]
    set y [lindex [$image move] 1]
    set tags [list IMAGE "id $image"]
if {0} {
    if {[catch {$c create bitmap $x $y -bitmap @$fname -tags $tags} item]} {
	Dialog__acknowledge $c "Image file not readable!"
	$image canvas ""
	return
    }
} else {
    if {[catch {image create photo -file $fname} img]} {
	if {[catch {image create bitmap -file $fname -maskfile $fname} img]} {
	    Dialog__acknowledge $c "Image file not readable!" "" $img
	    $image canvas ""
	    return
	}
    }
    set item [$c create image $x $y -image $img -tags $tags]
}
    $image items $item
    $image lower
}

proc IMAGE__delete { image } {
    __delete $image
}

proc IMAGE__size { image } {
    __size $image IMAGE
}

proc IMAGE__move { image dx dy } {
    __move $image $dx $dy
}

proc IMAGE__select { image } {
    __select $image IMAGE
}

proc IMAGE__unselect { image } {
    __unselect $image
}

proc IMAGE__color { image color } {
    __color $image IMAGE $color
}

proc IMAGE__lower { image } {
    [$image canvas] lower [$image items]
}


##
## The following set of procedures handle MENU objects.
##

proc MENU__canvas { menu } {

    set interpreter [$menu interpreter]
    set c [$menu canvas]
    set w [winfo parent $c]
    set name $w.tools.[$menu id]

    set name $w.menu.[$menu id]
    $w.menu add cascade -label [$menu name] -menu $name
    menu $name -tearoffcommand MENU__tearoff
    foreach cmd [$menu items] {
	if {$cmd==""} {
	    $name add separator
	} else {
	    tkined_makemenu $name $cmd newname newcmd
	    $newname add command -label $newcmd \
		-command [list MENU__send $menu $newcmd]
	}
    }

    $interpreter items "$menu [$interpreter items]"
}

proc MENU__tearoff { dady child } {
    global tornoff
    lappend tornoff($dady) $child
}

proc MENU__send { menu cmd } {

    set editor [$menu editor]
    set interpreter [$menu interpreter]
    set l ""
    foreach id [$editor selection] {
	lappend l [$id retrieve]
    }
    $interpreter send $cmd $l
}

proc MENU__delete { menu } {
    global tornoff

    set interpreter [$menu interpreter]
    set c [$menu canvas]
    set w [winfo parent $c]

    set dady $w.menu.[$menu id]
    if [info exists tornoff($dady)] {
	foreach child $tornoff($dady) {
	    destroy $child
	}
    }

    destroy $w.menu.[$menu id]

    set last ""
    for {set i 0} {1} {incr i} {
	set next [$w.menu entrycget $i -menu]
	if {[string compare $next $dady] == 0} {
	    $w.menu delete $i
	    break
	}
	if {[string compare $next $last] == 0} break
	set last $next
    }

    # and remove the menu from the interpreter
    set new ""
    foreach t [$interpreter items] {
	if {$t != $menu} { 
	    lappend new $t
	}
    }
    $interpreter items $new
}



##
## The following set of procedures handle interpreter objects.
## The delete method is a dummy that is needed in cases where
## the creation of a new interpreter fails (e.g. we can't open
## another pipe).
##

proc INTERPRETER__queue { interpreter qlen } {
    set w [[$interpreter editor] toplevel]
    if {$qlen > 0} { set state disabled } { set state normal }

    foreach menu [$interpreter items] {
	set name $w.menu.$menu
	set last [$name index last]
	if {$last == "none"} continue
	for {set idx 0} {$idx <= $last} {incr idx} {
	    catch {$name entryconfigure $idx -state $state} msg;
	}
    }
}

proc INTERPRETER__delete { interpreter } {
}


##
## The following set of procedures handle log objects.
##

proc LOG__canvas { log } {

    global tcl_platform

    # The offset used to position log windows automatically.
    static offset
    if {![info exists offset]} {
	set offset 80
    } else {
	incr offset 10
	if {$offset > 180} {set offset 80}
    }

    set c [$log canvas]
    set w [winfo parent $c]
    toplevel $w.$log

    # set up the menu bar
    frame $w.$log.menu -borderwidth 1 -relief raised

    # set up the file menu
    menubutton $w.$log.menu.file -text "File" -menu $w.$log.menu.file.m
    menu $w.$log.menu.file.m
    $w.$log.menu.file.m add command -label "Clear" \
	-accelerator "  Alt+C" \
	-command "$log clear"
    bind $w.$log <Alt-c>  "$w.$log.menu.file.m invoke Clear"
    bind $w.$log <Alt-C>  "$w.$log.menu.file.m invoke Clear"
    bind $w.$log <Meta-c> "$w.$log.menu.file.m invoke Clear"
    bind $w.$log <Meta-C> "$w.$log.menu.file.m invoke Clear"
    $w.$log.menu.file.m add command -label "Open..." \
	-accelerator "  Alt+O" \
	-command "LOG__load $log"
    bind $w.$log <Alt-o>  "$w.$log.menu.file.m invoke Open..."
    bind $w.$log <Alt-O>  "$w.$log.menu.file.m invoke Open..."
    bind $w.$log <Meta-o> "$w.$log.menu.file.m invoke Open..."
    bind $w.$log <Meta-O> "$w.$log.menu.file.m invoke Open..."
    $w.$log.menu.file.m add command -label "Save As..." \
	-accelerator "  Alt+S" \
	-command "LOG__save $log"
    bind $w.$log <Alt-s>  "$w.$log.menu.file.m invoke {Save As...}"
    bind $w.$log <Alt-S>  "$w.$log.menu.file.m invoke {Save As...}"
    bind $w.$log <Meta-s> "$w.$log.menu.file.m invoke {Save As...}"
    bind $w.$log <Meta-S> "$w.$log.menu.file.m invoke {Save As...}"
    $w.$log.menu.file.m add sep
    $w.$log.menu.file.m add command -label "Print..." \
	-accelerator "  Alt+P" \
	-command "LOG__print $log"
    bind $w.$log <Alt-p>  "$w.$log.menu.file.m invoke Print..."
    bind $w.$log <Alt-P>  "$w.$log.menu.file.m invoke Print..."
    bind $w.$log <Meta-p> "$w.$log.menu.file.m invoke Print..."
    bind $w.$log <Meta-P> "$w.$log.menu.file.m invoke Print..."
    $w.$log.menu.file.m add command -label "Email..." \
	-accelerator "  Alt+E" \
	-command "LOG__email $log"
    bind $w.$log <Alt-e>  "$w.$log.menu.file.m invoke Email..."
    bind $w.$log <Alt-E>  "$w.$log.menu.file.m invoke Email..."
    bind $w.$log <Meta-e> "$w.$log.menu.file.m invoke Email..."
    bind $w.$log <Meta-E> "$w.$log.menu.file.m invoke Email..."
    $w.$log.menu.file.m add sep
    $w.$log.menu.file.m add command -label "New View" \
	-accelerator "  Alt+N" \
	-command "set l \[LOG create\]; \$l canvas $c; \$l name \[$log name\]"
    bind $w.$log <Alt-n>  "$w.$log.menu.file.m invoke {New View}"
    bind $w.$log <Alt-N>  "$w.$log.menu.file.m invoke {New View}"
    bind $w.$log <Meta-n> "$w.$log.menu.file.m invoke {New View}"
    bind $w.$log <Meta-N> "$w.$log.menu.file.m invoke {New View}"
    $w.$log.menu.file.m add command -label "Close View" \
	-accelerator "  Alt+W" \
	-command "$log delete"
    bind $w.$log <Alt-w>  "$w.$log.menu.file.m invoke {Close View}"
    bind $w.$log <Alt-W>  "$w.$log.menu.file.m invoke {Close View}"
    bind $w.$log <Meta-w> "$w.$log.menu.file.m invoke {Close View}"
    bind $w.$log <Meta-W> "$w.$log.menu.file.m invoke {Close View}"

    # set up the option menu
    menubutton $w.$log.menu.opts -text "Options" -menu $w.$log.menu.opts.m
    menu $w.$log.menu.opts.m
    $w.$log.menu.opts.m add checkbutton -label "Freeze" \
	-offvalue 0 -onvalue 1 -variable freeze$log \
	-accelerator "  ^Z"
    bind $w.$log <Control-z> "$w.$log.menu.opts.m invoke Freeze"
    $w.$log.menu.opts.m add checkbutton -label "Wrap" \
	-offvalue none -onvalue word -variable wrap$log \
	-accelerator "  ^W" -command "LOG__wrap $log"
    bind $w.$log <Control-w> "$w.$log.menu.opts.m invoke Wrap"

    pack $w.$log.menu -side top -fill x
    pack $w.$log.menu.file -side left
    pack $w.$log.menu.opts -side left

    scrollbar $w.$log.scrollbar -command "$w.$log.text yview" -relief sunken
    ##
    ## give a non unix platform an additional horizontal scrollbar to
    ## allow scrolling without a three button mouse:
    ##
    if {[string compare $tcl_platform(platform) unix] == 1} {
	## textarea with two scrollbars for non unix platforms:
	scrollbar $w.$log.scrollbar2 -orient horiz \
		-command "$w.$log.text xview" -relief sunken
	text $w.$log.text -height 24 -width 80 -setgrid true -wrap none \
		-relief sunken -borderwidth 2 \
		-yscrollcommand "$w.$log.scrollbar set" \
		-xscrollcommand "$w.$log.scrollbar2 set"
	pack $w.$log.scrollbar -side right -fill y
	pack $w.$log.scrollbar2 -side bottom -fill x
    } else {
	## textarea with one scrollbar for unix:
	text $w.$log.text -height 24 -width 80 -setgrid true -wrap none \
		-relief sunken -borderwidth 2 \
		-yscrollcommand "$w.$log.scrollbar set"
	pack $w.$log.scrollbar -side right -fill y
    }
    pack $w.$log.text -side left -padx 2 -pady 2 -fill both -expand yes

    $log items $w.$log

    # This special purpose binding makes it possible to send
    # complete lines back to the interpreter that created this
    # window. This allows us to use a log window as a simple
    # command frontend.

    bind $w.$log.text <Shift-Return> "LOG__process $log"

    # Make sure we destroy the log object if the window goes away
    # (e.g. the user deletes the toplevel using the window manager.

    bind $w.$log <Destroy> "$log delete"

    # Position the log window on the screen.

    wm withdraw $w.$log
    update idletasks
    set top [winfo toplevel $w]

    set rx [expr {[winfo rootx $top]}]
    set ry [expr {[winfo rooty $top]}]

    set cx [expr $rx+[winfo width $top]/4]
    set cy [expr $ry+[winfo height $top]/4]

    set x  [expr $cx+$offset]
    set y  [expr $cy+$offset]

    if {$x < 0} { set x 0 }
    if {$y < 0} { set y 0 }

    wm geometry $w.$log +$x+$y
    wm deiconify $w.$log

    update idletasks
}

proc LOG__process { log } {

    set w [$log items].text
    $w delete insert
#    set line [$w index "insert - 1 line"]
    set line insert
    set txt [$w get [$w index "$line linestart"] [$w index "$line lineend"]]
    if {$txt == ""} return

    [$log interpreter] send \
	eval ined append $log \"\[ catch \{ $txt \} err \; set err \]\\n\"

    $w insert insert "\n"
}

proc LOG__name { log } {
    wm title [$log items] [$log name]
    wm iconname [$log items] [$log name]
}

proc LOG__icon { log bitmap } {
    wm iconbitmap [$log items] $bitmap
}

proc LOG__append { log text } {
    global freeze$log
    set w [$log items]
    $w.text insert end $text
    if {! [set freeze$log]} {
	$w.text yview -pickplace end
    }
}

proc LOG__bind { log cmd text } {

    set w [$log items].text

if {1} {
    set bold "-foreground red"
    set normal "-foreground {}"

    set start [$w index insert]
    $w insert $start "<$text>"
    set end [$w index insert]
    set tag "tag$start$end"
    $w tag add $tag $start $end
    $w yview -pickplace end

#    $w tag configure $tag -borderwidth 1 -relief groove
    $w tag configure $tag -underline 1
    $w tag bind $tag <Any-Enter> "$w tag configure $tag $bold"
    $w tag bind $tag <Any-Leave> "$w tag configure $tag $normal"
    $w tag bind $tag <Button-3> "[$log interpreter] send $cmd"
    $w tag bind $tag <Button-1> "[$log interpreter] send $cmd"
} else {

    static idx
    if {![info exists idx]} {set idx 0}
    incr idx
    button $w.$idx -text "<$text>" -font [$w cget -font] \
	-command "[$log interpreter] send $cmd" \
	-padx 0 -pady 0 -relief groove
    $w window create end -window $w.$idx
}
}

proc LOG__unbind { log } {
    set w [$log items].text
    foreach tag [$w tag names] {
	catch {$w tag delete $tag}
    }
}

proc LOG__clear { log } {
    [$log items].text delete 0.0 end
}

proc LOG__delete { log } {
    destroy [$log items]
}

proc LOG__freeze { log } {
    if {[[$log items].menu.freeze cget -text] == "freeze"} {
	[$log items].menu.freeze configure -text melt
    } else {
	[$log items].menu.freeze configure -text freeze
    }
}

proc LOG__wrap { log } {
    global wrap$log
    [$log items].text configure -wrap [set wrap$log]
}

proc LOG__save { log } {

    set types {
	{{Text Files}	{.txt}	}
	{{All Files}	*	}
    }

    set fname [tk_getSaveFile -defaultextension .txt -filetypes $types \
	    -parent [$log items] -title "Write to file:"]
    if {$fname==""} return

    if {[catch {open $fname "w"} file]} {
	Dialog__acknowledge [$log items] "Unable to open $fname."
	return
    }

    puts $file [[$log items].text get 1.0 end]
    close $file

    $log attribute filename $fname
}

proc LOG__load { log } {

    set types {
	{{Text Files}	{.txt}	}
	{{All Files}	*	}
    }

    set fname [tk_getOpenFile -defaultextension .txt -filetypes $types \
	    -parent [$log items] -title "Read from file:"]
    if {$fname == ""} return

    if {[catch {open $fname r} file]} {
	Dialog__acknowledge [$log items] "Unable to read from $fname"
	return
    }

    $log attribute filename $fname

    set txt ""
    while {![eof $file]} {
	append txt "[gets $file]\n"
    }
    $log clear
    $log append $txt

    [$log items].text yview -pickplace end
    [$log items].text yview 1.0
    close $file
}

proc LOG__print { log } {

    global tkined

    set fname "$tkined(tmp)/tkined[pid].log"
    catch {file delete -force $fname}

    if {[file exists $fname] && ![file writable $fname]} {
	Dialog__acknowledge [$log items] "Can not write temporary file $fname."
	return
    }

    if {[catch {open $fname w} file]} {
	Dialog__acknowledge [$log items] "Can not open $fname: $file"
	return
    }

    if {[catch {puts $file [[$log items].text get 1.0 end]} err]} {
	Dialog__acknowledge [$log items] "Failed to write $fname: $err"
    }

    catch {close $file}

    Editor__print [$log editor] [$log items] $fname

    catch {file delete -force $fname}
}

proc LOG__email { log } {
    global env

    set result [Dialog__request [$log items] \
		"Please enter the email address:" \
		[list [list To: [$log address]] \
		      [list Subject: [$log name]] ] \
		[list send cancel] ]
    if {[lindex $result 0] == "cancel"} return

    set to [lindex $result 1]
    $log address $to
    set subject [lindex $result 2]

    if {[catch {split $env(PATH) :} path]} {
	set path "/usr/bin /bin /usr/ucb /usr/local/bin"
    }

    set mprog ""
    foreach mailer "Mail mail" {
	foreach dir $path {
	    set fname $dir/$mailer
	    if {[file executable $fname] && [file isfile $fname]} {
		set mprog $fname
		break
	    }
	}
	if {$mprog != ""} break
    }

    if {$mprog == ""} {
	Dialog__acknowledge [$log items] "Sorry, can not find mail program."
	return
    }

    if {[catch {open "|$mprog -s \"$subject\" $to" w} file]} {
        Dialog__acknowledge [$log items] "Unable to write to $mprog $to"
        return
    }

    puts $file [[$log items].text get 1.0 end]
    close $file
}

##
## Implementation of references to other tkined maps.
##

proc REFERENCE__canvas { ref } {
    set c [$ref canvas]
    $ref items [$c create bitmap \
		[lindex [$ref move] 0] [lindex [$ref move] 1] \
		      -background [[$ref canvas] cget -background] \
		      -bitmap reference -tags [list REFERENCE "id $ref"] ]
}

proc REFERENCE__open { reference {editor ""} } {
    if {$editor == ""} {
	set editor [$reference editor]
    }
    foreach w [winfo children .] { catch {$w configure -cursor watch} }
    update idletasks
    set result [Command__Open $editor [$reference address]]
    foreach w [winfo children .] { catch {$w configure -cursor top_left_arrow } }
    return $result
}

proc REFERENCE__load { reference } {
    set editor [EDITOR]
    if {[REFERENCE__open $reference $editor] == ""} {
	$editor delete
    }
}

proc REFERENCE__delete { reference } {
    __delete $reference
}

proc REFERENCE__size { reference } {
    __size $reference REFERENCE
}

proc REFERENCE__move { reference dx dy } {
    __move $reference $dx $dy
}

proc REFERENCE__select { reference } {
    __select $reference REFERENCE
}

proc REFERENCE__unselect { reference } {
    __unselect $reference
}

proc REFERENCE__color { reference color } {
    __color $reference REFERENCE $color
}

proc REFERENCE__font { reference font } {
    __font $reference REFERENCE $font
}

proc REFERENCE__raise { reference } {
    __raise $reference
}

proc REFERENCE__lower { reference } {
    __lower $reference
}

proc REFERENCE__icon { reference bitmap } {
    __icon $reference $bitmap
}

proc REFERENCE__clearlabel { reference } {
    __clearlabel $reference
}

proc REFERENCE__label { reference text } {
    __label $reference REFERENCE $text
}


##
##
##

proc STRIPCHART__canvas { stripchart } {
    set c [$stripchart canvas]
    set item [$c create stripchart -31 -21 31 21 \
	    -background [$c cget -background] \
	    -tags [list STRIPCHART "id $stripchart"]]
    eval $c move $item [$stripchart move]
    $stripchart items $item
}

proc STRIPCHART__delete { stripchart } {
    __delete $stripchart
}

proc STRIPCHART__size { stripchart } {
    __size $stripchart STRIPCHART
}

proc STRIPCHART__move { stripchart dx dy } {
    __move $stripchart $dx $dy
}

proc STRIPCHART__select { stripchart } {
#    set c [$stripchart canvas]
#    set item [$stripchart items]
#    $c itemconfigure $item -selected 1
#    $c addtag selected withtag $item
    __select $stripchart STRIPCHART
}

proc STRIPCHART__unselect { stripchart } {
#    set c [$stripchart canvas]
#    set item [$stripchart items]
#    $c itemconfigure $item -selected 0
#    $c dtag $item selected
    __unselect $stripchart
}

proc STRIPCHART__color { stripchart color } {
    __color $stripchart STRIPCHART $color
}

proc STRIPCHART__font { stripchart font } {
    __font $stripchart STRIPCHART $font
}

proc STRIPCHART__raise { stripchart } {
    __raise $stripchart
}

proc STRIPCHART__lower { stripchart } {
    __lower $stripchart
}

proc STRIPCHART__clearlabel { stripchart } {
    __clearlabel $stripchart
}

proc STRIPCHART__label { stripchart text } {
    __label $stripchart STRIPCHART $text
}

proc STRIPCHART__resize { stripchart x1 y1 x2 y2 } {
    set c [$stripchart canvas]
    set item [$stripchart items]
    $c coords $item $x1 $y1 $x2 $y2
}

proc STRIPCHART__clear { stripchart } {
    set c [$stripchart canvas]
    set item [$stripchart items]
    set bb [$c bbox $item] 
    set lower [lindex $bb 0]
    set upper [lindex $bb 2]
    set values 0
    for {set x $lower} {$x < $upper} {incr x} {
	lappend values 0
    }
    $c itemconfigure $item -values $values
}

proc STRIPCHART__values { stripchart args } {
    set c [$stripchart canvas]
    set item [$stripchart items]
    if {$args == ""} {
	return [$c itemcget $item -values]
    } else {
	$c itemconfigure $item -values [join $args]
    }
}

proc STRIPCHART__scale { stripchart num } {
    set c [$stripchart canvas]
    set item [$stripchart items]
    $c itemconfigure $item -scalevalue $num
}

proc STRIPCHART__jump { stripchart num } {
    set c [$stripchart canvas]
    set item [$stripchart items]
    $c itemconfigure $item -jump $num
}


##
##
##

proc BARCHART__canvas { barchart } {
    set c [$barchart canvas]
    set item [$c create barchart -31 -21 31 21 \
	    -autocolor [expr [winfo cells .] > 2] \
	    -background [$c cget -background] \
	    -tags [list BARCHART "id $barchart"]]
    eval $c move $item [$barchart move]
    $barchart items $item
}

proc BARCHART__delete { barchart } {
    __delete $barchart
}

proc BARCHART__size { barchart } {
    __size $barchart BARCHART
}

proc BARCHART__move { barchart dx dy } {
    __move $barchart $dx $dy
}

proc BARCHART__select { barchart } {
    __select $barchart BARCHART
}

proc BARCHART__unselect { barchart } {
    __unselect $barchart
}

proc BARCHART__color { barchart color } {
    __color $barchart BARCHART $color
}

proc BARCHART__font { barchart font } {
    __font $barchart BARCHART $font
}

proc BARCHART__raise { barchart } {
    __raise $barchart
}

proc BARCHART__lower { barchart } {
    __lower $barchart
}

proc BARCHART__clearlabel { barchart } {
    __clearlabel $barchart
}

proc BARCHART__label { barchart text } {
    __label $barchart BARCHART $text
}

proc BARCHART__resize { barchart x1 y1 x2 y2 } {
    set c [$barchart canvas]
    set item [$barchart items]
    $c coords $item $x1 $y1 $x2 $y2
}

proc BARCHART__clear { barchart } {
    $barchart values 0
}

proc BARCHART__values { barchart args } {
    set c [$barchart canvas]
    set item [$barchart items]
    if {$args == ""} {
	return [$c itemcget $item -values]
    } else {
	$c itemconfigure $item -values [join $args]
    }
}

proc BARCHART__scale { barchart num } {
    set c [$barchart canvas]
    set item [$barchart items]
    $c itemconfigure $item -scalevalue $num
}

## -------------------------------------------------------------------------

proc GRAPH__canvas { graph } {
    set c [$graph canvas]
    $c element create $graph
}

proc GRAPH__delete { graph } {
    set c [$graph canvas]
    $c element delete $graph
}

proc GRAPH__select { graph } {
    set c [$graph canvas]
    $c element configure $graph -symbol circle
    $c element configure $graph -scale 0.5
    $c element configure $graph -linewidth 2
}

proc GRAPH__unselect { graph } {
    set c [$graph canvas]
    $c element configure $graph -symbol line
    $c element configure $graph -linewidth 0
}

proc GRAPH__color { graph color } {
    set c [$graph canvas]
    $c element configure $graph -foreground $color
}

proc GRAPH__icon { graph dashes } {
    [$graph canvas] element configure $graph -dashes $dashes
}

proc GRAPH__clearlabel { graph } {
## XXX got memory errors
##    [$graph canvas] element configure $graph -label ""
}

proc GRAPH__label { graph text } {
## XXX got memory errors
##    [$graph canvas] element configure $graph -label $text
}

proc GRAPH__clear { graph } {
    set c [$graph canvas]
    $c element delete $graph
    $c element create $graph
}

proc GRAPH__postscript { graph } {

    global tkined_ps_map
    set tkined_ps_map(fixed) [list Courier 10]

    set c [$graph canvas]

    $c postscript -fontmap tkined_ps_map	    
}

##
##
##

proc DATA__canvas { data } {
    set s [STRIPCHART create]
    $s canvas [$data canvas]
    $data items $s
}

proc DATA__values { data args } {
    set values [join $args]
    foreach proc [info commands STREAM:*] {
	eval [list $proc $data $values]
    }
}
