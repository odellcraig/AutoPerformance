/*
 * client.h -- thrulay library, client API.
 *
 * Written by Stanislav Shalunov, http://www.internet2.edu/~shalunov/
 *            Bernhard Lutzmann, belu@users.sf.net
 *            Federico Montesino Pouzols, fedemp@altern.org
 *
 * @(#) $Id: client.h,v 1.1.2.7 2006/08/20 18:06:19 fedemp Exp $
 *
 * Copyright 2005, Internet2.
 * Legal conditions are in file LICENSE
 * (MD5 = ecfa50d1b0bfbb81b658c810d0476a52).
 */

/**
 * @file client.h
 * @ingroup client
 * @short thrulay client API.
 **/

#ifndef THRULAY_CLIENT_H_INCLUDED
#define THRULAY_CLIENT_H_INCLUDED

/**
 * @defgroup client thrulay client library
 *
 * @{
 **/

#include <stdint.h>

#define THRULAY_VERSION		"thrulay/2"
#define STREAMS_MAX		256

/**
 * thrulay client options 
 **/
typedef struct {
	char *server_name;	/** Where to send test traffic. */
	int num_streams;	/** Number of test streams to start. */
	int test_duration;	/** For how long, in seconds. */
	int reporting_interval;	/** How often to report progress. */
	int reporting_verbosity;/** Verbosity of the results report. */
	int window;		/** Window size, in bytes. */
	int block_size;		/** Block size, in bytes. */
	int port;		/** Server TCP port number. */
	uint64_t rate;		/** Rate in b/s.  Non-zero means UDP. */
	uint8_t dscp;		/** DSCP: 6 bit field */
	int busywait;		/** Busy wait in UDP test. */
	int ttl;                /** TTL field. */
	char *mcast_group;      /** Multicast group to send to. */
} thrulay_opt_t;

/**
 * Initialize an specification of thrulay client options with default
 * values. At least, the server name must be given after this
 * initialization to get a complete test specification.
 *
 * @param thrulay_opt Options variable to be initialized.
 **/
void
thrulay_client_options_init(thrulay_opt_t *thrulay_opt);

/**
 * Initialize a thrulay client.
 *
 * @param thrulay_opt Options of the thrulay
 * client. thrulay_client_options_init can be used to fill in the
 * options with default values.
 *
 * @return Return code.
 * @retval  0 Successful initialization.
 * @retval <0 If an error occurred (use thrulay_client_strerror to get
 * a description).
 **/
int
thrulay_client_init(thrulay_opt_t thrulay_opt);

/**
 * Run thrulay test as specified with thrulay_client_init.
 *
 * @return Return code.
 * @retval  0 Test completed successfully.
 * @retval <0 If an error occurred (use thrulay_client_strerror to get
 * a description).
 **/
int
thrulay_client_start(void);

/**
 * Print final measurement report.
 *
 * @return Return code.
 * @retval 0 On success.
 * @retval <0 If an error occurred (use thrulay_client_strerror to get
 * a description).
 **/
int
thrulay_client_report_final(void);

/**
 * Finish the thrulay client.
 **/
void
thrulay_client_exit(void);

/**
 * Get a description of an error code returned by any of the thrulay
 * client API functions.
 *
 * @return Description of last error in the thrulay client.
 * @retval NULL if an error occurrs, the error is unknown or a wrong
 * error code is given.
 **/
const char *
thrulay_client_strerror(int errorcode);

/** @} */

#endif /* #ifndef THRULAY_CLIENT_H_INCLUDED */
