/*
 * util.c -- common utility routines for thrulay.
 * 
 * Written by Stanislav Shalunov, http://www.internet2.edu/~shalunov/
 *            Bernhard Lutzmann, belu@users.sf.net
 *            Federico Montesino Pouzols, fedemp@altern.org
 * 
 * Copyright 2003, 2006, Internet2.
 * Legal conditions are in file LICENSE
 * (MD5 = ecfa50d1b0bfbb81b658c810d0476a52).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#ifndef WIN32
#include <sys/socket.h>
#include <netinet/in.h>
#endif
#include "util.h"
#include "rcs.h"

#ifndef SOL_IP
#ifdef IPPROTO_IP
#define SOL_IP		IPPROTO_IP
#endif
#endif

#ifndef SOL_IPV6
#ifdef IPPROTO_IPV6
#define SOL_IPV6	IPPROTO_IPV6
#endif
#endif

RCS_ID("@(#) $Id: util.c,v 1.1.1.1.2.6 2006/08/20 18:06:19 fedemp Exp $")

#ifndef HAVE_GETRUSAGE
#ifdef WIN32
/* Implementation of getrusage based on Windows native calls. Note
   this is a minimal implementation that only fills in the fields used
   by the thrulay server (ru_utime and ru_stime) with the times for
   the current thread. */
int getrusage(int process, struct rusage* rusage)
{
	FILETIME f_kernel, f_user;
	HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE,
				   GetCurrentThreadId());

	if (GetThreadTimes(thread, NULL, NULL, &f_kernel, &f_user) ) {
		LONGLONG t_user = *(LONGLONG*)&f_user;
		LONGLONG t_kernel = *(LONGLONG*)&f_kernel;
		if (rusage) {
			rusage->ru_utime.tv_usec = (DWORD)(t_user/10);
			rusage->ru_stime.tv_usec = (DWORD)(t_kernel / 10);
		}
	} else {
		if (rusage)
			memset(rusage, 0, sizeof(rusage));
	}

	return 0;
}
#endif
#endif

#ifdef WIN32

int
gettimeofday(struct timeval *tv,  void *tz)
{
	DWORD ms = GetTickCount();
	tv->tv_sec = ms / 1000;
	tv->tv_usec = ms * 1000;
	return 0;
}

#ifdef HAVE_W32_GETADDRINFO
const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{
	socklen_t len;

	switch (af) {
	case AF_INET:
		len = sizeof(struct sockaddr_in);
		break;
	case AF_INET6:
		len = sizeof(struct sockaddr_in6);
		break;
	default:
		return NULL;
	}

	if ( 0 != getnameinfo((struct sockaddr *)src, len, dst, cnt, 
			      NULL, 0, NI_NUMERICHOST) )
		return NULL;

	return dst;
}

const char *gai_strerror(int errcode)
{
	static char result[256];

	/* Obviously, this is not the best solution. A list of error
	   descriptions should be added at some time. */
	snprintf(result,sizeof(result),
		 "error code: %d; no description available on Windows",
		 errcode);

	return result;
}

#endif

#endif /* ifdef WIN32 */

void
error(int errcode, const char *msg)
{
	const char *prefix;
	int fatal = 1;

	if (errcode == ERR_FATAL) {
		prefix = "fatal";
	} else if (errcode == ERR_WARNING) {
		prefix = "warning";
		fatal = 0;
	} else {
		prefix = "UNKNOWN ERROR TYPE";
	}
	fprintf(stderr, "%s: %s\n", prefix, msg);
	if (fatal)
		exit(1);
}

ssize_t
recv_exactly(int d, void *buf, size_t nbytes)
{
	ssize_t rc = 0;
	size_t bytes_read = 0;
	while (bytes_read < nbytes &&
	/* In Win32, read/write don't work with sockets! */
#ifndef WIN32
	       (rc = read(d, (char *)buf+bytes_read,
			  nbytes-bytes_read)) > 0)
#else
	       (rc = recv(d, (char *)buf+bytes_read,
			  nbytes-bytes_read, 0)) > 0)
#endif
		bytes_read += rc;
	return rc == -1? rc: (ssize_t) bytes_read;
}

ssize_t
write_exactly(int d, const void *buf, size_t nbytes)
{
	ssize_t rc = 0;
	size_t bytes_written = 0;
	while (bytes_written < nbytes && 
	       (rc = write(d, (const char *)buf+bytes_written,
			   nbytes-bytes_written)) > 0)
		bytes_written += rc;
	return rc == -1? rc: (ssize_t) bytes_written;
}

ssize_t
send_exactly(int d, const void *buf, size_t nbytes)
{
	ssize_t rc = 0;
	size_t bytes_written = 0;
	/* In Win32, read/write don't work with sockets! */
#ifndef WIN32
	while (bytes_written < nbytes && 
	       (rc = write(d, (const char *)buf+bytes_written,
			   nbytes-bytes_written)) > 0)
#else 
	while (bytes_written < nbytes && 
	       (rc = send(d, (const char *)buf+bytes_written,
			  nbytes-bytes_written, 0)) > 0)
#endif
		bytes_written += rc;
	return rc == -1? rc: (ssize_t) bytes_written;
}

double
time_diff(const struct timeval *tv1, const struct timeval *tv2)
{
	return (double) (tv2->tv_sec - tv1->tv_sec)
		+ (double) (tv2->tv_usec - tv1->tv_usec)/1000000.0;
}

/* Set window size for file descriptor FD (which must point to a
   socket) to WINDOW for given direction DIRECTION (typically
   SO_SNDBUF or SO_RCVBUF).  Try hard.  Return actual window size. */
int
set_window_size_directed(int fd, int window, int direction)
{
	int rc, try, w;
	unsigned int optlen = sizeof w;

	rc = getsockopt(fd, SOL_SOCKET, direction, (char *)&w, &optlen);
	if (rc == -1)
		return -1;
	if (window <= 0)
		return w;

	try = window;
	do {
		rc = setsockopt(fd, SOL_SOCKET, direction,
				(char *)&try, optlen);
		try *= 7;
		try /= 8;
	} while (try > w && rc == -1);

	rc = getsockopt(fd, SOL_SOCKET, direction, (char *)&w, &optlen);
	if (rc == -1)
		return -1;
	else
		return w;
}

/* Set window size for file descriptor FD (which must point to a
   socket) to WINDOW.  Try hard.  Return actual window size. */
int
set_window_size(int fd, int window)
{
	int send, receive;

	send = set_window_size_directed(fd, window, SO_SNDBUF);
	receive = set_window_size_directed(fd, window, SO_RCVBUF);
	return send < receive? send: receive;
}

/* Set RFC 2474 style DSCP value for TOS byte. Returns 0 on success, -1 on
 * failure. */
int
set_dscp(int sock, uint8_t dscp)
{
	int optname = IP_TOS;
	int optlevel = SOL_IP;
	int sopt;

	if ((dscp & ~0x3F)) {
		fprintf(stderr, "Error: set_dscp(): bad DSCP value.\n");
		return -1;
	}

	/* Shift to set CU to zero
	 *
	 *   0   1   2   3   4   5   6   7
	 * +---+---+---+---+---+---+---+---+
	 * |         DSCP          |  CU   |
	 * +---+---+---+---+---+---+---+---+
	 */
	sopt = dscp << 2;

	{
		struct sockaddr_storage addr;
		socklen_t len = sizeof(addr);

		if (getsockname(sock, (struct sockaddr *)&addr, 
				&len) == -1) {
			perror("getsockname");
			return -1;
		}

		switch (((struct sockaddr *)&addr)->sa_family) {
		case AF_INET:
			optlevel = SOL_IP;
			optname = IP_TOS;
			break;
		case AF_INET6:
#ifdef IPV6_TCLASS
			optlevel = SOL_IPV6;
			optname = IPV6_TCLASS;
			break;
#else
			error(ERR_WARNING, "system does not support setting "
					"DSCP value in IPv6 traffic class.");
			return 0;	/* return 0 so we don't get two
					   error messages. */
#endif
		default:
			error(ERR_WARNING, "set_dscp(): Unknown address "
					"family");
			return -1;
		}
	}

	if (setsockopt(sock, optlevel, optname, (char *)&sopt, 
		       sizeof(sopt)) == -1) {
		perror("setsockopt");
		return -1;
	}

	return 0;
}

#define NTP_EPOCH_OFFSET	2208988800ULL

/*
 * Convert `timeval' structure value into NTP format (RFC 1305) timestamp.
 * The ntp pointer must resolve to already allocated memory (8 bytes) that
 * will contain the result of the conversion.
 * NTP format is 4 octets of unsigned integer number of whole seconds since
 * NTP epoch, followed by 4 octets of unsigned integer number of
 * fractional seconds (both numbers are in network byte order).
 */
void
tv2ntp(const struct timeval *tv, char *ntp)
{
	uint32_t msb, lsb;

	msb = tv->tv_sec + NTP_EPOCH_OFFSET;
	lsb = (uint32_t)((double)tv->tv_usec * 4294967296.0 / 1000000.0);

	msb = htonl(msb);
	lsb = htonl(lsb);

	memcpy(ntp, &msb, sizeof(msb));
	memcpy(ntp + sizeof(msb), &lsb, sizeof(lsb));
}

/*
 * Convert 8-byte NTP format timestamp into `timeval' structure value.
 * The counterpart to tv2ntp().
 */
void
ntp2tv(struct timeval *tv, const char *ntp)
{
	uint32_t msb, lsb;

	memcpy(&msb, ntp, sizeof(msb));
	memcpy(&lsb, ntp + sizeof(msb), sizeof(lsb));

	msb = ntohl(msb);
	lsb = ntohl(lsb);

	tv->tv_sec = msb - NTP_EPOCH_OFFSET;
	tv->tv_usec = (uint32_t)((double)lsb * 1000000.0 / 4294967296.0);
}

/* Make sure 0 <= tv.tv_usec < 1000000.  Return 0 if it was normal,
 * positive number otherwise. */
int
normalize_tv(struct timeval *tv)
{
	int result = 0;

	while (tv->tv_usec >= 1000000) {
		tv->tv_usec -= 1000000;
		tv->tv_sec++;
		result++;
	}
	while (tv->tv_usec < 0) {
		tv->tv_usec += 1000000;
		tv->tv_sec--;
		result++;
	}
	return result;
}

#ifndef HAVE_SYSLOG_H

/* Definition of logging functions when missing. Logs are written to a
   '.log' file. */

FILE *syslog_file;
int syslog_pid;
char *syslog_filename_suffix = "-syslog.log";
char *syslog_ident;

void
openlog(const char *ident, int option, int facility)
{
	const size_t buf_max_size = 256;
	char buf[buf_max_size];

	syslog_ident = strdup(ident);
	/* Concatenate ident + syslog_filename_suffix */
	memccpy(buf, ident, '\0', 
		buf_max_size - strlen(syslog_filename_suffix) - 1);
	strncpy(buf + strlen(ident), syslog_filename_suffix, 
		strlen(syslog_filename_suffix) + 1);

	if (syslog_file)
		fclose(syslog_file);

	syslog_file = fopen(buf, "a");
}

void
syslog(int priority, const char *format, ...)
{
	const char *priority_string[] = { "", "alert", "",
					  "error", "warning", "notice" };
	const size_t line_max_size = 512;
	char line[line_max_size];
	int rc;
	time_t now;
	struct tm *dt;
	va_list ap;

	time(&now);
	dt = localtime(&now);

	rc = snprintf(line, line_max_size, 
		      "%04d-%02d-%02d %02d:%02d:%02d %s(%s): ",
		      dt->tm_year + 1900, dt->tm_mon + 1, dt->tm_mday,
		      dt->tm_hour, dt->tm_min, dt->tm_sec, 
		      syslog_ident, priority_string[priority]);
	
	va_start(ap, format);
	vsnprintf(line + rc, line_max_size - rc, format, ap);
	va_end(ap);

	if (syslog_file) {
		fputs(line, syslog_file);
		fputs("\n", syslog_file);
	}
}

void
closelog(void)
{
	if (syslog_file)
		fclose(syslog_file);
	free(syslog_ident);
}

#endif /* HAVE_SYSLOG_H */
