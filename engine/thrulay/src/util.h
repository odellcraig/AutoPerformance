/*
 * util.h -- common utility routines for thrulay, headers.
 * 
 * Written by Stanislav Shalunov, http://www.internet2.edu/~shalunov/
 *            Bernhard Lutzmann, belu@users.sf.net
 *            Federico Montesino Pouzols, fedemp@altern.org
 * 
 * @(#) $Id: util.h,v 1.1.1.1.2.3 2006/08/20 18:06:19 fedemp Exp $
 * 
 * Copyright 2003, 2006, Internet2.
 * Legal conditions are in file LICENSE
 * (MD5 = ecfa50d1b0bfbb81b658c810d0476a52).
 */

#ifndef THRULAY_UTIL_H_INCLUDED
#define THRULAY_UTIL_H_INCLUDED

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#ifdef HAVE_FASTTIME_H
#include <fasttime.h>
#endif

#ifdef HAVE_TSCI2_H
#include <tsci2.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#ifndef WIN32
#include <sys/uio.h>
#else
#define _WIN32_WINNT 0x0501   /* Will only work in Windows XP or greater */
#include <ws2tcpip.h>
#endif

#include <unistd.h>

#define BLOCK_HEADER	16
#define MIN_BLOCK	BLOCK_HEADER
#define MAX_BLOCK	1048576

#define ERR_FATAL	0
#define ERR_WARNING	1

void
error(int, const char *);

ssize_t
recv_exactly(int d, void *buf, size_t nbytes);

ssize_t
write_exactly(int, const void *, size_t);

ssize_t
send_exactly(int d, const void *buf, size_t nbytes);

double
time_diff(const struct timeval *, const struct timeval *);

int
set_window_size_directed(int, int, int);

int
set_window_size(int, int);

int
set_dscp(int, uint8_t);

void
tv2ntp(const struct timeval *tv, char *);

void
ntp2tv(struct timeval *tv, const char *);

int
normalize_tv(struct timeval *);

/* 
 * Custom definition of some types and functions that are not
 * available on WIN32.
 */
#ifdef WIN32

#ifndef ECONNRESET
#define ECONNRESET WSAECONNRESET
#endif

int
gettimeofday(struct timeval *tv, void *tz);

struct timespec
{
	long int tv_sec;
	long int tv_nsec;
};

#define RUSAGE_SELF      0
#define RUSAGE_CHILDREN -1

struct rusage
{
	struct timeval ru_utime;
	struct timeval ru_stime;
};

int
getrusage(int process, struct rusage* rusage);

#ifdef HAVE_W32_GETADDRINFO
const char*
inet_ntop(int af, const void *src, char *dst, socklen_t cnt);

const char*
gai_strerror(int errcode);
#endif

#endif /* #ifndef WIN32 */

void
logging_log (int, const char *, ...);

#ifndef HAVE_SYSLOG_H

/* Definition of syslog related symbols (note only those currently
   used in thrulayd are defined). */
#ifndef LOG_WARNING
#define LOG_WARNING     4
#define LOG_NOTICE      5
#define LOG_ERR         3
#define LOG_ALERT       1
#endif

#ifndef LOG_NDELAY
#define LOG_NDELAY      0x08
#define LOG_CONS        0x02
#define LOG_PID         0x01
#endif

#ifndef LOG_DAEMON
#define LOG_DAEMON      (3<<3)
#endif

void
openlog(const char *ident, int option, int facility);

void
syslog(int priority, const char *format, ...);

void
closelog(void);

#endif /* HAVE_SYSLOG_H */

/*
 * We define `tsc_init()' and `tsc_gettimeofday()' here. If a TSC library is
 * installed, we use it. If none is found, we use good old `gettimeofday()'.
 *
 * Supported TSC libraries:
 *   fasttime: http://fasttime.sf.net/
 *   TSC-I2: http://tsc-xluo.sf.net/
 */
#if defined(HAVE_FASTTIME_H) && defined(HAVE_LIBFASTTIME)
#define tsc_init()		fasttime_init()
#define tsc_gettimeofday(tv)	fasttime_gettimeofday(tv)
#elif defined(HAVE_TSCI2_H) && defined(HAVE_LIBTSCI2)
#define tsc_init()		tsci2_init(TSCI2_DAEMON | \
				           TSCI2_CLIENT | \
				           TSCI2_FALLBACK)
#define tsc_gettimeofday(tv)	tsci2_gettimeofday(tv, 0)
#else
#define tsc_init()		;
#define tsc_gettimeofday(tv)	gettimeofday(tv, 0)
#endif

#endif /* ifndef THRULAY_UTIL_H_INCLUDED */
