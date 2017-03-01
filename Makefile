#
# This is a convenience Makefile for building and installing _scotty_
# on some platforms.
#

PLATFORMS	= jessie freebsd alpine macosx

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
	@echo "Other targets: uninstall distclean"
	@echo
	@echo Available platform targets: $(PLATFORMS)
	@echo

jessie freebsd:
	(cd tnm && autoconf && ./configure)
	(cd tkined && autoconf && ./configure)

alpine:
	(cd tnm && autoconf && ./configure --with-tirpc)
	(cd tkined && autoconf && ./configure)

macosx:
	(cd tnm && autoconf && ./configure --bindir=/usr/local/bin/ --libdir=/Library/Tcl)
	(cd tkined && autoconf && ./configure --bindir=/usr/local/bin/ --libdir=/Library/Tcl)

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
