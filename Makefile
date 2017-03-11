#
# This is a convenience Makefile for building and installing _scotty_
# on some platforms.
#

PLATFORMS	= wheezy jessie yakkety freebsd alpine macosx sunos

all:
	@echo
	@echo "This is a Makefile for convenient building and installing"
	@echo "of scotty with standard configuration."
	@echo
	@echo "Typical invocation:"
	@echo "  make _platform_"
	@echo "  make build"
	@echo "  sudo make install"
	@echo
	@echo "Other targets: clean uninstall distclean"
	@echo
	@echo Available platform targets: $(PLATFORMS)
	@echo

wheezy jessie yakkety freebsd slackware:
	(cd tnm && autoheader && autoconf && ./configure)
	(cd tkined && autoheader && autoconf && ./configure)

alpine:
	(cd tnm && autoheader && autoconf && ./configure --with-tirpc)
	(cd tkined && autoheader && autoconf && ./configure)

macosx:
	(cd tnm && autoheader && autoconf \
	    && ./configure --bindir=/usr/local/bin/ --libdir=/Library/Tcl)
	(cd tkined && autoheader && autoconf \
	    && ./configure --bindir=/usr/local/bin/ --libdir=/Library/Tcl)

netbsd:
	(cd tnm && autoheader && autoconf \
	    && ./configure --with-tcl=/usr/pkg/lib)
	(cd tkined && autoheader && autoconf \
	    && ./configure --with-tcl=/usr/pkg/lib --with-tk=/usr/pkg/lib)

sunos:
	(cd tnm && autoheader && autoconf \
	    && ./configure --with-tcl=/usr/local/lib)
	(cd tkined && autoheader && autoconf \
	    && ./configure --with-tcl=/usr/local/lib --with-tk=/usr/local/lib)

minix:
	(cd tnm && autoheader && autoconf \
	    && ./configure --with-tcl=/usr/pkg/lib)
	(cd tkined && autoheader && autoconf \
	    && ./configure --with-tcl=/usr/pkg/lib --with-tk=/usr/pkg/lib)

build:
	(cd tnm && make)
	(cd tkined && make)

install:
	(cd tnm && make install sinstall)
	(cd tkined && make install)

uninstall:
	(cd tnm && make uninstall)
	(cd tkined && make uninstall)

clean:
	(cd tnm && make clean)
	(cd tkined && make clean)

distclean:
	(cd tnm && make distclean)
	(cd tkined && make distclean)
