# library.tcl --
#
#	This file contains library procedures that are used in many
#	places by the Tkined scripts. They are auto loaded using
#	Tcl's autoload facility.
#
# Copyright (c) 1993-1996 Technical University of Braunschweig.
# Copyright (c) 1996-1997 University of Twente.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

##
## =========================================================================
## =================== T K I N E D   related subroutines ===================
## =========================================================================
##
##  LoadDefaults { class }
##  ShowDefaults
##
##  write   { {txt ""} {cmd {}}}
##  writeln { {txt ""} {cmd {}}}
##  try { body {varName {}} {handler {}}}
##  debug   { txt }
##
## =========================================================================
##

##
## Search for tkined.defaults files following the auto_path and read
## default definition. Each default definition has the following syntax:
##
##    <class>.<attribute>.<index>: <value>
##
## Every entry matching the class argument will be written to the global
## default array, indexed by <attribute>.<index>. A sample application
## is the definition of snmp related parameters like:
##
##    snmp.port.134.169.34.15: 8132
##    snmp.community.134.169.34.15: private
##
## The resulting tcl variables are:
##
##    default(port.134.169.34.15) -> 8132
##    default(community.134.169.34.15) -> private
##

proc LoadDefaults { args } {

    global auto_path 
    global default

    set filename tkined.defaults
    
    set reverse_path ""
    foreach dir $auto_path {
	set reverse_path "$dir $reverse_path"
    }
    
    foreach dir $reverse_path {
	if [file readable $dir/$filename] {
	    set fh [open $dir/$filename r]
	    while {![eof $fh]} {
		gets $fh line
		set line [string trim $line]
		if {($line == "") || ([regexp "^#|^!" $line])} continue
		foreach class $args {
		    if {[string match $class.* $line]} {
			set line [split $line ":"]
			set list [split [lindex $line 0] .]
			set name  [join [lrange $list 1 end] .]
			set value [string trim [lindex $line 1]]
			set default($name) $value
		    }
		}
	    }
	    close $fh
	}
    }
    return ""
}

##
## Show the defaults as loaded from the tkined.defaults files.
##

proc ShowDefaults {} {

    global default

    if {![info exists default]} {
	ined acknowledge \
	    "No parameters used from the tkined.default files."
	return
    }

    set result ""
    foreach name [array names default] {
	set attname  [lindex [split $name .] 0]
	set attindex [join [lrange [split $name .] 1 end] .]
	lappend result [format "%-16s %-16s %s" \
			$attname $attindex $default($name)]
    }

    set result [lsort $result]
    ined browse "Defined defaults used by this application:" "" \
	"Attribute         Index            Value" $result
}

##
## Write a report to a log object. The name is expected to be in
## the variable tool_name which may be set in one of the init
## procs below.
##

proc writeln {{txt ""} {cmd ""}} {
    write "$txt\n" $cmd
}

proc write {{txt ""} {cmd ""}} {
    static log
    global tool_name

    if {![info exists tool_name]} {
	set tool_name "Unknown"
    }

    if {(![info exists log]) || ([ined -noupdate retrieve $log] == "")} {
	set log [ined -noupdate create LOG]
	ined -noupdate name $log "$tool_name Report"
    }

    if {$cmd == ""} {
	ined append $log "$txt"
    } else {
	ined hyperlink $log $cmd $txt
    }
}

##
## Try to execute a tcl script. Much like catch, but it allows to jump
## into a another body if we get an error.
##

# proc try {body args} {
#     if {$args != "" && [llength $args] != 2} {
# 	error "wrong # args: should be \"try body ?msg handler?\""
#     }
#     if {$args == ""} {
# 	catch [list uplevel 1 $body]
#     } else {
# 	upvar [lindex $args 0] msg
# 	if {[catch [list uplevel 1 $body] msg]} { 
# 	    catch [list uplevel 1 [lindex $args 1]]
# 	}
#     }
# }

##
## Print messages if running in debug mode.
##

proc debug {args} {

    global debug

    if {[info exists debug] && ($debug == "true")} {
	foreach arg $args {
	    writeln $arg
	}
    }
}


##
## =========================================================================
## ================== M O N I T O R   related subroutines ==================
## =========================================================================
##
##  CloneNode { id clone {offset_x 20} {offset_y 20} }
##  DeleteClones {}
##  MoJoCheckIds { cmd ids } 
##  MoJoInfo {}
##  MoJoSelect { action } 
##  MoJoModify {}
##  MoJoAction { id msg }
##  MoJoParameters
##
## =========================================================================
##

##
## Clone a node object given by id and move the new object to a
## position given by relative coordinates offset_x and offset_y.
## The clone operation is only done when the global variable
## clone is set to true. All new object ids are collected in the
## global list clone_ids.
##

proc CloneNode { id clone {offset_x 20} {offset_y 20} } {

    global clone_ids

    if {![info exists clone_ids]} { set clone_ids "" }

    # not all objects understand every command, so we catch it

    catch {ined -noupdate name    $clone [ined -noupdate name $id]}
    catch {ined -noupdate address $clone [ined -noupdate address $id]}
    catch {ined -noupdate color   $clone [ined -noupdate color $id]}
    catch {ined -noupdate font    $clone [ined -noupdate font $id]}
    catch {ined -noupdate label   $clone [ined -noupdate label $id]}
    catch {ined -noupdate icon    $clone [ined -noupdate icon $id]}
    
    if {[set alias [ined -noupdate attribute $id "SNMP:Alias"]] != ""} {
	catch {ined -noupdate attribute $clone "SNMP:Alias" $alias}
    }
    
    if {! [catch {ined -noupdate move $id} xy]} {
	set x [expr {[lindex $xy 0]+$offset_x}]
	set y [expr {[lindex $xy 1]+$offset_y}]
	catch {ined -noupdate move $clone $x $y}
    }

    lappend clone_ids $clone

    return $clone
}

##
## Delete all previously created clones.
##

proc DeleteClones {} {

    global clone_ids

    if {[info exists clone_ids]} {
	foreach id $clone_ids {
	    catch {ined delete $id}
	}
    }
}

##
## Check if all ined objects given by ids are still there. Modify the 
## current job if any objects have been removed and kill the job, if no 
## ids have been left. This proc assumes that a command is simply a 
## command name and a list of ids as arguments.
##

proc MoJoCheckIds { cmd ids } {

    set new_ids ""
    foreach id $ids {
        if {[ined retrieve $id] != ""} {
            lappend new_ids $id
        }
    }

    if {$new_ids != $ids} {
        if {$new_ids == ""} {
            [job current] destroy
	    return 
        } else {
	    [job current] configure -command [list $cmd $new_ids]
        }
    }

    return $new_ids
}

##
## Display the jobs currently running. The ids of the command are
## converted to the name attributes to increase readability.
##

proc MoJoInfo { } {

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
	      $jobid $jobitv $jobrem $jobcnt $jobstat \
	      [lindex $jobcmd 0] ]

	# Convert the id's to hostnames for readability.
	
	foreach id [lindex $jobcmd 1] {
	    if {[catch {lindex [ined name $id] 0} host]} {
		set host $id
	    }
	    if {[string length $line] < 65} {
		append line " $host"
	    } else {
		lappend result $line
		set line [format "%35s %s" "" $host]
	    }
	}

	lappend result $line
    }

    set header " ID    INTV    REM  CNT STATUS  COMMAND"

    foreach line $result {
	if {[string length $line] > $len} {
	    set len [string length $line]
	}
    }

    for {set i [string length $header]} {$i < $len} {incr i} {
	append header " "
    }

    ined browse $header $result
}

##
## Select one of the existing jobs for doing action with it.
## This is at least used by the MoJoModify proc below.
##

proc MoJoSelect { action } {
    
    set jobs [job find]

    if {$jobs == ""} {
	ined acknowledge "Sorry, no job to $action."
        return
    }

    set res ""
    foreach j $jobs {
	# Convert the id's to hostnames for readability.
	
	set hosts [lindex [$j cget -command] 0]
	foreach id [lindex [$j cget -command] 1] {
	    if {[catch {lindex [ined -noupdate name $id] 0} host]} {
		set host $id
	    }      
	    lappend hosts $host
	}
	if {[llength $hosts] > 4} {
	    set hosts "[lrange $hosts 0 3] ..."
	}
    
	set status [$j cget -status]
	if {   ($action == "resume"  && $status == "suspend")
            || ($action == "suspend" && $status == "waiting")
            || ( ($action != "resume") && ($action != "suspend"))} {
		lappend res [format "%s %s" [lindex $j 0] $hosts]
	    }
    }

    if {$res == ""} {
	ined acknowledge "Sorry, no job to $action."
	return ""
    } 

    set res [ined list "Choose a job to $action:" $res [list $action cancel]]
    if {[lindex $res 0] == "cancel"} {
	return ""
    } else {
	return [lindex [lindex $res 1] 0]
    }
}

proc MoJoModify {} {

    static threshold

    # Ask for the job to modify.

    set jobid [MoJoSelect modify]

    if {$jobid == ""} return

    # Set up default values.

    if {![info exists threshold(rising,$jobid)]} {
	set threshold(rising,$jobid) ""
    }
    if {![info exists threshold(falling,$jobid)]} {
	set threshold(falling,$jobid) ""
    }
    if {![info exists threshold(action,$jobid)]} {
        set threshold(action,$jobid) "flash"
    }

    # Get the details about the selected job.

    set jobcmd ""
    set jobitv ""
    foreach job [job find] {
	if {$jobid == $job} {
	    set jobcmd  [$job cget -command]
	    set jobitv  [expr {[$job cget -interval] / 1000.0}]
	    set jobstat [$job cget -status]
	}
    }
    
    # Convert the status waiting to active
    
    if {$jobstat == "waiting"} { set jobstat active }
	
    # Convert the id's to hostnames for readability.
    
    set hosts ""
    foreach id [lindex $jobcmd 1] {
	if {[catch {lindex [ined -noupdate name $id] 0} host]} {
	    set host $id
	}      
	lappend hosts $host
    }
    if {[llength $hosts] > 3} {
	set hosts "[lrange $hosts 0 2] ..."
    }
    
    # Request for changes.
    
    set rising $threshold(rising,$jobid)
    set falling $threshold(falling,$jobid)
    set action $threshold(action,$jobid)

    set res [ined request "Modify $jobid ([lindex $jobcmd 0] $hosts)" \
	      [list [list "Intervaltime \[s\]:" $jobitv entry 10] \
	            [list "Job Status:" $jobstat radio active suspend] \
		    [list "Falling Threshold:" $falling entry 10] \
		    [list "Rising Threshold:" $rising entry 10] \
		    [list "Threshold action:" \
		          $action check syslog flash write] ] \
	      [list modify "kill job" cancel] ]
    
    if {[lindex $res 0] == "cancel"} return

    if {[lindex $res 0] == "kill job"} {
	$jobid destroy
	return
    }
    
    set jobitv  [lindex $res 1]
    set jobstat [lindex $res 2]
    set falling [lindex $res 3]
    set rising  [lindex $res 4]
    set action  [lindex $res 5]

    set threshold(rising,$jobid)  $rising
    set threshold(falling,$jobid) $falling
    set threshold(action,$jobid)  $action
    
    if {$jobstat == "active"}  { $jobid configure -status waiting }
    if {$jobstat == "suspend"} { $jobid configure -status suspended }

    if {[catch {expr round($jobitv * 1000)} ms] || $jobitv <= 0} {
	ined acknowledge "Illegal interval time ignored."
	return
    }

    if {$ms != [$jobid cget -interval]} {

	$jobid configure -interval $ms

	set newlist ""
	foreach cmd [ined restart] {
	    if {[lindex $cmd 3] == $jobid} {
		set cmd [lreplace $cmd 2 2 $jobitv]
	    }
	    lappend newlist $cmd
	}
	ined restart $newlist
    }

    # Now add, update or remove the threshold attribute.

    foreach id [lindex $jobcmd 1] {
	catch {ined attribute $id Monitor:RisingThreshold  $rising}
	catch {ined attribute $id Monitor:FallingThreshold $falling}
	catch {ined attribute $id Monitor:ThresholdAction  $action}
    }
}

proc MoJoCheckThreshold { id txt value {unit ""} } {
    global default

    set action [ined attribute $id "Monitor:ThresholdAction"]
    if {$action == ""} {
	if {[info exists default(Monitor.ThresholdAction)]} {
	    set action $default(Monitor.ThresholdAction)
	} else {
	return
	}
    }

    set rising  [ined attribute $id Monitor:RisingThreshold]
    if {$rising == ""} {
	if {[info exists default(Monitor.RisingThreshold)]} {
	    set rising $default(Monitor.RisingThreshold)
	}
    }

    set falling [ined attribute $id Monitor:FallingThreshold]
    if {$falling == ""} {
	if {[info exists default(Monitor.FallingThreshold)]} {
	    set falling $default(Monitor.FallingThreshold)
	}
    }

    if {$rising == "" && $falling == ""} return

    for {set i 0} {$i < [llength $value]} {incr i} {
	catch {
	    set v [lindex $value $i]
	    set r [lindex $rising $i]
	    set f [lindex $falling $i]
	    if {$r != "" && $v > $r} {
		set msg "$txt $v $unit exceeds rising threshold $r $unit"
	    } elseif {$f != "" && $v < $f} {
		set msg "$txt $v $unit exceeds falling threshold $f $unit"
	    } else {
		continue
	    }
	    MoJoAction $id $msg
	} msg
    }
}

##
## MoJoAction does the required action. We currently support the action
## flash, syslog and write. The last one will just write a message to
## the output window.
##

proc MoJoAction { id msg } {

    global default

    set action [ined attribute $id "Monitor:ThresholdAction"]
    if {$action == ""} {
	if {[info exists default(Monitor.ThresholdAction)]} {
	    set action $default(Monitor.ThresholdAction)
	} else {
	    return
	}
    }

    if {[lsearch $action syslog] >= 0} {
	syslog warning $msg
    }
    if {[lsearch $action flash] >= 0} {
	set jobid [job current]
	if {$jobid != ""} {
	    set secs [expr {[$jobid cget -interval] / 1000}]
	} else {
	    set secs 2
	}
	ined flash $id $secs
    }
    if {[lsearch $action write] >= 0} {
	writeln "[clock format [clock seconds]]:"
	writeln $msg
	writeln
    }
}

##
## Set the default parameters for monitoring jobs.
##

proc MoJoParameter {} {
    global interval
    global default

    set result [ined request "Monitor Default Parameter" \
        [list [list "Interval \[s\]:" $interval entry 8] \
	  [list "Use Graph Diagram:" $default(graph) radio true false ] ] \
	[list "set values" cancel] ]

    if {[lindex $result 0] == "cancel"} return

    set interval       [lindex $result 1]
    set default(graph) [lindex $result 2]

    if {$interval<1} { set interval 1 }
}


##
## =========================================================================
## ======================= I P   related subroutines =======================
## =========================================================================
##
##  IpInit { toolname }
##
##  GetIpAddress { node }
##  ForeachIpNode { id ip host list body }
##  IpFlash { ip }
##  IpService {}
##  nslook { arg }
##
## =========================================================================
##

##
## Initialize global variables that are used by these ip procs.
##

proc IpInit { toolname } {

    global default icmp_retries icmp_timeout icmp_delay icmp_routelength
    global tool_name

    set tool_name $toolname

    if {[info exists default(retries)]} {
	set icmp_retries $default(retries)
    } else {
	set icmp_retries 3
    }
    icmp -retries $icmp_retries
    
    if {[info exists default(timeout)]} {
	set icmp_timeout $default(timeout)
    } else {
	set icmp_timeout 3
    }
    icmp -timeout $icmp_timeout
    
    if {[info exists default(delay)]} {
	set icmp_delay $default(delay)
    } else {
	set icmp_delay 5
    }
    icmp -delay $icmp_delay
    
    if {[info exists default(routelength)]} {
	set icmp_routelength $default(routelength)
    } else {
	set icmp_routelength 16
    }
}

##
## Get the IP Address of a node. Query the name server, if the
## address attribute is not set to something that looks like a
## valid IP address.
##

proc GetIpAddress { node } {
    if {[lsearch "NODE STRIPCHART BARCHART GRAPH" [ined type $node]] >= 0} {
        set host [lindex [ined name $node] 0]
        set ip [lindex [ined address $node] 0]
        if {[regexp {^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$} $ip] > 0} {
            return $ip
        }
	if {[regexp {^[0-9a-fA-F:]+$} $ip] > 0} {
	    return $ip
	}
	if {[regexp {^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$} $host] > 0} {
	    return $host
	}
	if {[regexp {^[0-9a-fA-F:]+$} $host] > 0} {
	    return $host
	}
        if {[catch {nslook $host} ip]==0} {
            return [lindex $ip 0]
        }
    }
    return ""
}

##
## Evaluate body for every node where we get an IP address for. The id
## and ip variables are set to the id of the NODE object and it's IP
## address.
##

proc ForeachIpNode { id ip host list body } {
    upvar $id lid
    upvar $ip lip
    upvar $host lhost
    foreach comp $list {
        if {[ined type $comp] == "NODE"} {
	    set lid [ined id $comp]
            set lip [GetIpAddress $comp]
	    set lhost [ined name $comp]
            if {$lip == ""} {
		set host [lindex [ined name $comp] 0]
                writeln "Can not lookup IP Address for $host."
                continue
            }
	    uplevel 1 $body
	}
    }
}

##
## Flash the icon of the object given by the ip address.
##

proc IpFlash { ip {secs 2} } {
    foreach comp [ined retrieve] {
	if {([ined type $comp] == "NODE") && ([GetIpAddress $comp] == $ip)} {
	    ined flash [ined id $comp] $secs
	}
    }
}

##
## Read the file /etc/services and let the user select a TCP service of
## interest. The proc returns a list containing the name and the port number.
##

proc IpService { protocol } {

    set service(X11) 6000
    foreach serv [netdb services] {
	if {[lindex $serv 2] == "tcp"} {
	    set service([lindex $serv 0]) [lindex $serv 1]
	}
    }

    set result [ined list "Select the TCP service of interest:" \
		  [lsort [array names service]] \
		  [list select other cancel]]

    switch [lindex $result 0] {
	cancel { return }
	select {
	    set name [lindex [lindex $result 1] 0]
	    if {$name == ""} return
	}
	other {
	    static sname
	    static sport
	    if {![info exists sport]} { set sport "" }
	    if {![info exists sname]} { set sname "noname" }
	    set result [ined request "Set name and port number for service:" \
			 [list [list Name: $sname] [list Port: $sport]] \
			 [list select cancel] ]
	    if {[lindex $result 0] == "cancel"} return
	    set sname [lindex $result 1]
	    set sport [lindex $result 2]
	    set name $sname
	    set service($name) $sport
	}
	{} {
	    set name [lindex [lindex $result 1] 0]
            if {$name == ""} return
	}
    }

    return [list $name $service($name)]
}

proc nslook {arg} {
    if {[catch {netdb hosts name $arg} result]} {
        set result [netdb hosts address $arg]
    }
    return $result
}

##
## =========================================================================
## ========== S N M P   related subroutines ================================
## =========================================================================
##
##  SnmpInit { toolname }
##  SnmpParameter { list }
##  SnmpOpen { ip }
##  SnmpEditScalars { s path }
##  SnmpEditTable { s table }
##
## =========================================================================
##


##
## Initialize global variables that are used by these snmp procs.
##

proc SnmpInit { toolname } {

    global snmp_community snmp_timeout snmp_retries snmp_window snmp_delay
    global snmp_port snmp_protocol snmp_context snmp_user
    global snmp_browser
    global tool_name
    global default

    if {[info exists default(community)]} {
	set snmp_community $default(community)
    } else {
	set snmp_community "public"
    }

    if {[info exists default(timeout)]} {
	set snmp_timeout $default(timeout)
    } else {
	set snmp_timeout 5
    }

    if {[info exists default(retries)]} {
	set snmp_retries $default(retries)
    } else {
	set snmp_retries 3
    }

    if {[info exists default(window)]} {
	set snmp_window $default(window)
    } else {
	set snmp_window 10
    }

    if {[info exists default(delay)]} {
	set snmp_delay $default(delay)
    } else {
	set snmp_delay 5
    }

    if {[info exists default(port)]} {
	set snmp_port $default(port)
    } else {
	set snmp_port 161
    }

    if {[info exists default(protocol)]} {
	set snmp_protocol $default(protocol)
    } else {
	set snmp_protocol SNMPv1
    }

    if {[info exists default(context)]} {
	set snmp_context $default(context)
    } else {
	set snmp_context ""
    }

    if {[info exists default(user)]} {
	set snmp_user $default(user)
    } else {
	set snmp_user public
    }

    set snmp_browser ""

    set tool_name $toolname
}


##
## Set the parameters (community, timeout, retry) for snmp requests.
##

proc SnmpParameter {list} {

    global snmp_community snmp_timeout snmp_retries snmp_window snmp_delay
    global snmp_port snmp_protocol snmp_context snmp_user

    set result [ined request "SNMP Default Parameter" \
	    [list [list "Community:" $snmp_community entry 20] \
	      [list "UDP Port:" $snmp_port entry 10] \
	      [list "Timeout \[s\]:" $snmp_timeout entry 10] \
	      [list "Retries:" $snmp_retries scale 1 8] \
	      [list "Window Size:" $snmp_window scale 0 100] \
	      [list "Delay \[ms\]:" $snmp_delay scale 0 100] \
	      [list "Protocol:" $snmp_protocol radio SNMPv1 SNMPv2c SNMPv3] \
	      [list "User:" $snmp_user entry 10] \
	      [list "Context:" $snmp_context entry 10] ] \
	    [list accept "list alias" cancel] ]

    if {[lindex $result 0] == "cancel"} return

    if {[lindex $result 0] == "list alias"} {
	set res [ined list "List of known SNMP aliases:" [lsort [snmp alias]] \
		"show dismiss"]
	switch [lindex $res 0] {
	    "" -
	    show {
		set alias [lindex $res 1]
		if {[catch {snmp generator -alias $alias} s]} {
		    ined acknowledge $s
		} else {
		    writeln "SNMP alias definition for alias \"$alias\":"
		    writeln "  [$s configure]"
		    writeln
		    $s destroy
		}
	    }
	}
	return
    }

    set snmp_community [lindex $result 1]
    set snmp_port      [lindex $result 2]
    set snmp_timeout   [lindex $result 3]
    set snmp_retries   [lindex $result 4]
    set snmp_window    [lindex $result 5]
    set snmp_delay     [lindex $result 6]
    set snmp_protocol  [lindex $result 7]
    set snmp_user      [lindex $result 8]
    set snmp_context   [lindex $result 9]
}


##
## Open a snmp session and initialize it to the defaults stored in
## the global variables snmp_community, snmp_timeout, snmp_retries.
##

proc SnmpOpen { id ip } {

    global snmp_community snmp_timeout snmp_retries snmp_window snmp_delay
    global snmp_port snmp_protocol snmp_user snmp_context
    global default

    set alias [ined attribute $id "SNMP:Alias"]
    if {$alias != ""} {
	if {[catch {snmp generator -address $ip -alias $alias} s] == 0} {
	    return $s
	}
	set name [ined name $id]
	set res [ined confirm \
		"Unknown SNMP alias \"$alias\" for node $name:" \
		[list edit "use default"]]
	if {[lindex $res 0] == "edit"} {
	    set res [ined list "Select attribute \"SNMP alias\" for $name:" \
		    [lsort [snmp alias]] "accept cancel"]
	    if {[lindex $res 0] == "cancel"} {
		return [SnmpOpen $id $ip]
	    }
	    set alias [lindex $res 1]
	    ined attribute $id "SNMP:Alias" $alias
	    return [snmp generator -alias $alias]
	}
    }

    set config [ined attribute $id "SNMP:Config"]
    if {$config != ""} {
	if {[catch {eval snmp generator $config} s] == 0} {
            return $s
        }
    }

    set s [snmp generator -address $ip -port $snmp_port \
	    -timeout $snmp_timeout -retries $snmp_retries \
	    -window $snmp_window -delay $snmp_delay]
	
    if {[info exists default(protocol.$ip)]} {
	set protocol $default(protocol.$ip)
    } else {
	set protocol $snmp_protocol
    }
    if {[info exists default(community.$ip)]} { 
	set community $default(community.$ip)
    } else {
	set community $snmp_community
    }
    if {[info exists default(user.$ip)]} {
        set user $default(user.$ip)
    } else {
        set user $snmp_user
    }
    if {[info exists default(context.$ip)]} {
        set context $default(context.$ip)
    } else {
        set context $snmp_context
    }

    switch $protocol {
	SNMPv1 {
	    $s configure -community $community -version SNMPv1
	}
	SNMPv2c {
	    $s configure -community $community -version SNMPv2c
	}
	SNMPv2u {
	    $s configure -user $user -context $context -version SNMPv2u
	}
	default {
	    ined acknowledge "Unsuppoted version \"$protocol\" for $id \[$ip\]"
	    return
	}
    }
 
    if {[info exists default(port.$ip)]} {
	$s configure -port $default(port.$ip)
    }
    if {[info exists default(timeout.$ip)]} { 
	$s configure -timeout $default(timeout.$ip) 
    }
    if {[info exists default(retries.$ip)]} { 
	$s configure -retries $default(retries.$ip) 
    }
    if {[info exists default(window.$ip)]} { 
	$s configure -window $default(window.$ip) 
    }
    if {[info exists default(delay.$ip)]} { 
	$s configure -delay $default(delay.$ip) 
    }

    return $s
}

##
## Edit some MIB scalars.
##

proc SnmpEditScalars {s path} {

    set ip [TnmSnmpGetIp $s]
    if {[catch {nslook $ip} host]} { set host "" }
    set host [lindex $host 0]

    foreach suc [mib children $path] {
	set access [mib access $suc]
	if {[string match *write* $access]} {
	    if {[catch {$s get $suc.0} var]} continue
	    lappend varlist [list [mib name $suc] [lindex [lindex $var 0] 2]]
	    lappend oidlist [lindex [lindex $var 0] 0]
	}
    }
    if {![info exists varlist]} {
	return
    }

    set result [ined request "Edit SNMP scalars ($path) on $host \[$ip\]:" \
	             $varlist [list "set scalars" cancel] ]
    if {[lindex $result 0] == "cancel"} {
        return
    }

    set idx 0
    foreach oid $oidlist {
	set old [lindex [lindex $varlist $idx] 1]
	incr idx
	set val [lindex $result $idx]
	if {$val == $old} continue
	if {[catch {$s set [list [list $oid [mib syntax $oid] $val]]} err]} {
	    ined acknowledge "Set on [mib name $oid] failed: " "" $err
	}
    }
}

##
## Edit a MIB table.
##

proc SnmpEditTable {s table args} {

    set ip [TnmSnmpGetIp $s]
    if {[catch {nslook $ip} host]} { set host "" }
    set host [lindex $host 0]
    set tablename [mib name $table]

    set list ""
    if {[catch {
	$s walk x [lindex [mib children [mib children $table]] 0] {
	    foreach x $x {
		set oid  [lindex $x 0]
		set val  [lindex $x 2]
		set name [mib name $oid]
		set pfx [lindex [split $name .] 0]
		set idx [join [lrange [split $name .] 1 end] .]
		lappend list "$tablename $idx"
	    }
	}
    } error]} {
	ined acknowledge "Failed to retrieve $tablename:" "" $error
        return
    }
    if {$list == ""} {	
	return
    }

    set result [ined list "Select a row to edit:" $list [list edit cancel]]
    if {[lindex $result 0] == "cancel"} {
        return
    }

    set idx [lindex [lindex $result 1] 1]
    
    set list ""
    foreach pfx [mib children [mib children $table]] {
	set access [mib access $pfx.$idx]
	if {[string match *write* $access]} {
	    set name [lindex [split [mib name $pfx.$idx] .] 0]
	    if {[catch {lindex [lindex [$s get $pfx.$idx] 0] 2} value]} {
		writeln "$value"
		continue
	    }
	    lappend list [list $name [lindex [lindex [$s get $pfx.$idx] 0] 2]]
	    lappend oidlist "$pfx.$idx"
	}
    }
    
    if {$list != ""} {
	set txt "Edit instance $idx of $tablename:"
	set result [ined request $txt $list [list "set values" cancel]]
	if {[lindex $result 0] != "cancel"} {
	    set i 0
	    foreach value [lrange $result 1 end] {
		lappend varbind [list [lindex $oidlist $i] $value]
		incr i
	    }
	    if {[catch {$s set $varbind} error]} {
		ined acknowledge "Set operation failed:" "" $error
	    }
	}
    }
}

