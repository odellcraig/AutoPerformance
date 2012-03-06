/*
 * thrulay.c -- network throughput tester (the client part).
 * 
 * Written by Stanislav Shalunov, http://www.internet2.edu/~shalunov/
 *            Bernhard Lutzmann, belu@users.sf.net
 *            Federico Montesino Pouzols, fedemp@altern.org
 *
 * @(#) $Id: thrulay.c,v 1.4.2.13 2006/08/20 18:06:19 fedemp Exp $
 * 
 * Copyright 2003, 2006, Internet2.
 * Legal conditions are in file LICENSE
 * (MD5 = ecfa50d1b0bfbb81b658c810d0476a52).
 */

/**
 * @file thrulay.c
 *
 * @short thrulay client command line tool.
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <assert.h>

#include "client.h"
#include "thrulay.h"
#include "util.h"
#include "rcs.h"

RCS_ID("@(#) $Id: thrulay.c,v 1.4.2.13 2006/08/20 18:06:19 fedemp Exp $")

/* 
 * Convert a string like "12k" to a number like 12000.  Return zero on
 * error.
 */
static uint64_t
rate2i(char *s)
{
	uint64_t r;	/* Result. */
	char *p;
	int l;
	int suffix = 0;

	/* First, set the multiple. */
	l = strlen(s);
	switch (s[l-1]) {
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		r = 1;
		break;
	case 'k':
	case 'K':
		r = 1000;
		suffix = 1;
		break;
	case 'm':
	case 'M':
		r = 1000000;
		suffix = 1;
		break;
	case 'g':
	case 'G':
		r = 1000000000ULL;
		suffix = 1;
		break;
	case 't':
	case 'T':
		r = 1000000000000ULL;
		suffix = 1;
		break;
	default:
		r = 0;
	}
	if (suffix)
		s[l-1] = '\0';
	r *= strtoll(s, &p, 10);
	if (!*s || *p)	/* Invalid character found. */
		r = 0;
	return r;
}

static void
print_usage(void)
{
	fprintf(stderr, "Usage: \n");
	fprintf(stderr, " thrulay [-h] [-V] [-v] [-b] [-D DSCP] [-m#] [-t#] "
			"[-i#] [-w#] [-l#] [-p#] [-u#] [-T#] [-g group] "
			"host\n");
	fprintf(stderr, "\t-h\t\tdisplay this help and exit\n");
	fprintf(stderr, "\t-V\t\toutput version information and exit\n");
	fprintf(stderr, "\t-v\t\tverbose reporting of results\n");
	fprintf(stderr, "\t-b\t\tBusy wait in UDP test\n");
	fprintf(stderr, "\t-D DSCP\t\tDSCP value for TOS byte "
			"(default: unset)\n");
	fprintf(stderr, "\t-m#\t\tnumber of test streams (default: 1)\n");
	fprintf(stderr, "\t-t#\t\ttest duration, in seconds (default: 60s)\n");
	fprintf(stderr, "\t-i#\t\treporting interval, in seconds "
			"(default: 1s)\n");
	fprintf(stderr, "\t\t\t0 disables intermediate reports\n");
	fprintf(stderr, "\t-w#\t\twindow, in bytes (default: 4194304B)\n");
	fprintf(stderr, "\t-l#\t\tblock size, in bytes (default: 64kB)\n");
	fprintf(stderr, "\t-p#\t\tserver port (default: 5003)\n");
	fprintf(stderr, "\t-u#[kMGT]\tUDP mode with given rate "
				"(default: off)\n"
			"\t\t\tIn UDP mode, rate is in bits per second and "
				"can be\n"
			"\t\t\tfollowed by a SI suffix (k for 1000, M for "
				"100000);\n"
			"\t\t\tdefault packet size, 1500 bytes, can be "
				"changed with -l;\n"
			"\t\t\twindow size becomes the UDP send buffer size;\n"
			"\t\t\treporting interval is ignored.\n");
	fprintf(stderr, "\t-T#\t\tTTL field for multicast (default: 1)\n");
	fprintf(stderr, "\t-g group\tmulticast group to send data to\n");
	fprintf(stderr, "\thost\t\tserver to connect to (no default)\n");
}

static void __attribute__((noreturn))
usage(void)
{
	print_usage();
	exit(1);
}

static void __attribute__((noreturn))
version(void)
{
	printf(THRULAY_VERSION " (client part) release ");
	printf(PACKAGE_VERSION "\n");
	printf("Copyright (c) 2006, Internet2.\n");
	printf("See the source for copying conditions.\n");
	exit(0);
}

int
main(int argc, char *argv[])
{
	char *endptr = NULL;
	uint32_t tlng;
	int rc, ch;
	thrulay_opt_t thrulay_opt;

	thrulay_client_options_init(&thrulay_opt);

	while ((ch = getopt(argc, argv, "hVvbD:m:t:i:w:l:p:u:T:g:")) != -1)
		switch (ch) {
		case 'h':
			print_usage();
			exit(0);
		case 'V':
			version();
		case 'v':
			thrulay_opt.reporting_verbosity = 1;
			break;
		case 'b':
			thrulay_opt.busywait = 1;
			break;
		case 'D':
			if (thrulay_opt.dscp) {
				fprintf(stderr, "can only set \'-D\' once.\n");
				usage();
			}
			tlng = strtoul(optarg, &endptr, 0);
			/*
			 * Validate int conversion and verify user only sets
			 * last 6 bits (DSCP must fit in 6 bits - RFC 2474).
			 */
			if ((*endptr != '\0') || (tlng & ~0x3F)) {
				fprintf(stderr, "invalid value for option "
					"\'-D\'. DSCP value expected.\n");
				usage();
			}
			thrulay_opt.dscp = (uint8_t)tlng;
			break;
		case 'm':
			thrulay_opt.num_streams = atoi(optarg);
			if (thrulay_opt.num_streams <= 0 ||
			    thrulay_opt.num_streams >= STREAMS_MAX) {
				fprintf(stderr, "number of test streams must "
						"be a positive integer < %d\n",
						STREAMS_MAX);
				usage();
			}
			break;
		case 't':
			thrulay_opt.test_duration = atoi(optarg);
			if (thrulay_opt.test_duration <= 0) {
				fprintf(stderr, "test duration must be "
					"a positive integer (in seconds)\n");
				usage();
			}
			break;
		case 'i':
			thrulay_opt.reporting_interval = atoi(optarg);
			if (thrulay_opt.reporting_interval < 0) {
				fprintf(stderr, "reporting interval must be 0 "
					"or a positive integer (in seconds)\n");
				usage();
			}
			break;
		case 'w':
			thrulay_opt.window = atoi(optarg);
			if (thrulay_opt.window <= 0) {
				fprintf(stderr, "window must be "
					"a positive integer (in bytes)\n");
				usage();
			}
			break;
		case 'l':
			thrulay_opt.block_size = atoi(optarg);
			if (thrulay_opt.block_size <= 0) {
				fprintf(stderr, "block size must be "
					"a positive integer (in bytes)\n");
				usage();
			}
			break;
		case 'p':
			thrulay_opt.port = atoi(optarg);
			if (thrulay_opt.port <= 0) {
				fprintf(stderr, "port must be "
					"a positive integer\n");
				usage();
			}
			break;
		case 'u':
			thrulay_opt.rate = rate2i(optarg);
			if (thrulay_opt.rate == 0) {
				fprintf(stderr, "rate must be positive\n");
				usage();
			}
			break;
		case 'T':
#ifdef ENABLE_THRULAY_MULTICAST
			thrulay_opt.ttl = atoi(optarg);
			if (thrulay_opt.ttl < 0) {
				fprintf(stderr, "TTL cannot be negative\n");
				usage();
			}
#else
			fprintf(stderr,"multicast is disabled, continuing\n");
#endif
			break;
		case 'g':
#ifdef ENABLE_THRULAY_MULTICAST
			thrulay_opt.mcast_group = strdup(optarg);
#else
			fprintf(stderr,"multicast is disabled, continuing\n");
#endif
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();
	thrulay_opt.server_name = argv[0];
#if 0
	if (thrulay_opt.rate && thrulay_opt.reporting_interval) {
		fprintf(stderr, "No reporting interval with UDP tests.\n");
		usage();
	}
#endif

#if 0
	if ( !thrulay_opt.rate && NULL != thrulay_opt.mcast_group) {
		fprintf(stderr, "A multicast group was specified for a TCP "
			"test. It will be ignored.\n");
	}
#endif

#ifdef SIGPIPE
	/* Ignore SIGPIPE.  Always do in any socket code. */
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		perror("signal(SIGPIPE, SIG_IGN)");
		error(ERR_FATAL, "could not ignore SIGPIPE");
	}
#endif /* ifdef SIGPIPE */

	assert(sizeof(struct timeval) <= BLOCK_HEADER);

	rc = thrulay_client_init(thrulay_opt);
	if (rc < 0) {
		fprintf(stderr, "While initializing: ");
		error(ERR_FATAL, thrulay_client_strerror(rc));
	}

	rc = thrulay_client_start();
	if (rc < 0) {
		fprintf(stderr, "During test: ");
		error(ERR_FATAL, thrulay_client_strerror(rc));
	}

	rc = thrulay_client_report_final();
	if (rc < 0) {
		fprintf(stderr, "While generating final report: ");
		error(ERR_FATAL, thrulay_client_strerror(rc));
	}

	thrulay_client_exit();

	free(thrulay_opt.mcast_group);
	return 0;
}
