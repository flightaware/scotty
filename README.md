[![Build Status](https://travis-ci.org/flightaware/scotty.svg?branch=master)](https://travis-ci.org/flightaware/scotty)

Welcome to the _scotty_ Tcl extension for network management

by Juergen Schoenwaelder and many contributors.  Maintained by FlightAware LLC

Introduction
============

You are looking at the source tree of _scotty_, a software package
which allows to build IPv4 network management applications using Tcl
and Tk.  It consists of two Tcl extensions: _Tnm_ and _Tkined_, two
related applications: `scotty` a Tcl interpreter with _Tnm_ preloaded
and `tkined` a interactive gui network diagramm editor and monitor.

_scotty_ license terms can be found in `tnm/license.terms` which
is essentially identical to the
[Tcl/Tk license](https://www.tcl.tk/software/tcltk/license.html).


Tnm extension
-------------

The Tnm Tcl extension provides the following features:

* Access to Internet protocols: ICMP, DNS, UDP, SUN RPC, SNMP, NTP.
* Facilitates writing special purpose SNMP agents in Tcl and parse and
  access SNMP MIB definitions.
* Access the local network databases: hosts, networks, protocols,
  services, sunrpcs.
* Schedule jobs that are to be done regularly.
* Realize event driven programming on network maps.
* IP address and networks calculation.
* Write messages to the system logger.


tkined and the Tkined extension
-------------------------------

`tkined` is a network editor which allows to draw maps showing your
network configuration. The most important feature of `tkined` is its
programming interface which allows network management applications to
extend the capabilities of `tkined`. _scotty_ packages several
applications which allow for network autodetection, node monitoring,
geolocation and others.


About and Status
----------------

This source distribution is targetted at the current available
(February 2017) toolchains and Tcl versions.  The default is to build
shared libraries with Tcl stubs support.

`tkined` is working fine on all tested platform, except for Debian
jessie 32 bit, where it hangs on the first invocation of `ined`.

Scotty currently is only operating correctly on 32 bit platforms.
There are several known bugs which surface when running on 64 bit.

Note that the organisation of the source tree has been modified
heavily, to account for current policies of extension building.  Each
extension is given its own subdirectory, where they can be built
independently.

_Tnm_ tests have been updated and problems singled out and documented
in the `tnm/TODO` file.  Running `make test` should succeed without
failures, except DNS tests which depend on specific network setup and
availability and might fail on your site.

The current versions are _Tnm_ 3.1.3 and _Tkined_ 1.6.0. Due to the
large number of files that need to be updated to a new version, there
are helper scripts tnm.patch and tki.patch in the root directory that
perform the heavy lifting for you. If you change any file containing
the version numbers check whether these files need updating.


Obtaining and Installing scotty
===============================

_scotty_ is freely available in source form from several locations.
The historical homepage does not exist anymore. FlightAware has done
significant maintenance of the software.  The FlightAware version is
available from: <https://github.com/flightaware/scotty>

This particlar version, motivated by FlightAware, adapts the build
system to recent versions of operating systems and Tcl/Tk versions and
is available from: <https://github.com/jorge-leon/scotty>


Tested Operating Systems
------------------------

- Debian GNU/Linux 7.11 (wheezy), 8.7 (jessie) *
- Ubuntu 16.10 (yakkety) *
- Alpine Linux (Note: temporarily unavailable)
- Slackware Linux 14.2
- FreeBSD 10.3 * and 11
- NetBSD 7.0.2
- MacOSX 10.10.5 (Yosemite)
- Oracle Solaris 11.3

Note: operating systems marked with an * have the best support and
test coverage.  See tnm/TODO and tkined/TODO for a list of known
problems.


Tested Tcl/Tk Versions
----------------------

- 8.5.11 (Debian wheezy)
- 8.5.17 (Debian jessie)
- 8.6.2 (Debian jessie)
- 8.6.4 (MacOSX)
- 8.6.5 (Slackware)
- 8.6.6 (FreeBSD, Ubuntu, Alpine, Solaris)


Compilation Requirements
========================

* Tcl/Tk development files.
* OS specific (libc) development files.
* C compiler:
    * GCC
    * Clang on FreeBSD
* Make:
    * GNU Make
    * PMake on FreeBSD
* GNU autoconf
* On Alpine Linux (musl libc):
    * The libtirpc-dev package.
    * pkg-config (to find libtirpc)


Obtain, Compile, Install
------------------------

Clone the repository or download the source as zip archive and unpack
to any destination directory of your liking.

There is a convenience Makefile in the top level directory, which
automates build, install and uninstall for the tested platforms.  Just
run `make` and it will help you.  See the file PORTS for platform
specific details.

The following are detailed build instructions.

Enter the `tnm` directory and run the almost common place:
```
./configure
make
sudo make install
sudo make sinstall
```

Then enter the `tkined` directory and run:
```
./configure
make
sudo make install
```

After this, you should be able to start `scotty` via `scotty.3.0.x`
where 'x' is the respective sub sub version number and tkined as
`tkined`.

OS X Install
------------
The OS X configure commands most likely require --prefix=/usr/local and --exec-prefix=/usr/local options.
OS X now prevents installation in /usr/lib and /usr/bin in the System Integrity Protection implementation.
TEA trys to figure out the install paths from the TCL install. If you are using the OS X TCL, then these
directories will not be writable.

Uninstall
---------

You can uninstall the _Tnm_ extension and `scotty` by running `make
uninstall` in the `tnm` directory.

The _Tkined_ extension and `tkined` are uninstalled by running `make
uninstall` in the `tkined` directory.


Documentation
=============

Man files are installed in the respective system locations after
install.  To see what is available see the respective `doc`
subdirectory in `tnm` and `tkined`.

For a starter, look at Tnm(n) which gives an overview over the Tcl
commands provided by the Tnm extension and the tkined(1) page which
describes the Tkined network editor. A short description of the API
which is used to write new applications for the Tkined editor is
available in the ined(n) man page.

An overview about the Tnm extension has been presented at the 3rd
Tcl/Tk workshop in 1995. A PostScript copy of this paper is available
at <http://www.ibr.cs.tu-bs.de/users/schoenw/papers/tcltk-95.ps.gz>.

Some more general experience from doing this project over several
years were presented at the 1st European Tcl/Tk User Meeting
in 2000. A PostScript copy of this paper is also available at
<http://www.ibr.cs.tu-bs.de/users/schoenw/papers/tcltk-eu-2000.ps.gz>

Mark Newnham has collected some information about _scotty_ at his
sourceforge project wiki:
<https://sourceforge.net/p/tkined-scotty/wiki/Home>

The Tclers Wiki has some pages about _scotty_, here the link to the
most informative: [Tnm/Scotty/TkInEd](http://wiki.tcl.tk/691).

The (almost) original README file can be found in the `tnm` sub
directory.  Original build/install was done inside the platform
specific directories, you will find a INSTALL instructions file in
`tnm/unix`.

The _Tkined_ library and applications have their own change logs in
`tkined/changes` and `tkined/apps/changes`.

The file PORTS holds notes about building _scotty_ on different
platforms, `tnm/TODO` and `tkined/TODO` list bugs and improvement
ideas for the respective extension, including platform specific ones.


Credits
=======

Jürgen has listed meticulously all contributors and benefactors at
the end of the original `tnm/README.md` file.

Lionel Sambuc has provided all needed bits to compile scotty on
MINIX3.


History
=======

According to `tnm/changes` _scotty_ was released on 1993-07-19 as
version 0.5, this change log file reports until 2001-12-08.

In 2005 Jürgens seems to have imported the CVS repository to SVN at
Jakobs University, with the last check in on 2010-09-07.

DavidMcNett at FlightAware imported _scotty_ into GitHub, 'Initial
import of sc-scotty from Karl' (Lehenbauer).  Since then, there were
small commits every one to two years on this repository.

Mark Newnham imported _scotty_ to GitHub and then in 2015-03-14 to
sourceforge.  He has mainly worked on the "Front End" `tkined`.

Upgraded to the latest TEA version and improvements in UDP and DNS
2017-02-14 by Georg Lehner <jorge@at.anteris.net>.

Ressources remaining from the founders time:
* [Braunschweig university scotty page](https://www.ibr.cs.tu-bs.de/projects/scotty).
* Jürgens
  [SVN repository](https://cnds.eecs.jacobs-university.de/svn/schoenw/src/scotty/)
  at Jacobs University.
*
[Mailing list archive](https://mail.ibr.cs.tu-bs.de/pipermail/tkined/)
at Braunschweig university.

Some pages on the Tclers Wiki, discussing the status at different
points of time:

* [Scotty](http://wiki.tcl.tk/220)
* [Scotty in 2003](http://wiki.tcl.tk/8437)
* [Scotty in 2004](http://wiki.tcl.tk/12640)



# Emacs
# Local Variables:
# mode: markdown
# End:
