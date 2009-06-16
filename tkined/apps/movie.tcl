#!/bin/sh
# the next line restarts using tclsh -*- tcl -*- \
exec tclsh "$0" "$@"
#
# movie.tcl -
#
#	This script can be used to create PostScript or GIF dumps of
#	a Tkined editor. We have successfully used it to create a 
#	series of GIF files that can be converted into a mpeg or fli
#	movie. This file also includes the clock commands from clock.tcl.
#
# Copyright (c) 1994-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#
# @(#) $Id: movie.tcl,v 1.1.1.1 2006/12/07 12:16:59 karl Exp $

package require Tnm 3.0

namespace import Tnm::*

ined size

proc icon_time_display { id off } {

    if {[ined retrieve $id] == ""} {
	[job current] destroy
    }

    set time [expr {[clock seconds]+$off}]
    set sec  [expr {$time%60}]
    set min  [expr {($time%3600)/60}]
    set hour [expr {($time%86400)/3600}]

    ined -noupdate attribute $id time [format "%2d:%02d:%02d" $hour $min $sec]
    ined label $id time
}

proc "Create Icon Clock" { list } {

    global clock_ids

    set off [ined request "Local Offset to GMT time:" \
	     [list [list Hours: 2]] [list "set offset" cancel] ]

    if {[lindex $off 0] == "cancel" } return

    set off [expr {[lindex $off 1]*60*60}]

    set id [ined -noupdate create NODE]
    ined -noupdate icon $id clock
    ined -noupdate move $id 400 200
    job create -command "icon_time_display $id $off" -interval 10000

    lappend clock_ids $id
} 

proc text_time_display { id } {

    if {[ined retrieve $id] == ""} {
	[job current] destroy
    }

    ined text $id [clock format [clock seconds]]
}

proc "Create Text Clock" { list } {

    global clock_ids

    set id [ined -noupdate create TEXT ""]
    ined -noupdate move $id 400 200
    job create -command "text_time_display $id" -interval 10000

    lappend clock_ids $id
} 

##
## Get a PostScript image from the tkined editor.
##

proc PostScriptToFile { filename {id {}} } {

    if {$id == ""} {
	regsub -all {\\n} [ined postscript] "\n" ps
    } else {
	regsub -all {\\n} [ined postscript $id] "\n" ps
    }

    if {[catch {open $filename w+} fh]} {
        writeln "** GEE: Could not open $filename: $fh"
	return
    }

    if {[catch {puts $fh $ps} err]} {
	writeln "** GEE: Could not write to $filename: $err"
        return
    }

    catch {close $fh}
}

##
## Get a PostScript image and convert it to GIF.
##

proc GIFToFile { filename {id {}} } {
    global tnm

    set psname  "$tnm(tmp)/tkined[pid].ps"

    PostScriptToFile $psname $id

    set cmd \
	"gs -dNOPAUSE -q -r64x64 -sDEVICE=pbmraw -sOutputFile=- $psname quit.ps < /dev/null | pnmrotate -90 | ppmtogif > $filename 2> /dev/null"

    eval exec $cmd

    catch {file delete -force $psname}
}

##
##
##

proc PostScript { list } {
    global tnm

    set psname  "$tnm(tmp)/tkined[pid].ps"

    PostScriptToFile $psname

    set filename [ined savefile "Where to put the PostScript file?"]
    if {$filename == ""} return

    if {[catch {file rename $psname $filename} err]} {
	ined acknowledge "Failed to move file to $filename:" "" $err
	return
    }

    catch {file delete -force $psname}
}

##
##
##

proc GIF { list } {

    global tnm

    set gifname "$tnm(tmp)/tkined[pid].gif"

    GIFToFile $gifname

    set filename [ined savefile "Where to put the GIF file?"]
    if {$filename == ""} return

    if {[catch {file rename $gifname $filename} err]} {
	ined acknowledge "Failed to move file to $filename:" "" $err
	catch {file delete -force $gifname}
	return
    }
}

##
## Make a snapshot and put it in a directory below path. 
##

proc SnapShot { path {pfx ss} {id {}} } {

    set date [clock format [clock seconds]]
    set dir "$path/[eval format "%s-%s-%02d-%s" [lreplace $date 3 3]]"

#    syslog debug "moviemaker: woke up ($dir)"

    set cnt -1
    catch {
	foreach file [glob $dir/$pfx.*] {
	    set ext [string trimleft [file extension $file] .]
	    if {$ext == "000"} {
		set num 0
	    } else {
		set num [string trimleft $ext "0"]
	    }
	    if {[catch {expr $num + 1}]} {
#		syslog debug "moviemaker: ignoring $file"
		continue
	    }
	    if {$num > $cnt} {
		set cnt $num
	    }
	}
    } err
    incr cnt

    set filename [format "%s/$pfx.%03d" $dir $cnt]

#    syslog debug "moviemaker: will use filename $filename"

    if {![file isdirectory $filename]} {
	if {[catch {file mkdir [file dirname $filename]}]} {
#	    syslog debug "moviemaker: can not create dir $filename"
	}
    }

    GIFToFile $filename $id

    file delete -force $path/current.gif
    exec ln -s $filename $path/current.gif
}

##
## Start a new job to collect gifs form the tkined editor.
##

proc "Start Movie" { list } {

    global JOB

    if {[info exists JOB]} {
	ined acknowledge "There is already a job running."
	return
    }

    set res [ined request "Set parameter for snapshots:" \
	       [list \
	         [list "Path:" "/usr/local/WWW/ibr/projects/nm/tkined/movie"] \
	         [list "Interval \[s\]:" "300"] ] \
	       [list start cancel] ]

    if {[lindex $res 0] == "cancel"} return

    set path     [lindex $res 1]
    set interval [lindex $res 2]

    foreach comp [ined retrieve] {
	if {[ined type $comp] == "STRIPCHART"} {
	    ined -noupdate jump [ined id $comp] 1
	}
    }

    if {$list == ""} {
	set JOB [job create -command "SnapShot $path" \
		 -interval [expr {$interval * 1000}]]
    } else {
	foreach comp $list {
	    set id [ined id $comp]
	    lappend JOB [job create -command "SnapShot $path $id $id" \
			 -interval [expr {$interval * 1000}]]
	}
    }
}

##
## Kill the job that creates the snapshot files.
##

proc "Stop Movie" { list } {

    global JOB

    if {![info exists JOB]} {
	ined acknowledge "No job running."
	return
    }

    foreach job $JOB {
	$job destroy
    }
    unset JOB
}

##
## Display the jobs currently running.
##

proc "Movie Job Info" {list} {

    set jobs [job find]

    if {$jobs == ""} {
	ined acknowledge "Sorry, no jobs available."
	return
    }
    
    set result ""
    set len 0
    foreach j $jobs {

	set jobid   $j
	set jobcmd  [$j cget -command]
	set jobitv  [expr [$j cget -interval] / 1000.0]
	set jobrem  [expr [$j cget -time] / 1000.0]
	set jobcnt  [$j cget -iterations]
	set jobstat [$j cget -status]

	set line \
	     [format "%s %6.1f %6.1f %3d %8s %s" \
	      $jobid $jobitv $jobrem $jobcnt $jobstat $jobcmd ]

	if {[string length $line] > $len} {
	    set len [string length $line]
	}

	lappend result $line
    }

    set header " ID    INTV    REM CNT  STATUS    CMD"

    for {set i [string length $header]} {$i < $len} {incr i} {
	append header " "
    }

    ined browse $header $result
}

##
## And here comes the usual stuff.
##

proc "Delete Movie" { list } {

    global menus clock_ids

    if {[info exists clock_ids]} {
	foreach id $clock_ids {
	    ined delete $id
	}
    }

    foreach menu $menus {
	ined delete $menu
    }
    exit
}

set menus [ined create MENU Movie \
           "Create Icon Clock" "Create Text Clock" "" \
	   PostScript GIF "" \
           "Start Movie" "Stop Movie" "" \
           "Movie Job Info" "" "Delete Movie"]

vwait forever
