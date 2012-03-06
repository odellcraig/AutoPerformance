# @configure_input@
#
# $Id: Makefile.am,v 1.1.2.2 2006/06/14 16:07:55 fedemp Exp $
#

# for old versions of 'make'
@SET_MAKE@

# bootstrap.sh and README.release are not distributed.
EXTRA_DIST = LICENSE TODO

SUBDIRS = src doc

indent:
	@cd src && $(MAKE) indent

debian-package:	all
	mkdir -p debian-package/DEBIAN || true
	mkdir -p debian-package/usr/share/doc/thrulay || true
	mkdir -p debian-package/usr/share/man/man1 || true
	mkdir -p debian-package/usr/share/man/man8 || true
	cp -R debian/* debian-package/DEBIAN
	mkdir -p debian-package/usr/bin
	cp src/thrulay debian-package/usr/bin
	cp src/thrulayd debian-package/usr/bin
	cat doc/thrulay.1 | gzip -9 > debian-package/usr/share/man/man1/thrulay.1.gz
	cat doc/thrulayd.8 | gzip -9 > debian-package/usr/share/man/man8/thrulayd.8.gz
	strip debian-package/usr/bin/*
	cp README LICENSE TODO debian-package/usr/share/doc/thrulay
	cp doc/thrulay-protocol.txt debian-package/usr/share/doc/thrulay
	fakeroot dpkg-deb -b debian-package .
	lintian thrulay*.deb
