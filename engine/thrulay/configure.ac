# $Id: configure.ac,v 1.4.2.11 2007/02/01 09:19:39 fedemp Exp $
#
# Process this file with autoconf to produce a configure script.
AC_INIT([thrulay], [0.9rc1], [thrulay-users@internet2.edu])
PACKAGE=thrulay
VERSION="0.9"
# For libtool
LT_VERSION="0:0:0"
# AC_PREREQ([2.59])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([$PACKAGE], [$VERSION])

# Configure options.
# Multicast support.
AC_MSG_CHECKING([for multicast])
AC_ARG_ENABLE([multicast],
	      AC_HELP_STRING([--disable-multicast], 
			     [disable multicast (enabled by default)]))
if test -z "$enable_multicast"; then
   enable_multicast=yes
fi
if test "$enable_multicast" = "yes"; then
   AC_DEFINE([ENABLE_THRULAY_MULTICAST], [1], [multicast support])
fi

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AM_PROG_LIBTOOL

# Flags for restrictive warnings:
if test $GCC = yes ; then
   C_STD_FLAGS="-std=gnu99"
   WARNING_FLAGS="-Wno-long-long -Wall -pedantic -W -Wpointer-arith \
				 -Wnested-externs"
else
   C_STD_FLAGS=""
   WARNING_FLAGS=""
fi
AC_SUBST([C_STD_FLAGS])
AC_SUBST([WARNING_FLAGS])

# Checks for host-system types
AC_CANONICAL_HOST
case $host in
  *darwin*)
    AC_DEFINE([__DARWIN__], [1], [Darwin])
    AC_DEFINE([_BSD_SOCKLEN_T_], [], [Need this under Darwin so that \
    	socklen_t will be defined in <sys/socket.h>])
    ;;
  *freebsd*)
    AC_DEFINE([__FREEBSD__], [1], [FreeBSD])
    ;;
  *linux*)
    AC_DEFINE([__LINUX__], [1], [Linux])
    ;;
  *solaris*)
    AC_DEFINE([__SOLARIS__], [1], [Solaris])
    ;;
esac

# Checks for libraries.
AC_CHECK_LIB([m], [log])
AC_CHECK_LIB([socket], [socket])
AC_CHECK_LIB([nsl], [socket])
AC_CHECK_LIB([resolv], [herror])
AC_CHECK_LIB([rt], [nanosleep])
AC_CHECK_LIB([fasttime], [fasttime_init_context])
AC_CHECK_LIB([tsci2], [tsci2_init])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([netdb.h syslog.h sys/resource.h sys/select.h sys/socket.h sys/wait.h])
AC_CHECK_HEADERS([float.h stdint.h])
AC_CHECK_HEADERS([winsock2.h ws2tcpip.h])
AC_CHECK_HEADERS([fasttime.h tsci2.h])

# Check if MinGW32 specific libs are required.
AC_SEARCH_LIBS([gethostbyname], [nsl socket], [], [
my_old_LIBS=$LIBS
LIBS="-lws2_32 $LIBS"
ws2_result=yes
AC_MSG_CHECKING([for gethostbyname in ws2_32])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <winsock2.h>]],
				[[gethostbyname("");]])],
		[], [LIBS=$my_old_LIBS ws2_result=no])
AC_MSG_RESULT([$ws2_result])
])

# Checks for functions
AC_MSG_CHECKING([for getaddrinfo])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <ws2tcpip.h>]],
				   [[getaddrinfo(NULL,NULL,NULL,NULL);]])], 
		  [AC_DEFINE([HAVE_W32_GETADDRINFO], [1], 
			     [getaddrinfo function])
		   AC_MSG_RESULT([yes])],
		  [AC_MSG_RESULT([no])])

AC_CHECK_FUNCS([initstate getrusage])

# Checks for typedefs, structures, and compiler characteristics.

# Check for multicast structs
AC_MSG_CHECKING([for struct ip_mreqn])
AC_TRY_COMPILE([#ifndef WIN32
		#include <netinet/in.h>
		#else
		#include <ws2tcpip.h>
		#endif], 
	       [struct ip_mreqn mreq;
	        mreq.imr_address.s_addr = 0;], 
	       [AC_MSG_RESULT([yes])
	        AC_DEFINE([HAVE_STRUCT_IP_MREQN], [1], [Struct ip_mreqn])], 
	       [AC_MSG_RESULT([no])])
AC_MSG_CHECKING([for struct ip_mreq])
AC_TRY_COMPILE([#ifndef WIN32
		#include <netinet/in.h
		#else
		#include <ws2tcpip.h>
		#endif], 
	       [struct ip_mreq mreq;
	        mreq.imr_interface.s_addr = 0;], 
	       [AC_MSG_RESULT([yes])
	        AC_DEFINE([HAVE_STRUCT_IP_MREQ], [1], [Struct ip_mreq])],
	       [AC_MSG_RESULT(no)])

case $host in
  *solaris*)
    AC_MSG_NOTICE([Solaris system detected, disabling stripping of binaries])
    STRIP=
    ;;
esac

AC_SUBST([LT_VERSION])
AC_OUTPUT([Makefile doc/Makefile src/Makefile thrulay.spec])
