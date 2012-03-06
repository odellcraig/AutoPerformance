/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* multicast support */
#define ENABLE_THRULAY_MULTICAST 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the <fasttime.h> header file. */
/* #undef HAVE_FASTTIME_H */

/* Define to 1 if you have the <float.h> header file. */
#define HAVE_FLOAT_H 1

/* Define to 1 if you have the `getrusage' function. */
#define HAVE_GETRUSAGE 1

/* Define to 1 if you have the `initstate' function. */
#define HAVE_INITSTATE 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `fasttime' library (-lfasttime). */
/* #undef HAVE_LIBFASTTIME */

/* Define to 1 if you have the `m' library (-lm). */
#define HAVE_LIBM 1

/* Define to 1 if you have the `nsl' library (-lnsl). */
#define HAVE_LIBNSL 1

/* Define to 1 if you have the `resolv' library (-lresolv). */
#define HAVE_LIBRESOLV 1

/* Define to 1 if you have the `rt' library (-lrt). */
#define HAVE_LIBRT 1

/* Define to 1 if you have the `socket' library (-lsocket). */
/* #undef HAVE_LIBSOCKET */

/* Define to 1 if you have the `tsci2' library (-ltsci2). */
/* #undef HAVE_LIBTSCI2 */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Struct ip_mreq */
/* #undef HAVE_STRUCT_IP_MREQ */

/* Struct ip_mreqn */
#define HAVE_STRUCT_IP_MREQN 1

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/wait.h> header file. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the <tsci2.h> header file. */
/* #undef HAVE_TSCI2_H */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* getaddrinfo function */
/* #undef HAVE_W32_GETADDRINFO */

/* Define to 1 if you have the <winsock2.h> header file. */
/* #undef HAVE_WINSOCK2_H */

/* Define to 1 if you have the <ws2tcpip.h> header file. */
/* #undef HAVE_WS2TCPIP_H */

/* Name of package */
#define PACKAGE "thrulay"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "thrulay-users@internet2.edu"

/* Define to the full name of this package. */
#define PACKAGE_NAME "thrulay"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "thrulay 0.9rc1"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "thrulay"

/* Define to the version of this package. */
#define PACKAGE_VERSION "0.9rc1"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "0.9"

/* Need this under Darwin so that socklen_t will be defined in <sys/socket.h>
   */
/* #undef _BSD_SOCKLEN_T_ */

/* Darwin */
/* #undef __DARWIN__ */

/* FreeBSD */
/* #undef __FREEBSD__ */

/* Linux */
#define __LINUX__ 1

/* Solaris */
/* #undef __SOLARIS__ */
