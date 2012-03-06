/**
 * client.c -- thrulay library, client API implementation.
 * 
 * Written by Stanislav Shalunov, http://www.internet2.edu/~shalunov/
 *            Bernhard Lutzmann, belu@users.sf.net
 *            Federico Montesino Pouzols, fedemp@altern.org
 * 
 * @(#) $Id: client.c,v 1.1.2.13 2006/08/20 18:06:19 fedemp Exp $
 *
 * Copyright 2003, 2006 Internet2.
 * Legal conditions are in file LICENSE
 * (MD5 = ecfa50d1b0bfbb81b658c810d0476a52).
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <assert.h>
#include <signal.h>
#include <sys/param.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <errno.h>
#ifndef WIN32
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>	/* need this under FreeBSD for
				 * <netinet/ip.h> */
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#else
#define _WIN32_WINNT 0x0501   /* Will only work in Windows XP or later */
#include <winsock2.h>
#include <ws2tcpip.h>
#define sleep(t)    _sleep(t)
#define close(s) closesocket(s)  /* If close is only used with sockets*/
#endif /* ndef WIN32 */

#include "client.h"
#include "reporting.h"
#include "rcs.h"
#include "util.h"

#define THRULAY_GREET		THRULAY_VERSION "+"

#define DEFAULT_PORT		5003
#define UDP_PORT		5003
#define TRY_UDP_PORTS		1000

#define IPV4_HEADER_SIZE	20
#define IPV6_HEADER_SIZE	40
#define UDP_HEADER_SIZE		8
#define UDP_PAYLOAD_SIZE	24

#ifndef SOL_IP
#ifdef IPPROTO_IP
#define SOL_IP			IPPROTO_IP
#endif
#endif

#ifndef SOL_TCP
#ifdef IPPROTO_TCP
#define SOL_TCP			IPPROTO_TCP
#endif
#endif

/* IP_MTU not defined in <bits/in.h> under Linux */
#if defined (__LINUX__) && !defined(IP_MTU)
#define IP_MTU 14
#endif

#if defined (__SOLARIS__)
#define RANDOM_MAX		4294967295UL	/* 2**32-1 */
#elif defined (__DARWIN__)
#define RANDOM_MAX		LONG_MAX	/* Darwin */
#else
#define RANDOM_MAX		RAND_MAX	/* Linux, FreeBSD, Windows */
#endif

RCS_ID("@(#) $Id: client.c,v 1.1.2.13 2006/08/20 18:06:19 fedemp Exp $")

int
thrulay_tcp_init(void);

int
thrulay_tcp_init_id(int);

void
thrulay_tcp_exit(void);

void
thrulay_tcp_exit_id(int);

int
thrulay_tcp_start(void);

void
thrulay_tcp_stop(void);

int
thrulay_tcp_report(void);

int
thrulay_tcp_report_id(int);

void
thrulay_tcp_report_final(void);

void
thrulay_tcp_report_final_id(int);

int
thrulay_udp_init(void);

void
thrulay_udp_exit(void);

int
thrulay_udp_start(void);

int
thrulay_udp_report_final(void);

/* Statistics */

#define STREAM_PER_INTERVAL_QUANTILE_SEQ(id)  (2 * id)
#define STREAM_FINAL_QUANTILE_SEQ(id)         (2 * id + 1)

/* Get how many quantile sequences are required to keep statistics for
   a given number of streams. */
int
required_quantile_seqs(int num_streams);

int
tcp_stats_init(void);

/* Timer */
int
timer_start(void);

int
timer_stop(void);

int
timer_check(void);

int
timer_report(struct timeval *);

void
timer_end(struct timeval *);

/* Client options */
static thrulay_opt_t thrulay_opt;

static fd_set rfds_orig, wfds_orig;
static int maxfd = 0;
static char *block;
static int stop_test = 0;
static int server_block_size;
static int local_window, server_window;
static int mtu, mss;

/* UDP test */
static unsigned int client_port, server_port;
static unsigned int packet_size;
static unsigned int protocol_rate;	/* In packets per 1000 seconds. */
static int tcp_sock, udp_sock;
static uint64_t npackets;
static struct sockaddr *server = NULL;
static struct sockaddr *udp_destination = NULL;
static socklen_t udp_destination_len;

/* Stream information */
static struct _stream {
	/* Connection socket */
	int sock;
	/* Counters for tracking send/recv progress with non-blocking I/O */
	size_t wcount;
	size_t rcount;
} stream[STREAMS_MAX];

/* Statistics information (per stream) */
static struct _stat {
	/* Block counter */
	unsigned int blocks_since_first;
	unsigned int blocks_since_last;

	/* RTT */
	double min_rtt_since_first;
	double min_rtt_since_last;
	double max_rtt_since_first;
	double max_rtt_since_last;
	double tot_rtt_since_first;
	double tot_rtt_since_last;
} stats[STREAMS_MAX];

/* Timer information */
static struct _timer {
	struct timeval start;
	struct timeval stop;
	struct timeval next;
	struct timeval last;
	double runtime;
} timer;

/* Be very careful with error code numbers! */
static const char *thrulay_client_error_s[] = {
	"No error", /* 0 */
	"gettimeofday(): failed", /* 1 */
	"could not initialize UDP test", /* 2 */
	"getaddrinfo(): failed for multicast group", /* 3 */
	"malloc(): failed", /* 4 */
	"different local window", /* 5 */
	"gettimeofday() failed, unable to start timer", /* 6 */
	"gettimeofday() failed, unable to process new timestamp", /* 7 */
	"gettimeofday() failed, unable to print stream stats.", /* 8 */
	"getaddrinfo(): failed for server name resolution while initializing a TCP test", /* 9 */
	"getaddrinfo(): failed for server name resolution while initializing an UDP test", /* 10 */
	"could not establish connection to server", /* 11 */
	"could not read server banner", /* 12 */
	"not a thrulay server responded", /* 13 */
	"could not read rejection reason", /* 14 */
	"stop (connection rejected", /* 15 */
	"could not send session proposal", /* 16 */
	"could not read session response", /* 17 */
	"server closed connection after proposal", /* 18 */
	"could not send terminating message", /* 19 */
	"could not recv session results", /* 20 */
	"server rejected our UDP proposal", /* 21 */
	"malformed session response from server", /* 22 */
	"socket address family not supported", /* 23 */
	"socket address family not supported for multicast", /* 24 */
	"nanosleep() failed", /* 25 */
	"sending UDP packet failed",  /* 26 */
	"server gave block size less than MIN_BLOCK", /* 27 */
	"server gave block size too large", /* 28 */
	"server gave ridiculously small window", /* 29 */
	"different server window", /* 30 */
	"different server block size", /* 31 */
	"different MSS", /* 32 */
	"different MTU", /* 33 */
	"select(): failed", /* 34 */
	"WSAStartup failed while initializing client", /* 35 */
	"error in quantile computation" /* 36 */
};

static const int max_thrulay_client_error = 36;

const char *
thrulay_client_strerror(int errorcode)
{
	if( 0 >= errorcode && errorcode >= -max_thrulay_client_error){
		return thrulay_client_error_s[-errorcode];
	} else {
		return NULL;
	}	
}

/* Set default options. */
void
thrulay_client_options_init (thrulay_opt_t *opt)
{
	if(NULL == opt)
		return;

	opt->server_name = NULL;
	opt->num_streams = 1;
	opt->test_duration = 60;
	opt->reporting_interval = 1;
	opt->reporting_verbosity = 0;
	opt->window = 4194304;
	opt->block_size = 0;
	opt->port = DEFAULT_PORT;
	opt->rate = 0;
	opt->dscp = 0;
	opt->busywait = 0;
	opt->ttl = 1;
	opt->mcast_group = NULL;
}

int
tcp_stats_init (void)
{
	int rc, id;

	for (id = 0; id < thrulay_opt.num_streams; id++)
	{
		/* Blocks */
		stats[id].blocks_since_first = 0;
		stats[id].blocks_since_last = 0;

		/* RTT */
		stats[id].min_rtt_since_first = 1000.0;
		stats[id].min_rtt_since_last = 1000.0;
		stats[id].max_rtt_since_first = -1000.0;
		stats[id].max_rtt_since_last = -1000.0;
		stats[id].tot_rtt_since_first = 0.0;
		stats[id].tot_rtt_since_last = 0.0;
	}

	rc = quantile_init(required_quantile_seqs(thrulay_opt.num_streams), 
			   QUANTILE_EPS, 1024 * 1024);
	if (-1 == rc) {
		return -4;
	}

	return 0;
}

void 
tcp_stats_exit()
{
	/* Deinitialize quantile sequences. */
	quantile_exit();
}

int
required_quantile_seqs(int num_streams)
{
	int quantile_seqs;

	/* We need two quantile sequences per stream (one for interval
	   reports and one for the stream final report - plus another
	   quantile sequence for global statistics. */
	if (num_streams == 1) {
		/* Fortunately, if there is only one stream, we can
		   save 1 quantile sequence. */
		quantile_seqs = 2;
	} else {
		quantile_seqs = 2 * num_streams + 1;
	}
	return quantile_seqs;
}

/* Process new timestamp. */
int
new_timestamp(int id, struct timeval *tv)
{
	struct timeval this;
	double relative;
	int rc;

	if (tsc_gettimeofday(&this) == -1) {
		perror("gettimeofday");
		return -7;
	}
	normalize_tv(&this);

	relative = time_diff(tv, &this);
	if (relative < 0) {
		error(ERR_WARNING, "negative delay, ignoring block");
		return 0;
	}

	/* quantile sequence for stream interval report. */
	rc = quantile_value_checkin(STREAM_PER_INTERVAL_QUANTILE_SEQ(id), 
				    relative);
	if (rc < 0)
		return -36;
	/* quantile sequence for stream final report. */
	rc = quantile_value_checkin(STREAM_FINAL_QUANTILE_SEQ(id), relative);
	if (rc < 0)
		return -36;
	/* If there is more than one stream, we use an additional
	   global sequence for the global final report. */
	if (thrulay_opt.num_streams > 1) {
		rc = quantile_value_checkin(2 * thrulay_opt.num_streams, 
					    relative);
		if (rc < 0)
			return -36;
	}

	/* Update statistics for stream. */
	stats[id].blocks_since_first++;
	stats[id].blocks_since_last++;
	if (stats[id].min_rtt_since_first > relative)
		stats[id].min_rtt_since_first = relative;
	if (stats[id].min_rtt_since_last > relative)
		stats[id].min_rtt_since_last = relative;
	if (stats[id].max_rtt_since_first < relative)
		stats[id].max_rtt_since_first = relative;
	if (stats[id].max_rtt_since_last < relative)
		stats[id].max_rtt_since_last = relative;
	stats[id].tot_rtt_since_first += relative;
	stats[id].tot_rtt_since_last += relative;

	return 0;
}

/* If test duration is over, stops TCP test. */
void
timer_end (struct timeval *now)
{
	if (now->tv_sec > timer.stop.tv_sec
			|| (now->tv_sec == timer.stop.tv_sec
				&& now->tv_usec >= timer.stop.tv_usec)) {
		thrulay_tcp_stop();
	}
}

/* If progress report should be displayed does so and updates timer values. */
int
timer_report (struct timeval *now)
{
	int rc;

	if (now->tv_sec > timer.next.tv_sec
			|| (now->tv_sec == timer.next.tv_sec
				&& now->tv_usec >= timer.next.tv_usec)) {
		rc = thrulay_tcp_report();
		if (rc < 0)
			return rc;

		timer.last.tv_sec = now->tv_sec;
		timer.last.tv_usec = now->tv_usec;

		while (timer.next.tv_sec < now->tv_sec
				|| (timer.next.tv_sec == now->tv_sec
					&& timer.next.tv_usec <= now->tv_usec))
			timer.next.tv_sec += thrulay_opt.reporting_interval;
	}

	return 0;
}

int
timer_check (void)
{
	int rc;
	struct timeval this;

	if (tsc_gettimeofday(&this) == -1) {
		perror("gettimeofday");
		return -1;
	}
	normalize_tv(&this);

	if (0 != thrulay_opt.reporting_interval) {
		rc = timer_report(&this);
		if (rc < 0)
			return rc;
	}
	timer_end(&this);

	return 0;
}

/* Stop timer. Calculates runtime and saves this to `timer.runtime'. */
int
timer_stop (void)
{
	struct timeval this;

	if (tsc_gettimeofday(&this) == -1) {
		perror("gettimeofday");
		return -1;
	}
	normalize_tv(&this);

	/* Set final runtime. */
	timer.runtime = time_diff(&timer.start, &this);

	return 0;
}

/* Start timer. This saves the starting time to `timer.start', saves time when
 * next progress report should be displayed in `timer.next' and time when
 * test should stop in `timer.stop'. This are all timeval structures. */
int
timer_start (void)
{
	if (tsc_gettimeofday(&timer.start) == -1) {
		perror("gettimeofday");
		return -6;
	}
	normalize_tv(&timer.start);

	timer.stop.tv_sec = timer.start.tv_sec + thrulay_opt.test_duration;
	timer.stop.tv_usec = timer.start.tv_usec;

	if (0 != thrulay_opt.reporting_interval) {
		/* if intermediate reports not disabled */
		timer.last.tv_sec = timer.start.tv_sec;
		timer.last.tv_usec = timer.start.tv_usec;

		timer.next.tv_sec = timer.start.tv_sec + 
			thrulay_opt.reporting_interval;
		timer.next.tv_usec = timer.start.tv_usec;
	} else {
		/* if disabled. Unnecessary, just to keep consistency */
		timer.last.tv_sec = timer.stop.tv_sec + 1;
		timer.last.tv_usec = 0;

		timer.next.tv_sec = timer.stop.tv_sec + 1;
		timer.next.tv_usec = 0;
	}

	return 0;
}

void
thrulay_tcp_report_final_id(int id)
{
	int rc, quantile_seq;
	double quantile_25, quantile_50, quantile_75;

	if (stats[id].blocks_since_first == 0) {
		/* This stream was not very active :) */
		if (thrulay_opt.reporting_verbosity > 0) {
			printf("#(%2d) %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f "
			       "%8.3f %8.3f\n",
			       id, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
		} else {
			printf("#(%2d) %8.3f %8.3f %8.3f %8.3f %8.3f %8.3f\n",
			       id, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
		}
		return;
	}

	if (stats[id].blocks_since_first > 3 ) {
		quantile_seq = STREAM_FINAL_QUANTILE_SEQ(id);
		/* Finish stream final sequence. */
		rc = quantile_finish(quantile_seq);
		/* Get results. */
		quantile_output(quantile_seq, stats[id].blocks_since_first, 
				0.25, &quantile_25);
		quantile_output(quantile_seq, stats[id].blocks_since_first, 
				0.50, &quantile_50);
		quantile_output(quantile_seq, stats[id].blocks_since_first, 
				0.75, &quantile_75);
	} else {
		if (1 == stats[id].blocks_since_first) {
			quantile_25 = quantile_50 = quantile_75 = 
				stats[id].min_rtt_since_first;
		} else if (2 == stats[id].blocks_since_first) {
			quantile_25 = quantile_50 = 
				stats[id].min_rtt_since_first;
			quantile_75 =
				stats[id].max_rtt_since_first;
		}  else {
			quantile_25 = stats[id].min_rtt_since_first;
			quantile_50 = stats[id].tot_rtt_since_first -
				stats[id].max_rtt_since_first -
				stats[id].min_rtt_since_first;
			quantile_75 = stats[id].max_rtt_since_first;
		}
	}

	printf("#(%2d) %8.3f %8.3f %8.3f %8.3f %8.3f",
	       id,
	       0.0,
	       timer.runtime,
	       (double)stats[id].blocks_since_first *
	       (double)server_block_size * 8.0 / 1000000.0 /
	       timer.runtime,
	       1000.0 * quantile_50,                  /* delay */ 
	       1000.0 * (quantile_75 - quantile_25)   /* jitter */ 
	       );

	/* Verbose output shows min., avg. and max. */
	if (thrulay_opt.reporting_verbosity > 0) {
		printf(" %8.3f %8.3f %8.3f\n",
		       stats[id].min_rtt_since_first * 1000.0,
		       stats[id].tot_rtt_since_first * 1000.0 /
		       (double)stats[id].blocks_since_first,
		       stats[id].max_rtt_since_first * 1000.0);
	} else {
		printf("\n");
	}
}


void
thrulay_tcp_report_final(void)
{
	double mbs = 0.0;
	double min_rtt = 1000.0;
	double max_rtt = -1000.0;
	double tot_rtt = 0.0;
	double avg_rtt_sum = 0.0;
	uint64_t total_blocks = 0;
	double quantile_25, quantile_50, quantile_75;
	int id, gseq;


	/* If just one stream, no need to compute any more. */
	if (thrulay_opt.num_streams > 1) {
		for (id = 0; id < thrulay_opt.num_streams; id++)
			thrulay_tcp_report_final_id(id);
	}
	
	/* Now calculate global statistics. */
	for (id = 0; id < thrulay_opt.num_streams; id++) {
		/* Total number of blocks */
		total_blocks += stats[id].blocks_since_first;

		/* Calculate throughput of all streams together. */
		mbs += (double)stats[id].blocks_since_first *
			(double)server_block_size * 8.0 / 1000000.0 /
			timer.runtime;

		/* Calculate minimum RTT */
		min_rtt = (min_rtt < stats[id].min_rtt_since_first ?
			   min_rtt : stats[id].min_rtt_since_first);

		/* Calculate maximum RTT */
		max_rtt = (stats[id].max_rtt_since_first < max_rtt ?
			   max_rtt : stats[id].max_rtt_since_first);

		/* Calculate sum of average RTT */
		if (stats[id].blocks_since_first != 0) {
			tot_rtt += stats[id].tot_rtt_since_first;
			avg_rtt_sum += stats[id].tot_rtt_since_first *
				1000.0 /
				(double)stats[id].blocks_since_first;
		}
	}

	/* Finish global sequence. */
	if (thrulay_opt.num_streams > 1 ) {
		gseq = 2 * thrulay_opt.num_streams;
	} else {
		/* Just 1 stream, so the global sequence is the same
		   as the stream total sequence. */
		gseq = 1;
	}

	if (total_blocks > 3) {
		quantile_finish(gseq);
		/* Get global quantiles. */
		quantile_output(gseq, total_blocks, 0.25, &quantile_25);
		quantile_output(gseq, total_blocks, 0.50, &quantile_50);
		quantile_output(gseq, total_blocks, 0.75, &quantile_75);
	} else {
		if (1 == total_blocks) {
			quantile_25 = quantile_50 = quantile_75 = min_rtt;
		} else if (2 == total_blocks) {
			quantile_25 = quantile_50 = min_rtt;
			quantile_75 = max_rtt;
		}  else {
			quantile_25 = min_rtt;
			quantile_50 = tot_rtt - max_rtt - min_rtt;
			quantile_75 = max_rtt;
		}
	}

	printf("#(**) %8.3f %8.3f %8.3f %8.3f %8.3f",
	       0.0,
	       timer.runtime,			    /* global runtime */
	       mbs,			 	    /* MB/s */
	       1000.0 * quantile_50,                /* delay */ 
	       1000.0 * (quantile_75 - quantile_25) /* jitter */ 
	       );

	/* Verbose output shows min., avg. and max. */
	if (thrulay_opt.reporting_verbosity > 0) {
		printf(" %8.3f %8.3f %8.3f\n",
		       min_rtt * 1000.0,		/* minimal RTT */
		       avg_rtt_sum / thrulay_opt.num_streams,	/* avg. RTT */
		       max_rtt * 1000.0 		/* maximal RTT */
		       );
	} else {
		printf("\n");
	}
}

/* Variables for progress reporting. */
char report_buffer[STREAMS_MAX * 80];
char *report_buffer_ptr = NULL;
int report_buffer_len = 0;

int
thrulay_tcp_report_id (int id)
{
	struct timeval this;
	double diff_first_last;
	double relative;	/* Time difference between now and last */
	int qseq = STREAM_PER_INTERVAL_QUANTILE_SEQ(id);
	int rc, n = 0;

	if (tsc_gettimeofday(&this) == -1) {
		perror("gettimeofday");
		return -8;
	}
	normalize_tv(&this);

	diff_first_last = time_diff(&timer.start, &timer.last);
	relative = time_diff(&timer.last, &this);

	/* Time must be monotonically increasing, at least
	 * roughly, for this program to work. */

	if (stats[id].blocks_since_last == 0) {
		n = sprintf(report_buffer_ptr,
			    " (%2d) %8.3f %8.3f %8.3f %8.3f %8.3f",
			    id,
			    diff_first_last,
			    diff_first_last + relative,
			    0.0,	/* MB/s */
			    0.0,        /* delay */
			    0.0         /* jitter */
			    );
		if (thrulay_opt.reporting_verbosity > 0) {
			n += sprintf(report_buffer_ptr + n,
				      " %8.3f %8.3f %8.3f\n",
				      0.0,	/* min RTT since last */
				      0.0,	/* avg RTT since last */
				      0.0);	/* max RTT since last */
		} else {
			n += sprintf(report_buffer_ptr + n, "\n");
		}
	} else {
		double quantile_25, quantile_50, quantile_75;

		if (stats[id].blocks_since_last > 3) {

			/* Finish interval quantile sequence. */
			rc = quantile_finish(qseq);
			if (rc < 0)
				return -36;
			/* Get results. */
			rc = quantile_output(qseq, stats[id].blocks_since_last,
					     0.25, 
					     &quantile_25);
			if (rc < 0)
				return -36;
			rc = quantile_output(qseq, stats[id].blocks_since_last,
					     0.50, &quantile_50);
			if (rc < 0)
				return -36;
			rc = quantile_output(qseq, stats[id].blocks_since_last,
					     0.75, &quantile_75);
			if (rc < 0)
				return -36;
		} else {
			if (1 == stats[id].blocks_since_last) {
				quantile_25 = quantile_50 = quantile_75 = 
					stats[id].min_rtt_since_last;
			} else if (2 == stats[id].blocks_since_last) {
				quantile_25 = quantile_50 = 
					stats[id].min_rtt_since_last;
				quantile_75 =
					stats[id].max_rtt_since_last;
			} else {
				quantile_25 = stats[id].min_rtt_since_last;
				quantile_50 = stats[id].tot_rtt_since_last -
					stats[id].max_rtt_since_last -
					stats[id].min_rtt_since_last;
				quantile_75 = stats[id].max_rtt_since_last;
			}
		}

		n = sprintf(report_buffer_ptr,
			     " (%2d) %8.3f %8.3f %8.3f %8.3f %8.3f",
			     id,
			     diff_first_last,
			     diff_first_last + relative,
			     (double)stats[id].blocks_since_last *
			     (double)server_block_size *
			     8.0 / 1000000.0 / relative,
			     1000.0 * quantile_50,                 /*delay*/ 
			     1000.0 * (quantile_75 - quantile_25)  /*jitter*/
			     );

		/* Verbose output shows min., avg. and max. */
		if (thrulay_opt.reporting_verbosity > 0) {
			n += sprintf(report_buffer_ptr + n,
				     " %8.3f %8.3f %8.3f\n",
				     stats[id].min_rtt_since_last*1000.0,
				     stats[id].tot_rtt_since_last*1000.0 /
				     (double)stats[id].blocks_since_last,
				     stats[id].max_rtt_since_last*1000.0);
		} else { 
			n += sprintf(report_buffer_ptr + n, "\n");
		}
	}
	report_buffer_ptr += n;
	report_buffer_len += n;

	stats[id].blocks_since_last = 0;
	stats[id].min_rtt_since_last = 1000.0;
	stats[id].max_rtt_since_last = -1000.0;
	stats[id].tot_rtt_since_last = 0.0;

	/* Reinit interval quantile sequence. */
	quantile_exit_seq(qseq);
	quantile_init_seq(qseq);


	return 0;
}

int
thrulay_tcp_report (void)
{
	int id;

	report_buffer_ptr = &report_buffer[0];
	report_buffer_len = 0;

	for (id = 0; id < thrulay_opt.num_streams; id++) {
		int rc = thrulay_tcp_report_id(id);
		if (rc < 0)
			return rc;
	}

	/* Display progress report. */
	write_exactly(STDOUT_FILENO, report_buffer, report_buffer_len);

	return 0;
}

/*
 * Return a socket connected to the right TCP port on the right
 * server.  Use the given window size, if non-zero, locally.  Store
 * the actual window size in *real_window.  Store server socket
 * address structure in `saptr' (if both saptr and lenp are non-NULL) and
 * size of socket address structure in `lenp'.
 */
int
name2socket(char *server_name, int port, int window, int *real_window,
	void **saptr, socklen_t *lenp)
{
	int sockfd, n;
	struct addrinfo hints, *res, *ressave;
	char service[7];

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(service, sizeof(service), "%d", port);

	if ((n = getaddrinfo(server_name, service, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(n));
		return -9;
	}
	ressave = res;

	do {
		sockfd = socket(res->ai_family, res->ai_socktype,
				res->ai_protocol);
		if (sockfd < 0)
			continue;	/* ignore this one */

		if (window)
			*real_window = set_window_size(sockfd, window);

		if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
			break;		/* success */

		close(sockfd);
	} while ((res = res->ai_next) != NULL);

	if (res == NULL) {
		return -11;
	}

	if (saptr && lenp) {
		*saptr = malloc(res->ai_addrlen);
		if (*saptr == NULL) {
			perror("malloc");
			return -4;
		}
		memcpy(*saptr, res->ai_addr, res->ai_addrlen);
		*lenp = res->ai_addrlen;
	}

	freeaddrinfo(ressave);

	return sockfd;
}

/* Read the thrulay greeting from a TCP connection associated with socket s. */
int
read_greeting(int s)
{
	char buf[1024];
	int rc;
	size_t greetlen = sizeof(THRULAY_GREET) - 1;

	rc = recv_exactly(s, buf, greetlen);
	assert(rc <= (int) greetlen);
	if (rc != (int) greetlen) {
		if (rc == -1)
			perror("recv");
		return -12;
	}
	if (strncmp(buf, THRULAY_VERSION, sizeof(THRULAY_VERSION) - 1) != 0)
		return -13;
	if (buf[greetlen - 1] != '+') {
		error(ERR_WARNING, "connection rejected");
		rc = recv(s, buf, sizeof(buf) - 1, 0);
		buf[sizeof(buf) - 1] = '\0';
		if (rc == -1) {
			perror("reading rejection reason");
			return -14;
		}
		assert(rc < (int) sizeof(buf));
		buf[rc] = '\0';
		fprintf(stderr, "server said: %s", buf);
		if (buf[rc-1] != '\n')
			fprintf(stderr, "\n");
		return -15;
	}

	return 0;
}

int
send_proposal(int s, char *proposal, int proposal_size)
{
	int rc;

	rc = send_exactly(s, proposal, (size_t) proposal_size);
	assert(rc <= proposal_size);
	if (rc < proposal_size) {
		if (rc == -1)
			perror("send");
		return -16;
	}

	return 0;
}

/* Read response to a proposal.  Return the size of the response. */
int
read_response(int s, char *buf, int max)
{
	int rc;

	/* XXX: Assume that few-byte session response will come in one
           TCP packet and read in one block. */
	rc = recv(s, buf, max - 1, 0);
	assert(rc < max);
	if (rc == -1) {
		perror("recv");
		return -17;
	} else if (rc == 0) {
		return -18;
	}
	assert(rc > 0);
	buf[rc] = '\0';
	return rc;
}

int
thrulay_udp_report_final (void)
{
	int rc;
	char buf[65536];

	snprintf(buf, sizeof(buf), "+%llu:", (long long unsigned)npackets);

	rc = send_exactly(tcp_sock, buf, strlen(buf));
	if (rc == -1)
		return -19;

	while ((rc = recv(tcp_sock, buf, sizeof(buf) - 1, 0)) != 0) {
		if (rc == -1) {
			perror("recv");
			return -20;
		}
		write_exactly(STDOUT_FILENO, buf, rc);
	}

	return 0;
}

int
thrulay_udp_start (void)
{
	int rc;
	int val;
	char buf[65536];
	char random_state[256];
	int to_write;
	uint64_t packet;
	uint32_t msb, lsb;
	char nonce[8];
	int n, response_size;
	double urand;
	double erand;
	double emean;
	struct timespec req, rem;
	struct timeval this, next;
	long long unsigned npackets_llu;
	int header_size = 0;

	to_write = snprintf(buf, sizeof(buf), "%s:u:%u:%u:%u:%llu+",
			    THRULAY_VERSION, client_port, 
			    thrulay_opt.block_size, protocol_rate, 
			    (long long unsigned)npackets);
	rc = send_proposal(tcp_sock, buf, to_write);
	if (rc < 0)
		return rc;

	rc = timer_start();
	if (rc < 0)
		return rc;

	response_size = read_response(tcp_sock, buf, sizeof(buf));
	if (response_size < 0)
		return response_size;

	if (strcmp(buf, "u:-") == 0)
		return -21;
	rc = sscanf(buf, "%u:%u:%u:%llu:%n", &server_port, &packet_size,
		    &protocol_rate, &npackets_llu, &n);
	if ((rc != 4) || (response_size != n+9) || (buf[n+8] != '+') ||
	    (packet_size > (int)sizeof(buf))) {
		return -22;
	}
	memcpy(nonce, buf+n, sizeof(nonce));
	npackets = (uint64_t)npackets_llu;

	/* Set target UDP server port and header size used for sending the
	 * UDP packets. */
	switch (udp_destination->sa_family) {
	case AF_INET:
		{
			struct sockaddr_in *sin = 
				(struct sockaddr_in *)udp_destination;
			sin->sin_port = htons(server_port);

			header_size = IPV4_HEADER_SIZE + UDP_HEADER_SIZE;
		}
		break;
	case AF_INET6:
		{
			struct sockaddr_in6 *sin6 =
				(struct sockaddr_in6 *)udp_destination;
			sin6->sin6_port = htons(server_port);

			header_size = IPV6_HEADER_SIZE + UDP_HEADER_SIZE;
		}
		break;
	default:
		return -23;
	}

	/* Disable keep-alives on the control TCP connection. */
	val = 0;
	rc = setsockopt(tcp_sock, SOL_SOCKET, SO_KEEPALIVE, (char *)&val,
	                sizeof(val));
	if (rc == -1)
		error(ERR_WARNING, "failed to disable keep-alives");

#ifdef ENABLE_THRULAY_MULTICAST
	/* Set TTL field if requested. */
	if (1 != thrulay_opt.ttl) {
		switch (udp_destination->sa_family) {
			int rc;
		case AF_INET:
			rc = setsockopt(udp_sock, IPPROTO_IP, 
					IP_MULTICAST_TTL, 
					(void*)&thrulay_opt.ttl, 
					sizeof(thrulay_opt.ttl));
			if (rc < 0) {
				error(ERR_WARNING, 
				      "setsockopt(IP_MULTICAST_TTL) failed, "
				      "continuing.");
			}
			break;
		case AF_INET6:
			rc = setsockopt(udp_sock, IPPROTO_IPV6, 
					IPV6_MULTICAST_HOPS, 
					(void*)&thrulay_opt.ttl, 
					sizeof(thrulay_opt.ttl));
			if (rc < 0) {
				error(ERR_WARNING, 
				      "setsockopt(IPV6_MULTICAST_HOPS) failed,"
				      " continuing.");
			}
						
			break;
		default:
			return -24;
		}
	}
#endif

	/* Increase the buffer space of the sending UDP socket. */
	set_window_size_directed(udp_sock, thrulay_opt.window, SO_SNDBUF);

	memset(buf, '2', sizeof(buf));	/* Nothing special about '2'. */
	memcpy(buf, nonce, sizeof(nonce));
#ifdef HAVE_INITSTATE
	initstate(getpid() + getppid() + time(NULL),
		random_state, sizeof(random_state));
#else
	srand(time(NULL)+(unsigned int)random_state);
#endif

	/* RFC 2330: Chapter 11.1.3
	 * lambda = 1/emean */
	emean = 1000.0/(double)protocol_rate;

	/* Fill `next' timeval with current time */
	if (tsc_gettimeofday(&next) == -1) {
		perror("gettimeofday");
		return -1;
	}
	normalize_tv(&next);

	for (packet = 0; packet < npackets; packet++) {
		/* RFC 2330: Chapter 11.1.3
		 * (Generating Poisson Sampling Intervals)
		 *
		 * Calculate Ui, a uniformly distributed (pseudo) random number
		 * between 0 and 1.
		 */
#ifdef HAVE_INITSTATE
		urand = (double)((random()+1.0)/(RANDOM_MAX+1.0));
#else
                urand = (double)((rand()+1.0)/(RAND_MAX+1.0));
#endif
		assert(urand > 0 && urand <= 1);

		/* RFC 2330: Chapter 11.1.3
		 *
		 * Ei = -log(Ui) / lambda
		 */
		erand = -log(urand) * emean;

		/* Update `next' timeval. */
		next.tv_sec += floor(erand);
		next.tv_usec += (emean - floor(erand)) * 1000000;
		normalize_tv(&next);

		/* Calculate and set sequence number */
		msb = htonl(packet >> 32);	/* Most significant. */
		lsb = htonl(packet & ((1ULL<<32)-1));/* Least significant. */
		memcpy(buf+8, &msb, 4);
		memcpy(buf+12, &lsb, 4);

		if (thrulay_opt.busywait) {
			/* Busy wait. */

			do {
				if (tsc_gettimeofday(&this) == -1) {
					perror("gettimeofday");
					return -1;
				}
				normalize_tv(&this);
			} while (this.tv_sec < next.tv_sec ||
					(this.tv_sec == next.tv_sec &&
					 this.tv_usec < next.tv_usec));
		} else {
			/* No busy wait. */

			if (tsc_gettimeofday(&this) == -1) {
				perror("gettimeofday");
				return -1;
			}
			normalize_tv(&this);

			/* Calculate how long to sleep. */
			req.tv_sec = next.tv_sec - this.tv_sec;
			req.tv_nsec = (next.tv_usec - this.tv_usec) * 1000;
			if (req.tv_nsec < 0) {
				--req.tv_sec;
				req.tv_nsec += 1000000000;
			}

			/* Only sleep if requested sleeping time is positive.
			 * If we have set a high rate with -u then we are
			 * maybe late with this packet. */
			if (req.tv_sec >= 0) {
				/* According to nanosleep(2), the nanosecond
				 * field has to be in the range 0 to 999999999.
				 */
				assert(req.tv_nsec >= 0);
				assert(req.tv_nsec <= 999999999);

#ifndef WIN32
				do {
					rc = nanosleep(&req, &rem);
					if (rc == -1) {
						if (errno != EINTR) {
							perror("nanosleep");
							return -25;
						}
						req.tv_sec = rem.tv_sec;
						req.tv_nsec = rem.tv_nsec;
					}
				} while (rc != 0);
#else
				Sleep(req.tv_sec*1000+req.tv_nsec/1000000);
#endif
			}
		}

		if (tsc_gettimeofday(&this) == -1) {
			perror("gettimeofday");
			return -1;
		}
		normalize_tv(&this);

		tv2ntp(&this, buf+16);

		rc = sendto(udp_sock, buf, packet_size - header_size, 0,
		            udp_destination, udp_destination_len);
		if (rc == -1) {
			perror("sendto");
			return -26;
		}
	}

	timer_stop();

	sleep(1);	/* Let the UDP traffic drain. */
	close(udp_sock);

	if (thrulay_opt.reporting_verbosity > 0) {
		printf("Client runtime: %8.3fs\n", timer.runtime);
	}

	/* UDP test completed successfully. */
	return 0;
}

void
thrulay_udp_exit (void)
{
	if (close(tcp_sock) == -1)
		error(ERR_WARNING, "thrulay_udp_exit(): Unable to close "
			"TCP connection socket.");

	/* Free memory of server sockaddr */
	free(server);
	free(udp_destination);
}

int
thrulay_udp_init (void)
{
	int rc;
	int tries;
	struct addrinfo hints, *res, *ressave;
	char service[7];
	socklen_t server_len;

	tcp_sock = name2socket(thrulay_opt.server_name, thrulay_opt.port, 
			       0, NULL, (void *)&server, &server_len);
	if (tcp_sock < 0)
		return tcp_sock;

	rc = read_greeting(tcp_sock);
	if (rc < 0)
		return rc;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	/* Use same address family as for TCP socket */
	hints.ai_family = server->sa_family;
	hints.ai_socktype = SOCK_DGRAM;

	client_port = UDP_PORT;
	tries = 0;
	do {
		client_port++;
		tries++;

		snprintf(service, sizeof(service), "%d", client_port);

		if ((rc = getaddrinfo(NULL, service, &hints,
						&res)) != 0) {
			fprintf(stderr, "getaddrinfo(): %s\n",
				gai_strerror(rc));
			return -10;
		}
		ressave = res;

		do {
			udp_sock = socket(res->ai_family, res->ai_socktype,
					res->ai_protocol);
			if (udp_sock < 0)
				continue;

			/* Differentiated Services (DS) */
			if (thrulay_opt.dscp) {
				rc = set_dscp(udp_sock, thrulay_opt.dscp);
				if (rc == -1)
					error(ERR_WARNING,
							"thrulay_udp_init(): "
							"Unable to set DSCP "
							"value.");
			}

			if ((rc = bind(udp_sock, res->ai_addr,
							res->ai_addrlen)) == 0)
				break;		/* success */

			close(udp_sock);
		} while ((res = res->ai_next) != NULL);
	} while ((rc < 0) && (tries < TRY_UDP_PORTS));
	if (rc < 0)
		return -2;

	freeaddrinfo(ressave);

	/* Check whether test blocks should go to a multicast group or
	   to the server address. */
	udp_destination = malloc(res->ai_addrlen);
	if (NULL == udp_destination) {
		perror("malloc");
		return -4;
	}
	if (NULL != thrulay_opt.mcast_group) {
		hints.ai_flags = AI_PASSIVE;
		hints.ai_family = server->sa_family;
		hints.ai_socktype = SOCK_STREAM;

		if ((rc = getaddrinfo(thrulay_opt.mcast_group, service, &hints,
						&res)) != 0) {
			fprintf(stderr, "getaddrinfo(): %s\n", 
				gai_strerror(rc));
			return -3;
		}
		memcpy(udp_destination, res->ai_addr, res->ai_addrlen);
		udp_destination_len = res->ai_addrlen;
	} else {
		/* No multicast group. UDP Destination address is the
		   same as server address. */
		memcpy(udp_destination, server, server_len);
		udp_destination_len = server_len;
	}

	/* Protocol rate is in packets per 1000 seconds
	 *
	 * thrulay_opt.rate is in bits per second:
	 *  => (thrulay_opt.rate/8) = bytes/second
	 *  => (1000 * thrulay_opt.rate/8) = bytes per 1000 seconds
	 */
	protocol_rate = ((1000/8) * thrulay_opt.rate)/thrulay_opt.block_size;

	/* npackets is number of packets to send in test
	 *
	 * npackets = protocol_rate / 1000 * thrulay_opt.test_duration
	 */
	npackets = (thrulay_opt.test_duration*thrulay_opt.rate)/
		(8*thrulay_opt.block_size);

	/* Successful initialization */
	return 0;
}

void
thrulay_tcp_stop_id (int id)
{
	/* Delete stream socket in FD set. So it won't be processed in test
	 * loop. */
	FD_CLR(stream[id].sock, &rfds_orig);
	FD_CLR(stream[id].sock, &wfds_orig);

	/* Close our testing socket. This tells the server that this test
	 * has finished and the server will log test duration and average
	 * throughput. */
	thrulay_tcp_exit_id(id);
}

/* Stop TCP test. */
void
thrulay_tcp_stop (void)
{
	stop_test = 1;
}

/* Some common MTU sizes with topology. */
struct _mtu_info {
	int mtu;
	char *top;
} mtu_list[] = {
	{ 65535,	"Hyperchannel" },		/* RFC1374 */
	{ 17914,	"16 MB/s Token Ring" },
	{ 16436,	"Linux Loopback device" },
	{ 9000,		"Ethernet, jumbo-frames" },	/* Internet2 */
	{ 8166,		"802.4 Token Bus" },		/* RFC1042 */
	{ 4464,		"4 MB/s Token Ring" },
	{ 4352,		"FDDI" },			/* RFC1390 */
	{ 1500,		"Ethernet (or PPP)" },		/* RFC894, RFC1548 */
	{ 1492,		"IEEE 802.3" },
	{ 1006,		"SLIP" },			/* RFC1055 */
	{ 576,		"X.25 & ISDN" },		/* RFC1356 */
	{ 296,		"PPP (low delay)" },
};
#define MTU_LIST_NUM	11

/* Calculate MTU out of MSS.
 * Set's global variable `mtu' and returns pointer to topology info.
 *
 * According to RFC879:
 * 	MSS = MTU - sizeof(TCPHDR) - sizeof(IPHDR)
 *
 * Where:
 * 	20 <= sizeof(IPHDR) <= 60
 *	20 <= sizeof(TCPHDR) <= 60
 *
 * This implies:
 *	MSS + 40 <= MTU <= MSS + 120
 */
char *
mtu_calc (int mss)
{
	int i;

#ifdef IP_MTU
	if (mtu) {
		for (i = 0; i < MTU_LIST_NUM; i++) {
			if (mtu == mtu_list[i].mtu) {
				return (mtu_list[i].top);
			}
		}
	}
	return "unknown";
#endif

	for (i = 0; i < MTU_LIST_NUM; i++) {
		if (((mss + 40) <= mtu_list[i].mtu)
				&& (mtu_list[i].mtu <= (mss + 120))) {
			mtu = mtu_list[i].mtu;
			return (mtu_list[i].top);
		}
	}

	/* No match. Return default one. */
	mtu = mss + 40;
	return "unknown";
}

/* Print test info before tests start. Displayed info includes local/remote
 * window, block size, MTU, MSS, test duration and reporting interval.
 */
void
thrulay_tcp_info (void)
{
	char *str_top = NULL;

	if (thrulay_opt.reporting_verbosity < 0)
		return;

	/* Print local/remote window and block size */
	printf("# local window = %dB; remote window = %dB\n",
			local_window, server_window);
	if (thrulay_opt.block_size == server_block_size) {
		printf("# block size = %dB\n", thrulay_opt.block_size);
	} else {
		printf("# requested block size = %dB; "
				"actual block size = %dB\n", 
		       thrulay_opt.block_size, server_block_size);
	}

	str_top = mtu_calc(mss);

	/* Print MTU, MSS, topology info */
	printf("# MTU: %dB; MSS: %dB; Topology guess: %s\n", mtu, mss,
			str_top);
#ifdef IP_MTU
	printf("# MTU = getsockopt(IP_MTU); MSS = getsockopt(TCP_MAXSEG)\n");
#else
	if (!strcmp(str_top, "unknown")) {
		printf("# MTU = MSS + 40; MSS = getsockopt(TCP_MAXSEG)\n");
	} else {
		printf("# MTU = guessed out of MSS as in RFC 879; "
				"MSS = getsockopt(TCP_MAXSEG)\n");
	}
#endif

	/* Print test duration and reporting interval. */
	printf("# test duration = %ds; ",thrulay_opt.test_duration);
	if (0 < thrulay_opt.reporting_interval) {
		printf("reporting interval = %ds\n", 
		       thrulay_opt.reporting_interval);
	} else {
		printf("intermediate reporting disabled\n");
	}
	printf("# delay (median) and jitter (interquartile spread of delay) "
	       "are reported in ms\n");
	if (thrulay_opt.reporting_verbosity > 0) {
		printf("#(ID) begin, s   end, s   Mb/s  RTT delay,ms "
		       "jitter     min      avg      max\n");
	} else {
		printf("#(ID) begin, s   end, s   Mb/s  RTT delay,ms "
		       "jitter\n");
	}

	fflush(stdout);
}

/* Start TCP test. */
int
thrulay_tcp_start (void)
{
	int rc;
	int id;
	struct timeval tv, timeout;
	fd_set rfds, wfds;
	char buf[STREAMS_MAX][1024];
	int to_write;

	for (id = 0; id < thrulay_opt.num_streams; id++) {
		int my_server_window, my_server_block_size, my_mss = 0, sopt;
		socklen_t len;
#ifdef IP_MTU
		int my_mtu;
#endif

		to_write = snprintf(buf[0], sizeof(buf[0]), "%s:t:%u:%u+", 
				    THRULAY_VERSION, thrulay_opt.window, 
				    thrulay_opt.block_size);
		assert(to_write > 0 && to_write < (int) sizeof(buf[0]));

		rc = send_proposal(stream[id].sock, buf[0], to_write);
		if (rc < 0)
			return rc;

		rc = read_response(stream[id].sock, buf[0], sizeof(buf[0]));
		if (rc < 0)
			return rc;
		my_server_window = my_server_block_size = -1;
		/* XXX: Very long numbers will not, of course, be processed
		 *      correctly by sscanf() below.  We could, if the number
		 *      exceeds a certain value (e.g., has more than a certain
		 *      number of characters, use some ``large'' -- still supported
		 *      -- value for window or block size.  It's not worth the
		 *      trouble. */
		rc = sscanf(buf[0], "%d:%d+", &my_server_window, 
			    &my_server_block_size);
		if (rc != 2)
			return -22;
		assert(my_server_window >= 0 && my_server_block_size >= 0);
		if (my_server_block_size < MIN_BLOCK)
			return -27;
		if (my_server_block_size > MAX_BLOCK)
			return -28;
		if (my_server_window < 1500)
			return -29;

#ifdef IP_MTU
		len = sizeof(my_mtu);
		if (getsockopt(stream[id].sock, SOL_IP, IP_MTU,
			       (char *)&my_mtu, &len) == -1) {
			perror("getsockopt");
			error(ERR_WARNING, "unable to determine Path MTU");
		}
#endif

		/* Get Maximum Segment Size (MSS) */
#ifdef TCP_MAXSEG
		len = sizeof(my_mss);
		if (getsockopt(stream[id].sock, SOL_TCP, TCP_MAXSEG, 
			       (char *)&my_mss, &len) == -1) {
			perror("getsockopt");
			error(ERR_WARNING, "unable to determine TCP_MAXSEG");
		}
#else
		perror("getsockopt");
		error(ERR_WARNING, 
		      "getsockopt(TCP_MAXSEG) not supported on Windows");
#endif
		/*
		 * Check/set local/remote window, server block size, MSS.
		 * As we display the local/remote window, server block size, 
		 * MSS, MTU only once, we check that this information is the
		 * same for every stream.
		 */
		if (id == 0) {
			/* If this is first stream, initialize global values.*/
			server_window = my_server_window;
			server_block_size = my_server_block_size;
			mss = my_mss;
#ifdef IP_MTU
			mtu = my_mtu;
#endif
		}
		if (server_window != my_server_window) {
			return -30;
		}
		if (server_block_size != my_server_block_size) {
			return -31;
		}
		if (mss != my_mss) {
			return -32;
		}
#ifdef IP_MTU
		if (mtu != my_mtu) {
			return -33;
		}
#endif

		/* Differentiated Services (DS) */
		if (thrulay_opt.dscp) {
			rc = set_dscp(stream[id].sock, thrulay_opt.dscp);
			if (rc == -1)
				error(ERR_WARNING, "thrulay_tcp_init_id(): "
				      "Unable to set DSCP value.");
		}
		
#ifndef WIN32
		assert((unsigned int)stream[id].sock < FD_SETSIZE);
#endif
		
		/* Set non-blocking IO. */
#ifndef WIN32
		sopt = fcntl(stream[id].sock, F_GETFL);

		if (-1 == sopt) {
			error(ERR_WARNING, "fcntl(F_GETFL): failed");
		} else {
			rc = fcntl(stream[id].sock,F_SETFL,sopt | O_NONBLOCK);
			if (-1 == rc) {
				error(ERR_WARNING, "fcntl(F_SETFL,O_NONBLOCK "
				      "failed");
			}
		}
#else
		sopt = ioctlsocket(stream[id].sock, FIONBIO, (unsigned long*)1);
		if (-1 == sopt) {
			error(ERR_WARNING, "ioctlsocket(FIONBIO): failed");
		} 
#endif /* ndef WIN32 */
	}

	/* Allocate memory for writing blocks. */
	block = malloc((size_t)server_block_size + 
		       (thrulay_opt.num_streams -1) * BLOCK_HEADER);
	if (block == NULL) {
		return -4;
	}

	thrulay_tcp_info();

	rc = timer_start();
	if (rc < 0)
		return rc;

	stop_test = 0;
	while (!stop_test) {
		rfds = rfds_orig;
		wfds = wfds_orig;

		timeout.tv_sec = 0;
		timeout.tv_usec = 1000;

		rc = select(maxfd + 1, &rfds, &wfds, NULL, &timeout);

		if (rc < 0) {
			perror("select");
			return -34;
		}

		if (rc == 0) {
			/* Timeout occured. Check timer. */
			timer_check();
		}

		if (rc > 0) {
			/* Check all stream sockets. */
			for (id = 0; id < thrulay_opt.num_streams; id++) {

				/* Recv from socket. */
				if (FD_ISSET(stream[id].sock, &rfds)) {
					if (0 == stream[id].rcount && 
					    tsc_gettimeofday(&tv) == -1) {
						perror("gettimeofday");
						return -1;
					}

					/* Non-blocking recv. */
					rc = recv(stream[id].sock,
						  buf[id]+stream[id].rcount,
						  BLOCK_HEADER-
						  stream[id].rcount, 0);

					if (-1 == rc && EAGAIN != errno) {
						perror("read");
						error(ERR_WARNING, "premature "
								"end of test");
						thrulay_tcp_stop_id(id);
						break;
					} else if (0 < rc) {
						stream[id].rcount += rc;
						if ( BLOCK_HEADER == 
						     stream[id].rcount ) {
							/* Whole block read */
							memcpy(&tv, buf[id], 
							       sizeof(tv));
							rc = new_timestamp(id, &tv);
							if (rc < 0)
								return rc;
							stream[id].rcount = 0;
						}
					}
				}

				/* Send to socket */
				if (FD_ISSET(stream[id].sock, &wfds)) {
					if (0 == stream[id].wcount) {
						if (tsc_gettimeofday(&tv) == 
						    -1) {
							perror("gettimeofday");
							return -1;
						}
						memcpy(block + id*BLOCK_HEADER,
						       &tv, sizeof(tv));
					}

					/* Non-blocking send */
					rc = send(stream[id].sock,
						   block + id*BLOCK_HEADER +
						   stream[id].wcount,
						   (size_t)server_block_size-
						   stream[id].wcount, 0);

					if (rc == -1 && EAGAIN != errno) {
						perror("send");
						error(ERR_WARNING, "premature "
						      "end of test");
						thrulay_tcp_stop_id(id);
					} else if (rc > 0) {
						stream[id].wcount += rc;
					}
					if ( (size_t)server_block_size == 
					     stream[id].wcount ) {
						/* Whole block written */
						stream[id].wcount = 0;
					}
				} /* Send to socket */
			} /* for(id = 0; id < thrulay_opt.num_streams; id++) */

			timer_check();
		} /* if (rc > 0) { */
	}
	timer_stop();

	/* Free memory of writing block. */
	free(block);

	/* TCP test completed successfully. */
	return 0;
}

/* Deinitialize TCP stream with ID `id'. */
void
thrulay_tcp_exit_id (int id)
{
	/* Close connection to server. */
	if (close(stream[id].sock) == -1)
		error(ERR_WARNING, "thrulay_tcp_exit_id(): Unable to close "
				"server socket.");
}

/* Deinitialize all TCP streams. */
void
thrulay_tcp_exit (void)
{
	int id;

	for (id = 0; id < thrulay_opt.num_streams; id++) {
		thrulay_tcp_exit_id(id);
	}

	tcp_stats_exit();
}

/* Initialize TCP stream with ID `id'. */
int
thrulay_tcp_init_id (int id)
{
	int rc, my_local_window;
	/* Open socket. Do not greet yet */
	stream[id].wcount = stream[id].rcount = 0;
	stream[id].sock = name2socket(thrulay_opt.server_name, 
				      thrulay_opt.port, thrulay_opt.window,
				      &my_local_window, NULL, NULL);

	if (stream[id].sock < 0)
		return stream[id].sock;

	rc = read_greeting(stream[id].sock);
	if (rc < 0)
		return rc;

	if (id == 0) {
		/* If this is first stream, initialize global values. */
		local_window = my_local_window;
	}
	if (local_window != my_local_window) {
		return -5;
	}

	memset(block + id*BLOCK_HEADER, '2', (size_t)server_block_size);

	return 0;
}

/* Initialize all that has to do with TCP streams. */
int
thrulay_tcp_init (void)
{
	int rc, id;

	rc = tcp_stats_init();
	if (rc < 0)
		return rc;

	/* Clean recv/send FD sets for select(). */
	FD_ZERO(&rfds_orig);
	FD_ZERO(&wfds_orig);

	for (id = 0; id < thrulay_opt.num_streams; id++) {
		/* Initialize TCP stream. */
		rc = thrulay_tcp_init_id(id);
		if (rc < 0)
			return rc;

		/* FD set */
		FD_SET(stream[id].sock, &rfds_orig);
		FD_SET(stream[id].sock, &wfds_orig);
		maxfd = (stream[id].sock > maxfd ? stream[id].sock : maxfd);
	}
#ifdef WIN32
	maxfd = FD_SETSIZE - 1;
#endif

	return 0;
}

/* Initialize thrulay. */
int
thrulay_client_init (thrulay_opt_t opt)
{
	int rc;
	thrulay_opt = opt;
	tsc_init();

#ifdef WIN32
	{
		WSADATA wsaData;
		if (WSAStartup(MAKEWORD(2, 2), &wsaData)) {
			return -35;
		}
	}
#endif

	/* If client has set no block size, set default ones. */
	if (!thrulay_opt.block_size) {
		if (thrulay_opt.rate) {
			/* Set default UDP block size */
			thrulay_opt.block_size = 1500;
		} else {
			/* Set default TCP block size */
			thrulay_opt.block_size = 64 * 1024;
		}
	}

	if (thrulay_opt.rate) {
		rc = thrulay_udp_init();
	} else {
		rc = thrulay_tcp_init();
	}

	return rc;
}

/* Deinitialize thrulay. */
void
thrulay_client_exit (void)
{

	if (thrulay_opt.rate) {
		thrulay_udp_exit();
	} else {
		thrulay_tcp_exit();
	}

#ifdef WIN32
	WSACleanup();
#endif
}

int
thrulay_client_start (void)
{
	int rc;

	if (thrulay_opt.rate) {
		rc = thrulay_udp_start();
	} else {
		rc = thrulay_tcp_start();
	}

	return rc;
}

int
thrulay_client_report_final (void)
{
	int rc = 0;

	if (thrulay_opt.reporting_verbosity < 0)
		return 0;

	if (thrulay_opt.rate) {
		rc = thrulay_udp_report_final();
	} else {
		thrulay_tcp_report_final();
	}

	return rc;
}
