# Process this file with automake to produce Makefile.in
#
# $Id: Makefile.am,v 1.1.2.4 2006/08/18 23:12:30 fedemp Exp $
#

INDENT = indent
AM_CFLAGS = -I.. @C_STD_FLAGS@ @WARNING_FLAGS@ 

AM_LDFLAGS = -version-info @LT_VERSION@

lib_LTLIBRARIES = libthrulay.la

libthrulayincludedir = $(includedir)/thrulay

libthrulayinclude_HEADERS = client.h server.h

libthrulay_la_SOURCES = client.c server.c \
		       assertd.h assertd.c \
		       rcs.h \
		       reporting.h reporting.c \
		       util.h util.c

bin_PROGRAMS = thrulay

sbin_PROGRAMS = thrulayd

thrulay_SOURCES = thrulay.h thrulay.c

thrulay_LDFLAGS = libthrulay.la

thrulayd_SOURCES = thrulayd.h thrulayd.c

thrulayd_LDFLAGS = libthrulay.la

indent:
	$(INDENT) -orig *.c *.h
	rm -f *.c~ *.h~
