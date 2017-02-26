Welcome to the Scotty Tcl extension for network management

by Juergen Schoenwaelder and many contributors.  Maintained by FlightAware LLC

Introduction
---

You are looking at the source tree of scotty, a software package which allows to build network management applications using Tcl (and Tk). The scotty package includes the Tnm Tcl extension which provides Tcl commands to

* send and receive ICMP packets
* query the Domain Name System (DNS)
* access UDP sockets from Tcl
* probe and use some selected SUN RPCs
* retrieve and serve documents via HTTP
* send and reveice SNMP messages (SNMPv1, SNMPv2C, SNMPv3)
* write special purpose SNMP agents in Tcl
* parse and access SNMP MIB definitions
* schedule jobs that are to be done regularly
* realize event driven programming on network maps

This scotty distributions also includes the sources for Tkined. Tkined is a network editor which allows to draw maps showing your network configuration. The most important feature of Tkined is its programming interface which allows network management applications to extend the capabilities of Tkined. Most applications for Tkined are written using the Tnm Tcl extension.

Documentation
---

The "doc" subdirectory contains manual pages for the scotty package.  and the optional features. The most important ones are the Tnm.n page which gives an overview over the Tcl commands provided by the Tnm extension and the tkined.1 page which describes the Tkined network editor. You should be able to read them after installation with the UNIX "man" command, assuming your MANPATH contains the installation directory.

An overview about the Tnm extension has been presented at the 3rd Tcl/Tk workshop in 1995. A PostScript copy of this paper is available at <http://www.ibr.cs.tu-bs.de/users/schoenw/papers/tcltk-95.ps.gz>.

There is also a manual page for Tkined in the "doc" subdirectory. A short description of the API which is used to write new applications for the Tkined editor is available in the ined.n page.

Some more general experience from doing this project over several years were presented at the 1st European Tcl/Tk User Meeting in 2000. A PostScript copy of this paper is also available at <http://www.ibr.cs.tu-bs.de/users/schoenw/papers/tcltk-eu-2000.ps.gz>

Compiling and installing Tcl
---

This release requires Tcl8.4 and Tk8.4. You will find the latest version of these packages at <http://www.tcl.tk/> and many mirror sites around the globe. Note, this scotty version will most likely not work with later versions of Tcl and Tk.

This release contains everything you need to compile and run scotty and tkined with Tcl8.4 and Tk8.4. To compile, change to the "unix" subdirectory and follow the instructions in the README file in that directory for compiling scotty and tkined, installing it, and running the test suite. There is experimental code to compile scotty/tkined on 32 bit Windows platforms in the "win" subdirectory. See the README in the "win" subdirectory for more details.

Additional Information:
---

There is a mailing list which is used to announce new versions, to distribute small patches or discuss everything related to the Tnm extension and the Tkined network editor. To subscribe to this list, send a message to

	<tkined-request@ibr.cs.tu-bs.de> 

and follow the instructions. Additional information is available on the Web at the following URL:

	<http://wwwsnmp.cs.utwente.nl/~schoenw/scotty/>

These Web pages include release notes, pointer to related material, a list of frequently asked questions (FAQ) and the mailing list archive.

Support and bug fixes
---

We are generally interested in receiving bug reports or suggestions for improvements. When reporting bugs, please provide a good description how to reproduce the bug and indicate which version of Tcl and Scotty you are using plus the operating system name and version.  Bugs with a detailed bug report are usually fixed soon.

The fastest way to get bug fixed is of course to provide a patch. Patches should be submitted as unified context diffs (diff -u) rather than complete files to this list for the following reasons:

* Complete files makes it hard for readers to find the places where
  actually changes have been made.

* Complete files just waste bandwidth and storage space.

* Long messages are sorted out by mailman and require approval.

The easiest way to provide patches in in fact to grab the CVS sources and to produce a diff using "cvs diff" (and of course, you want to put something like "diff -u" into your ~/.cvsrc. Also do not forget to send patches for the documentation or test cases in case you patch changes the Tcl API in any way.

We can't provide much individual support for users. If you have some problems that you can't solve by reading the man pages, please make sure that it is not a Tcl related question (e.g. a quoting problem) before you send mail to the mailing list (the preferred way to ask questions as there are people who can help even if we are not on the
Internet).

Contributors
---

Scotty started very simple as a readline Tcl frontend when there was
no standard tclsh available. I added commands to access network
information when I started work on the network editor Tkined. Scotty
grew up in the last years and many people contributed ideas, patches
or complete modules to this package. Below is a list of contributors
(hopefully not too incomplete) in no particular order.

* The dns command and the icmp server ntping were written by Erik Schoenfelder <schoenfr@ibr.cs.tu-bs.de> who also helped me to fix bugs and to enhance portability.

* Stefan Schoek <schoek@ibr.cs.tu-bs.de> contributed the simple job scheduler. It was rewritten by me but the ideas were taken from his original Tcl version.

* Sven Schmidt <vschmidt@ibr.cs.tu-bs.de> did the original SNMP implementation.

* Michael Kernchen <kernchen@ibr.cs.tu-bs.de> wrote the GDMO parser and the CMIP Tcl interface based on the OSIMIS/ISODE MSAP API.

* Dirk Grunwald <grunwald@foobar.cs.colorado.edu> and Dan Mosedale <mosedale@genome.stanford.edu> provided patches for DEC alpha machines.

* Bob Shaw <bshaw@spdc.ti.com> helped by reporting bugs that made scotty more portable.

* Harlan Stenn <harlan@cms-stl.com> provided some bug fixes and a patch to get yanny to work on HP-UX and SGI boxes.

* John Rodkey <rodkey@westmx.westmont.edu> reported some portability problems on AIX machines.

* Reto Beeler <beeler@tech.ascom.ch> contributed a simple MIB browser.  It has been rewritten to use `hyperlinks' and is now part of the snmp browser script.

* John P. Rouillard <rouilj@cs.umb.edu> provided some patches to compile scotty on Solaris machines out of the box. He also contributed many ideas that improved the event and monitoring scripts.

* Juergen Luksch <luksch@telenet.de> reported compilation problems on SCO machines and provided some patches to fix them.

* De Clarke <de@lick.ucsc.edu> helped to make integration of scotty and extended Tcl easier.

* Sam Shen <sls@mh1.lbl.gov> send me a patch which implements the udp multicast command.

* Hakan Soderstrom <hs@soderstrom.se> wrote the msqltcl interface which is based on the sybtcl and oratcl interface witten by Tom Poindexter <tpoindex@nyx.cs.du.edu>. Thanks for making this good work available.

* David J. Hughes <Bambi@Bond.edu.au> did a great job on writing the Mini SQL Server. He did something successfully that we did not get right with our bones server.

* Doug Hughes <Doug.Hughes@Eng.Auburn.EDU> fixed a long outstanding bug in sunrpc probe and provided some other valuable fixes/comments.

* Peter Reilly <peter@euristix.ie> send some patches to correct the encoding of IP addresses.

* Hans Bayle <hansb@aie.nl> uncovered some bugs in the ASN.1 code (decode/encode object identifier) and some other minor problems while trying to run scotty on a SCO box.

* David Brower <dbrower@us.oracle.com> send in some patches to make purify happy.

* Giorgio Andreoli <andreoli@ercole.cefriel.it> helped to fix some Integer32 related problems.

* Jim Madden <jmadden@ucsd.edu> provided some patches to fix some bugs in the monitoring scripts.

* Andre Beck <beck@ibh-dd.de> provided some fixes for the interface load monitoring script.

* Richard Kooijman wrote the code to display a MIB tree on a canvas (as part of the tricklet package). 

* Andy <ayoder@fore.com> provided some patches for the MIB parser.

* Joergen Haegg <jh@axis.se> send some patches to improve portability.

* Peter J M Polkinghorne <Peter.Polkinghorne@gec-hrc.co.uk> provide some ideas that made processing of set operations `as if simultaneous' a bit easier.

* Graeme McKerrell <graemem@pdd.3com.com> provided some bug fixes.
              v
* Robert Premuz <rpremuz@malik.srce.hr> reported various bugs in the sources and the man page. He also helped to fix a couple of problems in the SNMP agent module.

* David Keeney <keeney@zk3.dec.com> send some patches for the USEC implementation and some 64 bit fixes for Digital UNIX V4.0.

* David Engel <david@ods.com> send some patches and ideas that improved the SNMP implementation.

* Mark J. Elkins <mje@mje99.posix.co.za> send some patched for the snmp_cisco.tcl script to upload/download cisco router config files.

* Peter Maersk-Moller <peter.maersk-moller@jrc.it> provided patches and enhancements for some of the Tkined scripts.

* Ron R.A.M. Sprenkels <sprenkel@cs.utwente.nl> found a couple of bugs and inconsistencies while trying to use scotty for an ATM project.

* Philip Hardin <philip_hardin@trigate.tri.sbc.com> send some patches for the GDMO parser to recognize the "SET-BY-CREATE" property.

* Martin F. Gergeleit <gergeleit@gmd.de> ported Sun's RPC sources to Windows NT and made it freely available.

* Havard Eidnes <Havard.Eidnes@runit.sintef.no> provided a bug fix for the nmtrapd before it was ever released. He also provided fixes for cases where you receive traps before clients do connect.

* Bram van de Waaij <waaij@cs.utwente.nl> contributed a map so that everyone can find breweries in the Netherlands.

* Poul-Henning Kamp <phk@freebsd.org> contributed some fixes for the nmicmpd to better handle broadcast addresses.

* Aaron Dewell <dewell@woods.net> helped to debug some Linux/Sparc byteorder problems.

* Michael I Schwartz <mschwart@du.edu> reported some installation problems and provided some bug fixes.

* Michael J. Long <mjlong@Summa4.COM> prototyped the code for reading and maintaining MIB size and range restrictions.

* Dave Zeltserman <davez9@gte.net> provided useful feedback on a pre-release of Tnm 3.0 on the Windows platform.

* David Perkins <dperkins@dsperkins.com> reviewed the Tnm::mib command and provided useful clarifications and suggestions.

* Viktor Dukhovni <Viktor-Dukhovni@deshaw.com> provided some bug fixes for early Tnm 3.0 snapshots.

* Eli Gurvitz <egurvitz@ndsisrael.com> provided bug fixes for the UNIX trap receiver implementation.

* Kevin Buhr <buhr@mozart.stat.wisc.edu> provided security related patches for the UNIX trap receiver implementation.

* Michael Rochkind <Mrochkind@ndsisrael.com> helped to fix some memory leaks.

* Carsten Zerbst <zerbst@tu-harburg.de> provided a bug fix for the UDP multicast code.

* Jun Kuriyama <kuriyama@imgsrc.co.jp> provided some bug fixes while porting to FreeBSD.

* Frank Strauss <strauss@ibr.cs.tu-bs.de> submitted scripts to interact with agents supporting the DISMAN-SCRIPT-MIB.

* Louis A. Mamakos <louie@transsys.com> provided useful feedback and some BSD related patches.

* Bill Fenner <fenner@research.att.com> provided some bug fixes for the frozen MIB loader.

* Matt Selsky <selsky@columbia.edu> provided several bug fixes.

* Pete Flugstad <pete_flugstad@icon-labs.com> provided patches to keep the VC++ compiler happy.

* George Ross <gdmr@dcs.ed.ac.uk> provided patches to make packaging easier.

The Tkined network editor started as an experiment to rewrite an already existing network editor called ined on top of the Tk/Tcl toolkit.

* The original ined editor was written by Thomas Birke <birke@ibr.cs.tu-bs.de> and Hinnerk Ruemenapf <rueme@ibr.cs.tu-bs.de> using the InterViews toolkit.

* Guntram Hueske <hueske@ibr.cs.tu-bs.de> turned the stripchart and barchart Tcl code into C code that implements two new canvas items.  Guntram also wrote an application called tkgraphs which inspired the GRAPH object type and the diagram window.

* Erik Schoenfelder <schoenfr@ibr.cs.tu-bs.de> contributed ideas that really improved the tkined interface. He is also a very good tester.

* Thanks to Brian Bartholomew <bb@math.ufl.edu> for some interesting proposals and Henning G. Schulzrinne <hgs@research.att.com> for some bug reports.

* Many thanks to Mark Weissman <weissman@gte.com> for making his emacs key bindings available, although they are not used anymore.

* Dieter Rueffler <dieter@prz.tu-berlin.de> proposed to integrate "hyperlinks" to remote tkined maps using URLs. The idea is now implemented as reference objects.

* Juergen Luksch <luksch@telenet.de> reported compilation problems on SCO machines and provided some suggestions on how to fix them.

* Detlev Habicht <habicht@ims.uni-hannover.de> provided some icons.

* Stefan Petri <petri@ibr.cs.tu-bs.de> reported some tricky bugs.

* Jim Madden <jmadden@ucsd.edu> send some patches to fix some bugs.

* John Rouillard <rouilj@cs.umb.edu> always sends very good bug and wish lists to the mailing list. This usually causes a lot of work but is always appreciated.

* Dan Razzell <razzell@cs.ubc.ca> send a patch to implement the ined eval command which allows a script to play with internals from tkined.

* Poul-Henning Kamp <phk@freebsd.org> contributed some changes to the topology discovering application.

* Xavier Redon <Xavier.Redon@eudil.fr> submitted some bug reports and bug fixes for the topology discovering application.

* Klaus Goessl <goessl@powernet.de> provided a fix for a bug which prevented users from moving objects horizontally or vertically.

* Paul Gampe <paulg@apnic.net> contributed a patch which allows to monitor storage utilizations via the HOST-RESOURCES-MIB.

* Van Trinh <van.trinh@sun.com> contributed a patch to let the trap listener use the currenly configured SNMP version.

* Jussi Kuosa <jussi.kuosa@tellabs.com> contributed patches to improve the Win32 initialization and better align with Tcl/Tk stubs.

Many thanks to all the bug reports and suggestions reported to me or 
the tkined mailing list.

Acknowledgements
---

The development of this software took place at the Technical University of Braunschweig (Germany) and the University of Twente (The Netherlands).  Thanks to both Universities for allowing me to make these sources freely available.

Many thanks also to Optical Data Systems <http://www.ods.com/> for sponsoring the porting project to Windows NT.

Thanks go also to my friends at Gaertner Datensysteme <http://www.gaertner.de/> for their support and their work on the Windows NT port.

Additional thanks go UUNET <http://www.uunet.com/> for sponsoring our SNMPv3 implementation.
